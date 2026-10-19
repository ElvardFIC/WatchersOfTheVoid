// ue_wrap/core/trace.h -- standalone engine access for line-of-sight traces
// (UKismetSystemLibrary::LineTraceSingleForObjects). Principle-7 engine-wrapper
// layer: wraps the KSL CDO/UFunction resolve + the ParamFrame plumbing of a world
// line trace. NO network logic, NO gameplay state.
//
// One owner of the trace primitive: LineHitForObjects, over any object-type set and ignore list.
// The {WorldStatic, WorldDynamic} helpers below are that call with the native canReach's own set;
// wisp.cpp's canReach parity trace, nameplate occlusion, and the director's stuck line and slope
// detour use them.

#pragma once

#include "ue_wrap/core/types.h"

#include <cstdint>

namespace ue_wrap::trace {

struct Hit {
    bool blocked = false;
    void* actor = nullptr;
    void* component = nullptr;
    FVector point{};
    FVector normal{};
};

bool LineHitForObjects(void* worldCtx, const FVector& start, const FVector& end, const uint8_t* objectTypes,
                       int32_t nTypes, void* const* ignore, int32_t nIgnore, Hit* out);
bool LineHitStatDyn(void* worldCtx, const FVector& start, const FVector& end, Hit* out);

// Line trace start->end against the {WorldStatic, WorldDynamic} object set (the
// native canReach's own set: EObjectTypeQuery1/2 -- level geometry, doors, props;
// NOT pawns, so player bodies never self-block). bTraceComplex=false,
// bIgnoreSelf=true. `worldCtx` is any live actor (the WorldContextObject).
//
// Returns  1 = blocked (something static/dynamic between start and end),
//          0 = clear line,
//         -1 = unresolvable (KSL CDO/UFunction not ready, null ctx, call failed)
//              -- the caller picks its own safe default.
// Game thread only (dispatches a UFunction).
int LineBlockedStatDyn(void* worldCtx, const FVector& start, const FVector& end);

// Placement queries. Ignore worldCtx (the source actor). Box sweep also includes
// pawns/physics bodies/vehicles. Return -1 on missing metadata or failed calls.
int BoxBlocked(void* worldCtx, const FVector& start, const FVector& end, const FVector& halfSize);
// Static/dynamic floor with an upward normal; false includes unresolved/no hit.
bool FindFloor(void* worldCtx, const FVector& start, const FVector& end, FVector& point);

}  // namespace ue_wrap::trace
