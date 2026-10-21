#include "../../src/votv-coop/src/coop/world/event_cue_sync.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Obj { int id = 0, serial = 1; bool live = true; uint32_t world = 0; } obj[10];
uint32_t world = 1, session = 1;
bool assets = true, callOk = true, fields = true, thread = true, enumerating = false;
int plays = 0, attempts = 0, frameSize = 8;
std::vector<void*> components;
std::function<void()> onPlay, onFrame;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p) { return p ? static_cast<test::Obj*>(p)->id : -1; }
void* ObjectAt(int32_t i) { return i >= 0 && i < 10 ? &test::obj[i] : nullptr; }
int32_t SlotSerial(int32_t i) { return i >= 0 && i < 10 ? test::obj[i].serial : 0; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
bool IsLiveByIndex(void* p,int32_t i) { return p && p == ObjectAt(i) && IsLive(p); }
void* ClassOf(void* p) { return p; }
void* FindClassDefaultObject(const wchar_t*) { return &test::obj[2]; }
void* FindObject(const wchar_t*,const wchar_t*) { return test::assets ? &test::obj[3] : nullptr; }
void* FindFunction(void*,const wchar_t* name) { return &test::obj[!wcscmp(name,L"runEvent") ? 4 : 5]; }
int32_t FindParamOffset(void*,const wchar_t*) { return 0; }
int32_t FunctionFrameSize(void* fn) { return fn == &test::obj[4] ? test::frameSize : 64; }
std::vector<ParamInfo> FunctionParams(void*) {
    const wchar_t* names[]{L"WorldContextObject",L"EmitterTemplate",L"Location",L"Rotation",L"Scale",L"bAutoDestroy",L"PoolingMethod",L"bAutoActivateSystem"};
    const int offsets[]{0,8,16,28,40,52,53,54};
    std::vector<ParamInfo> out;
    for (int i = 0; i < (test::fields ? 8 : 7); ++i) { ParamInfo p{}; p.name=names[i];p.offset=offsets[i];out.push_back(p); }
    auto cb=std::move(test::onFrame);test::onFrame={};if(cb)cb();return out;
}
bool CallFunction(void*,void* fn,void* data) {
    ++test::attempts;
    CHECK(fn==&test::obj[5]);
    auto* bytes=static_cast<uint8_t*>(data);
    void* context=nullptr;void* asset=nullptr;FVector pos{},scale{};
    std::memcpy(&context,bytes,8);std::memcpy(&asset,bytes+8,8);
    std::memcpy(&pos,bytes+16,12);std::memcpy(&scale,bytes+40,12);
    CHECK(context==&test::obj[0]&&asset==&test::obj[3]&&pos.Z==6000&&scale.X==1&&bytes[52]&&bytes[54]&&!bytes[53]);
    ++test::plays;auto cb=std::move(test::onPlay);test::onPlay={};if(cb)cb();return test::callOk;
}
}
namespace ue_wrap::fname_utils { reflection::FName StringToFName(const std::wstring&) { return {1,0}; } }
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t*) { return &test::obj[6]; }
size_t ForEachInstance(void*,InstanceFn fn,void* ctx) {
    test::enumerating=true;for(void* p:test::components)fn(ctx,p,reflection::InternalIndexOf(p));test::enumerating=false;return test::components.size();
}
}
namespace ue_wrap::engine { void* GetParticleSystemTemplate(void*) { return &test::obj[3]; } }
namespace ue_wrap::world_identity {
uint32_t Generation() { return test::world; }
void* CurrentWorld() { return reinterpret_cast<void*>(uintptr_t(test::world)); }
void* WorldOf(void* p) { return p ? reinterpret_cast<void*>(uintptr_t(static_cast<test::Obj*>(p)->world)) : nullptr; }
}
namespace coop::net::session_serial { uint32_t Current() { return test::session; } }
namespace ue_wrap::game_thread { bool IsGameThread() { return test::thread; } }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function&,int,PreFn,PostFn) { return true; }
void ResolvePendingNames() {} bool NameWatchLive(const facts::Function&,int) { return true; }
}
namespace coop::world_sound_sync { void Install(net::Session*) {} void OnDisconnect() {} bool OnReliable(const net::EventCuePayload& p) { return p.cueId>=1&&p.cueId<=5; } }
int main() {
    using namespace test; namespace E=coop::event_cue_sync; using coop::net::Role;
    for(int i=0;i<10;++i)obj[i].id=i;
    obj[0].world=obj[1].world=obj[7].world=obj[8].world=world;
    coop::players::Registry::Get().local=&obj[0];
    coop::net::Session s;E::Install(&s);
    ue_wrap::reflection::FName row{1,0};ue_wrap::script_gate::Call fire{};
    fire.object=&obj[1];fire.function=&obj[4];fire.locals=reinterpret_cast<uint8_t*>(&row);
    s.ok[1]=false;E::OnRunEventPost(fire);CHECK(s.sent.size()==1&&E::g_out[1].queue.size()==1);
    s.ok[1]=true;E::Install(&s);CHECK(s.sent.size()==2&&E::g_out[1].queue.empty());E::Install(&s);CHECK(s.sent.size()==2);
    s.sent.clear();s.ready[2]=false;E::OnRunEventPost(fire);CHECK(s.sent.size()==1);
    fire.locals=nullptr;E::OnRunEventPost(fire);fire.locals=reinterpret_cast<uint8_t*>(&row);
    frameSize=1;E::OnRunEventPost(fire);frameSize=8;CHECK(s.sent.size()==1);
    s.ok[1]=false;E::OnRunEventPost(fire);++s.generation[1];s.ok[1]=true;E::Install(&s);CHECK(s.sent.size()==1);
    components={&obj[7],&obj[8]};obj[8].world=world+1;
    s.sent.clear();s.onSend=[]{CHECK(!enumerating);};E::QueueConnectBroadcastForSlot(1);CHECK(s.sent.size()==1);
    s.sent.clear();s.ok[1]=false;E::QueueConnectBroadcastForSlot(1);CHECK(E::g_out[1].queue.size()==1);
    ++obj[7].serial;s.ok[1]=true;E::Install(&s);CHECK(s.sent.empty());
    s.ok[1]=false;E::OnRunEventPost(fire);++world;s.ok[1]=true;E::Install(&s);CHECK(s.sent.empty());
    obj[0].world=obj[1].world=obj[7].world=world;
    E::OnDisconnect();s.mode=Role::Client;E::Install(&s);
    coop::net::EventCuePayload p{0,0,0,6000};assets=false;E::g_cueTemplates[0].Reset();
    E::OnReliable(p);CHECK(plays==0&&E::g_in.size()==1);assets=true;E::Install(&s);CHECK(plays==1&&E::g_in.empty());
    coop::players::Registry::Get().local=nullptr;E::OnReliable(p);CHECK(E::g_in.size()==1);
    coop::players::Registry::Get().local=&obj[0];E::Install(&s);CHECK(plays==2&&E::g_in.empty());
    fields=false;++obj[5].serial;E::OnReliable(p);CHECK(plays==2&&E::g_in.size()==1);
    fields=true;++obj[5].serial;E::Install(&s);CHECK(plays==3&&E::g_in.empty());
    ++obj[3].serial;assets=false;E::OnReliable(p);CHECK(E::g_in.size()==1&&plays==3);
    assets=true;E::Install(&s);CHECK(plays==4&&E::g_in.empty());
    callOk=false;E::OnReliable(p);CHECK(plays==5&&E::g_in.empty());callOk=true;E::Install(&s);CHECK(plays==5);
    ++obj[5].serial;onFrame=[]{++obj[0].serial;};E::OnReliable(p);CHECK(plays==5&&E::g_in.size()==1);
    E::Install(&s);CHECK(plays==6&&E::g_in.empty());
    onPlay=[&]{E::OnReliable(p);};E::OnReliable(p);CHECK(plays==8&&E::g_in.empty());
    onPlay=[]{E::OnDisconnect();};E::OnReliable(p);CHECK(plays==9&&E::g_in.empty());E::Install(&s);
    assets=false;E::g_cueTemplates[0].Reset();E::OnReliable(p);++session;assets=true;E::Install(&s);CHECK(plays==9&&E::g_in.empty());
    assets=false;E::g_cueTemplates[0].Reset();E::OnReliable(p);E::g_in.front().expires={};assets=true;E::Install(&s);CHECK(plays==9&&E::g_in.empty());
    p.x=std::numeric_limits<float>::quiet_NaN();E::OnReliable(p);p.x=3'000'000;E::OnReliable(p);p={99,0,0,0};E::OnReliable(p);CHECK(plays==9);
    E::OnDisconnect();puts("PASS event cues: peer retries/readiness, bounded frames, join source identity/world, deferred assets/pawn/frames, cached asset reuse, no ambiguous duplicate, reentrancy and session/expiry cleanup");
}
