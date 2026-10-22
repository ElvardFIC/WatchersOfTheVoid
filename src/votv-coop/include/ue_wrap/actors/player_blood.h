#pragma once
#include "ue_wrap/core/types.h"
#include <functional>
namespace ue_wrap::script_gate { struct Call; }
namespace ue_wrap::player_blood {
struct Drop { FVector location{}, velocity{}; float size = 1.f; };
bool Valid(const Drop& drop);
// Only the native blood-loss effect's call to the shared blood manager.
bool ReadDrop(const script_gate::Call& call, Drop& drop);
// False means dependencies were not ready; once dispatched, never retry the drop.
bool Replay(const Drop& drop, const std::function<bool()>& current);
}
