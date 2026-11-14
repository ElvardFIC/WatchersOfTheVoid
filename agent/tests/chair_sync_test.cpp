// Real chair controller and IsSitting predicate; engine/transport boundaries doubled.
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/engine/world_identity.h"
#include "coop/player/roster_ledger.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <cstring>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true,chair=false;void* cls=nullptr;void* world=nullptr;
 ue_wrap::FVector location{};ue_wrap::FRotator rotation{};void* component=nullptr;std::wstring key;int rotations=0,moves=0,shifts=0,clears=0;bool sitting=false;} objects[64];
uint32_t generation=1,sessionSerial=1;uintptr_t worldToken=123;uint64_t now=1000;
void* seat=nullptr;bool resolved=true,readLocation=true,readRotation=true,readSeat=true,readRagdoll=true,ragdoll=false,dead=false,sleeping=false,computer=false;
std::function<void()> onLocation,onRotation,onKey,onSeat,onApply,onMove,onComponentLocation,onComponentRotation,onClear;
bool moveOk=true,rotationOk=true,componentLocationOk=true,componentRotationOk=true;
ue_wrap::FVector forward{1,0,0};void* driverAtvs[8]{};uint8_t driverSlots[8]{};int driverCount=0;
std::array<coop::roster_ledger::Row,32> people;
Obj& O(void* p){return *static_cast<Obj*>(p);}
void At(std::function<void()>& f){auto copy=std::move(f);f={};if(copy)copy();}
}
#ifndef CHAIR_SYNC_SOURCE
#define CHAIR_SYNC_SOURCE "../../src/votv-coop/src/coop/player/sit_pose.cpp"
#endif
#include CHAIR_SYNC_SOURCE
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<64&&p==&test::objects[i]&&test::objects[i].live;}
void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
std::wstring ClassNameOf(void* p){return p&&test::O(p).chair?L"prop_sitOfficeChair_C":L"prop_box_C";}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::worldToken);}
void* WorldOf(void* p){return p?test::O(p).world:nullptr;}
}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void*){return true;}
std::wstring GetInteractableKeyString(void* p){auto key=test::O(p).key;test::At(test::onKey);return key;}
}
namespace ue_wrap::engine {
bool ReadMainPlayerSittingOn(void*,void*& out){out=test::seat;test::At(test::onSeat);return test::readSeat;}
bool ReadMainPlayerRagdollState(void*,bool& ragdoll,bool& dead){ragdoll=test::ragdoll;dead=test::dead;return test::readRagdoll;}
bool TryGetActorLocation(void* p,FVector& out){out=test::O(p).location;test::At(test::onLocation);return test::readLocation;}
bool TryGetActorRotation(void* p,FRotator& out){out=test::O(p).rotation;test::At(test::onRotation);return test::readRotation;}
bool SetActorRotation(void* p,const FRotator& r){if(test::rotationOk)test::O(p).rotation=r;++test::O(p).rotations;test::At(test::onApply);return test::rotationOk;}
bool SetActorLocation(void* p,const FVector& loc){if(test::moveOk)test::O(p).location=loc;++test::O(p).moves;test::At(test::onMove);return test::moveOk;}
bool TryGetComponentLocation(void* p,FVector& out){out=test::O(p).location;test::At(test::onComponentLocation);return test::componentLocationOk;}
bool TryGetComponentWorldRotation(void* p,FRotator& out){out=test::O(p).rotation;test::At(test::onComponentRotation);return test::componentRotationOk;}
FVector GetComponentLocation(void*){return {};}FRotator GetComponentWorldRotation(void*){return {};}
FVector GetActorForwardVector(void*){return test::forward;}
}
namespace ue_wrap::device_screen {
void* ReadActiveInterface(void*){return test::computer?&test::objects[62]:nullptr;}
std::wstring ClassifyWidgetClaimKey(void* p){return p?L"laptop":L"";}
}
namespace ue_wrap::sleep {bool IsSleeping(){return test::sleeping;}}
namespace ue_wrap::sit_pose {
void DriveSitting(void* p,bool value){test::O(p).sitting=value;}
void ClearMeshShift(void* p){++test::O(p).clears;test::At(test::onClear);}
void ShiftMesh(void* p,float,float){++test::O(p).shifts;}
float ModelHead(void*){return 20;}float ModelHalfLength(void*){return 80;}
void* AtvDriverSeat(void* atv){return test::O(atv).component;}
}
namespace ue_wrap::atv {bool EnsureResolved(){return false;}bool IsAtv(void*){return false;}}
namespace coop::atv_sync {bool LocalIsDriving(){return false;}
int CopyIndexed(void** atvs,uint8_t* slots,int max){const int n=(std::min)(max,test::driverCount);for(int i=0;i<n;++i){atvs[i]=test::driverAtvs[i];slots[i]=test::driverSlots[i];}return n;}
void* ActorSeatedBy(uint8_t slot){for(int i=0;i<test::driverCount;++i)if(test::driverSlots[i]==slot)return test::driverAtvs[i];return nullptr;}}
namespace coop::puppet_drive {coop::RemotePlayer& Puppet(int slot){return *coop::players::Registry::Get().Puppet(slot);}}
namespace coop::prop_element_tracker {
void* ResolveLiveActorByKey(const std::wstring& key,void*){return test::resolved&&key==L"chair"?&test::objects[1]:nullptr;}
}
namespace coop::active_drive {uint64_t NowMs(){return test::now;}}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace coop::roster_ledger {const Row& Get(int slot){return test::people.at(slot);}}
namespace test {
namespace S=coop::sit_pose;
coop::net::Session session,other;
void Reset(){
 onLocation=onRotation=onKey=onSeat=onApply=onMove=onComponentLocation=onComponentRotation=onClear={};coop::element::callback={};
 S::OnDisconnect();session={};other={};++generation;++sessionSerial;++worldToken;now=1000;
 for(int i=0;i<64;++i){int serial=objects[i].serial+1;objects[i]={};objects[i].id=i;objects[i].serial=serial;
  objects[i].cls=&objects[63];objects[i].world=reinterpret_cast<void*>(worldToken);}
 objects[63].world=nullptr;objects[1].chair=objects[2].chair=true;objects[1].key=L"chair";objects[2].key=L"other";
 auto& registry=coop::players::Registry::Get();registry.peer=0;registry.local=&objects[0];
 for(int i=0;i<32;++i){registry.puppets[i].actor=&objects[i+3];registry.puppets[i].curStateBits_=coop::net::kStateBitSitting;registry.puppets[i].viewRefreshes=0;people[i].playerNo=uint16_t(i+1);}
 seat=nullptr;resolved=readLocation=readRotation=readSeat=readRagdoll=true;ragdoll=dead=sleeping=computer=false;
 coop::element::reach=coop::element::body=coop::element::target=true;
 moveOk=rotationOk=componentLocationOk=componentRotationOk=true;forward={1,0,0};driverCount=0;S::Install(&session);
}
coop::net::SitAnchorPayload Packet(float yaw=0,float x=0){
 coop::net::SitAnchorPayload p{};p.active=1;p.x=x;p.y=20;p.z=30;p.yaw=yaw;p.chairYaw=yaw;
 coop::net::WireKeyFromString(L"chair",p.chairKey);return p;
}
coop::net::SitAnchorPayload Sent(){coop::net::SitAnchorPayload p{};CHECK(!session.sent.empty());std::memcpy(&p,session.sent.back().data.data(),sizeof(p));return p;}
void Small(){
 Reset();seat=&objects[1];S::Tick();CHECK(session.sent.size()==1);
 now+=50;objects[0].location.X=2;S::Tick();CHECK(session.sent.size()==2&&Sent().x==2);
 now+=50;objects[0].rotation.Yaw=.6f;S::Tick();CHECK(session.sent.size()==3);
 now+=20;objects[0].rotation.Yaw=5;S::Tick();CHECK(session.sent.size()==3);
 now+=30;session.sendOk=false;S::Tick();CHECK(session.sent.size()==3);
 objects[0].rotation.Yaw=8;session.sendOk=true;S::Tick();CHECK(session.sent.size()==4&&Sent().yaw==8);
}
void Axes(){
 for(int axis=0;axis<3;++axis){Reset();seat=&objects[1];S::Tick();now+=50;
  if(axis==0)objects[1].rotation.Pitch=3;else if(axis==1)objects[1].rotation.Yaw=3;else objects[1].rotation.Roll=3;
  S::Tick();CHECK(session.sent.size()==2);auto p=Sent();CHECK(p.chairPitch+p.chairYaw+p.chairRoll==3);
 }
 Reset();seat=&objects[1];objects[1].rotation.Yaw=179;S::Tick();now+=50;objects[1].rotation.Yaw=-179;S::Tick();CHECK(session.sent.size()==2);
 now+=1000;S::Tick();CHECK(session.sent.size()==3);
}
void Interpolation(){
 Reset();auto p=Packet(179);S::OnAnchor(1,p);S::DriveRemotes();CHECK(objects[4].rotation.Yaw==179&&objects[1].rotation.Yaw==179);
 now+=50;p=Packet(-179,10);S::OnAnchor(1,p);S::DriveRemotes();CHECK(objects[4].location.X==0);
 now+=25;S::DriveRemotes();CHECK(std::fabs(std::fabs(objects[4].rotation.Yaw)-180)<.01f&&objects[4].location.X==5);
 CHECK(std::fabs(ue_wrap::NormalizeAxis(objects[4].rotation.Yaw-objects[1].rotation.Yaw))<.01f);
 p=Packet(-160,20);S::OnAnchor(1,p);S::DriveRemotes();CHECK(objects[4].location.X==5);
 now+=50;S::DriveRemotes();CHECK(objects[4].location.X==20&&objects[4].rotation.Yaw==-160);
}
void Off(){
 Reset();auto p=Packet(60);S::OnAnchor(1,p);S::DriveRemotes();const int rotations=objects[1].rotations;
 p.active=0;p.chairYaw=120;S::OnAnchor(1,p);S::DriveRemotes();CHECK(!S::g_anchor[1].on&&objects[1].rotations==rotations&&objects[1].rotation.Yaw==60);
 Reset();seat=&objects[1];S::Tick();seat=nullptr;session.sendOk=false;S::Tick();CHECK(S::g_lastChair.Raw());
 session.sendOk=true;S::Tick();CHECK(!Sent().active&&!Sent().chairKey.len&&!S::g_lastChair.Raw());
}
void Authority(){
 Reset();auto p=Packet(60);coop::element::reach=false;S::OnAnchor(1,p);S::DriveRemotes();CHECK(!S::g_anchor[1].on&&!objects[4].moves);
 coop::element::reach=true;seat=&objects[1];S::OnAnchor(1,p);CHECK(!S::g_anchor[1].on);seat=nullptr;
 objects[1].chair=false;S::OnAnchor(1,p);CHECK(!S::g_anchor[1].on);objects[1].chair=true;
 coop::element::callback=[&]{p.x=999;};S::OnAnchor(1,p);S::DriveRemotes();CHECK(objects[4].location.X==0);
 Reset();p=Packet();coop::element::callback=[]{S::OnPeerLeft(1);};S::OnAnchor(1,p);CHECK(!S::g_anchor[1].on);
}
void Late(){
 Reset();resolved=false;auto p=Packet(80);S::OnAnchor(1,p);S::DriveRemotes();CHECK(S::g_anchor[1].on&&!S::g_anchor[1].ready&&!objects[4].moves);
 resolved=true;S::DriveRemotes();CHECK(S::g_anchor[1].ready&&objects[4].rotation.Yaw==80);
 Reset();coop::players::Registry::Get().puppets[1].actor=nullptr;S::OnAnchor(1,p);S::DriveRemotes();CHECK(!objects[4].moves);
 coop::players::Registry::Get().puppets[1].actor=&objects[4];S::DriveRemotes();CHECK(objects[4].rotation.Yaw==80);
}
void Lifetime(){
 for(int mode=0;mode<10;++mode){Reset();auto p=Packet(40);S::OnAnchor(1,p);S::DriveRemotes();int moved=objects[4].moves;
  switch(mode){case 0:++generation;break;case 1:++worldToken;break;case 2:++sessionSerial;break;case 3:session.mode=coop::net::Role::Client;break;
   case 4:session.active=false;break;case 5:session.linked=false;break;case 6:++people[1].playerNo;break;case 7:++session.peerEpoch;break;
   case 8:++objects[1].serial;break;case 9:now+=3001;break;}
  S::DriveRemotes();CHECK(!S::g_anchor[1].on&&objects[4].moves==moved);
 }
 Reset();auto p=Packet(35);S::OnAnchor(31,p);S::OnAnchor(32,p);S::DriveRemotes();CHECK(objects[34].rotation.Yaw==35&&!objects[4].moves);
}
void Sample(){
 for(int mode=0;mode<10;++mode){Reset();seat=&objects[1];
  switch(mode){case 0:readLocation=false;break;case 1:readRotation=false;break;case 2:objects[1].key=std::wstring(32,L'a');break;
   case 3:objects[1].key=L"None";break;case 4:objects[1].key=L"\u0404";break;
   case 5:objects[0].location.Z=std::numeric_limits<float>::quiet_NaN();break;case 6:objects[1].rotation.Pitch=std::numeric_limits<float>::infinity();break;
   case 7:onLocation=[]{++objects[0].serial;};break;case 8:onRotation=[]{++objects[1].serial;};break;case 9:onKey=[]{objects[1].key=L"rebound";};break;}
  S::Tick();CHECK(session.sent.empty());
 }
}
void Transitions(){
 for(int mode=0;mode<5;++mode){Reset();seat=&objects[1];S::Tick();
  switch(mode){case 0:seat=nullptr;break;case 1:ragdoll=true;break;case 2:dead=true;break;case 3:sleeping=true;break;case 4:computer=true;break;}
  S::Tick();CHECK(!S::LocalIsSitting()&&!Sent().active&&!objects[0].sitting&&objects[0].clears);
 }
 for(auto bits:{uint8_t(0),coop::net::kStateBitSleeping,coop::net::kStateBitRagdoll,coop::net::kStateBitComputer}){
  Reset();auto p=Packet(40);S::OnAnchor(1,p);S::DriveRemotes();int moves=objects[4].moves;
  coop::players::Registry::Get().puppets[1].curStateBits_=coop::net::kStateBitSitting|bits;
  if(!bits)coop::players::Registry::Get().puppets[1].curStateBits_=0;
  S::DriveRemotes();CHECK(!S::g_anchor[1].on&&objects[4].moves==moves);
 }
 Reset();auto p=Packet();S::OnAnchor(1,p);onApply=[]{S::OnPeerLeft(1);};S::DriveRemotes();CHECK(!objects[4].moves);
}
void Completion(){
 for(int mode=0;mode<8;++mode){Reset();seat=&objects[1];session.allowSend=[mode](int,coop::net::ReliableKind,const void*,int){
  switch(mode){case 0:S::OnDisconnect();break;case 1:S::Install(&other);break;case 2:++generation;break;
   case 3:seat=&objects[2];break;case 4:++objects[0].serial;break;case 5:++objects[1].serial;break;
   case 6:objects[1].cls=&objects[62];break;case 7:++objects[63].serial;break;}return true;};
  S::Tick();CHECK(!S::g_lastChair.Raw());
 }
 Reset();seat=&objects[1];session.allowSend=[](int,coop::net::ReliableKind,const void*,int){S::Tick();return true;};
 S::Tick();CHECK(session.sent.size()==1&&S::g_lastChair.Is(&objects[1]));
}
}
int main(int argc,char** argv){using namespace test;const char* mode=argc>1?argv[1]:"all";
 struct Group{const char* name;void(*run)();}groups[]={{"small",Small},{"axes",Axes},{"interpolation",Interpolation},{"off",Off},{"authority",Authority},
  {"late",Late},{"lifetime",Lifetime},{"sample",Sample},{"transitions",Transitions},{"completion",Completion}};
 bool ran=false;for(const auto& group:groups)if(!std::strcmp(mode,"all")||!std::strcmp(mode,group.name)){group.run();std::printf("PASS chair %s\n",group.name);ran=true;}
 CHECK(ran);return 0;
}
