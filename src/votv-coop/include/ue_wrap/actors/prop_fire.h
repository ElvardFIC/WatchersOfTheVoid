#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <functional>
#include <vector>
namespace ue_wrap::prop_fire {
struct State { bool burning=false; float fuel=0, fuelSpeed=1; };
bool Valid(const State& state);
bool IsProp(void* actor);
bool IsGasoline(void* actor);
void* Owner(void* component);
void CollectBurning(std::vector<CachedObjRef>& actors);
bool Read(void* actor, State& state);
// Native verbs for the host. The client mirror bypasses subclass ignition events.
// attempted becomes true immediately before dispatch, even if native completion fails.
bool Ignite(void* actor,float fuel,bool* attempted=nullptr,const std::function<bool()>& current={});
bool Extinguish(void* actor,bool* attempted=nullptr,const std::function<bool()>& current={});
bool Apply(void* actor,const State& state,const std::function<bool()>& current);
}
// ue_wrap/actors/prop_fire.h -- the prop's fire members: the flame a prop carries once it burns, and the verbs that
// light it. Engine-wrapper layer (principle 7). Implementation: src/ue_wrap/actors/prop_fire.cpp.

#pragma once

namespace ue_wrap::prop {

// Whether the prop's `flameBase` (an object field of prop_C) holds a live object, so whether the prop is burning.
// False when the prop's class declares no such field. Game thread.
bool PropFlameBaseValid(void* prop);

// The prop's attemptIgnite(), resolved on its own class (FindDispatchFunctionCached). False when the function does
// not resolve or the call could not run. Game thread.
bool CallAttemptIgnite(void* prop);

// The prop_C's ignite(fuel) on `prop`, resolved on its own class as above. False when the function does not resolve
// or the call could not run. Game thread.
bool CallIgnite(void* prop, float fuel);

}  // namespace ue_wrap::prop
