// coop/props/prop_drive_stream.cpp -- see coop/props/prop_drive_stream.h.

#include "coop/props/prop_drive_stream.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/local_prop_hold.h"
#include "coop/props/active_drive.h"
#include "coop/props/drive_generation.h"
#include "coop/props/prop_element_tracker.h"  // FindLiveActorByKey: the index only, never the cold walk
#include "coop/props/prop_park.h"             // Park / Unpark: the parked prop, and a Character welded on it
#include "coop/props/prop_wire_parity.h"      // the join converge's own physics restore
#include "coop/props/remote_prop.h"           // ResolveLiveActorByEid, IsActorUnderAnyDrive
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::prop_drive_stream {
namespace {

namespace AD = coop::active_drive;
namespace E  = ue_wrap::engine;
namespace PR = ue_wrap::prop;
namespace R  = ue_wrap::reflection;

struct Drive {
    AD::ActiveDrive d;
    ue_wrap::CachedObjRef actorRef, meshRef;
    void* LiveActor() const { return actorRef.Get(); }
    uint8_t gen = 0;
    bool physicsParked = false;   // this module turned simulation off, so the end turns it on
    uint32_t relatches = 0;       // the game switched the parked copy's simulation back on, this park
};

// A park whose copy the game switches back to simulating this many times is being fought every
// tick, not nudged once: said loudly, once, as the held-prop drive says it (remote_prop).
constexpr uint32_t kRelatchLoud = 60;
std::unordered_map<uint32_t, Drive>   g_drives;    // eid -> drive
std::unordered_map<uint32_t, uint8_t> g_endedGen;  // eid -> the generation its end edge closed

// A pose whose prop has not arrived here is retried on a throttle, never per tick: the host
// re-sends every tick the prop moves, and though the resolve is two O(1) lookups, a line per
// miss is not. One line names a sustained miss.
struct Miss { uint64_t nextTryMs = 0; uint32_t count = 0; };
std::unordered_map<uint32_t, Miss> g_miss;
constexpr uint64_t kMissRetryMs = 250;
constexpr uint32_t kMissSaidAt  = 40;     // about ten seconds of misses: one line

// An end edge whose prop has not arrived here yet -- a joiner mid-drag whose snapshot is still
// landing -- is kept until the prop resolves, so its copy still gets the rest pose the host's
// came to, or until it expires.
struct PendingEnd { coop::net::PropDriveEndPayload p; uint64_t expiresMs = 0; };
std::unordered_map<uint32_t, PendingEnd> g_pendingEnd;
constexpr uint64_t kPendingEndMs = 30000;

// The drained batch, kept across ticks for its capacity: the session swaps its buffer with this
// one, so the steady state of a drag allocates nothing on either thread. Game thread only.
std::vector<coop::net::PropPoseSnapshot> g_batch;

// Newer-than on the wrapping byte the stream carries.
bool GenAfter(uint8_t a, uint8_t b) { return drive_generation::After(a, b); }
bool Held(void* actor) {
    return local_prop_hold::IsHeld(actor) || remote_prop::IsActorUnderAnyDrive(actor);
}

// A frozen or static prop keeps its physics off whatever the stream did: it is moved kinematically
// and left so, the stuck wall-attachable rule the held-prop receiver applies.
bool PhysicsStaysOff(void* actor) { return PR::IsFrozen(actor) || PR::IsStatic(actor); }

// By eid first -- the registry's O(1) row, which a joiner's keyed copy carries from the bind --
// then the key INDEX; never the cold object-array walk the general key resolver falls back to.
void* Resolve(uint32_t eid, const coop::net::WireKey& key) {
    if (void* a = coop::remote_prop::ResolveLiveActorByEid(eid)) return a;
    if (key.len == 0) return nullptr;
    return coop::prop_element_tracker::FindLiveActorByKey(coop::remote_prop::KeyToWString(key));
}

void GiveBackPhysics(Drive& dr, void* actor) {
    if (!dr.physicsParked || !actor) return;
    const bool simulate = !PhysicsStaysOff(actor);
    coop::prop_park::Unpark(actor, PR::GetStaticMesh(actor), simulate);
    dr.physicsParked = false;
}

void YieldToHand(uint32_t eid, uint8_t gen, void* actor, Drive* dr) {
    // A local PHC/beam needs simulation. A remote hand already owns its kinematic
    // park, so restoring simulation there would break that other stream instead.
    if (dr && dr->LiveActor() == actor && local_prop_hold::IsHeld(actor)) GiveBackPhysics(*dr, actor);
    g_endedGen[eid] = gen;
    g_pendingEnd.erase(eid);
}

// The end edge on a live prop: the final pose, then the host's physics flags through the same
// parity restore the join's converge applies after its teleport, then the velocity -- only into
// a body this module parked and the flags leave simulating, and only when it is not zero, since
// assigning a velocity wakes a body at rest.
void ApplyEnd(void* actor, const coop::net::PropDriveEndPayload& p, bool parkedHere) {
    E::SetActorLocation(actor, ue_wrap::FVector{p.x, p.y, p.z});
    E::SetActorRotation(actor, ue_wrap::FRotator{p.pitch, p.yaw, p.roll});
    // The host's frozen and sleep at the end first: a hook that bites a frozen prop unfreezes it on
    // the host only (hook_C's attach_a runs setPropProps), so a copy still frozen here takes the
    // host's flags before the physics decision reads them.
    coop::prop_wire_parity::ConvergeFrozenSleep(actor, p.physFlags);
    if (!PhysicsStaysOff(actor)) {
        coop::prop_wire_parity::RestoreSpParityPhysicsAfterConverge(actor, p.physFlags);
        const float lin2 = p.linVelX * p.linVelX + p.linVelY * p.linVelY + p.linVelZ * p.linVelZ;
        if (parkedHere && coop::prop_wire_parity::SpParitySimulate(p.physFlags) && lin2 > 0.f) {
            void* mesh = PR::GetStaticMesh(actor);
            ue_wrap::engine::SetComponentLinearVelocity(mesh, p.linVelX, p.linVelY, p.linVelZ);
            ue_wrap::engine::SetComponentAngularVelocity(mesh, p.angVelX, p.angVelY, p.angVelZ);
        }
    }
    // The park is over, whatever the flags left the body: a Character it stopped moves again.
    coop::prop_park::Unpark(actor, /*mesh=*/nullptr, /*rootSimulates=*/false);
}

}  // namespace

void TickApplyAndDrive(coop::net::Session& s) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::TickApplyAndDrive");
    const uint64_t nowMs = AD::NowMs();
    g_batch.clear();
    if (s.TakeRemotePropDriveBatch(g_batch)) {
        for (const coop::net::PropPoseSnapshot& e : g_batch) {
            const uint32_t eid = e.elementId;
            if (eid == 0) continue;
            // The generation gate: a pose of a closed generation, or older than the live drive's,
            // is a datagram that outlived its stream.
            auto ended = g_endedGen.find(eid);
            if (ended != g_endedGen.end() && !GenAfter(e.ctx, ended->second)) continue;
            auto it = g_drives.find(eid);
            if (it != g_drives.end()) {
                if (!drive_generation::AtLeast(e.ctx, it->second.gen)) continue;
                if (it->second.gen == e.ctx) {
                    // The steady state: the row already names the actor. No resolve, no string.
                    if (void* live = it->second.LiveActor()) {
                        if (Held(live)) {
                            YieldToHand(eid, e.ctx, live, &it->second);
                            g_drives.erase(it);
                        } else
                            AD::BeginLerpToPose(it->second.d, ue_wrap::FVector{e.x, e.y, e.z},
                                                ue_wrap::FRotator{e.pitch, e.yaw, e.roll}, nowMs);
                        continue;
                    }
                }
            }
            // A first pose, a new generation, or a row whose actor died: resolve, on a throttle.
            auto miss = g_miss.find(eid);
            if (miss != g_miss.end() && nowMs < miss->second.nextTryMs) continue;
            void* actor = Resolve(eid, e.key);
            if (!actor) {
                Miss& m = g_miss[eid];
                m.nextTryMs = nowMs + kMissRetryMs;
                if (++m.count == kMissSaidAt)
                    UE_LOGI("[PROP-DRIVE] CLIENT eid=%u has not resolved in %u tries -- the host is "
                            "streaming a prop this peer does not have (a join still landing, or a "
                            "prop it never received)", eid, m.count);
                continue;
            }
            g_miss.erase(eid);
            if (!PR::IsDescendantOfProp(actor)) continue;
            // A hand's stream on the same prop wins: the holder is its syncer for as long as it
            // holds it, and the host closes this stream the moment it sees the hand.
            if (Held(actor)) {
                YieldToHand(eid, e.ctx, actor, it != g_drives.end() ? &it->second : nullptr);
                if (it != g_drives.end()) g_drives.erase(it);
                continue;
            }
            Drive& dr = g_drives[eid];
            if (dr.d.actor && dr.d.actor != actor) GiveBackPhysics(dr, dr.LiveActor());
            const bool alreadyParked = dr.physicsParked && dr.LiveActor() == actor;
            AD::ResetDriveState(dr.d);
            dr.d.actor    = actor;
            dr.d.actorIdx = R::InternalIndexOf(actor);
            dr.d.mesh     = PR::GetStaticMesh(actor);
            dr.actorRef.Set(actor);
            dr.meshRef.Set(dr.d.mesh);
            dr.d.isTrashMirror = true;  // the fixed-delay follow of a carried clump: freeze on a gap
            dr.d.lastEid  = eid;
            dr.gen        = e.ctx;
            dr.physicsParked = alreadyParked; // a new generation still owns this body's existing park
            dr.relatches  = 0;
            if (dr.d.mesh && !PhysicsStaysOff(actor)) {
                coop::prop_park::Park(actor, dr.d.mesh);
                dr.physicsParked = true;
            }
            if (ended != g_endedGen.end()) g_endedGen.erase(ended);
            // A pose of a closed generation never reaches here, so a pending end for this prop is
            // of an OLDER generation than the one parking now: stale, and dropped before it can
            // land on the new stream.
            g_pendingEnd.erase(eid);
            UE_LOGI("[PROP-DRIVE] CLIENT park eid=%u gen=%u actor=%p (physics %s)",
                    eid, static_cast<unsigned>(e.ctx), actor,
                    dr.physicsParked ? "off" : "left as it was");
            AD::BeginLerpToPose(dr.d, ue_wrap::FVector{e.x, e.y, e.z},
                                ue_wrap::FRotator{e.pitch, e.yaw, e.roll}, nowMs);
        }
    }
    for (auto it = g_drives.begin(); it != g_drives.end();) {
        Drive& dr = it->second;
        void* actor = dr.LiveActor();
        if (!actor) { it = g_drives.erase(it); continue; }   // the destroy crossed on its own seam
        if (Held(actor)) {
            // A hand took it before the end edge landed: the holder's drive owns the actor from here,
            // and its release restores the physics. This drive yields for good rather than writing
            // a stale target back once the hand lets go.
            UE_LOGI("[PROP-DRIVE] CLIENT yield eid=%u -- a held-prop stream owns the actor", it->first);
            YieldToHand(it->first, dr.gen, actor, &dr);
            it = g_drives.erase(it);
            continue;
        }
        // The physics receiver's re-latch, as the held-prop drive does it: a parked copy stays
        // kinematic when the game here switches its simulation back on. That includes a copy parked
        // frozen, which the park left as it was, and which an unstick or an unfreeze here has since
        // freed: it is parked kinematic from then on, and the end edge hands its physics back.
        if (dr.d.mesh && !dr.meshRef.Alive()) {
            dr.d.mesh = PR::GetStaticMesh(actor);
            dr.meshRef.Set(dr.d.mesh);
        }
        if (dr.d.mesh && ue_wrap::engine::IsComponentSimulatingPhysics(dr.d.mesh)) {
            coop::prop_park::Park(actor, dr.d.mesh);
            dr.physicsParked = true;
            if (++dr.relatches == 1) {
                UE_LOGI("[PROP-DRIVE] CLIENT eid=%u -- the game turned simulation back on under the park; "
                        "re-latched kinematic", it->first);
            } else if (dr.relatches == kRelatchLoud) {
                UE_LOGW("[PROP-DRIVE] CLIENT eid=%u -- re-latched %u times: something here keeps switching the "
                        "parked copy's simulation back on", it->first, static_cast<unsigned>(dr.relatches));
            }
        }
        AD::AdvanceLerp(dr.d, nowMs);
        ++it;
    }
    // An end edge that arrived before its prop: applied the moment the prop resolves.
    for (auto it = g_pendingEnd.begin(); it != g_pendingEnd.end();) {
        if (nowMs >= it->second.expiresMs || g_drives.count(it->first)) {
            it = g_pendingEnd.erase(it);   // expired, or a newer stream parked the prop first
            continue;
        }
        void* actor = coop::remote_prop::ResolveLiveActorByEid(it->first);
        if (!actor) { ++it; continue; }
        if (PR::IsDescendantOfProp(actor) && !Held(actor)) {
            ApplyEnd(actor, it->second.p, /*parkedHere=*/false);
            UE_LOGI("[PROP-DRIVE] CLIENT END eid=%u gen=%u -> (%.1f,%.1f,%.1f) applied late (the "
                    "prop arrived after its stream ended)", it->first,
                    static_cast<unsigned>(it->second.p.gen), it->second.p.x, it->second.p.y,
                    it->second.p.z);
        }
        it = g_pendingEnd.erase(it);
    }
}

void OnEnd(const coop::net::PropDriveEndPayload& p) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::OnEnd");
    if (!p.eid) return;
    const auto ended = g_endedGen.find(p.eid);
    if (ended != g_endedGen.end() && !GenAfter(p.gen, ended->second)) return;
    auto it = g_drives.find(p.eid);
    if (it == g_drives.end()) {
        // The actor may have appeared since an older end was queued. Once this newer
        // end lands immediately, that old retry must never overwrite it next tick.
        g_pendingEnd.erase(p.eid);
        // Never parked here. The generation still closes, so a pose of it in flight cannot park
        // the prop now; the rest pose is applied if the prop is here, kept for it if not, and
        // dropped only for a prop a hand holds -- that stream owns it.
        g_endedGen[p.eid] = p.gen;
        void* actor = coop::remote_prop::ResolveLiveActorByEid(p.eid);
        if (actor) {
            if (PR::IsDescendantOfProp(actor) && !Held(actor)) {
                ApplyEnd(actor, p, /*parkedHere=*/false);
                UE_LOGI("[PROP-DRIVE] CLIENT END eid=%u gen=%u -> (%.1f,%.1f,%.1f) (never parked here)",
                        p.eid, static_cast<unsigned>(p.gen), p.x, p.y, p.z);
            }
        } else {
            g_pendingEnd[p.eid] = PendingEnd{p, AD::NowMs() + kPendingEndMs};
        }
        return;
    }
    Drive& dr = it->second;
    if (!drive_generation::AtLeast(p.gen, dr.gen)) return;
    if (void* actor = dr.LiveActor()) {
        if (Held(actor)) {
            YieldToHand(p.eid, p.gen, actor, &dr);
            // A hand's stream parked this prop after ours did, and its release will hand the
            // physics back; touching the pose or the physics here would undo that park.
            UE_LOGI("[PROP-DRIVE] CLIENT END eid=%u gen=%u -- a held-prop stream owns the actor; "
                    "row dropped, nothing touched", p.eid, static_cast<unsigned>(p.gen));
        } else {
            ApplyEnd(actor, p, dr.physicsParked);
            dr.physicsParked = false;
            UE_LOGI("[PROP-DRIVE] CLIENT END eid=%u gen=%u -> (%.1f,%.1f,%.1f) flags=0x%02x",
                    p.eid, static_cast<unsigned>(p.gen), p.x, p.y, p.z,
                    static_cast<unsigned>(p.physFlags));
        }
    }
    g_drives.erase(it);
    g_endedGen[p.eid] = p.gen;
}

bool IsParked(void* actor) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::IsParked");
    if (!actor) return false;
    // The live actor, not the cached pointer: a row whose actor died is erased only on the next tick,
    // and a new actor at the freed address is not parked.
    for (const auto& kv : g_drives)
        if (kv.second.LiveActor() == actor) return true;
    return false;
}

bool TryGetPresentationVelocity(void* actor, ue_wrap::FVector& velocity) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::TryGetPresentationVelocity");
    if (!actor) return false;
    for (const auto& [eid, drive] : g_drives) {
        if (drive.LiveActor() != actor) continue;
        velocity = AD::PresentationVelocity(drive.d, AD::NowMs());
        return true;
    }
    return false;
}

void OnDisconnect() {
    for (auto& kv : g_drives) GiveBackPhysics(kv.second, kv.second.LiveActor());
    g_drives.clear();
    g_endedGen.clear();
    g_miss.clear();
    g_pendingEnd.clear();
    g_batch.clear();
}

}  // namespace coop::prop_drive_stream
