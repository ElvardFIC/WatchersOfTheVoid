#include "ue_wrap/actors/llama.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include <cstring>
namespace ue_wrap::llama {
namespace {
namespace R=reflection;
struct Live {
    CachedObjRef object,cls;uint32_t generation=world_identity::Generation();void* world=world_identity::CurrentWorld();
    explicit Live(void* p=nullptr){if(p&&R::IsLive(p)){object.Set(p);cls.Set(R::ClassOf(p));}}
    bool Current()const{return world&&world==world_identity::CurrentWorld()&&generation==world_identity::Generation()&&object.Get()&&cls.Get()&&R::ClassOf(object.Get())==cls.Get();}
};
bool Typed(const Live& object,const wchar_t* name){
    if(!object.Current())return false;void* cls=object.cls.Get();
    for(int i=0;cls&&i<64;++i,cls=R::SuperStructOf(cls)){if(!R::IsLive(cls))return false;if(R::NameEquals(R::NameOf(cls),name))return object.Current();}
    return false;
}
struct Bit {
    Live owner;const wchar_t* name;int32_t offset=-1;uint8_t mask=0;
    Bit(const Live& object,const wchar_t* property):owner(object),name(property){if(owner.Current())R::FindBoolProperty(owner.cls.Get(),name,offset,mask);}
    bool Current()const{int32_t actual=-1;uint8_t bits=0;return offset>=0&&mask&&owner.Current()&&
        R::FindBoolProperty(owner.cls.Get(),name,actual,bits)&&actual==offset&&bits==mask&&owner.Current();}
    bool Read(bool& out)const{if(!Current())return false;const bool value=(static_cast<uint8_t*>(owner.object.Get())[offset]&mask)!=0;
        if(!Current())return false;out=value;return true;}
    bool Write(bool value)const{if(!Current())return false;auto& byte=static_cast<uint8_t*>(owner.object.Get())[offset];
        byte=value?uint8_t(byte|mask):uint8_t(byte&~mask);return Current();}
};
struct Component {
    Live actor,component;const wchar_t* field;int offset=-1;
    Component(const Live& owner,const wchar_t* name,const wchar_t* type):actor(owner),field(name){
        if(!actor.Current())return;offset=R::FindPropertyOffset(actor.cls.Get(),field);
        if(offset<0||!actor.Current())return;void* p=nullptr;std::memcpy(&p,static_cast<uint8_t*>(actor.object.Get())+offset,sizeof(p));
        component=Live(p);if(!Typed(component,type))component=Live();
    }
    bool Current()const{if(offset<0||!actor.Current()||!component.Current()||R::FindPropertyOffset(actor.cls.Get(),field)!=offset||!actor.Current())return false;
        void* p=nullptr;std::memcpy(&p,static_cast<uint8_t*>(actor.object.Get())+offset,sizeof(p));return p==component.object.Raw()&&component.Current();}
};
struct Function {
    Live owner;CachedObjRef fn;const wchar_t* name;
    Function(const Live& object,const wchar_t* verb):owner(object),name(verb){if(owner.Current())fn.Set(R::FindDispatchFunctionCached(owner.cls.Get(),verb));}
    bool Current()const{return owner.Current()&&fn.Get()&&R::FindDispatchFunctionCached(owner.cls.Get(),name)==fn.Get()&&owner.Current()&&fn.Get();}
};
bool Slot(const ParamFrame& frame,const wchar_t* name,int bytes){
    const int offset=frame.ParamOffset(name),size=frame.FrameSize();
    if(!frame.valid()||offset<0||bytes<=0||size<bytes||offset>size-bytes)return false;
    for(const auto& p:R::FunctionParams(frame.function()))if(!_wcsicmp(p.name.c_str(),name))return p.offset==offset&&p.size==bytes;
    return false;
}
}
bool IsLlama(void* actor){return Typed(Live(actor),L"prop_llama_C");}
size_t Collect(CachedObjRef* out,size_t capacity){
    if(!out||!capacity)return 0;void* world=world_identity::CurrentWorld();const auto generation=world_identity::Generation();
    CachedObjRef cls;cls.Set(object_index::ClassByName(L"prop_llama_C"));if(!world||!cls.Get())return 0;
    struct Context{CachedObjRef* out;size_t cap,count=0;void* world;uint32_t generation;};Context c{out,capacity,0,world,generation};
    object_index::ForEachInstance(cls.Get(),[](void* p,void* actor,int32_t){auto& c=*static_cast<Context*>(p);
        if(c.count>=c.cap||c.world!=world_identity::CurrentWorld()||c.generation!=world_identity::Generation()||!IsLlama(actor)||world_identity::WorldOf(actor)!=c.world)return;
        for(size_t i=0;i<c.count;++i)if(c.out[i].Is(actor))return;c.out[c.count++].Set(actor);
    },&c);
    if(world!=world_identity::CurrentWorld()||generation!=world_identity::Generation()||!cls.Get()){for(size_t i=0;i<c.count;++i)out[i].Reset();return 0;}
    return c.count;
}
bool Read(void* actor,bool& active){const Live source(actor);bool value=false;
    if(!IsLlama(actor)||!Bit(source,L"h").Read(value)||!source.Current())return false;active=value;return true;}
bool Entry(void* function,const uint8_t* parameters,int32_t& out){
    CachedObjRef fn;if(!function||!parameters||!R::IsLive(function))return false;fn.Set(function);
    const int offset=R::FindParamOffset(function,L"EntryPoint"),size=R::FunctionFrameSize(function);
    if(!fn.Get()||offset<0||size<4||size>65536||offset>size-4)return false;
    bool found=false;for(const auto& p:R::FunctionParams(function))if(!_wcsicmp(p.name.c_str(),L"EntryPoint"))found=p.offset==offset&&p.size==4;
    if(!found||!fn.Get())return false;int32_t value=0;std::memcpy(&value,parameters+offset,4);if(!fn.Get())return false;out=value;return true;
}
bool ParkBegin(void* actor,bool& original,const std::function<bool()>& current){
    const Live source(actor);const Bit setting(source,L"yumpskare");
    bool value=false;
    if(!IsLlama(actor)||!setting.Read(value)||!source.Current()||(current&&!current())||!source.Current()||!setting.Write(false)||!source.Current())return false;
    original=value;return true;
}
bool RestoreBegin(void* actor,bool original,const std::function<bool()>& current){
    const Live source(actor);const Bit setting(source,L"yumpskare");
    return IsLlama(actor)&&source.Current()&&(!current||current())&&source.Current()&&setting.Write(original)&&source.Current();
}
bool Apply(void* actor,bool active,bool cue,bool& dispatched,const std::function<bool()>& current){
    dispatched=false;const Live source(actor);if((cue&&!active)||!IsLlama(actor)||!source.Current())return false;
    const Component light(source,L"PointLight",L"PointLightComponent");
    const auto valid=[&]{return source.Current()&&light.Current()&&(!current||current())&&source.Current()&&light.Current();};
    if(!valid())return false;Function visibility(light.component,L"SetVisibility");if(!visibility.Current())return false;ParamFrame frame(visibility.fn.Get());
    if(!Slot(frame,L"bNewVisibility",1)||!Slot(frame,L"bPropagateToChildren",1)||!frame.Set<bool>(L"bNewVisibility",active)||
        !frame.Set<bool>(L"bPropagateToChildren",false)||!visibility.Current()||!valid()||!visibility.Current()||
        !Call(light.component.object.Get(),frame)||!visibility.Current()||!valid())return false;
    if(!cue)return true;
    const Component audio(source,L"Audio",L"AudioComponent");Function activate(audio.component,L"Activate");
    if(!audio.Current()||!activate.Current())return false;ParamFrame play(activate.fn.Get());
    if(!Slot(play,L"bReset",1)||!play.Set<bool>(L"bReset",false)||!activate.Current()||!valid()||!audio.Current()||!activate.Current())return false;
    const bool ok=Call(audio.component.object.Get(),play);dispatched=true;
    return ok&&valid()&&audio.Current()&&activate.Current();
}
}
