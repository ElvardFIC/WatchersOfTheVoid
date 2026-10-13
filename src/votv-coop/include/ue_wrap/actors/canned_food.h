#pragma once
#include <cstdint>
namespace ue_wrap::save_record { struct SaveRecord; }

namespace ue_wrap::canned_food {
struct State {
    bool opened = false;
    int32_t type = 0;
    int32_t foods = 0;
    bool operator==(const State&) const = default;
};
// prop_cannedFood_C and descendants only. No native meal or inventory operation.
bool Read(void* actor, State& out);
// Writes reflected fields and calls init() to derive the mesh. Caller owns display
// physics/collision and must recheck its session/actor identity after this call.
bool Apply(void* actor, const State& state);
// Refresh a copied hold record from the matching live actor; unrelated classes pass
// through. False defers a can snapshot whose identity/state cannot be confirmed.
// Does not call native verbs or mutate the live inventory.
bool RefreshHeldRecord(void* actor, save_record::SaveRecord& record);
inline bool Valid(const State& s) {
    return s.type >= 0 && s.type <= 3 && s.foods >= 0 && s.foods <= 4095;
}
}
