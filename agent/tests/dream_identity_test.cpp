#ifndef PUPPET_IDENTITY_SOURCE
#define PUPPET_IDENTITY_SOURCE "../../src/votv-coop/src/coop/player/puppet_identity.cpp"
#endif
#include PUPPET_IDENTITY_SOURCE
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)

namespace test {
namespace SG = ue_wrap::script_gate;
struct Object {
    int id = 0, serial = 1, actorOffset = 0, frameSize = 2*sizeof(void*);
    bool live = true;
} objects[16];
struct Watch { const ue_wrap::facts::Function* row; int tag; SG::PreFn pre; SG::PostFn post; };
std::vector<Watch> watches;
bool namesReady = false;
int parameterQueries = 0, collected = 0, destroyed = 0;
constexpr const wchar_t* kClass = L"dreamBurger_coll_C";
constexpr const wchar_t* kHandler =
    L"BndEvt__Capsule_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature";
bool Dispatch(Object& function, void* first, void* second = nullptr,
              const wchar_t* cls = kClass, const wchar_t* name = kHandler, bool withLocals = true) {
    struct Frame { void* first; void* second; } frame{first,second};
    SG::Call call{}; call.object = &objects[0]; call.function = &function;
    call.locals = withLocals ? reinterpret_cast<uint8_t*>(&frame) : nullptr;
    const Watch* selected = nullptr;
    if (namesReady) for (const auto& watch : watches) {
        if (!wcscmp(watch.row->Class(), cls) && !wcscmp(watch.row->Name(), name)) {
            CHECK(watch.row->GetMode() == ue_wrap::facts::Mode::Declared);
            selected = &watch; call.tag = watch.tag;
            if (watch.pre && watch.pre(call) == SG::Verdict::Cancel) return false;
            break;
        }
    }
    // The native handler's two effects: consume the local collection entry and
    // destroy its actor. awoken() rewards the local player only after all entries
    // were consumed; refusing a remote touch must leave both effects untouched.
    ++collected; ++destroyed;
    if (selected && selected->post) selected->post(call);
    return true;
}
}

namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p) { return p ? static_cast<test::Object*>(p)->id : -1; }
int32_t SlotSerial(int32_t i) { return test::objects[i].serial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLiveByIndex(void* p, int32_t i) {
    return i >= 0 && i < 16 && p == &test::objects[i] && test::objects[i].live;
}
int32_t FindParamOffset(void* fn, const wchar_t* name) {
    CHECK(!wcscmp(name,L"OtherActor")); ++test::parameterQueries;
    return static_cast<test::Object*>(fn)->actorOffset;
}
int32_t FunctionFrameSize(void* fn) { return static_cast<test::Object*>(fn)->frameSize; }
}
namespace ue_wrap::world_identity {
void* WorldOf(void*) { return nullptr; }
void* CurrentWorld() { return nullptr; }
}
namespace ue_wrap::facts { Resolved ResolveOn(const Function&,void*) { return {}; } }
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t*) { return nullptr; }
uint64_t ClassListingId(void*) { return 0; }
uint64_t ClassUnlistVersion() { return 0; }
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function& row,int tag,PreFn pre,PostFn post) {
    test::watches.push_back({&row,tag,pre,post}); return true;
}
void ResolvePendingNames() { test::namesReady = true; }
int PendingNameCount() { return test::namesReady ? 0 : 1; }
bool NameWatchLive(const facts::Function&,int) { return test::namesReady; }
bool NameWatchSettled(const facts::Function&,int) { return test::namesReady; }
uintptr_t ScopeBitFor(const void*) { return 1; }
uintptr_t ScopeBits(void*) { return 0; }
void MarkScope(const Call&,uintptr_t) {}
bool IsBodyActive(void*,void*) { return false; }
}
namespace coop::players {
bool Registry::IsPuppetBody(void* actor) { return PeerIdOfBody(actor) != kPeerIdUnknown; }
uint8_t Registry::PeerIdOfBody(void* actor) {
    for (const auto& body : bodies) if (body.actor == actor) {
        const auto* object = static_cast<const test::Object*>(actor);
        if (object->live && object->serial == body.serial) return body.slot;
    }
    return kPeerIdUnknown;
}
}
namespace coop::puppet_identity::scan {
void InstallPump(int,int) {}
void Rearm(int) {}
bool Resolve(uint64_t) { return false; }
void ArmWindow(Window,bool) {}
bool WindowArmed(Window) { return false; }
void Clear() {}
}

int main() {
    using namespace test;
    namespace PI = coop::puppet_identity;
    auto& registry = coop::players::Registry::Get();
    for (int i=0;i<16;++i) objects[i].id=i;
    Object& function=objects[8];
    Object& local=objects[1]; Object& puppet=objects[2];
    Object& ragdoll=objects[3]; Object& nonplayer=objects[4];
    registry.bodies={{&puppet,puppet.serial,1},{&ragdoll,ragdoll.serial,1}};
    coop::net::Session session;
    PI::Install(&session); CHECK(PI::WatchesLive()==0);
    PI::Tick(); CHECK(PI::WatchesLive()==20);
    const auto registrations=watches.size(); PI::Install(&session); CHECK(watches.size()==registrations);
    for (const auto role:{coop::net::Role::Host,coop::net::Role::Client}) {
        session.mode=role;
        const auto before=collected;
        CHECK(!Dispatch(function,&puppet)); CHECK(!Dispatch(function,&ragdoll));
        CHECK(collected==before && destroyed==before); // no local progress/reward from remote touch
        CHECK(Dispatch(function,&local)); CHECK(Dispatch(function,&nonplayer));
        CHECK(Dispatch(function,nullptr)); // preserve native behavior beyond the registered deny-list
        CHECK(collected==before+3 && destroyed==collected);
        CHECK(Dispatch(function,&puppet,nullptr,L"unrelated_C"));
        CHECK(Dispatch(function,&puppet,nullptr,kClass,L"ReceiveTick"));
    }
    // Missing call data fails open without memorising a missing offset.
    const auto queries=parameterQueries;
    CHECK(Dispatch(objects[9],&puppet,nullptr,kClass,kHandler,false)); CHECK(parameterQueries==queries);
    CHECK(!Dispatch(objects[9],&puppet));
    objects[10].actorOffset=-1; CHECK(Dispatch(objects[10],&puppet));
    objects[11].actorOffset=sizeof(void*); CHECK(!Dispatch(objects[11],&local,&puppet));
    // Declared parameter bytes must bound the read. The physical test buffer has
    // a second pointer, but this malformed function does not declare room for it.
    // Without the production bound it reads that puppet and cancels unrelated work.
    objects[12].actorOffset=sizeof(void*); objects[12].frameSize=sizeof(void*);
    CHECK(Dispatch(objects[12],&local,&puppet));
    objects[12].frameSize=2*sizeof(void*); CHECK(!Dispatch(objects[12],&local,&puppet));
    objects[13].frameSize=0; CHECK(Dispatch(objects[13],&puppet));
    objects[13].frameSize=-1; CHECK(Dispatch(objects[13],&puppet));
    objects[13].frameSize=sizeof(void*); CHECK(!Dispatch(objects[13],&puppet));
    // A loaded replacement, including a UFunction address reused in place, must
    // resolve its own parameter layout instead of reading the old OtherActor.
    CHECK(!Dispatch(function,&puppet)); ++function.serial; function.actorOffset=sizeof(void*);
    CHECK(!Dispatch(function,&local,&puppet)); CHECK(Dispatch(function,&puppet,&local));
    function.actorOffset=-1; ++function.serial; CHECK(Dispatch(function,&puppet));
    function.actorOffset=0; ++function.serial; CHECK(!Dispatch(function,&puppet));
    // Registry identity is consulted at each call: replacement or retired bodies
    // must not keep their former body's deny-list entry.
    ++puppet.serial; CHECK(Dispatch(function,&puppet));
    registry.bodies[0].serial=puppet.serial; CHECK(!Dispatch(function,&puppet));
    registry.bodies.clear(); CHECK(Dispatch(function,&puppet));
    registry.bodies={{&puppet,puppet.serial,2}}; CHECK(!Dispatch(function,&puppet));
    PI::OnDisconnect(); registry.bodies.clear(); PI::Install(&session); PI::Tick();
    CHECK(Dispatch(function,&local)); CHECK(Dispatch(function,&puppet));
    int rows=0; const auto* counts=PI::Counters(rows); bool saw=false;
    for(int i=0;i<rows;++i) if(!std::strcmp(counts[i].row,"dreamBurger")) {
        saw=true; CHECK(counts[i].refused>=10);
    }
    CHECK(saw);
    std::puts("PASS production dream burger native-body gate: host/client puppet and ragdoll refusal, local/nonplayer preservation, watch readiness, malformed frames, function replacement, recycled offsets and body retirement");
}
