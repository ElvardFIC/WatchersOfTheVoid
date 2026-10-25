#include "../../src/votv-coop/src/coop/world/event_fire_sync.cpp"
#include "../../src/votv-coop/src/coop/world/event_fire_policy.cpp"
#include "../../src/votv-coop/src/coop/world/event_fire_delivery.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)

namespace test {
struct Obj {
    int id = 0, serial = 1; bool live = true;
    const wchar_t* name = L""; void* cls = nullptr;
    void* save = nullptr; void* eventer = nullptr;
    coop::event_fire_sync::RawArray pass{};
} obj[16];
struct Frame { ue_wrap::reflection::FName event{}, special{}; };
uint32_t world = 1, session = 1;
bool gameplay = true, callsOk = true;
bool frameReady = true;
std::function<void()> insideNative, insideIntern;
int nativeRuns = 0, specialRuns = 0;
std::vector<std::function<void()>> tasks;
struct Watch { ue_wrap::script_gate::PreFn pre; ue_wrap::script_gate::PostFn post; } run{}, special{};
ue_wrap::script_gate::Call Call(void* fn, Frame& frame, void* caller = nullptr, bool ours = false) {
    ue_wrap::script_gate::Call call{}; call.object = &obj[2]; call.function = fn;
    call.locals = reinterpret_cast<uint8_t*>(&frame); call.callerFunction = caller; call.fromOurCode = ours;
    return call;
}
bool Dispatch(void* fn, Frame& frame, void* caller = nullptr, bool ours = false) {
    auto call = Call(fn, frame, caller, ours);
    auto& watch = fn == &obj[3] ? run : special;
    if (watch.pre && watch.pre(call) == ue_wrap::script_gate::Verdict::Cancel) return true;
    if (fn == &obj[3]) ++nativeRuns; else ++specialRuns;
    auto cb = std::move(insideNative);insideNative={};if(cb)cb();
    if (watch.post) watch.post(call); return true;
}
void Drain() { auto pending = std::move(tasks); tasks.clear(); for (auto& task : pending) task(); }
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p) { return p ? static_cast<test::Obj*>(p)->id : -1; }
int32_t SlotSerial(int32_t i) { return test::obj[i].serial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
void* ObjectAt(int32_t i) { return &test::obj[i]; }
bool IsLiveByIndex(void* p, int32_t i) { return i >= 0 && i < 16 && p == &test::obj[i] && test::obj[i].live; }
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
void* ClassOf(void* p) { return static_cast<test::Obj*>(p)->cls; }
std::wstring ClassNameOf(void*) { return L"trigger_eventer_C"; }
std::wstring ToString(const FName& n) { return n.ComparisonIndex == 1 ? L"arirGraff_0" : L"solar"; }
int32_t FindPropertyOffset(void*, const wchar_t*) { return -1; }
void* FindFunction(void*, const wchar_t*) { return nullptr; }
int32_t FindParamOffset(void*, const wchar_t*) { return 0; }
int32_t FunctionFrameSize(void*) { return test::frameReady ? sizeof(test::Frame) : 0; }
std::vector<ParamInfo> FunctionParams(void* fn) {
    ParamInfo a{}, b{}; a.name = fn == &test::obj[3] ? L"event" : L"eventName1";
    a.offset = 0; b.name = L"special"; b.offset = sizeof(FName); return {a,b};
}
bool CallFunction(void*, void* fn, void* data) {
    return test::callsOk && test::Dispatch(fn, *static_cast<test::Frame*>(data), nullptr, true);
}
}
namespace ue_wrap::fname_utils { reflection::FName StringToFName(const std::wstring& n) { auto cb=std::move(test::insideIntern);test::insideIntern={};if(cb)cb();return {n == L"arirGraff_0" ? 1 : 2,0}; } }
namespace ue_wrap::object_index { void* ClassByName(const wchar_t*) { return nullptr; } }
namespace ue_wrap::world_singleton { void* Gamemode() { return &test::obj[0]; } }
namespace ue_wrap::world_identity {
uint32_t Generation() { return test::world; }
void* CurrentWorld() { return nullptr; }
void* WorldOf(void*) { return nullptr; }
WorldKind CurrentWorldKind() { return test::gameplay ? WorldKind::Gameplay : WorldKind::Unknown; }
}
namespace coop::net::session_serial { uint32_t Current() { return test::session; } }
namespace ue_wrap::game_thread {
bool IsGameThread() { return true; }
bool IsDefinitelyOffGameThread() { return false; }
void Post(std::function<void()> task) { test::tasks.push_back(std::move(task)); }
}
namespace ue_wrap::hot_path { unsigned long CurrentThreadId() { return 0; } unsigned long long CallerRva(const void*) { return 0; } }
namespace ue_wrap::engine {
bool TryGetActorLocation(void*, FVector&) { return false; }
bool TryGetActorRotation(void*, FRotator&) { return false; }
}
namespace coop::teleport_client { void GatherOthersTo(uint8_t,float,float,float,float) {} }
namespace coop::event_schedule_authority { void Install(net::Session*) {} void OnDisconnect() {} }
namespace coop::event_location_sync { bool IsLocationRow(const char*) { return false; } }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function& row,int,PreFn pre,PostFn post) {
    (!wcscmp(row.Name(),L"runEvent") ? test::run : test::special) = {pre,post}; return true;
}
void ResolvePendingNames() {}
bool NameWatchLive(const facts::Function&,int) { return true; }
bool NameWatchSettled(const facts::Function&,int) { return true; }
bool IsEnabled() { return true; }
}

int main() {
    using namespace test; namespace E = coop::event_fire_sync; namespace D = E::delivery;
    for (int i = 0; i < 16; ++i) { obj[i].id = i; obj[i].cls = &obj[15]; }
    obj[0].save = &obj[1]; obj[0].eventer = &obj[2];
    E::g_resolved = true; E::g_offSaveSlot = offsetof(Obj,save); E::g_offEventer = offsetof(Obj,eventer);
    E::g_gmClass.Set(&obj[15]); E::g_slotClass.Set(&obj[15]); E::g_eventerClass.Set(&obj[15]);
    E::g_offPassEvents = offsetof(Obj,pass); E::g_offEventParam = E::g_offSpecialParam = 0;
    E::g_runEventFn = &obj[3]; E::g_runSpecialEventFn = &obj[4]; E::g_settimeFn = &obj[5];
    coop::net::Session s; E::Install(&s);
    Frame f{{1,0},{}};
    // A native nested prank roll publishes the chosen case once per peer;
    // repeated same-name specials remain distinct occurrences.
    Dispatch(&obj[4],f,&obj[6]); Dispatch(&obj[4],f,&obj[6]);
    CHECK(s.sent.size() == 4 && specialRuns == 2);
    for (const auto& sent : s.sent) { auto* p = reinterpret_cast<const coop::net::EventFirePayload*>(sent.bytes.data());
        CHECK(p->dispatch == 1 && std::string(p->name) == "arirGraff_0"); }
    s.sent.clear(); CHECK(E::HostFire(E::FireKind::RunEvent,L"solar",L"None")); Drain();
    CHECK(nativeRuns == 1 && s.sent.size() == 2); // F1 direct reflected dispatch
    s.sent.clear(); f.event={2,0}; Dispatch(&obj[3],f,&obj[5],true);
    CHECK(s.sent.size() == 2); // reflected-clock nested scheduler body

    // A refused send retries in order, without duplicating a healthy recipient.
    s.sent.clear(); s.ok[1] = false; f.event={1,0}; Dispatch(&obj[4],f); f.event={2,0}; Dispatch(&obj[4],f);
    CHECK(s.sent.size() == 2 && s.sent[0].slot == 2 && s.sent[1].slot == 2);
    s.ok[1] = true; E::Install(&s); CHECK(s.sent.size() == 4);
    CHECK(std::string(reinterpret_cast<const coop::net::EventFirePayload*>(s.sent[2].bytes.data())->name) == "arirGraff_0");
    E::Install(&s); CHECK(s.sent.size() == 4);
    s.sent.clear(); s.ok[1]=false; Dispatch(&obj[4],f); ++s.generation[1]; s.ok[1]=true; E::Install(&s);
    CHECK(s.sent.size()==1); // replaced slot never inherits old fires
    s.sent.clear(); s.ok[1]=false; Dispatch(&obj[4],f); ++world; s.ok[1]=true; E::Install(&s); CHECK(s.sent.size()==1);
    s.sent.clear(); s.ok[1]=false; Dispatch(&obj[4],f); ++session; s.ok[1]=true; E::Install(&s); CHECK(s.sent.size()==1);
    // A send callback ending the session cannot invalidate the active drain.
    s.sent.clear(); s.onSend=[] { D::Clear(); }; Dispatch(&obj[4],f); CHECK(s.sent.size()==1);
    E::Install(&s); s.sent.clear(); s.onSend=[] { ++world; }; Dispatch(&obj[4],f); CHECK(s.sent.size()==1);

    E::OnDisconnect(); s.mode=coop::net::Role::Client; E::Install(&s); s.sent.clear();
    const int before = specialRuns; Dispatch(&obj[4],f); CHECK(specialRuns==before && s.sent.empty());
    CHECK(!E::HostFire(E::FireKind::SpecialEvent,L"arirGraff_0",L"None"));
    CHECK(E::DevProbeClientRefusal());
    coop::net::EventFirePayload payload{}; payload.dispatch=1; std::strcpy(payload.name,"arirGraff_0");
    E::OnReliable(payload); E::OnReliable(payload); CHECK(specialRuns==before+2 && s.sent.empty());
    // A join-window fire retries without needing another packet or world-ready edge.
    obj[0].eventer=nullptr; E::OnReliable(payload); CHECK(!E::g_pending.empty()); obj[0].eventer=&obj[2];
    E::g_pending.front().lastAttempt={}; E::Install(&s); CHECK(E::g_pending.empty() && specialRuns==before+3);
    // World changes discard queued fires, while history override remains native.
    gameplay=false; E::OnReliable(payload); CHECK(!E::g_pending.empty()); ++world; gameplay=true; E::Install(&s);
    CHECK(E::g_pending.empty() && specialRuns==before+3);
    payload.dispatch=9; E::OnReliable(payload); CHECK(specialRuns==before+3);
    const char* lane=nullptr; CHECK(E::ReplayVerdict("earthTp",&lane)==0 && E::ReplayVerdict("unknown",&lane)==-1);
    payload.dispatch=1;
    frameReady=false;++obj[4].serial;E::OnReliable(payload);CHECK(E::g_pending.size()==1);
    frameReady=true;++obj[4].serial;E::g_pending.front().lastAttempt={};E::Install(&s);CHECK(E::g_pending.empty());
    int seen=specialRuns;insideNative=[&]{E::OnReliable(payload);CHECK(specialRuns==seen+1);};
    E::OnReliable(payload);CHECK(specialRuns==seen+2&&E::g_pending.empty());
    seen=specialRuns;std::function<void()> again;
    again=[&]{insideNative=again;E::OnReliable(payload);};insideNative=again;
    E::OnReliable(payload);CHECK(specialRuns==seen+8&&E::g_pending.size()==1);
    insideNative={};E::Install(&s);CHECK(specialRuns==seen+9&&E::g_pending.empty());
    seen=specialRuns;insideNative=[]{E::OnDisconnect();};E::OnReliable(payload);
    CHECK(specialRuns==seen+1&&E::g_pending.empty()&&E::g_replayed.empty());
    E::Install(&s);insideNative=[&]{E::OnDisconnect();E::Install(&s);E::OnReliable(payload);};
    E::OnReliable(payload);CHECK(E::g_pending.size()==1);
    seen=specialRuns;E::Install(&s);CHECK(specialRuns==seen+1&&E::g_pending.empty());
    seen=specialRuns;insideIntern=[]{E::OnDisconnect();};E::OnReliable(payload);
    CHECK(specialRuns==seen&&E::g_pending.empty());
    E::Install(&s);payload.dispatch=0;std::strcpy(payload.name,"solar");
    const auto prior=nativeRuns;insideNative=[&]{E::OnReliable(payload);};E::OnReliable(payload);
    CHECK(nativeRuns==prior+1&&E::g_pending.empty());
    E::OnDisconnect();E::Install(&s);callsOk=false;E::OnReliable(payload);callsOk=true;
    E::OnReliable(payload);CHECK(nativeRuns==prior+1); // ambiguous dispatch is consumed once
    E::OnDisconnect();E::Install(&s);gameplay=false;E::OnReliable(payload);CHECK(!E::g_pending.empty());
    ++session;gameplay=true;E::Install(&s);CHECK(E::g_pending.empty()&&nativeRuns==prior+1);
    E::OnDisconnect(); std::puts("PASS event dispatch, chosen/repeated specials, ordered retries, peer/world/session cleanup and client replay gates");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
const FName& NameOf(void*){static const FName name{};return name;}
}
