// Actual coin module + exact extracted drive arbitration helpers. Engine/network boundaries doubled.
#include "support_2135.h"
#include "../../src/votv-coop/src/coop/items/coingun_collect.cpp"
#include <array>
namespace test {
coop::net::Session session;
bool mirror=true,captured=false,coinType=true,credit=true,paramsOk=true,destroyOnCredit=false;
void* materializing=nullptr;uint32_t bornEid=24;int32_t balance=100;int calls=0;
}
namespace coop::coingun_sync {
const wchar_t* const kVerbNameCollect=L"actionOptionIndex";
int32_t ReadCoinPoints(void*){return 5;}
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
bool Call(void*,ParamFrame&){++test::calls;if(test::credit)test::balance+=5;
 if(test::destroyOnCredit)test::actor.live=false;return true;}
}
namespace ue_wrap::economy {bool ReadPoints(int32_t* out){*out=test::balance;return true;}}
namespace ue_wrap::reflection {
std::wstring ClassNameOf(void*){return L"baocoin_C";}
void* FindFunction(void*,const wchar_t*){return &test::ui;}
}
namespace ue_wrap::game_thread {bool RegisterInterceptor(void*,UFunctionInterceptor){return true;}}
namespace ue_wrap::script_gate {bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}}
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
 namespace D=drive_test;
 auto claim=[](int slot,int gen){if(!D::TakeRemoteDrive(slot,uint16_t(gen),&actor))return false;
  D::g_drives[slot].actor=&actor;D::g_drives[slot].actorIdx=actor.id;D::g_holds[slot].driveGen=uint16_t(gen);return true;};
 CHECK(claim(3,10));CHECK(claim(1,20));CHECK(!D::g_drives[3].actor);
 CHECK(D::IsRejectedRelease(3,10,&actor));CHECK(!claim(2,30));
 CHECK(D::IsRejectedRelease(2,30,&actor));D::ResetDriveState(D::g_drives[1]);
 CHECK(D::IsRejectedRelease(3,10,&actor)); // winner let go: loser still must not teleport it
 CHECK(claim(3,11)&&!D::IsRejectedRelease(3,11,&actor));
 D::NoteConflict(2,40,&actor,3);++actor.serial;CHECK(!D::IsRejectedRelease(2,40,&actor));
 D::NoteConflict(2,40,&actor,3);++world;CHECK(!D::IsRejectedRelease(2,40,&actor));
 puts("PASS coin authority/identity/retry/expiry/dedup + remote arbitration and late-release guard");
}
