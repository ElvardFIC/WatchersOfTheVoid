#include "ue_wrap/devices/grill_saved_state.h"
#include "ue_wrap/devices/grill.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/component_calls.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::grill_saved_state {
namespace {
namespace R=reflection;
constexpr float kMarker=43217.f,kVersion=1.f;
bool Valid(const std::vector<float>& fields){return fields.size()==4 && fields[0]==kMarker && fields[1]==kVersion &&
    std::isfinite(fields[2]) && fields[2]>=0 && fields[2]<=1.01f && (fields[3]==0 || fields[3]==1);}
struct Fields {
    CachedObjRef actor;
    uint32_t world=0;
    int burn=-1,active=-1;
    uint8_t mask=0;
    bool Current()const{return actor.Get() && world==world_identity::Generation();}
    bool Resolve(void* object){
        if(!Supported(object))return false;actor.Set(object);world=world_identity::Generation();
        static R::InstanceOffset burnField{L"a"};burn=burnField.Of(object);
        return burn>=0 && R::FindBoolProperty(R::ClassOf(object),L"active",active,mask) && active>=0 && mask && Current();
    }
};
bool Read(void* actor,std::vector<float>& value){
    Fields fields;if(!fields.Resolve(actor))return false;float burn=0;
    std::memcpy(&burn,static_cast<uint8_t*>(actor)+fields.burn,sizeof(burn));
    std::vector<float> result{kMarker,kVersion,burn,float(bool(static_cast<uint8_t*>(actor)[fields.active]&fields.mask))};
    if(!Valid(result) || !fields.Current())return false;value=std::move(result);return true;
}
}
bool Supported(void* actor){return grill::IsGrill(actor);}
bool SaveNative(void* actor,void* record){
    std::vector<float> values;if(!record || !Read(actor,values))return false;
    std::vector<float> existing;
    if(save_record::ReadFloatGroup(record,1,existing) && !existing.empty() && !Valid(existing))return false;
    return save_record::ReplaceFloatGroup(record,1,values);
}
bool LoadNative(void* actor,const void* record){
    if(!record || !Supported(actor))return false;
    std::vector<float> values;
    if(!save_record::ReadFloatGroup(record,1,values) || values.empty())return true; // native saves have no extension
    if(!Valid(values))return false;
    Fields fields;if(!fields.Resolve(actor))return false;
    auto* bytes=static_cast<uint8_t*>(actor);std::memcpy(bytes+fields.burn,&values[2],sizeof(float));
    bytes[fields.active]=values[3]!=0?bytes[fields.active]|fields.mask:bytes[fields.active]&~fields.mask;
    // Repaint fire/materials only. Native ticking remains with the owning world or hand.
    return component_calls::CallParamlessNamed(actor,L"upd") && fields.Current();
}
bool RefreshHeldRecord(void* actor,save_record::SaveRecord& record){
    if(!Supported(actor))return true;
    if(record.className!=R::ClassNameOf(actor))return false;
    std::vector<float> values;if(!Read(actor,values))return false;
    if(record.floats.size()>2 || (record.floats.size()==2 && !record.floats[1].empty() && !Valid(record.floats[1])))return false;
    if(record.floats.size()<2)record.floats.resize(2);record.floats[1]=std::move(values);return true;
}
bool PreserveHostRecord(void* actor,save_record::SaveRecord& record){
    if(!Supported(actor))return true;
    CachedObjRef identity;identity.Set(actor);const auto world=world_identity::Generation();
    const auto current=[&]{return identity.Get() && world==world_identity::Generation();};
    auto adjusted=record;
    if(!RefreshHeldRecord(actor,adjusted) || !current())return false;
    grill::Stock stock{};if(!grill::ReadStock(actor,stock,current) || !current())return false;
    // Native getData/loadData: coals are ints[0][0], hasLid is bools[1][0].
    // Keep unrelated base groups and fields unchanged.
    if(adjusted.ints.empty())adjusted.ints.resize(1);
    if(adjusted.ints[0].empty())adjusted.ints[0].resize(1);
    if(adjusted.bools.size()<2)adjusted.bools.resize(2);
    if(adjusted.bools[1].empty())adjusted.bools[1].resize(1);
    adjusted.ints[0][0]=stock.coals;adjusted.bools[1][0]=uint8_t(stock.lid);
    record=std::move(adjusted);return true;
}
}
