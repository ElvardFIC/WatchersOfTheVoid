#pragma once
#include "ue_wrap/core/types.h"
#include <functional>
namespace ue_wrap::sponge_effects {
struct Use {FVector location{},normal{};float soap=0;};
bool IsSponge(void* actor);
bool ReadUse(void* actor,void* player,Use& use);
bool ReadSoap(void* actor,float& soap);
inline bool ValidSurface(const FVector& location,const FVector& normal) {
    for(const auto value:{location.X,location.Y,location.Z})if(!std::isfinite(value) || std::fabs(value)>1.e7f)return false;
    const float length=normal.X*normal.X+normal.Y*normal.Y+normal.Z*normal.Z;
    return std::isfinite(length) && length>=.5f && length<=1.5f;
}
bool Surface(const FVector& location,const FVector& normal,const std::function<bool()>& current);
// Once dispatched, one-shot effects are consumed even if the world changes in
// the native callback. False only allows retry before native dispatch.
bool Burst(void* actor,const std::function<bool()>& current = {});
}
