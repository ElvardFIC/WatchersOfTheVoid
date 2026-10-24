// coop/interactables/atv_sync.cpp -- see coop/interactables/atv_sync.h. The ATV (AATV_C) rig sync.
// The mirror simulates: a peer that does not author an ATV runs the rig natively, physics and tick
// on, and is corrected toward the authority through velocity, never frozen or teleported. AATV_C is
// a five-body constraint rig whose visible output is suspension travel, and SetActorLocation moves
// the root only, so a kinematic apply dragged four constrained bodies behind one teleported root.
// The one thing a mirror may not do is author collision damage (the hit guard). MTA's shape (a hard
// velocity write per packet, a warp past a speed-scaled threshold, an elected syncer for an
// unoccupied vehicle that sends only on change) with one divergence: correction through velocity,
// since their vehicle is one rigid body. Two predicates: IsPoseAuthor (I drive or carry it) and
// OwnsTick (that, or I am the host and nobody authors it); the seat (occupantSlot, which the
// mount deny reads) and the author (authorSlot, who streams) stay separate, since a grabbing
// peer must not deny the seat. Keyed by the game Key. The seat is self-elected, and two peers
// mounting inside one round-trip both elect themselves: the lower slot wins in OnReliable, a
// tie-break, not authority.

#include "coop/interactables/atv_sync.h"
#include "atv_spawn_park.h"
#include "coop/interactables/atv_corrector.h"
#include "coop/dev/atv_eject_drill.h"
#include "coop/interactables/atv_condition_sync.h"
#include "coop/interactables/atv_hit_guard.h"
#include "coop/interactables/atv_sync_internal.h"
#include "coop/interactables/atv_index_internal.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"  // WireKeyFromString / StringFromWireKey / FnvKey (shared)
#include "coop/player/players_registry.h"   // Registry::Local / LocalPeerId / kMaxPeers
#include "coop/player/roster_ledger.h"      // SubscribeSlotReplaced: a departed author must not hold an ATV

#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"          // ReadMainPlayerGrabState (grabber authority) + Get/SetActorRootPhysicsVelocity
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"      // IsSeeded / Backlog: whether the host's walk of its ATVs can be complete
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/world_identity.h"     // the generation-stamped index
#include "ue_wrap/core/types.h"           // FVector, FRotator, NormalizeAxis

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <chrono>

namespace coop::atv_sync {
namespace {

namespace R = ue_wrap::reflection;
namespace A = ue_wrap::atv;
using ue_wrap::FVector;
using ue_wrap::FRotator;
using coop::net::WireKeyFromString;
using coop::net::StringFromWireKey;

constexpr uint64_t kDriveSendMs = 50;   // about 20 Hz while a peer authors it
constexpr uint64_t kIdleSendMs  = 200;  // the idle syncer's ceiling, 5 Hz, and only when it changed
// Never nothing: the change gate asks whether the sender's copy moved and is blind to the
// receiver, whose mirror can roll while the host's copy is parked, and a dropped connect snapshot
// would otherwise be permanent for an idle ATV. The gate lowers the rate; this floor keeps it
// non-zero.
constexpr uint64_t kIdleKeepaliveMs = 2000;

// The idle syncer's change gate (MTA's WriteVehicleInformation in our units): a parked ATV sends
// nothing.
constexpr float kIdleMovedCm   = 1.0f;
constexpr float kIdleTurnedDeg = 0.5f;
constexpr float kIdleMovingCmS = 1.0f;

}  // namespace

namespace detail {

// Per-ATV state; a mirror is a simulating body whose velocity is biased at packet arrival, with
// no interpolation state. Game thread only.

std::atomic<coop::net::Session*> g_session{nullptr};

// g_atvs is game-thread only: Install, Tick, OnReliable, the connect snapshot and OnDisconnect run
// serially inside the pump. Tick mutates entry fields only; inserts and erases happen in the hub
// pass (scan_hub::Tick runs before this Tick) and in OnReliable (the drain, before the ticks).
std::unordered_map<std::wstring, AtvEntry> g_atvs;

}  // namespace detail

using detail::g_indexGen;
using detail::SendAtvSpawn;
using detail::ResetAnnounceState;
using detail::RegisterWithScanHub;
using detail::g_session;
using detail::g_atvs;

namespace {

bool     g_installed    = false;  // latches the one-time index; Install is the per-tick ensure path

// HOST: the slots owed the set edge (bit = slot). A snapshot that withheld the edge sets its slot's bit; the owed
// poll in Tick clears it once the edge goes, a new replay for the slot or the slot's departure clears it too.
// Each owed slot retries on its own interval (1 s, doubled after a retry that did not send the edge, up to 32 s) and
// says why a retry withheld once per reason; clearing the bit resets all three.
uint32_t g_setOwed = 0;
static_assert(coop::players::kMaxPeers <= 32, "g_setOwed holds one bit per slot");
constexpr uint64_t kOwedRetryMinMs = 1000;
constexpr uint64_t kOwedRetryMaxMs = 32000;
uint64_t    g_owedNextMs[coop::players::kMaxPeers] = {};
uint64_t    g_owedIntervalMs[coop::players::kMaxPeers] = {};
const char* g_owedSaid[coop::players::kMaxPeers] = {};   // the reason last logged for the slot's bit

void ClearOwed(int slot) {
    g_setOwed &= ~(1u << slot);
    g_owedNextMs[slot] = 0;
    g_owedIntervalMs[slot] = 0;
    g_owedSaid[slot] = nullptr;
}
// Arming starts the first retry a second away, never at the next Tick. `said` is the reason the snapshot already
// logged, so the first retry does not repeat it.
void ArmOwed(int slot, uint64_t nowMs, const char* said) {
    g_setOwed |= 1u << slot;
    g_owedIntervalMs[slot] = kOwedRetryMinMs;
    g_owedNextMs[slot] = nowMs + kOwedRetryMinMs;
    g_owedSaid[slot] = said;
}


// True if this peer's player is seated in `actor`, per the local engine.
bool IsLocalOccupant(void* actor, void* localPlayer) {
    return localPlayer && A::IsDriven(actor) && A::GetOccupantPlayer(actor) == localPlayer;
}

// True if this peer can claim or holds driver authority; a seat another peer holds cannot be
// claimed (the double-mount race).
bool CanClaimOrIsDriver(void* actor, void* localPlayer, uint8_t occupantSlot, uint8_t localSlot) {
    if (!IsLocalOccupant(actor, localPlayer)) return false;
    return occupantSlot == 0xFF || occupantSlot == localSlot;
}

// True if this peer's player is grabbing `actor` with the grav hand (carried, not seated). The
// ATV has no grabbed flag of its own (isDriven and Player are written only on the seating path),
// so the grabber identity is the player's grabbing_actor or holding_actor, both accepted.
bool IsLocalGrabber(void* actor, void* localPlayer) {
    if (!localPlayer || !actor) return false;
    ue_wrap::engine::MainPlayerGrabState gs{};
    if (!ue_wrap::engine::ReadMainPlayerGrabState(localPlayer, gs)) return false;
    return gs.grabbingActor == actor || gs.holdingActor == actor;
}

// Pose authority: this peer streams `actor`, driving or carrying it. Not tick ownership.
bool IsPoseAuthor(void* actor, void* localPlayer, uint8_t occupantSlot, uint8_t localSlot) {
    return CanClaimOrIsDriver(actor, localPlayer, occupantSlot, localSlot) ||
           IsLocalGrabber(actor, localPlayer);
}

// Tick ownership: the peer that authors the ATV, else the host (MTA's unoccupied-vehicle
// election). A different set from pose authority: on the host with nobody driving, IsPoseAuthor
// is false and this is true. Its two jobs are electing the idle syncer and feeding the collision
// guard's owned set; there is no brain-off (parking the tick was measured useless, since
// SetCenterOfMass runs unconditionally per tick, and the wheel torque and battery terms are
// already single-peer by the game's own isDriven gates).
bool OwnsTickFor(bool isPoseAuthor, bool isHost, uint8_t authorSlot) {
    return isPoseAuthor || (isHost && authorSlot == 0xFF);
}

// An AtvStatePayload from a live read; false if the transform read fails. `grabbed` marks the
// authority as the grav-hand grabber (bit 2); `authorSlot` names who holds it (0xFF = nobody,
// which elects the host as its idle syncer).
bool ReadPayload(void* actor, const std::wstring& key, uint8_t occupantSlot, uint8_t authorSlot,
                 bool adopt, coop::net::AtvStatePayload& p, bool grabbed = false) {
    FVector loc; FRotator rot;
    if (!A::GetRootTransform(actor, loc, rot)) return false;
    FVector lin{}, ang{};
    // Best effort: a failed read leaves zeros, the honest value and also a body at rest.
    ue_wrap::engine::GetActorRootPhysicsVelocity(actor, lin, ang);
    std::memset(&p, 0, sizeof(p));
    WireKeyFromString(key, p.key);
    p.x = loc.X; p.y = loc.Y; p.z = loc.Z;
    p.pitch = rot.Pitch; p.yaw = rot.Yaw; p.roll = rot.Roll;
    p.linVelX = lin.X; p.linVelY = lin.Y; p.linVelZ = lin.Z;
    p.angVelX = ang.X; p.angVelY = ang.Y; p.angVelZ = ang.Z;
    p.occupantSlot = occupantSlot;
    p.authorSlot   = authorSlot;
    uint8_t sb = 0;
    if (A::IsDriven(actor)) sb |= 0x1;
    if (A::GetBrake(actor)) sb |= 0x2;
    if (grabbed)            sb |= 0x4;
    p.stateBits = sb;
    p.adopt = adopt ? 1 : 0;
    // The condition block (tires, spare, dirt, fuel, health), filled at this one site so the
    // authority, the idle syncer and the adopt seed cannot fork; a failed read leaves it zeroed
    // with tiresValid 0, and receivers touch nothing.
    coop::atv_condition_sync::FillPayload(actor, p);
    return true;
}




// The idle syncer's change gate: true if the ATV moved, turned or is moving; the baseline updates
// on a yes, so a slow drift accumulates into a send instead of being rounded away.
bool IdleWorthSending(AtvEntry& e) {
    FVector loc; FRotator rot;
    if (!A::GetRootTransform(e.actor, loc, rot)) return false;
    FVector lin{}, ang{};
    ue_wrap::engine::GetActorRootPhysicsVelocity(e.actor, lin, ang);
    bool send = !e.haveLastSync || Len(lin) > kIdleMovingCmS;
    if (!send) {
        const FVector d{ loc.X - e.lastSyncPos.X, loc.Y - e.lastSyncPos.Y, loc.Z - e.lastSyncPos.Z };
        send = Len(d) > kIdleMovedCm ||
               std::fabs(ue_wrap::NormalizeAxis(rot.Pitch - e.lastSyncRot.Pitch)) > kIdleTurnedDeg ||
               std::fabs(ue_wrap::NormalizeAxis(rot.Yaw   - e.lastSyncRot.Yaw))   > kIdleTurnedDeg ||
               std::fabs(ue_wrap::NormalizeAxis(rot.Roll  - e.lastSyncRot.Roll))  > kIdleTurnedDeg;
    }
    if (send) { e.lastSyncPos = loc; e.lastSyncRot = rot; e.haveLastSync = true; }
    return send;
}



// A peer whose seat or authorship we hold has left or been replaced (slots recycle lowest-free,
// so a slot can change occupant with no absence between, which is why this hangs off the
// ledger's row transition). Every ATV it held is released: an authorSlot stuck on a departed peer
// means nobody ever syncs that ATV again.
void OnSlotReplaced(int slot, const coop::roster_ledger::Row& /*outgoing*/,
                    const coop::roster_ledger::Row& /*incoming*/) {
    if (slot < 0 || slot > 0xFE) return;
    const uint8_t s8 = static_cast<uint8_t>(slot);
    if (slot < static_cast<int>(coop::players::kMaxPeers)) ClearOwed(slot);  // a leaver is owed no edge
    int freed = 0;
    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (e.occupantSlot == s8) { e.occupantSlot = 0xFF; ++freed; }
        if (e.authorSlot   == s8) { e.authorSlot   = 0xFF; ++freed; }
        // The change gate re-arms: the other peers still hold the departed claim, and healing them
        // depends on the host sending something.
        e.haveLastSync = false;
    }
    if (freed > 0)
        UE_LOGI("atv: slot %d departed -- freed %d ATV seat/author reservation(s)", slot, freed);
}

void SubscribeDepartures() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::roster_ledger::SubscribeSlotReplaced(&OnSlotReplaced);
}


}  // namespace

bool IndexCurrent() { return g_indexGen == ue_wrap::world_identity::Generation(); }

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    // Install is the per-tick ensure path (net_pump re-calls it until the class loads), latched so
    // the one-time registration happens once.
    if (!g_installed && A::EnsureResolved()) {
        RegisterWithScanHub();  // the hub builds the index on its own cadence
        // The condition layout is warmed here, with ATV_C resident, so its one-shot resolve (thirteen
        // offsets and four function lookups) lands at session setup rather than in the first apply frame.
        ue_wrap::atv_condition::Resolve();
        coop::atv_hit_guard::InstallHitGuard();      // the seven ComponentHit interceptors -- Tick refuses to run without them
        SubscribeDepartures();  // a departed author must not hold an ATV hostage
        g_installed = true;
    }
}

// A peer may name itself as an ATV's holder, never anyone else: authorSlot elects the idle syncer
// and every other peer's mount deny reads it, so an unattributed one is an authority assertion,
// and one packet a second from any client would pin an empty ATV as occupied for everyone. Client
// scoped: the host legitimately speaks for other peers in its snapshot and its relay, and the
// relay preserves the origin slot.
bool SenderMaySpeakFor(uint8_t senderSlot, uint8_t claimedSlot) {
    if (senderSlot == 0 || senderSlot == 0xFF) return true;  // host, or unattributed local path
    return claimedSlot == 0xFF || claimedSlot == senderSlot;
}

void OnReliable(const coop::net::AtvStatePayload& payload, uint8_t senderPeerSlot) {
    std::wstring key = StringFromWireKey(payload.key);
    if (key.empty()) { UE_LOGW("atv: OnReliable empty key -- dropping"); return; }
    if (!A::EnsureResolved()) return;
    if (!IndexCurrent()) return;  // a stale-generation index holds another world's ATVs; the stream re-sends
    // A non-finite guard before the engine writes.
    if (!std::isfinite(payload.x) || !std::isfinite(payload.y) || !std::isfinite(payload.z) ||
        !std::isfinite(payload.pitch) || !std::isfinite(payload.yaw) || !std::isfinite(payload.roll)) {
        UE_LOGW("atv: OnReliable non-finite pose -- dropping key='%ls'", key.c_str());
        return;
    }
    auto it = g_atvs.find(key);
    if (it == g_atvs.end()) return;  // not indexed yet -- the throttled rebuild will pick it up
    AtvEntry& e = it->second;
    if (!R::IsLiveByIndex(e.actor, e.idx)) return;

    void* localPlayer = coop::players::Registry::Get().Local();
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();

    // The simultaneous-mount tie-break, before the authority early-return that reads the seat: two
    // peers mounting inside one round-trip both elect themselves and each treats the other's stream
    // as an echo, a permanent double drive. The lower slot wins, a total order both hold; only a
    // genuine claim from a peer we outrank takes the seat, so an echo can never demote us.
    if (payload.occupantSlot != 0xFF && payload.occupantSlot < localSlot &&
        IsLocalOccupant(e.actor, localPlayer) &&
        (e.occupantSlot == 0xFF || e.occupantSlot == localSlot)) {
        UE_LOGI("atv: seat contention on '%ls' -- slot %u outranks local slot %u; yielding pose authority",
                key.c_str(), static_cast<unsigned>(payload.occupantSlot),
                static_cast<unsigned>(localSlot));
        e.occupantSlot = payload.occupantSlot;   // a mirror now; Tick's release edge is suppressed for a yield
    }

    // Our own authority ignores the incoming pose, so a relayed copy cannot fight the live drive.
    if (IsPoseAuthor(e.actor, localPlayer, e.occupantSlot, localSlot)) return;

    // The seat and author, if the sender may name them.
    if (!SenderMaySpeakFor(senderPeerSlot, payload.authorSlot) ||
        !SenderMaySpeakFor(senderPeerSlot, payload.occupantSlot)) {
        UE_LOGW("atv: slot %u named holder author=%u occ=%u on '%ls' -- refusing (a peer speaks "
                "only for itself)", static_cast<unsigned>(senderPeerSlot),
                static_cast<unsigned>(payload.authorSlot),
                static_cast<unsigned>(payload.occupantSlot), key.c_str());
        return;
    }
    e.occupantSlot = payload.occupantSlot;
    e.authorSlot   = payload.authorSlot;

    // A connect snapshot warps (the joiner has not seen the world); a live packet corrects. The rig
    // keeps simulating either way; the state machine between "adopt an idle ATV" and "mirror an
    // authored one" is where the release defect lived.
    coop::atv_corrector::ApplyCorrection(e, payload, /*snap*/ payload.adopt != 0);
    // The condition block applies after the pose, behind the same gates; presence consumption is
    // further gated inside on the host as sender (atv_condition_sync.h).
    coop::atv_condition_sync::ApplyPayload(e, payload, senderPeerSlot);
}

void OnAtvRelease(const coop::net::AtvReleasePayload& payload, uint8_t senderPeerSlot) {
    std::wstring key = StringFromWireKey(payload.key);
    if (key.empty()) { UE_LOGW("atv: OnAtvRelease empty key -- dropping"); return; }
    if (!A::EnsureResolved()) return;
    if (!IndexCurrent()) return;  // a stale-generation index holds another world's ATVs
    auto it = g_atvs.find(key);
    if (it == g_atvs.end()) return;  // not indexed yet -- nothing whose author we could clear
    AtvEntry& e = it->second;
    if (!R::IsLiveByIndex(e.actor, e.idx)) return;

    void* localPlayer = coop::players::Registry::Get().Local();
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();

    // Our own authority ignores a stale or echoed release.
    if (IsPoseAuthor(e.actor, localPlayer, e.occupantSlot, localSlot)) return;

    // Only the recorded author may clear the author (the host excepted), or any client could hand
    // itself the idle election or un-seat a driver.
    if (senderPeerSlot != 0 && senderPeerSlot != 0xFF &&
        e.authorSlot != 0xFF && e.authorSlot != senderPeerSlot) {
        UE_LOGW("atv: slot %u released '%ls' held by slot %u -- refusing",
                static_cast<unsigned>(senderPeerSlot), key.c_str(),
                static_cast<unsigned>(e.authorSlot));
        return;
    }

    // The whole handler: the seat and the author free. No physics write and no velocity, since
    // nothing was frozen and every AtvState carried the velocity; on the host this election makes
    // it the idle syncer on the next tick.
    e.occupantSlot = 0xFF;
    e.authorSlot   = 0xFF;
    UE_LOGI("atv: OnAtvRelease key='%ls' -- author cleared (the rig kept simulating throughout)",
            key.c_str());
}

// The mirror half (atv_runtime_mirror.cpp) reaches the map through these (atv_sync_internal.h);
// every structural change to it is made here or by the index's hub pass (atv_index.cpp).
coop::net::Session* LaneSession() { return g_session.load(std::memory_order_acquire); }

const AtvEntry* FindEntry(const std::wstring& key) {
    auto it = g_atvs.find(key);
    return it == g_atvs.end() ? nullptr : &it->second;
}

void AdoptClientMirror(const std::wstring& key, void* actor) {
    AtvEntry e{};
    e.actor = actor;
    e.idx = R::InternalIndexOf(actor);
    e.world = ue_wrap::world_identity::WorldOf(actor);
    e.isClientSpawnedMirror = true;
    g_atvs[key] = std::move(e);
}

void EraseEntry(const std::wstring& key) {
    g_atvs.erase(key);
}

namespace {

// What one run of the set half did. `edge` is the AtvSetComplete going out; `keys` the distinct keys it counted.
struct SetSent {
    bool     edge = false;
    int      spawns = 0;    // AtvSpawns sent
    int      poses = 0;     // AtvStates sent (the connect snapshot only)
    size_t   named = 0;     // ATVs the walk found
    uint32_t keys = 0;
    const char* why = nullptr;   // what withheld the edge; null when it went
};

// The set half: the host's ATVs themselves, walked from the object index (the lane's rows lag a birth by a pass and
// can hold a stale row under a live key, and the joiner removes whatever the set does not name), one AtvSpawn each,
// then the edge. `withPoses` is the connect snapshot: each ATV's AtvState follows its AtvSpawn, ahead of the edge. An
// owed retry (`!withPoses`) sends nothing while the walk still cannot be taken as complete, so a wait costs no wire.
SetSent SendSetToSlot(coop::net::Session* s, int peerSlot, bool withPoses) {
    SetSent r;
    struct Held { std::wstring key; void* actor; };
    std::vector<Held> held;
    const size_t loading = ForEachAnnounceableAtv([](void* ctx, void* actor, const std::wstring& key) {
        static_cast<std::vector<Held>*>(ctx)->push_back(Held{ key, actor });
    }, &held);
    r.named = held.size();
    // The walk is complete only if no ATV is still loading, the lane's index has passed this world and the host's
    // object index has applied its births: the joiner removes everything the set does not name.
    const char* incomplete = nullptr;
    if (loading > 0) incomplete = "an ATV is still loading on the host";
    else if (!IndexCurrent()) incomplete = "the host's ATV index has not completed a pass in this world";
    else if (!ue_wrap::object_index::IsSeeded()) incomplete = "the host's object index is not seeded";
    else if (ue_wrap::object_index::Backlog() > 0) incomplete = "the host's object index has births it has not applied";
    if (!withPoses && incomplete) { r.why = incomplete; return r; }
    void* localPlayer = coop::players::Registry::Get().Local();
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    // Every ATV is announced first on the same lane, so the joiner holds it under its Key before the pose
    // lands. Divergence from MTA, which builds a joiner's world from these packets alone
    // (reference/mtasa-blue/Server/mods/deathmatch/logic/CMapManager.cpp:195-297): ours also loads the
    // host's save, so a joiner adopts the ATV its save already holds, and the set's edge below lets it
    // drop the ones the save holds and this world does not.
    std::unordered_set<std::wstring> sentKeys;   // several save ATVs can share a key; the joiner's set is of keys
    for (const Held& h : held)
        if (SendAtvSpawn(h.key, h.actor, peerSlot)) { ++r.spawns; sentKeys.insert(h.key); }
    if (withPoses) {
        for (const Held& h : held) {
            // An ATV born inside the hub's pass window has no row yet: it is announced and the hub files it, its
            // stream following. The index is read here, never written.
            const auto row = g_atvs.find(h.key);
            if (row == g_atvs.end() || row->second.actor != h.actor) continue;
            AtvEntry& e = row->second;
            // The change gate re-arms for the joiner: the snapshot is fire-once and a fresh joiner is
            // the peer most likely to drop it, and a satisfied gate would then send nothing until the
            // host's copy moved.
            e.haveLastSync = false;
            // The joiner gets pose and velocity and warps to it. It needs who holds the ATV: the host
            // itself if it authors, else the recorded author (or nobody, which makes the host its
            // syncer). An ATV in the air at a join arrives moving and lands.
            const bool hostAuthors = IsPoseAuthor(e.actor, localPlayer, e.occupantSlot, localSlot);
            const uint8_t authorSlot = hostAuthors ? localSlot : e.authorSlot;
            coop::net::AtvStatePayload p{};
            if (!ReadPayload(e.actor, h.key, e.occupantSlot, authorSlot, /*adopt*/true, p)) continue;
            s->SendReliableToSlot(peerSlot, coop::net::ReliableKind::AtvState, &p, sizeof(p));
            ++r.poses;
        }
    }
    // The edge, behind the AtvSpawns on the same lane. Withheld whenever the walk cannot be taken as complete -- an
    // announce that did not go, or one of the reasons above.
    const char* withheld = r.spawns != static_cast<int>(held.size()) ? "an AtvSpawn was not sent" : incomplete;
    if (!withheld) {
        coop::net::AtvSetCompletePayload done{};
        done.count = static_cast<uint32_t>(sentKeys.size());
        if (s->SendReliableToSlot(peerSlot, coop::net::ReliableKind::AtvSetComplete, &done, sizeof(done))) {
            r.edge = true;
            r.keys = done.count;
        } else {
            withheld = "the edge was not sent";
        }
    }
    r.why = withheld;
    if (withheld && withPoses)
        UE_LOGW("atv: connect-snapshot to slot %d has no set edge -- %s; owed: retried while the slot is "
                "world-ready, from 1 s with a growing wait", peerSlot, withheld);
    return r;
}

}  // namespace

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;  // host-only snapshot
    if (peerSlot < 0 || peerSlot >= static_cast<int>(coop::players::kMaxPeers)) return;
    ClearOwed(peerSlot);   // this replay supersedes an owed one
    const uint64_t nowMs = NowMs();
    if (!A::EnsureResolved()) {
        ArmOwed(peerSlot, nowMs, "the ATV wrapper is not resolved");
        UE_LOGW("atv: connect-snapshot to slot %d withheld -- the ATV wrapper is not resolved; owed: retried while the "
                "slot is world-ready, from 1 s with a growing wait", peerSlot);
        return;
    }
    const SetSent r = SendSetToSlot(s, peerSlot, /*withPoses*/ true);
    if (!r.edge) ArmOwed(peerSlot, nowMs, r.why);
    UE_LOGI("atv: connect-snapshot -- sent %d ATV pose(s) (%d AtvSpawn announce(s); the set names %zu) to slot %d (of %zu indexed)",
            r.poses, r.spawns, r.named, peerSlot, g_atvs.size());
}

namespace {
// HOST, while a bit is set: each owed slot that is world-ready and due gets the set half again (the AtvSpawns the
// edge counts, then the edge; no poses, the snapshot sent those). The edge going clears the bit; a retry that did
// not send it doubles that slot's interval and says why, once per reason. Divergence from MTA,
// whose join packet is one counted batch and so is never withheld: this set is N reliable messages behind a walk
// that can be incomplete, so the host holds the edge as a debt
// (reference/mtasa-blue/Server/mods/deathmatch/logic/CMapManager.cpp:195-297).
void PollOwedSetEdges(coop::net::Session* s, uint64_t nowMs) {
    if (!g_setOwed) return;
    for (int slot = 0; slot < static_cast<int>(coop::players::kMaxPeers); ++slot) {
        if (!(g_setOwed & (1u << slot)) || nowMs < g_owedNextMs[slot] || !s->IsSlotWorldReady(slot)) continue;
        const SetSent r = SendSetToSlot(s, slot, /*withPoses*/ false);
        if (r.edge) {
            ClearOwed(slot);
            UE_LOGI("atv: the owed set edge to slot %d sent (%u key(s))", slot, r.keys);
            continue;
        }
        if (!g_owedSaid[slot] || std::strcmp(g_owedSaid[slot], r.why ? r.why : "") != 0) {
            g_owedSaid[slot] = r.why ? r.why : "";
            UE_LOGW("atv: the owed set edge to slot %d is withheld -- %s; retried with a growing wait", slot,
                    g_owedSaid[slot]);
        }
        g_owedIntervalMs[slot] = g_owedIntervalMs[slot] * 2 > kOwedRetryMaxMs ? kOwedRetryMaxMs
                                                                              : g_owedIntervalMs[slot] * 2;
        g_owedNextMs[slot] = nowMs + g_owedIntervalMs[slot];
    }
}
}  // namespace

void Tick() {
    // Every early return means this module is not deciding who owns what, and the collision guard
    // must then be inert: it suppresses damage by default, and armed against a stale set it would
    // make the local ATV invulnerable. Disarmed first; the path that earns it re-arms at the end.
    struct DisarmUnlessArmed {
        bool armed = false;
        ~DisarmUnlessArmed() {
            if (armed) return;
            coop::atv_hit_guard::SetActive(false);
            // The set clears too: OwnsTick is a public read, and a stale set would label a mirror
            // sample as an owner sample.
            coop::atv_hit_guard::PublishOwned(nullptr, 0);
        }
    } scope;

    if (!A::EnsureResolved()) return;
    RegisterWithScanHub();  // safety net for any order where Tick precedes Install
    if (!IndexCurrent()) return;  // index belongs to a dead world -- wait for the hub's next pass
    auto* s = g_session.load(std::memory_order_acquire);

    if (!s || !s->connected()) return;
    // Parked spawns go back to OnAtvSpawn only inside a gameplay world, which re-judges the rest of its readiness.
    if (s->role() != coop::net::Role::Host &&
        ue_wrap::world_identity::CurrentWorldKind() == ue_wrap::world_identity::WorldKind::Gameplay)
        spawn_park::DrainReady();
    void* localPlayer = coop::players::Registry::Get().Local();
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    const uint64_t nowMs = NowMs();
    // The set edge a connect snapshot withheld is owed to its slot; ahead of the guard's return, which is no
    // concern of the snapshot.
    if (s->role() == coop::net::Role::Host) PollOwedSetEdges(s, nowMs);

    // Fail closed: without the hit interceptors a non-owner would author damage on a simulating
    // rig, so nothing is mirrored and the ERROR at Install says why.
    if (!coop::atv_hit_guard::Armed()) return;

    const bool isHost = s->role() == coop::net::Role::Host;
    void* owned[coop::atv_hit_guard::kMaxOwned];
    int   ownedN = 0;

    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (!R::IsLiveByIndex(e.actor, e.idx)) continue;

        const bool isDriver  = CanClaimOrIsDriver(e.actor, localPlayer, e.occupantSlot, localSlot);
        const bool isGrabber = !isDriver && IsLocalGrabber(e.actor, localPlayer);  // mutually exclusive
        const bool authority = isDriver || isGrabber;

        // The authority-lost edge, with two causes handled oppositely: a dismount or ungrab frees
        // the ATV (the seat and author clear, and a release goes out); a yield (still seated, but
        // outranked by a lower slot) means someone else holds it, and clearing would erase the
        // winner's claim, re-claim next tick and flap.
        const bool yielded = IsLocalOccupant(e.actor, localPlayer) &&
                             e.occupantSlot != 0xFF && e.occupantSlot != localSlot;
        if (e.wasPoseAuthor && !authority && !yielded) {
            e.occupantSlot = 0xFF;
            e.authorSlot   = 0xFF;
            coop::net::AtvReleasePayload rp{};
            WireKeyFromString(kv.first, rp.key);
            s->SendReliable(coop::net::ReliableKind::AtvRelease, &rp, sizeof(rp));
            UE_LOGI("atv: authority released key='%ls' -- author cleared; the host now syncs it idle",
                    kv.first.c_str());
        }
        e.wasPoseAuthor = authority;

        if (authority) {
            if (isDriver) e.occupantSlot = localSlot;   // claim the seat locally
            e.authorSlot = localSlot;
        }

        // Tick ownership, a different question from pose authority.
        const bool ownsTick = OwnsTickFor(authority, isHost, e.authorSlot);
        // The eject drill, env-gated, once per process.
        coop::atv_eject_drill::MaybeFire(e.actor, kv.first.c_str(), nowMs, isHost, authority, ownsTick);
        if (ownsTick) {
            if (ownedN < coop::atv_hit_guard::kMaxOwned) {
                owned[ownedN++] = e.actor;
            } else {
                static bool sSaturated = false;
                if (!sSaturated) { sSaturated = true;
                    UE_LOGE("atv: owned-set saturated at %d -- ATV '%ls' and any beyond it will "
                            "have their OWN collisions suppressed on the peer that owns them",
                            coop::atv_hit_guard::kMaxOwned, kv.first.c_str()); }
            }
        }

        if (authority) {
            if (nowMs - e.lastSentMs >= kDriveSendMs) {
                e.lastSentMs = nowMs;
                coop::net::AtvStatePayload p{};
                const uint8_t occSlot = isDriver ? localSlot : uint8_t{0xFF};  // grabber: no seated driver
                if (ReadPayload(e.actor, kv.first, occSlot, localSlot, /*adopt*/false, p, /*grabbed*/isGrabber)) {
                    s->SendReliable(coop::net::ReliableKind::AtvState, &p, sizeof(p));
                    coop::atv_condition_sync::NoteSent(e, p);  // keep the idle gate's baseline fresh across a drive-to-idle handoff
                }
            }
        } else if (ownsTick) {
            // The idle syncer (the host, nobody driving): a slower cadence and a change gate, so a
            // parked ATV costs nothing while one rolling down a hill still converges.
            if (nowMs - e.lastSentMs >= kIdleSendMs) {
                // The clock bumps once the window elapses, as MTA's does, not only on a send:
                // bumped only on a send it stayed stale for the parked ATV, and the gate then ran
                // its dispatches at the pump rate.
                e.lastSentMs = nowMs;
                // The payload is built before the gate so the condition block can vote: a parked
                // host eject (a mask flip, no motion) would otherwise wait out the keepalive.
                coop::net::AtvStatePayload p{};
                const bool readable  = ReadPayload(e.actor, kv.first, e.occupantSlot, /*authorSlot*/0xFF, /*adopt*/false, p);
                const bool changed   = IdleWorthSending(e);
                const bool condMoved = readable && coop::atv_condition_sync::CondChangedSinceLastSend(e, p);
                const bool keepalive = nowMs - e.lastIdleSendMs >= kIdleKeepaliveMs;
                if (readable && (changed || condMoved || keepalive)) {
                    e.lastIdleSendMs = nowMs;
                    s->SendReliable(coop::net::ReliableKind::AtvState, &p, sizeof(p));
                    coop::atv_condition_sync::NoteSent(e, p);
                }
            }
        }
        // A mirror does nothing here; it is corrected at packet arrival.
    }

    // Published before arming, so the guard never runs against an unrefreshed set.
    coop::atv_hit_guard::PublishOwned(owned, ownedN);
    coop::atv_hit_guard::SetActive(true);
    scope.armed = true;
}

void OnDisconnect() {
    spawn_park::Clear();
    ResetArrivalState();
    for (int slot = 0; slot < static_cast<int>(coop::players::kMaxPeers); ++slot) ClearOwed(slot);
    // Disarmed first: nothing publishes an owned set from here, and a live hit must reach the game.
    coop::atv_hit_guard::SetActive(false);
    coop::atv_hit_guard::PublishOwned(nullptr, 0);
    for (auto& kv : g_atvs) {
        const bool live = R::IsLiveByIndex(kv.second.actor, kv.second.idx);
        if (kv.second.isClientSpawnedMirror) {
            if (live) A::DestroyMirror(kv.second.actor);   // a mirror this lane spawned is a coop artifact
        }
        // Nothing to restore: the lane disables neither tick nor physics, so a session leaves every
        // save ATV as it found it.
    }
    const size_t n = g_atvs.size();
    g_atvs.clear();
    ResetAnnounceState();
    g_installed = false;  // a new session re-indexes via the next Install (latched again)
    const auto c  = coop::atv_corrector::ReadCounters();
    const auto hg = coop::atv_hit_guard::ReadCounters();
    if (n > 0)
        UE_LOGI("atv: OnDisconnect -- cleared %zu ATV(s) (brains restored; spawned mirrors destroyed); "
                "hit guard: %s, %llu neutered / %llu allowed / %llu UNRESOLVED; "
                "corrector: %llu nudged / %llu warped "
                "/ %llu cut-on-stall / %llu parked-replace",
                n, hg.armed ? "armed" : "NEVER ARMED",
                static_cast<unsigned long long>(hg.neutered),
                static_cast<unsigned long long>(hg.allowed),
                static_cast<unsigned long long>(hg.unresolved),
                static_cast<unsigned long long>(c.corrections),
                static_cast<unsigned long long>(c.warps),
                static_cast<unsigned long long>(c.stallWarps),
                static_cast<unsigned long long>(c.restPlaces));
}

bool OwnsTick(void* actor) {
    // The set, the latch and the answer live with the collision guard (atv_hit_guard::Owns).
    return coop::atv_hit_guard::Owns(actor);
}

// Whether a remote peer occupies this ATV; the mount deny reads it.
bool IsOccupiedByOther(void* actor, uint8_t* outOccupantSlot) {
    if (!actor) return false;
    if (!IndexCurrent()) return false;   // a stale-generation index holds another world's ATVs
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    for (const auto& kv : g_atvs) {
        if (kv.second.actor == actor) {
            const uint8_t occ = kv.second.occupantSlot;
            if (occ != 0xFF && occ != localSlot) {
                if (outOccupantSlot) *outOccupantSlot = occ;
                return true;
            }
            return false;
        }
    }
    return false;
}

void* ActorSeatedBy(uint8_t slot) {
    if (slot == 0xFF) return nullptr;       // 0xFF is the empty seat, never a slot
    if (!IndexCurrent()) return nullptr;    // a stale-generation index holds another world's ATVs
    for (const auto& kv : g_atvs)
        if (kv.second.occupantSlot == slot && R::IsLiveByIndex(kv.second.actor, kv.second.idx)) return kv.second.actor;
    return nullptr;
}

}  // namespace coop::atv_sync
