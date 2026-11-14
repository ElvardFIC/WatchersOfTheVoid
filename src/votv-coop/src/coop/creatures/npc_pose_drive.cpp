// coop/creatures/npc_pose_drive.cpp -- the CLIENT-mirror pose interpolator + engine drive for
// coop::element::Npc. See coop/element/npc.h. A SUBSET of remote_player.cpp (pos + full rotation +
// speed + stateBits; no headYawDelta/vitals/ragdoll/mesh-offset).
//
// The interp is the proven advance-before-rebase shape (the interp-starvation fix,
// the mirror starved of interpolation): SetTargetNpcPose advances the open window FIRST,
// then rebases. The drive writes the mirror's transform + CMC.Velocity/MovementMode so the
// NPC's OWN AnimBP animates natively (NPCs are ACharacter subclasses with the same CMC layout
// as the player puppet). The mirror's CMC tick is parked at spawn (npc_mirror) so this drive
// is authoritative -- no integration fight.

#include "coop/element/npc.h"

#include "coop/net/protocol.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/actors/fossilhound.h"  // DriveBodyYaw -- the hound's body facing
#include "ue_wrap/actors/kerfur.h"   // DriveKerfurState -- host-authoritative command and spooky flag
#include "ue_wrap/devices/floppy_slot.h"  // kSlotTypeCount -- the seven disc types floppyFromType indexes
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/actors/sit_pose.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/actors/wisp.h"     // DriveWispLanding -- the fade-in edge on a wisp_C mirror
#include "ue_wrap/actors/rufus.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace coop::element {
namespace {

namespace E   = ue_wrap::engine;
namespace Pup = ue_wrap::puppet;
namespace R   = ue_wrap::reflection;

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
// Shortest-arc delta in degrees, (-180, 180] -- avoids the 359->1 "long way round" spin (MTA).
float OffsetDegrees(float fromDeg, float toDeg) {
    float d = std::fmod(toDeg - fromDeg, 360.f);
    if (d > 180.f)  d -= 360.f;
    if (d < -180.f) d += 360.f;
    return d;
}

constexpr int   kInterpWindowMs  = 75;     // same as RemotePlayer (the proven jitter window)
constexpr float kSnapBaseCm      = 1000.f; // > one window's legal motion -> a real teleport, snap
constexpr float kSnapPerSpeedSec = 0.5f;

}  // namespace

void Npc::SetTargetNpcPose(const coop::net::EntityPoseSnapshot& snap) {
    const ue_wrap::FVector tgtPos{snap.x, snap.y, snap.z};

    // Head-look: store the streamed lookAt (a WORLD target, independent of the pos interp --
    // the native FAnimNode_LookAt node smooths it via its own InterpSpeed, so no LERP here).
    // Refreshed on EVERY packet (incl. a head-only turn with a stationary body), so the mirror
    // tracks the host kerfur's gaze at ~sendHz. Cleared when the host stops sending it.
    hasLookAt_ = (snap.stateBits & coop::net::kEntityPoseBitHasLookAt) != 0;
    if (hasLookAt_) curLookAt_ = ue_wrap::FVector{snap.lookAtX, snap.lookAtY, snap.lookAtZ};

    // Body-facing: the visible-body (ACharacter::Mesh) world yaw, interpolated like the actor
    // yaw (shortest-arc) and driven onto the mirror mesh in ApplyToEngine. Cleared -> body interp
    // term goes to a no-op when the host stops sending it (only the kerfur family and the fossil hound set the bit).
    hasBodyYaw_ = (snap.stateBits & coop::net::kEntityPoseBitHasBodyYaw) != 0;
    if (!hasBodyYaw_) errorBodyYaw_ = 0.f;

    // Host-authoritative kerfur command and spooky (snapped, not interpolated -- they select the
    // AnimBP state, not a pose). Applied in ApplyToEngine onto the parked mirror (which runs no AI
    // so nothing fights the write). Non-kerfur NPCs never set the bit -> hasKerfState_ stays false.
    hasKerfState_ = (snap.stateBits & coop::net::kEntityPoseBitHasKerfurState) != 0;
    if (hasKerfState_) {
        kerfState_  = snap.kerfState;
        kerfSpooky_ = (snap.stateBits & coop::net::kEntityPoseBitKerfurSpooky) != 0;
    }
    // The reports disc. Bit clear, or a type outside the seven the game indexes, is an empty slot.
    // Applied in Tick, not here: the mesh swap can fail while the library is still loading, and a
    // failed apply has to be asked again on a later frame even when the pose itself has settled.
    {
        const bool hasDisc = (snap.stateBits & coop::net::kEntityPoseBitKerfurHasFloppy) != 0;
        const int t = hasDisc ? static_cast<int>(snap.floppyType) : -1;
        wantFloppy_ = (t >= 0 && t < ue_wrap::floppy_slot::kSlotTypeCount)
                          ? static_cast<int8_t>(t) : int8_t{-1};
    }

    // First packet OR a teleport (error beyond the snap threshold) -> SNAP, no LERP across.
    const float dx = tgtPos.X - curPos_.X, dy = tgtPos.Y - curPos_.Y, dz = tgtPos.Z - curPos_.Z;
    const float distErr = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float snapThresh = kSnapBaseCm + kSnapPerSpeedSec * snap.speed;
    if (!hasPose_ || distErr > snapThresh) {
        curPos_ = tgtPos;
        curYaw_ = snap.yaw;
        curPitch_ = snap.pitch; curRoll_ = snap.roll;
        curSpeed_ = snap.speed;
        curStateBits_ = snap.stateBits;
        targetPos_ = tgtPos;
        targetYaw_ = snap.yaw;
        targetPitch_ = snap.pitch; targetRoll_ = snap.roll;
        errorPos_ = ue_wrap::FVector{};
        errorYaw_ = 0.f;
        errorPitch_ = errorRoll_ = 0.f;
        if (hasBodyYaw_) { curBodyYaw_ = snap.bodyYaw; targetBodyYaw_ = snap.bodyYaw; errorBodyYaw_ = 0.f; }
        window_.Close();  // frozen at target (snapped)
        hasPose_ = true;
        dirty_ = true;
        return;
    }

    // Advance-before-rebase (load-bearing -- the interp-starvation fix): bring curPos_ to NOW
    // using the STILL-OPEN window's cached error BEFORE overwriting the target/error below.
    AdvanceInterp();

    targetPos_ = tgtPos;
    targetYaw_ = snap.yaw;
    targetPitch_ = snap.pitch; targetRoll_ = snap.roll;
    curSpeed_ = snap.speed;          // not interpolated -- the AnimBP blends locomotion
    curStateBits_ = snap.stateBits;  // snapped
    errorPos_.X = tgtPos.X - curPos_.X;
    errorPos_.Y = tgtPos.Y - curPos_.Y;
    errorPos_.Z = tgtPos.Z - curPos_.Z;
    errorYaw_ = OffsetDegrees(curYaw_, snap.yaw);
    errorPitch_ = OffsetDegrees(curPitch_, snap.pitch);
    errorRoll_ = OffsetDegrees(curRoll_, snap.roll);
    if (hasBodyYaw_) { targetBodyYaw_ = snap.bodyYaw; errorBodyYaw_ = OffsetDegrees(curBodyYaw_, snap.bodyYaw); }
    window_.Open(NowMs(), kInterpWindowMs);
    dirty_ = true;
}

void Npc::AdvanceInterp() {
    if (!window_.IsOpen()) return;  // no window open -- frozen at target
    bool arrived = false;
    const float dAlpha = window_.Advance(NowMs(), &arrived);  // shared alpha/dAlpha bookkeeping
    curPos_.X += errorPos_.X * dAlpha;
    curPos_.Y += errorPos_.Y * dAlpha;
    curPos_.Z += errorPos_.Z * dAlpha;
    curYaw_     += errorYaw_     * dAlpha;
    curPitch_ += errorPitch_ * dAlpha;
    curRoll_ += errorRoll_ * dAlpha;
    curBodyYaw_ += errorBodyYaw_ * dAlpha;  // no-op when errorBodyYaw_ == 0, i.e. not tracking
    dirty_ = true;
    if (arrived) {
        curPos_ = targetPos_;  // exact arrival (kills float drift over the window)
        curYaw_ = targetYaw_;
        curPitch_ = targetPitch_; curRoll_ = targetRoll_;
        curBodyYaw_ = targetBodyYaw_;
    }
}

void Npc::Tick() {
    void* actor = GetActor();
    if (!actor || !R::IsLiveByIndex(actor, GetInternalIdx())) return;  // unbound / GC'd mirror
    if (!hasPose_) return;  // no pose yet -- leave the mirror at its spawn transform
    AdvanceInterp();
    if (dirty_) {
        dirty_ = false;
        if (!SameAsApplied(actor)) {
            if (ApplyToEngine()) RememberApplied(actor);
            else dirty_ = true;  // retry a failed write even when the interpolation window is closed
        }
    }
    if (isWispMirror_ && !wispLanded_ && !(curStateBits_ & coop::net::kStateBitInAir))
        wispLanded_ = ue_wrap::wisp::DriveWispLanding(actor);
    // The disc is not part of the pose dirty bit: a swap that failed (the mesh component or the
    // disc library not ready yet) retries on the next frame, including while the body is still.
    if (hasKerfState_ && shownFloppy_ != wantFloppy_) {
        const bool has = wantFloppy_ >= 0;
        const auto type = static_cast<uint8_t>(has ? wantFloppy_ : 0);
        if (ue_wrap::kerfur::DriveKerfurFloppy(actor, has, type))
            shownFloppy_ = wantFloppy_;
    }
}

bool Npc::SameAsApplied(void* actor) const {
    const Applied& a = applied_;
    if (a.actor != actor) return false;
    if (a.pos.X != curPos_.X || a.pos.Y != curPos_.Y || a.pos.Z != curPos_.Z) return false;
    if (a.pitch != curPitch_ || a.roll != curRoll_) return false;
    if (a.yaw != curYaw_ || a.speed != curSpeed_ || a.stateBits != curStateBits_) return false;
    if (a.hasLookAt != hasLookAt_ || a.hasBodyYaw != hasBodyYaw_ || a.hasKerfState != hasKerfState_)
        return false;
    if (hasLookAt_ && (a.lookAt.X != curLookAt_.X || a.lookAt.Y != curLookAt_.Y || a.lookAt.Z != curLookAt_.Z))
        return false;
    if (hasBodyYaw_ && a.bodyYaw != curBodyYaw_) return false;
    if (hasKerfState_ && (a.kerfState != kerfState_ || a.spooky != kerfSpooky_)) return false;
    return true;
}

void Npc::RememberApplied(void* actor) {
    applied_ = Applied{actor, curPos_, curLookAt_, curYaw_, curPitch_, curRoll_, curSpeed_, curBodyYaw_, curStateBits_,
                       kerfState_, hasLookAt_, hasBodyYaw_, hasKerfState_, kerfSpooky_};
}

bool Npc::ApplyToEngine() {
    void* actor = GetActor();
    if (!actor) return false;
    // NPC capsule centre IS the actor pivot (the host streams the actor's location, the mirror is the
    // same class spawned at the same place) -- no mesh-offset reconstruction (unlike RemotePlayer).
    const bool moved = E::SetActorLocation(actor, curPos_);
    const bool turned = E::SetActorRotation(actor, ue_wrap::FRotator{curPitch_, curYaw_, curRoll_});
    // Drive the mirror's OWN CMC so its AnimBP reads the right Velocity + MovementMode (the native
    // locomotion path -- the same fields the host NPC's possessed CMC carries). Reconstruct planar
    // velocity from the streamed body-yaw + speed magnitude.
    const float yawRad = curYaw_ * 0.01745329252f;  // PI/180
    const ue_wrap::FVector vel{ std::cos(yawRad) * curSpeed_, std::sin(yawRad) * curSpeed_, 0.f };
    const bool inAir = (curStateBits_ & coop::net::kStateBitInAir) != 0;
    Pup::DriveCharacterMovement(actor, vel, inAir);
    // Aim the mirror's head and neck at the host's streamed look target. Writes the kerfur
    // AnimBP `lookAt` + sets customLookAt=true so the mirror's own BUA stops auto-aiming at the
    // LOCAL player camera (the desync this fixes). Class-gated inside -> safe no-op on non-kerfur
    // NPCs. Re-asserted with every pose write, so a moving gaze tracks (customLookAt persists once set).
    if (hasLookAt_) Pup::DriveKerfurLookAt(actor, curLookAt_);
    // Drive the VISIBLE body facing -- set the mirror's ACharacter::Mesh WORLD yaw to the
    // host's streamed (interpolated) value. MUST be AFTER SetActorRotation above (moving the actor
    // root re-bases this child mesh's world transform). The mirror's actor tick is off, so the
    // kerfur BP never overwrites it -> no gate flag needed. Each is class-gated inside: the kerfur's, and the fossil
    // hound's (its mesh hangs on a SpringArm a mirror never turns).
    bool bodyReady=true;
    if (hasBodyYaw_) {
        Pup::DriveKerfurBodyYaw(actor, curBodyYaw_);
        ue_wrap::fossilhound::DriveBodyYaw(actor, curBodyYaw_);
        if(ue_wrap::rufus::IsFamily(actor))bodyReady=ue_wrap::rufus::DriveBodyYaw(actor,curBodyYaw_);
    }
    // Write the host-authoritative kerfur command and spooky flag so the parked mirror's AnimBP
    // state machine matches the host (the mirror runs no AI -> it can't pick its own). Class-gated
    // inside -> safe no-op on non-kerfur NPCs.
    if (hasKerfState_) ue_wrap::kerfur::DriveKerfurState(actor, kerfState_, kerfSpooky_);
    ue_wrap::sit_pose::DriveSitting(actor, (curStateBits_ & coop::net::kEntityPoseBitKerfurOnAtv) != 0);
    return moved && turned && bodyReady;
}

}  // namespace coop::element
