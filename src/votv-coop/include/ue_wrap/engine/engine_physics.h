// ue_wrap/engine/engine_physics.h -- a primitive component's rigid body: simulation on or off,
// whether it simulates, its linear and angular velocity, and its centre of mass. Engine-wrapper layer (principle 7):
// each call is one reflected UFunction call on the component, with a stack parameter frame and no
// allocation in a warmed call. Functions and their schemas are cached by incarnation;
// unavailable or invalid schemas refuse dispatch and can resolve on a later call. Game thread.
// Implementation: src/ue_wrap/engine/engine_physics.cpp.

#pragma once

#include "ue_wrap/core/types.h"

namespace ue_wrap::engine {

// A live UPrimitiveComponent, the only thing the calls below dispatch on: a UPrimitiveComponent function run on
// anything else runs on the wrong layout.
bool IsPrimitiveComponent(void* component);

// UPrimitiveComponent::SetSimulatePhysics. A no-op on anything but a live primitive component, as each call here is.
// True when the call ran: a live primitive component, the UFunction resolved, the dispatch made.
bool SetComponentSimulatePhysics(void* component, bool simulate);

// SetPhysicsLinearVelocity (cm/s) and SetPhysicsAngularVelocityInDegrees, replacing the body's own
// (bAddToCurrent false). A kinematic body ignores both: switch its simulation on first. True when the call ran.
bool SetComponentLinearVelocity(void* component, float vx, float vy, float vz);
bool SetComponentAngularVelocity(void* component, float wx, float wy, float wz);

// IsSimulatingPhysics: USceneComponent's UFunction, which UPrimitiveComponent overrides natively.
// False for anything but a live primitive component, or when it did not resolve.
bool IsComponentSimulatingPhysics(void* component);

// GetCenterOfMass: the body's centre of mass in world space; for a body others are welded into, the whole
// weld's. False for anything but a live primitive component or when it did not resolve, `out` left as it was.
bool GetComponentCenterOfMass(void* component, FVector& out);

// Every primitive component the actor's Blueprint chain declares as a member (a component variable) that
// simulates is stopped; returns the number of successful current setters. For an actor driven from elsewhere,
// which must not also simulate. Bounded member layouts are cached by class/ancestor incarnation;
// each traversal owns its offset snapshot and rechecks actor, component and field binding.
// Null-safe.
int StopActorSimulating(void* actor);

}  // namespace ue_wrap::engine
