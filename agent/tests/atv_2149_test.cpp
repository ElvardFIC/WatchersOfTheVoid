// Production functions extracted unchanged by run_2149.ps1; engine boundaries doubled.
#include "support_2135.h"
#include "coop/interactables/atv_sync.h"
#include "coop/interactables/atv_sync_internal.h"
#include "coop/player/players_registry.h"
#include "coop/net/session.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/trace.h"
#include "ue_wrap/engine/engine.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
namespace test {
void* held=nullptr; bool seated=false, dismountOk=true, destroyOnDismount=false;
ue_wrap::engine::MainPlayerGrabState grab{};
int dismounts=0, floorCalls=0, boxCalls=0, floorMode=0, boxMode=0;
ue_wrap::FVector extent{100,60,50}; ue_wrap::FRotator rotation{};
ue_wrap::FVector sourcePosition{0,0,50},forward{1,0,0};
std::function<void()> onTransform,onBounds,onForward,onFloor,onBox;
void At(std::function<void()>& callback){auto copy=std::move(callback);callback={};if(copy)copy();}
struct Args {void* player;ue_wrap::reflection::FString name;};
int frameSize=sizeof(Args);
}
namespace ue_wrap::reflection {
int32_t FunctionFrameSize(void*){return test::frameSize;}
int32_t FindParamOffset(void*,const wchar_t* n){return !wcscmp(n,L"player")?0:offsetof(test::Args,name);}
}
namespace ue_wrap::atv {
namespace R=ue_wrap::reflection;
bool IsAtv(void* a){return a==&test::actor;}
bool Dismount(void*){++test::dismounts;if(test::dismountOk)test::seated=false;
 if(test::destroyOnDismount)test::actor.live=false;return test::dismountOk;}
bool GetRootTransform(void*,FVector& l,FRotator& r){l=test::sourcePosition;r=test::rotation;test::At(test::onTransform);return true;}
#ifndef ATV_PLACEMENT_SOURCE
#define ATV_PLACEMENT_SOURCE "../../_research/tests_2149/placement.h"
#endif
#include ATV_PLACEMENT_SOURCE
}
namespace ue_wrap::engine {
bool GetActorBounds(void*,bool,FVector& c,FVector& e){c={0,0,50};e=test::extent;test::At(test::onBounds);return true;}
FVector GetActorForwardVector(void*){test::At(test::onForward);return test::forward;}
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& out){out=test::grab;if(test::held)out.grabbingActor=test::held;return true;}
}
namespace ue_wrap::trace {
bool FindFloor(void*,const FVector& a,const FVector& b,FVector& p){
 ++test::floorCalls;CHECK(a.Z==40&&b.Z==-60);
 test::At(test::onFloor);
 if(test::floorMode==1)return false;
 if(test::floorMode==2&&test::floorCalls%5==0)return false; // corner over a pit
 p={a.X,a.Y,0};return true;
}
int BoxBlocked(void*,const FVector& a,const FVector& b,const FVector& half){
 ++test::boxCalls;CHECK(half.X==100&&half.Y==60&&a.Z>=54&&b.Z>=53);
 test::At(test::onBox);
 if(test::boxMode==1)return 1;
 if(test::boxMode==2)return -1;
 return b.Y>0?1:0; // wall on the first side: must try the other side
}
}
namespace coop::atv_sync {
namespace R=ue_wrap::reflection;namespace A=ue_wrap::atv;
std::atomic<coop::net::Session*> g_session{nullptr};
std::unordered_map<std::wstring,AtvEntry> g_atvs;
bool IndexCurrent(){return true;}
bool IsLocalOccupant(void*,void*){return test::seated;}
#ifndef ATV_MOUNT_SOURCE
#define ATV_MOUNT_SOURCE "../../_research/tests_2149/mount.h"
#endif
#include ATV_MOUNT_SOURCE
}
namespace S=coop::atv_sync;
int main(){using namespace test;using V=ue_wrap::script_gate::Verdict;
 coop::net::Session session;S::g_session=&session;
 auto& reg=coop::players::Registry::Get();reg.peer=1;
 grab.grabbingActor=&actor;CHECK(S::IsLocalGrabber(&actor,reg.Local()));grab={};
 S::AtvEntry e{};e.actor=&actor;e.idx=1;e.syncerSlot=2;e.lastGrabbed=true;S::g_atvs[L"atv"]=e;
 wchar_t sit[]=L"sit",other[]=L"upgrades";
 Args args{reg.Local(),{sit,4,4}};ue_wrap::script_gate::Call c{};
 c.object=&actor;c.function=&ui;c.locals=reinterpret_cast<uint8_t*>(&args);
 CHECK(S::OnMountPre(c)==V::Cancel);
 c.fromOurCode=true;CHECK(S::OnMountPre(c)==V::Cancel); // safety applies to indirect native use too
 args.name={other,9,9};CHECK(S::OnMountPre(c)==V::Run); // maintenance not blocked
 args.name={sit,4,4};args.player=&ui;CHECK(S::OnMountPre(c)==V::Run);
 args.player=reg.Local();frameSize=1;CHECK(S::OnMountPre(c)==V::Run);frameSize=sizeof(Args);
 S::g_atvs[L"atv"].syncerSlot=0xFF;CHECK(S::OnMountPre(c)==V::Run);
 held=&actor;CHECK(S::OnMountPre(c)==V::Cancel);session.linked=false;CHECK(S::OnMountPre(c)==V::Run);
 session.linked=true;held=nullptr;
 // The old AcceptIncomingOccupancy/authorSlot model was removed. Exercise the
 // current stamped-seat query instead; host authority is outside this fixture.
 S::g_atvs[L"atv"].occupantSlot=2;CHECK(S::OnMountPre(c)==V::Cancel);
 S::g_atvs[L"atv"].occupantSlot=reg.LocalPeerId();CHECK(S::OnMountPre(c)==V::Run);
 S::g_atvs[L"atv"].occupantSlot=0xFF;
 S::g_atvs[L"atv"].lastGrabbed=false;CHECK(S::OnMountPre(c)==V::Run);
 ue_wrap::FVector location{};
 CHECK(ue_wrap::atv::FindCompanionLocation(&actor,location));
 CHECK(location.Y==-145&&location.X==0&&location.Z==54); // nearby supported opposite bay
 for(int mode=1;mode<=2;++mode){boxMode=mode;CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));}
 boxMode=0;
 for(int mode=1;mode<=2;++mode){floorMode=mode;floorCalls=0;CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));}
 floorMode=0;rotation.Roll=45;CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));rotation={};
 for(float yaw:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
  rotation.Yaw=yaw;const int queries=floorCalls+boxCalls;
  CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));CHECK(floorCalls+boxCalls==queries);
 }
 rotation={};
 extent.X=std::numeric_limits<float>::quiet_NaN();CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));
 extent={100,60,50};
 // No query or output may outlive the original actor/world, even on a blocked bay.
 for(auto* boundary:{&onTransform,&onBounds,&onForward,&onFloor,&onBox}){
  for(int mode=0;mode<3;++mode){
   actor.live=true;sourcePosition={0,0,50};rotation={};floorCalls=boxCalls=0;location={7,8,9};
   *boundary=[mode]{if(mode==0)++actor.serial;else if(mode==1)++world;else actor.live=false;};
   CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));
   CHECK(location.X==7&&location.Y==8&&location.Z==9);
   CHECK(floorCalls<=5&&boxCalls<=1);
  }
 }
 actor.live=true;
 for(int axis=0;axis<6;++axis){
  sourcePosition={0,0,50};rotation={};location={7,8,9};
  onFloor=[axis]{switch(axis){case 0:sourcePosition.X+=2;break;case 1:sourcePosition.Y+=2;break;
   case 2:sourcePosition.Z+=2;break;case 3:rotation.Pitch+=2;break;case 4:rotation.Yaw+=2;break;case 5:rotation.Roll+=2;break;}};
  CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));CHECK(location.X==7&&location.Y==8&&location.Z==9);
 }
 sourcePosition={0,0,50};rotation={};forward.Z=std::numeric_limits<float>::quiet_NaN();
 const int queries=floorCalls+boxCalls;CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));CHECK(floorCalls+boxCalls==queries);
 puts("PASS ATV mount gates, parking geometry, retired actor/world and changed source pose");
}
