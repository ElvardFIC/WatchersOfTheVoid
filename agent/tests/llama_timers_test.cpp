#include "../../src/votv-coop/src/ue_wrap/actors/llama_timer.cpp"
#include "../../src/votv-coop/src/coop/props/llama_timers.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL llama timers %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace T=coop::llama_timers;namespace M=ue_wrap::llama_timer;namespace R=ue_wrap::reflection;
enum Id {Actor=1,Second=2,Class=600,DerivedClass,TimerType=702,Library=710,LibraryClass,ForeignClass,Exists=800,Active,Paused,Pause,Resume,Count=1024};
struct Obj {int id=0,serial=1;bool live=true,foreignWorld=false;void* cls=nullptr;void* parent=nullptr;R::FName name{};
    uint64_t timer=123;bool exists=true,active=true,paused=false;float remaining=.23f;} objects[Count];
Obj& O(void* p){CHECK(p);return *static_cast<Obj*>(p);}void* At(int i){return &objects[i];}
uint32_t generation=1,serial=1;uintptr_t world=100;uint64_t now=1000;
coop::net::Session session;bool hand=false,library=true,pauseEffect=true,resumeEffect=true;
int missing=-1,fail=-1,timerSize=8,frameSize=32,pauseCalls=0,resumeCalls=0,libraryLookups=0;bool wrongField=false,wrongTimerName=false;
std::wstring badSlot;int badSize=-1,badOffset=-1;std::function<void(int)> onCall;std::vector<void*> instances;
void Fire(int fn){auto copy=std::move(onCall);onCall={};if(copy)copy(fn);}
void When(int target,const std::function<void()>& action){onCall=[target,action](int fn){if(fn==target)action();else When(target,action);};}
void Reset(){++generation;++serial;++world;T::OnDisconnect();
    for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(generation*10000)+i;objects[i].cls=At(i);objects[i].name.ComparisonIndex=i;}
    for(int i=1;i<=300;++i)objects[i].cls=At(Class);objects[Library].cls=At(LibraryClass);objects[DerivedClass].parent=At(Class);
    session={};session.mode=coop::net::Role::Client;now=1000;hand=false;library=pauseEffect=resumeEffect=true;
    missing=fail=-1;timerSize=8;frameSize=32;pauseCalls=resumeCalls=libraryLookups=0;wrongField=wrongTimerName=false;badSlot.clear();badSize=badOffset=-1;onCall={};instances={At(Actor)};}
void Begin(){T::Install(&session);T::Tick();}
bool PausedNow(int actor=Actor){return objects[actor].paused;}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}int32_t SlotSerial(int32_t i){return i>=0&&i<test::Count?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}void* ObjectAt(int32_t i){return i>=0&&i<test::Count?test::At(i):nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}void* SuperStructOf(void* p){return test::O(p).parent;}
const FName& NameOf(void* p){return test::O(p).name;}std::wstring ToString(const FName&){return L"llama_timer_fixture";}
bool NameEquals(const FName& n,const wchar_t* name){return (n.ComparisonIndex==test::LibraryClass&&!wcscmp(name,L"KismetSystemLibrary"))||
    (!test::wrongTimerName&&n.ComparisonIndex==test::TimerType&&!wcscmp(name,L"TimerHandle"));}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& type){CHECK(!wcscmp(name,L"timer"));offset=offsetof(test::Obj,timer);type=test::wrongField?L"IntProperty":L"StructProperty";return true;}
void* PropertyInnerStruct(void*,const wchar_t* name){CHECK(!wcscmp(name,L"timer"));return test::At(test::TimerType);}
int32_t StructSize(void* p){return p==test::At(test::TimerType)?test::timerSize:sizeof(test::Obj);}
void* FindClassDefaultObject(const wchar_t* name){CHECK(!wcscmp(name,L"KismetSystemLibrary"));++test::libraryLookups;return test::library?test::At(test::Library):nullptr;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){const wchar_t* names[]={L"K2_TimerExistsHandle",L"K2_IsTimerActiveHandle",L"K2_IsTimerPausedHandle",L"K2_PauseTimerHandle",L"K2_UnPauseTimerHandle"};
    for(int i=0;i<5;++i)if(!wcscmp(name,names[i]))return test::missing==test::Exists+i?nullptr:test::At(test::Exists+i);return nullptr;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
std::vector<ParamInfo> FunctionParams(void* p){std::vector<ParamInfo> params={{L"WorldContextObject",0,8,0},{L"Handle",8,8,0}};
    if(test::O(p).id<test::Pause)params.push_back({L"ReturnValue",16,1,0});
    for(auto it=params.begin();it!=params.end();)if(it->name==test::badSlot){if(test::badSize<0)it=params.erase(it);else{it->size=test::badSize;if(test::badOffset>=0)it->offset=test::badOffset;++it;}}else ++it;return params;}
bool CallFunction(void* owner,void* fn,void* raw){using namespace test;CHECK(owner==At(Library));const int id=O(fn).id;auto* bytes=static_cast<uint8_t*>(raw);
    void* pointer=nullptr;uint64_t handle=0;std::memcpy(&pointer,bytes,8);std::memcpy(&handle,bytes+8,8);auto& actor=O(pointer);
    CHECK(handle&&handle==actor.timer);const bool result=fail!=id;
    if(id==Exists)bytes[16]=uint8_t(actor.exists);else if(id==Active)bytes[16]=uint8_t(actor.exists&&actor.active&&!actor.paused);else if(id==Paused)bytes[16]=uint8_t(actor.exists&&actor.paused);
    else if(id==Pause){++pauseCalls;if(pauseEffect)actor.paused=true;}else if(id==Resume){++resumeCalls;if(resumeEffect)actor.paused=false;}else CHECK(false);
    Fire(id);return result;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}
void* WorldOf(void* p){if(!p||test::O(p).id>=test::Class)return nullptr;return reinterpret_cast<void*>(test::O(p).foreignWorld?test::world+1:test::world);}
}
namespace ue_wrap::llama {
bool IsLlama(void* p){if(!p||!reflection::IsLive(p))return false;void* cls=reflection::ClassOf(p);return cls==test::At(test::Class)||cls==test::At(test::DerivedClass);}
size_t Collect(CachedObjRef* out,size_t capacity){size_t count=0;for(void* actor:test::instances){if(count>=capacity)break;out[count++].Set(actor);}return count;}
}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace test {
void Group(int id){Reset();switch(id){
    case 1:Begin();CHECK(pauseCalls==1&&PausedNow()&&T::g_rows.size()==1);break;
    case 2:instances.clear();Begin();CHECK(!pauseCalls);instances={At(Actor)};now+=100;T::Tick();CHECK(pauseCalls==1&&PausedNow());break;
    case 3:for(int cause=0;cause<2;++cause){Reset();if(cause)objects[Actor].foreignWorld=true;else objects[Actor].cls=At(ForeignClass);Begin();CHECK(!pauseCalls&&T::g_rows.empty());}break;
    case 4:hand=true;Begin();CHECK(!pauseCalls&&T::g_rows.empty());break;
    case 5:Begin();++objects[Actor].serial;T::OnDisconnect();CHECK(!resumeCalls&&T::g_debt.empty());break;
    case 6:Begin();++objects[Class].serial;T::OnDisconnect();CHECK(!resumeCalls&&T::g_debt.empty());break;
    case 7:wrongField=true;Begin();CHECK(!pauseCalls);break;
    case 8:for(int cause=0;cause<2;++cause){Reset();if(cause)wrongTimerName=true;else timerSize=4;Begin();CHECK(!pauseCalls);}break;
    case 9:objects[Actor].timer=0;Begin();CHECK(!pauseCalls);break;
    case 10:objects[Actor].exists=false;Begin();CHECK(!pauseCalls&&T::g_rows.empty());break;
    case 11:objects[Actor].active=false;Begin();CHECK(!pauseCalls&&T::g_rows.empty());break;
    case 12:objects[Actor].paused=true;Begin();T::OnDisconnect();CHECK(!pauseCalls&&!resumeCalls&&PausedNow());break;
    case 13:for(int fn=Exists;fn<=Resume;++fn){Reset();missing=fn;Begin();CHECK(!pauseCalls);}Reset();library=false;Begin();CHECK(!pauseCalls);
        Reset();objects[Library].cls=At(ForeignClass);Begin();CHECK(!pauseCalls);
        Reset();Begin();for(int repeat=0;repeat<10;++repeat){now+=100;T::Tick();}CHECK(libraryLookups==1&&pauseCalls==1);
        ++objects[LibraryClass].serial;now+=100;T::Tick();CHECK(libraryLookups==2&&pauseCalls==1);T::OnDisconnect();CHECK(resumeCalls==1&&libraryLookups==2);break;
    case 14:for(const wchar_t* slot:{L"WorldContextObject",L"Handle",L"ReturnValue"})for(int size:{-1,3}){Reset();badSlot=slot;badSize=size;Begin();CHECK(!pauseCalls);}
        Reset();badSlot=L"Handle";badSize=8;badOffset=0;Begin();CHECK(!pauseCalls);Reset();frameSize=70000;Begin();CHECK(!pauseCalls);break;
    case 15:Begin();CHECK(objects[Actor].timer==123&&objects[Actor].remaining==.23f&&PausedNow());break;
    case 16:pauseEffect=false;Begin();CHECK(pauseCalls==1&&!PausedNow());pauseEffect=true;now+=100;T::Tick();CHECK(pauseCalls==2&&PausedNow());break;
    case 17:fail=Pause;Begin();CHECK(pauseCalls==1&&PausedNow()&&T::g_rows.size()==1);T::OnDisconnect();CHECK(resumeCalls==1&&!PausedNow()&&T::g_debt.empty());break;
    case 18:for(int cause=0;cause<3;++cause){Reset();Begin();if(cause==0)++serial;if(cause==1)session.mode=coop::net::Role::Host;if(cause==2)session.linked=false;
        T::Install(&session);CHECK(resumeCalls==1&&!PausedNow());}break;
    case 19:When(Exists,[]{T::OnDisconnect();});Begin();CHECK(!pauseCalls&&T::g_rows.empty()&&T::g_debt.empty());
        for(int nth=1;nth<=40;++nth){Reset();M::State state;CHECK(M::Inspect(At(Actor),state,[]{return true;}));int calls=0;bool before=false,dispatched=false;
            M::Pause(state.lease,dispatched,[&]{if(++calls==nth){before=pauseCalls==0;objects[Actor].timer=456;}return true;});if(before)CHECK(!pauseCalls&&!dispatched);}
        for(int nth=2;nth<=30;++nth){Reset();M::State state;state.lease.handle=999;int calls=0;bool retired=false;
            const bool read=M::Inspect(At(Actor),state,[&]{if(++calls==nth){++generation;retired=true;}return true;});if(retired)CHECK(!read&&state.lease.handle==999);}break;
    case 20:When(Pause,[]{T::Tick();T::OnDisconnect();});Begin();CHECK(pauseCalls==1&&resumeCalls==1&&!PausedNow()&&T::g_debt.empty());
        T::Install(nullptr);CHECK(resumeCalls==1);break;
    case 21:instances.clear();for(int actor=1;actor<=300;++actor)instances.push_back(At(actor));Begin();CHECK(pauseCalls==256&&T::g_rows.size()==256);
        now+=100;T::Tick();CHECK(pauseCalls==256);T::OnDisconnect();CHECK(resumeCalls==256&&T::g_debt.empty());break;
    case 22:objects[Actor].remaining=.137f;Begin();now+=50000;T::Tick();CHECK(objects[Actor].remaining==.137f&&objects[Actor].timer==123&&pauseCalls==1);
        T::OnDisconnect();CHECK(objects[Actor].remaining==.137f&&objects[Actor].timer==123&&resumeCalls==1);break;
    case 23:Begin();CHECK(PausedNow());T::OnDisconnect();CHECK(resumeCalls==1&&!PausedNow());T::OnDisconnect();CHECK(resumeCalls==1);break;
    case 24:for(int cause=0;cause<3;++cause){Reset();Begin();if(cause==0)objects[Actor].timer=456;if(cause==1)objects[Actor].exists=false;if(cause==2)objects[Actor].paused=false;
        T::OnDisconnect();CHECK(!resumeCalls&&T::g_debt.empty());}
        for(int nth=1;nth<=40;++nth){Reset();M::State state;CHECK(M::Inspect(At(Actor),state,[]{return true;}));bool dispatched=false;
            CHECK(M::Pause(state.lease,dispatched,[]{return true;})==M::Result::Done);int calls=0;bool before=false;
            M::Resume(state.lease,[&]{if(++calls==nth){before=resumeCalls==0;objects[Actor].timer=456;}return true;});if(before)CHECK(!resumeCalls);}break;
    case 25:Begin();resumeEffect=false;T::OnDisconnect();CHECK(resumeCalls==1&&PausedNow()&&T::g_debt.size()==1);resumeEffect=true;
        T::Install(nullptr);CHECK(resumeCalls==2&&!PausedNow()&&T::g_debt.empty());
        Reset();Begin();++world;T::OnDisconnect();CHECK(!resumeCalls&&T::g_debt.empty());
        Reset();When(Pause,[]{missing=Paused;});Begin();CHECK(PausedNow()&&T::g_rows.size()==1);T::OnDisconnect();CHECK(!resumeCalls&&T::g_debt.size()==1);
        missing=-1;T::Install(nullptr);CHECK(resumeCalls==1&&!PausedNow()&&T::g_debt.empty());
        Reset();Begin();wrongField=true;now+=100;T::Tick();CHECK(T::g_rows.size()==1&&PausedNow());T::OnDisconnect();CHECK(T::g_debt.size()==1&&!resumeCalls);
        wrongField=false;T::Install(nullptr);CHECK(resumeCalls==1&&!PausedNow()&&T::g_debt.empty());break;
    default:CHECK(false);}
}
}
int main(){for(int id=1;id<=25;++id){test::Group(id);std::printf("PASS llama recovery %d\n",id);}return 0;}
