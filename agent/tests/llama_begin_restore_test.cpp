#include "../../src/votv-coop/src/ue_wrap/actors/llama_begin_restore.cpp"
#include "../../src/votv-coop/src/coop/props/llama_begin_restore.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <set>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL begin restore %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace T=coop::llama_begin_restore;namespace M=ue_wrap::llama_begin_restore;namespace R=ue_wrap::reflection;
enum Id {Actor,Sphere,Other,ActorClass=20,SphereClass,TimerType,KslClass,ForeignClass,Library=30,Get=40,Set,Graph,Exists,Active,Count=420};
struct Obj {int id=0,serial=1;bool live=true,world=false;void* cls=nullptr;void* parent=nullptr;R::FName name{};
    uint8_t flags=4,collision=3;void* sphere=nullptr;uint64_t timer=0;} objects[Count];
void* At(int id){return &objects[id];}Obj& O(void* p){CHECK(p);return *static_cast<Obj*>(p);}
uint32_t generation=1,sessionSerial=1;uintptr_t world=100;uint64_t now=1000;
coop::net::Session session;int gets=0,sets=0,graphs=0,existsQueries=0,activeQueries=0;
bool hand=false,wrongField=false,badSpan=false,graphEffect=true,setEffect=true,timerActive=true;int timerSize=8,missing=-1,fail=-1;
std::wstring badParam;int paramBytes=-1;std::function<void(int)> callback;std::set<uint64_t> invocations;
void Fire(int fn){auto f=std::move(callback);callback={};if(f)f(fn);}
void Reset(){T::OnDisconnect();++generation;++sessionSerial;++world;
    for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(generation*1000)+i;objects[i].cls=At(i);objects[i].name.ComparisonIndex=i;}
    objects[Actor].cls=At(ActorClass);objects[Sphere].cls=At(SphereClass);objects[Other].cls=At(SphereClass);objects[Library].cls=At(KslClass);
    objects[Actor].sphere=At(Sphere);objects[Actor].world=objects[Sphere].world=objects[Other].world=true;
    for(int i=100;i<Count;++i){objects[i].cls=At(ActorClass);objects[i].sphere=At(Sphere);objects[i].world=true;}
    session={};session.mode=coop::net::Role::Client;now=1000;gets=sets=graphs=existsQueries=activeQueries=0;hand=wrongField=badSpan=false;
    graphEffect=setEffect=timerActive=true;timerSize=8;missing=fail=-1;badParam.clear();paramBytes=-1;callback={};invocations={500};
    T::Install(&session);
}
uint64_t Begin(void* actor=nullptr,uint64_t invocation=500,bool finish=true){uint64_t ticket=0;CHECK(T::Prepare(actor?actor:At(Actor),true,invocation,[]{return true;},ticket)&&ticket);
    T::Commit(ticket);objects[Sphere].collision=0;if(finish)T::Finished(invocation);return ticket;}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}int32_t SlotSerial(int32_t i){return i>=0&&i<test::Count?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}bool IsLiveByIndex(void* p,int32_t i){return p&&i>=0&&i<test::Count&&p==test::At(i)&&IsLive(p);}
void* ObjectAt(int32_t i){return i>=0&&i<test::Count?test::At(i):nullptr;}
void* SuperStructOf(void* p){return test::O(p).parent;}const FName& NameOf(void* p){return test::O(p).name;}
std::wstring ToString(const FName&){return L"begin_restore_fixture";}
bool NameEquals(const FName& n,const wchar_t* name){return (n.ComparisonIndex==test::ActorClass&&!wcscmp(name,L"prop_llama_C"))||
    (n.ComparisonIndex==test::SphereClass&&!wcscmp(name,L"SphereComponent"))||(n.ComparisonIndex==test::TimerType&&!wcscmp(name,L"TimerHandle"))||
    (n.ComparisonIndex==test::KslClass&&!wcscmp(name,L"KismetSystemLibrary"));}
int32_t StructSize(void* p){return p==test::At(test::TimerType)?test::timerSize:sizeof(test::Obj);}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& type){
    if(!wcscmp(name,L"Sphere")){offset=test::badSpan?int(sizeof(test::Obj)-4):offsetof(test::Obj,sphere);type=L"ObjectProperty";}
    else if(!wcscmp(name,L"timer")){offset=offsetof(test::Obj,timer);type=test::wrongField?L"IntProperty":L"StructProperty";}else return false;return true;}
void* PropertyInnerStruct(void*,const wchar_t*){return test::At(test::TimerType);}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& offset,uint8_t& mask){offset=offsetof(test::Obj,flags);mask=!wcscmp(name,L"h")?1:4;return true;}
void* FindClassDefaultObject(const wchar_t*){return test::At(test::Library);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){int fn=-1;
    if(!wcscmp(name,L"GetCollisionEnabled"))fn=test::Get;if(!wcscmp(name,L"SetCollisionEnabled"))fn=test::Set;
    if(!wcscmp(name,L"ExecuteUbergraph_prop_llama"))fn=test::Graph;if(!wcscmp(name,L"K2_TimerExistsHandle"))fn=test::Exists;
    if(!wcscmp(name,L"K2_IsTimerActiveHandle"))fn=test::Active;return fn<0||fn==test::missing?nullptr:test::At(fn);}
int32_t FunctionFrameSize(void*){return 32;}
std::vector<ParamInfo> FunctionParams(void* p){using namespace test;std::vector<ParamInfo> params;
    switch(O(p).id){case Get:params={{L"ReturnValue",0,1,0}};break;case Set:params={{L"NewType",0,1,0}};break;case Graph:params={{L"EntryPoint",0,4,0}};break;
    case Exists:case Active:params={{L"WorldContextObject",0,8,0},{L"Handle",8,8,0},{L"ReturnValue",16,1,0}};break;default:CHECK(false);}
    for(auto it=params.begin();it!=params.end();)if(it->name==badParam){if(paramBytes<0)it=params.erase(it);else{it->size=paramBytes;++it;}}else ++it;return params;
}
bool CallFunction(void* object,void* fn,void* raw){using namespace test;const int id=O(fn).id;auto* bytes=static_cast<uint8_t*>(raw);const bool success=id!=fail;
    if(id==Get){++gets;bytes[0]=O(object).collision;}
    if(id==Set){++sets;CHECK(bytes[0]<=3);if(setEffect)O(object).collision=bytes[0];}
    if(id==Graph){++graphs;CHECK(*reinterpret_cast<int32_t*>(bytes)==108);if(graphEffect)O(object).timer=321;}
    if(id==Exists||id==Active){CHECK(object==At(Library));void* actor=*reinterpret_cast<void**>(bytes);CHECK(*reinterpret_cast<uint64_t*>(bytes+8)==O(actor).timer);
        bytes[16]=uint8_t(id==Exists?O(actor).timer!=0:timerActive);if(id==Exists)++existsQueries;else ++activeQueries;}
    Fire(id);return success;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}
void* WorldOf(void* p){return p&&test::O(p).world?CurrentWorld():nullptr;}}
namespace ue_wrap::llama {bool IsLlama(void* p){return p&&test::O(p).live&&test::O(p).cls==test::At(test::ActorClass);}}
namespace ue_wrap::script_gate {bool IsInvocationActive(uint64_t id){return test::invocations.contains(id);}}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace test {
void Group(int id){Reset();uint64_t ticket=0;M::Snapshot snapshot;switch(id){
    case 51:CHECK(T::Prepare(At(Actor),false,500,[]{return true;},ticket)&&!ticket&&T::g_rows.empty()&&!gets);break;
    case 52:objects[Actor].timer=123;CHECK(!T::Prepare(At(Actor),true,500,[]{return true;},ticket)&&!ticket&&T::g_rows.empty());break;
    case 53:objects[Actor].timer=123;CHECK(!T::Prepare(At(Actor),true,500,[]{return true;},ticket));T::OnDisconnect();CHECK(!graphs&&objects[Actor].timer==123);break;
    case 54:Begin();T::OnDisconnect();CHECK(objects[Sphere].collision==3&&objects[Actor].timer==321&&graphs==1);break;
    case 55:objects[Sphere].cls=At(ForeignClass);CHECK(!T::Prepare(At(Actor),true,500,[]{return true;},ticket)&&!gets);break;
    case 56:badSpan=true;CHECK(!T::Prepare(At(Actor),true,500,[]{return true;},ticket)&&!gets);break;
    case 57:badParam=L"ReturnValue";paramBytes=4;CHECK(!T::Prepare(At(Actor),true,500,[]{return true;},ticket)&&!gets);
        Reset();objects[Sphere].collision=4;CHECK(!T::Prepare(At(Actor),true,500,[]{return true;},ticket));break;
    case 58:snapshot.collision=99;callback=[](int fn){CHECK(fn==Get);++objects[ActorClass].serial;};CHECK(!M::Capture(At(Actor),snapshot,[]{return true;})&&snapshot.collision==99);break;
    case 59:for(int cause=0;cause<3;++cause){Reset();Begin();if(cause==0)++objects[Actor].serial;if(cause==1)++objects[ActorClass].serial;if(cause==2)++world;
        T::OnDisconnect();CHECK(!graphs&&!sets&&T::g_rows.empty());}break;
    case 60:Begin();++objects[SphereClass].serial;T::OnDisconnect();CHECK(!graphs&&!sets);break;
    case 61:for(int cause=0;cause<2;++cause){Reset();if(cause)timerSize=4;else wrongField=true;CHECK(!T::Prepare(At(Actor),true,500,[]{return true;},ticket));}break;
    case 62:Begin(nullptr,500,false);T::OnDisconnect();T::Finished(501);CHECK(!graphs&&T::g_frames.size()==1);T::Finished(500);CHECK(graphs==1&&T::g_frames.empty());break;
    case 63:callback=[](int fn){CHECK(fn==Get&&T::g_frames.size()==1);uint64_t nested=0;CHECK(!T::Prepare(At(Actor),true,501,[]{return true;},nested));};Begin();CHECK(T::g_rows.size()==1);
        Reset();callback=[](int fn){CHECK(fn==Get);T::Finished(500);};CHECK(!T::Prepare(At(Actor),true,500,[]{return true;},ticket)&&T::g_rows.empty());break;
    case 64:CHECK(T::Prepare(At(Actor),true,500,[]{return true;},ticket));T::Finished(500);T::OnDisconnect();CHECK(!graphs&&!sets);break;
    case 65:CHECK(T::Prepare(At(Actor),true,500,[]{return true;},ticket));T::Cancel(ticket);objects[Sphere].collision=0;T::OnDisconnect();CHECK(T::g_rows.empty()&&!sets&&!graphs);break;
    case 66:Begin(nullptr,500,false);T::OnDisconnect();CHECK(!sets&&!graphs);T::Finished(500);CHECK(sets==1&&graphs==1);break;
    case 67:Begin();objects[Sphere].collision=1;T::OnDisconnect();CHECK(!sets&&objects[Sphere].collision==1&&graphs==1);break;
    case 68:Begin();objects[Actor].sphere=At(Other);T::OnDisconnect();CHECK(!sets&&!graphs);break;
    case 69:Begin();objects[Actor].timer=999;T::OnDisconnect();CHECK(!graphs&&objects[Actor].timer==999);
        Reset();Begin();{std::function<void(int)> action;action=[&](int fn){if(fn==Set)objects[Actor].timer=999;else callback=action;};callback=action;T::OnDisconnect();CHECK(!graphs&&objects[Actor].timer==999);}break;
    case 70:Begin();T::OnDisconnect();CHECK(graphs==1);T::Tick();T::OnDisconnect();CHECK(graphs==1);break;
    case 71:Begin();T::Tick();CHECK(!graphs);session.mode=coop::net::Role::Host;T::Install(&session);CHECK(graphs==1);
        Reset();Begin();objects[Actor].flags|=1;T::OnDisconnect();CHECK(!graphs&&T::g_rows.size()==1);objects[Actor].flags&=~1;now+=100;T::Tick();CHECK(graphs==1);break;
    case 72:Begin();fail=Graph;T::OnDisconnect();CHECK(graphs==1&&objects[Actor].timer==321);now+=100;T::Tick();CHECK(graphs==1);
        Reset();Begin();graphEffect=false;T::OnDisconnect();CHECK(graphs==1&&!objects[Actor].timer);T::Tick();CHECK(graphs==1);
        Reset();Begin();T::Consumed(At(Actor));T::OnDisconnect();CHECK(!graphs&&T::g_rows.empty());
        Reset();Begin();++objects[Actor].serial;T::Consumed(At(Actor));CHECK(T::g_rows.size()==1);T::Tick();CHECK(T::g_rows.empty());break;
    case 73:Begin();T::OnDisconnect();CHECK(existsQueries==1&&activeQueries==1&&objects[Actor].timer==321);break;
    case 74:for(int i=0;i<256;++i){const auto invocation=uint64_t(1000+i);invocations.insert(invocation);Begin(At(100+i),invocation);}CHECK(T::g_rows.size()==256);
        CHECK(!T::Prepare(At(356),true,2000,[]{return true;},ticket));objects[100].live=false;T::Tick();CHECK(T::Prepare(At(356),true,2000,[]{return true;},ticket));T::Cancel(ticket);
        Reset();Begin();CHECK(T::Prepare(At(Actor),true,501,[]{return true;},ticket));T::Commit(ticket);T::Finished(501);T::OnDisconnect();CHECK(objects[Sphere].collision==3&&graphs==1);break;
    case 75:Begin();wrongField=true;T::OnDisconnect();CHECK(!graphs&&T::g_rows.size()==1);wrongField=false;T::Tick();CHECK(!graphs);now+=100;T::Tick();CHECK(graphs==1);
        Reset();Begin(nullptr,500,false);invocations.clear();T::Tick();T::OnDisconnect();CHECK(!graphs&&objects[Sphere].collision==3);break;
    default:CHECK(false);}
}
}
int main(){for(int id=51;id<=75;++id){test::Group(id);std::printf("PASS llama lifecycle %d\n",id);}return 0;}
