#include "../../src/votv-coop/src/coop/items/gravity_motion.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Actor { void* gun=nullptr; int id=0; bool live=true; int serial=1; };
Actor prop{nullptr,1},player{nullptr,2},gun{nullptr,3},component{nullptr,4},puppet{nullptr,5},orphanGun{nullptr,6};
uint32_t world=1;
bool parked=false,held=false,frozen=false,stat=false,atv=false,door=false,result=false;
int wakes=0,pre=0,post=0;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a) { return a ? static_cast<test::Actor*>(a)->id : -1; }
int32_t SlotSerial(int32_t i) { return i==1 ? test::prop.serial : 1; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLiveByIndex(void* a,int32_t i) { return a && static_cast<test::Actor*>(a)->live && InternalIndexOf(a)==i; }
int32_t InstanceOffset::Of(void*) { return int32_t(offsetof(test::Actor,gun)); }
void* OuterOf(void* a) { return a==&test::component ? &test::prop : a==&test::orphanGun ? &test::puppet : a==&test::gun ? &test::player : nullptr; }
void* FindClass(const wchar_t*) { return &test::gun; }
void* FindFunction(void*,const wchar_t*) { return &test::gun; }
void* FindDispatchFunctionCached(void*,const wchar_t*) { return &test::gun; }
}
namespace ue_wrap::world_identity {
void* CurrentWorld() { return reinterpret_cast<void*>(uintptr_t(test::world)); }
void* WorldOf(void*) { return CurrentWorld(); }
uint32_t Generation() { return test::world; }
}
namespace ue_wrap::ufunction_hook {
bool InstallPreHook(void*,PreNativeCallback,bool) { ++test::pre;return true; }
bool InstallPostHook(void*,PostNativeCallback,bool) { ++test::post;return true; }
void* CurrentResult() { return &test::result; }
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::script_gate { bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn) { return true; } }
namespace ue_wrap::prop {
bool IsDescendantOfProp(void* a) { return a==&test::prop; }
bool IsStatic(void*) { return test::stat; }
bool IsFrozen(void*) { return test::frozen; }
std::wstring GetInteractableKeyString(void*) { return L"prop"; }
}
namespace ue_wrap::atv { bool IsAtv(void*) { return test::atv; } }
namespace ue_wrap::kitchen_machine { void* DoorParent(void*) { return test::door ? &test::prop : nullptr; } }
namespace ue_wrap::engine { bool SetComponentSimulatePhysics(void*,bool) { ++test::wakes;return true; } }
namespace coop::remote_prop { bool IsActorUnderAnyDrive(void*) { return test::held; } }
namespace coop::prop_drive_stream { bool IsParked(void*) { return test::parked; } }
using namespace coop;
using namespace coop::gravity_motion;
int main() {
    net::Session session;
    test::player.gun=&test::gun;players::Registry::Get().local=&test::player;
    Tick(session);CHECK(test::pre==1 && test::post==2);Tick(session);CHECK(test::post==2);
    players::Registry::Get().puppet.actor=&test::puppet;
    ue_wrap::script_gate::Call tick{};tick.object=&test::gun;
    CHECK(GunTick(tick)==ue_wrap::script_gate::Verdict::Run);
    tick.object=&test::orphanGun;
    CHECK(GunTick(tick)==ue_wrap::script_gate::Verdict::Cancel);
    // The native graph consumes input and stores obj on whichever component runs
    // first. Exercise both tick orders: only the local component may consume it.
    for (bool orphanFirst:{false,true}) {
        bool input=true;void* owner=nullptr;
        for (int i=0;i<2;++i) {
            tick.object=((i==0)==orphanFirst) ? &test::orphanGun : &test::gun;
            if (GunTick(tick)==ue_wrap::script_gate::Verdict::Run && input) { input=false;owner=tick.object; }
        }
        CHECK(owner==&test::gun);
    }
    Force(&test::component,&test::player,nullptr);CHECK(!Target()); // only local gun bytecode
    test::parked=true;Query(&test::component,&test::gun,nullptr);CHECK(test::result);
    Force(&test::component,&test::gun,nullptr);CHECK(Target()==&test::prop && test::wakes==1);
    g_until=active_drive::NowMs()-1;CHECK(Target()==&test::prop); // low-FPS shot survives until sent
    NoteStreamed(&test::player);CHECK(Target()==&test::prop);
    NoteStreamed(&test::prop);CHECK(!Target());
    bool* excluded[]={&test::held,&test::frozen,&test::stat,&test::atv,&test::door};
    for (auto* flag:excluded) {
        *flag=true;test::result=false;
        Force(&test::component,&test::gun,nullptr);Query(&test::component,&test::gun,nullptr);
        CHECK(!Target() && !test::result);*flag=false;
    }
    Force(&test::component,&test::gun,nullptr);++test::prop.serial;CHECK(!Target());
    Force(&test::component,&test::gun,nullptr);CHECK(Target());++test::world;CHECK(!Target());
    Tick(session);CHECK(!Target());Force(&test::component,&test::gun,nullptr);CHECK(Target());
    OnDisconnect();CHECK(!Target());test::result=false;Query(&test::component,&test::gun,nullptr);CHECK(!test::result);
    std::puts("PASS: production gravity hooks, source filter, parked wake/query, low-FPS shot, exclusions, serial and world");
}
