#include "ue_wrap/actors/llama_begin_restore.h"
#include "ue_wrap/actors/llama.h"
#include "ue_wrap/core/call.h"
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <vector>
namespace ue_wrap::llama_begin_restore {
namespace {
namespace R=reflection;
bool Typed(void* object,const wchar_t* name){if(!object||!R::IsLive(object))return false;void* cls=R::ClassOf(object);
    for(int i=0;cls&&i<64;++i,cls=R::SuperStructOf(cls)){if(!R::IsLive(cls))return false;if(R::NameEquals(R::NameOf(cls),name))return true;}return false;}
bool Identity(const Snapshot& s){return s.world&&s.world==world_identity::CurrentWorld()&&s.generation==world_identity::Generation()&&s.actor.Get()&&s.cls.Get()&&
    R::ClassOf(s.actor.Get())==s.cls.Get()&&world_identity::WorldOf(s.actor.Get())==s.world;}
bool Offset(const Snapshot& s,const wchar_t* name,const wchar_t* type,int bytes,int& out){
    if(!Identity(s))return false;std::wstring kind;int32_t offset=-1;
    if(!R::PropertyOffsetAndClass(s.cls.Get(),name,offset,kind)||kind!=type)return false;const int size=R::StructSize(s.cls.Get());
    if(offset<0||bytes<=0||size<bytes||offset>size-bytes||!Identity(s))return false;out=offset;return true;
}
bool Bindings(const Snapshot& s){
    int sphere=-1,timer=-1;if(!Offset(s,L"Sphere",L"ObjectProperty",8,sphere)||sphere!=s.sphereOffset||!Offset(s,L"timer",L"StructProperty",8,timer)||timer!=s.timerOffset)return false;
    CachedObjRef type;type.Set(R::PropertyInnerStruct(s.cls.Get(),L"timer"));
    if(!type.Get()||!s.timerType.Is(type.Get())||!R::NameEquals(R::NameOf(type.Get()),L"TimerHandle")||R::StructSize(type.Get())!=8||!Identity(s))return false;
    void* component=nullptr;std::memcpy(&component,static_cast<uint8_t*>(s.actor.Get())+sphere,8);
    return s.sphere.Is(component)&&s.sphereClass.Get()&&R::ClassOf(component)==s.sphereClass.Get()&&Typed(component,L"SphereComponent")&&
        world_identity::WorldOf(component)==s.world&&Identity(s);
}
bool Handle(const Snapshot& s,uint64_t& out){if(!Bindings(s))return false;uint64_t handle=0;std::memcpy(&handle,static_cast<uint8_t*>(s.actor.Get())+s.timerOffset,8);
    if(!Bindings(s))return false;out=handle;return true;}
bool Bit(const Snapshot& s,const wchar_t* name,bool& out){int offset=-1;uint8_t mask=0;
    if(!Identity(s)||!R::FindBoolProperty(s.cls.Get(),name,offset,mask)||offset<0||!mask||offset>=R::StructSize(s.cls.Get())||!Identity(s))return false;
    const bool value=(static_cast<uint8_t*>(s.actor.Get())[offset]&mask)!=0;if(!Identity(s))return false;out=value;return true;}
struct Function {
    CachedObjRef object,cls,fn;const wchar_t* name;
    Function(void* owner,const wchar_t* verb):name(verb){if(owner&&R::IsLive(owner)){object.Set(owner);cls.Set(R::ClassOf(owner));if(cls.Get())fn.Set(R::FindDispatchFunctionCached(cls.Get(),name));}}
    bool Current()const{return object.Get()&&cls.Get()&&fn.Get()&&R::ClassOf(object.Get())==cls.Get()&&R::FindDispatchFunctionCached(cls.Get(),name)==fn.Get()&&object.Get()&&cls.Get()&&fn.Get();}
};
void* Library(){static CachedObjRef object,cls;
    if(object.Get()&&cls.Get()&&R::ClassOf(object.Get())==cls.Get()&&Typed(object.Get(),L"KismetSystemLibrary"))return object.Get();
    object.Reset();cls.Reset();void* found=R::FindClassDefaultObject(L"KismetSystemLibrary");if(!Typed(found,L"KismetSystemLibrary"))return nullptr;
    object.Set(found);cls.Set(R::ClassOf(found));return object.Get()&&cls.Get()&&R::ClassOf(object.Get())==cls.Get()?object.Get():nullptr;
}
struct Slot {const wchar_t* name;int bytes;};
bool Slots(const ParamFrame& frame,std::initializer_list<Slot> fields){
    if(!frame.valid())return false;const auto params=R::FunctionParams(frame.function());std::vector<std::pair<int,int>> spans;
    for(const auto& field:fields){const int offset=frame.ParamOffset(field.name),size=frame.FrameSize();
        if(offset<0||field.bytes<=0||size<field.bytes||offset>size-field.bytes||std::count_if(params.begin(),params.end(),[&](const auto& p){return !_wcsicmp(p.name.c_str(),field.name)&&p.offset==offset&&p.size==field.bytes;})!=1)return false;
        for(const auto& other:spans)if(offset<other.second&&other.first<offset+field.bytes)return false;spans.emplace_back(offset,offset+field.bytes);
    }return true;
}
bool Collision(const Snapshot& s,uint8_t& out,const std::function<bool()>& current){
    const Function query(s.sphere.Get(),L"GetCollisionEnabled");if(!query.Current())return false;ParamFrame frame(query.fn.Get());uint8_t value=255;
    if(!Slots(frame,{{L"ReturnValue",1}})||!query.Current()||!Current(s)||!current||!current()||!Current(s)||!query.Current()||!Call(query.object.Get(),frame)||
        !query.Current()||!Current(s)||!current()||!Current(s)||!query.Current()||!frame.GetRaw(L"ReturnValue",&value,1)||value>3)return false;out=value;return true;
}
Result Status(const Snapshot& s){return Retired(s)?Result::Retired:Result::Retry;}
}
bool Current(const Snapshot& s){return Bindings(s);}
bool Retired(const Snapshot& s){const auto world=world_identity::CurrentWorld();
    if(s.generation!=world_identity::Generation()||(world&&world!=s.world)||!s.actor.Is(s.actor.Raw())||!s.cls.Is(s.cls.Raw())||!s.sphere.Is(s.sphere.Raw())||
        !s.sphereClass.Is(s.sphereClass.Raw())||!s.timerType.Is(s.timerType.Raw())||R::ClassOf(s.actor.Raw())!=s.cls.Raw()||R::ClassOf(s.sphere.Raw())!=s.sphereClass.Raw())return true;
    const auto sourceWorld=world_identity::WorldOf(s.actor.Raw()),sphereWorld=world_identity::WorldOf(s.sphere.Raw());
    if((sourceWorld&&sourceWorld!=s.world)||(sphereWorld&&sphereWorld!=s.world))return true;
    if(!world)return false;int sphere=-1,timer=-1;
    if(!Offset(s,L"Sphere",L"ObjectProperty",8,sphere)||!Offset(s,L"timer",L"StructProperty",8,timer))return false;
    if(sphere!=s.sphereOffset||timer!=s.timerOffset)return true;void* component=nullptr;std::memcpy(&component,static_cast<uint8_t*>(s.actor.Get())+sphere,8);
    if(!s.sphere.Is(component))return true;CachedObjRef type;type.Set(R::PropertyInnerStruct(s.cls.Get(),L"timer"));return type.Get()&&!s.timerType.Is(type.Get());
}
bool Capture(void* actor,Snapshot& out,const std::function<bool()>& current){
    if(!current||!current()||!llama::IsLlama(actor))return false;Snapshot s;s.actor.Set(actor);s.cls.Set(R::ClassOf(actor));s.world=world_identity::CurrentWorld();s.generation=world_identity::Generation();
    if(!Offset(s,L"Sphere",L"ObjectProperty",8,s.sphereOffset)||!Offset(s,L"timer",L"StructProperty",8,s.timerOffset))return false;
    void* component=nullptr;std::memcpy(&component,static_cast<uint8_t*>(s.actor.Get())+s.sphereOffset,8);if(!Typed(component,L"SphereComponent"))return false;
    s.sphere.Set(component);s.sphereClass.Set(R::ClassOf(component));s.timerType.Set(R::PropertyInnerStruct(s.cls.Get(),L"timer"));
    uint64_t handle=0;bool activeState=false;
    if(!Handle(s,handle)||handle||!Bit(s,L"h",activeState)||activeState||!Collision(s,s.collision,current)||!current()||!Current(s)||!Handle(s,handle)||handle||
        !Bit(s,L"h",activeState)||activeState)return false;out=s;return true;
}
Result Restore(const Snapshot& s,bool confirmed,bool& attempted,const std::function<bool()>& current){
    if(!Current(s))return Status(s);if(!current||!current()||!Current(s))return Status(s);
    const Function set(s.sphere.Get(),L"SetCollisionEnabled"),graph(s.actor.Get(),L"ExecuteUbergraph_prop_llama"),exists(Library(),L"K2_TimerExistsHandle"),active(exists.object.Get(),L"K2_IsTimerActiveHandle");
    if(!set.Current()||!graph.Current()||!exists.Current()||!active.Current())return Result::Retry;
    ParamFrame collision(set.fn.Get()),body(graph.fn.Get()),has(exists.fn.Get()),running(active.fn.Get());
    if(!Slots(collision,{{L"NewType",1}})||!collision.Set(L"NewType",s.collision)||!Slots(body,{{L"EntryPoint",4}})||!body.Set<int32_t>(L"EntryPoint",108)||
        !Slots(has,{{L"WorldContextObject",8},{L"Handle",8},{L"ReturnValue",1}})||!Slots(running,{{L"WorldContextObject",8},{L"Handle",8},{L"ReturnValue",1}}))return Result::Retry;
    const auto valid=[&]{return Current(s)&&current()&&Current(s);};uint8_t actual=255;
    if(!Collision(s,actual,valid))return Status(s);
    // Only our disabled state authorizes restoring the old collision mode.
    if(actual==0&&actual!=s.collision){
        if(!valid()||!set.Current()||!Call(set.object.Get(),collision)||!valid()||!set.Current()){
            if(!Current(s))return Status(s);if(!Collision(s,actual,valid)||actual!=s.collision)return Result::Retry;
        }else if(!Collision(s,actual,valid)||actual!=s.collision)return Result::Retry;
    }
    if(!confirmed||attempted)return Result::Done;
    uint64_t handle=0;bool enabled=false,activeState=false;if(!Handle(s,handle))return Status(s);if(handle)return Result::Done;
    if(!Bit(s,L"yumpskare",enabled)||!enabled||!Bit(s,L"h",activeState)||activeState||!valid()||!graph.Current()||!Handle(s,handle)||handle||
        !Bit(s,L"yumpskare",enabled)||!enabled||!Bit(s,L"h",activeState)||activeState||!Current(s))return handle?Result::Done:Result::Retry;
    // Irreversible creation: never replay an admitted graph after an unknown return.
    attempted=true;Call(graph.object.Get(),body);
    if(!Current(s)||!valid())return Status(s);if(!Handle(s,handle))return Status(s);
    if(!handle)return Result::Done;
    if(!has.Set<void*>(L"WorldContextObject",s.actor.Get())||!has.Set(L"Handle",handle)||!running.Set<void*>(L"WorldContextObject",s.actor.Get())||!running.Set(L"Handle",handle)||
        !exists.Current()||!active.Current()||!valid())return Result::Done;
    // Read back only; an unknown creation cannot be safely repeated.
    uint8_t present=0,started=0;
    const bool observed=Call(exists.object.Get(),has)&&exists.Current()&&valid()&&has.GetRaw(L"ReturnValue",&present,1)&&present==1&&
        Call(active.object.Get(),running)&&active.Current()&&valid()&&running.GetRaw(L"ReturnValue",&started,1)&&started<=1;
    (void)observed;return Current(s)?Result::Done:Status(s);
}
}
