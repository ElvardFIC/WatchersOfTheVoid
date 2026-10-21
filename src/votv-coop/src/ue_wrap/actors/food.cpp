// ue_wrap/actors/food.cpp -- see food.h.

#include "ue_wrap/actors/food.h"
#include "ue_wrap/actors/cooking_food.h"

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

int32_t g_temp = -1;
int32_t g_ripe = -1;
int32_t g_uses = -1;
CachedObjRef g_class;

bool Resolve() {
    void* cls=object_index::ClassByName(L"prop_food_C");if(!cls)return false;
    if(g_class.Is(cls))return true;
    CachedObjRef live;live.Set(cls);const auto world=world_identity::Generation();
    const auto temperature=R::FindPropertyOffset(cls,L"temperature");
    const auto ripeness=R::FindPropertyOffset(cls,L"ripeness");
    const auto uses=R::FindPropertyOffset(cls,L"uses");
    if(temperature<0 || ripeness<0 || uses<0 || !live.Get() || world!=world_identity::Generation())return false;
    g_temp=temperature;g_ripe=ripeness;g_uses=uses;g_class=live;return true;
}

}  // namespace

bool IsFood(void* actor) {
    if (!actor || !R::IsLive(actor) || !Resolve()) return false;
    void* cls = R::ClassOf(actor);
    void* base=g_class.Get();
    return cls && base && (cls == base || R::IsDescendantOfAny(cls, &base, 1));
}

bool Read(void* actor, Condition& out) {
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();
    if (!IsFood(actor) || !live.Get() || world!=world_identity::Generation()) return false;
    const auto base=g_class;void* cls=R::ClassOf(actor);
    auto* b = reinterpret_cast<const uint8_t*>(actor);
    Condition value{};
    std::memcpy(&value.temperature,b+g_temp,sizeof(float));
    std::memcpy(&value.ripeness,b+g_ripe,sizeof(float));
    std::memcpy(&value.uses,b+g_uses,sizeof(int32_t));
    if(cooking_food::IsCookingFood(actor)&&!cooking_food::ReadProgress(actor,value.cookProgress))return false;
    if(!live.Get() || !base.Get() || R::ClassOf(actor)!=cls || world!=world_identity::Generation())return false;
    out=value;return true;
}

bool Write(void* actor, const Condition& in,const std::function<bool()>& current) {
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();
    if (!IsFood(actor) || !live.Get() || world!=world_identity::Generation()) return false;
    const auto base=g_class;void* cls=R::ClassOf(actor);const int temp=g_temp,ripe=g_ripe,uses=g_uses;
    if(current && !current())return false;
    if(in.cookProgress>=0 && !cooking_food::WriteProgress(actor,in.cookProgress))return false;
    if(!live.Get() || !base.Get() || R::ClassOf(actor)!=cls || world!=world_identity::Generation() || (current && !current()))return false;
    auto* b = reinterpret_cast<uint8_t*>(actor);
    std::memcpy(b+temp,&in.temperature,sizeof(float));
    std::memcpy(b+ripe,&in.ripeness,sizeof(float));
    std::memcpy(b+uses,&in.uses,sizeof(int32_t));
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
