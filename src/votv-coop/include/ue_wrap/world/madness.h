#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <cstdint>
#include <functional>
namespace ue_wrap::madness {
struct State {bool active=false,hud=false,playing=false;int32_t kills=0;float elapsed=0;};
// Indexed class lookup, restricted to actors in the current world.
void* FindHost();
bool Read(void* actor,State& out);
struct View {
    CachedObjRef widget,audio,widgetClass,audioClass;
    bool started=false;int32_t kills=-1;uint32_t world=0;bool shown=false;
    uint64_t revision=0;void* worldObject=nullptr;
};
// Presentation only. Never creates the event master, grunts or weapons, and
// never calls ui_madnessCombat::addCount (which would spawn more enemies).
bool Apply(View& view,const State& state,const std::function<bool()>& current = {});
void Clear(View& view);
}
