#define PROP_SYNCER_HOST_NO_MAIN
#include "prop_syncer_host_test.cpp"
namespace test {
void Pump(){now+=S::kAnswerRetryMs;S::DrainAnswers(now);}
bool Pending(uint32_t eid=1,uint8_t slot=1){return S::g_answers.count(S::AnswerKey(eid,slot));}
bool Control(const H::Driven& d,bool grant=true,bool dead=false){
 return S::Answer(*S::g_session,d.syncer,d.eid,d.claimSeq,d.gen,
  grant?N::PropSyncResult::Granted:N::PropSyncResult::Revoked,N::PropSyncRefusal::None,
  d.elected?N::PropSyncKind::Throw:N::PropSyncKind::Claim,&d,dead);
}
void Change(N::Session& s,int cause){switch(cause){case 0:++world;break;case 1:++serial;break;
 case 2:++objects[0].serial;break;case 3:++objects[1].serial;break;case 4:++s.generations[1];break;}}
void Passed(const char* family,int n){std::printf("PASS %s%02d\n",family,n);}
}
int main(){using namespace test;N::Session s;
 Reset(s);s.sendOk=false;S::OnClaim(s,Claim(),1);CHECK(Pending()&&H::FindByEid(1)&&s.answerAttempts.size()==1);s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.size()==1&&s.answers[0].kind==uint8_t(N::PropSyncKind::Claim));Passed("GD",1);
 Reset(s);s.sendOk=false;float release[3]={10,20,30};CHECK(S::ElectSyncer(&objects[0],31,29,release,now,false));CHECK(Pending(1,31));s.sendOk=true;Pump();CHECK(!Pending(1,31)&&s.answers.size()==1&&s.answers[0].kind==uint8_t(N::PropSyncKind::Throw)&&s.answers[0].seq==29);Passed("GD",2);
 Reset(s);auto d=Seed();s.sendOk=false;S::EndClientRow(*H::FindByEid(1),S::Ending::Range);CHECK(!H::FindByEid(1)&&Pending());s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.size()==1&&s.answers[0].result==uint8_t(N::PropSyncResult::Revoked));Passed("GD",3);
 Reset(s);Seed();objects[0].live=false;s.sendOk=false;S::EndClientRow(*H::FindByEid(1),S::Ending::Died);CHECK(Pending());calls.clear();s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.size()==1&&!Count("class")&&!Count("eid"));Passed("GD",4);
 for(int variant=0;variant<3;++variant){Reset(s);d=Seed(1,31);s.sendOk=false;Control(d);if(variant==0)++s.generations[31];else if(variant==1)s.generations[31]=0;else s.linked=false;s.sendOk=true;Pump();CHECK(!Pending(1,31)&&s.answers.empty());}Passed("GD",5);
 Reset(s);d=Seed();s.sendOk=false;Control(d,false);H::EraseRow(*H::FindByEid(1));S::OnSlotLeft(1);s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.empty());Passed("GD",6);
 for(bool grant:{false,true}){Reset(s);d=Seed();s.ready[1]=false;Control(d,grant);CHECK(Pending()&&s.answerAttempts.empty());S::DrainAnswers(now);CHECK(Pending());s.ready[1]=true;S::DrainAnswers(now);CHECK(!Pending()&&s.answers.size()==1);}Passed("GD",7);
 Reset(s);d=Seed();s.sendOk=false;Control(d);for(int i=0;i<12;++i)Pump();CHECK(Pending()&&s.answerAttempts.size()==13);s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.size()==1);Passed("GD",8);
 Reset(s);d=Seed();s.sendOk=false;Control(d);for(int i=0;i<8;++i)S::DrainAnswers(now);CHECK(Pending()&&s.answerAttempts.size()==1);++now;S::DrainAnswers(now);CHECK(s.answerAttempts.size()==1);Pump();CHECK(s.answerAttempts.size()==2);Passed("GD",9);
 Reset(s);d=Seed();s.sendOk=false;Control(d);now-=1;S::DrainAnswers(now);CHECK(Pending()&&s.answerAttempts.size()==1);Passed("GD",10);
 Reset(s);d=Seed();s.sendOk=false;Control(d);++serial;S::DrainAnswers(now+1000);CHECK(!Pending());S::Admit(s);CHECK(S::g_answers.empty()&&s.answers.empty());Passed("GD",11);
 Reset(s);d=Seed();s.sendOk=false;Control(d);N::Session replacement;S::Admit(replacement);CHECK(S::g_answers.empty()&&s.answers.empty());Passed("GD",12);
 Reset(s);d=Seed();s.sendOk=false;Control(d);++s.stops;S::DrainAnswers(now+1000);CHECK(!Pending()&&s.answerAttempts.size()==1);S::Admit(s);CHECK(S::g_answers.empty()&&s.answers.empty());Passed("GD",13);
 Reset(s);d=Seed();s.sendOk=false;Control(d);++world;S::Admit(s);CHECK(S::g_answers.empty()&&s.answers.empty());Passed("GD",14);
 Reset(s);d=Seed();s.sendOk=false;Control(d);s.mode=N::Role::Client;S::DrainAnswers(now+1000);CHECK(!Pending()&&s.answerAttempts.size()==1);S::Admit(s);CHECK(S::g_answers.empty()&&s.answers.empty());Passed("GD",15);
 Reset(s);d=Seed();s.sendOk=false;Control(d);++objects[0].serial;s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.empty());Passed("GD",16);
 for(int variant=0;variant<2;++variant){Reset(s);d=Seed();s.sendOk=false;Control(d);if(variant==0)++objects[1].serial;else objects[0].cls=2;s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.empty());}Passed("GD",17);
 Reset(s);d=Seed();s.sendOk=false;Control(d);objects[0].eid=2;s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.empty());Passed("GD",18);
 Reset(s);d=Seed();s.sendOk=false;Control(d);S::Receipt receipt(*H::FindByEid(1));auto changed=receipt.value;changed.maxGapMs=777;CHECK(S::Save(receipt,changed));s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.size()==1&&H::FindByEid(1)->maxGapMs==777);Passed("GD",19);
 Reset(s);d=Seed();bool replaced=false;hook=[&](const std::string& n){if(n=="answer"&&!replaced){replaced=true;auto next=Seed();next.claimSeq=99;*H::FindByEid(1)=next;Control(next);}};Control(d);CHECK(Pending()&&s.answers.size()==1);hook={};Pump();CHECK(!Pending()&&s.answers.size()==2&&s.answers.back().seq==99);Passed("GD",20);
 Reset(s);d=Seed();s.sendOk=false;Control(d);H::EraseRow(*H::FindByEid(1));Control(d,false);CHECK(Pending()&&!S::g_answers.at(S::AnswerKey(1,1))->grant);s.sendOk=true;Pump();CHECK(s.answers.size()==1&&s.answers[0].result==uint8_t(N::PropSyncResult::Revoked));Passed("GD",21);
 Reset(s);d=Seed();s.sendOk=false;Control(d);S::OnRowOpened(1);CHECK(!Pending());auto next=Seed();H::EraseRow(*H::FindByEid(1));s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.empty());
 Reset(s);d=Seed();s.sendOk=false;Control(d);Seed();s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.empty());Passed("GD",22);
 Reset(s);S::g_answerDraining=true;for(int slot=1;slot<32;++slot){d=Seed(1,uint8_t(slot));Control(d,false);}S::g_answerDraining=false;S::DrainAnswers(now);CHECK(s.answers.size()==S::kAnswerBudget);for(int i=0;i<8;++i)S::DrainAnswers(now);CHECK(S::g_answers.empty()&&s.answers.size()==31);
 Reset(s);d=Seed();bool recursive=false;hook=[&](const std::string& n){if(n=="answer"&&!recursive){recursive=true;auto next=Seed();Control(next);S::DrainAnswers(now);}};Control(d);CHECK(recursive&&s.answers.size()==1&&Pending());hook={};Pump();CHECK(s.answers.size()==2&&!Pending());Passed("GD",23);
 Reset(s);d=Seed();s.inspectAnswer=[&](const void* data,size_t size){N::PropSyncAnswerPayload old{};std::memcpy(&old,data,size);S::g_answers.clear();auto next=Seed();next.claimSeq=56;*H::FindByEid(1)=next;Control(next);CHECK(std::memcmp(&old,data,size)==0);};Control(d);CHECK(s.answers.size()==1&&s.answers[0].seq==7&&Pending());Pump();CHECK(s.answers.back().seq==56&&!Pending());Passed("GD",24);
 Reset(s);d=Seed(1,1,true);s.sendOk=false;S::EndClientRow(*H::FindByEid(1),S::Ending::HostVerb);CHECK(Pending()&&H::FindByEid(1)&&H::FindByEid(1)->syncer==0);s.sendOk=true;Pump();CHECK(!Pending()&&s.answers.size()==1&&s.answers[0].seq==7&&s.answers[0].gen==5&&s.answers[0].kind==uint8_t(N::PropSyncKind::Throw));Passed("GD",25);
 const char* boundaries[]={"class","eid","answer","eid","answer"};
 for(int revoke=0;revoke<2;++revoke)for(int stage=0;stage<5;++stage)for(int cause=0;cause<5;++cause){
  Reset(s);d=Seed();if(revoke)H::EraseRow(*H::FindByEid(1));s.sendOk=false;
  if(stage>=3){Control(d,!revoke);CHECK(Pending());}calls.clear();bool fired=false;
  hook=[&](const std::string& n){if(!fired&&n==boundaries[stage]){fired=true;Change(s,cause);}};
  if(stage>=3)Pump();else Control(d,!revoke);
  CHECK(fired&&!Pending()&&s.answerAttempts.size()==size_t((stage>=3?1:0)+((stage==2||stage==4)?1:0)));
  if(stage==0)CHECK(Count("eid")==0);
  hook={};Pump();CHECK(!Pending()&&s.answers.empty());Passed("GX",revoke*25+stage*5+cause+1);
 }
 puts("PASS actual grant/revoke delivery: 75 cases");
}
