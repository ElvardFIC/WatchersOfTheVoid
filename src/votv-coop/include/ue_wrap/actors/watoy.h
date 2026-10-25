#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <cstddef>
#include <functional>
namespace ue_wrap::watoy {
struct State { bool active=false; float volume=0.1f,pitch=1.f; bool effect=false; };
struct View {CachedObjRef actor,effect;bool ownsEffect=false;uint64_t revision=0;};
bool IsToy(void* actor);
size_t Collect(CachedObjRef* out,size_t cap);
bool Read(void* actor,State& state);
bool Activate(void* actor,const std::function<bool()>& current={});
// Presentation only: no zero-gravity component, native timeline or physics verbs.
bool ApplyAudio(void* actor,const State& state,const std::function<bool()>& current={});
bool Apply(View& view,void* actor,const State& state,const std::function<bool()>& current);
void Clear(View& view);
bool PlayStartCue();
void* ZeroGravityOwner(void* component);
}
