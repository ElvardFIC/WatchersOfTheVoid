#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <cstddef>
#include <functional>
#include <memory>
namespace ue_wrap::watoy {
struct State { bool active=false; float volume=0.1f,pitch=1.f; bool effect=false; };
struct Presentation {CachedObjRef actor,cls,effect;bool ownsEffect=false,retired=false;uint64_t revision=0;uint32_t world=0;};
// Controllers stage copies while native calls can reenter. Copies share the
// presentation, but only the copy that admitted its latest revision may clear it.
struct View {
    std::shared_ptr<Presentation> presentation;
    uint64_t revision=0;
    CachedObjRef Effect()const{return presentation?presentation->effect:CachedObjRef{};}
    CachedObjRef Actor()const{return presentation?presentation->actor:CachedObjRef{};}
    bool OwnsEffect()const{return presentation && presentation->ownsEffect;}
};
bool IsToy(void* actor);
size_t Collect(CachedObjRef* out,size_t cap);
bool Read(void* actor,State& state);
// dispatched is set immediately before the native call. Once true, uncertain
// completion must be consumed rather than replaying the gameplay activation.
bool Activate(void* actor,const std::function<bool()>& current={},bool* dispatched=nullptr);
// Presentation only: no zero-gravity component, native timeline or physics verbs.
bool ApplyAudio(void* actor,const State& state,const std::function<bool()>& current={});
bool Apply(View& view,void* actor,const State& state,const std::function<bool()>& current);
void Clear(View& view);
bool PlayStartCue();
void* ZeroGravityOwner(void* component);
}
