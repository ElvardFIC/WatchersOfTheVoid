// Actual coin module + exact extracted drive arbitration helpers. Engine/network boundaries doubled.
#include "support_2135.h"
#include "../../src/votv-coop/src/coop/items/coingun_collect.cpp"
#include <array>
namespace test {
coop::net::Session session;
bool mirror=true,captured=false,coinType=true,credit=true,paramsOk=true,destroyOnCredit=false;
bool balanceReadable=true,unreadableAfter=false,dispatchOK=true;
int32_t coinPoints=5;
std::function<void()> duringCollect;
void* materializing=nullptr;uint32_t bornEid=24;int32_t balance=100;int calls=0;
}
namespace coop::coingun_sync {
const wchar_t* const kVerbNameCollect=L"actionOptionIndex";
int32_t ReadCoinPoints(void*){return test::coinPoints;}
namespace internal {
coop::net::Session* Session(){return &test::session;}
bool IsCoinActor(void* p){return p&&test::coinType;}
bool IsCapturedCoin(void*){return test::captured;}
void* CoinClass(){return &test::actor;}
}
}
namespace coop::world_actor_sync {
bool IsMirroredActor(void* p){return p&&test::mirror;}
void* MaterializingActor(){return test::materializing;}
unsigned int MaterializingEid(){return test::bornEid;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f):fn_(f){}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){return test::paramsOk;}
bool Call(void* coin,ParamFrame&){
 ue_wrap::script_gate::Call b{};b.object=coin;b.fromOurCode=true;
 if(coop::coingun_sync::OnCollectVerb(b)==ue_wrap::script_gate::Verdict::Cancel)return true;
 ++test::calls;
 if(test::duringCollect){auto fn=std::move(test::duringCollect);test::duringCollect={};fn();}
 if(test::credit)test::balance+=test::coinPoints;
 if(test::destroyOnCredit)test::actor.live=false;
 if(test::unreadableAfter)test::balanceReadable=false;
 return test::dispatchOK;
}
}
namespace ue_wrap::economy {bool ReadPoints(int32_t* out){*out=test::balance;return test::balanceReadable;}}
namespace ue_wrap::reflection {
std::wstring ClassNameOf(void*){return L"baocoin_C";}
void* FindFunction(void*,const wchar_t*){return &test::ui;}
}
namespace ue_wrap::game_thread {bool RegisterInterceptor(const facts::Resolved&,UFunctionInterceptor){return true;}}
namespace ue_wrap::script_gate {bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
 :ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
Resolved Resolve(const Function&){return {};}
}
namespace ue_wrap::script_gate {bool WatchName(const facts::Function&,int,PreFn,PostFn){return true;}}
// The arbitration harness models a free local hand; held-hand continuity has its own runner.
namespace coop::local_prop_hold {bool IsHeld(void*){return false;}bool Yield(void*){return true;}}
namespace coop::local_streams {uint16_t CurrentHoldGen(){return 1;}}
namespace coop::roster_ledger {void RegisterPerSlotClear(void(*)(void*,int),void*){}}
namespace drive_test {
using namespace coop::active_drive;
struct ActiveDrive {void* actor=nullptr;int actorIdx=-1;
 void* LiveActor()const{return ue_wrap::reflection::IsLiveByIndex(actor,actorIdx)?actor:nullptr;}};
void ResetDriveState(ActiveDrive& d){d={};}
std::array<ActiveDrive,4> g_drives{};
#include "../../_research/tests_2151/drive.h"
}
int main(){using namespace test;namespace C=coop::coingun_sync;using V=ue_wrap::script_gate::Verdict;
 namespace E=coop::element;
 session.mode=coop::net::Role::Client;
 auto& players=coop::players::Registry::Get();players.local=&ui;players.puppet.actor=&actor;
 ue_wrap::script_gate::Call b{};b.object=&actor;
 CHECK(C::OnCollectVerb(b)==V::Cancel&&session.sent.size()==1);
 E::testCoinEid=E::kInvalidId;materializing=&actor;
 CHECK(C::OnCollectVerb(b)==V::Cancel);coop::net::CoinCollectPayload msg{};
 std::memcpy(&msg,session.sent.back().data.data(),sizeof(msg));CHECK(msg.elementId==bornEid);
 materializing=&ui;auto sent=session.sent.size();
 CHECK(C::OnCollectVerb(b)==V::Cancel&&session.sent.size()==sent);
 CHECK(C::g_pendingCollects.size()==1); // another birth cannot lend its eid
 E::testCoinEid=20;session.sendOk=false;C::g_nextCollectRetryMs=0;C::internal::TickCollect();
 CHECK(C::g_pendingCollects.size()==1&&session.sent.size()==sent);
 C::OnCollectVerb(b);CHECK(C::g_pendingCollects.size()==1); // dedup failed overlap/press
 session.sendOk=true;C::g_nextCollectRetryMs=0;C::internal::TickCollect();
 CHECK(C::g_pendingCollects.empty()&&session.sent.size()==sent+1);
 mirror=false;materializing=nullptr;CHECK(C::OnCollectVerb(b)==V::Run);
 captured=true;CHECK(C::OnCollectVerb(b)==V::Cancel);captured=false;mirror=true;
 void* overlap[]={nullptr,&actor};sent=session.sent.size();
 CHECK(C::OnCollectPre(&actor,overlap)&&session.sent.size()==sent);
 overlap[1]=&ui;CHECK(C::OnCollectPre(&actor,overlap)&&session.sent.size()==sent+1);
 session.sendOk=false;C::OnCollectVerb(b);CHECK(C::g_pendingCollects.size()==1);
 ++world;C::g_nextCollectRetryMs=0;C::internal::TickCollect();CHECK(C::g_pendingCollects.empty());
 C::OnCollectVerb(b);C::g_pendingCollects.front().expiresMs=0;C::g_nextCollectRetryMs=0;
 C::internal::TickCollect();CHECK(C::g_pendingCollects.empty());
 C::OnCollectVerb(b);C::internal::OnDisconnectCollect();CHECK(C::g_pendingCollects.empty());
 session.mode=coop::net::Role::Host;session.sendOk=true;E::testTarget=&actor;msg.elementId=20;
 auto collect=[&](int n=sizeof(coop::net::CoinCollectPayload)){C::OnCoinCollect(reinterpret_cast<uint8_t*>(&msg),n,1,&ui);};
 collect(sizeof(msg)+1);CHECK(calls==0);E::testOutcome=E::IntentOutcome::OutOfReach;
 collect();CHECK(calls==0);E::testOutcome=E::IntentOutcome::Ok;
 coinType=false;collect();CHECK(calls==0);coinType=true;
 paramsOk=false;collect();CHECK(calls==0);paramsOk=true;
 credit=false;collect();collect();CHECK(calls==2&&balance==100);
 credit=true;collect();collect();CHECK(calls==3&&balance==105);
 C::g_collected.clear();destroyOnCredit=true;collect();CHECK(calls==4&&!actor.live);
 actor.live=true;++actor.serial;destroyOnCredit=false;collect();CHECK(calls==5);
 session.linked=false;collect();CHECK(calls==5);session.linked=true;
 auto resetCoin=[&]{C::internal::OnDisconnectCollect();actor.live=true;++actor.serial;balance=100;calls=0;
  credit=paramsOk=balanceReadable=dispatchOK=true;destroyOnCredit=unreadableAfter=false;coinPoints=5;
  E::testCoinEid=20;E::testTarget=&actor;E::testOutcome=E::IntentOutcome::Ok;duringCollect={};};
 // Host overlap, host E and two remote attempts race on the SAME reservation.
 resetCoin();duringCollect=[&]{collect();CHECK(C::OnCollectPre(&actor,overlap));
  CHECK(C::OnCollectVerb(b)==V::Cancel);auto nested=b;nested.fromOurCode=true;CHECK(C::OnCollectVerb(nested)==V::Cancel);};
 collect();CHECK(calls==1&&balance==105);collect();CHECK(calls==1);
 resetCoin();CHECK(C::OnCollectPre(&actor,overlap));collect();CHECK(C::OnCollectVerb(b)==V::Cancel);
 CHECK(calls==1&&balance==105);
 resetCoin();CHECK(C::OnCollectVerb(b)==V::Cancel);collect();CHECK(C::OnCollectPre(&actor,overlap));
 CHECK(calls==1&&balance==105);
 // A coin touching the host during construction has no registry eid yet.
 resetCoin();E::testCoinEid=E::kInvalidId;CHECK(C::OnCollectPre(&actor,overlap));
 E::testCoinEid=20;collect();CHECK(calls==1&&balance==105);
 resetCoin();overlap[1]=nullptr;CHECK(!C::OnCollectPre(&actor,overlap)&&calls==0);overlap[1]=&ui;
 // An unreadable pre-balance refuses WITHOUT invoking the native destroy path.
 resetCoin();balanceReadable=false;collect();CHECK(calls==0&&actor.live);
 balanceReadable=true;collect();CHECK(calls==1&&balance==105);
 // Unknown outcome or a false return after partial execution must never retry credit.
 resetCoin();unreadableAfter=true;collect();balanceReadable=true;unreadableAfter=false;collect();CHECK(calls==1&&balance==105);
 resetCoin();dispatchOK=false;collect();collect();CHECK(calls==1&&balance==105);
 resetCoin();duringCollect=[]{C::internal::OnDisconnectCollect();};collect();CHECK(C::g_collected.empty());
 resetCoin();duringCollect=[]{++world;};collect();CHECK(calls==1);
 collect();CHECK(calls==2); // new world identity is independent
 resetCoin();coinPoints=-1;collect();CHECK(calls==0);
 resetCoin();msg._pad=1;collect();CHECK(calls==0);msg._pad=0;
 session.ready=false;collect();CHECK(calls==0);session.ready=true;
 C::OnCoinCollect(reinterpret_cast<uint8_t*>(&msg),sizeof(msg),0,&ui);CHECK(calls==0);
 session.mode=coop::net::Role::Client;mirror=true;session.sendOk=false;C::OnCollectVerb(b);
 CHECK(C::g_pendingCollects.size()==1);session.sendOk=true;C::g_nextCollectRetryMs=0;
 session.allowSend=[](int,auto,const void*,int){C::internal::OnDisconnectCollect();return false;};
 C::internal::TickCollect();CHECK(C::g_pendingCollects.empty());session.allowSend={};
 session.sendOk=false;C::OnCollectVerb(b);session.sendOk=true;C::g_nextCollectRetryMs=0;
 session.allowSend=[](int,auto,const void*,int){C::g_nextCollectRetryMs=0;C::internal::TickCollect();return true;};
 C::internal::TickCollect();CHECK(C::g_pendingCollects.empty());session.allowSend={};
 session.allowSend=[](int,auto,const void*,int){C::internal::OnDisconnectCollect();return false;};
 C::OnCollectVerb(b);CHECK(C::g_pendingCollects.empty());session.allowSend={};
 // Personal map coins remain native on clients; neither E nor overlap forwards.
 session.mode=coop::net::Role::Client;mirror=false;materializing=nullptr;sent=session.sent.size();
 CHECK(C::OnCollectVerb(b)==V::Run&&!C::OnCollectPre(&actor,overlap)&&session.sent.size()==sent);
 session.mode=coop::net::Role::Host;
 puts("PASS coin shared host E/overlap/remote reservation, reentry, pre-enrollment, unreadable balance, reset/world identity and local map coins");
 namespace D=drive_test;
 auto claim=[](int slot,int gen){if(!D::TakeRemoteDrive(slot,uint16_t(gen),&actor))return false;
  D::g_drives[slot].actor=&actor;D::g_drives[slot].actorIdx=actor.id;D::g_holds[slot].actorIdentity.Set(&actor);
  D::g_holds[slot].driveGen=uint16_t(gen);return true;};
 CHECK(claim(3,10));CHECK(claim(1,20));CHECK(!D::g_drives[3].actor);
 CHECK(D::IsRejectedRelease(3,10,&actor));CHECK(!claim(2,30));
 CHECK(D::IsRejectedRelease(2,30,&actor));D::ResetDriveState(D::g_drives[1]);
 CHECK(D::IsRejectedRelease(3,10,&actor)); // winner let go: loser still must not teleport it
 CHECK(claim(3,11)&&!D::IsRejectedRelease(3,11,&actor));
 D::NoteConflict(2,40,&actor,3);++actor.serial;CHECK(!D::IsRejectedRelease(2,40,&actor));
 D::NoteConflict(2,40,&actor,3);++world;CHECK(!D::IsRejectedRelease(2,40,&actor));
 puts("PASS coin authority/identity/retry/expiry/dedup + remote arbitration and late-release guard");
}
