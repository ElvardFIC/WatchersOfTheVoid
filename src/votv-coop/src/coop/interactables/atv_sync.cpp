// coop/interactables/atv_sync.cpp -- see coop/interactables/atv_sync.h. The ATV (AATV_C) rig sync.
// The mirror simulates: a peer that does not sync an ATV runs the rig natively, physics and tick
// on, and is corrected toward the syncer through velocity, never frozen or teleported. AATV_C is
// a five-body constraint rig whose visible output is suspension travel, and SetActorLocation moves
// the root only, so a kinematic apply dragged four constrained bodies behind one teleported root.
// The one thing a mirror may not do is author collision damage (the hit guard). MTA's shape (a hard
// velocity write per packet, a warp past a speed-scaled threshold, a server-assigned syncer for an
// unoccupied vehicle that sends only on change) with one divergence: correction through velocity,
// since their vehicle is one rigid body. The seat (occupantSlot, which the mount deny reads) and the
// syncer (syncerSlot, who streams) are the host's table (atv_authority.cpp), never decided by the
// machine they concern: a client's block reaches the host alone, which re-sends it stamped, and every
// machine follows the stamp. A peer's own sit and grab are a request and a local body; it yields to
// the answer. Keyed by the game Key.

#include "coop/interactables/atv_sync.h"
#include "atv_spawn_park.h"
#include "coop/interactables/atv_authority.h"
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
#include "coop/player/roster_ledger.h"      // SubscribeSlotReplaced: a departed peer must not hold an ATV
#include "coop/session/net_pump.h"          // HasAnnouncedWorldReady: a client sends no request before it

#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"          // ReadMainPlayerGrabState (the grab) + Get/SetActorRootPhysicsVelocity
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
namespace AU = coop::atv_authority;
using ue_wrap::FVector;
using ue_wrap::FRotator;
using coop::net::WireKeyFromString;
using coop::net::StringFromWireKey;

constexpr uint64_t kIdleSendMs  = 200;  // the idle syncer's ceiling, 5 Hz, and only when it changed
// Never nothing: the change gate asks whether the sender's copy moved and is blind to the
// receiver, whose mirror can roll while the host's copy is parked, and a dropped connect snapshot
// would otherwise be permanent for an idle ATV. The gate lowers the rate; this floor keeps it
// non-zero.
constexpr uint64_t kIdleKeepaliveMs = 2000;
constexpr uint64_t kPassMs = 1000;      // the host's syncer pass, once a second

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

// True if this peer's player is grabbing `actor` with the grav hand (carried, not seated). The
// ATV has no grabbed flag of its own (isDriven and Player are written only on the seating path),
// so the grabber identity is the player's grabbing_actor or holding_actor, both accepted.
bool IsLocalGrabber(void* actor, void* localPlayer) {
    if (!localPlayer || !actor) return false;
    ue_wrap::engine::MainPlayerGrabState gs{};
    if (!ue_wrap::engine::ReadMainPlayerGrabState(localPlayer, gs)) return false;
    return gs.grabbingActor == actor || gs.holdingActor == actor;
}

namespace {

bool     g_installed    = false;  // latches the one-time index; Install is the per-tick ensure path
uint8_t  g_lastSeq      = 0;      // this client's one request counter (1..255, wrapping to 1)
uint64_t g_lastPassMs   = 0;      // the host's last syncer pass

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

// The next request number: one counter per client for its sits, leaves and takeovers.
uint8_t NextSeq() {
    g_lastSeq = g_lastSeq >= 255 ? uint8_t{1} : static_cast<uint8_t>(g_lastSeq + 1);
    return g_lastSeq;
}

// Whether this machine writes `e`: the host's table names it (on the host that is the host itself), or this
// client's own sit or takeover is sent and not yet answered (the optimistic window, the client drives at once).
bool IsLocalSyncer(const AtvEntry& e, uint8_t localSlot) {
    if (e.syncerSlot == localSlot) return true;
    return e.pendingSeq != 0 && e.pendingAction != AU::AtvRequest::Leave;
}

// An AtvStatePayload from a live read; false if the transform read fails. `grabbed` marks the
// sender's grav hand as holding it (bit 2); `occupantSlot` and `syncerSlot` are the table's values,
// the stamp a host block carries.
bool ReadPayload(void* actor, const std::wstring& key, uint8_t occupantSlot, uint8_t syncerSlot,
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
    p.syncerSlot   = syncerSlot;
    uint8_t sb = 0;
    if (A::IsDriven(actor)) sb |= 0x1;
    if (A::GetBrake(actor)) sb |= 0x2;
    if (grabbed)            sb |= 0x4;
    p.stateBits = sb;
    p.adopt = adopt ? 1 : 0;
    // The condition block (tires, spare, dirt, fuel, health), filled at this one site so the
    // syncer, the idle syncer and the adopt seed cannot fork; a failed read leaves it zeroed
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

// HOST: one block to every world-ready client but `skipSlot` (-1: none). A client whose world is not ready
// gets the connect snapshot instead, and none of the stream.
void SendToReadyClients(coop::net::Session& s, const coop::net::AtvStatePayload& p, int skipSlot) {
    for (uint8_t slot = 1; slot < coop::net::kMaxPeers; ++slot)
        if (slot != skipSlot && s.IsSlotWorldReady(slot))
            s.SendReliableToSlot(slot, coop::net::ReliableKind::AtvState, &p, sizeof(p));
}

// A peer whose seat or syncership we hold has left or been replaced (slots recycle lowest-free,
// so a slot can change occupant with no absence between, which is why this hangs off the
// ledger's row transition). Every ATV it sat in or synced goes back to the host's table: a seat or
// a syncer stuck on a departed peer means nobody ever syncs that ATV again. Slot 0, the host, is never a
// departure.
void OnSlotReplaced(int slot, const coop::roster_ledger::Row& /*outgoing*/,
                    const coop::roster_ledger::Row& /*incoming*/) {
    if (slot < 1 || slot > 0xFE) return;
    const uint8_t s8 = static_cast<uint8_t>(slot);
    if (slot < static_cast<int>(coop::players::kMaxPeers)) ClearOwed(slot);  // a leaver is owed no edge
    int freed = 0;
    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (e.occupantSlot == s8 || e.syncerSlot == s8) { AU::ClearTable(e); ++freed; }
        // The change gate re-arms: the other peers still hold the departed claim, and healing them
        // depends on the host sending something.
        e.haveLastSync = false;
    }
    if (freed > 0)
        UE_LOGI("atv: slot %d departed -- cleared the seat and syncer of %d ATV(s)", slot, freed);
}

void SubscribeDepartures() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::roster_ledger::SubscribeSlotReplaced(&OnSlotReplaced);
}

// This machine's own body against `e`, the one place the local seat and carry are written. A rising carry
// edge owes a takeover (a falling one sends nothing: the carrier stays the syncer until the ATV settles).
void ReadLocalBody(AtvEntry& e, void* localPlayer) {
    const bool wasCarrying = e.localCarrying;
    e.localSeated = IsLocalOccupant(e.actor, localPlayer);
    e.localCarrying = !e.localSeated && IsLocalGrabber(e.actor, localPlayer);   // seated or carrying, never both
    if (e.localCarrying && !wasCarrying) e.takeoverDue = true;
    else if (!e.localCarrying) e.takeoverDue = false;
}

// The game's own dismount for a seat the host did not give this machine; no leave follows it (it was never the
// host's occupant). The failure is said once: a dismount that cannot run repeats every frame.
void YieldSeat(AtvEntry& e, const std::wstring& key, const char* why) {
    if (!A::Dismount(e.actor)) {
        static bool sSaid = false;
        if (!sSaid) { sSaid = true;
            UE_LOGW("atv: the ATV's own dismount did not run for '%ls' (%s) -- the seat stays (said once)",
                    key.c_str(), why); }
        return;
    }
    e.sentSeated = false;
    UE_LOGI("atv: seat on '%ls' yielded -- %s", key.c_str(), why);
}

// The grav hand's grab of an ATV the host gave to another machine.
void YieldCarry(AtvEntry& e, void* localPlayer, const std::wstring& key, const char* why) {
    if (ue_wrap::engine::ReleaseMainPlayerGrabIfHolding(localPlayer, e.actor))
        UE_LOGI("atv: carry of '%ls' dropped -- %s", key.c_str(), why);
}

// HOST: the host's own body against the table, each step over every entry before the next (the map is
// unordered, so a host that leaves X and sits in Y inside one pump interval is judged leave-first). The host
// Tick runs before the pump's receive drain: a client request already in the table wins, one in the same
// frame's drain meets the host's verdict.
void JudgeHostBody(void* localPlayer) {
    for (auto& kv : g_atvs)                                  // (1) the body's seat and carry
        if (R::IsLiveByIndex(kv.second.actor, kv.second.idx)) ReadLocalBody(kv.second, localPlayer);
    for (auto& kv : g_atvs) {                                // (2) falling seat, rising seat, rising carry
        AtvEntry& e = kv.second;
        if (R::IsLiveByIndex(e.actor, e.idx) && !e.localSeated && e.sentSeated) {
            AU::JudgeLocal(e, AU::AtvRequest::Leave);
            e.sentSeated = false;
        }
    }
    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (R::IsLiveByIndex(e.actor, e.idx) && e.localSeated && !e.sentSeated) {
            AU::JudgeLocal(e, AU::AtvRequest::Sit);
            e.sentSeated = true;
        }
    }
    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (!R::IsLiveByIndex(e.actor, e.idx) || !e.takeoverDue) continue;
        e.takeoverDue = false;
        if (e.localCarrying) AU::JudgeLocal(e, AU::AtvRequest::Takeover);
    }
    for (auto& kv : g_atvs) {                                // (3) the host's yield on its own table
        AtvEntry& e = kv.second;
        if (!R::IsLiveByIndex(e.actor, e.idx)) continue;
        if (e.localSeated && e.occupantSlot != 0) YieldSeat(e, kv.first, "the seat is another's");
        if (e.localCarrying && e.syncerSlot != 0) YieldCarry(e, localPlayer, kv.first, "another syncs it");
    }
}

// HOST: one stamped block to every world-ready client for each entry whose table changed.
void SendStamps(coop::net::Session& s) {
    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (!e.stampDue || !R::IsLiveByIndex(e.actor, e.idx)) continue;
        coop::net::AtvStatePayload p{};
        if (!ReadPayload(e.actor, kv.first, e.occupantSlot, e.syncerSlot, /*adopt*/false, p)) continue;
        SendToReadyClients(s, p, -1);
        coop::atv_condition_sync::NoteSent(e, p);
        e.stampDue = false;
    }
}

// CLIENT: one request to the host, its seq the latest. The state moves only when the send did, so a refused
// send is retried by the next tick's edge.
void SendSeat(coop::net::Session& s, AtvEntry& e, const std::wstring& key, bool sit) {
    coop::net::AtvSeatPayload p{};
    WireKeyFromString(key, p.key);
    p.action = sit ? 1 : 0;
    p.seq = NextSeq();
    if (!s.SendReliable(coop::net::ReliableKind::AtvSeat, &p, sizeof(p))) return;
    e.pendingSeq = p.seq;
    e.pendingAction = sit ? AU::AtvRequest::Sit : AU::AtvRequest::Leave;
    e.sentSeated = sit;
}

void SendTakeover(coop::net::Session& s, AtvEntry& e, const std::wstring& key) {
    coop::net::AtvTakeoverPayload p{};
    WireKeyFromString(key, p.key);
    p.seq = NextSeq();
    if (!s.SendReliable(coop::net::ReliableKind::AtvTakeover, &p, sizeof(p))) return;
    e.pendingSeq = p.seq;
    e.pendingAction = AU::AtvRequest::Takeover;
    e.takeoverDue = false;
}

// CLIENT: this client's own body against `e`: read it, ask the host for what it did once its own world is
// ready (a level, so an edge before the gate is sent at it and never lost), then yield to what the host decided.
// The yield reads only a request this client has SENT, and only outside the window of an unanswered one, where
// only the answer moves it: a sit or grab made before the gate is neither yielded nor streamed.
void TickClientBody(coop::net::Session& s, AtvEntry& e, const std::wstring& key, void* localPlayer,
                    uint8_t localSlot, bool gateOpen) {
    ReadLocalBody(e, localPlayer);
    if (gateOpen) {
        if (e.localSeated && !e.sentSeated) SendSeat(s, e, key, true);
        else if (!e.localSeated && e.sentSeated) SendSeat(s, e, key, false);
        if (e.takeoverDue) SendTakeover(s, e, key);
    }
    if (e.pendingSeq != 0) return;
    if (e.sentSeated && e.localSeated && e.occupantSlot != localSlot)
        YieldSeat(e, key, "the host's stamp names another occupant");
    else if (e.localCarrying && !e.takeoverDue && e.syncerSlot != localSlot)
        YieldCarry(e, localPlayer, key, "the host's stamp names another syncer");
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
        SubscribeDepartures();  // a departed peer must not hold an ATV hostage
        g_installed = true;
    }
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
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) return;

    if (s->role() == coop::net::Role::Host) {
        // A client's block: applied only from the ATV's syncer, then re-sent to the other clients with the host's
        // stamp. The stamp is overwritten, never read from the sender, and the bits a client may not set are
        // masked (bit 3 is the host's own mark).
        if (!AU::AdmitBlock(e, payload, senderPeerSlot, NowMs())) return;
        coop::atv_corrector::ApplyCorrection(e, payload, /*snap*/ false);
        coop::atv_condition_sync::ApplyPayload(e, payload, /*fromHost*/ false);
        coop::net::AtvStatePayload out = payload;
        out.syncerSlot = e.syncerSlot;
        out.occupantSlot = e.occupantSlot;
        out.stateBits = static_cast<uint8_t>((payload.stateBits & 0x07) | coop::net::kAtvFromSyncer);
        out.adopt = 0;
        SendToReadyClients(*s, out, senderPeerSlot);
        return;
    }

    // A client takes the host's table FIRST, before any return: the stamp is what the host decided.
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    e.occupantSlot = payload.occupantSlot;
    e.syncerSlot   = payload.syncerSlot;
    // An ATV this machine syncs, or has a request in flight for, keeps its own state and takes only the stamp.
    if (e.syncerSlot == localSlot || e.pendingSeq != 0) return;
    // Pose and condition: the syncer's re-sent block, the host's own copy when the host is the syncer, or the
    // connect snapshot (a joiner syncs nothing yet, so it always places). A connect snapshot warps (the joiner has
    // not seen the world); a live packet corrects. The rig keeps simulating either way.
    const bool place = (payload.stateBits & coop::net::kAtvFromSyncer) != 0 || e.syncerSlot == 0 ||
                       payload.adopt != 0;
    if (!place) return;
    coop::atv_corrector::ApplyCorrection(e, payload, /*snap*/ payload.adopt != 0);
    // The condition block applies after the pose, behind the same gates; every block a client receives is the
    // host's, and its presence is consumed only for the host's own copy (atv_condition_sync.h).
    coop::atv_condition_sync::ApplyPayload(e, payload, /*fromHost*/ true);
}

void OnAnswer(const coop::net::AtvAnswerPayload& a) {
    const std::wstring key = StringFromWireKey(a.key);
    auto it = g_atvs.find(key);
    if (it == g_atvs.end()) {
        UE_LOGI("atv: answer for '%ls' (seq %u) with no entry -- older, dropped", key.c_str(),
                static_cast<unsigned>(a.seq));
        return;
    }
    AtvEntry& e = it->second;
    if (a.seq != e.pendingSeq) {
        UE_LOGI("atv: answer for '%ls' seq %u is not the latest request (%u) -- older, logged only", key.c_str(),
                static_cast<unsigned>(a.seq), static_cast<unsigned>(e.pendingSeq));
        return;
    }
    e.pendingSeq = 0;
    // No engine call before the actor is known live: on a dead entry the window only closes, and the
    // successor reset finishes the rest.
    if (!R::IsLiveByIndex(e.actor, e.idx)) return;
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    e.occupantSlot = a.occupantSlot;
    e.syncerSlot   = a.syncerSlot;
    UE_LOGI("atv: host %s '%ls' seq %u (reason %u) -- seat %u, syncer %u", a.admitted ? "granted" : "refused",
            key.c_str(), static_cast<unsigned>(a.seq), static_cast<unsigned>(a.reason),
            static_cast<unsigned>(a.occupantSlot), static_cast<unsigned>(a.syncerSlot));
    switch (e.pendingAction) {
    case AU::AtvRequest::Sit:
        if (e.localSeated && a.occupantSlot != localSlot) YieldSeat(e, key, "the host refused the sit");
        break;
    case AU::AtvRequest::Takeover:
        if (a.syncerSlot != localSlot)
            YieldCarry(e, coop::players::Registry::Get().Local(), key, "the host refused the takeover");
        break;
    case AU::AtvRequest::Leave:
        break;
    }
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
            // The joiner gets pose and velocity and warps to it, stamped with the host's table: it holds the seat and
            // the syncer from the first block, and places its copy once even when a client syncs the ATV. An ATV in
            // the air at a join arrives moving and lands.
            coop::net::AtvStatePayload p{};
            if (!ReadPayload(e.actor, h.key, e.occupantSlot, e.syncerSlot, /*adopt*/true, p)) continue;
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
    const bool gateOpen = !isHost && coop::net_pump::HasAnnouncedWorldReady();
    if (isHost) {
        JudgeHostBody(localPlayer);
        if (nowMs - g_lastPassMs >= kPassMs) { g_lastPassMs = nowMs; AU::RunPass(nowMs); }
        SendStamps(*s);
    }
    void* owned[coop::atv_hit_guard::kMaxOwned];
    int   ownedN = 0;

    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (!R::IsLiveByIndex(e.actor, e.idx)) continue;
        if (!isHost) TickClientBody(*s, e, kv.first, localPlayer, localSlot, gateOpen);

        // Tick ownership: this machine syncs the ATV (on the host: the table names the host), so it runs the
        // brain and the collision damage. There is no brain-off (parking the tick was measured useless, since
        // SetCenterOfMass runs unconditionally per tick, and the wheel torque and battery terms are already
        // single-peer by the game's own isDriven gates).
        const bool syncs = IsLocalSyncer(e, localSlot);
        // The eject drill, env-gated, once per process.
        coop::atv_eject_drill::MaybeFire(e.actor, kv.first.c_str(), nowMs, isHost, syncs, syncs);
        if (syncs) {
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
        if (!syncs) continue;   // a mirror does nothing more here; it is corrected at packet arrival

        const bool driving = e.localSeated || e.localCarrying;
        if (!isHost || driving) {
            // A client syncer streams at the drive rate whether it sits, carries or has only not been handed
            // back yet; the host streams at it while its own player drives or carries.
            if (nowMs - e.lastSentMs >= kDriveSendMs) {
                e.lastSentMs = nowMs;
                coop::net::AtvStatePayload p{};
                const uint8_t occ = isHost ? e.occupantSlot : uint8_t{0xFF};   // the host stamps; a client's is overwritten
                if (ReadPayload(e.actor, kv.first, occ, localSlot, /*adopt*/false, p, /*grabbed*/e.localCarrying)) {
                    if (isHost) SendToReadyClients(*s, p, -1);
                    else s->SendReliable(coop::net::ReliableKind::AtvState, &p, sizeof(p));
                    coop::atv_condition_sync::NoteSent(e, p);  // keep the idle gate's baseline fresh across a drive-to-idle handoff
                }
            }
        } else if (nowMs - e.lastSentMs >= kIdleSendMs) {
            // The idle syncer (the host, nobody driving): a slower cadence and a change gate, so a
            // parked ATV costs nothing while one rolling down a hill still converges.
            // The clock bumps once the window elapses, as MTA's does, not only on a send:
            // bumped only on a send it stayed stale for the parked ATV, and the gate then ran
            // its dispatches at the pump rate.
            e.lastSentMs = nowMs;
            // The payload is built before the gate so the condition block can vote: a parked
            // host eject (a mask flip, no motion) would otherwise wait out the keepalive.
            coop::net::AtvStatePayload p{};
            const bool readable  = ReadPayload(e.actor, kv.first, e.occupantSlot, /*syncerSlot*/0, /*adopt*/false, p);
            const bool changed   = IdleWorthSending(e);
            const bool condMoved = readable && coop::atv_condition_sync::CondChangedSinceLastSend(e, p);
            const bool keepalive = nowMs - e.lastIdleSendMs >= kIdleKeepaliveMs;
            if (readable && (changed || condMoved || keepalive)) {
                e.lastIdleSendMs = nowMs;
                SendToReadyClients(*s, p, -1);
                coop::atv_condition_sync::NoteSent(e, p);
            }
        }
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
    g_lastSeq = 0;
    g_lastPassMs = 0;
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
    AU::LogSummary();
    AU::Reset();
}

bool OwnsTick(void* actor) {
    // The set, the latch and the answer live with the collision guard (atv_hit_guard::Owns).
    return coop::atv_hit_guard::Owns(actor);
}

// Whether a remote peer occupies this ATV; the mount deny reads it. The occupant is the host's table, stamped.
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
