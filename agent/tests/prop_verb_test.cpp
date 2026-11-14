#define PROP_VERB_FIXTURE
#define PROP_PUBLISHER_NO_MAIN
#include "prop_publisher_test.cpp"
namespace actual_handoff {
using Driven=coop::prop_drive_host::Driven;
using Ending=coop::prop_syncer_host::Ending;
struct Receipt {Driven value;};
struct Retirement {bool current=true;bool Current()const{return current;}};
void OnRowOpened(uint32_t eid){coop::prop_syncer_host::OnRowOpened(eid);}
void Complete(Driven d,const Receipt& receipt,bool toHost,const Retirement& retiring,
              uint64_t nowMs,Ending why){
#include "verb_handoff.inc"
}
}
namespace test {
void VerbReset(N::Session& s){
 Reset(s);nativeKey=L"prop";propClass=true;suppressed=remoteHand=hotbarHand=robotHold=false;
 lastHand=worldObject=nullptr;logLines.clear();
 transition=[](const H::Driven& before){actual_handoff::Complete(before,{before},true,{},now,coop::prop_syncer_host::Ending::HostVerb);};
}
H::Driven* Open(int actor=0,bool client=false){return client?H::OpenClientRow(&objects[actor],31,"test"):H::Open(&objects[actor],true,"test");}
void Competing(){auto next=Seed(1,false,true);next.maxGapMs=99;*H::FindByEid(1)=next;}
void Cause(int cause){if(cause==0)++world;if(cause==1)++serial;if(cause==2)++objects[0].serial;if(cause==3)++objects[1].serial;if(cause==4)Competing();}
void Pass(const char* family,int n){std::printf("PASS %s%02d\n",family,n);}
}
int main(){using namespace test;N::Session s,other;
 for(int n=1;n<=25;++n){VerbReset(s);
  switch(n){
  case 1: objects[0].live=false;CHECK(!Open()&&!Count("descendant")&&!Count("key"));break;
  case 2: objects[1].live=false;CHECK(!Open()&&!Count("descendant"));break;
  case 3: for(int cause=0;cause<3;++cause){VerbReset(s);hook=[=](const std::string& stage){if(stage!="key")return;if(cause==0)++world;if(cause==1)worldObject=&objects[47];if(cause==2)H::OnDisconnect();};CHECK(!Open()&&H::Rows().empty());}break;
  case 4: for(int cause=0;cause<6;++cause){VerbReset(s);hook=[&](const std::string& stage){if(stage!="key")return;if(cause==0)H::g_session=&other;if(cause==1)++serial;if(cause==2)++s.stops;if(cause==3)s.mode=N::Role::Client;if(cause==4)s.linked=false;if(cause==5)H::g_host=false;};CHECK(!Open()&&H::Rows().empty());}break;
  case 5: {char reason[32]="original reason";hook=[&](const std::string& stage){if(stage=="key")std::strcpy(reason,"changed reason");};CHECK(H::Open(&objects[0],true,reason));CHECK(std::any_of(logLines.begin(),logLines.end(),[](const auto& text){return text.find("original reason")!=std::string::npos;}));}break;
  case 6: {const auto before=Seed();CHECK(!Open()&&H::Rows().size()==1&&H::FindByEid(1)->revision==before.revision);objects[5].eid=1;CHECK(!Open(5)&&H::Rows().size()==1);}break;
  case 7: for(bool reset:{false,true}){VerbReset(s);bool entered=false;hook=[&](const std::string& stage){if(stage!="descendant"||entered)return;entered=true;if(reset){H::OnDisconnect();CHECK(H::BindSession(s,true));}CHECK(!Open());};auto row=Open();CHECK(entered&&Count("descendant")==1&&H::g_opening.empty());CHECK(bool(row)==!reset);hook={};if(reset)CHECK(Open());}break;
  case 8: {bool entered=false;hook=[&](const std::string& stage){if(stage!="key"||entered)return;entered=true;objects[5].eid=2;CHECK(Open(5));};auto row=Open();CHECK(row&&row->eid==1&&H::Rows().size()==2&&H::FindByEid(2)->ref.Raw()==&objects[5]);}break;
  case 9: for(int hand=0;hand<4;++hand){VerbReset(s);held=hand==0;lastHand=hand==1?&objects[0]:nullptr;remoteHand=hand==2;hotbarHand=hand==3;CHECK(!Open()&&!Count("key"));}break;
  case 10: for(uint32_t eid:{0u,coop::element::kInvalidId,coop::element::kHostRangeSize,65535u}){VerbReset(s);objects[0].eid=eid;CHECK(!Open()&&H::g_genByEid.empty());}break;
  case 11: for(int invalid=0;invalid<3;++invalid){VerbReset(s);nativeKey=invalid==0?std::wstring(sizeof(N::WireKey{}.data)+1,L'a'):invalid==1?std::wstring(1,wchar_t(256)):std::wstring(1,L'\0');CHECK(!Open()&&H::g_genByEid.empty());}VerbReset(s);nativeKey.assign(sizeof(N::WireKey{}.data),L'a');CHECK(Open()->key.len==nativeKey.size());break;
  case 12: for(auto key:{L"",L"None"}){VerbReset(s);nativeKey=key;CHECK(!Open()&&H::Rows().empty());}break;
  case 13: hook=[](const std::string& stage){if(stage=="key")objects[0].eid=2;};CHECK(!Open()&&H::Rows().empty()&&H::g_genByEid.empty());break;
  case 14: hook=[](const std::string& stage){if(stage=="clock")Competing();};CHECK(!Open()&&H::FindByEid(1)->maxGapMs==99&&H::g_genByEid.empty());break;
  case 15: for(bool replace:{false,true}){VerbReset(s);bool done=false;hook=[&](const std::string& stage){if(stage!="opened"||done)return;done=true;if(replace)Competing();else for(int eid=2;eid<150;++eid)Seed(eid);};auto row=Open();CHECK(done);if(replace)CHECK(!row&&H::FindByEid(1)->maxGapMs==99);else CHECK(row&&row->eid==1&&row->ref.Raw()==&objects[0]);}break;
  case 16: CHECK(!H::OpenClientRow(&objects[0],0,"bad"));CHECK(!H::OpenClientRow(&objects[0],N::kMaxPeers,"bad"));CHECK(Open(0,true)->admitMs==now);VerbReset(s);CHECK(H::OpenClientRow(&objects[0],31,"absent",[]{return !H::FindByEid(1);}));VerbReset(s);hook=[](const std::string& stage){if(stage=="opened"){auto newer=Seed(1,true);newer.admitMs=777;*H::FindByEid(1)=newer;}};CHECK(!Open(0,true)&&H::FindByEid(1)->admitMs==777);break;
  case 17: {Seed(1,false,true);bool entered=false;hook=[&](const std::string& stage){if(stage!="hand"||entered)return;entered=true;for(int eid=2;eid<150;++eid)Seed(eid);};auto row=H::ElectClientRow(&objects[0],2,"elect");CHECK(row&&entered&&row->syncer==2&&row->eid==1);}break;
  case 18: for(bool heartbeat:{false,true}){VerbReset(s);Seed(1,false,true);hook=[=](const std::string& stage){if(stage!="hand")return;if(heartbeat){H::OnPeerWorldReady();H::FindByEid(1)->maxGapMs=99;}else Competing();};CHECK(!H::ElectClientRow(&objects[0],2,"elect")&&H::FindByEid(1)->maxGapMs==99);}break;
  case 19: {auto before=Seed(1,true);before.episode=123456;*H::FindByEid(1)=before;H::EraseRow(*H::FindByEid(1));actual_handoff::Complete(before,{before},true,{},now,coop::prop_syncer_host::Ending::HostVerb);auto row=H::FindByEid(1);CHECK(row&&!row->syncer&&row->handoffFrom==before.revision&&row->episode!=before.episode);}break;
  case 20: for(int mode=0;mode<3;++mode){VerbReset(s);Seed(1,true);if(mode)hook=[=](const std::string& stage){if(stage==(mode==1?"clientEnd":"opened"))Competing();};H::Claim(&objects[0],"claim");auto row=H::FindByEid(1);CHECK(row);if(mode)CHECK(row->maxGapMs==99&&!row->claimed);else CHECK(row->claimed&&!row->syncer&&!row->handoffFrom);}break;
  case 21: for(bool replace:{false,true}){VerbReset(s);Seed(1,true);if(replace)hook=[](const std::string& stage){if(stage=="clientEnd"){auto row=Seed();row.maxGapMs=99;*H::FindByEid(1)=row;}};H::Coast(&objects[0],"coast");auto row=H::FindByEid(1);CHECK(row&&!row->syncer);if(replace)CHECK(row->claimed&&row->maxGapMs==99);else CHECK(!row->claimed&&!row->handoffFrom);}break;
  case 22: hook=[](const std::string& stage){if(stage=="robot")Competing();};H::Coast(&objects[0],"gate");CHECK(H::FindByEid(1)->maxGapMs==99&&!Count("key"));VerbReset(s);robotHold=true;H::Coast(&objects[0],"robot");CHECK(H::Rows().empty());break;
  case 23: for(int mode=0;mode<3;++mode){VerbReset(s);auto before=Seed(1,false,true);H::g_genByEid[1]=before.gen;if(mode==1)hook=[](const std::string& stage){if(stage=="clock")Competing();};if(mode==2)hook=[](const std::string& stage){if(stage=="opened")Competing();};H::Coast(&objects[0],"release",true);auto row=H::FindByEid(1);if(mode)CHECK(row->maxGapMs==99);else CHECK(row->gen==before.gen+1&&row->episode!=before.episode&&!row->claimed&&row->resend);if(mode==1)CHECK(H::g_genByEid[1]==before.gen);}break;
  case 24: {Seed();objects[0].live=false;H::Release(&objects[0]);H::Release(reinterpret_cast<void*>(uintptr_t(1234)));CHECK(!Count("foreignCapture")&&!Count("clock"));VerbReset(s);auto before=Seed();hook=[](const std::string& stage){if(stage=="clock"){auto next=Seed();next.maxGapMs=99;*H::FindByEid(1)=next;}};H::Release(&objects[0]);CHECK(H::FindByEid(1)->claimed&&H::FindByEid(1)->maxGapMs==99);VerbReset(s);Seed();H::Release(&objects[0]);CHECK(!H::FindByEid(1)->claimed&&H::FindByEid(1)->lastMoveMs==now);}break;
  case 25: for(int verb=0;verb<3;++verb){VerbReset(s);auto before=Seed(1,true);H::EraseRow(*H::FindByEid(1));actual_handoff::Complete(before,{before},true,{},now,coop::prop_syncer_host::Ending::HostVerb);H::VerbProof proof(&objects[0]);CHECK(proof.Capture()&&proof.Handoff(before));if(verb==0)H::Claim(&objects[0],"consume");if(verb==1)H::Coast(&objects[0],"consume");if(verb==2){H::FindByEid(1)->claimed=false;CHECK(H::ElectClientRow(&objects[0],2,"consume"));}CHECK(!H::FindByEid(1)->handoffFrom&&!proof.Handoff(before));}break;
  }Pass("VA",n);
 }
 const char* reads[]={"descendant","hand","lastHand","remoteHand","hotbarHand","eid","className","suppression","key","eid"};
 for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){
  VerbReset(s);bool fired=false;int ids=0;
  hook=[&](const std::string& name){if(name=="eid")++ids;if(fired||name!=reads[stage]||(stage==9&&ids<2))return;fired=true;Cause(cause);};
  CHECK(!Open()&&fired);if(cause==4)CHECK(H::Rows().size()==1&&H::FindByEid(1)->maxGapMs==99);else CHECK(H::Rows().empty());
  CHECK(H::g_genByEid.empty()&&!Count("opened"));
  if(stage<8)CHECK(!Count("key"));
  Pass("VX",stage*5+cause+1);
 }
 VerbReset(s);H::OnDisconnect();puts("PASS drive verb admission: 75 named fixes; actual admission and handoff commit bodies");
}
