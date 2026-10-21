#include "../../src/votv-coop/src/ue_wrap/world/event_location.cpp"
#include "../../src/votv-coop/src/coop/world/event_location_sync.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "facts_test_support.h"
namespace ue_wrap::facts {Resolved Resolve(const Function&){return {};}}
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr,*outer=nullptr;const wchar_t* name=L"";
 void* eventer=nullptr,*controller=nullptr,*box=nullptr,*component=nullptr,*anti=nullptr;
 struct Arr{void** data=nullptr;int32_t num=0,max=0;}filter;
 uint8_t active=0;ue_wrap::FTransform transform{};ue_wrap::FVector extent{100,100,100};}o[16];
uint32_t world=1,sessionId=1;uint64_t now=1000;int fires=0,gathers=0,rotations=0,gatherTicks=0;uint8_t anchor=255;bool result=false,callOk=true,fireOk=true;
ue_wrap::FVector localPos{500,500,0},gatherPos{};std::function<void()> onGather,onPose,onReadBox,onTickGather;
struct Frame{ue_wrap::FVector center{},extent{100,100,100};void* entrant=nullptr;}frame;
ue_wrap::ufunction_hook::CallerFrame caller{};
std::vector<std::pair<std::wstring,std::wstring>> watches;
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
void* OuterOf(void* p){return static_cast<test::Obj*>(p)->outer;}void* SuperStructOf(void*){return nullptr;}
const FName& NameOf(void* p){static FName n;n={static_cast<test::Obj*>(p)->id,0};return n;}
bool NameEquals(const FName& n,const wchar_t* name){return !wcscmp(test::o[n.ComparisonIndex].name,name);}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::o[i].serial;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<16?&test::o[i]:nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<16&&p==&test::o[i]&&IsLive(p);}
int32_t FindPropertyOffset(void* object,const wchar_t* name){
 if(object==&test::o[10]||object==&test::o[12]) {
  if(!wcscmp(name,L"CallFunc_K2_GetComponentLocation_ReturnValue"))return offsetof(test::Frame,center);
  if(!wcscmp(name,L"CallFunc_GetScaledBoxExtent_ReturnValue"))return offsetof(test::Frame,extent);
  if(!wcscmp(name,L"K2Node_ComponentBoundEvent_OtherActor"))return offsetof(test::Frame,entrant);return -1;
 }
 if(!wcscmp(name,L"eventer"))return offsetof(test::Obj,eventer);
 if(!wcscmp(name,L"event_vent"))return offsetof(test::Obj,controller);
 if(!wcscmp(name,L"event_agrav"))return offsetof(test::Obj,anti);
 if(!wcscmp(name,L"triggerBox"))return offsetof(test::Obj,box);
 if(!wcscmp(name,L"Box"))return offsetof(test::Obj,component);
 if(!wcscmp(name,L"filter"))return offsetof(test::Obj,filter);
 if(!wcscmp(name,L"BoxExtent"))return offsetof(test::Obj,extent);return -1;
}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,active);mask=1;return !wcscmp(n,L"isActive");}
int32_t FunctionFrameSize(void* fn){return fn==&test::o[10]||fn==&test::o[12]?sizeof(test::Frame):sizeof(FTransform);}
std::vector<ParamInfo> FunctionParams(void* fn){ParamInfo p{};p.name=fn==&test::o[8]?L"ReturnValue":L"allIndex";p.offset=0;return {p};}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return !wcscmp(name,L"K2_GetComponentToWorld")?&test::o[8]:&test::o[9];}
void* FindFunction(void*,const wchar_t*){return &test::o[8];}
bool CallFunction(void* actor,void* fn,void* data){if(!test::callOk)return false;
 if(fn==&test::o[8]){std::memcpy(data,&static_cast<test::Obj*>(actor)->transform,sizeof(FTransform));auto cb=std::move(test::onReadBox);test::onReadBox={};if(cb)cb();}
 else {if(!test::fireOk)return false;CHECK(*static_cast<int32_t*>(data)==-1);++test::fires;}return true;}
}
namespace ue_wrap::world_singleton {void* Gamemode(){return &test::o[0];}}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionId;}}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& p){p=test::localPos;auto cb=std::move(test::onPose);test::onPose={};if(cb)cb();return true;}
bool TryGetActorRotation(void*,FRotator& r){++test::rotations;r={0,25,0};return true;}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return &test::o[8];}}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* c,const wchar_t* f,int,PreFn,PostFn){test::watches.emplace_back(c,f);return true;}
void ResolvePendingNames(){}
}
namespace ue_wrap::ufunction_hook {bool InstallPostHook(const facts::Resolved&,PostNativeCallback,bool){return true;}CallerFrame CurrentCallerFrame(){return test::caller;}void* CurrentResult(){return &test::result;}}
namespace coop::active_drive {uint64_t NowMs(){return test::now;}}
namespace coop::teleport_client {
void TickGather(){++test::gatherTicks;auto cb=std::move(test::onTickGather);test::onTickGather={};if(cb)cb();}void ClearGather(){}
void GatherOthersTo(uint8_t slot,float x,float y,float z,float){++test::gathers;test::anchor=slot;test::gatherPos={x,y,z};auto cb=std::move(test::onGather);test::onGather={};if(cb)cb();}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){using namespace test;namespace S=coop::event_location_sync;namespace L=ue_wrap::event_location;
 for(int i=0;i<16;++i){o[i].id=i;o[i].cls=&o[i];}
 o[0].eventer=&o[1];o[1].controller=&o[2];o[1].anti=&o[7];
 o[2].name=L"trigger_TBoxActivator_C";o[2].box=&o[3];o[3].name=L"trigger_box_C";o[3].component=&o[4];
 o[10].name=L"ExecuteUbergraph_trigger_agrav";o[10].outer=&o[11];o[11].name=L"trigger_agrav_C";
 o[12].name=L"ExecuteUbergraph_trigger_box";o[12].outer=&o[3];
 auto& reg=coop::players::Registry::Get();reg.local=&o[5];reg.puppets[1].actor=&o[6];reg.puppets[1].pos={0,0,0};
 coop::net::Session s;S::Tick(s);CHECK(fires==0&&gathers==0);
 CHECK(S::IsLocationRow("vent")&&S::IsLocationRow("agrav")&&!S::IsLocationRow("toeStab"));
 // Arm merely prepares the box; client enters later, without a native puppet overlap.
 o[3].active=1;reg.puppets[1].pos={500,0,0};now+=250;S::Tick(s);CHECK(!fires&&!gathers);
 reg.puppets[1].pos={10,20,0};now+=250;S::Tick(s);CHECK(fires==1&&gathers==1&&anchor==1&&gatherPos.X==10);
 now+=250;S::Tick(s);CHECK(fires==1&&gathers==1);
 // Re-arm and actual host overlap: gather's recursive overlap cannot duplicate native effects.
 ue_wrap::script_gate::Call arm{};arm.object=&o[2];S::OnArm(arm);localPos={0,0,0};
 ue_wrap::script_gate::Call hit{};hit.object=&o[3];hit.callerFunction=&o[12];hit.callerLocals=reinterpret_cast<uint8_t*>(&frame);frame.entrant=&o[5];
 onGather=[&]{CHECK(S::OnRunBox(hit)==ue_wrap::script_gate::Verdict::Cancel);};
 CHECK(S::OnRunBox(hit)==ue_wrap::script_gate::Verdict::Run&&gathers==2&&anchor==0);
 // Exact native zero-gravity query: missing client collision is supplied by its valid pose.
 arm.object=&o[7];S::OnArm(arm);localPos={500,500,0};caller={&o[10],reinterpret_cast<uint8_t*>(&frame)};
 result=false;S::OnAgravOverlap(nullptr,&o[7],nullptr);CHECK(result&&gathers==2);
 S::Tick(s);CHECK(gathers==3&&anchor==1);S::OnAgravOverlap(nullptr,&o[7],nullptr);S::Tick(s);CHECK(gathers==3);
 S::OnArm(arm);s.ready[1]=false;result=false;S::OnAgravOverlap(nullptr,&o[7],nullptr);CHECK(!result);
 s.ready[1]=true;reg.puppets[1].pose=false;S::OnAgravOverlap(nullptr,&o[7],nullptr);CHECK(!result);reg.puppets[1].pose=true;
 S::OnAgravOverlap(nullptr,&o[6],nullptr);CHECK(!result);o[10].outer=&o[3];S::OnAgravOverlap(nullptr,&o[7],nullptr);CHECK(!result);o[10].outer=&o[11];
 ++world;S::OnAgravOverlap(nullptr,&o[7],nullptr);CHECK(!result);S::Tick(s);
 s.mode=coop::net::Role::Client;auto count=gathers;now+=250;S::Tick(s);S::OnAgravOverlap(nullptr,&o[7],nullptr);CHECK(gathers==count&&!result);
 // Real geometry: rotation/negative scale, invalid quaternion, NaN and zero extent.
 L::Box b{};b.extent={10,2,3};b.transform.RotZ=.70710678f;b.transform.RotW=.70710678f;b.transform.SX=-2;
 CHECK(L::Contains(b,{0,19,0})&&!L::Contains(b,{3,0,0}));
 b.transform.RotW=0;CHECK(!L::Contains(b,{0,0,0}));b.transform={};b.extent.X=0;CHECK(!L::Contains(b,{0,0,0}));
 b.extent.X=10;CHECK(!L::Contains(b,{std::numeric_limits<float>::quiet_NaN(),0,0}));
 L::Box query{};CHECK(L::ReadAgravQuery(&o[10],reinterpret_cast<uint8_t*>(&frame),query));
 CHECK(!L::ReadAgravQuery(&o[12],reinterpret_cast<uint8_t*>(&frame),query));
 // Retry a failed native activation without teleporting players repeatedly.
 s.mode=coop::net::Role::Host;localPos={500,500,0};arm.object=&o[2];S::OnArm(arm);
 fireOk=false;count=gathers;now+=250;S::Tick(s);CHECK(gathers==count+1);
 now+=250;S::Tick(s);CHECK(gathers==count+1);fireOk=true;auto firedBefore=fires;
 now+=250;S::Tick(s);CHECK(gathers==count+1&&fires==firedBefore+1);
 // Reusing the same session/world must not retain old fired/queued state.
 ++sessionId;CHECK(!S::Current());S::Tick(s);CHECK(S::Current());
 auto prepare=[&]{S::OnDisconnect();S::Tick(s);o[3].active=1;localPos={0,0,0};reg.puppets[1].pos={500,500,0};now+=250;};
 prepare();count=gathers;firedBefore=fires;auto rotated=rotations;
 onPose=[] {S::OnDisconnect();};S::Tick(s);CHECK(gathers==count&&fires==firedBefore&&rotations==rotated);
 prepare();count=gathers;rotated=rotations;onPose=[] {++o[5].serial;};S::Tick(s);
 CHECK(gathers==count&&rotations==rotated);
 prepare();count=gathers;firedBefore=fires;onReadBox=[] {++world;};S::Tick(s);CHECK(gathers==count&&fires==firedBefore);
 S::OnDisconnect();S::Tick(s);count=gatherTicks;onTickGather=[&] {S::Tick(s);};S::Tick(s);CHECK(gatherTicks==count+1);
 // Actual overlap must cancel a stale gather after native pose reentry as well.
 prepare();count=gathers;onPose=[] {S::OnDisconnect();};S::OnRunBox(hit);CHECK(gathers==count);
 S::OnDisconnect();CHECK(!S::Current());
 puts("PASS location events: native query identity, remote occupancy, arm/fire separation, host/client roles, exact initiator, dedupe/reentry, readiness/world and rotated bounds");
}
