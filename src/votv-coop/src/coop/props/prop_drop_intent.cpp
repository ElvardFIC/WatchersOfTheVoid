// coop/props/prop_drop_intent.cpp -- see coop/props/prop_drop_intent.h for the design.

#include "coop/props/prop_drop_intent.h"
#include "coop/comms/peer_action_feed.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_snapshot.h"

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
#include "coop/props/water_pour_sync.h"
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
#include <cstring>
#include <map>
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
    PendingPlace(void* value, bool extracted, bool authored)
        : containerExtract(extracted), playerSpawn(authored) { actor.Set(value); }
};
std::vector<PendingPlace> g_pending;         // GT-only
struct AwaitingHost {
    ue_wrap::CachedObjRef actor;
    std::wstring key, className;
    net::PropDropIntentPayload request{};
    net::ReliableKind kind = net::ReliableKind::PropDropIntent;
    uint64_t next = 0;
    bool accepted = false, recordSubmitted = false, parked = false;
};
std::vector<AwaitingHost> g_awaitingHost;
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
    if(coop::water_pour_sync::ObserveBirth(sourceObject,result))return;
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

#include "prop_drop_host.inc"
#include "prop_drop_replies.inc"

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
    UE_ASSERT_GAME_THREAD("prop_drop_intent::Tick");
    PlacementTick(session);
    std::erase_if(g_awaitingHost, [](const AwaitingHost& row) {
        void* a = row.actor.Get();
        return !a || (row.recordSubmitted && PT::GetPropElementIdForActor(a) != coop::element::kInvalidId);
    });
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
        if (IsAwaitingHost(actor) || IsRefusedPlacement(actor)) {
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
            else {
                probe::NoteDrainExit(actor, "key-wait-expired", e.tries, std::wstring());
                UE_LOGW("[PROP-DROP] role=client actor=%p cls='%ls' key-wait-expired tries=%d world=%u; no shared birth authored",
                    actor, R::ClassNameOf(actor).c_str(), e.tries, ue_wrap::world_identity::Generation());
            }
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
            else if (RetainForRecovery(e,key,reason)) probe::NoteDrainExit(actor, reason, e.tries, key);
            else keep.push_back(e); // a full recovery pool still cannot surrender this birth to the sweep
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
                !coop::props::prop_food_sync::ValidCondition(food.temperature, food.ripeness, food.uses,food.cookProgress)) {
                retryTransform("food-state-wait-expired");
                continue;
            }
            p.hasFoodCondition = 1;
            p.foodTemperature = food.temperature;
            p.foodRipeness = food.ripeness;
            p.foodUses = food.uses;
            p.foodCookProgress=food.cookProgress;
        }
        if (!++g_nextPlacementRequest) ++g_nextPlacementRequest;
        p.requestId = g_nextPlacementRequest;
        const auto kind = freshBirth ? net::ReliableKind::ReelEjectIntent : net::ReliableKind::PropDropIntent;
        if (g_awaitingHost.size() + g_refusedPlacements.size() >= 256) {
            keep.push_back(e);
            continue;
        }
        // Register before transport: a synchronous reply must find this ticket.
        g_awaitingHost.push_back({e.actor, key, cls, p, kind, PlacementNow() + 1000, false, false, parked});
        const bool sent = session->SendReliable(kind, &p, sizeof(p));
        if (!stillCurrent()) continue;
        if (!sent) {
            std::erase_if(g_awaitingHost,[&](const auto& row){return row.request.requestId == p.requestId;});
            retryTransform("send-wait-expired");
            continue; // do not consume the parked key or publish a record without its birth
        }
        if (!stillCurrent()) continue;
        // State publication waits for an explicit host acceptance.
        UE_LOGI("[PROP-DROP] role=client request=%u actor=%p key='%ls' cls='%ls' world=%u source=%s",
            p.requestId, actor, key.c_str(), cls.c_str(), ue_wrap::world_identity::Generation(),
            parked ? "parked-pickup" : e.containerExtract ? "container" : e.playerSpawn ? "player-spawn" : "device-eject");
        if (!stillCurrent()) continue;
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
    if (!actor) return false;
    for (const auto& row : g_awaitingHost) if (row.actor.Get() == actor) return true;
    return false;
}

bool IsRefusedPlacement(void* actor) {
    if (!actor) return false;
    for (const auto& row : g_refusedPlacements) if (row.Get() == actor) return true;
    return false;
}

void OnPlacementResult(const net::PropPlacementResultPayload& p, int senderSlot) {
    UE_ASSERT_GAME_THREAD("prop_drop_intent::OnPlacementResult");
    auto* session = LoadSession();
    if (!session || !session->connected() || session->role() != net::Role::Client || senderSlot != 0 ||
        p.code > static_cast<uint8_t>(net::PropPlacementCode::Retry)) return;
    if (p.key.len > sizeof(p.key.data) || p.className.len > sizeof(p.className.data)) return;
    const auto key = WireToWide(p.key.len,p.key.data,sizeof(p.key.data));
    const auto cls = WireToWide(p.className.len,p.className.data,sizeof(p.className.data));
    for (auto it = g_awaitingHost.begin(); it != g_awaitingHost.end(); ++it) {
        if (it->request.requestId != p.requestId || it->key != key || it->className != cls) continue;
        UE_LOGI("[PROP-DROP] role=client result=%u request=%u actor=%p key='%ls' cls='%ls' world=%u",
            p.code, p.requestId, it->actor.Get(), key.c_str(), cls.c_str(), ue_wrap::world_identity::Generation());
        if (p.code != static_cast<uint8_t>(net::PropPlacementCode::Refused)) {
            it->next = PlacementNow() + 1000;
            if (p.code == static_cast<uint8_t>(net::PropPlacementCode::Accepted)) {
                it->accepted = true;
                PublishAccepted(session);
            }
            return; // Tick publishes the record using an owned copy, across native callbacks.
        }
        if (it->parked) NoteClientKeyedDestroy(it->key);
        void* actor = it->actor.Get();
        if (actor && PT::GetPropElementIdForActor(actor) == coop::element::kInvalidId)
            g_refusedPlacements.push_back(it->actor); // recover locally by picking it back up
        g_awaitingHost.erase(it);
        coop::peer_action_feed::AnnounceDirect(coop::players::Registry::Get().LocalPeerId(),
                                               coop::peer_action_feed::Action::PlacementRefused);
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
bool FreshPlacementAllowed(const net::PropDropIntentPayload& p, uint8_t senderSlot) {
    // The client fresh-birth author, class-whitelisted: reels (the caddy eject), desk modules
    // (the socket unplug), drives, floppy discs and server upgrades (a box's take-out). Not a general
    // client-spawn door; any other class here is a protocol violation, dropped.
    const std::wstring cls = WireToWide(p.className.len, p.className.data, sizeof(p.className.data));
    // Host resolving a CLIENT's eject class: object_index, host world up so the index is current.
    void* clsObj = ue_wrap::object_index::ClassByName(cls.c_str());
    const bool isReel = clsObj && ue_wrap::tape_caddy::EnsureResolved() &&
                        ue_wrap::tape_caddy::IsReelClass(clsObj);
    const bool isModule = clsObj && ue_wrap::phys_mods::EnsureResolved() &&
                          ue_wrap::phys_mods::IsModuleClass(clsObj);
    const bool isDrive = clsObj && ue_wrap::drive_chain::EnsureResolved() &&
                         ue_wrap::drive_chain::IsDriveClass(clsObj);
    const bool isDisc = clsObj && ue_wrap::floppy_disc::EnsureResolved() &&
                        ue_wrap::floppy_disc::IsDiscClass(clsObj);
    // A server box's take-out hands the player a new upgrade, born into the hand like a module.
    const bool isServerUpg = clsObj && ue_wrap::serverbox::IsUpgradeClass(clsObj);
    const bool isSleepingBag = clsObj && ue_wrap::floor_bed::IsFloorBedClass(clsObj);
    if (!isReel && !isModule && !isDrive && !isDisc && !isServerUpg && !isSleepingBag) {
        UE_LOGW("[PROP-DROP] HOST birth intent from slot=%u rejected: class '%ls' not whitelisted",
                senderSlot, cls.c_str());
        return false;
    }
    // A module birth matching a fresh unplug deny for this sender is the raced ghost that got
    // dropped before the deny landed; reap it (the physics-mods sync logs).
    if (isModule && coop::physmods_sync::HostShouldReapModuleBirth(senderSlot, clsObj)) return false;
    // The same for an upgrade a refused take-out handed over (the server upgrade sync logs).
    if (isServerUpg && coop::server_upgrade_sync::HostShouldReapUpgradeBirth(senderSlot)) return false;
    // Drive births are authored normally; a denied rack-take ghost is reaped when its row arrives, by the row's
    // hash (drive_payload_sync).
    return true;
}
void ReceivePlacement(net::Session& session, const net::PropDropIntentPayload& p,
                      uint8_t slot, net::ReliableKind kind) {
    if (!BeginPlacement(session,p,slot,kind)) return;
    const uint64_t epoch = g_pendingEpoch;
    void* world = ue_wrap::world_identity::CurrentWorld();
    const auto occupant = coop::roster_ledger::Get(slot).playerNo;
    const auto generation = coop::roster_ledger::Get(slot).bornGeneration;
    const auto current = [&] {
        return epoch == g_pendingEpoch && world == ue_wrap::world_identity::CurrentWorld() &&
            session.connected() && session.role() == net::Role::Host &&
            session.HasPeerConn(slot) && session.IsSlotWorldReady(slot) && occupant == coop::roster_ledger::Get(slot).playerNo &&
            generation == coop::roster_ledger::Get(slot).bornGeneration;
    };
    const auto cls = WireToWide(p.className.len,p.className.data,sizeof(p.className.data));
    const auto key = WireToWide(p.key.len,p.key.data,sizeof(p.key.data));
    if (!ValidPlacement(p) || key == L"None" ||
        (kind == net::ReliableKind::ReelEjectIntent && !FreshPlacementAllowed(p,slot))) {
        if (current()) FinishPlacement(slot,p.requestId,net::PropPlacementCode::Refused);
        return;
    }
    void* existing = nullptr;
    const auto indexed = coop::prop_key_index::Resolve(key,&existing);
    if (!current()) return;
    if (indexed == coop::prop_key_index::Read::Pending) {
        FinishPlacement(slot,p.requestId,net::PropPlacementCode::Retry);
        return;
    }
    if (existing) {
        // Only this transaction's cached success proves placement. A same-key
        // actor may belong to another placement and must never receive our record.
        FinishPlacement(slot,p.requestId,net::PropPlacementCode::Refused);
        return;
    }
    for (int peer = 1; peer < g_placementPeers.size(); ++peer)
        for (const auto& row : g_placementPeers[peer].records) {
            if ((peer == slot && row.request.requestId == p.requestId) ||
                WireToWide(row.request.key.len,row.request.key.data,sizeof(row.request.key.data)) != key) continue;
            if (row.processing || (row.code == net::PropPlacementCode::Accepted && row.actor.Get())) {
                // The key index may trail FinishSpawn. The transaction already
                // owns this identity; another request must not author a duplicate.
                FinishPlacement(slot,p.requestId,row.processing ? net::PropPlacementCode::Retry : net::PropPlacementCode::Refused);
                return;
            }
        }
    void* actor = HostSpawnPlacedProp(p,cls,key,slot);
    if (!current()) return;
    FinishPlacement(slot,p.requestId,actor ? net::PropPlacementCode::Accepted : net::PropPlacementCode::Refused,actor);
    if (actor) coop::drive_payload_sync::NoteClientBrought(actor,slot);
}
} // namespace

void OnPropDropIntent(net::Session& session, const net::PropDropIntentPayload& p, uint8_t slot) {
    UE_ASSERT_GAME_THREAD("prop_drop_intent::OnPropDropIntent");
    ReceivePlacement(session,p,slot,net::ReliableKind::PropDropIntent);
}
void OnReelEjectIntent(net::Session& session, const net::PropDropIntentPayload& p, uint8_t slot) {
    UE_ASSERT_GAME_THREAD("prop_drop_intent::OnReelEjectIntent");
    ReceivePlacement(session,p,slot,net::ReliableKind::ReelEjectIntent);
}
void OnPeerLeft(int slot) {
    if (slot > 0 && slot < g_placementPeers.size()) g_placementPeers[slot] = {};
}

void Reset() {
    g_awaitingHost.clear();
    UE_ASSERT_GAME_THREAD("prop_drop_intent::Reset");
    ++g_pendingEpoch;
    g_refusedPlacements.clear();
    g_placementWorld = ue_wrap::world_identity::CurrentWorld();
    for (int slot = 0; slot < g_placementPeers.size(); ++slot) g_placementPeers[slot] = {};
    // The drain's session gate normally empties this first; anything still here reached teardown
    // unresolved, and the tally says so rather than losing it.
    for (const PendingPlace& e : g_pending)
        coop::dev::prop_birth_key_probe::NoteDrainExit(e.actor.Raw(), "reset-dropped", e.tries,
                                                       std::wstring());
    g_pending.clear();
    g_parkedKeys.clear();
    g_parkFifo.clear();
}

}  // namespace coop::prop_drop_intent
