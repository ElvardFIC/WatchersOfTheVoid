// coop/player/remote_player_ragdoll.cpp -- see coop/player/remote_player_ragdoll.h.

#include "coop/player/remote_player_ragdoll.h"

#include "coop/player/skin_effects.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"
#include <cmath>

namespace coop {

namespace R = ue_wrap::reflection;
namespace E = ue_wrap::engine;

namespace {

// Show/hide the puppet's TWO kel body meshes -- the native mesh slot and its
// mesh_playerVisible child, both of which render. VISIBILITY only (SetVisibility,
// propagate=false): the mesh ASSETS are untouched, so a pak-loaded custom-skin asset keeps its
// component reference and cannot be GC'd during the flop, which clearing the mesh would risk by
// dropping the only strong ref. Forcing visible=true on the restore side is a safe no-op when
// nothing was hidden -- visible IS the puppet's normal state, forced at spawn -- so the restore
// needs no bookkeeping.
void SetPuppetKelMeshesVisible(void* puppetActor, bool visible) {
    E::SetSceneComponentVisibility(
        ue_wrap::puppet::GetNativeBodyMeshComponent(puppetActor), visible, /*propagate=*/true);
    E::SetSceneComponentVisibility(
        ue_wrap::puppet::GetMeshPlayerVisibleComponent(puppetActor), visible, /*propagate=*/true);
}

}  // namespace

bool RagdollDisplay::OnWireBit(bool ragdollBit, void* puppetActor, int32_t puppetIdx) {
    const bool changed = ragdollBit != wireState_;
    wireState_ = ragdollBit;
    if (ragdollBit) {
        if (active_) return false;
        const auto now = std::chrono::steady_clock::now();
        if (!changed && now < retryAfter_) return false;
        retryAfter_ = now + std::chrono::milliseconds(500);
        Start(puppetActor, puppetIdx);
        return false;
    }
    if (!changed) return false;
    retryAfter_ = {};
    Stop(puppetActor, puppetIdx);
    return true;
}

bool RagdollDisplay::SetPose(const coop::net::RagdollPoseSnapshot& snap) {
    // Velocity alone never corrects positional drift after local collisions.
    // Translate the whole articulated body together so joints keep their pose.
    if (active_ && bodyRef_.Get()) {
        const float values[] = {snap.x, snap.y, snap.z, snap.linVelX, snap.linVelY,
                                snap.linVelZ, snap.angVelX, snap.angVelY, snap.angVelZ};
        for (float value : values) if (!std::isfinite(value)) return false;
        if (std::fabs(snap.x) > net::kMaxCoord || std::fabs(snap.y) > net::kMaxCoord ||
            std::fabs(snap.z) > net::kMaxCoord) return false;
        ue_wrap::FVector current{};
        void* mesh = E::GetRagdollBodyMesh(body_);
        if (mesh && E::GetBoneWorldLocationByName(mesh, L"pelvis", current) && bodyRef_.Get() &&
            std::isfinite(current.X) && std::isfinite(current.Y) && std::isfinite(current.Z)) {
            ue_wrap::FVector delta{snap.x - current.X, snap.y - current.Y, snap.z - current.Z};
            const float distanceSq = delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z;
            // First sample/teleport: align immediately. Normal drift converges
            // gradually, with a 2cm dead zone to avoid constant resting jitter.
            const bool snapNow = !pelvisAligned_ || distanceSq > 1000.f * 1000.f;
            if (snapNow || distanceSq > 2.f * 2.f) {
                const float alpha = snapNow ? 1.f : 0.25f;
                delta.X *= alpha; delta.Y *= alpha; delta.Z *= alpha;
                if (E::TranslateRagdollBody(body_, delta) && bodyRef_.Get()) pelvisAligned_ = true;
            }
        }
        if (!bodyRef_.Get()) return false;
        const ue_wrap::FVector linVel(snap.linVelX, snap.linVelY, snap.linVelZ);
        const ue_wrap::FVector angVel(snap.angVelX, snap.angVelY, snap.angVelZ);
        E::DriveRagdollBodyPelvisVelocity(body_, linVel, angVel);
        return true;
    }
    return false;
}

RagdollDisplay::Drive RagdollDisplay::DriveAttached(void* puppetActor, int32_t puppetIdx) {
    if (!active_) return Drive::Inactive;
    if (bodyRef_.Get()) {
        // The VISIBLE player-model body is the whole display; the puppet actor
        // just rides the pelvis attach. Nothing to drive per frame.
        return Drive::Attached;
    }
    // Self-heal: the body was GC-killed mid-ragdoll, a level transition having reaped it. Recover
    // NOW -- un-hide, detach, clear -- so the puppet is not stuck invisible on a dead attachment;
    // the owner resumes pose-drive this same tick.
    Stop(puppetActor, puppetIdx);
    return Drive::StoppedNow;
}

void RagdollDisplay::TeardownForDestroy() {
    // The body is a SEPARATE actor that would otherwise outlive the puppet as
    // an orphan. IsLiveByIndex-guarded so a GC-recycled address isn't mistaken
    // for our body. No detach + no kel un-hide: the puppet actor (and its
    // hidden meshes) is destroyed right after.
    if (bodyRef_.Get()) E::DestroyActor(body_);
    body_ = nullptr;
    bodyIdx_ = -1;
    bodyRef_.Reset();
    wireState_ = false;  // a recycled puppet starts un-ragdolled + re-converges
    retryAfter_ = {};
    active_ = false;
    pelvisAligned_ = false;
}

void RagdollDisplay::Start(void* puppetActor, int32_t puppetIdx) {
    if (!puppetActor || !R::IsLiveByIndex(puppetActor, puppetIdx)) return;
    ue_wrap::CachedObjRef puppetRef; puppetRef.Set(puppetActor);

    // Spawn VOTV's playerRagdoll_C VISIBLE with this puppet's current player mesh and materials.
    // If it fails, leave the puppet UPRIGHT and pose-driving (active_ stays false) -- never show the
    // placeholder ragdoll. The body's Player field points at this puppet; no ragdollMode is called.
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    if (!E::TryGetActorLocation(puppetActor, loc) || !E::TryGetActorRotation(puppetActor, rot)) {
        UE_LOGW("RagdollDisplay::Start: the puppet's location or rotation could not be read -- puppet stays upright "
                "(graceful)");
        return;
    }
    void* body = E::SpawnPlayerRagdollBody(puppetActor, loc, rot);
    if (!body) {
        UE_LOGW("RagdollDisplay::Start: playerRagdoll_C spawn failed -- puppet stays upright (graceful)");
        return;
    }
    body_ = body;
    bodyIdx_ = R::InternalIndexOf(body);
    bodyRef_.Set(body);

    // PELVIS-attach the puppet actor to the body: the position anchor (nameplate +
    // recover hand-off follow the flop). The puppet itself goes INVISIBLE next -- the
    // copied player model is the display. If the attach fails, destroy the body + stay upright.
    if (!puppetRef.Get() || !E::AttachActorToRagdollBody(puppetActor, body) || !bodyRef_.Get() || !puppetRef.Get()) {
        UE_LOGW("RagdollDisplay::Start: pelvis-attach failed -- destroying body, puppet stays upright");
        if (bodyRef_.Get()) E::DestroyActor(body);
        body_ = nullptr; bodyIdx_ = -1; bodyRef_.Reset();
        return;
    }
    // Hide the standing player model for the flop so it does not double-image with the
    // ragdoll. The skin effect rig (belly light / joint sparks) hides with it as well.
    SetPuppetKelMeshesVisible(puppetActor, false);
    coop::skin_effects::SetRigVisible(puppetActor, false);
    active_ = true;
    pelvisAligned_ = false;
    UE_LOGI("RagdollDisplay::Start: VISIBLE player-model body=%p spawned, puppet pelvis-attached + meshes hidden", body);
}

void RagdollDisplay::Stop(void* puppetActor, int32_t puppetIdx) {
    if (puppetActor && R::IsLiveByIndex(puppetActor, puppetIdx)) {
        // Un-hide the kel meshes FIRST (their normal state -- safe no-op if Start
        // never hid them), then detach (KeepWorld -- the puppet stays where the flop
        // left it; the next pose drives it back to the streamed pose) BEFORE
        // destroying the body it's attached to.
        SetPuppetKelMeshesVisible(puppetActor, true);
        coop::skin_effects::SetRigVisible(puppetActor, true);
        E::DetachActorFromRagdollBody(puppetActor);
    }
    // Destroy the physics body (IsLiveByIndex-guarded against a recycled addr).
    if (bodyRef_.Get()) E::DestroyActor(body_);
    body_ = nullptr;
    bodyIdx_ = -1;
    bodyRef_.Reset();
    active_ = false;
    pelvisAligned_ = false;
    UE_LOGI("RagdollDisplay::Stop: player meshes restored, puppet detached, ragdoll body destroyed");
}

}  // namespace coop
