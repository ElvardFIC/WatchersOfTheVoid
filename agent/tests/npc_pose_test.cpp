#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include "coop/net/session.h"
#include "coop/element/registry.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/cached_obj_ref.h"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#ifndef NPC_POSE_SOURCE
#define NPC_POSE_SOURCE "../../src/votv-coop/src/coop/creatures/npc_pose_host.cpp"
#endif
#include NPC_POSE_SOURCE
#include "../../src/votv-coop/src/coop/element/element.cpp"
#include "../../src/votv-coop/src/coop/element/registry.cpp"
namespace test {
namespace P=coop::npc_sync;namespace E=coop::element;namespace N=coop::net;
struct Object {int cls=200;void* root=nullptr;void* held=nullptr;ue_wrap::FVector pos{10,20,30},vel{3,4,0},look{40,50,60};ue_wrap::FRotator rot{5,370,-370};ue_wrap::FVector scale{1,1,1};} objects[256];
bool live[256];int serial[256];
N::Session session,successor;N::Session* active=&session;
uint32_t sessionSerial=1,world=1;int worlds[2],worldIndex=0;
std::vector<std::unique_ptr<E::Element>> owners;
std::function<void(int,void*)> callback;
std::vector<int> trace;
int joins=0,claims=0,releases=0,freezes=0,thaws=0,deaths=0;
bool physicsOkay=true,hand=false,transformOkay=true,hasLook=true,hasState=true,hasDisc=true;
int yawProvider=2;float bodyYaw=375;
void Boundary(int stage,void* actor=nullptr){trace.push_back(stage);if(callback)callback(stage,actor);}
E::Element* Add(int obj=0){auto p=std::make_unique<E::Element>(E::ElementType::Npc);p->SetTypeName("kerfurOmega_C");CHECK(E::Registry::Get().AllocHostId(p.get())!=E::kInvalidId);if(obj>=0)p->SetActor(&objects[obj],obj);auto* v=p.get();owners.push_back(std::move(p));return v;}
void Reset(){P::DropKerfurHolds();owners.clear();for(int i=0;i<256;++i){objects[i]={};objects[i].root=&objects[230];live[i]=true;++serial[i];}session={};successor={};active=&session;++sessionSerial;world=1;worldIndex=0;callback={};trace.clear();joins=claims=releases=freezes=thaws=deaths=0;physicsOkay=transformOkay=hasLook=hasState=hasDisc=true;hand=false;yawProvider=2;bodyYaw=375;}
void Replace(E::ElementId id){
 for(auto& p:owners)if(p&&p->GetId()==id){p.reset();break;}
 for(uint32_t i=0;i<E::kHostRangeSize;++i){auto* v=Add(-1);if(v->GetId()==id){v->SetActor(&objects[0],0);return;}}
 CHECK(false);
}
void Cause(int cause,E::ElementId id){
 if(cause==0)active=&successor;
 if(cause==1){++world;worldIndex=1;}
 if(cause==2)++serial[0];
 if(cause==3)++serial[200];
 if(cause==4)Replace(id);
}
bool Seen(int stage){return std::find(trace.begin(),trace.end(),stage)!=trace.end();}
P::Row ReadRow(E::Element* element){P::Row row;CHECK(E::Registry::Get().ReadIdentity(element->GetId(),row.identity));CHECK(row.actor.Capture(element->GetActor()));return row;}
void Pass(const char* family,int n){std::printf("PASS %s%02d\n",family,n);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<256;++i)if(p==&test::objects[i])return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<256&&p==&test::objects[i]&&test::live[i];}
bool IsLive(void* p){return IsLiveByIndex(p,InternalIndexOf(p));}
int32_t AllocateSlotSerial(int32_t i){return i>=0&&i<256?test::serial[i]:0;}
int32_t SlotSerial(int32_t i){return AllocateSlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<256?&test::objects[i]:nullptr;}
void* ClassOf(void* p){return p?&test::objects[static_cast<test::Object*>(p)->cls]:nullptr;}
int32_t StructSize(void*){return sizeof(test::Object);}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& type){CHECK(!std::wcscmp(name,L"RootComponent"));offset=offsetof(test::Object,root);type=L"ObjectProperty";return true;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return &test::objects[240+(!std::wcscmp(name,L"location")?0:!std::wcscmp(name,L"rotation")?1:2)];}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return &test::worlds[test::worldIndex];}void* WorldOf(void* p){return reflection::InternalIndexOf(p)<100?CurrentWorld():nullptr;}}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::npc_sync {
net::Session* GetSession(){return test::active;}
void SyncDestroyedNpcByEid(element::ElementId id,void* actor){++test::deaths;CHECK(actor==test::objects);for(auto& p:test::owners)if(p&&p->GetId()==id){p.reset();break;}test::Boundary(24);}
}
namespace coop::npc_join_delivery {void Queue(int){test::Boundary(25);}void Tick(){++test::joins;test::Boundary(20);}}
namespace coop::npc_world_enum {void DrainPendingExSpawns(){test::Boundary(21);}}
namespace coop::kerfur_census {void Tick(){test::Boundary(22);}}
namespace coop::prop_drive_host {
bool InAHand(void* actor){test::Boundary(12,actor);return test::hand;}
void Claim(void* actor,const char*){++test::claims;test::Boundary(13,actor);}
void Release(void* actor){++test::releases;test::Boundary(14,actor);}
ClaimLease ClaimOwned(void* actor,ClaimSource source,const char*,const std::function<bool()>& current){if(current&&!current())return {};++test::claims;test::Boundary(13,actor);if(current&&!current())return {};ClaimLease lease;lease.eid=1;lease.token=1;lease.source=source;return lease;}
bool ReleaseOwned(const ClaimLease&,const std::function<bool()>& current){if(current&&!current())return false;++test::releases;test::Boundary(14,&test::objects[80]);return !current||current();}
bool OwnsClaim(const ClaimLease& lease){return bool(lease);}
bool HasClaims(void*){return false;}

}
namespace ue_wrap {
namespace P::name {const wchar_t* GetActorLocationFn=L"location";const wchar_t* GetActorRotationFn=L"rotation";const wchar_t* GetActorScale3DFn=L"scale";}
namespace R=reflection;
struct ParamFrame{int stage;float output[3]{};explicit ParamFrame(void* fn):stage(R::InternalIndexOf(fn)-240){}bool valid()const{return true;}bool GetRaw(const wchar_t*,void* p,int size){CHECK(size==12);std::memcpy(p,output,12);return true;}};
bool Call(void* actor,ParamFrame& f){auto* obj=static_cast<test::Object*>(actor);const void* value=f.stage==0?static_cast<const void*>(&obj->pos):f.stage==1?static_cast<const void*>(&obj->rot):static_cast<const void*>(&obj->scale);std::memcpy(f.output,value,12);test::Boundary(f.stage==2?0:30+f.stage,actor);return test::transformOkay;}
namespace engine {
namespace P=ue_wrap::P;
#include "npc_pose_transform.inc"
FVector GetActorVelocity(void* actor){const auto result=static_cast<test::Object*>(actor)->vel;test::Boundary(1,actor);return result;}
bool SetActorSimulatePhysics(void* actor,bool simulate){if(simulate)++test::thaws;else ++test::freezes;test::Boundary(simulate?16:15,actor);return test::physicsOkay;}
}
namespace puppet {
bool ReadCharacterIsFalling(void* actor){test::Boundary(2,actor);return true;}
bool ReadKerfurLookAt(void* actor,FVector& out){out=static_cast<test::Object*>(actor)->look;test::Boundary(3,actor);return test::hasLook;}
bool ReadKerfurBodyYaw(void* actor,float& out){out=test::bodyYaw;test::Boundary(4,actor);return test::yawProvider==0;}
}
namespace fossilhound {bool ReadBodyYaw(void* actor,float& out){out=test::bodyYaw;test::Boundary(5,actor);return test::yawProvider==1;}}
namespace rufus {bool ReadBodyYaw(void* actor,float& out){out=test::bodyYaw;test::Boundary(6,actor);return test::yawProvider==2;}}
namespace kerfur {
void* ReadHoldObject(void* actor){void* held=static_cast<test::Object*>(actor)->held;test::Boundary(11,actor);return held;}
bool ReadKerfurState(void* actor,uint8_t& state,bool& spooky,uint8_t& face){state=3;spooky=true;face=4;test::Boundary(7,actor);return test::hasState;}
bool ReadKerfurFloppy(void* actor,bool& has,uint8_t& type){has=test::hasDisc;type=2;test::Boundary(8,actor);return true;}
}
namespace sit_pose {bool ReadIsOnAtv(void* actor){test::Boundary(9,actor);return true;}}
}
int main(){using namespace test;
 for(int n=1;n<=25;++n){Reset();auto* element=Add();const auto id=element->GetId();
  switch(n){
  case 1: callback=[](int stage,void*){if(stage==20&&joins<2)P::TickPoseStream();};P::TickPoseStream();CHECK(joins==1&&session.poseWrites==1);break;
  case 2:
   for(int cause=0;cause<6;++cause){Reset();Add();callback=[=](int stage,void*){if(stage!=1)return;if(cause==0)active=&successor;if(cause==1)++sessionSerial;if(cause==2)++session.stops;if(cause==3)session.mode=N::Role::Client;if(cause==4)session.active=false;if(cause==5)session.linked=false;};P::TickPoseStream();CHECK(!session.poseWrites&&!Seen(2));}break;
  case 3: for(int cause=0;cause<4;++cause){Reset();Add();callback=[=](int stage,void*){if(stage!=1)return;if(cause==0)++world;if(cause==1)worldIndex=1;if(cause==2)P::DropKerfurHolds();if(cause==3)worldIndex=1;};P::TickPoseStream();CHECK(!session.poseWrites&&!Seen(2));}break;
  case 4: Add(1);callback=[=](int stage,void* actor){if(stage==1&&actor==&objects[0])owners[1].reset();};P::TickPoseStream();CHECK(session.poseBatch.size()==1&&session.poseBatch[0].elementId==id);break;
  case 5: for(int cause=0;cause<6;++cause){Reset();auto* el=Add();auto row=ReadRow(el);if(cause==0)el->SetTypeName("other_C");if(cause==1)el->SetBeingDeleted(true);if(cause==2)el->SetActor(&objects[1],1);if(cause==3){auto* newer=Add(0);CHECK(newer);}if(cause==4)Replace(el->GetId());if(cause==5){const auto eid=el->GetId();E::Registry::Get().FreeId(eid);CHECK(E::Registry::Get().RegisterMirror(eid,el));}CHECK(!row.Current());}break;
  case 6: for(int cause=0;cause<3;++cause){Reset();auto row=ReadRow(Add());if(cause==0)++serial[0];if(cause==1)++serial[200];if(cause==2)objects[0].cls=201;CHECK(!row.Current());}break;
  case 7: callback=[](int stage,void*){if(stage==1)P::TickPoseStream();};P::TickPoseStream();CHECK(joins==1&&session.poseBatch.size()==1);break;
  case 8: {Add(1);callback=[](int stage,void* actor){if(stage==9&&actor==&objects[1])++serial[0];};P::TickPoseStream();CHECK(session.poseBatch.size()==1&&session.poseBatch[0].elementId==owners[1]->GetId());}break;
  case 9: {for(int i=1;i<70;++i)Add(i);std::set<uint32_t> seen;for(int i=0;i<70;++i){P::TickPoseStream();CHECK(session.poseBatch.size()==N::kMaxNpcBatchEntries);for(const auto& pose:session.poseBatch)seen.insert(pose.elementId);}CHECK(seen.size()==70);P::DropKerfurHolds();P::TickPoseStream();CHECK(P::g_turn==1);}break;
  case 10: session.linked=false;objects[0].held=&objects[80];P::TickPoseStream();CHECK(claims==1&&!session.poseWrites&&P::KerfurIsHolding(&objects[80]));objects[0].held=nullptr;live[0]=false;P::TickPoseStream();CHECK(deaths==1&&releases==1&&!session.poseWrites);break;
  case 11: for(int stage:{21,22}){Reset();Add();callback=[=](int at,void*){if(at==stage)++session.stops;};P::TickPoseStream();CHECK(!session.poseWrites&&!Seen(11));if(stage==21)CHECK(!Seen(22));}break;
  case 12: element->SetActor(nullptr,-1);P::TickPoseStream();CHECK(!deaths);element->SetActor(&objects[0],0);live[0]=false;P::TickPoseStream();CHECK(deaths==1&&owners[0]==nullptr);break;
  case 13: objects[0].root=nullptr;P::TickPoseStream();CHECK(session.poseBatch.empty()&&!Seen(1));objects[0].root=&objects[230];callback=[](int stage,void*){if(stage==30)objects[0].root=&objects[231];};P::TickPoseStream();CHECK(session.poseBatch.empty()&&!Seen(31));break;
  case 14: for(int invalid=0;invalid<7;++invalid){Reset();Add();const auto nan=std::numeric_limits<float>::quiet_NaN();if(invalid==0)objects[0].pos.X=nan;if(invalid==1)objects[0].pos.X=N::kMaxCoord+1;if(invalid==2)objects[0].vel.Z=nan;if(invalid==3)objects[0].vel={N::kMaxSpeed,N::kMaxSpeed,0};if(invalid==4)objects[0].rot.Yaw=nan;if(invalid==5)objects[0].look.X=nan;if(invalid==6)bodyYaw=nan;P::TickPoseStream();if(invalid<5)CHECK(session.poseBatch.empty());else{CHECK(session.poseBatch.size()==1);CHECK(!(session.poseBatch[0].stateBits&(invalid==5?N::kEntityPoseBitHasLookAt:N::kEntityPoseBitHasBodyYaw)));}}break;
  case 15: for(int provider=0;provider<3;++provider){Reset();Add();yawProvider=provider;P::TickPoseStream();CHECK(session.poseBatch.size()==1&&session.poseBatch[0].bodyYaw==15);CHECK(Seen(4)&&Seen(5)==(provider>=1)&&Seen(6)==(provider>=2));}break;
  case 16: P::TickPoseStream();{CHECK(session.poseBatch.size()==1);const auto pose=session.poseBatch[0];CHECK(pose.speed==5&&pose.yaw==10&&pose.roll==-10&&pose.kerfState==3&&pose.kerfFace==4&&pose.floppyType==2&&pose.lookAtY==50);CHECK((pose.stateBits&127)==127);}break;
  case 17: objects[0].held=&objects[80];callback=[](int stage,void*){if(stage==15){P::DropKerfurHolds();P::NoteKerfurHolding(&objects[81]);}};P::TickPoseStream();CHECK(freezes==1&&!claims&&!session.poseWrites&&P::KerfurIsHolding(&objects[81]));break;
  case 18: objects[0].held=&objects[80];P::TickPoseStream();CHECK(P::KerfurIsHolding(&objects[80]));++serial[80];CHECK(!P::KerfurIsHolding(&objects[80]));objects[0].held=nullptr;P::TickPoseStream();CHECK(!releases&&!thaws);break;
  case 19: for(int cause=0;cause<5;++cause){Reset();session.mode=N::Role::Client;P::NoteKerfurHolding(&objects[80]);CHECK(P::KerfurIsHolding(&objects[80]));if(cause==0)++serial[80];if(cause==1)++serial[200];if(cause==2)++sessionSerial;if(cause==3)++world;if(cause==4)++session.stops;CHECK(!P::KerfurIsHolding(&objects[80]));}Reset();session.mode=N::Role::Client;P::NoteKerfurHolding(&objects[80]);P::ForgetKerfurHolding(&objects[80]);CHECK(!P::KerfurIsHolding(&objects[80]));break;
  case 20: for(int stage:{12,15}){Reset();Add();objects[0].held=&objects[80];callback=[=](int at,void*){if(at==stage)objects[0].held=&objects[81];};P::TickPoseStream();CHECK(!claims);if(stage==12)CHECK(!freezes);}break;
  case 21: for(int where=0;where<4;++where){Reset();Add();objects[0].held=&objects[80];hand=where==0;if(where==1)callback=[](int stage,void*){if(stage==15)hand=true;};P::TickPoseStream();if(where<2)CHECK(!claims);if(where==0)CHECK(!freezes);if(where>=2){objects[0].held=nullptr;if(where==2)hand=true;else callback=[](int stage,void*){if(stage==14)hand=true;};P::TickPoseStream();CHECK(!thaws&&releases==(where==3?1:0));}}break;
  case 22: objects[0].held=&objects[80];P::TickPoseStream();objects[0].held=nullptr;physicsOkay=false;P::TickPoseStream();CHECK(releases==1&&thaws==1&&P::g_holds.size()==1);physicsOkay=true;P::TickPoseStream();CHECK(releases==1&&thaws==2&&P::g_holds.empty());break;
  case 23: Add(1);objects[0].held=objects[1].held=&objects[80];P::TickPoseStream();objects[0].held=nullptr;P::TickPoseStream();CHECK(P::KerfurIsHolding(&objects[80])&&!thaws&&!releases);objects[1].held=nullptr;P::TickPoseStream();CHECK(thaws==1&&releases==1);break;
  case 24: for(int cause=0;cause<3;++cause){Reset();auto* el=Add();objects[0].held=&objects[80];P::TickPoseStream();if(cause==0)owners[0].reset();if(cause==1)el->SetActor(&objects[1],1);if(cause==2)el->SetBeingDeleted(true);callback=[](int stage,void*){if(stage==14){P::DropKerfurHolds();P::NoteKerfurHolding(&objects[81]);}};P::TickPoseStream();CHECK(releases==1&&!thaws&&P::KerfurIsHolding(&objects[81]));}break;
  case 25: objects[0].held=&objects[80];P::TickPoseStream();P::DropKerfurHolds();CHECK(P::g_holds.empty()&&!P::KerfurIsHolding(&objects[80])&&!thaws&&!releases);break;
  }Pass("NH",n);
 }
 for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){
  Reset();auto* el=Add();auto id=el->GetId();bool fired=false;
  callback=[=,&fired](int at,void*){if(at==stage&&!fired){fired=true;Cause(cause,id);}};
  P::TickPoseStream();CHECK(fired&&session.poseBatch.empty());
  // No next native pose read may follow a cancelled continuation, including yaw fallback.
  CHECK(!Seen(stage+1));Pass("NX",stage*5+cause+1);
 }
 Reset();puts("PASS NPC pose/hold: 75 named fixes; actual pose, Registry, Element and transform bodies");
}
