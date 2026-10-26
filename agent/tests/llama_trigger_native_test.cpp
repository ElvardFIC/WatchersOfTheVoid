#include "../../src/votv-coop/src/ue_wrap/actors/llama_trigger.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL native %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
using ue_wrap::FVector;using ue_wrap::FRotator;namespace M=ue_wrap::llama_trigger;namespace R=ue_wrap::reflection;
enum Id {Actor,Sphere,Audio,Mesh,Other,Body=10,Class=50,SphereClass,AudioClass,MeshClass,TimerType,ForeignClass,Library=70,Math,Active=100,Physics,ComponentPosition,ActorPosition,Overlap,Axes,Rotation,Clear,Push,Graph,Count=160};
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;void* parent=nullptr;R::FName name{};uint8_t flags=4;
    void* sphere=nullptr;void* audio=nullptr;void* mesh=nullptr;uint64_t timer=123;float radius=200;FVector position{};} objects[Count];
Obj& O(void* p){CHECK(p);return *static_cast<Obj*>(p);}void* At(int id){return &objects[id];}
uint32_t generation=1;uintptr_t world=100;bool timerActive=true,simulation=true,hit=true,turns=true;
int fail=-1,missing=-1,badTimerSize=8,trace=0,frees=0,turnCount=0,pushCount=0,tailCount=0,clearCount=0;
int malformedArray=0,badSize=-1;std::wstring badSlot,badField;std::function<void(int)> onCall,onResolve;
std::vector<void*> overlapBodies;FVector velocity{},axes[3]{};FRotator rotation{1,2,3};std::vector<int> calls;
void Fire(std::function<void(int)>& action,int fn){auto f=std::move(action);action={};if(f)f(fn);}
void Reset(){++generation;++world;for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(generation*1000)+i;objects[i].cls=At(i);objects[i].name.ComparisonIndex=i;}
    objects[Actor].cls=At(Class);objects[Sphere].cls=At(SphereClass);objects[Audio].cls=At(AudioClass);objects[Mesh].cls=At(MeshClass);
    objects[Other].cls=At(ForeignClass);objects[Actor].sphere=At(Sphere);objects[Actor].audio=At(Audio);objects[Actor].mesh=At(Mesh);
    timerActive=simulation=hit=turns=true;fail=missing=-1;badTimerSize=8;trace=frees=turnCount=pushCount=tailCount=clearCount=malformedArray=0;
    badSize=-1;badSlot=badField=L"";onCall=onResolve={};overlapBodies={At(Body)};velocity={};rotation={1,2,3};calls.clear();}
M::Probe Probe(){M::Probe p;CHECK(M::Capture(At(Actor),p,[]{return true;}));return p;}
bool Launch(M::Probe& p,bool& admitted,FVector eye={10,20,30}){return M::Launch(p,eye,admitted,[]{return true;});}
std::vector<R::ParamInfo> Params(int fn){using P=R::ParamInfo;std::vector<P> p;
    switch(fn){case Active:p={{L"WorldContextObject",0,8,0},{L"Handle",8,8,0},{L"ReturnValue",16,1,0}};break;
    case Physics:p={{L"BoneName",0,8,0},{L"ReturnValue",8,1,0}};break;
    case ComponentPosition:case ActorPosition:p={{L"ReturnValue",0,12,0}};break;
    case Overlap:p={{L"WorldContextObject",0,8,0},{L"SpherePos",8,12,0},{L"SphereRadius",20,4,0},{L"ObjectTypes",24,16,0},{L"ActorClassFilter",40,8,0},{L"ActorsToIgnore",48,16,0},{L"OutActors",64,16,0},{L"ReturnValue",80,1,0}};break;
    case Axes:p={{L"Forward",0,12,0},{L"Right",12,12,0},{L"Up",24,12,0},{L"ReturnValue",36,12,0}};break;
    case Rotation:p={{L"NewRotation",0,12,0},{L"bTeleportPhysics",12,1,0},{L"ReturnValue",13,1,0}};break;
    case Clear:p={{L"WorldContextObject",0,8,0},{L"Handle",8,8,0}};break;
    case Push:p={{L"NewVel",0,12,0},{L"bAddToCurrent",12,1,0},{L"BoneName",16,8,0}};break;
    case Graph:p={{L"EntryPoint",0,4,0}};break;default:CHECK(false);}
    for(auto it=p.begin();it!=p.end();)if(it->name==badSlot){if(badSize<0)it=p.erase(it);else{it->size=badSize;++it;}}else ++it;return p;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}int32_t SlotSerial(int32_t i){return i>=0&&i<test::Count?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}void* ObjectAt(int32_t i){return i>=0&&i<test::Count?test::At(i):nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}void* SuperStructOf(void* p){return test::O(p).parent;}
const FName& NameOf(void* p){return test::O(p).name;}std::wstring ToString(const FName&){return L"trigger_fixture";}
bool NameEquals(const FName& n,const wchar_t* name){return (n.ComparisonIndex==test::Class&&!wcscmp(name,L"prop_llama_C"))||
    (n.ComparisonIndex==test::SphereClass&&!wcscmp(name,L"SphereComponent"))||(n.ComparisonIndex==test::AudioClass&&!wcscmp(name,L"AudioComponent"))||
    (n.ComparisonIndex==test::MeshClass&&!wcscmp(name,L"StaticMeshComponent"))||(n.ComparisonIndex==test::TimerType&&!wcscmp(name,L"TimerHandle"));}
int32_t StructSize(void* p){return p==test::At(test::TimerType)?test::badTimerSize:sizeof(test::Obj);}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& type){
    if(!wcscmp(name,L"Sphere")){offset=offsetof(test::Obj,sphere);type=L"ObjectProperty";}
    else if(!wcscmp(name,L"Audio")){offset=offsetof(test::Obj,audio);type=L"ObjectProperty";}
    else if(!wcscmp(name,L"StaticMesh")){offset=offsetof(test::Obj,mesh);type=L"ObjectProperty";}
    else if(!wcscmp(name,L"timer")){offset=offsetof(test::Obj,timer);type=L"StructProperty";}
    else if(!wcscmp(name,L"SphereRadius")){offset=offsetof(test::Obj,radius);type=L"FloatProperty";}
    else return false;if(test::badField==name)type=L"IntProperty";return true;}
void* PropertyInnerStruct(void*,const wchar_t*){return test::At(test::TimerType);}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& offset,uint8_t& mask){offset=offsetof(test::Obj,flags);mask=!wcscmp(name,L"h")?1:4;return true;}
void* FindClassDefaultObject(const wchar_t* name){return test::At(!wcscmp(name,L"KismetSystemLibrary")?test::Library:test::Math);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){int id=-1;
    const wchar_t* names[]={L"K2_IsTimerActiveHandle",L"IsSimulatingPhysics",L"K2_GetComponentLocation",L"K2_GetActorLocation",L"SphereOverlapActors",L"MakeRotationFromAxes",L"K2_SetActorRotation",L"K2_ClearAndInvalidateTimerHandle",L"SetPhysicsLinearVelocity",L"ExecuteUbergraph_prop_llama"};
    for(int i=0;i<10;++i)if(!wcscmp(name,names[i]))id=test::Active+i;test::Fire(test::onResolve,id);return id<0||id==test::missing?nullptr:test::At(id);}
int32_t FunctionFrameSize(void*){return 128;}std::vector<ParamInfo> FunctionParams(void* p){return test::Params(test::O(p).id);}
void EngineFree(void* p){++test::frees;std::free(p);}
bool CallFunction(void* owner,void* function,void* raw){using namespace test;const int fn=O(function).id;auto* bytes=static_cast<uint8_t*>(raw);calls.push_back(fn);const bool result=fail!=fn;
    switch(fn){case Active:CHECK(*reinterpret_cast<void**>(bytes)==At(Actor)&&*reinterpret_cast<uint64_t*>(bytes+8)==objects[Actor].timer);bytes[16]=timerActive;break;
    case Physics:CHECK(owner==At(Mesh)&&!*reinterpret_cast<uint64_t*>(bytes));bytes[8]=simulation;break;
    case ComponentPosition:case ActorPosition:std::memcpy(bytes,&O(owner).position,12);break;
    case Overlap:{CHECK(*reinterpret_cast<void**>(bytes)==At(Actor)&&*reinterpret_cast<float*>(bytes+20)==objects[Sphere].radius);
        CHECK(*reinterpret_cast<uint8_t**>(bytes+24)&&**reinterpret_cast<uint8_t**>(bytes+24)==2&&*reinterpret_cast<int32_t*>(bytes+32)==1);
        CHECK(!*reinterpret_cast<void**>(bytes+40)&&!*reinterpret_cast<int32_t*>(bytes+56));
        struct Array{void* data;int num,max;} output;output.num=int(overlapBodies.size());output.max=output.num;output.data=std::malloc(std::max(size_t(1),overlapBodies.size())*8);
        std::memcpy(output.data,overlapBodies.data(),overlapBodies.size()*8);if(malformedArray==1)output.num=-1;if(malformedArray==2)output.num=65537;if(malformedArray==3)output.max=output.num-1;
        std::memcpy(bytes+64,&output,16);bytes[80]=hit;break;}
    case Axes:std::memcpy(axes,bytes,36);std::memcpy(bytes+36,&rotation,12);break;
    case Rotation:CHECK(!bytes[12]);++turnCount;bytes[13]=turns;break;
    case Clear:CHECK(*reinterpret_cast<void**>(bytes)==At(Actor)&&*reinterpret_cast<uint64_t*>(bytes+8)==123);++clearCount;*reinterpret_cast<uint64_t*>(bytes+8)=0;break;
    case Push:CHECK(owner==At(Mesh));std::memcpy(&velocity,bytes,12);CHECK(!bytes[12]&&!*reinterpret_cast<uint64_t*>(bytes+16));++pushCount;break;
    case Graph:CHECK(owner==At(Actor)&&*reinterpret_cast<int32_t*>(bytes)==212);++tailCount;objects[Actor].flags|=1;break;
    default:CHECK(false);}
    Fire(onCall,fn);return result;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::llama {bool IsLlama(void* p){return p&&test::O(p).cls==test::At(test::Class)&&test::O(p).live;}}
namespace ue_wrap::trace {int LineBlockedStatDyn(void* actor,const FVector& from,const FVector& to){CHECK(actor==test::At(test::Actor)&&from.X==test::objects[test::Audio].position.X);test::Fire(test::onCall,-2);return test::trace;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace test {
void Group(int id){Reset();M::Probe p;bool admitted=false;uint64_t output=999;std::vector<ue_wrap::CachedObjRef> actors;
    switch(id){
    case 26:objects[Actor].cls=At(ForeignClass);CHECK(!M::Capture(At(Actor),p,[]{return true;}));break;
    case 27:objects[Actor].flags|=1;CHECK(!M::Capture(At(Actor),p,[]{return true;}));break;
    case 28:objects[Actor].flags&=~4;CHECK(!M::Capture(At(Actor),p,[]{return true;}));break;
    case 29:badField=L"timer";CHECK(!M::ReadTimer(At(Actor),output)&&output==999);break;
    case 30:badTimerSize=4;CHECK(!M::ReadTimer(At(Actor),output)&&output==999);break;
    case 31:objects[Actor].timer=0;CHECK(!M::Capture(At(Actor),p,[]{return true;}));break;
    case 32:timerActive=false;CHECK(!M::Capture(At(Actor),p,[]{return true;}));Reset();p=Probe();timerActive=false;CHECK(!Launch(p,admitted)&&!admitted);break;
    case 33:objects[Sphere].cls=At(ForeignClass);CHECK(!M::Capture(At(Actor),p,[]{return true;}));break;
    case 34:for(float radius:{0.f,-1.f,1e7f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){objects[Sphere].radius=radius;CHECK(!M::Capture(At(Actor),p,[]{return true;}));}
        Reset();badField=L"SphereRadius";CHECK(!M::Capture(At(Actor),p,[]{return true;}));Reset();p=Probe();objects[Sphere].radius+=1;CHECK(!Launch(p,admitted)&&!admitted);break;
    case 35:objects[Sphere].position={4,5,6};p=Probe();CHECK(p.center.X==4&&p.center.Y==5&&p.center.Z==6);objects[Sphere].position.X=std::numeric_limits<float>::infinity();
        p.radius=999;CHECK(!M::Capture(At(Actor),p,[]{return true;})&&p.radius==999);break;
    case 36:objects[Audio].position={7,8,9};p=Probe();CHECK(p.origin.X==7&&p.origin.Y==8&&p.origin.Z==9&&M::ClearLine(p,{2,3,4},[]{return true;}));
        trace=-1;CHECK(!M::ClearLine(p,{2,3,4},[]{return true;}));break;
    case 37:objects[Mesh].cls=At(ForeignClass);CHECK(!M::Capture(At(Actor),p,[]{return true;}));break;
    case 38:simulation=false;CHECK(!M::Capture(At(Actor),p,[]{return true;}));Reset();p=Probe();simulation=false;CHECK(!Launch(p,admitted)&&!admitted);break;
    case 39:for(int cause=0;cause<6;++cause){Reset();p=Probe();if(cause==0)objects[Actor].mesh=At(Other);if(cause==1)objects[Actor].audio=At(Other);
        if(cause==2)objects[Actor].sphere=At(Other);if(cause==3)++objects[Actor].serial;if(cause==4)++objects[MeshClass].serial;if(cause==5)++objects[TimerType].serial;
        CHECK(!Launch(p,admitted)&&!admitted&&!pushCount);}break;
    case 40:for(int cause=0;cause<2;++cause){Reset();p=Probe();if(cause)++generation;else ++world;CHECK(!Launch(p,admitted)&&!admitted);}break;
    case 41:for(int fn:{Active,Physics,ComponentPosition,ActorPosition}){Reset();missing=fn;CHECK(!M::Capture(At(Actor),p,[]{return true;}));}
        for(const wchar_t* name:{L"WorldContextObject",L"Handle",L"ReturnValue",L"BoneName"})for(bool size:{false,true}){Reset();badSlot=name;badSize=size?3:-1;CHECK(!M::Capture(At(Actor),p,[]{return true;}));}break;
    case 42:case 43:case 44:p=Probe();CHECK(M::Overlaps(p,actors,[]{return true;})&&actors.size()==1&&frees==1);break;
    case 45:p=Probe();CHECK(M::Overlaps(p,actors,[]{return true;})&&frees==1);break;
    case 46:p=Probe();fail=Overlap;CHECK(!M::Overlaps(p,actors,[]{return true;})&&frees==1&&actors.empty());break;
    case 47:for(int cause=1;cause<=3;++cause){Reset();p=Probe();malformedArray=cause;CHECK(!M::Overlaps(p,actors,[]{return true;})&&frees==1);}break;
    case 48:p=Probe();overlapBodies={At(Body),At(Body),nullptr};CHECK(M::Overlaps(p,actors,[]{return true;})&&actors.size()==1);break;
    case 50:p.radius=999;onCall=[](int fn){CHECK(fn==Active);++objects[Class].serial;};CHECK(!M::Capture(At(Actor),p,[]{return true;})&&p.radius==999);break;
    case 51:p=Probe();CHECK(Launch(p,admitted)&&admitted&&velocity.X==30&&velocity.Y==60&&velocity.Z==190);break;
    case 52:p=Probe();CHECK(Launch(p,admitted)&&axes[0].X==20&&axes[0].Y==-10&&axes[1].Z==30&&axes[2].X==-300&&axes[2].Y==-600&&axes[2].Z==500);break;
    case 53:p=Probe();CHECK(!Launch(p,admitted,{0,0,30})&&!admitted&&!turnCount);break;
    case 54:p=Probe();rotation.Yaw=std::numeric_limits<float>::infinity();CHECK(!Launch(p,admitted)&&!admitted&&!turnCount);break;
    case 55:p=Probe();CHECK(Launch(p,admitted,{-10,-20,-30})&&velocity.X==-30&&velocity.Y==-60);break;
    case 56:p=Probe();CHECK(Launch(p,admitted,{10,20,0})&&velocity.Z==100);break;
    case 57:p=Probe();CHECK(!Launch(p,admitted,{std::numeric_limits<float>::max(),20,0})&&!admitted&&!turnCount);break;
    case 58:for(int fn:{Axes,Rotation,Clear,Push,Graph}){Reset();p=Probe();missing=fn;CHECK(!Launch(p,admitted)&&!admitted&&!turnCount&&!clearCount&&!pushCount);}
        Reset();p=Probe();onCall=[](int fn){CHECK(fn==Axes);++objects[Push].serial;};CHECK(!Launch(p,admitted)&&!admitted&&!turnCount);
        Reset();p=Probe();onCall=[](int fn){CHECK(fn==Axes);objects[Actor].position.X=2;};CHECK(!Launch(p,admitted)&&!admitted&&!turnCount);break;
    case 59:p=Probe();badSlot=L"bTeleportPhysics";badSize=4;CHECK(!Launch(p,admitted)&&!admitted&&!turnCount);break;
    case 60:p=Probe();turns=false;CHECK(!Launch(p,admitted)&&!admitted&&!clearCount&&!pushCount);break;
    case 61:p=Probe();CHECK(Launch(p,admitted)&&admitted&&objects[Actor].timer==0&&clearCount==1);Reset();p=Probe();fail=Clear;
        CHECK(!Launch(p,admitted)&&admitted&&clearCount==1&&!pushCount);break;
    case 62:p=Probe();{std::function<void(int)> callback;callback=[&](int fn){if(fn==Clear)objects[Actor].timer=456;else onCall=callback;};
        onCall=callback;CHECK(!Launch(p,admitted)&&admitted&&objects[Actor].timer==456&&!pushCount);}break;
    case 63:case 64:p=Probe();CHECK(Launch(p,admitted)&&pushCount==1);if(id==63){Reset();p=Probe();fail=Push;CHECK(!Launch(p,admitted)&&admitted&&pushCount==1&&tailCount==1);}break;
    case 65:p=Probe();CHECK(Launch(p,admitted)&&tailCount==1&&(objects[Actor].flags&1));CHECK(calls[calls.size()-3]==Clear&&calls[calls.size()-2]==Push&&calls.back()==Graph);break;
    default:CHECK(false);}
}
void Recovery(int id){Reset();auto p=Probe();bool admitted=false,target=true;const auto source=[]{return true;};
    std::function<void(int)> callback;
    switch(id){
    case 51:callback=[&](int fn){if(fn==Clear)target=false;else onCall=callback;};onCall=callback;
        CHECK(M::Launch(p,{10,20,30},admitted,[&]{CHECK(target);return true;},source)&&admitted&&pushCount==1&&tailCount==1);break;
    case 52:callback=[&](int fn){if(fn==Clear)target=false;else onCall=callback;};onCall=callback;
        CHECK(M::Launch(p,{10,20,30},admitted,[&]{return target;},source)&&!target&&pushCount==1&&tailCount==1);break;
    case 53:{FVector eye{10,20,30};callback=[&](int fn){if(fn==Clear)eye.X=200;else onCall=callback;};onCall=callback;
        CHECK(M::Launch(p,{10,20,30},admitted,[&]{return eye.X==10;},source)&&eye.X==200&&velocity.X==30&&tailCount==1);}break;
    case 54:{ue_wrap::CachedObjRef body;body.Set(At(Body));callback=[&](int fn){if(fn==Clear)++objects[Body].serial;else onCall=callback;};onCall=callback;
        CHECK(M::Launch(p,{10,20,30},admitted,[&]{return body.Get()!=nullptr;},source)&&!body.Get()&&tailCount==1);}break;
    case 55:for(int cause=0;cause<4;++cause){Reset();p=Probe();callback=[&](int fn){if(fn==Clear){if(cause==0)++objects[Actor].serial;if(cause==1)++objects[Class].serial;
            if(cause==2)++world;if(cause==3)++generation;}else onCall=callback;};onCall=callback;
            CHECK(!M::Launch(p,{10,20,30},admitted,source,source)&&admitted&&!pushCount&&!tailCount);}break;
    case 56:callback=[&](int fn){if(fn==Rotation)objects[Actor].position.X=5;else onCall=callback;};onCall=callback;
        CHECK(!M::Launch(p,{10,20,30},admitted,source,source)&&!admitted&&!clearCount&&!pushCount);break;
    case 57:fail=Push;CHECK(!M::Launch(p,{10,20,30},admitted,source,source)&&admitted&&pushCount==1&&tailCount==1);
        CHECK(!M::Launch(p,{10,20,30},admitted,source,source)&&!admitted&&pushCount==1&&tailCount==1);break;
    case 58:callback=[&](int fn){if(fn==Push)++objects[Push].serial;else onCall=callback;};onCall=callback;
        CHECK(M::Launch(p,{10,20,30},admitted,source,source)&&admitted&&pushCount==1&&tailCount==1);break;
    default:CHECK(false);}
}
}
int main(){for(int id=26;id<=65;++id){if(id==49)continue;test::Group(id);std::printf("PASS llama trigger %d\n",id);}
    for(int id=51;id<=58;++id){test::Recovery(id);std::printf("PASS llama recovery %d\n",id);}return 0;}
