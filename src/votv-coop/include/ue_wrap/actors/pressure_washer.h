#pragma once
#include "ue_wrap/core/types.h"
#include <functional>

namespace ue_wrap::pressure_washer {
bool IsWasher(void* actor);
bool Read(void* actor, bool& firing, float& level);
// A hand display never runs the native Tick that spawns damaging water projectiles.
bool SetRemoteLevel(void* actor, float level,const std::function<bool()>& current = {});
// One native particle burst and its attenuated sound; no gameplay projectile.
bool EmitRemote(void* actor, const FVector& origin, const FRotator& aim,const std::function<bool()>& current = {});
}
