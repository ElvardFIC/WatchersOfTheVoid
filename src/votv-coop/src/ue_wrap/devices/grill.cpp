#include "ue_wrap/devices/grill.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::grill {
namespace {
namespace R=reflection;
struct Fields {
    int active=-1,lid=-1,coals=-1,burn=-1,volume=-1,sound=-1;
    uint8_t activeMask=0,lidMask=0;
    CachedObjRef actor,audio;
    uint32_t world=0;
    bool Current()const {
        if(!actor.Get() || !audio.Get() || world!=world_identity::Generation())return false;
        void* component=nullptr;std::memcpy(&component,static_cast<uint8_t*>(actor.Get())+sound,sizeof(component));
        return component==audio.Raw();
    }
    bool Resolve(void* object) {
        if(!IsGrill(object))return false;
        actor.Set(object);world=world_identity::Generation();
        static R::InstanceOffset coalsField{L"coals"},burnField{L"a"},audioField{L"cookingLoop"},volumeField{L"VolumeMultiplier"};
        coals=coalsField.Of(object);burn=burnField.Of(object);sound=audioField.Of(object);
        void* cls=R::ClassOf(object);
        if(!R::FindBoolProperty(cls,L"active",active,activeMask) || active<0 || !activeMask ||
           !R::FindBoolProperty(cls,L"hasLid",lid,lidMask) || lid<0 || !lidMask ||
           coals<0 || burn<0 || sound<0 || !actor.Get() || world!=world_identity::Generation())return false;
        void* component=nullptr;std::memcpy(&component,static_cast<uint8_t*>(object)+sound,sizeof(component));
        audio.Set(component);if(!audio.Get())return false;
        volume=volumeField.Of(component);return volume>=0 && Current();
    }
};
template<class T>T Value(void* object,int offset) {T value{};std::memcpy(&value,static_cast<uint8_t*>(object)+offset,sizeof(value));return value;}
template<class T>void Put(void* object,int offset,T value) {std::memcpy(static_cast<uint8_t*>(object)+offset,&value,sizeof(value));}
void Bit(void* object,int offset,uint8_t mask,bool value) {
    auto& bits=static_cast<uint8_t*>(object)[offset];bits=value?bits|mask:bits&~mask;
}
bool Playing(const Fields& fields,bool& playing) {
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(fields.audio.Get()),L"IsPlaying"));
    return fields.Current() && frame.valid() && Call(fields.audio.Get(),frame) && fields.Current() &&
        frame.GetRaw(L"ReturnValue",&playing,sizeof(playing));
}
}
bool IsGrill(void* actor) {
    if(!actor || !R::IsLive(actor))return false;
    void* cls=object_index::ClassByName(L"prop_bbq_C");
    return cls && R::IsDescendantOfAny(R::ClassOf(actor),&cls,1);
}
namespace {
struct StockFields {
    CachedObjRef actor,cls;
    uint32_t world=0;
    int coals=-1,lid=-1;uint8_t mask=0;
    bool Current()const{return actor.Get() && cls.Get() && R::ClassOf(actor.Get())==cls.Get() && world==world_identity::Generation();}
    bool Resolve(void* object){
        if(!IsGrill(object))return false;
        actor.Set(object);cls.Set(R::ClassOf(object));world=world_identity::Generation();
        static R::InstanceOffset field{L"coals"};coals=field.Of(object);
        return coals>=0 && Current() && R::FindBoolProperty(cls.Get(),L"hasLid",lid,mask) && lid>=0 && mask && Current();
    }
};
}
bool ReadStock(void* actor,Stock& stock,const std::function<bool()>& current){
    if(current && !current())return false;
    StockFields fields;if(!fields.Resolve(actor) || (current && !current()))return false;
    Stock value{Value<int32_t>(actor,fields.coals),bool(Value<uint8_t>(actor,fields.lid)&fields.mask)};
    if(value.coals<0 || value.coals>16 || !fields.Current() || (current && !current()))return false;
    stock=value;return true;
}
Edit ChangeStock(void* actor,uint8_t operation,int32_t value,const std::function<bool()>& current){
    if(!current || !current() || (operation!=1 && operation!=2) || value<0 || value>(operation==1?16:1))return Edit::Retry;
    StockFields fields;if(!fields.Resolve(actor) || !current())return Edit::Retry;
    if(operation==1){
        // Reflection may re-enter another edit before this field is resolved.
        // Re-sample on the next drain instead of losing that newer increment.
        if(Value<int32_t>(actor,fields.coals)!=value-1)return Edit::Retry;
        Put(actor,fields.coals,value);return Edit::Applied;
    }
    Bit(actor,fields.lid,fields.mask,value!=0);
    // A write followed by interrupted native painting is already an attempt.
    // Its caller must send the outcome, rather than apply the operation again.
    if(!fields.Current() || !current())return Edit::Interrupted;
    const bool called=component_calls::CallParamlessNamed(actor,L"updLid");
    return called && fields.Current() && current()?Edit::Applied:Edit::Interrupted;
}
bool Valid(const State& state) {
    return state.coals>=0 && state.coals<=16 && std::isfinite(state.burn) && state.burn>=0 && state.burn<=1.01f &&
        std::isfinite(state.volume) && state.volume>=0 && state.volume<=1.f;
}
bool Read(void* actor,State& out) {
    Fields fields;if(!fields.Resolve(actor))return false;
    State value{};
    value.active=(Value<uint8_t>(actor,fields.active)&fields.activeMask)!=0;
    value.lid=(Value<uint8_t>(actor,fields.lid)&fields.lidMask)!=0;
    value.coals=Value<int32_t>(actor,fields.coals);value.burn=Value<float>(actor,fields.burn);
    value.volume=Value<float>(fields.audio.Get(),fields.volume);
    if(!Valid(value) || !Playing(fields,value.cooking))return false;
    out=value;return true;
}
bool Apply(void* actor,const State& state,const std::function<bool()>& current) {
    const auto valid=[&]{return !current || current();};
    if(!Valid(state) || !valid())return false;
    Fields fields;if(!fields.Resolve(actor))return false;
    const auto live=[&]{return valid() && fields.Current();};
    bool playing=false;if(!Playing(fields,playing) || !live())return false;
    Bit(actor,fields.active,fields.activeMask,state.active);Bit(actor,fields.lid,fields.lidMask,state.lid);
    Put(actor,fields.coals,state.coals);Put(actor,fields.burn,state.burn);
    // updLid changes only visibility/collision. upd paints fire/materials and
    // heating-volume collision; ReceiveTick remains the sole native heat/fuel writer.
    if(!component_calls::CallParamlessNamed(actor,L"updLid") || !live() ||
       !component_calls::CallParamlessNamed(actor,L"upd") || !live() ||
       !component_calls::SetVolumeMultiplier(fields.audio.Get(),state.volume) || !live())return false;
    return component_calls::SetAudioPlaying(fields.audio.Get(),state.cooking,live);
}
bool StopSound(void* actor,const std::function<bool()>& current) {
    if((current && !current()) || !IsGrill(actor))return false;
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();
    static R::InstanceOffset soundField{L"cookingLoop"};const auto sound=soundField.Of(actor);
    if(sound<0 || !live.Get() || world!=world_identity::Generation())return false;
    void* component=nullptr;std::memcpy(&component,static_cast<uint8_t*>(actor)+sound,sizeof(component));
    CachedObjRef audio;audio.Set(component);
    const auto valid=[&]{
        if(!live.Get() || !audio.Get() || world!=world_identity::Generation() || (current && !current()))return false;
        void* assigned=nullptr;std::memcpy(&assigned,static_cast<uint8_t*>(actor)+sound,sizeof(assigned));return assigned==audio.Raw();
    };
    return valid() && component_calls::SetAudioPlaying(audio.Get(),false,valid);
}
}
