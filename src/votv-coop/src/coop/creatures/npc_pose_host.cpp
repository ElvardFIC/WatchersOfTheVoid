// coop/creatures/npc_pose_host.cpp -- HOST-side NPC transform egress.
//
// Owns the two host-only paths that READ live NPC actor transforms and push them
// onto the wire:
//   - TickPoseStream():            per-tick EntityPose batch (unreliable, ~sendHz)
//                                  so client mirrors MOVE between spawn + destroy.
//   - QueueConnectBroadcastForSlot: connect-edge EntitySpawn re-send (reliable) so
//                                  a fresh joiner mirrors NPCs that spawned before
//                                  it joined.
// Both iterate MirrorManager<Npc> with the same actor-bind + liveness guard and
// run on the game thread (net_pump::Tick asserts GT). Receiver-side mirror drive
// is npc_mirror.cpp; host PRE/POST/interceptor + lifecycle stay in npc_sync.cpp.

#include "coop/creatures/npc_sync.h"

#include "coop/element/mirror_manager.h"
#include "coop/element/mirror_managers.h"  // PropMirrors/NpcMirrors/WaMirrors
#include "coop/element/npc.h"
#include "coop/creatures/kerfur_entity.h"  // scope A: GetOriginOffEidForEid -- carry the off->active retire eid
#include "coop/creatures/npc_world_enum.h" // DrainPendingExSpawns (EX_CallMath catch)
#include "coop/dev/kerfur_census.h"        // [dev] kerfur_census=1: periodic HOST census (5-vs-6 measurement)
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/props/prop_drive_host.h"  // a world prop the Omega is holding follows the host copy
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/actors/fossilhound.h"
#include "ue_wrap/actors/rufus.h"
#include "ue_wrap/engine/engine_attach.h"
#include "ue_wrap/actors/kerfur.h"  // HasSaveKey (savePersisted flag) + ReadKerfurState (mirror fidelity)
#include "ue_wrap/actors/sit_pose.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"  // NormalizeAxis

#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace coop::npc_sync {
namespace {

namespace R = ue_wrap::reflection;

// The single host/mirror Npc set (same template singleton npc_sync.cpp +
// npc_mirror.cpp wrap; MirrorManager<Npc>::Instance() owns the storage).
using coop::element::NpcMirrors;   // canonical accessor (coop/element/mirror_managers.h)

// One Omega's current holdObject. The prop-drive channel moves a world prop the robot picked up;
// a hold that is not a keyed prop is refused there, and an empty hand releases the previous one
// so it coasts to rest instead of staying glued to the last pose.
struct KerfurHold {
    void*   kerfur    = nullptr;
    int32_t kerfurIdx = -1;
    void*   held      = nullptr;
    int32_t heldIdx   = -1;
};
std::vector<KerfurHold> g_kerfurHolds;
std::unordered_set<void*> g_kerfurHeldNow;

void RememberHeld(void* actor) {
    if (actor) g_kerfurHeldNow.insert(actor);
}
void ForgetHeld(void* actor) {
    if (actor) g_kerfurHeldNow.erase(actor);
}

bool StillLive(void* actor, int32_t idx) {
    return actor && idx >= 0 && R::IsLiveByIndex(actor, idx);
}

void ForgetKerfurHold(void* kerfur) {
    for (size_t i = 0; i < g_kerfurHolds.size();) {
        if (g_kerfurHolds[i].kerfur != kerfur) { ++i; continue; }
        ForgetHeld(g_kerfurHolds[i].held);
        g_kerfurHolds[i] = g_kerfurHolds.back();
        g_kerfurHolds.pop_back();
    }
}

void SyncKerfurHold(void* kerfur) {
    void* held = ue_wrap::kerfur::ReadHoldObject(kerfur);
    KerfurHold* row = nullptr;
    for (KerfurHold& h : g_kerfurHolds)
        if (h.kerfur == kerfur && StillLive(h.kerfur, h.kerfurIdx)) { row = &h; break; }
    if (!row) {
        if (!held) return;
        g_kerfurHolds.push_back(KerfurHold{kerfur, R::InternalIndexOf(kerfur),
                                           held, R::InternalIndexOf(held)});
        RememberHeld(held);
        ue_wrap::engine::SetActorSimulatePhysics(held, false);
        coop::prop_drive_host::Claim(held, "kerfur");
        return;
    }
    if (row->held != held || !StillLive(row->held, row->heldIdx)) {
        if (StillLive(row->held, row->heldIdx)) {
            ForgetHeld(row->held);
            ue_wrap::engine::SetActorSimulatePhysics(row->held, true);
            coop::prop_drive_host::Release(row->held);
        }
        row->held = held;
        row->heldIdx = held ? R::InternalIndexOf(held) : -1;
        RememberHeld(held);
    }
    if (!held) {
        ForgetKerfurHold(kerfur);
        return;
    }
    // Every pass, not only the edge: a claimed row the drive closed is re-opened by the next
    // claim, which is how that channel expects a verb that still holds. Physics stays off so a
    // hand-release that arrived in the same moment cannot throw the prop out of the robot's hands.
    ue_wrap::engine::SetActorSimulatePhysics(held, false);
    coop::prop_drive_host::Claim(held, "kerfur");
}

void SweepKerfurHolds() {
    for (size_t i = 0; i < g_kerfurHolds.size();) {
        KerfurHold& h = g_kerfurHolds[i];
        if (StillLive(h.kerfur, h.kerfurIdx)) { ++i; continue; }
        if (StillLive(h.held, h.heldIdx)) {
            ForgetHeld(h.held);
            coop::prop_drive_host::Release(h.held);
        }
        g_kerfurHolds[i] = g_kerfurHolds.back();
        g_kerfurHolds.pop_back();
    }
}

}  // namespace

void DropKerfurHolds() {
    // The prop-drive set is cleared by its own disconnect, later in the same teardown. Releasing
    // here would coast props the session is already throwing away, and a pointer kept past this
    // point can be a new object by the next session.
    g_kerfurHolds.clear();
    g_kerfurHeldNow.clear();
}

bool KerfurIsHolding(void* actor) {
    return actor && g_kerfurHeldNow.count(actor) != 0;
}

void NoteKerfurHolding(void* actor) {
    RememberHeld(actor);
}

void ForgetKerfurHolding(void* actor) {
    // The host's rows are the pose sweep's: it forgets when holdObject changes, and a grab
    // restarting a drive must not wipe a take the robot still has. A client only has the note
    // the host's take release planted, and the next grab of that prop is the robot letting go.
    auto* s = GetSession();
    if (!actor || !s || s->role() != coop::net::Role::Client) return;
    ForgetHeld(actor);
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    // HOST-only: re-send EntitySpawn (class + CURRENT transform) for every already-spawned NPC
    // to the freshly-connected client `peerSlot`, so a joiner mirrors NPCs that spawned BEFORE it
    // joined. The client's npc_mirror::OnEntitySpawn materializes each; the
    // MirrorManager::Install is idempotent so a re-send to an already-mirroring peer is a no-op.
    auto* s = GetSession();
    if (!s || s->role() != coop::net::Role::Host) return;
    if (peerSlot < 1) return;  // SendReliableToSlot range-validates the upper bound

    std::vector<coop::element::Npc*> elems;  // connect edge (cold) -- a per-call vector is fine
    NpcMirrors().Snapshot(elems);
    int sent = 0, unbound = 0, unread = 0;
    for (coop::element::Npc* el : elems) {
        if (!el) continue;
        void* actor = el->GetActor();
        // Skip elements with no bound actor / a GC-purged actor; a real game-spawned NPC binds in
        // POST.
        if (!actor || !R::IsLiveByIndex(actor, el->GetInternalIdx())) { ++unbound; continue; }
        // A row at the origin or at a zero facing would misplace the joiner's mirror: an NPC whose
        // location or rotation cannot be read is left out, named and counted. The read failing is a
        // dispatch that faulted, which the pose stream repeats every tick, so no later row could
        // place it either.
        ue_wrap::FVector loc{};
        ue_wrap::FRotator rot{};
        if (!ue_wrap::engine::TryGetActorLocation(actor, loc) || !ue_wrap::engine::TryGetActorRotation(actor, rot)) {
            UE_LOGW("npc-sync: connect-snapshot -- NPC eid=%u left out for slot %d, its location or rotation could "
                    "not be read", static_cast<unsigned>(el->GetId()), peerSlot);
            ++unread;
            continue;
        }
        coop::net::EntitySpawnPayload p{};
        const std::string& tn = el->GetTypeName();
        p.className.len = 0;
        for (size_t i = 0; i < tn.size() && i < 63; ++i)
            p.className.data[p.className.len++] = tn[i];
        p.elementId = static_cast<uint32_t>(el->GetId());
        const auto scl = ue_wrap::engine::GetActorScale3D(actor);  // mirror at true size
        p.locX = loc.X; p.locY = loc.Y; p.locZ = loc.Z;
        p.rotPitch = rot.Pitch; p.rotYaw = rot.Yaw; p.rotRoll = rot.Roll;
        p.scaleX = scl.X; p.scaleY = scl.Y; p.scaleZ = scl.Z;
        // Mark save-persisted NPCs so the joiner ADOPTS its own local twin (class-match in
        // coop/npc_adoption) instead of spawning a duplicate. A non-None int_save Key == this NPC
        // is a save object both peers booted (the kerfur); a host-spawned transient enemy has no
        // key -> savePersisted=0 -> the receiver fresh-spawns a mirror. Only the PRESENCE of a key
        // is portable, never its value: the kerfur's key is minted RANDOM per load, so it differs
        // across peers and key-equality adoption cannot work.
        p.savePersisted = ue_wrap::kerfur::HasSaveKey(actor) ? 1 : 0;
        // Carry the off->active dup RETIRE key for a kerfur that was OFF at the blob instant and
        // the host turned ON in the join window. GetOriginOffEidForEid returns the host EID of the
        // off-prop it replaced (kInvalidId for an always-active kerfur), and the joiner retires its
        // stale local off-prop MIRROR bound at that EXACT eid (kerfur_reconcile). This rides the
        // npc EntitySpawn -- the channel that DOES reach the joiner -- rather than the
        // KerfurConvert reliable, which is pre-world-gated mid-join.
        const coop::element::ElementId offEid = coop::kerfur_entity::GetOriginOffEidForEid(el->GetId());
        p.retireOffEid = (offEid == coop::element::kInvalidId) ? 0u : static_cast<uint32_t>(offEid);
        if (s->SendReliableToSlot(peerSlot, coop::net::ReliableKind::EntitySpawn, &p, sizeof(p)))
            ++sent;
    }
    UE_LOGI("npc-sync: connect-snapshot -- sent %d existing NPC(s) to slot %d (%zu Npc Element(s), %d unbound-skipped, "
            "%d unread-skipped)", sent, peerSlot, elems.size(), unbound, unread);
}

void TickPoseStream() {
    // HOST-only: read each live NPC's transform + CMC velocity each tick + publish ONE EntityPose
    // batch. Runs on the game thread from the net-pump tick; the batch is serialized once and
    // stamped per peer in session_streams.
    auto* s = GetSession();
    if (!s || s->role() != coop::net::Role::Host) return;
    const bool connected = s->connected();

    // Wisp lane: enroll EX_CallMath-caught spawns queued since the last tick (the
    // Func-thunk fires PRE-Finish; by this tick FinishSpawningActor ran -> real transform).
    coop::npc_world_enum::DrainPendingExSpawns();

    // DIAGNOSTIC ([dev] kerfur_census=1): a periodic HOST kerfur census, so a live run can diff
    // host against client without a rebuild.
    coop::kerfur_census::Tick();

    // Reused scratch (no per-tick heap alloc): Snapshot() clears+fills elems; batch is cleared then
    // refilled in-place and handed to SetLocalNpcPoseBatch BY CONST-REF (it copies), so this static
    // keeps its buffer across ticks. Safe because TickPoseStream is game-thread-only.
    static std::vector<coop::element::Npc*> elems;
    NpcMirrors().Snapshot(elems);
    static std::vector<coop::net::EntityPoseSnapshot> batch;
    batch.clear();

    // Fair-share rotation for >kMaxNpcBatchEntries tracked NPCs (the 32-wisp swarm
    // overflows the 31-entry MTU cap). Without it Snapshot's stable order starves the SAME tail
    // NPCs every tick (frozen mirrors); rotating the start index spreads the loss evenly
    // (~31/N of ticks each -- the MTA far-sync rotation shape). No wire change.
    static size_t s_rotateCursor = 0;
    const size_t total = elems.size();
    const size_t start = (total > static_cast<size_t>(coop::net::kMaxNpcBatchEntries))
                             ? (s_rotateCursor++ % total) : 0;

    int truncated = 0;
    for (size_t k = 0; k < total; ++k) {
        coop::element::Npc* el = elems[(start + k) % total];
        if (!el) continue;
        void* actor = el->GetActor();
        if (!actor) continue;  // never-bound (POST pending) -- not a death
        if (!R::IsLiveByIndex(actor, el->GetInternalIdx())) {
            // A BOUND actor that is no longer live died through a path the
            // K2_DestroyActor PRE cannot see (the wisp's EX_VirtualFunction self-destroy at
            // dawn/proximity). Without this the Element leaks and the client mirror ghosts
            // forever. Eid-keyed retire + EntityDestroy broadcast; deferred teardown. A kerfur
            // converted by its verb never reaches here: the converge at the verb's return released
            // its element in the same tick.
            coop::npc_sync::SyncDestroyedNpcByEid(el->GetId(), actor);
            continue;
        }
        // Before the peer check: a carry that starts while nobody is connected is already claimed
        // when the first peer arrives, and the drive's join resend parks it for them.
        SyncKerfurHold(actor);
        if (!connected) continue;  // no peers: lifecycle-only pass, no batch to build
        if (static_cast<int>(batch.size()) >= coop::net::kMaxNpcBatchEntries) { ++truncated; continue; }
        coop::net::EntityPoseSnapshot snap{};
        snap.elementId = static_cast<uint32_t>(el->GetId());
        ue_wrap::FVector loc{};
        ue_wrap::FRotator rot{};
        if (!ue_wrap::engine::TryGetActorLocation(actor, loc) || !ue_wrap::engine::TryGetActorRotation(actor, rot))
            continue;  // no pose this tick
        const auto vel = ue_wrap::engine::GetActorVelocity(actor);
        snap.x = loc.X; snap.y = loc.Y; snap.z = loc.Z;
        snap.yaw = ue_wrap::NormalizeAxis(rot.Yaw);
        snap.pitch = ue_wrap::NormalizeAxis(rot.Pitch);
        snap.roll = ue_wrap::NormalizeAxis(rot.Roll);
        snap.speed = std::sqrt(vel.X * vel.X + vel.Y * vel.Y);  // horizontal velocity magnitude
        snap.stateBits = ue_wrap::puppet::ReadCharacterIsFalling(actor) ? coop::net::kStateBitInAir : uint8_t{0};
        // Head-look: read the kerfur AnimBP's resolved `lookAt` WORLD target (the head/neck
        // FAnimNode_LookAt aim point). Only kerfur-family NPCs carry it (ReadKerfurLookAt is
        // class-gated -> false for non-kerfur NPCs); the bit tells the client when lookAt is valid.
        ue_wrap::FVector lookAt{};
        if (ue_wrap::puppet::ReadKerfurLookAt(actor, lookAt)) {
            snap.lookAtX = lookAt.X; snap.lookAtY = lookAt.Y; snap.lookAtZ = lookAt.Z;
            snap.stateBits |= coop::net::kEntityPoseBitHasLookAt;
            static bool s_loggedLook = false;
            if (!s_loggedLook) { s_loggedLook = true;
                UE_LOGI("npc-headlook: host streaming kerfur lookAt=(%.0f,%.0f,%.0f) eid=%u (first)",
                        lookAt.X, lookAt.Y, lookAt.Z, snap.elementId); }
        }
        // Body-facing: the kerfur actor BP aims the VISIBLE body (ACharacter::Mesh world yaw) at
        // the LOCAL player, decoupled from the actor root above. Read the resolved mesh world yaw
        // so the mirror reproduces the host's body facing (its actor tick is off -> it cannot
        // compute it).
        // The fossil hound's body turns apart from its root too: its mesh hangs on the SpringArm's end, which the
        // host's copy turns (the graph's tick, toward the velocity) and a mirror never does.
        float bodyYaw = 0.f;
        if (ue_wrap::puppet::ReadKerfurBodyYaw(actor, bodyYaw) ||
            ue_wrap::fossilhound::ReadBodyYaw(actor, bodyYaw) || ue_wrap::rufus::ReadBodyYaw(actor,bodyYaw)) {
            snap.bodyYaw = ue_wrap::NormalizeAxis(bodyYaw);
            snap.stateBits |= coop::net::kEntityPoseBitHasBodyYaw;
            static bool s_loggedBody = false;
            if (!s_loggedBody) { s_loggedBody = true;
                UE_LOGI("npc-bodyfacing: host streaming bodyYaw=%.1f eid=%u (first)",
                        snap.bodyYaw, snap.elementId); }
        }
        // Host-authoritative kerfur cosmetic/command state: the parked mirror runs no AI (actor
        // tick off + timers neutralized) so it cannot pick its own command/face. Stream the host
        // kerfur's State (enum_kerfurCommand -> AnimBP state machine) + isSpooky + face index so
        // the mirror's body animation matches. Class-gated (non-kerfur NPCs skip).
        uint8_t kState = 0, kFace = 0; bool kSpooky = false;
        if (ue_wrap::kerfur::ReadKerfurState(actor, kState, kSpooky, kFace)) {
            snap.kerfState = kState;
            snap.kerfFace  = kFace;
            snap.stateBits |= coop::net::kEntityPoseBitHasKerfurState;
            if (kSpooky) snap.stateBits |= coop::net::kEntityPoseBitKerfurSpooky;
            bool hasDisc = false;
            uint8_t discType = 0;
            if (ue_wrap::kerfur::ReadKerfurFloppy(actor, hasDisc, discType) && hasDisc) {
                snap.stateBits |= coop::net::kEntityPoseBitKerfurHasFloppy;
                snap.floppyType = discType;
            }
        }
        if (ue_wrap::sit_pose::ReadIsOnAtv(actor))
            snap.stateBits |= coop::net::kEntityPoseBitKerfurOnAtv;
        batch.push_back(snap);
    }
    SweepKerfurHolds();
    if (!connected) return;  // publish is peer-dependent
    if (truncated > 0) {
        static bool s_warnedTrunc = false;
        if (!s_warnedTrunc) { s_warnedTrunc = true;
            UE_LOGW("npc-pose: >%d NPCs this tick -- %d rotated out of this batch (fair-share "
                    "rotation active: each NPC streams ~%d/N of ticks; a persistent count this "
                    "high may warrant multi-datagram batches)",
                    coop::net::kMaxNpcBatchEntries, truncated, coop::net::kMaxNpcBatchEntries); }
    }
    static bool s_loggedFirst = false;
    if (!batch.empty() && !s_loggedFirst) { s_loggedFirst = true;
        UE_LOGI("npc-pose: streaming %zu NPC(s) -> peers (first batch)", batch.size()); }
    s->SetLocalNpcPoseBatch(batch);  // by const-ref copy; our static `batch` keeps its buffer
}

}  // namespace coop::npc_sync
