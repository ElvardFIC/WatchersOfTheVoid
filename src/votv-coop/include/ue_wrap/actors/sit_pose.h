// ue_wrap/actors/sit_pose.h -- the kerfur AnimBP sit pose, the same sequence the Omega uses on
// the ATV. Player skins share that skeleton, so driving isOnAtv is what stops a seated body from
// standing through a chair. Engine-wrapper layer: no coop state. Game thread.

#pragma once

namespace ue_wrap::sit_pose {

// Write isOnAtv (and turn foot IK off while seated) on every kerfur-family AnimInstance of
// `pawn` -- mesh_playerVisible and the inherited Mesh slot on a player, the body mesh on an
// Omega. No-op on a null or dead actor or a non-kerfur AnimBP.
void DriveSitting(void* pawn, bool sitting);

// True iff any kerfur-family AnimInstance on `pawn` already has isOnAtv set (the Omega's own
// ATV sit, or a previous DriveSitting). False on a non-kerfur body.
bool ReadIsOnAtv(void* pawn);


// The driver's sit component (sitarrow). Null if the ATV is dead or the property is absent;
// the caller then seats the puppet on the rig itself.
void* AtvDriverSeat(void* atv);

// Slide the visible body on its parent, not the actor. The capsule stays where the game or the
// pose stream put it, so standing up does not start from inside the floor. `upCm` is world up
// (negative sinks the mesh). `forwardCm` is along the actor's facing. The base relative location
// is remembered on the first call and restored by ClearMeshShift. No-op on a null or dead pawn.
void ShiftMesh(void* pawn, float upCm, float forwardCm);
void ClearMeshShift(void* pawn);

// Half the kerfur body's length, and one head (an eighth of that length), from the capsule.
// The sit clip does not change them. Zero until the pawn has a capsule.
float ModelHalfLength(void* pawn);
float ModelHead(void* pawn);

}  // namespace ue_wrap::sit_pose
