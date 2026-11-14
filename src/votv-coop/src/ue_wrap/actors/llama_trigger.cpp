#include "ue_wrap/actors/llama_trigger.h"
#include "ue_wrap/actors/llama.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/trace.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>
namespace ue_wrap::llama_trigger {
namespace {
namespace R=reflection;
bool Finite(const FVector& p){return std::isfinite(p.X)&&std::isfinite(p.Y)&&std::isfinite(p.Z);}
struct Live {
    CachedObjRef object,cls;void* world=world_identity::CurrentWorld();uint32_t generation=world_identity::Generation();
    explicit Live(void* p){if(p&&R::IsLive(p)){object.Set(p);cls.Set(R::ClassOf(p));}}
    bool Current()const{return world&&world==world_identity::CurrentWorld()&&generation==world_identity::Generation()&&
        object.Get()&&cls.Get()&&R::ClassOf(object.Get())==cls.Get();}
};
bool Typed(const Live& p,const wchar_t* name){if(!p.Current())return false;void* cls=p.cls.Get();
    for(int i=0;cls&&i<64;++i,cls=R::SuperStructOf(cls)){if(!R::IsLive(cls))return false;if(R::NameEquals(R::NameOf(cls),name))return p.Current();}return false;}
bool Field(const Live& owner,const wchar_t* name,const wchar_t* type,int bytes,int& offset){
    std::wstring actual;int found=-1;if(!owner.Current()||!R::PropertyOffsetAndClass(owner.cls.Get(),name,found,actual)||actual!=type)return false;
    const int size=R::StructSize(owner.cls.Get());if(found<0||bytes<=0||size<bytes||found>size-bytes||!owner.Current())return false;offset=found;return true;
}
template<class T> bool Read(const Live& owner,const wchar_t* name,const wchar_t* type,T& out){
    int offset=-1;if(!Field(owner,name,type,sizeof(T),offset))return false;T value{};
    std::memcpy(&value,static_cast<uint8_t*>(owner.object.Get())+offset,sizeof(value));if(!owner.Current())return false;out=value;return true;
}
bool Bit(const Live& owner,const wchar_t* name,bool& out){int offset=-1;uint8_t mask=0;
    if(!owner.Current()||!R::FindBoolProperty(owner.cls.Get(),name,offset,mask)||offset<0||!mask||offset>=R::StructSize(owner.cls.Get())||!owner.Current())return false;
    const bool value=(static_cast<uint8_t*>(owner.object.Get())[offset]&mask)!=0;if(!owner.Current())return false;out=value;return true;
}
bool Component(const Live& owner,const wchar_t* field,const wchar_t* type,CachedObjRef& object,CachedObjRef& cls){
    void* pointer=nullptr;if(!Read(owner,field,L"ObjectProperty",pointer))return false;const Live part(pointer);
    if(!Typed(part,type)||world_identity::WorldOf(pointer)!=owner.world)return false;object=part.object;cls=part.cls;return owner.Current()&&part.Current();
}
struct Function {
    Live owner;CachedObjRef fn;const wchar_t* name;
    Function(void* object,const wchar_t* verb):owner(object),name(verb){if(owner.Current())fn.Set(R::FindDispatchFunctionCached(owner.cls.Get(),name));}
    bool Current()const{return owner.Current()&&fn.Get()&&R::FindDispatchFunctionCached(owner.cls.Get(),name)==fn.Get()&&owner.Current()&&fn.Get();}
};
struct Slot {const wchar_t* name;int bytes;};
bool Slots(const ParamFrame& frame,std::initializer_list<Slot> fields){
    if(!frame.valid())return false;const auto params=R::FunctionParams(frame.function());std::vector<std::pair<int,int>> spans;
    for(const auto& field:fields){const int offset=frame.ParamOffset(field.name),size=frame.FrameSize();
        if(offset<0||size<field.bytes||offset>size-field.bytes||std::count_if(params.begin(),params.end(),[&](const auto& p){return !_wcsicmp(p.name.c_str(),field.name)&&p.offset==offset&&p.size==field.bytes;})!=1)return false;
        for(const auto& span:spans)if(offset<span.second&&span.first<offset+field.bytes)return false;spans.emplace_back(offset,offset+field.bytes);
    }return true;
}
bool Position(void* object,const wchar_t* name,FVector& out,const std::function<bool()>& current){
    const Function function(object,name);if(!function.Current())return false;ParamFrame frame(function.fn.Get());FVector value{};
    if(!Slots(frame,{{L"ReturnValue",12}})||!function.Current()||!current()||!function.Current()||!Call(object,frame)||!function.Current()||!current()||
        !frame.GetRaw(L"ReturnValue",&value,sizeof(value))||!Finite(value))return false;out=value;return true;
}
bool Timer(const Live& source,uint64_t& out,CachedObjRef* type=nullptr,int* fieldOffset=nullptr){
    int offset=-1;if(!Field(source,L"timer",L"StructProperty",8,offset))return false;CachedObjRef inner;inner.Set(R::PropertyInnerStruct(source.cls.Get(),L"timer"));
    if(!inner.Get()||!R::NameEquals(R::NameOf(inner.Get()),L"TimerHandle")||R::StructSize(inner.Get())!=8||!source.Current()||!inner.Get())return false;
    uint64_t value=0;std::memcpy(&value,static_cast<uint8_t*>(source.object.Get())+offset,8);
    if(!source.Current()||!inner.Get())return false;out=value;if(type)*type=inner;if(fieldOffset)*fieldOffset=offset;return true;
}
bool Current(const Probe& p){
    if(!p.world||p.world!=world_identity::CurrentWorld()||p.generation!=world_identity::Generation()||!p.actor.Get()||!p.cls.Get()||
        R::ClassOf(p.actor.Get())!=p.cls.Get()||world_identity::WorldOf(p.actor.Get())!=p.world)return false;
    const Live source(p.actor.Get());CachedObjRef object,cls;
    if(!Component(source,L"Sphere",L"SphereComponent",object,cls)||!p.sphere.Is(object.Get())||!p.sphereClass.Is(cls.Get())||
        !Component(source,L"Audio",L"AudioComponent",object,cls)||!p.audio.Is(object.Get())||!p.audioClass.Is(cls.Get())||
        !Component(source,L"StaticMesh",L"StaticMeshComponent",object,cls)||!p.mesh.Is(object.Get())||!p.meshClass.Is(cls.Get()))return false;
    CachedObjRef timer;uint64_t handle=0;return Timer(source,handle,&timer)&&p.timerType.Is(timer.Get())&&source.Current();
}
bool Armed(const Probe& p){uint64_t timer=0;bool active=false,setting=false;const Live source(p.actor.Get());
    float radius=0;return Current(p)&&Timer(source,timer)&&timer==p.timer&&timer&&Bit(source,L"h",active)&&!active&&Bit(source,L"yumpskare",setting)&&setting&&
        (!p.radius||(Read(Live(p.sphere.Get()),L"SphereRadius",L"FloatProperty",radius)&&radius==p.radius));}
struct Array {void* data=nullptr;int32_t num=0,max=0;};static_assert(sizeof(Array)==16);
}
bool ReadTimer(void* actor,uint64_t& out){return llama::IsLlama(actor)&&Timer(Live(actor),out);}
bool Capture(void* actor,Probe& out,const std::function<bool()>& current){
    const Live source(actor);if(!current||!current()||!llama::IsLlama(actor)||!source.Current()||world_identity::WorldOf(actor)!=source.world)return false;
    Probe p;p.actor=source.object;p.cls=source.cls;p.world=source.world;p.generation=source.generation;
    if(!Component(source,L"Sphere",L"SphereComponent",p.sphere,p.sphereClass)||!Component(source,L"Audio",L"AudioComponent",p.audio,p.audioClass)||
        !Component(source,L"StaticMesh",L"StaticMeshComponent",p.mesh,p.meshClass)||!Timer(source,p.timer,&p.timerType)||!Armed(p))return false;
    const auto valid=[&]{return current()&&Armed(p);};
    if(!Read(Live(p.sphere.Get()),L"SphereRadius",L"FloatProperty",p.radius)||!std::isfinite(p.radius)||p.radius<=0||p.radius>1e6f)return false;
    const Function active(R::FindClassDefaultObject(L"KismetSystemLibrary"),L"K2_IsTimerActiveHandle");
    const Function physics(p.mesh.Get(),L"IsSimulatingPhysics");if(!active.Current()||!physics.Current())return false;
    ParamFrame timer(active.fn.Get()),simulation(physics.fn.Get());bool armed=false,simulating=false;const R::FName none{};
    if(!Slots(timer,{{L"WorldContextObject",8},{L"Handle",8},{L"ReturnValue",1}})||!timer.Set<void*>(L"WorldContextObject",actor)||!timer.Set(L"Handle",p.timer)||
        !Slots(simulation,{{L"BoneName",8},{L"ReturnValue",1}})||!simulation.Set(L"BoneName",none)||!active.Current()||!valid()||!active.Current()||
        !Call(active.owner.object.Get(),timer)||!active.Current()||!valid()||!timer.GetRaw(L"ReturnValue",&armed,1)||!armed||!physics.Current()||
        !Call(p.mesh.Get(),simulation)||!physics.Current()||!valid()||!simulation.GetRaw(L"ReturnValue",&simulating,1)||!simulating||
        !Position(p.sphere.Get(),L"K2_GetComponentLocation",p.center,valid)||!Position(p.audio.Get(),L"K2_GetComponentLocation",p.origin,valid)||
        !Position(actor,L"K2_GetActorLocation",p.position,valid)||!valid())return false;
    float radius=0;if(!Read(Live(p.sphere.Get()),L"SphereRadius",L"FloatProperty",radius)||radius!=p.radius||!valid())return false;out=p;return true;
}
bool Overlaps(const Probe& p,std::vector<CachedObjRef>& out,const std::function<bool()>& current){
    const auto valid=[&]{return current&&current()&&Armed(p);};if(!valid())return false;
    const Function overlap(R::FindClassDefaultObject(L"KismetSystemLibrary"),L"SphereOverlapActors");if(!overlap.Current())return false;ParamFrame frame(overlap.fn.Get());
    uint8_t pawn=2;Array types{&pawn,1,1},ignore;
    if(!Slots(frame,{{L"WorldContextObject",8},{L"SpherePos",12},{L"SphereRadius",4},{L"ObjectTypes",16},{L"ActorClassFilter",8},{L"ActorsToIgnore",16},{L"OutActors",16},{L"ReturnValue",1}})||
        !frame.Set<void*>(L"WorldContextObject",p.actor.Get())||!frame.Set(L"SpherePos",p.center)||!frame.Set(L"SphereRadius",p.radius)||!frame.Set(L"ObjectTypes",types)||
        !frame.Set<void*>(L"ActorClassFilter",nullptr)||!frame.Set(L"ActorsToIgnore",ignore)||!overlap.Current()||!valid()||!overlap.Current())return false;
    const bool called=Call(overlap.owner.object.Get(),frame);Array result;const bool read=frame.GetRaw(L"OutActors",&result,sizeof(result));
    struct Release{void* data;~Release(){if(data)R::EngineFree(data);}}release{result.data};bool hit=false;
    if(!called||!read||!overlap.Current()||!valid()||!frame.GetRaw(L"ReturnValue",&hit,1)||result.num<0||result.num>65536||result.max<result.num||(result.num&&!result.data))return false;
    std::vector<CachedObjRef> actors;actors.reserve(std::min(result.num,32));std::unordered_set<void*> seen;auto** pointers=static_cast<void**>(result.data);
    // Bound native rows separately from the controller's 31 registered candidates.
    // Unrelated pawns must not crowd a valid player out of a truncated overlap list.
    if(hit)for(int i=0;i<result.num;++i){void* actor=pointers[i];if(!actor||!R::IsLive(actor)||world_identity::WorldOf(actor)!=p.world||
        !seen.insert(actor).second)continue;CachedObjRef ref;ref.Set(actor);actors.push_back(ref);}
    if(!valid())return false;out=std::move(actors);return true;
}
bool ClearLine(const Probe& p,const FVector& eye,const std::function<bool()>& current){
    return Finite(eye)&&current&&current()&&Armed(p)&&trace::LineBlockedStatDyn(p.actor.Get(),p.origin,eye)==0&&current()&&Armed(p);
}
bool Launch(const Probe& p,const FVector& eye,bool& admitted,const std::function<bool()>& current,const std::function<bool()>& sourceCurrent){
    admitted=false;const auto armed=[&]{return current&&current()&&Armed(p);};if(!Finite(eye)||!armed())return false;
    const FVector delta{eye.X-p.position.X,eye.Y-p.position.Y,eye.Z-p.position.Z};
    // Native MakeRotationFromAxes(Cross(delta,up), delta, Cross(Cross(delta,up),delta)).
    const FVector x{delta.Y,-delta.X,0},z{-delta.X*delta.Z,-delta.Y*delta.Z,delta.X*delta.X+delta.Y*delta.Y};
    const FVector velocity{delta.X*3,delta.Y*3,delta.Z*3+100};
    if(!Finite(delta)||!Finite(x)||!Finite(z)||!Finite(velocity)||double(delta.X)*delta.X+double(delta.Y)*delta.Y<1e-6)return false;
    const Function axes(R::FindClassDefaultObject(L"KismetMathLibrary"),L"MakeRotationFromAxes"),rotation(p.actor.Get(),L"K2_SetActorRotation"),
        clear(R::FindClassDefaultObject(L"KismetSystemLibrary"),L"K2_ClearAndInvalidateTimerHandle"),motion(p.mesh.Get(),L"SetPhysicsLinearVelocity"),tail(p.actor.Get(),L"ExecuteUbergraph_prop_llama"),
        active(R::FindClassDefaultObject(L"KismetSystemLibrary"),L"K2_IsTimerActiveHandle"),physics(p.mesh.Get(),L"IsSimulatingPhysics");
    if(!axes.Current()||!rotation.Current()||!clear.Current()||!motion.Current()||!tail.Current()||!active.Current()||!physics.Current())return false;
    ParamFrame math(axes.fn.Get()),turn(rotation.fn.Get()),timer(clear.fn.Get()),push(motion.fn.Get()),effects(tail.fn.Get()),checkTimer(active.fn.Get()),checkPhysics(physics.fn.Get());const R::FName none{};
    if(!Slots(math,{{L"Forward",12},{L"Right",12},{L"Up",12},{L"ReturnValue",12}})||!math.Set(L"Forward",x)||!math.Set(L"Right",delta)||!math.Set(L"Up",z)||
        !Slots(turn,{{L"NewRotation",12},{L"bTeleportPhysics",1},{L"ReturnValue",1}})||!turn.Set<bool>(L"bTeleportPhysics",false)||
        !Slots(timer,{{L"WorldContextObject",8},{L"Handle",8}})||!timer.Set<void*>(L"WorldContextObject",p.actor.Get())||!timer.Set(L"Handle",p.timer)||
        !Slots(push,{{L"NewVel",12},{L"bAddToCurrent",1},{L"BoneName",8}})||!push.Set(L"NewVel",velocity)||!push.Set<bool>(L"bAddToCurrent",false)||!push.Set(L"BoneName",none)||
        !Slots(effects,{{L"EntryPoint",4}})||!effects.Set<int32_t>(L"EntryPoint",212)||
        !Slots(checkTimer,{{L"WorldContextObject",8},{L"Handle",8},{L"ReturnValue",1}})||!checkTimer.Set<void*>(L"WorldContextObject",p.actor.Get())||!checkTimer.Set(L"Handle",p.timer)||
        !Slots(checkPhysics,{{L"BoneName",8},{L"ReturnValue",1}})||!checkPhysics.Set(L"BoneName",none))return false;
    const auto prepared=[&]{return axes.Current()&&rotation.Current()&&clear.Current()&&motion.Current()&&tail.Current()&&active.Current()&&physics.Current()&&armed();};
    FRotator result{};bool turned=false;
    if(!prepared()||!Call(axes.owner.object.Get(),math)||!prepared()||!math.GetRaw(L"ReturnValue",&result,12)||!std::isfinite(result.Pitch)||!std::isfinite(result.Yaw)||!std::isfinite(result.Roll)||
        !turn.Set(L"NewRotation",result))return false;
    // Re-read source points: a callback may have moved the prop after the LOS query.
    FVector position{},origin{},center{};bool timerActive=false,simulating=false;const auto equal=[](const FVector& a,const FVector& b){return a.X==b.X&&a.Y==b.Y&&a.Z==b.Z;};
    if(!prepared()||!Call(active.owner.object.Get(),checkTimer)||!prepared()||!checkTimer.GetRaw(L"ReturnValue",&timerActive,1)||!timerActive||
        !Call(p.mesh.Get(),checkPhysics)||!prepared()||!checkPhysics.GetRaw(L"ReturnValue",&simulating,1)||!simulating||
        !Position(p.actor.Get(),L"K2_GetActorLocation",position,armed)||!Position(p.audio.Get(),L"K2_GetComponentLocation",origin,armed)||
        !Position(p.sphere.Get(),L"K2_GetComponentLocation",center,armed)||!equal(position,p.position)||!equal(origin,p.origin)||!equal(center,p.center)||
        !prepared()||!Call(p.actor.Get(),turn)||!prepared()||!turn.GetRaw(L"ReturnValue",&turned,1)||!turned||
        !Position(p.actor.Get(),L"K2_GetActorLocation",position,armed)||!equal(position,p.position)||!prepared())return false;
    const auto source=[&]{const auto& predicate=sourceCurrent?sourceCurrent:current;return Current(p)&&predicate&&predicate()&&Current(p);};
    admitted=true;const bool cleared=Call(clear.owner.object.Get(),timer);uint64_t handle=UINT64_MAX,actual=0;int offset=-1;
    // Never copy back over a replacement timer installed by a native callback.
    if(!cleared||!source()||!clear.Current()||!timer.GetRaw(L"Handle",&handle,8)||handle||!Timer(Live(p.actor.Get()),actual,nullptr,&offset)||actual!=p.timer)return false;
    std::memcpy(static_cast<uint8_t*>(p.actor.Get())+offset,&handle,8);
    if(!source()||!motion.Current())return false;
    // A failed ProcessEvent return does not establish that the velocity had no
    // effect. Its one-shot is never repeated; native event/light/audio cleanup
    // must still start once while this source remains ours.
    const bool moved=Call(p.mesh.Get(),push);
    if(!source()||!tail.Current())return false;
    const bool completed=Call(p.actor.Get(),effects);return moved&&completed&&source()&&tail.Current();
}
}
