#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <cstdint>
namespace ue_wrap::madness {
struct State {bool active=false,hud=false,playing=false;int32_t kills=0;float elapsed=0;};
// Indexed class lookup, restricted to actors in the current world.
void* FindHost();
bool Read(void* actor,State& out);
struct View {CachedObjRef widget,audio;bool started=false;int32_t kills=-1;};
// Presentation only. Never creates the event master, grunts or weapons, and
// never calls ui_madnessCombat::addCount (which would spawn more enemies).
bool Apply(View& view,const State& state);
void Clear(View& view);
}
