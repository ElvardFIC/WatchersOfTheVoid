// Production functions extracted unchanged by run_2149.ps1; engine boundaries doubled.
#include "support_2135.h"
#include "coop/interactables/atv_sync.h"
#include "coop/interactables/atv_sync_internal.h"
#include "coop/player/players_registry.h"
#include "coop/net/session.h"
#include "ue_wrap/devices/atv.h"
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
struct Args {void* player;ue_wrap::reflection::FString name;};
int frameSize=sizeof(Args);
}
namespace ue_wrap::reflection {
int32_t FunctionFrameSize(void*){return test::frameSize;}
int32_t FindParamOffset(void*,const wchar_t* n){return !wcscmp(n,L"player")?0:offsetof(test::Args,name);}
}
namespace ue_wrap::atv {
bool IsAtv(void* a){return a==&test::actor;}
bool Dismount(void*){++test::dismounts;if(test::dismountOk)test::seated=false;
 if(test::destroyOnDismount)test::actor.live=false;return test::dismountOk;}
bool GetRootTransform(void*,FVector& l,FRotator& r){l={0,0,50};r=test::rotation;return true;}
#include "../../_research/tests_2149/placement.h"
}
namespace ue_wrap::engine {
bool GetActorBounds(void*,bool,FVector& c,FVector& e){c={0,0,50};e=test::extent;return true;}
FVector GetActorForwardVector(void*){return {1,0,0};}
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& out){out=test::grab;if(test::held)out.grabbingActor=test::held;return true;}
}
namespace ue_wrap::trace {
bool FindFloor(void*,const FVector& a,const FVector& b,FVector& p){
 ++test::floorCalls;CHECK(a.Z==40&&b.Z==-60);
 if(test::floorMode==1)return false;
 if(test::floorMode==2&&test::floorCalls%5==0)return false; // corner over a pit
 p={a.X,a.Y,0};return true;
}
int BoxBlocked(void*,const FVector& a,const FVector& b,const FVector& half){
 ++test::boxCalls;CHECK(half.X==100&&half.Y==60&&a.Z>=54&&b.Z>=53);
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
bool IsOccupiedByOther(void* a,uint8_t*){return a==&test::actor&&g_atvs[L"atv"].occupantSlot==2;}
#include "../../_research/tests_2149/mount.h"
}
namespace S=coop::atv_sync;
int main(){using namespace test;using V=ue_wrap::script_gate::Verdict;
 coop::net::Session session;S::g_session=&session;
 auto& reg=coop::players::Registry::Get();reg.peer=1;
 grab.gravityActor=&actor;CHECK(S::IsLocalGrabber(&actor,reg.Local()));grab={};
 S::AtvEntry e{};e.actor=&actor;e.idx=1;e.authorSlot=2;S::g_atvs[L"atv"]=e;
 wchar_t sit[]=L"sit",other[]=L"upgrades";
 Args args{reg.Local(),{sit,4,4}};ue_wrap::script_gate::Call c{};
 c.object=&actor;c.function=&ui;c.locals=reinterpret_cast<uint8_t*>(&args);
 CHECK(S::OnMountPre(c)==V::Cancel);
 c.fromOurCode=true;CHECK(S::OnMountPre(c)==V::Cancel); // safety applies to indirect native use too
 args.name={other,9,9};CHECK(S::OnMountPre(c)==V::Run); // maintenance not blocked
 args.name={sit,4,4};args.player=&ui;CHECK(S::OnMountPre(c)==V::Run);
 args.player=reg.Local();frameSize=1;CHECK(S::OnMountPre(c)==V::Run);frameSize=sizeof(Args);
 S::g_atvs[L"atv"].authorSlot=0xFF;CHECK(S::OnMountPre(c)==V::Run);
 held=&actor;CHECK(S::OnMountPre(c)==V::Cancel);session.linked=false;CHECK(S::OnMountPre(c)==V::Run);
 session.linked=true;held=nullptr;
 coop::net::AtvStatePayload p{};p.occupantSlot=0xFF;p.authorSlot=2;p.stateBits=coop::net::kAtvStateBitGrabbed;
 seated=true;e.occupantSlot=1;e.authorSlot=1;e.wasPoseAuthor=true;
 CHECK(!S::AcceptIncomingOccupancy(e,p,3,reg.Local(),1));CHECK(seated&&dismounts==0); // forged owner
 CHECK(!S::AcceptIncomingOccupancy(e,p,0xFF,reg.Local(),1));CHECK(seated&&dismounts==0);
 dismountOk=false;CHECK(!S::AcceptIncomingOccupancy(e,p,2,reg.Local(),1));CHECK(seated&&e.occupantSlot==1);
 dismountOk=true;CHECK(S::AcceptIncomingOccupancy(e,p,2,reg.Local(),1));
 CHECK(!seated&&e.occupantSlot==0xFF&&e.authorSlot==2&&!e.wasPoseAuthor);
 e.wasPoseAuthor=true;CHECK(S::AcceptIncomingOccupancy(e,p,2,reg.Local(),1));
 CHECK(!e.wasPoseAuthor); // local release edge must not erase a newer carrier claim
 held=&actor;p.occupantSlot=2;p.authorSlot=2;p.stateBits=coop::net::kAtvStateBitDriven;
 CHECK(!S::AcceptIncomingOccupancy(e,p,2,reg.Local(),1)); // local carrier keeps real handle
 held=nullptr;seated=true;p.occupantSlot=0;p.authorSlot=0;e.occupantSlot=1;
 CHECK(S::AcceptIncomingOccupancy(e,p,0,reg.Local(),1));CHECK(!seated&&e.occupantSlot==0);
 seated=true;p.occupantSlot=2;p.authorSlot=2;e.occupantSlot=1;
 int count=dismounts;CHECK(S::AcceptIncomingOccupancy(e,p,2,reg.Local(),1));CHECK(seated&&dismounts==count);
 p.occupantSlot=0xFF;p.authorSlot=2;p.stateBits=coop::net::kAtvStateBitGrabbed;
 destroyOnDismount=true;CHECK(!S::AcceptIncomingOccupancy(e,p,2,reg.Local(),1));actor.live=true;
 destroyOnDismount=false;
 ue_wrap::FVector location{};
 CHECK(ue_wrap::atv::FindCompanionLocation(&actor,location));
 CHECK(location.Y==-145&&location.X==0&&location.Z==54); // nearby supported opposite bay
 for(int mode=1;mode<=2;++mode){boxMode=mode;CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));}
 boxMode=0;
 for(int mode=1;mode<=2;++mode){floorMode=mode;floorCalls=0;CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));}
 floorMode=0;rotation.Roll=45;CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));rotation={};
 extent.X=std::numeric_limits<float>::quiet_NaN();CHECK(!ue_wrap::atv::FindCompanionLocation(&actor,location));
 puts("PASS ATV native-menu gate, carrier/seat races, invalid sender/lifetime, placement walls/pits/failure/tilt");
}
