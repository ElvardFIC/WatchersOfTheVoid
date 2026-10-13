// ue_wrap/engine/engine_playerragdoll.h -- the plushie ragdoll body the game spawns for a faint,
// driven for a puppet. Engine-wrapper layer (principle 7): each call marshals one UFunction call
// or one reflected field access, with no gameplay, network or coop state. Game thread unless a
// declaration says otherwise. Implementation: src/ue_wrap/engine/engine_playerragdoll.cpp.

#pragma once

#include "ue_wrap/core/types.h"

namespace ue_wrap::engine {

// Spawn playerRagdoll_C as an actor shell, then install the owner's selected mesh,
// materials and a compatible articulated physics asset before starting simulation.
// No plushie substitute on failure. Death-free (never calls ragdollMode).
void* SpawnPlayerRagdollBody(void* ownerPlayer, const FVector& location, const FRotator& rotation);

// Copy the selected model/materials and install the full compatible physical
// skeleton, replacing playerRagdoll_C's plushie physics override. No simulation
// starts until this succeeds. Unsupported rigs return false without a substitute.
bool ConfigurePlayerRagdollMesh(void* sourceMesh, void* targetMesh);

// Attach `actor` to `body`'s mesh at the pelvis bone (KeepWorld), so it follows the flop; Detach
// reverses it. Game thread.
bool AttachActorToRagdollBody(void* actor, void* body);

bool DetachActorFromRagdollBody(void* actor);

// The sender's ragdoll stream: the native ragdoll's pelvis world transform and velocity, in cm/s
// and deg/s. False while not ragdolling. Game thread.
bool ReadLocalRagdollPelvisPhysics(void* mainPlayer, FVector& outLoc, FRotator& outRot,
                                   FVector& outLinVel, FVector& outAngVel);

// The receiver's ragdoll stream: set the mirror body's pelvis linear and angular velocity so it
// tracks the sender's flop. Game thread.
void DriveRagdollBodyPelvisVelocity(void* body, const FVector& linVel, const FVector& angVel);

// A ragdoll actor's simulating SkeletalMesh; null if dead. Game thread.
void* GetRagdollBodyMesh(void* ragdollActor);

// The local player's native ragdoll mesh; the owning field is non-null exactly while ragdolling.
// Game thread.
void* GetLocalRagdollBodyMesh(void* mainPlayer);

}  // namespace ue_wrap::engine
