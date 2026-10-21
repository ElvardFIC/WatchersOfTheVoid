// ue_wrap/actors/food.h -- a world food's live condition: temperature, ripeness, use count.
//
// These three live on the actor and move every tick. The save record is not their carrier:
// loadData treats the capturing peer's clock stamp as elapsed time. Direct field writes, no
// loadData. Engine-wrapper layer: no network. Game thread.

#pragma once

#include <cstdint>
#include <functional>

namespace ue_wrap::food {

struct Condition {
    float   temperature = 0.f;
    float   ripeness    = 0.f;
    int32_t uses        = 0;
    float   cookProgress = -1.f; // -1 for food outside prop_cookingFood's family
};

// True for prop_food_C and every class below it. False until that class has loaded.
bool IsFood(void* actor);

// Read the three fields. False when the actor is not a food or a field did not resolve.
bool Read(void* actor, Condition& out);

// Write the three fields in place. False on the same misses. Does not call loadData.
bool Write(void* actor, const Condition& in,const std::function<bool()>& current = {});

}  // namespace ue_wrap::food
