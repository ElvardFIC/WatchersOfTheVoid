#include "../../src/votv-coop/src/coop/interactables/atv_steering_sync.cpp"
#include "../../src/votv-coop/src/ue_wrap/devices/atv_steering.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "coop/interactables/atv_sync_internal.h"
#include "coop/interactables/atv_condition_sync.h"
#include "coop/net/wire_key_util.h"
#include <atomic>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int index,serial=1;bool live=true;void* handle=nullptr;ue_wrap::FRotator rotation{};bool driven=false;void* occupant=nullptr;};
Obj objects[]={{0},{1},{2},{3},{4}};
struct Params {ue_wrap::FRotator rotation;bool sweep,teleport;float angle;};
bool fields=true,assets=true,owned=false;uint32_t world=1,serial=1;
int bars=0,wheels=0;float wheelAngle=0;
std::function<void()> callback;
ue_wrap::script_gate::PostFn tick=nullptr;
void Tick(){objects[1].rotation.Yaw=0;ue_wrap::script_gate::Call call{};call.object=&objects[0];tick(call);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->index:-1;}
void* ObjectAt(int32_t i){return i>=0&&i<5?&test::objects[i]:nullptr;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<5?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p;}
int32_t InstanceOffset::Of(void*){if(!test::fields)return -1;if(!wcscmp(prop_,L"handleAxis"))return offsetof(test::Obj,handle);CHECK(!wcscmp(prop_,L"RelativeRotation"));return offsetof(test::Obj,rotation);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){CHECK(!wcscmp(name,L"K2_SetRelativeRotation"));return test::assets?&test::objects[2]:nullptr;}
int32_t FunctionFrameSize(void*){return sizeof(test::Params);}
std::vector<ParamInfo> FunctionParams(void* fn){using P=test::Params;
    if(fn==&test::objects[3])return {{L"A",offsetof(P,angle),4,0}};
    CHECK(fn==&test::objects[2]);return {{L"NewRotation",offsetof(P,rotation),12,0},{L"bSweep",offsetof(P,sweep),1,0},{L"bTeleport",offsetof(P,teleport),1,0}};}
bool CallFunction(void* actor,void* fn,void* raw){auto& p=*static_cast<test::Params*>(raw);
    if(fn==&test::objects[2]){CHECK(actor==&test::objects[1]&&p.rotation.Pitch==20&&p.rotation.Roll==0&&!p.sweep&&!p.teleport);test::objects[1].rotation=p.rotation;++test::bars;}
    else {CHECK(fn==&test::objects[3]&&actor==&test::objects[0]);test::wheelAngle=p.angle;++test::wheels;}
    auto cb=std::move(test::callback);test::callback={};if(cb)cb();return true;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p==&test::objects[0]||p==&test::objects[1]?CurrentWorld():nullptr;}
}
namespace ue_wrap::atv {
bool IsDriven(void* p){return static_cast<test::Obj*>(p)->driven;}
void* GetOccupantPlayer(void* p){return static_cast<test::Obj*>(p)->occupant;}
bool GetRootTransform(void*,FVector& at,FRotator& rot){at={1,2,3};rot={};return true;}
bool GetBrake(void*){return false;}bool GetLights(void*){return false;}bool GetTurbo(void*){return false;}
bool GetRadio(void*){return false;}bool GetHasMap(void*){return false;}uint8_t ReadModuleMask(void*){return 0;}
}
namespace ue_wrap::engine {bool GetActorRootPhysicsVelocity(void*,FVector& lin,FVector& ang){lin={};ang={};return true;}}
namespace coop::atv_condition_sync {void FillPayload(void*,net::AtvStatePayload& p){p.tiresValid=1;}}
namespace coop::atv_sync {
namespace R=ue_wrap::reflection;
namespace A=ue_wrap::atv;
using ue_wrap::FVector;using ue_wrap::FRotator;using coop::net::WireKeyFromString;
bool OwnsTick(void*){return test::owned;}
std::atomic<net::Session*> g_session{nullptr};
std::unordered_map<std::wstring,AtvEntry> g_atvs;
std::unordered_map<void*,std::wstring> g_synthForActor;
bool IsSynthKey(const std::wstring&){return false;}
void SendAtvSpawn(const std::wstring&,void*,int){CHECK(false);}
bool IsPoseAuthor(void*,void*,uint8_t,uint8_t){return false;}
#include "read_payload.inc"
void PublishRadio(net::Session&,void*,const std::wstring&,int,bool){}
#include "connect_snapshot.inc"
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* i,const char* f,const wchar_t* c,const wchar_t* n,Mode m):ident_(i),feature_(f),cls_(c),fn_(n),mode_(m),next_(nullptr){}
Resolved Resolve(const Function& row){return test::assets?Resolved(&test::objects[3],&row):Resolved{};}
}
namespace ue_wrap::script_gate {bool WatchName(const facts::Function&,int,PreFn,PostFn post){test::tick=post;return true;}}
int main(){using namespace test;namespace S=coop::atv_steering_sync;namespace FX=ue_wrap::atv_steering;
    static_assert(sizeof(coop::net::AtvStatePayload)==152&&offsetof(coop::net::AtvStatePayload,steering)==86);
    objects[0].handle=&objects[1];objects[1].rotation.Yaw=-33.25f;
    coop::players::Registry::Get().local=&objects[4];coop::roster_ledger::rows[0].playerNo=1;coop::roster_ledger::rows[1].playerNo=2;
    CHECK(S::Capture(&objects[0])==-3325);fields=false;CHECK(S::Capture(&objects[0])==coop::net::kAtvSteeringUnavailable);fields=true;
    objects[1].rotation.Yaw=45.005f;CHECK(S::Capture(&objects[0])==4500);
    objects[1].rotation.Yaw=std::numeric_limits<float>::quiet_NaN();CHECK(S::Capture(&objects[0])==coop::net::kAtvSteeringUnavailable);
    CHECK(!S::Valid(4501)&&!S::Valid(-4501)&&S::Valid(-4500)&&S::Valid(coop::net::kAtvSteeringUnavailable));
    coop::net::Session session;S::Install(&session);CHECK(tick);
    S::Receive(&objects[0],-3325,1);Tick();CHECK(bars==1&&wheels==1&&objects[1].rotation.Yaw==-33.25f&&wheelAngle==-33.25f);
    Tick();CHECK(bars==2&&objects[1].rotation.Yaw==-33.25f); // native reset must not win between packets
    S::Receive(&objects[0],4500,1);Tick();CHECK(wheelAngle==45);S::Receive(&objects[0],0,1);Tick();CHECK(wheelAngle==0);
    owned=true;const int before=bars;Tick();CHECK(bars==before&&S::g_targets.empty());owned=false;
    // Direct local driver check closes the frame before ownership is published.
    S::Receive(&objects[0],3000,1);objects[0].driven=true;objects[0].occupant=&objects[4];Tick();CHECK(bars==before);objects[0].driven=false;
    // Native dependencies can appear later; another steering packet is unnecessary.
    S::Receive(&objects[0],2000,1);assets=false;Tick();CHECK(bars==before);assets=true;Tick();CHECK(wheelAngle==20);
    ++coop::roster_ledger::rows[1].playerNo;const int replaced=bars;Tick();CHECK(bars==replaced&&S::g_targets.empty());
    S::Receive(&objects[0],1000,1);S::OnPeerLeft(1);Tick();CHECK(bars==replaced);
    S::Receive(&objects[0],1000,1);++objects[0].serial;Tick();CHECK(bars==replaced);
    S::Receive(&objects[0],1000,1);++world;Tick();CHECK(bars==replaced);S::Install(&session);CHECK(S::g_targets.empty());
    S::Receive(&objects[0],1000,0xff);Tick();CHECK(wheelAngle==10); // idle host seed
    S::Receive(&objects[0],coop::net::kAtvSteeringUnavailable,0);CHECK(S::g_targets.empty());
    S::Receive(&objects[0],1200,0);const int wheelBefore=wheels;callback=[]{S::OnDisconnect();};Tick();CHECK(wheels==wheelBefore&&!S::g_session);
    S::Install(&session);S::Receive(&objects[0],1500,0);callback=[&]{S::Receive(&objects[0],-1500,0);};Tick();CHECK(wheels==wheelBefore);Tick();CHECK(wheelAngle==-15);
    S::Receive(&objects[0],-2000,0);++serial;Tick();CHECK(wheelAngle==-15);S::OnDisconnect();
    // A late-join state snapshot must neither replay nor consume a queued honk.
    coop::atv_sync::g_session=&session;coop::atv_sync::AtvEntry entry{};entry.actor=&objects[0];entry.idx=0;entry.honkPulse=true;
    objects[1].rotation.Yaw=25;coop::atv_sync::g_atvs[L"atv"]=entry;
    coop::atv_sync::QueueConnectBroadcastForSlot(1);
    CHECK(coop::atv_sync::g_atvs[L"atv"].honkPulse);
    CHECK(session.atvStates.size()==1&&!(session.atvStates[0].cockpit&coop::net::kAtvCockpitHonk));
    CHECK(session.atvStates[0].adopt&&session.atvStates[0].steering==2500);
    session.blocked=1;coop::atv_sync::QueueConnectBroadcastForSlot(1);CHECK(coop::atv_sync::g_atvs[L"atv"].honkPulse);
    std::puts("PASS ATV steering and join: native tick reset, wheels, driver handoff, readiness, occupancy, teardown and retained honk");
}
