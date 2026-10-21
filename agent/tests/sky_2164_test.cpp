#include "support_2135.h"
#include "../../src/votv-coop/src/coop/world/sky_sync.cpp"
#include "ue_wrap/core/game_thread.h"
#include "nonentity_facts_support.h"
#include <limits>
namespace test {
bool readable=true, hidden=false, light=false, eye=false, applyOk=true;
int skyApplies=0, blackApplies=0, eyeApplies=0;
int entryOffset=0,entrySize=4;
}
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*){return test::entryOffset;}
int32_t FunctionFrameSize(void*){return test::entrySize;}
}
namespace ue_wrap::skysphere {
bool EnsureResolved(){return true;}
bool ReadSky(FRotator& r,float& m){r={1,2,3};m=.5f;return true;}
void ApplySky(const FRotator&,float){++test::skyApplies;test::Callback();}
bool ReadEye(bool& b){b=test::eye;return true;}
bool CallSetEye(bool b){test::eye=b;++test::eyeApplies;return true;}
bool ReadBlackHole(bool& h,bool& l){h=test::hidden;l=test::light;return test::readable;}
bool ApplyBlackHole(bool h,bool l){
 CHECK(coop::sky_sync::OnBlackHoleVisibilityPre({})==script_gate::Verdict::Run);
 ++test::blackApplies;if(!test::applyOk)return false;
 test::hidden=h;test::light=l;test::Callback();return true;
}
}
int main(){using namespace test;namespace S=coop::sky_sync;namespace F=coop::net::sky_flags;
 using V=ue_wrap::script_gate::Verdict;coop::net::Session s;S::Install(&s);
 CHECK(ue_wrap::script_gate::watchedBodies.size()==3);
 CHECK(S::OnBlackHolePre({})==V::Run);
 coop::net::SkyStatePayload p{};hidden=light=true;CHECK(S::MakePayload(p)&&p.blackHoleFlags==7);
 S::QueueConnectBroadcastForSlot(2);CHECK(s.sent.size()==1);
 coop::net::SkyStatePayload join{};std::memcpy(&join,s.sent[0].data.data(),sizeof(join));CHECK(join.blackHoleFlags==7);
 readable=false;CHECK(S::MakePayload(p)&&p.blackHoleFlags==0);readable=true;
 s.mode=coop::net::Role::Client;
 announced=false;CHECK(S::OnBlackHolePre({})==V::Cancel); // also block loading-window local starts
 CHECK(S::OnBlackHoleVisibilityPre({})==V::Cancel);announced=true;
 ue_wrap::script_gate::Call graph{};graph.object=&actor;graph.function=&actor;
 int32_t entry=0;graph.locals=reinterpret_cast<uint8_t*>(&entry);
 for(int32_t phase:{5062,3222,70,437,552,759,1023}){entry=phase;
  CHECK(S::OnBlackHoleGraphPre(graph)==V::Cancel);graph.fromOurCode=true;
  CHECK(S::OnBlackHoleGraphPre(graph)==V::Cancel);graph.fromOurCode=false;
  s.mode=coop::net::Role::Host;CHECK(S::OnBlackHoleGraphPre(graph)==V::Run);s.mode=coop::net::Role::Client;
 }
 for(int32_t ordinary:{4855,4700,1282,1642,1915,1930,3276,5067}){entry=ordinary;CHECK(S::OnBlackHoleGraphPre(graph)==V::Run);}
 entry=437;entryOffset=-1;CHECK(S::OnBlackHoleGraphPre(graph)==V::Run);entryOffset=0;
 entrySize=3;CHECK(S::OnBlackHoleGraphPre(graph)==V::Run);entrySize=4;
 p=join;S::OnReliable(p);CHECK(blackApplies==1&&hidden&&light);
 p.blackHoleFlags=F::kBlackHoleValid;S::OnReliable(p);CHECK(!hidden&&!light&&blackApplies==2);
 p.blackHoleFlags=0;S::OnReliable(p);CHECK(blackApplies==2); // missing host metadata is not host-clear
 for(uint8_t f:{uint8_t(2),uint8_t(8),uint8_t(255)}){p.blackHoleFlags=f;S::OnReliable(p);CHECK(blackApplies==2);}
 p=join;p.pad[1]=1;int before=skyApplies;S::OnReliable(p);CHECK(skyApplies==before);
 p=join;p.moonPhase=std::numeric_limits<float>::quiet_NaN();S::OnReliable(p);CHECK(skyApplies==before);
 p=join;applyOk=false;S::OnReliable(p);CHECK(!hidden);applyOk=true;S::OnReliable(p);CHECK(hidden&&light);
 s.mode=coop::net::Role::Host;before=skyApplies;S::OnReliable(p);CHECK(skyApplies==before);
 s.mode=coop::net::Role::Client;before=blackApplies;callback=[&]{++world;};S::OnReliable(p);CHECK(blackApplies==before);
 callback=[&]{S::OnDisconnect();};S::OnReliable(p);CHECK(blackApplies==before);
 CHECK(S::OnBlackHolePre({})==V::Run);before=skyApplies;S::OnReliable(p);CHECK(skyApplies==before);
 S::Install(&s);s.linked=false;CHECK(S::OnBlackHolePre({})==V::Run);S::OnReliable(p);CHECK(skyApplies==before);
 puts("PASS sky controller: host snapshot/join, client start suppression, clear/unread, malformed input, retries, disconnect/world reentry");
 puts("PASS sky event: tp and all six native latent destinations are host-owned; client duplicates cannot author audio/mail/bookkeeping, ordinary sky/initialization/moon paths remain native");
}
