// Real player_damage guard and registration, engine/network boundary doubles.
#include "../../src/votv-coop/src/coop/player/player_damage.cpp"
#include "ue_wrap/actors/vitals.h"
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
int a=1,b=2,other=3;
bool accept=true;
int attempts=0;
std::map<std::wstring,ue_wrap::script_gate::PreFn> guards;
}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* cls,const wchar_t* name,int,PreFn pre,PostFn post) {
    CHECK(std::wstring(cls)==L"mainPlayer_C" && pre && !post);
    ++test::attempts;
    if (!test::accept) return false;
    test::guards[name]=pre;return true;
}
bool WatchName(const facts::Function& row,int tag,PreFn pre,PostFn post) {
    if (tag==0x50530002) return WatchClassName(row.Class(),row.Name(),tag,pre,post);
    return true; // Separate damage-delivery registrations are outside this guard fixture.
}
}
namespace coop::config_registry::detail { struct RegistryDef {
    static FlagRow Flag(){return {nullptr,RegistryCtorKey{}};}
}; }
namespace coop::config_registry::rows {const FlagRow player_damage_no_refusal=detail::RegistryDef::Flag();}
namespace coop::config {bool ResolveFlag(const config_registry::FlagRow&){return false;}}
namespace coop::creature_hit {bool IsDelivered(void*){std::abort();}}
namespace coop::element {
Registry& Registry::Get(){std::abort();}
ElementId Registry::EidForActor(void*)const{std::abort();}
}
namespace coop::player_hit {
bool Deliver(uint8_t,const net::PlayerHitPayload&){std::abort();}
uint8_t BoneIndex(const std::wstring&){std::abort();}
bool InApply(){std::abort();}
}
namespace ue_wrap::reflection {
int32_t ParamOffset::Of(void*){std::abort();}
const FName& NameOf(void*){std::abort();}
void* ClassOf(void*){std::abort();}
std::wstring ClassNameOf(void*){std::abort();}
std::wstring ToString(const FName&){std::abort();}
}
namespace ue_wrap::facts {Resolved ResolveOn(const Function&,void*){std::abort();}}
namespace ue_wrap::game_thread {bool RegisterInterceptor(const facts::Resolved&,UFunctionInterceptor){std::abort();}}
namespace ue_wrap::object_index { void* ClassByName(const wchar_t*) { return nullptr; } }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::reflection { void* FindFunction(void*,const wchar_t*) { return nullptr; } }
namespace ue_wrap::game_thread { bool RegisterInterceptor(void*,UFunctionInterceptor) { return true; } }
namespace coop::hand_item { void* LocalHandActor() { return nullptr; } }
namespace coop::pvp {
void Tick() {}
void NoteBodies(void*,void*) {}
bool CancelLocalImpact(void*,void*) { return false; }
void OnPuppetImpact(void*,void*,void*) {}
}
namespace ue_wrap::reflection { bool IsLive(void*) { std::abort(); } }
namespace ue_wrap::engine {
bool InvokeAddPlayerDamage(void*,float,bool) { std::abort(); }
void* GetController(void*) { std::abort(); }
}
namespace ue_wrap::game_thread { void Post(std::function<void()>) { std::abort(); } }
namespace ue_wrap::vitals { bool Read(Field,float*) { std::abort(); } }
int main() {
    using namespace coop::player_damage;
    using ue_wrap::script_gate::Verdict;
    coop::net::Session s;
    auto& reg=coop::players::Registry::Get();
    reg.local=&test::a;
    Install(&s);
    test::accept=false;Tick();CHECK(test::attempts==4 && test::guards.empty());
    test::accept=true;Tick();CHECK(test::attempts==8 && test::guards.size()==4);
    Tick();CHECK(test::attempts==8);
    for (const auto* name:{L"ragdollMode",L"fallen",L"ignite",L"startBurning"}) {
        auto f=test::guards.at(name);
        ue_wrap::script_gate::Call c{};
        CHECK(f(c)==Verdict::Run);
        c.object=&test::a;CHECK(f(c)==Verdict::Run);
        c.object=&test::b;CHECK(f(c)==Verdict::Cancel);
        // The same verdict for engine, interface and internal Blueprint routes.
        c.callerObject=&test::other;c.fromOurCode=true;CHECK(f(c)==Verdict::Cancel);
        // Pawn replacement between ticks: the atomic still names a, but the
        // current registry is authoritative for this GT-only script callback.
        reg.local=&test::b;CHECK(f(c)==Verdict::Run);
        c.object=&test::a;CHECK(f(c)==Verdict::Cancel);
        reg.local=nullptr;CHECK(f(c)==Verdict::Run);
        reg.local=&test::a;c.object=&test::b;
        s.linked=false;CHECK(f(c)==Verdict::Run);s.linked=true;
        Install(nullptr);CHECK(f(c)==Verdict::Run);Install(&s);
    }
    std::puts("PASS: production native-state guards, registration/retry, all routes, pawn swap, boot, solo/disconnect");
}
