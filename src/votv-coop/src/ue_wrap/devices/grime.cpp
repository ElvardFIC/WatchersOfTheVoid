#include "ue_wrap/devices/grime.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/engine/engine_component.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::grime {
namespace {
namespace R=reflection;
struct Layout {
    CachedObjRef cls,apply;
    int process=-1,type=-1,material=-1,parameter=-1;
} g_layout;
template<class T>T Read(void* object,int offset){T value{};std::memcpy(&value,static_cast<uint8_t*>(object)+offset,sizeof(value));return value;}
struct Identity {
    CachedObjRef actor;
    uint32_t world=world_identity::Generation();
    explicit Identity(void* object){actor.Set(object);}
    bool Current()const{return actor.Get() && world==world_identity::Generation();}
};
}
bool EnsureResolved(){
    void* cls=object_index::ClassByName(L"grime_C");
    if(!cls){g_layout={};return false;}
    if(g_layout.cls.Is(cls) && g_layout.process>=0 && g_layout.type>=0 &&
       g_layout.material>=0 && g_layout.parameter>=0 && g_layout.apply.Get())return true;
    Layout fields;fields.cls.Set(cls);
    fields.process=R::FindPropertyOffset(cls,L"process");fields.type=R::FindPropertyOffset(cls,L"Type");
    fields.material=R::FindPropertyOffset(cls,L"dynmat");fields.parameter=R::FindPropertyOffset(cls,L"cleanParameter");
    fields.apply.Set(R::FindFunction(cls,L"applyMaterial"));
    if(!fields.cls.Get())return false;g_layout=fields;
    return fields.process>=0 && fields.type>=0;
}
bool IsGrime(void* actor){
    if(!actor || !EnsureResolved())return false;
    void* base=g_layout.cls.Get();return base && R::IsDescendantOfAny(R::ClassOf(actor),&base,1);
}
bool ReadProcess(void* actor,float& out){
    if(!actor || !R::IsLive(actor))return false;Identity live(actor);if(!IsGrime(actor) || !live.Current())return false;
    const float value=Read<float>(actor,g_layout.process);if(!std::isfinite(value))return false;out=value;return true;
}
bool ReadType(void* actor,int32_t& out){
    if(!actor || !R::IsLive(actor))return false;Identity live(actor);if(!IsGrime(actor) || !live.Current())return false;
    out=Read<int32_t>(actor,g_layout.type);return true;
}
bool ReadEndedProcess(void* actor,int32_t index,int32_t serial,float& out){
    const auto world=world_identity::Generation();
    const auto held=[&]{return actor && index>=0 && serial && R::ObjectAt(index)==actor && R::SlotSerial(index)==serial &&
        world==world_identity::Generation();};
    if(!held())return false;void* actorWorld=world_identity::WorldOf(actor);
    if(actorWorld && actorWorld!=world_identity::CurrentWorld())return false;
    if(!EnsureResolved())return false;void* base=g_layout.cls.Get();
    if(!base || !R::IsDescendantOfAny(R::ClassOf(actor),&base,1) || !held())return false;
    const float value=Read<float>(actor,g_layout.process);if(!std::isfinite(value))return false;out=value;return true;
}
bool WriteProcessAndApply(void* actor,float process,const std::function<bool()>& current){
    if(!actor || !R::IsLive(actor) || !std::isfinite(process) || process<0 || (current && !current()))return false;
    Identity live(actor);if(!IsGrime(actor) || !live.Current())return false;
    const auto fields=g_layout;void* actorClass=R::ClassOf(actor);
    const auto stable=[&]{return live.Current() && fields.cls.Get() && R::ClassOf(actor)==actorClass;};
    const auto valid=[&]{return stable() && (!current || current()) && stable() && Read<float>(actor,fields.process)==process;};
    if(fields.material<0 || fields.parameter<0 || !stable() || (current && !current()) || !stable())return false;
    std::memcpy(static_cast<uint8_t*>(actor)+fields.process,&process,sizeof(process));
    const auto repaint=[&]{
        if(!valid())return false;void* material=Read<void*>(actor,fields.material);if(!material || !R::IsLive(material))return false;
        CachedObjRef cached;cached.Set(material);
        if(!cached.Get())return false;const auto parameter=Read<R::FName>(actor,fields.parameter);
        const auto bound=[&]{if(!valid() || !cached.Get() || Read<void*>(actor,fields.material)!=material)return false;
            const auto now=Read<R::FName>(actor,fields.parameter);return now.ComparisonIndex==parameter.ComparisonIndex && now.Number==parameter.Number;};
        // clean() paints process/100, including subclasses whose maxProcess is50.
        return bound() && engine::SetScalarParameterValue(material,parameter,process/100.f) && bound();
    };
    void* existing=Read<void*>(actor,fields.material);
    if(existing && R::IsLive(existing))return repaint();if(!valid())return false;
    CachedObjRef function;function.Set(R::FindDispatchFunctionCached(actorClass,L"applyMaterial"));
    if(!function.Get() || !valid())return false;ParamFrame frame(function.Get());
    if(!frame.valid() || !Call(actor,frame) || !valid() || !function.Get())return false;
    return repaint();
}
}
