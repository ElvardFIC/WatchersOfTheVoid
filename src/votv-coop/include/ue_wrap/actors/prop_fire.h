#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <functional>
#include <vector>
namespace ue_wrap::prop_fire {
struct State { bool burning=false; float fuel=0, fuelSpeed=1; };
bool Valid(const State& state);
bool IsProp(void* actor);
void* Owner(void* component);
void CollectBurning(std::vector<CachedObjRef>& actors);
bool Read(void* actor, State& state);
// Native verbs for the host. The client mirror bypasses subclass ignition events.
bool Ignite(void* actor,float fuel);
bool Extinguish(void* actor);
bool Apply(void* actor,const State& state,const std::function<bool()>& current);
}
