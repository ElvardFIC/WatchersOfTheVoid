// coop/player/remote_player_ragdoll.h -- the remote player's ragdoll DISPLAY.
// Gameplay layer (principle 7): engine access via ue_wrap only. Game thread only.
//
// When a remote player ragdolls (wire kStateBitRagdoll), spawn VOTV's own
// playerRagdoll_C body with the puppet's active mesh and material overrides,
// and HIDE the puppet's two body meshes for the flop.
// Hiding is SetVisibility only, never a mesh-asset clear, so a pak-loaded
// custom-skin asset keeps its component reference and cannot be collected
// mid-flop. The puppet actor stays pelvis-attached to the body as the position
// anchor for the nameplate and the recover hand-off. The spawn is DEATH-FREE:
// ragdollMode is globally scoped and would kill the host, so it is never called.
//
// Owned by RemotePlayer (composition): the owner keeps its glue side effects keyed
// off the return values, so this class never reaches back into its state.

#pragma once
#include "ue_wrap/core/cached_obj_ref.h"

#include "coop/net/protocol.h"
#include "ue_wrap/engine/engine_playerragdoll.h"

#include <cstdint>
#include <chrono>

namespace coop {

class RagdollDisplay {
public:
    // Wire-bit edge detector (called on every pose packet with the streamed
    // kStateBitRagdoll). Spawns the body on the rising edge / recovers on the
    // falling edge. A failed spawn is retried with a backoff while the bit stays
    // set, including recovery after a body dies (the bit is continuous, so a
    // dropped edge self-heals on the next pose; TeardownForDestroy resets the
    // latch so a recycled puppet re-converges). Returns true when a STOP ran
    // this call: the owner re-bases its presentation yaw on the wire truth
    // (the get-up is a visual discontinuity anyway).
    bool OnWireBit(bool ragdollBit, void* puppetActor, int32_t puppetIdx);

    // Ragdoll physics sync: apply pelvis velocities and correct position by
    // translating all bodies together. First sample/teleport aligns immediately;
    // ordinary drift converges gradually. Validated authored-bone batches drive
    // a poseable selected-model display once complete; local physics bridges
    // entry and missing/unsupported pose data.
    // Returns true when applied to a live body; a packet racing ahead of the spawn
    // edge is dropped harmless, and the next one applies.
    bool SetPose(const coop::net::RagdollPoseSnapshot& snap);

    // ApplyToEngine head. While ragdolled the VISIBLE player-model body is the whole
    // display and the attachment owns the puppet transform -- ordinary standing
    // pose-driving would fight it. Fresh ragdoll packets correct the body's pelvis.
    enum class Drive {
        Inactive,   // not ragdolling -- the owner pose-drives as normal
        Attached,   // attachment owns the transform this tick -- owner returns
        StoppedNow  // body died under us (e.g. level-transition GC): self-
                    // healed (kel un-hidden, detach, clear) -- owner re-bases
                    // its yaw and falls through to normal pose-drive THIS tick
    };
    Drive DriveAttached(void* puppetActor, int32_t puppetIdx);

    // RemotePlayer::Destroy teardown: destroy the BODY only (the puppet actor
    // dies right after -- it would otherwise orphan the body; its hidden kel
    // meshes die with it, so no un-hide) + reset every latch so a recycled
    // puppet starts un-ragdolled and re-converges.
    void TeardownForDestroy();

    // Test/diagnostic seam (forwarded by RemotePlayer accessors).
    bool Active() const { return active_; }
    void* Body() const { return body_; }
    int32_t BodyIdx() const { return bodyIdx_; }

private:
    void Start(void* puppetActor, int32_t puppetIdx);
    void Stop(void* puppetActor, int32_t puppetIdx);

    // Last wire state; unsuccessful starts retry no faster than twice a second.
    bool wireState_ = false;
    std::chrono::steady_clock::time_point retryAfter_{};
    // The puppet is currently pelvis-attached to a body -> the attachment
    // drives the transform instead of the pose drive.
    bool active_ = false;
    bool pelvisAligned_ = false;
    bool simulationStarted_ = false;
    std::chrono::steady_clock::time_point simulationAfter_{};
    ue_wrap::CachedObjRef puppetRef_;
    uint8_t rigTag_ = 0;
    uint16_t rigBones_ = 0;
    uint32_t rigSignature_ = 0;
    std::vector<ue_wrap::engine::ArticulatedBone> bones_;
    // The spawned (visible) playerRagdoll_C body + its GUObjectArray index
    // (IsLiveByIndex safety across ticks). null/-1 when not ragdolled.
    ue_wrap::CachedObjRef bodyRef_;
    void*   body_ = nullptr;
    int32_t bodyIdx_ = -1;
};

}  // namespace coop
