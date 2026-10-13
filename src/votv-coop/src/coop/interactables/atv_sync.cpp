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
// mount deny reads) and the author (authorSlot, who streams) stay separate. A carried ATV
// denies mounting until released. Keyed by the save key. The seat is self-elected, and two peers
// mounting inside one round-trip both elect themselves: the lower slot wins in OnReliable, a
// tie-break, not authority.

#include "coop/interactables/atv_sync.h"
#include "coop/interactables/atv_corrector.h"
#include "coop/dev/atv_eject_drill.h"
#include "coop/interactables/atv_condition_sync.h"
#include "coop/interactables/atv_hit_guard.h"
#include "coop/interactables/atv_sync_internal.h"

#include "coop/net/intent_bucket.h"  // a sender's budget of service edits on the host
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"  // WireKeyFromString / StringFromWireKey / FnvKey (shared)
#include "coop/player/players_registry.h"   // Registry::Local / LocalPeerId / kMaxPeers
#include "coop/player/remote_player.h"      // the sender's puppet, for the host's reach check
#include "coop/player/roster_ledger.h"      // SubscribeSlotReplaced: a departed author must not hold an ATV

#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"          // ReadMainPlayerGrabState (grabber authority) + Get/SetActorRootPhysicsVelocity
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/engine/world_identity.h"     // the generation-stamped index
#include "coop/element/object_scan_hub.h"      // the shared sliced scan pass
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

namespace coop::atv_sync {
namespace {

namespace R = ue_wrap::reflection;
namespace A = ue_wrap::atv;
using ue_wrap::FVector;
using ue_wrap::FRotator;
using coop::net::WireKeyFromString;
using coop::net::StringFromWireKey;
using coop::net::FnvKey;

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

// A peer that does not author an ATV reports its own player's service of it (coop/interactables/
// atv_condition_sync): measured five times a second, and once more before an author's block
// overwrites the copy, and sent at most once per window, the measurements between merged.
constexpr uint64_t kServiceScanMs = 200;
constexpr uint64_t kServiceSendMs = 200;
// A service is the player's own use of a can, a toolbox or a tyre on the ATV, within arm's reach of
// it; a movement on a copy whose player is further away is never reported.
constexpr float kServiceReachCm = 600.f;
// A sender's edit budget on the host: a scan's worth a second, with a burst for the block-arrival
// scans of a second ATV.
constexpr coop::net::IntentBudget kEditBudget{8.f, 6.f};


// Per-ATV state; a mirror is a simulating body whose velocity is biased at packet arrival, with
// no interpolation state. Game thread only.

std::atomic<coop::net::Session*> g_session{nullptr};

// g_atvs is game-thread only: Install, Tick, OnReliable, the connect snapshot and OnDisconnect run
// serially inside the pump. Tick mutates entry fields only; inserts and erases happen in the hub
// pass (scan_hub::Tick runs before this Tick) and in OnReliable (the drain, before the ticks).
std::unordered_map<std::wstring, AtvEntry> g_atvs;
size_t   g_lastLogCount = SIZE_MAX;
uint64_t g_lastLogHash  = 0;
bool     g_installed    = false;  // latches the one-time index; Install is the per-tick ensure path
coop::net::IntentBucket g_editBuckets[coop::players::kMaxPeers];  // host: by sender slot

// Runtime-spawned ATV identity. Nothing sells an ATV, but list_props row 'atv' spawns an ATV_C
// from the spawn menu; such an ATV has no save twin on the other peers and its own save key is
// minted randomly per peer, so the host gives it a synthetic wire key ("coopatv#N") and announces
// it, and clients fresh-spawn a native AATV_C under that key. A save-placed ATV (a deterministic
// key both peers loaded) keeps its real key.
std::unordered_set<std::wstring>        g_savePlacedKeys;    // host: real keys seen before any client connected, so save-placed
std::unordered_set<void*>               g_savePlacedActors;  // host: the actors present before any client, so a save ATV that mints its key late is not misread as a runtime spawn
std::unordered_map<void*, std::wstring> g_synthForActor;     // actor -> synthetic wire key (host runtime-spawned + client mirror)
uint32_t                                g_synthCounter = 0;  // host: the monotonic synthetic-key id
bool                                    g_companionSpawned = false;  // host: a second ATV already minted this session
bool                                    g_companionCapped = false;   // the post-mint watch has closed; do not mint another
void*                                   g_companionActor = nullptr;  // the minted body, until it is destroyed
uint64_t                                g_companionSettledSince = 0; // settled at two or fewer while a save twin is still absent
std::wstring                            g_companionKey;
// The same pause the mint waits for the source scooter to sit still. While the loaded save
// names coopatv1 and that scooter is not live yet, the companion is not given that key.
// After this long of a settled count that is still two or fewer, it is announced under the
// next free key, and the count watch stays open so a later third body is still removed.
constexpr uint64_t kCompanionSettleMs = 2000;
void*                                   g_compSrc = nullptr;
FVector                                 g_compLoc{};
uint64_t                                g_compStillSince = 0;

const wchar_t* const kSynthPrefix = L"coopatv#";  // distinguishes synth keys from real ATV keys ("atv"/base64)

bool IsSynthKey(const std::wstring& k) {
    return k.compare(0, 8, kSynthPrefix) == 0;  // "coopatv#" is 8 chars
}

// A WireClassName from a wide class name (ASCII), truncated at 63.
void FillWireClassName(coop::net::WireClassName& out, const std::wstring& name) {
    out.len = 0;
    for (size_t i = 0; i < name.size() && i < sizeof(out.data); ++i)
        out.data[out.len++] = static_cast<char>(name[i]);
}
std::wstring WireClassNameToString(const coop::net::WireClassName& in) {
    std::wstring s;
    const uint8_t n = in.len <= sizeof(in.data) ? in.len : static_cast<uint8_t>(sizeof(in.data));
    s.reserve(n);
    for (uint8_t i = 0; i < n; ++i) s.push_back(static_cast<wchar_t>(static_cast<unsigned char>(in.data[i])));
    return s;
}

// Host: announces a runtime ATV so clients fresh-spawn a native mirror; slot < 0 broadcasts (a
// new ATV mid-session), slot >= 0 seeds one joiner. Reads the class and the current pose.
void SendAtvSpawn(const std::wstring& synthKey, void* actor, int slot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || !actor) return;
    void* cls = R::ClassOf(actor);
    if (!cls) return;
    FVector loc; FRotator rot;
    if (!A::GetRootTransform(actor, loc, rot)) return;
    coop::net::AtvSpawnPayload p{};
    WireKeyFromString(synthKey, p.synthKey);
    FillWireClassName(p.className, R::ToString(R::NameOf(cls)));
    p.x = loc.X; p.y = loc.Y; p.z = loc.Z;
    p.pitch = rot.Pitch; p.yaw = rot.Yaw; p.roll = rot.Roll;
    if (slot < 0) s->SendReliable(coop::net::ReliableKind::AtvSpawn, &p, sizeof(p));
    else          s->SendReliableToSlot(slot, coop::net::ReliableKind::AtvSpawn, &p, sizeof(p));
}

void SendAtvDestroy(const std::wstring& synthKey) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    coop::net::AtvDestroyPayload p{};
    WireKeyFromString(synthKey, p.synthKey);
    s->SendReliable(coop::net::ReliableKind::AtvDestroy, &p, sizeof(p));
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
// so the grabber identity comes from the physical hand, held actor or gravity beam.
bool IsLocalGrabber(void* actor, void* localPlayer) {
    if (!localPlayer || !actor) return false;
    ue_wrap::engine::MainPlayerGrabState gs{};
    if (!ue_wrap::engine::ReadMainPlayerGrabState(localPlayer, gs)) return false;
    return gs.grabbingActor == actor || gs.holdingActor == actor || gs.gravityActor == actor ||
           gs.physicsActor == actor;
}

// Pose authority: this peer streams `actor`, driving or carrying it. Not tick ownership.
bool IsPoseAuthor(void* actor, void* localPlayer, uint8_t occupantSlot, uint8_t localSlot) {
    // A seated driver takes pose authority from a grabber. Otherwise the holder keeps publishing
    // its hand-held pose after a remote peer mounts, so the rider moves independently until the
    // dismount snaps the ATV back to the holder.
    if (occupantSlot != 0xFF && occupantSlot != localSlot) return false;
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
                 bool adopt, coop::net::AtvStatePayload& p, bool grabbed = false, bool honk = false) {
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
    if (A::IsDriven(actor)) sb |= coop::net::kAtvStateBitDriven;
    if (A::GetBrake(actor)) sb |= coop::net::kAtvStateBitBrake;
    if (grabbed)            sb |= coop::net::kAtvStateBitGrabbed;
    p.stateBits = sb;
    p.adopt = adopt ? 1 : 0;
    p.cockpit = 0;
    if (A::GetLights(actor)) p.cockpit |= coop::net::kAtvCockpitLights;
    if (A::GetTurbo(actor))  p.cockpit |= coop::net::kAtvCockpitTurbo;
    if (honk)                p.cockpit |= coop::net::kAtvCockpitHonk;
    if (A::GetRadio(actor))  p.cockpit |= coop::net::kAtvCockpitRadio;
    if (A::GetHasMap(actor)) p.cockpit |= coop::net::kAtvCockpitMap;
    p.modules = A::ReadModuleMask(actor);
    p._padCockpit = 0;
    // The condition block (tires, spare, dirt, fuel, health), filled at this one site so the
    // authority, the idle syncer and the adopt seed cannot fork; a failed read leaves it zeroed
    // with tiresValid 0, and receivers touch nothing.
    coop::atv_condition_sync::FillPayload(actor, p);
    return true;
}

struct RadioSent {
    std::string url;
    bool playing = false;
    bool have = false;
};
std::unordered_map<std::wstring, RadioSent> g_radioSent;

void PublishRadio(coop::net::Session& s, void* actor, const std::wstring& key, int slot, bool force) {
    const bool playing = A::GetRadio(actor);
    std::string url;
    for (wchar_t c : A::RadioUrl(actor)) {
        if (c <= 0 || c >= 256 || url.size() >= 159) {
            if (c >= 256) url.clear();
            break;
        }
        url.push_back(static_cast<char>(c));
    }
    RadioSent& prev = g_radioSent[key];
    if (!force && prev.have && prev.playing == playing && prev.url == url) return;
    coop::net::AtvRadioPayload p{};
    WireKeyFromString(key, p.key);
    p.playing = playing ? 1 : 0;
    if (!url.empty()) std::memcpy(p.url, url.c_str(), url.size());
    const bool ok = slot < 0
        ? s.SendReliable(coop::net::ReliableKind::AtvRadio, &p, sizeof(p))
        : s.SendReliableToSlot(slot, coop::net::ReliableKind::AtvRadio, &p, sizeof(p));
    if (!ok) return;
    prev.url = std::move(url);
    prev.playing = playing;
    prev.have = true;
}




bool PlayerInReach(void* actor, void* localPlayer) {
    if (!actor || !localPlayer) return false;
    FVector at; FRotator rot;
    if (!A::GetRootTransform(actor, at, rot)) return false;
    FVector me{};
    if (!ue_wrap::engine::TryGetActorLocation(localPlayer, me)) return false;
    return Len(FVector{ me.X - at.X, me.Y - at.Y, me.Z - at.Z }) <= kServiceReachCm;
}

// The local player's service of an ATV this peer does not author, since the author's last write,
// merged into the unsent one.
void CollectLocalService(AtvEntry& e, void* localPlayer) {
    coop::net::AtvConditionEditPayload p{};
    if (!coop::atv_condition_sync::TakeLocalService(e, PlayerInReach(e.actor, localPlayer), p)) return;
    if (e.hasPendingService) {
        coop::atv_condition_sync::MergeService(e.pendingService, p);
    } else {
        e.pendingService = p;
        e.hasPendingService = true;
    }
}

// The unsent service, once its window allows, where the truth is: a client's to the host, the
// host's own to the client that authors the ATV.
void FlushLocalService(coop::net::Session* s, AtvEntry& e, const std::wstring& key, bool isHost,
                       uint8_t localSlot, uint64_t nowMs) {
    if (!e.hasPendingService || nowMs - e.lastServiceSentMs < kServiceSendMs) return;
    coop::net::AtvConditionEditPayload& p = e.pendingService;
    WireKeyFromString(key, p.key);
    bool sent = false;
    bool attempted = false;
    if (!isHost) {
        attempted = true;
        sent = s->SendReliable(coop::net::ReliableKind::AtvConditionEdit, &p, sizeof(p));
    } else if (e.authorSlot != 0xFF && e.authorSlot != localSlot) {
        attempted = true;
        sent = s->SendReliableToSlot(e.authorSlot, coop::net::ReliableKind::AtvConditionEdit, &p, sizeof(p));
    }
    UE_LOGI("atv: local service of '%ls' %s -- fuel %+.1f health %+.1f dirt %+.2f tyres on %02X spare on %u",
            key.c_str(), sent ? (isHost ? "sent to its author" : "sent to the host") : "NOT sent",
            p.fuel, p.health, p.bodyDirt, static_cast<unsigned>(p.tiresAdded),
            static_cast<unsigned>(p.spareAdded));
    e.lastServiceSentMs = nowMs;
    // A false return is backpressure. The can or repair is already spent, and the local floor
    // only holds those values until the author's stream overwrites them, so the edit stays
    // pending for the next window. The host already authoring this ATV, or an idle one, has
    // nobody to forward to: the verb changed its own copy, and clearing stops a forever retry.
    if (!attempted || sent)
        e.hasPendingService = false;
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


// The scan-hub consumer: the hub's shared sliced pass drives these callbacks, preserving the
// sender state for keys that persist (only the actor and index update) and classifying identity
// as above (a save-placed ATV keeps its real key; a host-side runtime ATV gets a synthetic key
// and an announce). A spawn landing inside the pass window at a join is announced on the next
// pass, when the joiner is connected.
uint32_t g_indexGen = 0;  // world gen of the last completed pass (stale-gen index = EMPTY)
bool IndexCurrent() { return g_indexGen == ue_wrap::world_identity::Generation(); }
struct ScanFound { std::wstring wireKey; void* obj; int32_t idx; std::wstring realKey; };
std::vector<ScanFound> g_scanFound;   // pass scratch (GT-only)
bool g_scanIsHost = false;            // pass context, captured at pass begin
bool g_scanCapturing = false;
bool g_havePendingSpawn = false;
coop::net::AtvSpawnPayload g_pendingSpawn{};
bool g_clientMirrorWatch = false;          // a mirror spawned while the settled count was 1
void* g_clientMirrorActor = nullptr;       // that body; never a save-restored ATV
std::wstring g_clientMirrorKey;

void HubPassBegin(void*, bool) {
    g_scanFound.clear();
    auto* s = g_session.load(std::memory_order_acquire);
    g_scanIsHost = s && s->role() == coop::net::Role::Host;
    const bool isHost = g_scanIsHost;
    // The baseline window: before any client is connected, every keyed ATV the host has is
    // save-placed; after that a newly appearing key is a runtime spawn. Accumulated, not a
    // single-frame latch, so a save ATV slow to mint its key still lands in the set.
    g_scanCapturing = isHost && (!s || !s->connected());
}

void HubMatch(void*, void* obj) {
    const bool isHost = g_scanIsHost;
    const bool capturing = g_scanCapturing;
    auto& found = g_scanFound;
    {
        const std::wstring nm = R::ToString(R::NameOf(obj));
        if (nm.rfind(L"Default__", 0) == 0) return;  // skip CDO
        if (!R::IsLive(obj)) return;
        if (capturing) g_savePlacedActors.insert(obj);  // capture the ACTOR (even before it mints its key)
        std::wstring wireKey;
        std::wstring realKey;
        auto sf = g_synthForActor.find(obj);
        if (sf != g_synthForActor.end()) {
            wireKey = sf->second;  // the companion, even before the game has minted its own Key
            realKey = wireKey;
        } else {
        realKey = A::GetKeyString(obj);
        if (realKey.empty() || realKey == L"None") return;  // not yet keyed -- the next pass picks it up
        if (capturing) {
            g_savePlacedKeys.insert(realKey);                 // baseline: a save-placed ATV
            wireKey = realKey;
        } else if (isHost && g_savePlacedKeys.find(realKey) == g_savePlacedKeys.end() &&
                   g_savePlacedActors.find(obj) == g_savePlacedActors.end()) {
            // Host: a mid-session ATV not in the save set is a runtime spawn: a synthetic wire key
            // and an announce, so the clients fresh-spawn a mirror.
            wireKey = std::wstring(kSynthPrefix) + std::to_wstring(++g_synthCounter);
            g_synthForActor[obj] = wireKey;
            SendAtvSpawn(wireKey, obj, /*slot*/ -1);          // broadcast to all connected clients
            UE_LOGI("atv: runtime-spawned ATV detected -- synthKey='%ls' class='%ls' (host-announced AtvSpawn)",
                    wireKey.c_str(), R::ToString(R::NameOf(R::ClassOf(obj))).c_str());
        } else {
            wireKey = realKey;                                 // save-placed default ATV (both peers have it)
        }
        }
        found.push_back({ std::move(wireKey), obj, R::InternalIndexOf(obj), std::move(realKey) });
    }
}

size_t HubPassComplete(void*, bool isFull, uint32_t worldGen) {
    const bool isHost = g_scanIsHost;
    auto& found = g_scanFound;
    // Entries whose ATV vanished are dropped; a gone host-synthetic ATV sends AtvDestroy so the
    // clients tear down their mirror. On a full pass `found` is authoritative; on a tail pass it is
    // only the new tail, so liveness by index prunes instead.
    for (auto it = g_atvs.begin(); it != g_atvs.end();) {
        bool keep;
        if (isFull) {
            keep = false;
            for (auto& f : found) if (f.wireKey == it->first) { keep = true; break; }
            // Minted during this pass, or carried under its save key. Dropping it here would
            // announce a destroy and then mint another.
            auto held = g_synthForActor.find(it->second.actor);
            if (!keep && held != g_synthForActor.end() && held->second == it->first &&
                R::IsLiveByIndex(it->second.actor, it->second.idx))
                keep = true;
        } else {
            keep = R::IsLiveByIndex(it->second.actor, it->second.idx);
        }
        if (keep) { ++it; continue; }
        {
            auto held = g_synthForActor.find(it->second.actor);
            const bool ours = IsSynthKey(it->first) ||
                              (held != g_synthForActor.end() && held->second == it->first);
            if (ours) {
                if (isHost) SendAtvDestroy(it->first);
                if (it->second.actor) g_synthForActor.erase(it->second.actor);
            }
            if (it->first == g_companionKey) {
                g_companionSpawned = false;
                g_companionCapped = false;
                g_companionKey.clear();
                g_companionActor = nullptr;
                g_companionSettledSince = 0;
                g_compStillSince = 0;
            }
        }
        it = g_atvs.erase(it);
    }
    // Update or add. A wire key can outlive its actor (a save-placed key is deterministic and a
    // client join runs two level loads), and the successor must not inherit the dead actor's
    // authority, seat, correction clock and change-gate baseline: the entry stays, everything that
    // described the actor resets.
    for (auto& f : found) {
        AtvEntry& e = g_atvs[f.wireKey];
        if (e.actor && e.actor != f.obj) {
            // The flag belongs to the body we spawned. A save scooter that took this key must
            // not inherit it, or a later teardown would destroy the saved one.
            if (e.isClientSpawnedMirror && f.obj != g_clientMirrorActor)
                e.isClientSpawnedMirror = false;
            e.occupantSlot = 0xFF; e.authorSlot = 0xFF;
            e.wasPoseAuthor = false;
            e.haveLastSync = false; e.lastIdleSendMs = 0; e.lastPktMs = 0;
            e.lastErrCm = -1.f; e.stallPackets = 0; e.restReplaces = 0; e.lastRestPlaceMs = 0;
            // The service floor and an unsent edit were measured on the body that left. A
            // failed send keeps that edit until it goes out, so the successor would otherwise
            // inherit the old scooter's fuel, repair and dirt.
            e.hasPendingService = false;
            e.pendingService = {};
            e.lastServiceSentMs = 0;
            e.condFloorFields = 0;
            e.condFloorUntilMs = 0;
            e.condLocalRefValid = false;
            e.condLocalRefActor = nullptr;
            e.condExpressedSeeded = false;
            e.condExpressedActor = nullptr;
            e.haveLastSentCond = false;
        }
        e.actor = f.obj;
        e.idx   = f.idx;
    }
    g_indexGen = worldGen;
    // The keys hash over the whole index, since on a tail pass `found` is only the new arrivals.
    uint64_t keysHash = 0;
    for (auto& kv : g_atvs) keysHash ^= FnvKey(kv.first);
    if (g_atvs.size() != g_lastLogCount || keysHash != g_lastLogHash) {
        g_lastLogCount = g_atvs.size();
        g_lastLogHash  = keysHash;
        UE_LOGI("atv: index rebuilt -- %zu live ATV(s), keysHash=0x%016llX (%s pass, +%zu new) "
                "(compare host vs client for cross-peer Key stability)",
                g_atvs.size(), static_cast<unsigned long long>(keysHash),
                isFull ? "full" : "tail", found.size());
    }
    g_scanFound.clear();
    return g_atvs.size();
}

// A peer whose seat or authorship we hold has left or been replaced (slots recycle lowest-free,
// so a slot can change occupant with no absence between, which is why this hangs off the
// ledger's row transition). Every ATV it held is released: an authorSlot stuck on a departed peer
// means nobody ever syncs that ATV again.
void OnSlotReplaced(int slot, const coop::roster_ledger::Row& /*outgoing*/,
                    const coop::roster_ledger::Row& /*incoming*/) {
    if (slot < 0 || slot > 0xFE) return;
    const uint8_t s8 = static_cast<uint8_t>(slot);
    int freed = 0;
    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (e.occupantSlot == s8) { e.occupantSlot = 0xFF; ++freed; }
        if (e.authorSlot   == s8) { e.authorSlot   = 0xFF; ++freed; }
        // The change gate re-arms: the other peers still hold the departed claim, and healing them
        // depends on the host sending something.
        e.haveLastSync = false;
    }
    if (static_cast<size_t>(slot) < std::size(g_editBuckets)) g_editBuckets[slot].Reset();
    if (freed > 0)
        UE_LOGI("atv: slot %d departed -- freed %d ATV seat/author reservation(s)", slot, freed);
}

void SubscribeDepartures() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::roster_ledger::SubscribeSlotReplaced(&OnSlotReplaced);
}

ue_wrap::script_gate::Verdict OnHonkPre(const ue_wrap::script_gate::Call& call) {
    if (call.fromOurCode || !A::IsAtv(call.object)) return ue_wrap::script_gate::Verdict::Run;
    for (auto& kv : g_atvs) {
        if (kv.second.actor != call.object) continue;
        kv.second.honkPulse = true;
        break;
    }
    return ue_wrap::script_gate::Verdict::Run;
}

ue_wrap::script_gate::Verdict OnMountPre(const ue_wrap::script_gate::Call& c) {
    using V = ue_wrap::script_gate::Verdict;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || !c.locals || !c.function || !A::IsAtv(c.object)) return V::Run;
    const int frame = R::FunctionFrameSize(c.function);
    const int playerOff = R::FindParamOffset(c.function, L"player");
    const int nameOff = R::FindParamOffset(c.function, L"name");
    if (playerOff < 0 || playerOff > frame || sizeof(void*) > frame - playerOff ||
        nameOff < 0 || nameOff > frame || sizeof(R::FString) > frame - nameOff) return V::Run;
    void* player = nullptr;
    R::FString name{};
    std::memcpy(&player, c.locals + playerOff, sizeof(player));
    std::memcpy(&name, c.locals + nameOff, sizeof(name));
    if (!player || player != coop::players::Registry::Get().Local() ||
        !name.Data || name.Num != 4 || name.Max < name.Num ||
        _wcsnicmp(name.Data, L"sit", 4) != 0) return V::Run;
    // The native menu can outlive the E-input check. Refuse before it hides the
    // player or possesses the rig; maintenance/radio actions remain untouched.
    return IsHeldForMount(c.object) || IsOccupiedByOther(c.object) ? V::Cancel : V::Run;
}

void EnsureHonkWatch() {
    static bool once = false;
    if (once) return;
    once = true;
    ue_wrap::script_gate::WatchClassName(L"ATV_C", L"InpActEvt_atv_honk_K2Node_InputActionEvent",
                                         0x4154484B, &OnHonkPre, nullptr);
    ue_wrap::script_gate::WatchClassName(L"ATV_C", L"actionName",
                                         0x41544D54, &OnMountPre, nullptr);
    ue_wrap::script_gate::ResolvePendingNames();
}

// -1 while a load is still delivering actors. The count is every live scooter in THIS world,
// subclasses included. A stale scan total is not folded in: one that once saw a scooter from a
// world already left would keep the cap shut, or one that had not yet seen a second would open it.
int LiveAtvCount() {
    return A::CountLiveSettled();
}

// Our mint only. Never a third: a save scooter that was already in the world stays, and an
// index entry that has moved onto that scooter stays with it.
void RemoveMintedCompanion() {
    void* extra = g_companionActor;
    const std::wstring key = g_companionKey;
    auto it = key.empty() ? g_atvs.end() : g_atvs.find(key);
    const bool entryIsOurs = it != g_atvs.end() && (!extra || it->second.actor == extra);
    if (entryIsOurs) {
        g_atvs.erase(it);
        if (extra) g_synthForActor.erase(extra);
        if (!key.empty()) {
            SendAtvDestroy(key);
            UE_LOGW("atv: removed scooter '%ls' -- keeping the world at two", key.c_str());
        }
    } else if (!key.empty() && it == g_atvs.end()) {
        SendAtvDestroy(key);
    }
    if (extra && R::IsLive(extra)) {
        g_synthForActor.erase(extra);
        A::DestroyMirror(extra);
        if (!entryIsOurs)
            UE_LOGW("atv: removed the companion body -- keeping the world at two");
    }
    g_companionActor = nullptr;
    g_companionKey.clear();
    g_companionCapped = true;
}

// The wire key IS the save key. A second name (coopatv#N) made the friend's copy, restored
// from the save under the game key, a different scooter from the one the host was driving.
// Empty from EnsureOwnSaveKey means the key already belongs to another scooter and could not
// be moved: this body is not announced.
bool PublishCompanion(void* spawned) {
    std::wstring key = A::EnsureOwnSaveKey(spawned);
    if (key.empty() || key == L"None") {
        const std::wstring now = A::GetKeyString(spawned);
        if (!now.empty() && now != L"None") return false;
        key = std::wstring(kSynthPrefix) + std::to_wstring(++g_synthCounter);
    }
    auto taken = g_atvs.find(key);
    if (taken != g_atvs.end() && taken->second.actor && taken->second.actor != spawned)
        return false;
    g_companionKey = key;
    UE_LOGI("atv: companion game-save key '%ls' (its own record, not the other scooter's)",
            key.c_str());
    g_synthForActor[spawned] = key;
    AtvEntry e{};
    e.actor = spawned;
    e.idx = R::InternalIndexOf(spawned);
    g_atvs[key] = std::move(e);
    SendAtvSpawn(key, spawned, /*slot*/ -1);
    FVector loc{}; FRotator rot{};
    if (A::GetRootTransform(spawned, loc, rot))
        UE_LOGI("atv: companion announced at (%.0f, %.0f, %.0f) key='%ls'",
                loc.X, loc.Y, loc.Z, key.c_str());
    return true;
}

// Announced under `from`, now saved under `to`. The friend's mirror follows the destroy and
// the new announce, so the two names do not become two scooters.
void RebindCompanionKey(const std::wstring& from, const std::wstring& to) {
    AtvEntry entry{};
    auto it = g_atvs.find(from);
    if (it != g_atvs.end()) {
        entry = it->second;
        g_atvs.erase(it);
    }
    entry.actor = g_companionActor;
    entry.idx = R::InternalIndexOf(g_companionActor);
    g_atvs[to] = entry;
    if (g_companionActor) g_synthForActor[g_companionActor] = to;
    g_companionKey = to;
    SendAtvDestroy(from);
    SendAtvSpawn(to, g_companionActor, /*slot*/ -1);
    UE_LOGI("atv: companion save key moved to '%ls' -- '%ls' belongs to another scooter",
            to.c_str(), from.c_str());
}

// A scooter that arrived after we wrote our key. Above two, our mint is the extra and goes.
// At two or fewer, move to coopatv2 so the two records stay distinct.
void RepairCompanionKeyCollision() {
    if (!g_companionActor || !R::IsLive(g_companionActor) || g_companionKey.empty()) return;
    std::wstring mine = A::GetKeyString(g_companionActor);
    if (mine.empty() || mine == L"None") mine = g_companionKey;
    if (!A::FindOtherLiveByKey(mine, g_companionActor)) return;
    const int n = LiveAtvCount();
    if (n < 0) return;
    const bool canMove = n <= 2 && mine != L"coopatv2"
        && !A::FindOtherLiveByKey(L"coopatv2", g_companionActor)
        && !A::LoadedSaveNamesKey(L"coopatv2")
        && A::WriteSaveKey(g_companionActor, L"coopatv2");
    if (canMove) {
        RebindCompanionKey(g_companionKey, L"coopatv2");
        return;
    }
    UE_LOGW("atv: companion key '%ls' collides with a scooter that arrived later -- removing the companion",
            mine.c_str());
    RemoveMintedCompanion();
}

// Minted while the count was still short. The mint tick is not a settlement: the body just
// added is in it, and a save scooter can still be absent. An unsettled count (n < 0) does not
// close the watch. It closes when a later count has settled at two or fewer and the loaded
// save has no scooter still missing, or when a settled count above two has removed this mint.
// After it closes, a live scooter that arrives holding our key still moves that key or
// removes this body.
void WatchMintedCompanion(uint64_t nowMs) {
    if (!g_companionActor || !R::IsLive(g_companionActor)) {
        g_companionActor = nullptr;
        if (g_companionKey.empty()) {
            g_companionSpawned = false;
            g_companionCapped = false;
            g_companionSettledSince = 0;
        }
        return;
    }
    const int n = LiveAtvCount();
    if (n < 0) {
        g_companionSettledSince = 0;
        return;
    }
    if (n > 2) {
        RemoveMintedCompanion();
        return;
    }
    // Settled at two or fewer, on a tick after the mint. A saved scooter that is not live yet
    // is not in this count: more save records than other live bodies, or coopatv1 named by
    // the save and not held by a live scooter.
    const int saveN = A::LoadedSaveAtvCount();
    const bool twinLive = A::FindOtherLiveByKey(L"coopatv1", g_companionActor) != nullptr;
    const bool unseen = (saveN >= 0 && saveN > n - 1)
        || (A::LoadedSaveNamesKey(L"coopatv1") && !twinLive);
    if (unseen) {
        if (g_companionSettledSince == 0) g_companionSettledSince = nowMs;
        if (g_companionKey.empty() && nowMs - g_companionSettledSince >= kCompanionSettleMs) {
            if (!PublishCompanion(g_companionActor)) RemoveMintedCompanion();
        }
        return;
    }
    g_companionSettledSince = 0;
    if (g_companionKey.empty() && !PublishCompanion(g_companionActor)) {
        RemoveMintedCompanion();
        return;
    }
    g_companionCapped = true;
}

void TrySpawnCompanion(uint64_t nowMs) {
    if (g_companionSpawned) {
        if (!g_companionCapped) WatchMintedCompanion(nowMs);
        RepairCompanionKeyCollision();
        return;
    }
    const int live = LiveAtvCount();
    if (live < 0 || live != 1) return;
    void* src = nullptr;
    FVector loc{};
    FRotator rot{};
    for (auto& kv : g_atvs) {
        if (IsSynthKey(kv.first)) continue;
        if (!R::IsLiveByIndex(kv.second.actor, kv.second.idx)) continue;
        if (!src && A::GetRootTransform(kv.second.actor, loc, rot)) src = kv.second.actor;
    }
    if (!src) return;
    const float dx = loc.X - g_compLoc.X, dy = loc.Y - g_compLoc.Y, dz = loc.Z - g_compLoc.Z;
    if (src != g_compSrc || dx * dx + dy * dy + dz * dz > 150.f * 150.f) {
        g_compSrc = src;
        g_compLoc = loc;
        g_compStillSince = nowMs;
        return;
    }
    if (g_compStillSince == 0 || nowMs - g_compStillSince < kCompanionSettleMs) return;
    if (LiveAtvCount() != 1) return;  // a second one arrived while this one was settling
    if (A::IsDriven(src) || IsLocalGrabber(src, coop::players::Registry::Get().Local())) return;
    // A failed search does not mint a vehicle in a neighbouring room. Throttle
    // collision queries while the garage is crowded; moving an obstacle retries.
    static uint64_t lastSearch = 0;
    if (lastSearch && nowMs >= lastSearch && nowMs - lastSearch < 2000) return;
    lastSearch = nowMs;
    FVector spawn{};
    if (!A::FindCompanionLocation(src, spawn)) return;
    void* spawned = A::SpawnMirror(L"ATV_C", spawn, rot);
    if (spawned) {
        const int n = LiveAtvCount();
        if (n > 2) {
            A::DestroyMirror(spawned);
            g_companionSpawned = true;
            g_companionCapped = true;
            UE_LOGW("atv: refused a third scooter -- the world already has %d", n);
            return;
        }
    }
    if (!spawned) {
        static uint64_t sLastFail = 0;
        if (nowMs - sLastFail > 5000) {
            sLastFail = nowMs;
            UE_LOGW("atv: companion spawn failed beside (%.0f, %.0f, %.0f)", loc.X, loc.Y, loc.Z);
        }
        return;
    }
    // The key waits for a settled count after this tick. LiveAtvCount() >= 0 here is the count
    // the mint just joined, not a settlement that includes a scooter still on its way.
    g_companionActor = spawned;
    g_companionSpawned = true;
    g_companionCapped = false;
    g_companionSettledSince = 0;
    UE_LOGI("atv: companion minted at (%.0f, %.0f, %.0f) -- watching the count before its save key",
            spawn.X, spawn.Y, spawn.Z);
}

void RegisterWithScanHub() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::element::scan_hub::Register(coop::element::scan_hub::Consumer{
        "atv", nullptr, &A::EnsureResolved, &A::IsAtv,
        &HubPassBegin, &HubMatch, &HubPassComplete, /*settleScans*/ 15});
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    // Install is the per-tick ensure path (net_pump re-calls it until the class loads), latched so
    // the one-time registration happens once.
    if (!g_installed && A::EnsureResolved()) {
        RegisterWithScanHub();  // the hub builds the index on its own cadence
        // The condition layout is warmed here, with ATV_C resident, so its one-shot resolve (thirteen
        // offsets and four function lookups) lands at session setup rather than in the first apply frame.
        ue_wrap::atv_condition::Resolve();
        EnsureHonkWatch();
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

bool AcceptIncomingOccupancy(AtvEntry& e, const coop::net::AtvStatePayload& payload,
                             uint8_t senderPeerSlot, void* localPlayer, uint8_t localSlot) {
    // Validate before any local possession/seat change, including the tie-break.
    if (senderPeerSlot >= coop::players::kMaxPeers ||
        (payload.authorSlot != 0xFF && payload.authorSlot >= coop::players::kMaxPeers) ||
        (payload.occupantSlot != 0xFF && payload.occupantSlot >= coop::players::kMaxPeers) ||
        !SenderMaySpeakFor(senderPeerSlot, payload.authorSlot) ||
        !SenderMaySpeakFor(senderPeerSlot, payload.occupantSlot)) return false;
    const bool incomingRemoteDriver =
        payload.occupantSlot != 0xFF && payload.occupantSlot != localSlot;
    const bool incomingRemoteCarrier = payload.occupantSlot == 0xFF &&
        payload.authorSlot != 0xFF && payload.authorSlot != localSlot &&
        (payload.stateBits & coop::net::kAtvStateBitGrabbed) != 0;
    // A carrier wins a mount/grab race. Keep the live physics handle intact on
    // its owner; the newly seated peer runs its own native dismount instead.
    if (incomingRemoteDriver && IsLocalGrabber(e.actor, localPlayer)) return false;
    if (incomingRemoteCarrier && IsLocalOccupant(e.actor, localPlayer)) {
        ue_wrap::CachedObjRef vehicle; vehicle.Set(e.actor);
        if (!A::Dismount(e.actor) || !vehicle.Get()) return false;
        e.occupantSlot = 0xFF;
        e.authorSlot = payload.authorSlot;
        e.wasPoseAuthor = false; // do not emit a release for the winning carrier
    }
    // The local dismount/ungrab may already have happened before this packet,
    // with its release edge still waiting for Tick. It must not clear the new
    // remote carrier's claim on that next Tick.
    if (incomingRemoteCarrier && !IsLocalGrabber(e.actor, localPlayer)) e.wasPoseAuthor = false;

    // The simultaneous-mount tie-break, before the authority early-return that reads the seat: two
    // peers mounting inside one round-trip both elect themselves and each treats the other's stream
    // as an echo, a permanent double drive. The lower slot wins, a total order both hold; only a
    // genuine claim from a peer we outrank takes the seat, so an echo can never demote us.
    if (payload.occupantSlot != 0xFF && payload.occupantSlot < localSlot &&
        IsLocalOccupant(e.actor, localPlayer) &&
        (e.occupantSlot == 0xFF || e.occupantSlot == localSlot)) {
        ue_wrap::CachedObjRef vehicle; vehicle.Set(e.actor);
        if (!A::Dismount(e.actor) || !vehicle.Get()) return false;
        e.occupantSlot = payload.occupantSlot;   // a mirror now; Tick's release edge is suppressed for a yield
    }

    return true;
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

    // An older build's rear-seat claim. It carries no pose worth applying.
    if (payload.stateBits & coop::net::kAtvStateBitPassengerOnly) return;

    if (!AcceptIncomingOccupancy(e, payload, senderPeerSlot, localPlayer, localSlot)) return;

    // A surviving local driver/carrier ignores echoes after contention resolution.
    if (IsPoseAuthor(e.actor, localPlayer, e.occupantSlot, localSlot)) return;

    e.occupantSlot = payload.occupantSlot;
    e.authorSlot   = payload.authorSlot;

    // Lights, brake, turbo, radio, honk and the frame modules. The driver already has them.
    A::SetBrake(e.actor, (payload.stateBits & coop::net::kAtvStateBitBrake) != 0);
    A::SetLights(e.actor, (payload.cockpit & coop::net::kAtvCockpitLights) != 0);
    A::SetTurbo(e.actor, (payload.cockpit & coop::net::kAtvCockpitTurbo) != 0);
    A::SetRadio(e.actor, (payload.cockpit & coop::net::kAtvCockpitRadio) != 0);
    A::SetHasMap(e.actor, (payload.cockpit & coop::net::kAtvCockpitMap) != 0);
    A::ApplyModuleMask(e.actor, payload.modules);
    if (payload.cockpit & coop::net::kAtvCockpitHonk) A::PlayHonk(e.actor);

    // A connect snapshot warps (the joiner has not seen the world); a live packet corrects. The rig
    // keeps simulating either way; the state machine between "adopt an idle ATV" and "mirror an
    // authored one" is where the release defect lived.
    coop::atv_corrector::ApplyCorrection(e, payload, /*snap*/ payload.adopt != 0);
    // A service this peer's player made since the last scan is measured before the block below
    // overwrites the copy it was made on; the tick sends it.
    CollectLocalService(e, localPlayer);
    // The condition block applies after the pose, behind the same gates; presence consumption is
    // further gated inside on the host as sender (atv_condition_sync.h).
    coop::atv_condition_sync::ApplyPayload(e, payload, senderPeerSlot);
}

void OnRadio(const coop::net::AtvRadioPayload& payload, uint8_t senderPeerSlot) {
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    if (senderPeerSlot == localSlot) return;
    const std::wstring key = StringFromWireKey(payload.key);
    if (key.empty() || !A::EnsureResolved() || !IndexCurrent()) return;
    auto it = g_atvs.find(key);
    if (it == g_atvs.end() || !R::IsLiveByIndex(it->second.actor, it->second.idx)) return;
    // PublishRadio runs for the peer that authors the ATV (the driver or the grabber) and, while
    // nobody authors it, for the host as the idle syncer, and again in the host's connect snapshot.
    // A client beside a parked scooter does not send this. author 0xFF used to fail open, so any
    // client could set the url on every peer. Sender 0 stays accepted: that is the host's packet.
    // A relay keeps the origin slot, so a client's radio is applied only when that slot authors it.
    if (senderPeerSlot != 0 &&
        (it->second.authorSlot == 0xFF || senderPeerSlot != it->second.authorSlot)) {
        UE_LOGW("atv: radio of '%ls' from slot %u refused -- author is slot %u",
                key.c_str(), static_cast<unsigned>(senderPeerSlot),
                static_cast<unsigned>(it->second.authorSlot));
        return;
    }
    std::wstring url;
    if (payload.url[0]) {
        for (const char* c = payload.url; *c; ++c)
            url.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
    }
    A::DriveRadio(it->second.actor, url.empty() ? nullptr : url.c_str(), payload.playing != 0);
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

// A mirror spawned while the settled count was 1. The spawn tick is not a settlement. An
// unsettled count does not close the watch. Above two, this mirror is destroyed and a
// save-restored ATV is not. If another live body already holds the synth key, that body is
// adopted and the mirror is the one that goes. A save record that names the key, with no such
// body yet, keeps the watch open so the later body is adopted instead of left beside the mirror.
void WatchClientMirror() {
    if (!g_clientMirrorWatch) return;
    void* mirror = g_clientMirrorActor;
    if (!mirror || !R::IsLive(mirror)) {
        g_clientMirrorActor = nullptr;
        g_clientMirrorWatch = false;
        return;
    }
    const int n = LiveAtvCount();
    if (n < 0) return;
    const std::wstring key = g_clientMirrorKey;
    auto it = g_atvs.find(key);

    auto dropMirror = [&]() {
        g_synthForActor.erase(mirror);
        if (R::IsLive(mirror)) A::DestroyMirror(mirror);
        g_clientMirrorActor = nullptr;
        g_clientMirrorWatch = false;
        g_clientMirrorKey.clear();
    };

    if (it != g_atvs.end() && it->second.actor && it->second.actor != mirror &&
        R::IsLive(it->second.actor)) {
        it->second.isClientSpawnedMirror = false;
        g_synthForActor[it->second.actor] = key;
        dropMirror();
        UE_LOGI("atv: companion '%ls' is already in this world -- dropped the extra mirror", key.c_str());
        return;
    }
    if (void* other = A::FindOtherLiveByKey(key, mirror)) {
        AtvEntry e = it != g_atvs.end() ? it->second : AtvEntry{};
        // The entry was the mirror's. Its unsent service and floor belong to that body, not
        // to the save scooter this key is moving onto.
        if (e.actor != other) {
            e.hasPendingService = false;
            e.pendingService = {};
            e.lastServiceSentMs = 0;
            e.condFloorFields = 0;
            e.condFloorUntilMs = 0;
            e.condLocalRefValid = false;
            e.condLocalRefActor = nullptr;
            e.condExpressedSeeded = false;
            e.condExpressedActor = nullptr;
            e.haveLastSentCond = false;
        }
        e.actor = other;
        e.idx = R::InternalIndexOf(other);
        e.isClientSpawnedMirror = false;
        g_atvs[key] = std::move(e);
        g_synthForActor[other] = key;
        dropMirror();
        UE_LOGI("atv: companion '%ls' is already in this world -- dropped the extra mirror", key.c_str());
        return;
    }
    if (n > 2) {
        if (it != g_atvs.end() && it->second.actor == mirror) g_atvs.erase(it);
        else if (it != g_atvs.end()) it->second.isClientSpawnedMirror = false;
        dropMirror();
        UE_LOGW("atv: removed client mirror '%ls' -- keeping the world at two", key.c_str());
        return;
    }
    const int saveN = A::LoadedSaveAtvCount();
    if (saveN >= 0 && saveN > n - 1) return;
    if (!key.empty() && A::LoadedSaveNamesKey(key.c_str())) return;
    g_clientMirrorWatch = false;
}

void FlushPendingAtvSpawn() {
    WatchClientMirror();
    if (!g_havePendingSpawn) return;
    const int live = LiveAtvCount();
    if (live < 0) return;  // the load is still delivering actors; a count taken now is short
    const std::wstring synthKey = StringFromWireKey(g_pendingSpawn.synthKey);
    if (!synthKey.empty() && g_atvs.find(synthKey) != g_atvs.end()) {
        g_havePendingSpawn = false;
        return;
    }
    // The save already built this scooter. Adopting it keeps one actor, under the same key the
    // host saves and drives. Spawning here would be a third body with a copy of that key.
    if (void* existing = A::FindLiveByKey(synthKey)) {
        g_havePendingSpawn = false;
        AtvEntry e{};
        e.actor = existing;
        e.idx = R::InternalIndexOf(existing);
        g_atvs[synthKey] = std::move(e);
        g_synthForActor[existing] = synthKey;
        UE_LOGI("atv: companion '%ls' is already in this world -- not spawning another", synthKey.c_str());
        return;
    }
    if (live >= 2) {
        g_havePendingSpawn = false;
        UE_LOGI("atv: not spawning another scooter -- the world already has %d", live);
        return;
    }
    if (live == 0) return;  // the save scooter is not listed yet
    const coop::net::AtvSpawnPayload payload = g_pendingSpawn;
    g_havePendingSpawn = false;
    std::wstring className = WireClassNameToString(payload.className);
    if (className.empty()) return;
    const FVector loc{ payload.x, payload.y, payload.z };
    const FRotator rot{ payload.pitch, payload.yaw, payload.roll };
    void* spawned = A::SpawnMirror(className, loc, rot);
    if (!spawned) {
        UE_LOGW("atv: mirror spawn failed synthKey='%ls' class='%ls'", synthKey.c_str(), className.c_str());
        return;
    }
    // Same save key as the host, so a later world load finds this body instead of creating
    // another. skipSave: the host's actor is the one the world save writes.
    A::WriteSaveKey(spawned, synthKey);
    A::SetIncludedInSave(spawned, false);
    AtvEntry e{};
    e.actor = spawned;
    e.idx = R::InternalIndexOf(spawned);
    e.isClientSpawnedMirror = true;
    g_atvs[synthKey] = std::move(e);
    g_synthForActor[spawned] = synthKey;
    // The count on this tick is the one the mirror just joined. The watch runs from the next
    // flush, so a save scooter that appears after this spawn can still drop the mirror.
    g_clientMirrorActor = spawned;
    g_clientMirrorKey = synthKey;
    g_clientMirrorWatch = true;
    UE_LOGI("atv: spawned runtime-ATV mirror synthKey='%ls' class='%ls' actor=%p loc=(%.0f, %.0f, %.0f)",
            synthKey.c_str(), className.c_str(), spawned, loc.X, loc.Y, loc.Z);
}

void OnAtvSpawn(const coop::net::AtvSpawnPayload& payload, uint8_t /*senderPeerSlot*/) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() == coop::net::Role::Host) return;  // client-only (the host owns the real ATV)
    if (!A::EnsureResolved()) return;
    std::wstring synthKey = StringFromWireKey(payload.synthKey);
    if (synthKey.empty()) { UE_LOGW("atv: OnAtvSpawn empty synthKey -- dropping"); return; }
    if (g_atvs.find(synthKey) != g_atvs.end()) return;  // already spawned (re-announce / connect dup) -- idempotent
    if (!std::isfinite(payload.x) || !std::isfinite(payload.y) || !std::isfinite(payload.z) ||
        !std::isfinite(payload.pitch) || !std::isfinite(payload.yaw) || !std::isfinite(payload.roll)) {
        UE_LOGW("atv: OnAtvSpawn non-finite pose synthKey='%ls' -- dropping", synthKey.c_str());
        return;
    }
    if (WireClassNameToString(payload.className).empty()) {
        UE_LOGW("atv: OnAtvSpawn empty className synthKey='%ls' -- dropping", synthKey.c_str());
        return;
    }
    // Held until the object index has finished the load, so a scooter the save already restored
    // is visible and this does not add a third.
    g_pendingSpawn = payload;
    g_havePendingSpawn = true;
    FlushPendingAtvSpawn();
}

void OnAtvDestroy(const coop::net::AtvDestroyPayload& payload, uint8_t /*senderPeerSlot*/) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() == coop::net::Role::Host) return;  // client-only
    std::wstring synthKey = StringFromWireKey(payload.synthKey);
    if (synthKey.empty()) return;
    // A spawn held for the load must not flush after the host has already taken the scooter back.
    if (g_havePendingSpawn && StringFromWireKey(g_pendingSpawn.synthKey) == synthKey)
        g_havePendingSpawn = false;
    void* mirror = nullptr;
    if (g_clientMirrorKey == synthKey) {
        mirror = g_clientMirrorActor;
        if (mirror && R::IsLive(mirror)) {
            A::DestroyMirror(mirror);
            g_synthForActor.erase(mirror);
        }
        g_clientMirrorActor = nullptr;
        g_clientMirrorWatch = false;
        g_clientMirrorKey.clear();
    }
    if (!IndexCurrent()) return;  // the pass prunes a dead-world entry itself
    auto it = g_atvs.find(synthKey);
    if (it == g_atvs.end()) return;
    void* actor = it->second.actor;
    // The entry moved onto a save scooter. The mirror is already gone; that scooter stays.
    if (mirror && actor && actor != mirror) {
        it->second.isClientSpawnedMirror = false;
        UE_LOGI("atv: runtime-ATV mirror synthKey='%ls' destroyed; the saved scooter stays",
                synthKey.c_str());
        return;
    }
    const bool destroyed = it->second.isClientSpawnedMirror && R::IsLiveByIndex(actor, it->second.idx) &&
                           A::DestroyMirror(actor);  // our own fresh spawn
    if (actor) g_synthForActor.erase(actor);
    g_atvs.erase(it);
    UE_LOGI("atv: runtime-ATV mirror synthKey='%ls' %s", synthKey.c_str(),
            destroyed ? "destroyed" : "dropped from the lane, not destroyed here");
}

void OnConditionEdit(const coop::net::AtvConditionEditPayload& payload, uint8_t senderPeerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    const bool isHost = s->role() == coop::net::Role::Host;
    // A client's service reaches the host; the host's, or one it forwards, reaches the author.
    if (isHost ? (senderPeerSlot == 0 || senderPeerSlot >= std::size(g_editBuckets))
               : senderPeerSlot != 0) {
        UE_LOGW("atv: AtvConditionEdit from slot %u refused on the %s", static_cast<unsigned>(senderPeerSlot),
                isHost ? "host" : "client");
        return;
    }
    if (isHost && !g_editBuckets[senderPeerSlot].Take(kEditBudget, NowMs())) return;
    const std::wstring key = StringFromWireKey(payload.key);
    if (key.empty()) return;
    if (!A::EnsureResolved() || !IndexCurrent()) return;
    auto it = g_atvs.find(key);
    if (it == g_atvs.end()) {
        UE_LOGW("atv: service of '%ls' from slot %u names no indexed ATV -- dropped", key.c_str(),
                static_cast<unsigned>(senderPeerSlot));
        return;
    }
    AtvEntry& e = it->second;
    if (!R::IsLiveByIndex(e.actor, e.idx)) return;
    void* localPlayer = coop::players::Registry::Get().Local();
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    const bool iAuthor = IsPoseAuthor(e.actor, localPlayer, e.occupantSlot, localSlot);
    const bool putsOn = payload.tiresAdded != 0 || payload.spareAdded != 0;
    const bool putsOff = payload.tiresRemoved != 0 || payload.spareRemoved != 0;

    if (!isHost) {
        if (!iAuthor) {
            UE_LOGI("atv: service of '%ls' by slot %u arrived after this peer stopped authoring it -- dropped",
                    key.c_str(), static_cast<unsigned>(payload.originSlot));
            return;
        }
        coop::atv_condition_sync::ApplyService(e, payload, /*deltas*/true, /*presence*/true);
        return;
    }
    // CollectLocalService already required 6 m, but only on the machine that built the packet.
    // This host applies it or forwards it, so it measures again, against that slot's puppet and
    // not its own pawn. Before the first pose the puppet stands at a placeholder, which is not
    // where the peer is, so a missing body or an unreadable location refuses the edit.
    coop::RemotePlayer* senderPuppet = coop::players::Registry::Get().Puppet(senderPeerSlot);
    void* senderPawn = (senderPuppet && senderPuppet->valid() && senderPuppet->HasPose())
                           ? senderPuppet->actor() : nullptr;
    if (!PlayerInReach(e.actor, senderPawn)) {
        UE_LOGW("atv: service of '%ls' from slot %u refused -- the sender's pawn is not within %.0f cm",
                key.c_str(), static_cast<unsigned>(senderPeerSlot), kServiceReachCm);
        return;
    }
    if (iAuthor || e.authorSlot == 0xFF || e.authorSlot == localSlot) {
        coop::atv_condition_sync::ApplyService(e, payload, /*deltas*/true, /*presence*/true);
        return;
    }
    // A client authors it: tyre presence is the host's own, the accumulators are the author's, and
    // an author that made the service itself already streams them.
    if (putsOn || putsOff) coop::atv_condition_sync::ApplyService(e, payload, /*deltas*/false, /*presence*/true);
    if (e.authorSlot == senderPeerSlot) return;
    coop::net::AtvConditionEditPayload fwd = payload;
    fwd.originSlot = senderPeerSlot;
    s->SendReliableToSlot(e.authorSlot, coop::net::ReliableKind::AtvConditionEdit, &fwd, sizeof(fwd));
    UE_LOGI("atv: service of '%ls' by slot %u forwarded to its author, slot %u", key.c_str(),
            static_cast<unsigned>(senderPeerSlot), static_cast<unsigned>(e.authorSlot));
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;  // host-only snapshot
    if (peerSlot < 0 || peerSlot >= static_cast<int>(coop::players::kMaxPeers)) return;
    // The hub keeps the index a pass fresh; a spawn inside that window is announced on the next
    // pass.
    void* localPlayer = coop::players::Registry::Get().Local();
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    int sent = 0, spawns = 0;
    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (!R::IsLiveByIndex(e.actor, e.idx)) continue;
        // A synthetic-keyed ATV is announced first on the same lane, so the joiner fresh-spawns it
        // before the pose lands.
        auto held = g_synthForActor.find(e.actor);
        const bool runtime = IsSynthKey(kv.first) ||
                             (held != g_synthForActor.end() && held->second == kv.first);
        if (runtime) { SendAtvSpawn(kv.first, e.actor, peerSlot); ++spawns; }
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
        if (!ReadPayload(e.actor, kv.first, e.occupantSlot, authorSlot, /*adopt*/true, p,
                         /*grabbed*/false, e.honkPulse)) continue;
        e.honkPulse = false;
        s->SendReliableToSlot(peerSlot, coop::net::ReliableKind::AtvState, &p, sizeof(p));
        PublishRadio(*s, e.actor, kv.first, peerSlot, true);
        ++sent;
    }
    UE_LOGI("atv: connect-snapshot -- sent %d ATV pose(s) (%d runtime-ATV announce(s)) to slot %d (of %zu indexed)",
            sent, spawns, peerSlot, g_atvs.size());
}

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
    const uint64_t nowMs = NowMs();
    // Mint before a peer is connected. connected() flips when the first peer finishes joining,
    // so a host who walks over to look before that never saw the second scooter.
    if (s && s->role() == coop::net::Role::Host) TrySpawnCompanion(nowMs);

    if (!s || !s->connected()) return;
    FlushPendingAtvSpawn();  // a mirror announce that arrived before the load finished counting
    void* localPlayer = coop::players::Registry::Get().Local();
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    const bool isHost = s->role() == coop::net::Role::Host;

    // Fail closed: without the hit interceptors a non-owner would author damage on a simulating
    // rig, so nothing is mirrored and the ERROR at Install says why.
    if (!coop::atv_hit_guard::Armed()) return;

    void* owned[coop::atv_hit_guard::kMaxOwned];
    int   ownedN = 0;

    for (auto& kv : g_atvs) {
        AtvEntry& e = kv.second;
        if (!R::IsLiveByIndex(e.actor, e.idx)) continue;

        const bool isDriver = CanClaimOrIsDriver(e.actor, localPlayer, e.occupantSlot, localSlot);
        const bool seatHeldByOther = e.occupantSlot != 0xFF && e.occupantSlot != localSlot;
        const bool isGrabber = !isDriver && !seatHeldByOther &&
                               IsLocalGrabber(e.actor, localPlayer);  // mutually exclusive
        const bool authority = isDriver || isGrabber;

        // The authority-lost edge, with two causes handled oppositely: a dismount or ungrab frees
        // the ATV (the seat and author clear, and a release goes out); a yield (still seated, but
        // outranked by a lower slot) means someone else holds it, and clearing would erase the
        // winner's claim, re-claim next tick and flap.
        const bool yielded = seatHeldByOther;
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
        // A copy this peer does not own is a mirror, and its player's service of it is the one write
        // a mirror makes that must reach the truth.
        if (ownsTick) {
            coop::atv_condition_sync::ResetLocalService(e);
        } else {
            if (nowMs - e.lastServiceScanMs >= kServiceScanMs) {
                e.lastServiceScanMs = nowMs;
                CollectLocalService(e, localPlayer);
            }
            FlushLocalService(s, e, kv.first, isHost, localSlot, nowMs);
        }
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
                if (ReadPayload(e.actor, kv.first, occSlot, localSlot, /*adopt*/false, p,
                                /*grabbed*/isGrabber, e.honkPulse)) {
                    e.honkPulse = false;
                    s->SendReliable(coop::net::ReliableKind::AtvState, &p, sizeof(p));
                    coop::atv_condition_sync::NoteSent(e, p);  // keep the idle gate's baseline fresh across a drive-to-idle handoff
                    PublishRadio(*s, e.actor, kv.first, -1, false);
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
                const bool honk = e.honkPulse;
                const bool readable  = ReadPayload(e.actor, kv.first, e.occupantSlot, /*authorSlot*/0xFF,
                                                   /*adopt*/false, p, /*grabbed*/false, honk);
                const bool changed   = IdleWorthSending(e);
                const bool condMoved = readable && coop::atv_condition_sync::CondChangedSinceLastSend(e, p);
                const bool keepalive = nowMs - e.lastIdleSendMs >= kIdleKeepaliveMs;
                const uint8_t face = static_cast<uint8_t>(p.cockpit & ~coop::net::kAtvCockpitHonk);
                const bool brakeOn = (p.stateBits & coop::net::kAtvStateBitBrake) != 0;
                const bool faceMoved = readable && (!e.haveFace || e.lastCockpit != face ||
                                                    e.lastModules != p.modules || e.lastBrake != brakeOn);
                if (readable && (changed || condMoved || keepalive || honk || faceMoved)) {
                    e.honkPulse = false;
                    e.haveFace = true;
                    e.lastCockpit = face;
                    e.lastModules = p.modules;
                    e.lastBrake = brakeOn;
                    e.lastIdleSendMs = nowMs;
                    s->SendReliable(coop::net::ReliableKind::AtvState, &p, sizeof(p));
                    coop::atv_condition_sync::NoteSent(e, p);
                }
                if (readable) PublishRadio(*s, e.actor, kv.first, -1, false);
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
    // Disarmed first: nothing publishes an owned set from here, and a live hit must reach the game.
    coop::atv_hit_guard::SetActive(false);
    coop::atv_hit_guard::PublishOwned(nullptr, 0);
    for (auto& kv : g_atvs) {
        const bool live = R::IsLiveByIndex(kv.second.actor, kv.second.idx);
        if (kv.second.isClientSpawnedMirror) {
            if (live) A::DestroyMirror(kv.second.actor);   // a fresh-spawned runtime mirror is a coop artifact
            if (kv.second.actor == g_clientMirrorActor) g_clientMirrorActor = nullptr;
        }
        // Nothing to restore: the lane disables neither tick nor physics, so a session leaves every
        // save ATV as it found it.
    }
    const size_t n = g_atvs.size();
    g_atvs.clear();
    g_radioSent.clear();
    for (auto& b : g_editBuckets) b.Reset();
    g_synthForActor.clear();
    g_savePlacedKeys.clear();
    g_savePlacedActors.clear();
    g_synthCounter = 0;
    if (g_companionKey.empty() && g_companionActor && R::IsLive(g_companionActor))
        A::DestroyMirror(g_companionActor);  // minted, not yet its own save record
    g_companionSpawned = false;
    g_companionCapped = false;
    g_companionKey.clear();
    g_companionActor = nullptr;
    g_companionSettledSince = 0;
    if (g_clientMirrorActor && R::IsLive(g_clientMirrorActor))
        A::DestroyMirror(g_clientMirrorActor);
    g_clientMirrorActor = nullptr;
    g_clientMirrorWatch = false;
    g_clientMirrorKey.clear();
    g_havePendingSpawn = false;
    g_compSrc = nullptr;
    g_compStillSince = 0;
    g_installed = false;  // a new session re-indexes via the next Install (latched again)
    const auto c  = coop::atv_corrector::ReadCounters();
    const auto hg = coop::atv_hit_guard::ReadCounters();
    if (n > 0)
        UE_LOGI("atv: OnDisconnect -- cleared %zu ATV(s) (brains restored; runtime mirrors destroyed); "
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

bool LocalIsDriving() {
    if (!IndexCurrent()) return false;
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    for (const auto& kv : g_atvs) {
        if (kv.second.occupantSlot == localSlot) return true;
    }
    return false;
}

bool IsHeldForMount(void* actor, uint8_t* outHolderSlot) {
    if (!actor) return false;
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    // The fresh pawn read closes the interval before our next outgoing ATV state.
    if (IsLocalGrabber(actor, coop::players::Registry::Get().Local())) {
        if (outHolderSlot) *outHolderSlot = localSlot;
        return true;
    }
    if (!IndexCurrent()) return false;
    for (const auto& kv : g_atvs) {
        const AtvEntry& e = kv.second;
        if (e.actor != actor || !R::IsLiveByIndex(e.actor, e.idx)) continue;
        // A pose author without a driver seat is a carrier. Our own cached
        // claim can lag a release, so only the fresh pawn decides local holds.
        if (e.occupantSlot == 0xFF && e.authorSlot < coop::players::kMaxPeers &&
            e.authorSlot != localSlot) {
            if (outHolderSlot) *outHolderSlot = e.authorSlot;
            return true;
        }
        return false;
    }
    return false;
}

int CopyIndexed(void** atvs, uint8_t* driverSlots, int max) {
    if (!atvs || !driverSlots || max <= 0 || !IndexCurrent()) return 0;
    int n = 0;
    for (const auto& kv : g_atvs) {
        if (!R::IsLiveByIndex(kv.second.actor, kv.second.idx)) continue;
        if (n >= max) break;
        atvs[n] = kv.second.actor;
        driverSlots[n] = kv.second.occupantSlot;
        ++n;
    }
    return n;
}

}  // namespace coop::atv_sync
