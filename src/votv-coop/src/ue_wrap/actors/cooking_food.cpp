#include "ue_wrap/actors/cooking_food.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::cooking_food {
namespace {
namespace R=reflection;
bool Valid(float value){return std::isfinite(value)&&value>=0&&value<=1'000'000.f;}
}
bool IsCookingClass(void* cls) {
    for(int depth=0;cls && depth<64 && R::IsLive(cls);++depth,cls=R::SuperStructOf(cls))
        if(R::NameEquals(R::NameOf(cls),L"prop_cookingFood_C"))return true;
    return false;
}
bool IsCookingFood(void* actor) {
    return actor && R::IsLive(actor) && IsCookingClass(R::ClassOf(actor));
}
bool ReadProgress(void* actor,float& progress) {
    if(!IsCookingFood(actor))return false;
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();
    static R::InstanceOffset field{L"cookProgress"};const int off=field.Of(actor);
    if(off<0||!live.Get()||world!=world_identity::Generation())return false;
    float value=0;std::memcpy(&value,static_cast<uint8_t*>(actor)+off,sizeof(value));
    if(!Valid(value))return false;progress=value;return true;
}
bool WriteProgress(void* actor,float progress) {
    if(!Valid(progress)||!IsCookingFood(actor))return false;
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();
    static R::InstanceOffset field{L"cookProgress"};const int off=field.Of(actor);
    if(off<0||!live.Get()||world!=world_identity::Generation())return false;
    std::memcpy(static_cast<uint8_t*>(actor)+off,&progress,sizeof(progress));
    return true; // no cookItem/cooked, births, slicing or native catch-up
}
bool SaveNative(void* actor,void* record) {
    float progress=0;return ReadProgress(actor,progress)&&save_record::ReplaceFloatGroup(record,2,{progress});
}
bool LoadNative(void* actor,const void* record) {
    if(!IsCookingFood(actor)||!record)return false;
    std::vector<float> fields;
    if(!save_record::ReadFloatGroup(record,2,fields))return WriteProgress(actor,0.f); // original native save
    return fields.size()==1&&WriteProgress(actor,fields[0]);
}
bool RefreshHeldRecord(void* actor,save_record::SaveRecord& record) {
    if(!IsCookingFood(actor))return record.className.compare(0,16,L"prop_cookingFood")!=0;
    if(R::ClassNameOf(actor)!=record.className||record.key.empty()||record.key==L"None")return false;
    const int key=R::FindPropertyOffset(R::ClassOf(actor),L"key");
    float progress=0;
    if(key<0||save_record::ReadFNameAt(actor,key)!=record.key||!ReadProgress(actor,progress))return false;
    if(record.floats.size()<3)record.floats.resize(3);
    record.floats[2]={progress};return true;
}
}
