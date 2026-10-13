// ue_wrap/actors/food.cpp -- see food.h.

#include "ue_wrap/actors/food.h"

#include "ue_wrap/core/reflection.h"
#include "ue_wrap/actors/canned_food.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include <cstring>

namespace ue_wrap::food {
namespace {

namespace R = reflection;

int32_t g_temp = -2;
int32_t g_ripe = -2;
int32_t g_uses = -2;
void*   g_cls  = nullptr;

bool Resolve() {
    if (g_temp != -2) return g_temp >= 0 && g_ripe >= 0 && g_uses >= 0;
    g_cls = R::FindClass(L"prop_food_C");
    if (!g_cls) return false;
    g_temp = R::FindPropertyOffset(g_cls, L"temperature");
    g_ripe = R::FindPropertyOffset(g_cls, L"ripeness");
    g_uses = R::FindPropertyOffset(g_cls, L"uses");
    return g_temp >= 0 && g_ripe >= 0 && g_uses >= 0;
}

}  // namespace

bool IsFood(void* actor) {
    if (!actor) return false;
    if (!g_cls && !Resolve()) return false;
    void* cls = R::ClassOf(actor);
    return cls && (cls == g_cls || R::IsDescendantOfAny(cls, &g_cls, 1));
}

bool Read(void* actor, Condition& out) {
    if (!IsFood(actor) || !Resolve()) return false;
    auto* b = reinterpret_cast<const uint8_t*>(actor);
    out.temperature = *reinterpret_cast<const float*>(b + g_temp);
    out.ripeness    = *reinterpret_cast<const float*>(b + g_ripe);
    out.uses        = *reinterpret_cast<const int32_t*>(b + g_uses);
    return true;
}

bool Write(void* actor, const Condition& in) {
    if (!IsFood(actor) || !Resolve()) return false;
    auto* b = reinterpret_cast<uint8_t*>(actor);
    *reinterpret_cast<float*>(b + g_temp)    = in.temperature;
    *reinterpret_cast<float*>(b + g_ripe)    = in.ripeness;
    *reinterpret_cast<int32_t*>(b + g_uses)  = in.uses;
    return true;
}

}  // namespace ue_wrap::food

namespace ue_wrap::canned_food {
namespace {
namespace R = reflection;
CachedObjRef g_class;
int32_t g_opened = -1, g_type = -1, g_foods = -1;
uint8_t g_mask = 0;
bool Resolve(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    void* base = object_index::ClassByName(L"prop_cannedFood_C");
    if (!base) return false;
    void* cls = R::ClassOf(actor);
    if (!cls || (cls != base && !R::IsDescendantOfAny(cls, &base, 1))) return false;
    if (!g_class.Is(base)) {
        g_class.Reset();
        g_mask = 0;
        if (!R::FindBoolProperty(base, L"opened", g_opened, g_mask) || !g_mask) return false;
        g_type = R::FindPropertyOffset(base, L"type");
        g_foods = R::FindPropertyOffset(base, L"foods");
        if (g_opened < 0 || g_type < 0 || g_foods < 0) return false;
        g_class.Set(base);
    }
    return true;
}
}
bool Read(void* actor, State& out) {
    if (!Resolve(actor)) return false;
    const auto* bytes = static_cast<const uint8_t*>(actor);
    State value;
    value.opened = (bytes[g_opened] & g_mask) != 0;
    std::memcpy(&value.type, bytes + g_type, sizeof(value.type));
    std::memcpy(&value.foods, bytes + g_foods, sizeof(value.foods));
    if (!Valid(value)) return false;
    out = value;
    return true;
}
bool Apply(void* actor, const State& state) {
    if (!Valid(state) || !Resolve(actor)) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(actor), L"init");
    if (!fn) return false;
    ParamFrame frame(fn);
    if (!frame.valid()) return false;
    auto* bytes = static_cast<uint8_t*>(actor);
    bytes[g_opened] = (bytes[g_opened] & ~g_mask) | (state.opened ? g_mask : 0);
    std::memcpy(bytes + g_type, &state.type, sizeof(state.type));
    std::memcpy(bytes + g_foods, &state.foods, sizeof(state.foods));
    CachedObjRef identity; identity.Set(actor);
    return Call(actor, frame) && identity.Is(actor);
}
bool RefreshHeldRecord(void* actor, save_record::SaveRecord& record) {
    // The five native can classes. Do not treat similarly named unrelated props
    // as cans or block their snapshots while the can class is unloaded.
    const auto& cls = record.className;
    if (cls != L"prop_cannedFood_C" && cls != L"prop_cannedFood_0_C" &&
        cls != L"prop_cannedFood_1_C" && cls != L"prop_cannedFood_2_C" &&
        cls != L"prop_cannedFood_3_C") return true;
    State state;
    if (!Read(actor, state) || R::ClassNameOf(actor) != cls || record.key.empty() ||
        record.key == L"None" || prop::GetInteractableKeyString(actor) != record.key ||
        record.bools.empty() || record.bools[0].size() < 4) return false;
    // Native getData: ints[0] = [type, foods], bools[1][0] = opened.
    // Keep base identity/flags and unrelated fields; change only these three cells.
    if (record.ints.empty()) record.ints.resize(1);
    if (record.ints[0].size() < 2) record.ints[0].resize(2);
    if (record.bools.size() < 2) record.bools.resize(2);
    if (record.bools[1].empty()) record.bools[1].resize(1);
    record.ints[0][0] = state.type;
    record.ints[0][1] = state.foods;
    record.bools[1][0] = state.opened ? 1 : 0;
    return true;
}
}
