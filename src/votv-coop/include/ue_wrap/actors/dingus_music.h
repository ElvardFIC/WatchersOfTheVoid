#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <cstddef>
namespace ue_wrap::dingus_music {
struct State { bool playing=false,used=false; float volume=0; };
bool IsDingus(void* actor);
// Sound-only support also covers prop_abplush's qwak component.
bool IsSqueakyToy(void* actor);
bool IsAltUse(void* function,const uint8_t* params);
size_t Collect(CachedObjRef* out,size_t cap);
bool Read(void* actor,State& out);
bool Apply(void* actor,const State& state);
// Native action5 is touch followed by ws1.ToggleActive. No replay of action6,
// which also changes gameplay counters and can eventually destroy the toy.
bool Interact(void* actor,bool toggle);
bool ReadSqueakPitch(void* actor,float& pitch);
bool Squeak(void* actor,float pitch);
}
