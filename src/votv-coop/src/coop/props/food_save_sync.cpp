#include "coop/props/food_save_sync.h"
#include "ue_wrap/actors/cooking_food.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/log.h"
namespace coop::props::food_save_sync {
namespace {
namespace SG=ue_wrap::script_gate;
namespace R=ue_wrap::reflection;
UE_FACT_FN(kSave,"food_save",L"prop_food_C",L"getData",Declared);
UE_FACT_FN(kLoad,"food_save",L"prop_food_C",L"loadData",Declared);
bool g_save=false,g_load=false;
void* Record(const SG::Call& c) {
    if(!c.function)return nullptr;
    const int off=R::FindParamOffset(c.function,L"data"),size=R::FunctionFrameSize(c.function);
    if(off<0||off>size||size-off<ue_wrap::save_record::kSaveRecordBytes)return nullptr;
    if(auto* out=SG::OutParamPtr(c,off))return out;
    return c.locals?c.locals+off:nullptr;
}
void Saved(const SG::Call& c) {
    if(ue_wrap::cooking_food::IsCookingFood(c.object)&&!ue_wrap::cooking_food::SaveNative(c.object,Record(c)))
        UE_LOGW("food_save: cooking progress could not be saved");
}
void Loaded(const SG::Call& c) {
    if(ue_wrap::cooking_food::IsCookingFood(c.object)&&!ue_wrap::cooking_food::LoadNative(c.object,Record(c)))
        UE_LOGW("food_save: saved cooking progress was unreadable");
}
}
void Install() {
    constexpr int tag=0x46445356;
    if(!g_save)g_save=SG::WatchName(kSave,tag,nullptr,&Saved);
    if(!g_load)g_load=SG::WatchName(kLoad,tag,nullptr,&Loaded);
}
}
