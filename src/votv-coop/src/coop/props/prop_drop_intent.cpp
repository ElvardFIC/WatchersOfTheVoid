// coop/props/prop_drop_intent.cpp -- see coop/props/prop_drop_intent.h for the design.

#include "coop/props/prop_drop_intent.h"

#include "coop/dev/prop_birth_key_probe.h"   // the seam's key-timing and drain-exit instrumentation
#include "coop/element/registry.h"          // EidForActor (drain: tracked/mirror exclusion)
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"          // LocalHandActor (place detect: exclude the hand display)
#include "coop/props/prop_echo_suppress.h"  // IsMirrorSpawn (exclude host-echo adopt spawns)
#include "coop/props/join_membership_sweep.h"// RecordSelfAuthored (a player's own prop is never divergence)
#include "coop/props/prop_lifecycle.h"      // IsPerPlayerPropClass / IsWireSuppressedPropClass (the host gate)
#include "coop/props/prop_save_data.h"
#include "coop/props/prop_food_sync.h"
#include "ue_wrap/actors/food.h"
#include "coop/props/prop_spawn_authoring.h" // BirthIsPlayerAuthored (the spawn menu / the toolgun)
#include "ue_wrap/actors/floor_bed.h"
#include "coop/props/prop_wire_parity.h"    // PhysFlagsOf, the one builder of a live prop's flags
#include "coop/props/prop_element_tracker.h"// GetPropElementIdForActor
#include "coop/props/prop_key_index.h"  // a key on the wire to a live actor
#include "coop/props/container_contents_sync.h"  // TakeObjInFlight -- mark a container-extraction birth
#include "coop/props/container_birth.h"           // a thrown container's birth slice
#include "coop/session/world_load_episode.h"  // InEpisode (quiet during the join loadObjects churn)
#include "ue_wrap/core/call.h"                   // ParamFrame + Call (setKey on the host re-spawn)
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/engine/engine.h"                 // BeginDeferredSpawn/FinishDeferredSpawn/SetActorScale3D
#include "ue_wrap/core/fname_utils.h"            // StringToFName
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/hot_path_guard.h"         // UE_ASSERT_GAME_THREAD
#include "ue_wrap/core/log.h"
#include "ue_wrap/actors/floppy_disc.h"
#include "ue_wrap/actors/prop.h"                   // the prop lineage, key, name and parity-identity accessors
#include "ue_wrap/core/object_index.h"  // ClassByName -- the host's O(1) class resolve for a client's intent
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"            // profile::name::{GameplayStaticsClass,FinishSpawningActorFn,PropSetKeyFn}
#include "ue_wrap/desk/tape_caddy.h"            // IsReelClass whitelist + the Progress birth scalar
#include "ue_wrap/desk/phys_mods.h"             // IsModuleClass whitelist
#include "ue_wrap/devices/serverbox.h"          // IsUpgradeClass whitelist
#include "coop/interactables/physmods_sync.h"   // the denied-birth reap
#include "coop/interactables/server_upgrade_sync.h"  // the refused take-out's reap
#include "coop/interactables/drive_payload_sync.h"  // NoteOwnDrive, NoteClientBrought: a drive a player brings into the world
#include "ue_wrap/desk/drive_chain.h"           // IsDriveClass whitelist
#include "ue_wrap/core/types.h"
#include "ue_wrap/core/ufunction_hook.h"         // InstallPostHook (chains after host_spawn_watcher's)
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

namespace coop::prop_drop_intent {
namespace {

namespace P  = ue_wrap::profile;
namespace R  = ue_wrap::reflection;
namespace E  = ue_wrap::engine;
namespace GT = ue_wrap::game_thread;
namespace PT = coop::prop_element_tracker;

UE_FACT_FN(kRowFinishSpawningActor, "prop_drop_intent", L"GameplayStatics", L"FinishSpawningActor", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};
inline coop::net::Session* LoadSession() { return g_session.load(std::memory_order_acquire); }

// The client place-detection state, game thread only.
ue_wrap::facts::Resolved g_finishSpawnFn;  // GameplayStatics.FinishSpawningActor (resolved once)
bool  g_installed     = false;    // InstallPostHook done (chain once, not per tick)

struct PendingPlace {
    ue_wrap::CachedObjRef actor;
    int     tries = 0;   // bounded retries for key/state/transform readiness
    // True when this entry is the actor a container extraction just materialised (the
    // extraction-in-flight latch was live at enqueue). Admitted at drain as a host-authoritative
    // drop intent, since the client-extracted item's world actor is invisible to the host
    // otherwise (the fresh-birth whitelist covers only reel, module and drive births).
    bool    containerExtract = false;
    // True when a PLAYER's own spawn verb -- the sandbox spawn menu or the toolgun -- was on the
    // stack at the finish (coop/props/prop_spawn_authoring). Admitted at drain as an ordinary drop
    // intent: the class is whatever the catalog offered, so no whitelist can name it, and the
    // authorship is what makes it shared-world state rather than the per-peer churn every other
    // unmarked birth here is. Read at the seam because that is the only moment the VM still holds
    // the calling frame.
    bool    playerSpawn = false;
    bool    admitted = false;
    uint32_t requestId = 0;
    PendingPlace(void* value, bool extracted, bool authored)
        : containerExtract(extracted), playerSpawn(authored) { actor.Set(value); }
};
std::vector<PendingPlace> g_pending;         // GT-only
struct AwaitingHost {
    ue_wrap::CachedObjRef actor;
    std::wstring key, className;
    uint32_t requestId = 0;
    uint64_t until = 0;
    PlacementState state = PlacementState::Waiting;
    bool parked = false;
    bool recordSubmitted = false;
};
std::vector<AwaitingHost> g_awaitingHost;
uint32_t g_requestSequence = 0; // keep advancing across Reset; old replies cannot match new tickets
constexpr uint64_t kPlacementWaitMs = 10000;
uint64_t NowMs() {return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count());}
struct HostReply {
    coop::net::PropPlacementResultPayload packet{};
    bool complete = false, pending = false;
    std::wstring key;
};
std::array<std::deque<HostReply>, coop::net::kMaxPeers> g_replies;
std::array<uint64_t, coop::net::kMaxPeers> g_peerEpoch{};
std::array<uint32_t, coop::net::kMaxPeers> g_lastRequest{};
bool g_flushingReplies = false;
uint64_t g_pendingEpoch = 0;
bool g_draining = false;
const std::vector<PendingPlace>* g_activePending = nullptr;
uint64_t g_activePendingEpoch = 0;
struct ActivePendingScope {
    explicit ActivePendingScope(const std::vector<PendingPlace>& batch) {
        g_activePending = &batch;
        g_activePendingEpoch = g_pendingEpoch;
    }
    ~ActivePendingScope() { g_activePending = nullptr; }
};
struct DrainScope {
    DrainScope() { g_draining = true; }
    ~DrainScope() { g_draining = false; }
};
constexpr size_t kMaxPending  = 32;          // runaway backstop (a settled client rarely has >1 in flight)
constexpr int    kMaxKeyTries = 8;           // ~8 net-pump ticks (~64 ms) for the Key to restore, then give up

void FlushReplies(coop::net::Session& session) {
    if (g_flushingReplies) return;
    struct Scope { Scope(){g_flushingReplies=true;} ~Scope(){g_flushingReplies=false;} } scope;
    const auto epoch = g_pendingEpoch;
    int budget = 8;
    for (int slot = 1; slot < coop::net::kMaxPeers && budget; ++slot) {
        const auto peerEpoch = g_peerEpoch[slot];
        if (!session.IsSlotWorldReady(slot)) continue;
        for (size_t i = 0; i < g_replies[slot].size() && budget; ++i) {
            const auto row = g_replies[slot][i];
            if (!row.complete || !row.pending) continue;
            --budget;
            const bool sent = session.SendReliableToSlot(slot, coop::net::ReliableKind::PropPlacementResult,
                &row.packet, sizeof(row.packet));
            if (epoch != g_pendingEpoch || peerEpoch != g_peerEpoch[slot] || !session.connected() ||
                session.role() != coop::net::Role::Host) return;
            if (!sent) break;
            // A synchronous callback may append/evict cache entries. Never use
            // the old index after the transport call.
            for (auto& current : g_replies[slot])
                if (current.packet.requestId == row.packet.requestId) current.pending = false;
        }
    }
}

// The park set: keys the client locally destroyed (a pickup) and may re-place. A bounded FIFO
// set: the keyed-destroy note inserts, a matching place consumes, overflow evicts the oldest.
// The invariant it guards: only author a drop intent for a key whose pickup destroy already
// crossed to the host (the host destroyed its copy), so the host re-spawn creates exactly one
// prop. The set and the FIFO hold exactly the same keys, and every insert, consume and evict
// touches both: a consume that dropped only the set desynced them, a re-parked same-key prop
// then had two FIFO copies, and after enough cycles the evict popped a stale copy and erased
// the live entry, so the place stopped syncing.
std::unordered_set<std::wstring> g_parkedKeys;   // GT-only
std::deque<std::wstring>         g_parkFifo;      // GT-only (eviction order); mirrors g_parkedKeys 1:1
constexpr size_t kMaxParked = 64;

// Remove a key from both containers (a consume, or any targeted un-park), preserving the
// mirror invariant.
void UnparkKey(const std::wstring& key) {
    if (g_parkedKeys.erase(key) == 0) return;   // not parked -> FIFO can't hold it either (invariant)
    for (auto it = g_parkFifo.begin(); it != g_parkFifo.end(); ++it) {
        if (*it == key) { g_parkFifo.erase(it); break; }   // at most ONE copy by the invariant
    }
}

// Fill a fixed-size wire short string (the key and class-name fields share the shape).
template <size_t N>
void FillWireStr(uint8_t& len, char (&data)[N], const std::wstring& s) {
    len = 0;
    for (size_t i = 0; i < s.size() && i < (N - 1); ++i) data[len++] = static_cast<char>(s[i]);
}

std::wstring WireToWide(const uint8_t len, const char* data, size_t cap) {
    std::wstring w;
    const size_t n = (len < cap) ? len : cap;
    w.reserve(n);
    for (size_t i = 0; i < n; ++i) w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(data[i])));
    return w;
}

// The client finish-spawn post-hook: enqueue a fresh, untracked, non-echo keyed prop spawn in
// a settled session for place detection. Chains after the host spawn watcher's callback on
// the same UFunction, role-disjoint (that one is host-only, this client-only). Runs on the
// dispatching thread; the finish spawn is game-thread only in UE4 (actor construction).
void OnClientFinishSpawn(void* /*context*/, void* sourceObject, void* result) {
    if (!GT::IsGameThread()) return;
    // The probe's birth record comes before the session gates: the menu world's births after a quit
    // happen with no session, and they are what the pause-quit drill measures.
    if (coop::dev::prop_birth_key_probe::IsEnabled() && result && R::IsLive(result) &&
        ue_wrap::prop::IsDescendantOfProp(result))
        coop::dev::prop_birth_key_probe::NoteBirth(result);
    auto* s = LoadSession();
    if (!s || !s->connected()) return;
    if (s->role() != coop::net::Role::Client) return;      // host places broadcast via host_spawn_watcher
    // Was a player's own spawn verb on the stack? Asked FIRST, because it is the one question only
    // this instant can answer -- the VM's calling frame is gone by the drain -- and because it is
    // what exempts a birth from the load gates below.
    const bool playerAuthored = coop::prop_spawn_authoring::BirthIsPlayerAuthored(sourceObject);
    // The end condition: quiet during the load episode and the reconcile window's load-kind
    // segments (the join and reload bracket, whose spawn churn flooded this path; the mirror mark
    // below already scopes our own applies out). A mid-session bracket does not suppress: a
    // client's genuine place has no other delivery channel (the census express is host-only)
    // and the re-bracket sweep would doom it, so intent, host spawn, express, claim must flow.
    // The warning latch mirrors the destroy seam's.

    // A PLAYER-AUTHORED birth is exempt from both. They are a PROXY for "this is loader churn",
    // and authorship is the fact the proxy stood in for, so the proxy keeps its job for everything
    // else. Not an argument that a player cannot act here -- the episode runs to load-tail
    // quiescence, long after the player has a controller. It is that a deliberate creation is
    // never churn, and has no second delivery if dropped: the express is host-only, so a
    // suppressed one is lost for the session. Measured: the player's birth crossed while three
    // churn births in the following second were still refused.
    if (!playerAuthored) {
        if (coop::world_load_episode::InEpisode()) return;  // quiet during the join loadObjects churn
        if (coop::world_load_episode::InReconcileWindow() &&
            coop::world_load_episode::ReconcileWindowIsLoadKind()) {
            static uint32_t sWarned = 0;
            ++sWarned;
            if (sWarned <= 5 || (sWarned <= 100 && sWarned % 10 == 0) || sWarned % 100 == 0) {
                UE_LOGW("[PROP-DROP] client place-detection suppressed #%u (reconcile window, "
                        "kind=load -- bracket spawn churn; a genuine blind place would re-arrive "
                        "via rejoin)", sWarned);
            }
            return;
        }
    }
    void* actor = result;
    if (!actor || !R::IsLive(actor)) return;
    if (coop::prop_echo_suppress::IsMirrorSpawn(actor)) return;  // a host-authored mirror we adopt, not a place
    if (!ue_wrap::prop::IsDescendantOfProp(actor)) return;          // keyed Aprop_C lineage only
    if (PT::GetPropElementIdForActor(actor) != coop::element::kInvalidId) return;  // already tracked = not a fresh place
    // The hand-axis exclusion, the enqueue half (a fast path only, not load-bearing here: at
    // finish-spawn return the hold update has not yet written the holding actor, so the freshly
    // spawned hand view actor can pass this check; the drain-time re-check in Tick is the
    // authoritative one, the host spawn watcher's proven shape). The hand-axis test also covers
    // remote display mirrors.
    if (coop::hand_item::IsHandAxisActor(actor)) return;
    if (g_pending.size() >= kMaxPending) {
        UE_LOGW("[PROP-DROP] client pending-place cap %zu hit -- dropping %p", kMaxPending, actor);
        coop::dev::prop_birth_key_probe::NotePendingCapHit(actor);
        return;
    }
    // Was a container extraction in flight when this actor spawned? The extracted item's actor
    // materialises inside the take call, so the latch is live exactly here. Marks the entry as a
    // container-extraction birth, admitted at drain.
    const bool fromContainerExtract = (coop::props::container_contents_sync::TakeObjInCall() || coop::props::container_contents_sync::TakeObjWindowOpen());
    if (coop::dev::prop_birth_key_probe::IsEnabled()) {
        // The seam reading the probe exists for: is the Key there before any drain tick waits?
        coop::dev::prop_birth_key_probe::NoteEnqueue(
            actor, R::ClassNameOf(actor), ue_wrap::prop::GetInteractableKeyString(actor),
            fromContainerExtract);
    }
    g_pending.emplace_back(actor, fromContainerExtract, playerAuthored);
    if (fromContainerExtract)
        UE_LOGI("[PROP-DROP] CLIENT enqueued container-EXTRACT birth actor=%p (admitted at drain)", actor);
    if (playerAuthored) {
        // The mid-join answer (principle 8), and it is owed HERE rather than at the drain. The
        // divergence sweep dooms every live, unclaimed, in-universe keyed Aprop_C the host's
        // snapshot did not name -- which a prop this player made a moment ago is, until the host's
        // answer comes back. It is recorded as SELF-AUTHORED, not claimed: a claim is scoped to the
        // bracket now open and cleared when the next one opens, and a measured birth beat its
        // bracket by a second. Authorship is exact here and nowhere else -- an ambient birth
        // carries no mark, is never recorded, and is still swept, which is what keeps the client
        // adopting the host's world.
        coop::join_membership_sweep::RecordSelfAuthored(actor);
        UE_LOGI("[PROP-DROP] CLIENT enqueued PLAYER-AUTHORED birth actor=%p cls='%ls' "
                "(spawn menu or toolgun; admitted at drain, claimed against the join sweep)",
                actor, R::ClassNameOf(actor).c_str());
    }
}

// Host: spawn the authoritative prop by key at the transform. Mirrors the spawn receiver's
// spawn-by-key (deferred begin, set the key, write the parity identity, finish) but does not
// mark it a mirror, so the host's own finish-spawn watcher catches it and broadcasts the
// authoritative spawn to every peer. Returns the spawned actor, or null.
void* HostSpawnPlacedProp(const coop::net::PropDropIntentPayload& p, const std::wstring& cls,
                          const std::wstring& key, uint8_t authorSlot) {
    // Host resolving a CLIENT's placed-prop class: object_index, so an attacker's unknown class is an
    // O(1) miss, not a whole-array walk. Host world is always up, so the index is current.
    void* clsObj = ue_wrap::object_index::ClassByName(cls.c_str());
    if (!clsObj) {
        UE_LOGW("[PROP-DROP] HOST class '%ls' not found -- cannot spawn placed prop key='%ls'",
                cls.c_str(), key.c_str());
        return nullptr;
    }
    // The lineage gate, on the CLASS, before anything is spawned. This author is the one place a
    // client names a class the host then constructs, and the spawn-menu door widened its firing
    // set from four whitelisted lineages to the whole prop catalog -- so what the wire may name
    // has to be stated here rather than left to the callers. Prop lineage is the whole of it: the
    // parity write below already assumes it, the intent kinds are all prop lanes, and a creature
    // or event class arriving on this lane would run an unrelated begin-play on the host's world.
    // A client cannot reach this with a non-prop class through any built door; refusing is for the
    // wire, which is not a door.
    if (!ue_wrap::prop::IsClassDescendantOfProp(clsObj)) {
        UE_LOGW("[PROP-DROP] HOST refusing spawn intent from slot=%u: class '%ls' is not prop "
                "lineage (key='%ls')", authorSlot, cls.c_str(), key.c_str());
        return nullptr;
    }
    // Prop lineage is necessary and not sufficient. Two prop-lineage families are ones the host's
    // own express refuses to broadcast: a per-player class (the inventory container, which is one
    // actor per peer by design) and a wire-suppressed intermediate variant. Spawned from a wire
    // intent the host would construct them and then never express them -- one silent actor per
    // packet, accumulating in its world with nothing to retire it. A client reaches neither
    // through a built door; this states it for the wire, which is not a door.
    if (coop::prop_lifecycle::IsPerPlayerPropClass(cls) ||
        coop::prop_lifecycle::IsWireSuppressedPropClass(cls)) {
        UE_LOGW("[PROP-DROP] HOST refusing spawn intent from slot=%u: class '%ls' is per-player or "
                "wire-suppressed -- the host would build it and never express it (key='%ls')",
                authorSlot, cls.c_str(), key.c_str());
        return nullptr;
    }
    const ue_wrap::FVector  loc{p.locX, p.locY, p.locZ};
    const ue_wrap::FRotator rot{p.rotPitch, p.rotYaw, p.rotRoll};
    const uint64_t epoch = g_pendingEpoch;
    const uint64_t peerEpoch = g_peerEpoch[authorSlot];
    void* actor = E::BeginDeferredSpawn(clsObj, loc, rot);
    if (epoch != g_pendingEpoch || peerEpoch != g_peerEpoch[authorSlot]) return nullptr;
    if (!actor) {
        UE_LOGW("[PROP-DROP] HOST BeginDeferredSpawn('%ls') failed", cls.c_str());
        return nullptr;
    }
    ue_wrap::CachedObjRef identity;
    identity.Set(actor);
    const auto stillCurrent = [&] { return epoch == g_pendingEpoch &&
        peerEpoch == g_peerEpoch[authorSlot] && identity.Get() == actor; };
    if (p.hasFoodCondition > 1 ||
        (ue_wrap::food::IsFood(actor) != (p.hasFoodCondition == 1)) ||
        (p.hasFoodCondition && !coop::props::prop_food_sync::ValidCondition(
            p.foodTemperature, p.foodRipeness, p.foodUses))) {
        E::DestroyActor(actor);
        return nullptr; // never author a default portion from missing or malformed food data
    }
    // Set the key before finishing: the init inside the finish spawn's construction script mints
    // a fresh key unless one is already set, and writing our wire key first keeps the cross-peer
    // identity. The setter is resolved on the prop base class (its declaring class) and cached,
    // as the spawn receiver does: the function lookup is exact-owner with no superclass climb,
    // so a leaf-class resolve missed every subclass that does not redeclare the setter, and the
    // host spawn then auto-minted a key unequal to the client's, an identity split and a
    // host-side duplicate. Every wire class reaching here is prop lineage (the intent author
    // gates on it), so the base's setter is a valid member call on the spawned actor.
    static void* s_setKeyFn = nullptr;
    if (!s_setKeyFn) {
        if (void* propBase = R::FindClass(P::name::PropClass))
            s_setKeyFn = R::FindFunction(propBase, P::name::PropSetKeyFn);
    }
    void* setKeyFn = s_setKeyFn;
    if (setKeyFn) {
        const R::FName kf = ue_wrap::fname_utils::StringToFName(key);
        if (kf.ComparisonIndex != 0) {
            ue_wrap::ParamFrame sk(setKeyFn);
            if (!sk.SetRaw(L"Key", &kf, sizeof(kf)) || !ue_wrap::Call(actor, sk)) {
                UE_LOGW("[PROP-DROP] HOST setKey('%ls') failed on '%ls'", key.c_str(), cls.c_str());
            }
        }
    } else {
        UE_LOGW("[PROP-DROP] HOST setKey UFunction not found on '%ls' -- prop will auto-mint a Key", cls.c_str());
    }
    if (!stillCurrent()) return nullptr;
    const auto assignedKey = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!stillCurrent()) return nullptr;
    if (assignedKey != key) {
        E::DestroyActor(actor);
        return nullptr; // never finish/broadcast an auto-minted, unrelated identity
    }
    // The single-player parity identity (the props-table name and the static,
    // remove-without-respawn, frozen and sleep flags) before finishing: the init resolves the
    // true mesh, mass and collision from the name (empty gives the default white cube).
    if (ue_wrap::prop::IsDescendantOfProp(actor)) {
        const std::wstring nameW = WireToWide(p.propName.len, p.propName.data, sizeof(p.propName.data));
        R::FName nameRow{0, 0};
        if (!nameW.empty() && nameW != L"None") nameRow = ue_wrap::fname_utils::StringToFName(nameW);
        namespace pf = coop::net::propspawn_flags;
        ue_wrap::prop::WriteSpParityIdentity(
            actor, nameRow,
            (p.physFlags & pf::kStatic) != 0,
            (p.physFlags & pf::kRemoveWOrespawn) != 0,
            (p.physFlags & pf::kFrozen) != 0,
            (p.physFlags & pf::kSleep) != 0);
    }
    if (!stillCurrent()) return nullptr;
    // No mirror mark: the host finish-spawn watcher must see this and broadcast it.
    if (!E::FinishDeferredSpawn(actor, loc, rot)) {
        UE_LOGW("[PROP-DROP] HOST FinishDeferredSpawn('%ls') failed", cls.c_str());
        if (stillCurrent()) E::DestroyActor(actor);
        return nullptr;
    }
    if (!stillCurrent()) return nullptr;
    const auto finishedKey = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!stillCurrent()) return nullptr;
    if (finishedKey != key) {
        E::DestroyActor(actor);
        return nullptr;
    }
    // Construction initializes a full portion. Restore the client's LIVE values afterwards,
    // before the host poll or next-tick birth broadcast can publish those defaults.
    if (p.hasFoodCondition) {
        const bool restored = ue_wrap::food::Write(actor, {p.foodTemperature, p.foodRipeness, p.foodUses});
        if (!stillCurrent()) return nullptr;
        if (!restored) {
            E::DestroyActor(actor);
            return nullptr;
        }
        coop::props::prop_food_sync::ForgetPublished(key);
    }
    // Scale is a runtime transform (the deferred begin takes location and rotation only); apply
    // it after finishing and before the next-tick drain re-reads the scale for the broadcast.
    if (p.scaleX > 0.001f || p.scaleY > 0.001f || p.scaleZ > 0.001f) {
        // Clamped, because a scale is the one field on this intent a client can make absurd with
        // no other consequence to itself: the lane reads the client's own actor, so an honest
        // value is whatever the game gave it, and the bound only has to be past anything the game
        // does. Client-scoped by the project's rule -- the host's own spawns never come through
        // here.
        constexpr float kMaxScale = 100.0f;
        auto clamp = [](float v) { return v < 0.f ? 0.f : (v > kMaxScale ? kMaxScale : v); };
        E::SetActorScale3D(actor, ue_wrap::FVector{clamp(p.scaleX), clamp(p.scaleY), clamp(p.scaleZ)});
    }
    if (!stillCurrent()) return nullptr;
    // The prop's own save record, if the author's copy is already here. It usually is not -- it
    // rides behind this intent in the same FIFO -- and then it lands on this actor by Key once the
    // actor is indexed, which is what makes the store keyed by identity rather than by actor.
    // Until then this key is AWAITED: the host has just spawned a class-default copy, and
    // publishing that as canonical would overwrite the author's real state on the author's own
    // machine.
    if (!p.hasFoodCondition) {
        const bool applied = coop::prop_save_data::ApplyParked(actor, key);
        if (!stillCurrent()) return nullptr;
        if (!applied) coop::prop_save_data::ExpectRecordFor(actor, key, authorSlot);
    }
    return stillCurrent() ? actor : nullptr;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_installed) return;

    // Throttle a retry while GameplayStatics does not resolve: a class miss walks the whole array.
    static int s_retry = 0;
    if (s_retry > 0) { --s_retry; return; }

    if (!g_finishSpawnFn) {
        void* gsCls = R::FindClass(P::name::GameplayStaticsClass);
        if (!gsCls) { s_retry = 60; return; }
        g_finishSpawnFn = ue_wrap::facts::Resolve(kRowFinishSpawningActor);
        if (!g_finishSpawnFn) {
            UE_LOGW("prop_drop_intent: FinishSpawningActor UFunction not found -- client place-detect disabled");
            g_installed = true;  // permanent give-up (don't re-walk every tick)
            return;
        }
    }
    if (ue_wrap::ufunction_hook::InstallPostHook(g_finishSpawnFn, &OnClientFinishSpawn)) {
        UE_LOGI("prop_drop_intent: FinishSpawningActor post-hook installed (client place -> host DROP INTENT)");
    } else {
        UE_LOGW("prop_drop_intent: FinishSpawningActor post-hook FAILED (hook table full?) -- client place-detect off");
    }
    g_installed = true;
}

void Tick(coop::net::Session* session) {
    if (session && session->connected() && session->role() == coop::net::Role::Host) {
        FlushReplies(*session);
        return;
    }
    std::erase_if(g_awaitingHost, [](const AwaitingHost& row) {
        void* a = row.actor.Get();
        return !a || ((row.recordSubmitted || row.state == PlacementState::Refused) &&
            PT::GetPropElementIdForActor(a) != coop::element::kInvalidId);
    });
    const auto now = NowMs();
    for (auto& row : g_awaitingHost) {
        if ((row.state == PlacementState::Waiting || row.state == PlacementState::Accepted) && now >= row.until) {
            row.state = PlacementState::TimedOut;
            if (row.parked) NoteClientKeyedDestroy(row.key);
            UE_LOGW("[PROP-DROP] request=%u key='%ls' timed out; local actor retained for recovery, no fallback author",
                row.requestId, row.key.c_str());
        }
    }
    UE_ASSERT_GAME_THREAD("prop_drop_intent::Tick");
    if (g_pending.empty() || g_draining) return;
    DrainScope scope;
    if (!session || !session->connected() || session->role() != coop::net::Role::Client) {
        // Tally these before dropping them: an entry discarded here is one the seam accepted and
        // never resolved, and a count of enqueues that does not balance against the exits cannot
        // say which bound lost a prop.
        for (const PendingPlace& e : g_pending)
            coop::dev::prop_birth_key_probe::NoteDrainExit(e.actor.Raw(), "session-gone", e.tries,
                                                           std::wstring());
        g_pending.clear();
        return;
    }
    std::vector<PendingPlace> keep;
    // Native getters/publication can spawn another actor or reset this subsystem. Drain an owned
    // batch so callbacks can append safely, and never restore old-world entries after Reset.
    std::vector<PendingPlace> batch;
    batch.swap(g_pending);
    ActivePendingScope activePending(batch); // keep admitted work visible during native callbacks
    const uint64_t epoch = g_pendingEpoch;
    namespace probe = coop::dev::prop_birth_key_probe;
    const size_t drainCount = (std::min)(batch.size(), size_t{2});
    for (size_t i = 0; i < drainCount; ++i) {
        PendingPlace& e = batch[i];
        if (epoch != g_pendingEpoch) return;
        void* actor = e.actor.Get();
        if (!actor) {
            probe::NoteDrainExit(e.actor.Raw(), "died-before-drain", e.tries, std::wstring());
            continue;
        }
        const auto stillCurrent = [&] {
            return epoch == g_pendingEpoch && session->connected() &&
                   session->role() == coop::net::Role::Client && e.actor.Get() == actor;
        };
        if (IsLocalPlacement(actor)) {
            probe::NoteDrainExit(actor, "already-requested", e.tries, std::wstring());
            continue;
        }
        if (coop::element::Registry::Get().EidForActor(actor) != coop::element::kInvalidId) { // got tracked/bound
            probe::NoteDrainExit(actor, "already-tracked", e.tries, std::wstring());
            continue;
        }
        if (coop::prop_echo_suppress::IsMirrorSpawn(actor)) {                       // marked as a mirror since -> not a place
            probe::NoteDrainExit(actor, "late-echo", e.tries, std::wstring());
            continue;
        }
        // The drain-time hand-axis re-check: the enqueue-time check runs before the hold update
        // writes the holding actor (the host spawn watcher documents this window and excludes at
        // drain for the same reason). Without it a hold-to-pick-up's hand view husk, carrying the
        // item's parked key, authors a false drop intent: the host spawns a duplicate world prop
        // while the item is still in the player's hand, and the park is consumed. Drop the entry
        // permanently.
        if (coop::hand_item::IsHandAxisActor(actor)) {
            probe::NoteDrainExit(actor, "hand-axis-drop", e.tries, std::wstring());
            continue;
        }
        std::wstring key = ue_wrap::prop::GetInteractableKeyString(actor);
        if (!stillCurrent()) continue;
        if (e.playerSpawn && (key.empty() || key == L"None") &&
            ue_wrap::floor_bed::IsFloorBed(actor)) {
            // This floor surface overrides the prop construction script without
            // init/getKey. Waiting cannot create its identity. Saved/container
            // births and mirrors keep their existing restoration path.
            const bool initialized = ue_wrap::floor_bed::EnsureKey(actor);
            if (!stillCurrent()) continue;
            if (coop::element::Registry::Get().EidForActor(actor) != coop::element::kInvalidId ||
                coop::prop_echo_suppress::IsMirrorSpawn(actor) || coop::hand_item::IsHandAxisActor(actor))
                continue;
            if (initialized) key = ue_wrap::prop::GetInteractableKeyString(actor);
            if (!stillCurrent()) continue;
        }
        if (key.empty() || key == L"None") {
            // The key is not restored yet (the load runs after the finish). Re-defer a few ticks.
            if (++e.tries <= kMaxKeyTries) keep.push_back(e);
            else probe::NoteDrainExit(actor, "key-wait-expired", e.tries, std::wstring());
            continue;
        }
        probe::NoteKeyReadable(actor, e.tries);
        const bool parked = (g_parkedKeys.find(key) != g_parkedKeys.end());
        // The fresh births. A client's fresh prop spawn never broadcasts (the lifecycle's client
        // skip), so a caddy or reel-box eject on a client is a local-only ghost; an unparked
        // reel-class pending entry here is that birth (mirrors are excluded by the mirror mark above,
        // tracked actors by the eid check, and the actor already carries the key the init minted
        // inside the finish spawn). Author it host-side via the eject intent, the same spawn
        // author, class-whitelisted at the host. The whitelist widens to desk modules (the unplug
        // births a module into the hand, the same local-only-ghost class), to drives (a take that stays
        // in the world, a cheat's spawn; the row goes to the host from drive_payload_sync once the echo binds the
        // drive, so no birth scalar is needed), to floppy discs and to the
        // upgrade a server box's take-out hands the player.
        //
        // The disc is the one that is NOT born into a hand: a device's eject drops it in the world
        // at the slot's mouth. It needs the whitelist because the park below covers only a disc
        // this same client put into that device, and an eject reaches every other case.
        const bool isDiscBirth = ue_wrap::floppy_disc::EnsureResolved() &&
                                 ue_wrap::floppy_disc::IsDiscClass(R::ClassOf(actor));
        // An upgrade's birth that reaches here is a world birth too: one the take-out hands over dies inside the
        // pickup, so what comes is the one a full hand left at the player, and it falls where it is.
        const bool isUpgradeBirth = ue_wrap::serverbox::IsUpgradeClass(R::ClassOf(actor));
        const bool isSleepingBagBirth = ue_wrap::floor_bed::IsFloorBed(actor);
        // Proven world births take precedence over the class-only device
        // fallback. A crafted/spawned or container-extracted module is not an
        // unplug: do not force it asleep or apply a recent device-denial reap.
        const bool freshBirth = !parked && !e.playerSpawn && !e.containerExtract &&
            (isDiscBirth ||
             (ue_wrap::tape_caddy::EnsureResolved() &&
              ue_wrap::tape_caddy::IsReelClass(R::ClassOf(actor))) ||
             (ue_wrap::phys_mods::EnsureResolved() &&
              ue_wrap::phys_mods::IsModuleClass(R::ClassOf(actor))) ||
             (ue_wrap::drive_chain::EnsureResolved() &&
              ue_wrap::drive_chain::IsDriveClass(R::ClassOf(actor))) ||
             isUpgradeBirth || isSleepingBagBirth);
        // A container-extraction birth is admitted too: the client's take materialises the
        // extracted item as a world actor, and without this the fresh-birth whitelist (reel, module
        // and drive only) drops it at drain and the item never reaches the host's world. The host's
        // duplicate guard keeps the intent safe.
        //
        // And a PLAYER-AUTHORED birth, the spawn menu and the toolgun. It cannot be a class row:
        // the menu is the whole catalog, and every class on it is also what the world's own
        // spawners, morphs and impacts make for themselves. What admits it is the authorship the
        // seam read off the VM's calling frame, which the world's own births do not carry. It is
        // NOT a freshBirth: that kind means a device put an item in a hand and sleeps the host's
        // copy, while a menu prop is dropped in the world at the player's aim and must fall on the
        // host exactly as it falls here -- the transform block below sends its real physics state.
        if (!parked && !freshBirth && !e.containerExtract && !e.playerSpawn) {
            probe::NoteDrainExit(actor, "not-a-place-nor-whitelisted-birth", e.tries, key);
            continue;
        }
        e.admitted = true; // preserve a legitimate placement while capture/send retries
        // A failed native getter does not schedule its own retry. Keep this admitted birth
        // while its identity/session remain current; never send a fabricated transform.
        const auto retryTransform = [&](const char* reason) {
            if (!stillCurrent()) return;
            if (++e.tries <= kMaxKeyTries) keep.push_back(e);
            else probe::NoteDrainExit(actor, reason, e.tries, key);
        };
        ue_wrap::FVector loc{};
        if (!ue_wrap::engine::TryGetActorLocation(actor, loc)) {
            retryTransform("location-wait-expired");
            continue;
        }
        if (!stillCurrent()) continue;
        ue_wrap::FRotator rot{};
        if (!ue_wrap::engine::TryGetActorRotation(actor, rot)) {
            retryTransform("rotation-wait-expired");
            continue;
        }
        if (!stillCurrent()) continue;
        // Author the host-authoritative spawn intent (a place, or a fresh birth).
        coop::net::PropDropIntentPayload p{};
        const std::wstring cls = R::ClassNameOf(actor);
        FillWireStr(p.className.len, p.className.data, cls);
        FillWireStr(p.key.len, p.key.data, key);
        namespace pf = coop::net::propspawn_flags;
        if (ue_wrap::prop::IsDescendantOfProp(actor)) {
            const std::wstring nm = ue_wrap::prop::GetPropNameString(actor);
            FillWireStr(p.propName.len, p.propName.data, nm);
            p.physFlags = coop::prop_wire_parity::PhysFlagsOf(actor);  // the host reads its four identity bits
        }
        if (!stillCurrent()) continue;
        if (freshBirth) {
            // Born asleep on the host (no free fall; the held-prop pose stream takes over) -- for
            // the three lineages that are born INTO A HAND. A disc is not: its device drops it at
            // the slot's mouth with nobody holding it, and a sleeping host copy would park in mid
            // air while the client's own copy fell, and then drag the client's back up the moment
            // the pose stream took over. It falls on the host, which is the peer that owns it.
            // Not a clearing: the transform block above reads the prop's real physics state, so a
            // disc that has already come to rest during the key wait still crosses asleep, which
            // is where it is.
            if (!isDiscBirth && !isUpgradeBirth && !isSleepingBagBirth) p.physFlags |= pf::kSleep;
        }
        // A data drive this player brought into the world, born here or put back by any route, carries its row in its
        // data slot, which only this copy has: its row goes to the host once the host's echo binds it.
        if (ue_wrap::drive_chain::IsDriveClass(R::ClassOf(actor)))
            coop::drive_payload_sync::NoteOwnDrive(actor);
        const auto scl = ue_wrap::engine::GetActorScale3D(actor);
        if (!stillCurrent()) continue;
        p.locX = loc.X; p.locY = loc.Y; p.locZ = loc.Z;
        p.rotPitch = ue_wrap::NormalizeAxis(rot.Pitch);
        p.rotYaw   = ue_wrap::NormalizeAxis(rot.Yaw);
        p.rotRoll  = ue_wrap::NormalizeAxis(rot.Roll);
        p.scaleX = scl.X; p.scaleY = scl.Y; p.scaleZ = scl.Z;
        if (ue_wrap::food::IsFood(actor)) {
            ue_wrap::food::Condition food{};
            if (!ue_wrap::food::Read(actor, food) ||
                !coop::props::prop_food_sync::ValidCondition(food.temperature, food.ripeness, food.uses)) {
                if (++e.tries <= kMaxKeyTries) keep.push_back(e);
                else UE_LOGW("[PROP-DROP] cannot read valid food state for key='%ls' after retries", key.c_str());
                continue;
            }
            p.hasFoodCondition = 1;
            p.foodTemperature = food.temperature;
            p.foodRipeness = food.ripeness;
            p.foodUses = food.uses;
        }
        if (g_awaitingHost.size() >= 256) {
            retryTransform("placement-cap-expired");
            continue;
        }
        if (!e.requestId) {if (!++g_requestSequence) ++g_requestSequence; e.requestId = g_requestSequence;}
        p.requestId = e.requestId;
        // Publish the ticket before sending: a synchronous inbox callback must
        // be able to match a reply, and the actor is always identity-stamped.
        g_awaitingHost.push_back({e.actor, key, cls, p.requestId, now + kPlacementWaitMs, PlacementState::Waiting, parked});
        const bool sent = session->SendReliable(freshBirth ? coop::net::ReliableKind::ReelEjectIntent
                                         : coop::net::ReliableKind::PropDropIntent, &p, sizeof(p));
        if (!stillCurrent()) continue;
        if (!sent) {
            std::erase_if(g_awaitingHost, [&](const AwaitingHost& row){return row.requestId == p.requestId;});
            if (++e.tries <= kMaxKeyTries) keep.push_back(e);
            else UE_LOGW("[PROP-DROP] send refused for key='%ls' after retries", key.c_str());
            continue; // do not consume the parked key or publish a record without its birth
        }
        // Only the explicit host acceptance may release the saved state. Sending
        // it behind a refused/conflicting birth would overwrite the existing
        // host object that happens to own this key.
        probe::NoteDrainExit(actor,
                             freshBirth     ? "authored-fresh-birth"
                             : e.playerSpawn ? "authored-player-spawn"
                                             : "authored-drop-intent",
                             e.tries, key);
        UE_LOGI("[PROP-DROP] CLIENT authored %s key='%ls' cls='%ls' name='%ls' loc=(%.1f,%.1f,%.1f)%s",
                freshBirth     ? "FRESH-BIRTH intent"
                : e.playerSpawn ? "PLAYER-SPAWN intent"
                                : "drop intent",
                key.c_str(), cls.c_str(),
                WireToWide(p.propName.len, p.propName.data, sizeof(p.propName.data)).c_str(),
                p.locX, p.locY, p.locZ,
                coop::prop_save_data::Covers(actor) ? " +record" : "");
    }
    if (epoch != g_pendingEpoch) return;
    // Preserve old queued work before reentrant arrivals. A callback must not
    // displace an already accepted pending birth when the cap is reached.
    std::vector<PendingPlace> arrivals;
    arrivals.swap(g_pending);
    const auto requeue = [&](PendingPlace& e) {
        if (g_pending.size() < kMaxPending) g_pending.push_back(std::move(e));
        else probe::NoteDrainExit(e.actor.Raw(), "pending-cap-after-callback", e.tries, std::wstring());
    };
    for (auto& e : keep) requeue(e);
    for (size_t i = drainCount; i < batch.size(); ++i) requeue(batch[i]);
    for (auto& e : arrivals) requeue(e);
}

bool IsAwaitingHost(void* actor) {
    const auto state = StateFor(actor);
    return state == PlacementState::Waiting || state == PlacementState::Accepted;
}
PlacementState StateFor(void* actor) {
    if (actor) for (const auto& row : g_awaitingHost) if (row.actor.Get() == actor) return row.state;
    return PlacementState::None;
}
bool IsLocalPlacement(void* actor) {return StateFor(actor) != PlacementState::None;}
void OnPlacementResult(coop::net::Session& session, const coop::net::PropPlacementResultPayload& p, int senderSlot) {
    if (!session.connected() || session.role() != coop::net::Role::Client || senderSlot != 0 || !p.requestId ||
        p.code > static_cast<uint8_t>(coop::net::PropPlacementCode::SpawnFailed) ||
        p.reserved[0] || p.reserved[1] || p.reserved[2]) return;
    for (auto& row : g_awaitingHost) if (row.requestId == p.requestId && row.actor.Get()) {
        if (row.state != PlacementState::Waiting && row.state != PlacementState::TimedOut) return;
        row.state = p.code == static_cast<uint8_t>(coop::net::PropPlacementCode::Accepted)
            ? PlacementState::Accepted : PlacementState::Refused;
        if (row.state == PlacementState::Accepted) row.until = NowMs() + kPlacementWaitMs;
        else if (row.parked) NoteClientKeyedDestroy(row.key);
        UE_LOGI("[PROP-DROP] request=%u host result=%u key='%ls'", p.requestId, p.code, row.key.c_str());
        if (row.state == PlacementState::Accepted && !row.recordSubmitted) {
            row.recordSubmitted = true; // publish/capture may reenter or clear this vector
            const auto identity = row.actor;
            const auto key = row.key;
            const auto epoch = g_pendingEpoch;
            UnparkKey(key);
            void* actor = identity.Get();
            if (!actor) return;
            coop::join_membership_sweep::RecordClaimIfTracking(actor);
            if (epoch == g_pendingEpoch && identity.Get() == actor && session.connected())
                coop::prop_save_data::Publish(&session, actor, key); // this lane owns capture/send retries
        }
        return;
    }
}

bool IsPendingHostPlacement(void* actor) {
    auto* s = LoadSession();
    if (!actor || !s || !s->connected() || s->role() != coop::net::Role::Client) return false;
    for (const auto& row : g_pending)
        if (row.admitted && row.actor.Get() == actor) return true;
    if (g_activePending && g_activePendingEpoch == g_pendingEpoch)
        for (const auto& row : *g_activePending)
            if (row.admitted && row.actor.Get() == actor) return true;
    return false;
}

void* FindAwaitingHost(const std::wstring& key, const std::wstring& className) {
    for (const auto& row : g_awaitingHost) {
        if (row.key != key || row.className != className) continue;
        void* actor = row.actor.Get();
        if (actor && !coop::hand_item::IsHandAxisActor(actor)) return actor;
    }
    return nullptr;
}

void NoteClientKeyedDestroy(const std::wstring& key) {
    UE_ASSERT_GAME_THREAD("prop_drop_intent::NoteClientKeyedDestroy");
    if (key.empty() || key == L"None") return;
    if (g_parkedKeys.insert(key).second) {
        g_parkFifo.push_back(key);
        while (g_parkFifo.size() > kMaxParked) {
            coop::dev::prop_birth_key_probe::NoteParkEvict(g_parkFifo.front());
            g_parkedKeys.erase(g_parkFifo.front());
            g_parkFifo.pop_front();
        }
    }
}

namespace {
void HandlePlacement(coop::net::Session& session, const coop::net::PropDropIntentPayload& p,
                     uint8_t senderSlot, bool freshBirth) {
    using Code = coop::net::PropPlacementCode;
    if (!session.connected() || session.role() != coop::net::Role::Host || senderSlot == 0 ||
        senderSlot >= coop::net::kMaxPeers || !session.IsSlotWorldReady(senderSlot) || !p.requestId) return;
    auto& replies = g_replies[senderSlot];
    for (auto& row : replies) if (row.packet.requestId == p.requestId) {
        if (row.complete) row.pending = true;
        return; // includes reentry while native construction is still running
    }
    constexpr size_t kReplyLimit = 256;
    if (replies.size() >= kReplyLimit) {
        auto old = std::find_if(replies.begin(), replies.end(), [](const HostReply& row){
            return row.complete && !row.pending;
        });
        if (old == replies.end()) return; // no author without a reserved reply; client times out
        replies.erase(old);
    }
    const uint64_t epoch = g_pendingEpoch, peerEpoch = g_peerEpoch[senderSlot];
    const auto current = [&] {return epoch == g_pendingEpoch && peerEpoch == g_peerEpoch[senderSlot] &&
        session.connected() && session.role() == coop::net::Role::Host && session.IsSlotWorldReady(senderSlot);};
    replies.push_back({{p.requestId, static_cast<uint8_t>(Code::Invalid), {}}, false, false});
    const auto finish = [&](Code code) {
        if (!current()) return;
        for (auto& row : g_replies[senderSlot]) if (row.packet.requestId == p.requestId) {
            row.packet.code = static_cast<uint8_t>(code); row.complete = row.pending = true; break;
        }
    };
    // Once an old cached outcome is evicted, a replay must still never construct
    // a second actor. Requests advance on one ordered reliable lane, modulo wrap.
    const auto previous = g_lastRequest[senderSlot];
    if (previous && static_cast<int32_t>(p.requestId - previous) <= 0) {finish(Code::Invalid);return;}
    g_lastRequest[senderSlot] = p.requestId;
    const auto validString = [](uint8_t len, const char* bytes, size_t cap, bool required) {
        if (len >= cap || (required && !len)) return false;
        for (size_t i=0; i<len; ++i) if (bytes[i] == '\0') return false;
        return true;
    };
    const float transform[] = {p.locX,p.locY,p.locZ,p.rotPitch,p.rotYaw,p.rotRoll,p.scaleX,p.scaleY,p.scaleZ};
    if (!validString(p.className.len,p.className.data,sizeof(p.className.data),true) ||
        !validString(p.key.len,p.key.data,sizeof(p.key.data),true) ||
        !validString(p.propName.len,p.propName.data,sizeof(p.propName.data),false) ||
        p._pad[0] || p._pad[1] || p.hasFoodCondition > 1 ||
        !std::all_of(std::begin(transform),std::end(transform),[](float v){return std::isfinite(v);}) ||
        (p.hasFoodCondition && !coop::props::prop_food_sync::ValidCondition(p.foodTemperature,p.foodRipeness,p.foodUses))) {
        finish(Code::Invalid); return;
    }
    const std::wstring cls = WireToWide(p.className.len, p.className.data, sizeof(p.className.data));
    const std::wstring key = WireToWide(p.key.len, p.key.data, sizeof(p.key.data));
    if (key == L"None") {finish(Code::Invalid);return;}
    for (const auto& peerReplies : g_replies) for (const auto& row : peerReplies)
        if (!row.complete && row.key == key) {finish(Code::Conflict);return;}
    for (auto& row : g_replies[senderSlot])
        if (row.packet.requestId == p.requestId) row.key = key;
    void* clsObj = ue_wrap::object_index::ClassByName(cls.c_str());
    if (!clsObj) {finish(Code::SpawnFailed);return;}
    if (!ue_wrap::prop::IsClassDescendantOfProp(clsObj) || coop::prop_lifecycle::IsPerPlayerPropClass(cls) ||
        coop::prop_lifecycle::IsWireSuppressedPropClass(cls)) {finish(Code::Forbidden);return;}
    if (freshBirth) {
        const bool reel = ue_wrap::tape_caddy::EnsureResolved() && ue_wrap::tape_caddy::IsReelClass(clsObj);
        const bool module = ue_wrap::phys_mods::EnsureResolved() && ue_wrap::phys_mods::IsModuleClass(clsObj);
        const bool drive = ue_wrap::drive_chain::EnsureResolved() && ue_wrap::drive_chain::IsDriveClass(clsObj);
        const bool disc = ue_wrap::floppy_disc::EnsureResolved() && ue_wrap::floppy_disc::IsDiscClass(clsObj);
        const bool upgrade = ue_wrap::serverbox::IsUpgradeClass(clsObj);
        if ((!reel && !module && !drive && !disc && !upgrade && !ue_wrap::floor_bed::IsFloorBedClass(clsObj)) ||
            (module && coop::physmods_sync::HostShouldReapModuleBirth(senderSlot,clsObj)) ||
            (upgrade && coop::server_upgrade_sync::HostShouldReapUpgradeBirth(senderSlot))) {
            finish(Code::Forbidden);return;
        }
    }
    if (!current()) return;
    if (void* existing = coop::prop_key_index::Find(key)) {
        // An existing same-key object at a different position is not evidence
        // that this placement succeeded. Refuse without moving or duplicating it.
        finish(Code::Conflict); return;
    }
    void* actor = HostSpawnPlacedProp(p, cls, key, senderSlot);
    if (!current()) return;
    // A data drive the client brought in: its row, which only that client has, is the one the host takes for it.
    if (actor) coop::drive_payload_sync::NoteClientBrought(actor, senderSlot);
    if (actor) {
        UE_LOGI("[PROP-DROP] HOST spawned client-placed prop key='%ls' cls='%ls' slot=%u at (%.1f,%.1f,%.1f) "
                "-- FinishSpawn watcher broadcasts it this tick",
                key.c_str(), cls.c_str(), senderSlot, p.locX, p.locY, p.locZ);
    }
    finish(actor ? Code::Accepted : Code::SpawnFailed);
}
} // namespace

void OnPropDropIntent(coop::net::Session& session, const coop::net::PropDropIntentPayload& p,
                      uint8_t senderSlot) {
    UE_ASSERT_GAME_THREAD("prop_drop_intent::OnPropDropIntent");
    HandlePlacement(session,p,senderSlot,false);
}

void OnReelEjectIntent(coop::net::Session& session, const coop::net::PropDropIntentPayload& p,
                       uint8_t senderSlot) {
    UE_ASSERT_GAME_THREAD("prop_drop_intent::OnReelEjectIntent");
    HandlePlacement(session,p,senderSlot,true);
}

void Reset() {
    g_awaitingHost.clear();
    UE_ASSERT_GAME_THREAD("prop_drop_intent::Reset");
    ++g_pendingEpoch;
    for (auto& replies : g_replies) replies.clear();
    for (auto& epoch : g_peerEpoch) ++epoch;
    g_lastRequest.fill(0);
    // The drain's session gate normally empties this first; anything still here reached teardown
    // unresolved, and the tally says so rather than losing it.
    for (const PendingPlace& e : g_pending)
        coop::dev::prop_birth_key_probe::NoteDrainExit(e.actor.Raw(), "reset-dropped", e.tries,
                                                       std::wstring());
    g_pending.clear();
    g_parkedKeys.clear();
    g_parkFifo.clear();
}

void OnPeerLeft(int slot) {
    if (slot <= 0 || slot >= coop::net::kMaxPeers) return;
    ++g_peerEpoch[slot];
    g_replies[slot].clear();
    g_lastRequest[slot] = 0;
}

}  // namespace coop::prop_drop_intent
