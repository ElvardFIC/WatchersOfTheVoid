#pragma once

namespace ue_wrap::floor_bed {
// sleepingbagWrap_C and its colored/mattress variants, not rolled props or bunks.
// Game thread. No class pointer survives a world reload.
bool IsFloorBed(void* actor);
// Same class-family predicate for a reflected UClass, used to authorize a client's deployed bag.
bool IsFloorBedClass(void* actorClass);

// A fresh floor bed skips prop::init, so its save key can remain None. Dispatch
// the inherited native getKey to generate/register it, without mesh/physics init.
// Existing keys are preserved. Call only for a known new world birth, after any
// saved-key restoration. False on a missing verb, reentry or stale actor/readback.
bool EnsureKey(void* actor);
}
