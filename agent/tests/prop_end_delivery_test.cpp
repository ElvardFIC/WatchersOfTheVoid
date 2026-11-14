#define PROP_END_JOINT
#define PROP_PUBLISHER_NO_MAIN
#include "prop_publisher_test.cpp"
#ifndef PROP_END_CONTROLLER_SOURCE
#define PROP_END_CONTROLLER_SOURCE "../../src/votv-coop/src/coop/props/prop_syncer_host.cpp"
#endif
#include PROP_END_CONTROLLER_SOURCE
const void* EndDeliverySharedState();
namespace test {
namespace D=coop::prop_end_delivery;namespace S=coop::prop_syncer_host;
int releases=0;
void Start(N::Session& s,bool all=false){Reset(s);S::Bind(s);S::g_nextPassMs=now+100000;
 if(!all){s.generations.fill(0);s.generations[1]=s.generations[31]=1;}releases=0;calls.clear();}
N::PropDriveEndPayload Packet(float x=10){N::PropDriveEndPayload p{};p.eid=1;p.gen=5;p.x=x;p.y=20;p.z=30;p.yaw=10;return p;}
H::Driven ClientRow(){auto d=Seed(1,true);d.syncer=31;d.claimSeq=7;d.ownerGeneration=1;d.ownerDeparture=S::g_departures[31];*H::FindByEid(1)=d;return d;}
void Produce(N::Session& s,bool controller){if(controller){ClientRow();S::OnClientEnd(s,Packet(),31);}else {Seed(1,false,true);H::Tick(s);}}
void Pump(N::Session& s){now+=D::kRetryMs;D::Tick(s);}
bool Pending(uint32_t eid=1){return D::g_pending.count(eid);}
bool Direct(N::Session& s,N::PropDriveEndPayload p=Packet()){
 ue_wrap::CachedObjRef actor,cls;actor.Set(&objects[0]);cls.Set(&objects[1]);return D::Queue(s,p,actor,cls);
}
void Passed(const char* family,int n){std::printf("PASS %s%02d\n",family,n);}
void Change(N::Session& s,int cause){switch(cause){case 0:++world;break;case 1:++serial;break;case 2:++objects[0].serial;break;case 3:++objects[1].serial;break;case 4:++s.generations[1];break;}}
}
namespace coop::config {bool ResolveFlag(const config_registry::FlagRow&){return false;}}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn,void*){}}
namespace coop::dev::prop_syncer_drill {bool IsEnabled(){return true;}void BeforeGrant(net::Session&,uint8_t){test::Call("beforeGrant");}}
namespace coop::dev::swinger_drill {bool IsEnabled(){return false;}}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session& session,uint8_t slot,float reach){IntentTarget t;t.session_=&session;t.slot_=slot;t.reachUU_=reach;return t;}
IntentSubject IntentTarget::Authorize(void*)const{return {IntentOutcome::Ok};}
IntentSubject IntentTarget::AuthorizeSegment(const ue_wrap::FVector&,const ue_wrap::FVector&)const{return {IntentOutcome::Ok};}
}
namespace coop::players {Registry& Registry::Get(){static Registry r;return r;}void* Registry::Local()const{return &test::objects[0];}RemotePlayer* Registry::Puppet(int)const{static RemotePlayer p;return &p;}}
namespace coop {bool RemotePlayer::TryGetLocation(ue_wrap::FVector& out)const{out=test::location;return true;}}
namespace coop::remote_prop {void* ResolveLiveActorByEid(uint32_t){return &test::objects[0];}}
namespace coop::prop_throw {bool HasMarks(){return false;}bool TakeMark(const std::wstring&,Mark*){return false;}}
namespace coop::prop_drive_stream {
void FeedAdmitted(const net::PropDriveRow&){}
void ReleaseHost(const net::PropDriveEndPayload&){++test::releases;test::Call("release");}
void FeedAdmitted(const net::PropDriveRow& p,const Source& s){if(!s.current||s.current())FeedAdmitted(p);}
void ReleaseHost(const net::PropDriveEndPayload& p,const Source& s){if(!s.current||s.current())ReleaseHost(p);}
}
int main(){using namespace test;N::Session s;CHECK(EndDeliverySharedState()==&D::g_pending);
 Start(s);s.endSendOk[31]=false;Produce(s,false);CHECK(!H::FindByEid(1)&&Pending()&&s.endAttempts.size()==2&&s.endAttempts[0].accepted&&!s.endAttempts[1].accepted);s.endSendOk[31]=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==3&&s.endAttempts.back().slot==31);Passed("EH",1);
 Start(s);Produce(s,true);CHECK(s.endAttempts.size()==1&&s.endAttempts[0].slot==1&&releases==1);Passed("EH",2);
 Start(s);ClientRow();s.endSendOk[1]=false;S::EndClientRow(*H::FindByEid(1),S::Ending::Range);CHECK(!H::FindByEid(1)&&Pending());s.endSendOk[1]=true;Pump(s);CHECK(!Pending());Passed("EH",3);
 Start(s);s.sendOk=false;Produce(s,false);CHECK(Pending()&&H::Rows().empty());Passed("EH",4);
 Start(s);s.endSendOk[1]=s.endSendOk[31]=false;Produce(s,false);s.endSendOk[1]=s.endSendOk[31]=true;now+=250;H::Tick(s);CHECK(s.endAttempts.size()>2);for(int i=0;i<3;++i)Pump(s);CHECK(!Pending()&&H::Rows().empty());Passed("EH",5);
 Start(s);s.generations[31]=0;s.sendOk=false;auto packet=Packet();CHECK(Direct(s,packet));packet.x=999;s.sendOk=true;s.inspectEnd=[&](const void* data,size_t size){auto old=*static_cast<const N::PropDriveEndPayload*>(data);Direct(s,Packet(88));CHECK(std::memcmp(&old,data,size)==0);};Pump(s);CHECK(Pending());Pump(s);CHECK(s.endAttempts.back().payload.x==88&&!Pending());Passed("EH",6);
 Start(s);s.endSendOk[31]=false;Produce(s,false);for(int i=0;i<8;++i)Pump(s);int acceptedFirst=0;for(auto& a:s.endAttempts)if(a.slot==1&&a.accepted)++acceptedFirst;CHECK(acceptedFirst==1&&Pending());Passed("EH",7);
 Start(s);s.generations[31]=0;s.ready[1]=false;Produce(s,false);CHECK(Pending()&&s.endAttempts.empty());Pump(s);CHECK(Pending()&&s.endAttempts.empty());s.ready[1]=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==1);Passed("EH",8);
 Start(s);s.sendOk=false;Produce(s,false);++s.generations[31];s.generations[1]=0;s.sendOk=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==2);Passed("EH",9);
 Start(s);s.generations[31]=0;s.sendOk=false;Produce(s,false);S::OnSlotLeft(1);s.sendOk=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==1);Passed("EH",10);
 Start(s);s.generations[31]=0;s.sendOk=false;Produce(s,false);for(int i=0;i<12;++i)Pump(s);CHECK(Pending()&&s.endAttempts.size()==13);s.sendOk=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==14);Passed("EH",11);
 Start(s);s.generations[31]=0;s.sendOk=false;Produce(s,false);for(int i=0;i<8;++i)D::Tick(s);CHECK(s.endAttempts.size()==1);--now;D::Tick(s);CHECK(Pending()&&s.endAttempts.size()==1);Passed("EH",12);
 Start(s);s.sendOk=false;Produce(s,false);N::Session other;D::Tick(other);CHECK(!Pending()&&s.endAttempts.size()==2);Passed("EH",13);
 for(int change=0;change<3;++change){Start(s);s.sendOk=false;Produce(s,false);if(change==0)++serial;if(change==1)++s.stops;if(change==2)s.linked=false;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==2);}Passed("EH",14);
 for(int change=0;change<2;++change){Start(s);s.sendOk=false;Produce(s,false);if(change==0)++world;else s.mode=N::Role::Client;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==2);}Passed("EH",15);
 for(int change=0;change<3;++change){Start(s);s.sendOk=false;Produce(s,false);if(change==0)++objects[0].serial;if(change==1)++objects[1].serial;if(change==2)objects[0].cls=5;s.sendOk=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==2);}Passed("EH",16);
 Start(s);s.sendOk=false;Produce(s,false);objects[0].eid=2;s.sendOk=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==2);Passed("EH",17);
 Start(s);s.generations[31]=0;s.inspectEnd=[&](const void*,size_t){Direct(s,Packet(44));};CHECK(!Direct(s));CHECK(Pending());Pump(s);CHECK(!Pending()&&s.endAttempts.back().payload.x==44);Passed("EH",18);
 Start(s);s.sendOk=false;Produce(s,false);D::Opened(1);s.sendOk=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==2);
 Start(s);s.generations[31]=0;s.inspectEnd=[&](const void*,size_t){auto next=H::Open(&objects[0],false,"transient");CHECK(next);H::EraseRow(*next);};CHECK(!Direct(s));CHECK(!Pending());Passed("EH",19);
 Start(s);s.generations[31]=0;s.inspectEnd=[&](const void*,size_t){s.endSendOk[1]=false;Direct(s,Packet(66));};CHECK(!Direct(s));CHECK(Pending());s.endSendOk[1]=true;Pump(s);CHECK(!Pending()&&s.endAttempts.back().payload.x==66);
 Start(s);s.generations[31]=0;D::Receipt held[2];D::g_draining=true;for(int i=0;i<2;++i){objects[6+i].eid=100+i;auto p=Packet();p.eid=100+i;ue_wrap::CachedObjRef a,c;a.Set(&objects[6+i]);c.Set(&objects[1]);CHECK(D::Queue(s,p,a,c,-1,{},&held[i]));}D::g_draining=false;s.inspectEnd=[&](const void* data,size_t){const auto eid=static_cast<const N::PropDriveEndPayload*>(data)->eid;D::Cancel(held[eid==100?1:0]);};D::Tick(s);CHECK(s.endAttempts.size()==1&&D::g_pending.empty());Passed("EH",20);
 Start(s,true);Produce(s,false);CHECK(s.endAttempts.size()==D::kBudget&&Pending());for(int i=0;i<32;++i)Pump(s);CHECK(!Pending()&&s.endAttempts.size()==31);
 Start(s);s.generations[31]=0;D::g_draining=true;for(int i=0;i<20;++i){objects[6+i].eid=100+i;auto p=Packet();p.eid=100+i;ue_wrap::CachedObjRef a,c;a.Set(&objects[6+i]);c.Set(&objects[1]);CHECK(D::Queue(s,p,a,c));}D::g_draining=false;D::Tick(s);CHECK(s.endAttempts.size()==D::kBudget);D::Tick(s);CHECK(s.endAttempts.size()==20&&!D::g_pending.size());
 Start(s);s.generations[31]=0;bool recursive=false;hook=[&](const std::string& n){if(!recursive&&n=="endSend"){recursive=true;Direct(s,Packet(77));D::Tick(s);}};CHECK(!Direct(s));hook={};CHECK(s.endAttempts.size()==1&&Pending());Pump(s);CHECK(s.endAttempts.size()==2&&!Pending());Passed("EH",21);
 for(int bad=0;bad<9;++bad){Start(s);auto p=Packet();if(bad==0)p.gen=0;if(bad==1)p.eid=0;if(bad==2)p.x=N::kMaxCoord+1;if(bad==3)p.yaw=181;if(bad==4)p.linVelX=std::numeric_limits<float>::quiet_NaN();if(bad==5)p.angVelZ=std::numeric_limits<float>::infinity();if(bad==6)p._pad[0]=1;if(bad==7)p._pad[1]=1;if(bad==8)p.physFlags=0x80;CHECK(!Direct(s,p)&&!Pending()&&s.endAttempts.empty());ClientRow();S::OnClientEnd(s,p,31);CHECK(H::FindByEid(1)&&releases==0&&!Pending());}Passed("EH",22);
 Start(s);s.endSendOk[1]=false;Produce(s,true);CHECK(S::g_retiring.empty()&&Pending()&&!H::FindByEid(1));s.endSendOk[1]=true;Pump(s);CHECK(!Pending()&&s.endAttempts.size()==2);Passed("EH",23);
 Start(s);s.endSendOk[1]=false;Produce(s,true);const auto released=releases;const int flags=Count("flags");for(int i=0;i<6;++i)Pump(s);CHECK(releases==released&&released==1&&Count("flags")==flags&&Pending());Passed("EH",24);
 for(int ending=0;ending<3;++ending){Start(s);auto d=ClientRow();if(ending==0){d.elected=true;*H::FindByEid(1)=d;}if(ending==2)H::FindByEid(1)->admittedEver=false;S::EndClientRow(*H::FindByEid(1),ending==0?S::Ending::HostVerb:ending==1?S::Ending::Died:S::Ending::Range);CHECK(s.endAttempts.empty()&&!Pending());}Passed("EH",25);
 const char* stages[]={"class","eid","endSend","eid","endSend"};
 for(int controller=0;controller<2;++controller)for(int stage=0;stage<5;++stage)for(int cause=0;cause<5;++cause){
  Start(s);if(!controller)s.generations[31]=0;s.endSendOk[1]=false;
  if(stage>=3){Produce(s,controller);CHECK(Pending());}const auto before=s.endAttempts.size();bool fired=false;
  hook=[&](const std::string& name){if(!fired&&name==stages[stage]&&D::g_draining&&Pending()){fired=true;Change(s,cause);}};
  if(stage>=3)Pump(s);else Produce(s,controller);
  CHECK(fired&&!Pending()&&s.endAttempts.size()==before+size_t(stage==2||stage==4));
  hook={};Pump(s);CHECK(!Pending()&&s.endAttempts.size()==before+size_t(stage==2||stage==4));Passed("EX",controller*25+stage*5+cause+1);
 }
 puts("PASS actual publisher/controller End delivery: 75 cases");
}
