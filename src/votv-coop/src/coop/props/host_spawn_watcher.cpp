// coop/props/host_spawn_watcher.cpp -- see coop/props/host_spawn_watcher.h.

#include "coop/props/host_spawn_watcher.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/element/element.h"
#include "coop/element/registry.h"          // EidForActor (drain: tracked/mirror exclusion)
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"          // LocalHandActor (drain: hotbar view-actor exclusion)
#include "coop/props/prop_echo_suppress.h"  // IsMirrorSpawn (mirror-spawn exclusion)
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_wire_parity.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_lifecycle.h"      // ExpressSpawnedProp (reuse the keyed broadcast)
#include "coop/props/prop_save_data.h"       // Publish the record after the spawn Blueprint fills it
#include "coop/props/container_birth.h"
#include "coop/props/prop_spawn_authoring.h"
#include "ue_wrap/actors/floor_bed.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "coop/props/remote_prop_spawn.h"
#include "coop/session/host_world_settle.h"  // Settled: the host's start-up births are not live edits
#include "coop/props/join_membership_sweep.h"  // the join claim and sweep
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/hot_path_guard.h"  // UE_ASSERT_GAME_THREAD
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/actors/prop.h"            // IsDescendantOfProp (the keyed-prop gate for the Q-menu seam)
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/types.h"       // FTransform (the reflected SpawnTransform param layout)
#include "ue_wrap/core/ufunction_hook.h"  // the FinishSpawningActor Func patch, the keyed spawn seam
#include "ue_wrap/actors/water_pour.h"
#include "coop/props/water_pour_sync.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <cmath>          // asin/atan2 for the FQuat -> FRotator conversion
#include <cstdint>
#include <string>
#include <vector>

namespace coop::host_spawn_watcher {
namespace {

namespace P  = ue_wrap::profile;
namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace PT = coop::prop_element_tracker;

UE_FACT_FN(kRowBeginDeferredSpawn, "host_spawn_watcher", L"GameplayStatics",
           L"BeginDeferredActorSpawnFromClass", Declared);
UE_FACT_FN(kRowFinishSpawningActor, "host_spawn_watcher", L"GameplayStatics", L"FinishSpawningActor", Declared);

// The atomic session pointer: the POST observer is registered once but fires for the session
// lifetime, and the harness re-stores the boot-lifetime pointer through the install.
std::atomic<coop::net::Session*> g_session{nullptr};

inline coop::net::Session* LoadSession() {
    return g_session.load(std::memory_order_acquire);
}

// Resolved-once dependencies; object-array entries do not unload in the shipped engine.
ue_wrap::facts::Resolved g_beginDeferredFn;
int32_t g_classParamOff     = -1;   // ActorClass UClass* param
int32_t g_returnParamOff     = -1;   // ReturnValue AActor* param
int32_t g_xformParamOff      = -1;   // the SpawnTransform param, the spawn pose; the actor is not positioned yet at the POST
ue_wrap::facts::Resolved g_finishSpawnFn;  // FinishSpawningActor, the keyed sandbox-menu seam
int32_t g_finishReturnOff    = -1;       // FinishSpawningActor ReturnValue (the finished actor)
bool    g_observerRegistered = false;  // also the permanent give-up latch
bool    g_finishHookSettled  = false;  // independent of the optional ambient-prop observer
bool    g_classesResolved    = false;

// The ambient-prop class set (the pinecone spawner's outputs), resolved from the profile's
// mirror class list. Exact-pointer matched: these are leaf classes with no in-scope
// subclasses, so a hierarchy walk is unnecessary.
void* g_propClasses[ue_wrap::profile::name::kAmbientPropSpawnMirrorClassesSize] = {};

bool IsMirroredPropClass(void* cls) {
    if (!cls) return false;
    for (void* c : g_propClasses) {
        if (c && c == cls) return true;
    }
    return false;
}

// The death-watch for the mirrored transient props. A pinecone self-expires through its
// lifespan (an engine destroy; the spawner-spawned actor has no observable blueprint destroy),
// so each broadcast prop is liveness-checked and a destroy for its eid emitted the tick its
// actor dies. Game thread only, guarded on the producer side by the POST's game-thread gate.
struct WatchedProp {
    void*    actor       = nullptr;
    int32_t  internalIdx = -1;   // captured live -> IsLiveByIndex without deref
    uint32_t eid         = 0;    // PropDestroy identity (key=None)
};
std::vector<WatchedProp> g_watched;
// The cap is a runaway backstop: pinecones spawn on a random multi-second interval and expire
// after minutes, so only a handful are live at once. A shed entry silently loses that prop's
// despawn, hence the warning on overflow.
constexpr size_t kMaxWatched = 256;

void WatchSpawnedProp(void* actor, uint32_t eid) {
    if (!actor || eid == 0 || eid == coop::element::kInvalidId) return;
    const int32_t idx = R::InternalIndexOf(actor);
    if (idx < 0) return;
    for (auto& w : g_watched) {
        if (w.eid == eid) { w.actor = actor; w.internalIdx = idx; return; }
    }
    if (g_watched.size() >= kMaxWatched) {
        UE_LOGW("host_spawn_watcher: watch cap %zu hit -- shedding eid=%u (its despawn is lost)",
                kMaxWatched, g_watched.front().eid);
        g_watched.erase(g_watched.begin());
    }
    g_watched.push_back(WatchedProp{actor, idx, eid});
}

// The owner POST observer on the deferred spawn, peer-symmetric: the ambient set is an owner
// effect, since the pinecone spawner anchors at the local player's camera, so each peer rolls
// its own forest drops and broadcasts them, the firefly authority shape on the prop spawn
// pipeline. It fires for every deferred spawn (NPCs, lightning, every prop), filtered fast by
// an exact class-pointer match against the small ambient set before any actor dereference.
// The game-thread gate: actor spawning is game-thread-only in the engine, so a deferred-spawn
// POST is always on the game thread, the guard never misses a real spawn, and it is safe to
// dispatch UFunctions and mutate the game-thread-only watch list (the NPC POST, by contrast,
// reads only params and tolerates the parallel-animation worker dispatch).
void OnSpawnPost(void* /*self*/, void* /*function*/, void* params) {
    if (!GT::IsGameThread()) return;
    if (!params || g_classParamOff < 0 || g_returnParamOff < 0 || g_xformParamOff < 0) return;

    // Any connected peer broadcasts its own ambient spawns, the owner effect. The mirror-scope
    // check kills the echo: a wire mirror is spawned through this same UFunction by the spawn
    // receiver, and this POST fires inside that call, before the actor exists to carry a mirror
    // mark.
    auto* s = LoadSession();
    if (!s || !s->connected()) return;
    if (coop::prop_echo_suppress::InMirrorSpawnScope()) return;

    void* actorClass = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(params) + g_classParamOff);
    // The ambient-prop mirror (the pinecone, stick and crystal spawner output, a keyless spawn).
    // Fast-reject everything else by exact class-pointer compare before any actor dereference.
    // Pile and clump grabs do not come through here: their deferred spawn is EX_CallMath, invisible
    // to this ProcessEvent hook, and they sync through the use-press and held-edge seams instead.
    if (!IsMirroredPropClass(actorClass)) return;  // fast reject (exact pointer compares)

    void* actor = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(params) + g_returnParamOff);
    if (!actor || !R::IsLive(actor)) return;               // spawn failed / dead

    // Dedupe: if this actor is already a tracked prop Element (an unexpected overlap with the
    // keyed init-observer path, or a double POST) skip; never broadcast two eids for one actor.
    // The processed-init mark below also makes a later init POST on this actor a no-op.
    if (PT::GetPropElementIdForActor(actor) != coop::element::kInvalidId) return;

    const std::wstring cls = R::ClassNameOf(actor);

    // Mint the keyless prop Element (a None key, so the eid is the cross-peer identity, the trash
    // clump precedent) and the init dedupe latch. Marking the init processed here is deliberate
    // double-broadcast prevention: these classes' init is blueprint-internal and never fires our
    // init POST observer, so it is a no-op today; if a recook ever made one observable, the latch
    // makes the init POST skip rather than double-send.
    PT::MarkProcessedInit(actor);
    PT::MarkPropElement(actor, L"", cls, PT::EnrollSource::kExpressSeam);
    const coop::element::ElementId eid = PT::GetPropElementIdForActor(actor);
    if (eid == coop::element::kInvalidId) {
        UE_LOGW("host_spawn_watcher: MarkPropElement gave kInvalidId for '%ls' -- "
                "skipping PropSpawn (Registry full?)", cls.c_str());
        return;
    }

    coop::net::PropSpawnPayload p{};
    p.className.len = 0;
    for (size_t i = 0; i < cls.size() && i < 63; ++i)
        p.className.data[p.className.len++] = static_cast<char>(cls[i]);
    p.key.len      = 0;   // key=None: the eid is the identity
    p.propName.len = 0;   // these classes carry their own mesh (no list_props row)

    // Read the spawn-transform param, not the actor's location: at the deferred-spawn POST the
    // actor exists but is not yet positioned (the transform is applied by the later finish, the
    // deferred two-step), so it sits at the origin until then, and a location read mirrored the
    // prop to the world origin. The wrapper's FTransform is the reflected param layout,
    // static-asserted, so the type-safe cast avoids hand-offset slips. Pure reads, thread-safe.
    const auto* xt = reinterpret_cast<const ue_wrap::FTransform*>(
        reinterpret_cast<const uint8_t*>(params) + g_xformParamOff);
    p.locX   = xt->TX; p.locY = xt->TY; p.locZ = xt->TZ;
    p.scaleX = xt->SX; p.scaleY = xt->SY; p.scaleZ = xt->SZ;
    // Quaternion to rotator in degrees, the standard engine conversion.
    const float qx = xt->RotX, qy = xt->RotY, qz = xt->RotZ, qw = xt->RotW;
    const float sinp  = 2.f * (qw * qy - qz * qx);
    const float sinpc = sinp > 1.f ? 1.f : (sinp < -1.f ? -1.f : sinp);
    constexpr float kRadToDeg = 57.29577951308232f;
    p.rotPitch = std::asin(sinpc) * kRadToDeg;
    p.rotYaw   = std::atan2(2.f * (qw * qz + qx * qy), 1.f - 2.f * (qy * qy + qz * qz)) * kRadToDeg;
    p.rotRoll  = std::atan2(2.f * (qw * qx + qy * qz), 1.f - 2.f * (qx * qx + qy * qy)) * kRadToDeg;
    p.chipType = 0;       // not trash
    // Single-player parity: the mirror spawns simulating, so it drops under the client's own
    // physics (the local fall is intentionally not synced). No heavy, frozen, static or sleep
    // reads: the spawner output is a fresh falling prop, and those bools may not be initialised
    // yet before the finish.
    p.physFlags = coop::net::propspawn_flags::kSimulatePhysics;
    p.initLinVelX = p.initLinVelY = p.initLinVelZ = 0.f;
    p.initAngVelX = p.initAngVelY = p.initAngVelZ = 0.f;
    p.elementId = static_cast<uint32_t>(eid);

    if (s->SendPropSpawn(p)) {
        // No save record: these ambient mirrors are broadcast KEYLESS (the eid is the identity, a
        // few lines up), and a record is addressed by Key. Nothing to carry either way -- a
        // spawner's fresh prop has no save state yet.
        UE_LOGI("host_spawn_watcher: MIRROR ambient spawn cls='%ls' eid=%u at (%.0f,%.0f,%.0f) "
                "-- client spawns + drops it under local physics",
                cls.c_str(), p.elementId, p.locX, p.locY, p.locZ);
    } else {
        UE_LOGW("host_spawn_watcher: SendPropSpawn failed for '%ls' eid=%u (channel busy?) -- "
                "client missed this spawn; despawn-watch still armed", cls.c_str(), p.elementId);
    }
    // Self-claim (an open connect-snapshot bracket must not sweep our fresh prop) and enrol the
    // death-watch for the lifespan expiry or consumption despawn.
    coop::join_membership_sweep::RecordClaimIfTracking(actor);
    WatchSpawnedProp(actor, p.elementId);
}

// The keyed spawn seam, a Func patch on the finish. A ProcessEvent POST observer here caught
// only the dispatched route (the sandbox menu, the toolgun); the drop and quick-slot place
// run the inventory's EX_CallMath finish, invisible to a POST observer, so drops waited for
// the safety census. The Func patch funnels every dispatch route, the pile-thunk precedent,
// and strictly supersedes the observer. The callback only enqueues: at the finish's return
// inside the take verb the key is still the placeholder (the load restores the saved key
// after the finish; the take bracket exists for exactly this), and the hand's view actor has
// not yet been written to the held slot, which the hand exclusion below needs. The drain, next
// pump tick, adopts once the whole blueprint call has completed. Unkeyed entries wait for
// at most eight further passes; an early Init mark is retired only for an untracked, now
// keyed host birth. Proven player-authored floor beds also initialize their missing native
// key; other props keep the native key, without inventing an identity for their contents.
struct PendingFinishedSpawn {
    ue_wrap::CachedObjRef actor;
    bool initializeFloorKey = false;
    int tries = 0;
    bool coastBirth = false;
    PendingFinishedSpawn(void* a, bool floorKey, bool coast) : initializeFloorKey(floorKey), coastBirth(coast) { actor.Set(a); }
};
std::vector<PendingFinishedSpawn> g_pendingFinished;  // GT-only
constexpr size_t kMaxPendingFinished = 128;           // runaway backstop (level-load bursts)
uint64_t g_pendingEpoch = 0;
bool g_draining = false;
struct DrainScope {
    DrainScope() { g_draining = true; }
    ~DrainScope() { g_draining = false; }
};
size_t g_capDrops = 0;

void OnFinishSpawnFunc(void* /*context*/, void* srcObj, void* result) {
    if (!GT::IsGameThread()) return;
    auto* s = LoadSession();
    if (!s || !s->connected()) return;
    if (s->role() != coop::net::Role::Host) return;        // host-only broadcaster
    // The host's own world start-up is not a live edit: the game makes the level's props then, and the
    // join's capture, held until the world has settled, expresses them as they stand.
    if (!coop::host_world_settle::Settled() && !s->AnyWorldReadyPeer()) return;
    void* actor = result;
    if (!actor || !R::IsLive(actor)) return;
    // Wire and display mirror spawns are marked before the finish and never enqueued, since
    // expressing a mirror as a fresh world prop is the dupe.
    if (coop::prop_echo_suppress::IsMirrorSpawn(actor)) return;
    coop::water_pour_sync::ObserveHostBirth(srcObj,actor);
    if (!ue_wrap::prop::IsDescendantOfProp(actor)) return;  // KEYED Aprop_C lineage only
    const bool tracked = coop::element::Registry::Get().EidForActor(actor) != coop::element::kInvalidId;
    // Init may already have enrolled this actor, but it deliberately publishes NO save data:
    // the caller restores charge/uses only after FinishSpawningActor returns. Queue tracked
    // actors as well so the next-tick drain can publish that final state without a second spawn.
    if (g_pendingFinished.size() >= kMaxPendingFinished) {
        ++g_capDrops;
        UE_LOGW("host_spawn_watcher: pending-finished cap %zu hit -- dropping %p (%s)", kMaxPendingFinished,
                actor, tracked ? "tracked: its fall and its contents are not given"
                               : "the safety census expresses it");
        return;
    }
    const bool floorBirth = ue_wrap::floor_bed::IsFloorBed(actor) &&
                           coop::prop_spawn_authoring::BirthIsPlayerAuthored();
    // Breakable prop gibs use this common gameplay factory. Save loading has its
    // own direct spawn path: never stream thousands of resting load-time props.
    // Capture provenance now; the caller frame no longer exists at the drain.
    const auto caller = ue_wrap::ufunction_hook::CurrentCallerFrame();
    void* owner = caller.function ? R::OuterOf(caller.function) : nullptr;
    const bool gameplayBirth = owner && R::NameEquals(R::NameOf(owner), P::name::GamemodeClass) &&
        R::NameEquals(R::NameOf(caller.function), L"spawnPropThroughGamemode");
    g_pendingFinished.emplace_back(actor, floorBirth, gameplayBirth || ue_wrap::water_pour::IsProjectile(actor));
}

void InstallKeyedFinish() {
    if (g_finishHookSettled) return;
    void* cls = ue_wrap::object_index::ClassByName(P::name::GameplayStaticsClass);
    if (!cls) return; // boot/index backlog; cheap retry, never scan for unloaded content
    g_finishHookSettled = true;
    g_finishSpawnFn = ue_wrap::facts::ResolveOn(kRowFinishSpawningActor, cls);
    if (!g_finishSpawnFn) {
        UE_LOGE("host_spawn_watcher: FinishSpawningActor missing -- keyed births cannot be observed");
        return;
    }
    g_finishReturnOff = R::FindParamOffset(g_finishSpawnFn.Fn(), L"ReturnValue");
    const int frame = R::FunctionFrameSize(g_finishSpawnFn.Fn());
    if (g_finishReturnOff < 0 || g_finishReturnOff > frame || sizeof(void*) > frame - g_finishReturnOff) {
        UE_LOGE("host_spawn_watcher: FinishSpawningActor return layout invalid -- keyed birth hook disabled");
        g_finishSpawnFn = {};
        return;
    }
    if (ue_wrap::ufunction_hook::InstallPostHook(g_finishSpawnFn, &OnFinishSpawnFunc))
        UE_LOGI("host_spawn_watcher: keyed FinishSpawningActor hook installed (independent of ambient props)");
    else
        UE_LOGE("host_spawn_watcher: keyed FinishSpawningActor hook FAILED -- check hook capacity");
}

}  // namespace

void SetSession(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    // Every keyed prop uses the finish hook. Missing pinecone/crystal assets, a
    // missing ambient signature or a full POST observer table cannot disable it.
    InstallKeyedFinish();
    if (g_observerRegistered) return;

    // Retry optional ambient binding about once a second; lookups use the live index.
    static int s_retry = 0;
    if (s_retry > 0) { --s_retry; return; }

    if (!g_beginDeferredFn) {
        void* gsCls = ue_wrap::object_index::ClassByName(P::name::GameplayStaticsClass);
        if (!gsCls) { s_retry = 60; return; }
        g_beginDeferredFn = ue_wrap::facts::Resolve(kRowBeginDeferredSpawn);
        if (!g_beginDeferredFn) {
            UE_LOGW("host_spawn_watcher: %ls.%ls not found -- disabled for process lifetime",
                    kRowBeginDeferredSpawn.Class(), kRowBeginDeferredSpawn.Name());
            g_observerRegistered = true;  // permanent give-up latch (don't re-walk)
            return;
        }
        g_classParamOff  = R::FindParamOffset(g_beginDeferredFn.Fn(), L"ActorClass");
        g_returnParamOff = R::FindParamOffset(g_beginDeferredFn.Fn(), L"ReturnValue");
        g_xformParamOff  = R::FindParamOffset(g_beginDeferredFn.Fn(), L"SpawnTransform");
        if (g_classParamOff < 0 || g_returnParamOff < 0 || g_xformParamOff < 0) {
            UE_LOGW("host_spawn_watcher: BeginDeferred ActorClass@%d ReturnValue@%d SpawnTransform@%d not found -- disabled",
                    g_classParamOff, g_returnParamOff, g_xformParamOff);
            g_observerRegistered = true;
            return;
        }
    }

    // Resolve the ambient-prop classes. A partial result is fine (unresolved slots stay null and
    // never match); retry until all bind, since they load on gameplay-level entry.
    if (!g_classesResolved) {
        size_t resolved = 0;
        for (size_t i = 0; i < P::name::kAmbientPropSpawnMirrorClassesSize; ++i) {
            if (!g_propClasses[i])
                g_propClasses[i] = ue_wrap::object_index::ClassByName(P::name::kAmbientPropSpawnMirrorClasses[i]);
            if (g_propClasses[i]) ++resolved;
        }
        if (resolved < P::name::kAmbientPropSpawnMirrorClassesSize) {
            s_retry = 60;
            return;  // wait for the full set before going live (no partial coverage)
        }
        g_classesResolved = true;
    }

    if (!GT::RegisterPostObserver(g_beginDeferredFn, &OnSpawnPost)) {
        UE_LOGE("host_spawn_watcher: RegisterPostObserver FAILED (observer table full?) -- disabled");
        g_observerRegistered = true;  // give up (don't retry a full table forever)
        return;
    }
    g_observerRegistered = true;
    UE_LOGI("host_spawn_watcher: POST observer registered on %ls @ %p "
            "(ActorClass@%d, ReturnValue@%d, %zu ambient-prop classes resolved)",
            kRowBeginDeferredSpawn.Name(), g_beginDeferredFn.Fn(),
            g_classParamOff, g_returnParamOff,
            P::name::kAmbientPropSpawnMirrorClassesSize);
}

void TickWatchedProps(coop::net::Session* s) {
    // Mutates the game-thread-only watch list and sends destroys. The caller is the pump on the
    // game thread, asserted locally so a future off-thread call site is caught at the boundary.
    // Peer-symmetric: each peer death-watches the ambient props it broadcast, and the watch list
    // only ever holds own spawns.
    UE_ASSERT_GAME_THREAD("host_spawn_watcher::TickWatchedProps");
    if (!s || !s->connected() || g_watched.empty()) return;
    for (size_t i = 0; i < g_watched.size();) {
        WatchedProp& w = g_watched[i];
        if (R::IsLiveByIndex(w.actor, w.internalIdx)) { ++i; continue; }
        // The actor died (the lifespan expired, or it was consumed): despawn the mirror.
        coop::net::PropDestroyPayload dp{};
        dp.key.len   = 0;        // key=None: eid-only
        dp.elementId = w.eid;
        s->SendPropDestroy(dp);
        UE_LOGI("host_spawn_watcher: watched ambient prop eid=%u died -> PropDestroy (despawn mirror)",
                w.eid);
        g_watched.erase(g_watched.begin() + i);
    }
}

// A birth this host simulates -- a spawn-menu prop in mid-air, a forage drop, a prop built from a
// client's intent -- falls under each peer's own physics, and two simulations of one fall part
// ways: a barrel that rolled on the host stood upright on the client. The fall goes out on the
// driven-prop stream until the body rests, and the stream's end parks every copy at the host's
// final pose. A birth that is not simulating and awake does not move and is left alone, and past the cap
// a burst of births -- a spawn-menu pile, an event's debris -- falls locally as it always did, so
// the stream's per-tick reads stay bounded.
constexpr size_t kMaxCoastingBirths = 32;
namespace {
void CoastIfFalling(void* actor) {
    namespace pf = coop::net::propspawn_flags;
    if (coop::prop_drive_host::CoastingCount() >= kMaxCoastingBirths) {
        static bool sSaid = false;
        if (!sSaid) {
            sSaid = true;
            UE_LOGW("host_spawn_watcher: %zu births already coasting -- a further birth falls locally "
                    "(said once)", kMaxCoastingBirths);
        }
        return;
    }
    if (PT::GetPropElementIdForActor(actor) == coop::element::kInvalidId) return;
    // The fall drill's red: a falling birth is not streamed, so no copy gets the host's final pose.
    static const bool s_noCoast = coop::config::ResolveFlag(coop::config_registry::rows::prop_fall_no_coast);
    if (s_noCoast) return;
    // Simulating and awake, which also means neither frozen, static nor asleep: a body born resting,
    // or one that has landed by the drain, opens no row.
    if (!(coop::prop_wire_parity::PhysFlagsOf(actor) & pf::kSimulatePhysics)) return;
    coop::prop_drive_host::Coast(actor, "host birth");
}
}  // namespace

void DrainPendingSpawns(coop::net::Session* s) {
    UE_ASSERT_GAME_THREAD("host_spawn_watcher::DrainPendingSpawns");
    if (g_pendingFinished.empty() || g_draining) return;
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) {
        g_pendingFinished.clear();
        return;
    }
    DrainScope scope;
    std::vector<PendingFinishedSpawn> batch, keep;
    batch.swap(g_pendingFinished);
    const uint64_t epoch = g_pendingEpoch;
    for (auto& e : batch) {
        if (epoch != g_pendingEpoch) return;
        void* actor = e.actor.Get();
        if (!actor) continue;
        const auto current = [&] {
            return epoch == g_pendingEpoch && s->connected() &&
                   s->role() == coop::net::Role::Host && e.actor.Get() == actor;
        };
        if (!current()) continue;
        // takeObj restores its saved identity after Finish returns. Its retained
        // publisher also owns refused spawn/state sends; do not send a record
        // here before that publisher has queued the birth.
        if (coop::prop_lifecycle::ContainerBirthPending(actor)) { keep.push_back(e); continue; }
        // Marked as a mirror after our enqueue check: not ours.
        if (coop::prop_echo_suppress::IsMirrorSpawn(actor)) continue;
        // The local hotbar hand actor, the hold update's view spawn: display-only while held, and
        // the hand edge in the hand-item module expresses it if and when it is released into the
        // world. Never adopted here.
        if (actor == coop::hand_item::LocalHandActor()) continue;
        const auto publishFinalRecord = [&] {
            const std::wstring key = ue_wrap::prop::GetInteractableKeyString(actor);
            if (!current() || key.empty() || key == L"None" ||
                coop::prop_echo_suppress::IsMirrorSpawn(actor) ||
                actor == coop::hand_item::LocalHandActor()) return;
            const auto id = coop::element::Registry::Get().EidForActor(actor);
            const auto* owner = coop::element::Registry::Get().Get(id);
            if (owner && !owner->IsMirror() && !owner->IsBeingDeleted()) {
                coop::props::container_birth::NoteHostBirth(actor);
                if (!current()) return;
                coop::prop_save_data::Publish(s, actor, key);
            }
        };
        const auto coastFinishedBirth = [&] {
            // The factory/physicsImpact assigns velocities AFTER Finish returns.
            // Enroll first, then stream debris until it settles at the host pose.
            if (!e.coastBirth || !current() || coop::prop_echo_suppress::IsMirrorSpawn(actor) ||
                coop::hand_item::IsHandAxisActor(actor) || ue_wrap::prop::IsStatic(actor) ||
                ue_wrap::prop::IsFrozen(actor)) return;
            const auto id = coop::element::Registry::Get().EidForActor(actor);
            const auto* owner = coop::element::Registry::Get().Get(id);
            if (owner && !owner->IsMirror() && !owner->IsBeingDeleted())
                coop::prop_drive_host::Coast(actor, "gameplay birth");
        };
        const auto prior = coop::element::Registry::Get().EidForActor(actor);
        if (prior != coop::element::kInvalidId) {
            publishFinalRecord();
            coastFinishedBirth();
            continue;
        }
        if (e.initializeFloorKey) {
            const bool ready = ue_wrap::floor_bed::EnsureKey(actor);
            if (!current()) continue;
            if (!ready) {
                if (++e.tries <= 8) keep.push_back(e);
                else UE_LOGW("host_spawn_watcher: floor bed key initialization failed after retries actor=%p", actor);
                continue;
            }
        }
        if (!current()) continue;
        // Native getKey may re-enter another binder/hand/mirror seam.
        if (coop::element::Registry::Get().EidForActor(actor) != coop::element::kInvalidId ||
            coop::prop_echo_suppress::IsMirrorSpawn(actor) || actor == coop::hand_item::LocalHandActor())
            continue;
        // Container break drops restore loadData AFTER FinishSpawningActor. An earlier
        // Init observer may have spent its dedupe mark before the final key existed,
        // without enrolling anything. The finished-spawn queue is explicit provenance:
        // retry only this live, host-owned, still-untracked actor after its key is ready.
        const std::wstring finalKey = ue_wrap::prop::GetInteractableKeyString(actor);
        if (!current()) continue;
        if (finalKey.empty() || finalKey == L"None") {
            if (++e.tries <= 8) keep.push_back(e);
            continue;
        }
        // Key lookup can re-enter a binder; never clear a successfully enrolled actor's
        // latch, nor publish a hand view or a mirror that appeared during that call.
        if (coop::element::Registry::Get().EidForActor(actor) != coop::element::kInvalidId ||
            coop::prop_echo_suppress::IsMirrorSpawn(actor) || actor == coop::hand_item::LocalHandActor())
            continue;
        PT::UnmarkProcessedInit(actor);
        coop::prop_lifecycle::ExpressSpawnedProp(actor);
        if (!current()) continue;
        if (PT::GetPropElementIdForActor(actor) != coop::element::kInvalidId) {
            // A spawn's saved fields are often filled after FinishSpawningActor returns. The
            // deferred drain runs after that Blueprint call, so publish the actual record beside
            // the identity express (not class defaults). This matters for a host-ejected disc
            // whose data is loaded from its slot JSON after spawn.
            publishFinalRecord();
            coastFinishedBirth();
            if (!current()) continue;
            UE_LOGI("host_spawn_watcher: spawn-seam adopted %p eid=%u (FinishSpawningActor "
                    "Func seam -- drop/place and saved state visible to peers this tick)",
                    actor,
                    static_cast<unsigned>(PT::GetPropElementIdForActor(actor)));
        }
    }
    if (epoch != g_pendingEpoch) return;
    for (auto& e : keep) {
        if (g_pendingFinished.size() < kMaxPendingFinished) g_pendingFinished.push_back(e);
        else UE_LOGW("host_spawn_watcher: pending finished-spawn retry cap reached actor=%p", e.actor.Raw());
    }
}

size_t PendingCapDrops() { return g_capDrops; }

void OnDisconnect() {
    ++g_pendingEpoch;
    g_watched.clear();
    g_pendingFinished.clear();
    g_session.store(nullptr, std::memory_order_release);
}

}  // namespace coop::host_spawn_watcher
