#include "ue_wrap/actors/llama_timer.h"
#include "ue_wrap/actors/llama.h"
#include "ue_wrap/core/call.h"
#include <algorithm>
#include <cstring>
#include <initializer_list>
namespace ue_wrap::llama_timer {
namespace {
namespace R=reflection;
bool Typed(void* object,const wchar_t* name){if(!object||!R::IsLive(object))return false;void* cls=R::ClassOf(object);
    for(int i=0;cls&&i<64;++i,cls=R::SuperStructOf(cls)){if(!R::IsLive(cls))return false;if(R::NameEquals(R::NameOf(cls),name))return true;}return false;
}
bool Field(const Lease& lease,int& offset,CachedObjRef& type){
    if(!lease.world||lease.world!=world_identity::CurrentWorld()||lease.generation!=world_identity::Generation()||!lease.actor.Get()||!lease.cls.Get()||
        R::ClassOf(lease.actor.Get())!=lease.cls.Get()||world_identity::WorldOf(lease.actor.Get())!=lease.world)return false;
    std::wstring kind;int32_t actual=-1;if(!R::PropertyOffsetAndClass(lease.cls.Get(),L"timer",actual,kind)||kind!=L"StructProperty")return false;
    const int size=R::StructSize(lease.cls.Get());if(actual<0||size<8||actual>size-8)return false;
    CachedObjRef inner;inner.Set(R::PropertyInnerStruct(lease.cls.Get(),L"timer"));
    if(!inner.Get()||!R::NameEquals(R::NameOf(inner.Get()),L"TimerHandle")||R::StructSize(inner.Get())!=8||!lease.actor.Get()||!lease.cls.Get()||R::ClassOf(lease.actor.Get())!=lease.cls.Get())return false;
    offset=actual;type=inner;return true;
}
bool Capture(void* actor,Lease& out){
    if(!llama::IsLlama(actor))return false;Lease lease;lease.actor.Set(actor);lease.cls.Set(R::ClassOf(actor));lease.world=world_identity::CurrentWorld();lease.generation=world_identity::Generation();
    if(!Field(lease,lease.offset,lease.type))return false;std::memcpy(&lease.handle,static_cast<uint8_t*>(lease.actor.Get())+lease.offset,8);
    if(!lease.handle)return false;out=lease;return true;
}
struct Function {
    CachedObjRef owner,cls,fn;const wchar_t* name=nullptr;
    Function(void* object,const wchar_t* verb):name(verb){if(object&&R::IsLive(object)){owner.Set(object);cls.Set(R::ClassOf(object));if(cls.Get())fn.Set(R::FindDispatchFunctionCached(cls.Get(),name));}}
    bool Current()const{return owner.Get()&&cls.Get()&&fn.Get()&&R::ClassOf(owner.Get())==cls.Get()&&R::FindDispatchFunctionCached(cls.Get(),name)==fn.Get()&&owner.Get()&&cls.Get()&&fn.Get();}
};
void* Library(){
    // KSL outlives worlds. Do not walk the entire object array on every timer reconciliation.
    static CachedObjRef object,cls;
    if(object.Get()&&cls.Get()&&R::ClassOf(object.Get())==cls.Get()&&Typed(object.Get(),L"KismetSystemLibrary"))return object.Get();
    object.Reset();cls.Reset();void* found=R::FindClassDefaultObject(L"KismetSystemLibrary");
    if(!Typed(found,L"KismetSystemLibrary"))return nullptr;object.Set(found);cls.Set(R::ClassOf(found));
    return object.Get()&&cls.Get()&&R::ClassOf(object.Get())==cls.Get()?object.Get():nullptr;
}
struct Ops {
    void* cdo=Library();
    Function exists{cdo,L"K2_TimerExistsHandle"},active{cdo,L"K2_IsTimerActiveHandle"},paused{cdo,L"K2_IsTimerPausedHandle"},pause{cdo,L"K2_PauseTimerHandle"},resume{cdo,L"K2_UnPauseTimerHandle"};
    bool Current()const{return exists.Current()&&active.Current()&&paused.Current()&&pause.Current()&&resume.Current()&&Typed(exists.owner.Get(),L"KismetSystemLibrary");}
};
struct Slot{const wchar_t* name;int size;};
bool Slots(const ParamFrame& frame,std::initializer_list<Slot> slots){
    if(!frame.valid())return false;const auto params=R::FunctionParams(frame.function());std::vector<std::pair<int,int>> spans;
    for(const auto& slot:slots){const int offset=frame.ParamOffset(slot.name),size=frame.FrameSize();
        if(offset<0||slot.size<=0||size<slot.size||offset>size-slot.size||std::count_if(params.begin(),params.end(),[&](const auto& p){return !_wcsicmp(p.name.c_str(),slot.name)&&p.offset==offset&&p.size==slot.size;})!=1)return false;
        for(const auto& other:spans)if(offset<other.second&&other.first<offset+slot.size)return false;spans.emplace_back(offset,offset+slot.size);
    }return true;
}
bool Inputs(ParamFrame& frame,const Lease& lease,bool query){
    return (query?Slots(frame,{{L"WorldContextObject",8},{L"Handle",8},{L"ReturnValue",1}}):Slots(frame,{{L"WorldContextObject",8},{L"Handle",8}}))&&
        frame.Set<void*>(L"WorldContextObject",lease.actor.Get())&&frame.Set<uint64_t>(L"Handle",lease.handle);
}
bool Query(const Ops& ops,const Function& function,const Lease& lease,bool& out,const std::function<bool()>& current){
    if(!ops.Current()||!Current(lease)||!current||!current())return false;ParamFrame frame(function.fn.Get());uint8_t value=0;
    if(!Inputs(frame,lease,true)||!ops.Current()||!Current(lease)||!current()||!ops.Current()||!Current(lease)||!Call(function.owner.Get(),frame)||
        !ops.Current()||!Current(lease)||!current()||!ops.Current()||!Current(lease)||!frame.GetRaw(L"ReturnValue",&value,1)||value>1)return false;out=value!=0;return true;
}
bool Read(const Ops& ops,const Lease& lease,State& out,const std::function<bool()>& current){
    State state;state.lease=lease;
    if(!Query(ops,ops.exists,lease,state.exists,current))return false;
    if(state.exists&&(!Query(ops,ops.active,lease,state.active,current)||!Query(ops,ops.paused,lease,state.paused,current)))return false;
    if(state.active&&state.paused)return false;if(!Current(lease)||!current()||!Current(lease)||!ops.Current())return false;out=state;return true;
}
bool Preflight(const Ops& ops,const Lease& lease){
    if(!ops.Current())return false;
    // Include the restore verb before admitting a pause; unsupported metadata leaves the timer alone.
    ParamFrame pause(ops.pause.fn.Get()),resume(ops.resume.fn.Get()),exists(ops.exists.fn.Get()),active(ops.active.fn.Get()),paused(ops.paused.fn.Get());
    return Inputs(pause,lease,false)&&Inputs(resume,lease,false)&&Inputs(exists,lease,true)&&Inputs(active,lease,true)&&Inputs(paused,lease,true)&&ops.Current()&&Current(lease);
}
}
bool Current(const Lease& lease){
    int offset=-1;CachedObjRef type;if(!lease.handle||!Field(lease,offset,type)||offset!=lease.offset||!lease.type.Is(type.Get()))return false;
    uint64_t handle=0;std::memcpy(&handle,static_cast<uint8_t*>(lease.actor.Get())+offset,8);
    return handle==lease.handle&&lease.actor.Get()&&lease.cls.Get()&&R::ClassOf(lease.actor.Get())==lease.cls.Get()&&lease.type.Get()&&
        lease.world==world_identity::CurrentWorld()&&lease.generation==world_identity::Generation();
}
bool Retired(const Lease& lease){
    void* world=world_identity::CurrentWorld();
    if(!lease.handle||lease.generation!=world_identity::Generation()||(world&&world!=lease.world)||
        !lease.actor.Is(lease.actor.Raw())||!lease.cls.Is(lease.cls.Raw())||!lease.type.Is(lease.type.Raw())||R::ClassOf(lease.actor.Raw())!=lease.cls.Raw())return true;
    void* sourceWorld=world_identity::WorldOf(lease.actor.Raw());if(sourceWorld&&sourceWorld!=lease.world)return true;
    if(!world||!sourceWorld)return false;
    int offset=-1;CachedObjRef type;if(!Field(lease,offset,type))return false;
    if(offset!=lease.offset||!lease.type.Is(type.Get()))return true;
    uint64_t handle=0;std::memcpy(&handle,static_cast<uint8_t*>(lease.actor.Get())+offset,8);return handle!=lease.handle;
}
bool Inspect(void* actor,State& out,const std::function<bool()>& current){
    if(!current||!current())return false;Lease lease;if(!Capture(actor,lease)||!Current(lease))return false;const Ops ops;
    State state;if(!Preflight(ops,lease)||!Read(ops,lease,state,current)||!Current(lease))return false;out=state;return true;
}
Result Pause(const Lease& lease,bool& dispatched,const std::function<bool()>& current){
    if(!Current(lease))return Retired(lease)?Result::Retired:Result::Retry;const Ops ops;State state;
    if(!Preflight(ops,lease)||!Read(ops,lease,state,current))return Retired(lease)?Result::Retired:Result::Retry;
    if(!state.exists)return Result::Retired;if(state.paused)return Result::Done;if(!state.active)return Result::Retry;
    ParamFrame frame(ops.pause.fn.Get());if(!Inputs(frame,lease,false)||!ops.Current()||!Current(lease)||!current()||!ops.Current()||!Current(lease))return Result::Retry;
    dispatched=true;const bool called=Call(ops.pause.owner.Get(),frame);
    if(!Current(lease))return Retired(lease)?Result::Retired:Result::Retry;
    State after;if(!Read(ops,lease,after,current))return Result::Retry;
    // A positive paused query settles even an uncertain native return; dispatch remains owned either way.
    if(after.exists&&after.paused)return Result::Done;
    (void)called;return after.exists?Result::Retry:Result::Retired;
}
Result Resume(const Lease& lease,const std::function<bool()>& current){
    if(!Current(lease))return Retired(lease)?Result::Retired:Result::Retry;const Ops ops;State state;
    if(!Preflight(ops,lease)||!Read(ops,lease,state,current))return Retired(lease)?Result::Retired:Result::Retry;
    if(!state.exists||(!state.paused&&state.active))return Result::Done;
    if(!state.paused)return Result::Retry;
    ParamFrame frame(ops.resume.fn.Get());if(!Inputs(frame,lease,false)||!ops.Current()||!Current(lease)||!current()||!ops.Current()||!Current(lease))return Result::Retry;
    Call(ops.resume.owner.Get(),frame);if(!Current(lease))return Retired(lease)?Result::Retired:Result::Retry;
    State after;if(!Read(ops,lease,after,current))return Result::Retry;
    return !after.exists||(!after.paused&&after.active)?Result::Done:Result::Retry;
}
}
