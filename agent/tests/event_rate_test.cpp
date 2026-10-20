#include "../../src/votv-coop/src/coop/world/event_rate.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Obj { int id=0,serial=1; bool live=true; void* world=nullptr; void* cls=nullptr; const wchar_t* name=L""; float interval=600.f; } obj[20];
float rate=2.f; uint32_t gen=1,serial=1; void* world=reinterpret_cast<void*>(1);
int writes=0; bool writeOk=true; std::function<void()> onRead, onWrite;
ue_wrap::ufunction_hook::PostNativeCallback hook=nullptr;
void Tick() { coop::event_rate::g_scan={}; }
void Native(Obj& actor,float interval) { actor.interval=interval; if(hook)hook(&actor,&actor,nullptr); }
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p) { return p ? static_cast<test::Obj*>(p)->id : -1; }
int32_t SlotSerial(int32_t i) { return test::obj[i].serial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
void* ObjectAt(int32_t i) { return &test::obj[i]; }
bool IsLiveByIndex(void* p,int32_t i) { return i>=0&&i<20&&p==&test::obj[i]&&test::obj[i].live; }
bool IsLive(void* p) { return p&&static_cast<test::Obj*>(p)->live; }
void* ClassOf(void* p) { return static_cast<test::Obj*>(p)->cls; }
std::wstring ClassNameOf(void* p) { return static_cast<test::Obj*>(static_cast<test::Obj*>(p)->cls)->name; }
void* FindDispatchFunctionCached(void*,const wchar_t* n) { return !wcscmp(n,L"GetActorTickInterval")?&test::obj[16]:&test::obj[17]; }
int32_t FunctionFrameSize(void*) { return sizeof(float); }
std::vector<ParamInfo> FunctionParams(void* fn) { ParamInfo p{};p.name=fn==&test::obj[16]?L"ReturnValue":L"TickInterval";p.offset=0;return {p}; }
bool CallFunction(void* p,void* fn,void* data) {
    if(fn==&test::obj[16]) { *static_cast<float*>(data)=static_cast<test::Obj*>(p)->interval;
        auto cb=std::move(test::onRead);test::onRead={};if(cb)cb();return true; }
    if(!test::writeOk)return false;
    ++test::writes;static_cast<test::Obj*>(p)->interval=*static_cast<float*>(data);
    if(test::hook)test::hook(p,p,nullptr);
    auto cb=std::move(test::onWrite);test::onWrite={};if(cb)cb();return true;
}
}
namespace ue_wrap::facts { Resolved Resolve(const Function& row) { return Resolved(&test::obj[17],&row); } }
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n) { for(int i=10;i<14;++i)if(!wcscmp(test::obj[i].name,n))return &test::obj[i];return nullptr; }
size_t ForEachInstance(void* cls,InstanceFn cb,void* ctx) {
    size_t count=0;for(int i=0;i<8;++i)if(test::obj[i].live&&test::obj[i].cls==cls){cb(ctx,&test::obj[i],i);++count;}return count;
}
}
namespace ue_wrap::ufunction_hook { bool InstallPostHook(const facts::Resolved&,PostNativeCallback cb,bool) {test::hook=cb;return true;} }
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::gen;} void* CurrentWorld(){return test::world;}
void* WorldOf(void* p){return p?static_cast<test::Obj*>(p)->world:nullptr;}
WorldKind CurrentWorldKind(){return WorldKind::Gameplay;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace coop::config {float ResolveFloat(int){return test::rate;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long CurrentThreadId(){return 0;}unsigned long long CallerRva(const void*){return 0;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}

int main(){
    using namespace test;namespace E=coop::event_rate;
    for(int i=0;i<20;++i){obj[i].id=i;obj[i].cls=&obj[i];}
    for(int i=0;i<4;++i){obj[10+i].name=E::kSpawners[i];obj[i].cls=&obj[10+i];obj[i].world=world;}
    coop::net::Session s;E::Install(&s);
    for(int i=0;i<4;++i)CHECK(obj[i].interval==300.f);
    const int initial=writes;Tick();E::Install(&s);CHECK(writes==initial); // no compounding
    Native(obj[0],800.f);CHECK(obj[0].interval==400.f);
    Native(obj[0],400.f);CHECK(obj[0].interval==200.f); // new baseline may equal last applied
    rate=4.f;Tick();E::Install(&s);CHECK(obj[0].interval==100.f&&obj[1].interval==150.f);
    rate=1.f;Tick();E::Install(&s);CHECK(obj[0].interval==400.f&&obj[1].interval==600.f);
    rate=.5f;Tick();E::Install(&s);CHECK(obj[0].interval==800.f);
    rate=std::numeric_limits<float>::quiet_NaN();Native(obj[0],600.f);CHECK(obj[0].interval==300.f);
    rate=2.f;
    // A native interval write on another class, CDO or another world is untouched.
    obj[4].world=world;obj[14].name=L"ticker_insomniacSpawner_C";obj[4].cls=&obj[14];
    Native(obj[4],600.f);CHECK(obj[4].interval==600.f);
    obj[5].cls=&obj[10];Native(obj[5],600.f);CHECK(obj[5].interval==600.f);
    obj[6].cls=&obj[10];obj[6].world=reinterpret_cast<void*>(2);Native(obj[6],600.f);CHECK(obj[6].interval==600.f);
    E::OnDisconnect();CHECK(obj[0].interval==600.f&&obj[1].interval==600.f);
    s.mode=coop::net::Role::Client;E::Install(&s);Native(obj[0],500.f);CHECK(obj[0].interval==500.f);
    s.mode=coop::net::Role::Host;Tick();E::Install(&s);CHECK(obj[0].interval==250.f);
    // Recycled identities never receive a prior tenant's restoration.
    ++obj[0].serial;obj[0].interval=900.f;E::OnDisconnect();CHECK(obj[0].interval==900.f);
    E::Install(&s);CHECK(obj[0].interval==450.f);
    // Late class/actor and rejected setter retry use the original baseline.
    obj[7].cls=&obj[12];obj[7].world=world;writeOk=false;Tick();E::Install(&s);CHECK(obj[7].interval==600.f);
    writeOk=true;Tick();E::Install(&s);CHECK(obj[7].interval==300.f);
    // Ending the session from an engine callback cannot leave a dangling Wait.
    onRead=[] {E::OnDisconnect();};Native(obj[0],700.f);CHECK(E::g_waits.empty());
    Tick();E::Install(&s);onWrite=[] {E::OnDisconnect();};Native(obj[0],1000.f);CHECK(E::g_waits.empty());
    s.active=false;E::Install(&s);Native(obj[0],1200.f);CHECK(obj[0].interval==1200.f);
    std::puts("PASS event rate: host ownership, 2x/default/live edits, native rearm, no compounding, restore/recycle, late actors and reentrant teardown");
}
