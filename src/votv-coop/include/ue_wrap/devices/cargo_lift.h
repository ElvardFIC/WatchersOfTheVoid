#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/types.h"
#include <string>
#include <vector>
#include <functional>

namespace ue_wrap::cargo_lift {
struct State {
    float alpha=0;
    FVector anchor{}, position{};
    FRotator rotation{};
    bool moving=false, direction=false;
};
bool IsLift(void* actor);
std::wstring Key(void* actor);
void Enumerate(std::vector<CachedObjRef>& out);
int ButtonIndex(void* actor, void* component);
bool ButtonPosition(void* actor, int button, FVector& out);
bool Read(void* actor, State& out);
bool Activate(void* actor, int button, void* player);
// Replay the host's accepted button using the native movement/physics. Returns
// false while a previous native trip or incomplete component setup must finish.
bool Replay(void* actor,const State& state,int button,void* player,
            const std::function<bool()>& stillCurrent);
bool Valid(const State& state);
}
