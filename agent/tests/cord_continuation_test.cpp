#define CORD_CONTINUATION_NO_MAIN
#include "cord_intent_2170_test.cpp"
namespace test {
coop::net::Session replacement;
void HostState(){Host();objects[0].socket=&objects[1];objects[1].occupant=&objects[0];session.sent.clear();session.perSlot=true;session.links.fill(false);session.worlds.fill(false);session.generations.fill(1);session.links[31]=session.worlds[31]=true;}
void QueueIntent(uint8_t powered=0){auto p=Packet();p.powered=powered;S::Store(S::g_outbox,p,&objects[0],&objects[1]);}
void Release(uint16_t gen=7){S::g_localDragActive=true;S::g_localDragCord.Set(&objects[0]);S::g_localDragPose={};coop::net::WireKeyFromString(L"__mvcord_k0",S::g_localDragPose.key);S::g_localDragPose.holdGen=gen;S::EndLocalDrag(session,true);}
void SendHook(std::function<void()> fn,bool accepted=true){session.allowSend=[fn=std::move(fn),accepted,used=false](int,coop::net::ReliableKind,const void*,int)mutable{if(!used){used=true;fn();}return accepted;};}
bool Cached(){for(const auto& p:S::g_sent)if(p->accepted)return true;return false;}
size_t Messages(coop::net::ReliableKind kind){size_t n=0;for(const auto& p:session.sent)if(p.kind==kind)++n;return n;}
coop::net::CordStatePayload SentState(size_t i){coop::net::CordStatePayload p{};CHECK(session.sent.at(i).data.size()==sizeof(p));std::memcpy(&p,session.sent[i].data.data(),sizeof(p));return p;}
void Change(int cause){switch(cause){
 case 0:++world;break;
 case 1:replacement={};replacement.mode=session.mode;S::Install(&replacement);break;
 case 2:++sessionSerial;break;
 case 3:++session.stops;break;
 case 4:session.mode=session.mode==coop::net::Role::Host?coop::net::Role::Client:coop::net::Role::Host;break;
 case 5:++objects[0].serial;break;
 case 6:++objects[511].serial;break;
 case 7:++objects[1].serial;break;
 case 8:++objects[510].serial;break;
 case 9:if(session.perSlot)++session.generations[31];else ++session.peerEpoch;break;
}}
void Passed(const char* family,int n){std::printf("PASS %s%02d\n",family,n);}
}
int main(){using namespace test;using Kind=coop::net::ReliableKind;
 Reset();QueueIntent();replacement={};replacement.mode=coop::net::Role::Client;S::Install(&replacement);CHECK(S::g_outbox.empty()&&S::g_actors.size()==2&&S::g_bound==&replacement);
 Reset();QueueIntent();S::Begin(nullptr,false);objects[2].type=1;S::Match(nullptr,&objects[2]);S::OnDisconnect();S::Install(&session);CHECK(S::g_outbox.empty()&&S::g_actors.size()==2&&S::g_scratch.size()==3);CHECK(S::Complete(nullptr,false,world)==3&&S::g_actors.back().Get()==&objects[2]);Passed("CP",1);
 Reset();QueueIntent();++sessionSerial;S::Tick();CHECK(S::g_outbox.empty()&&session.sent.empty());Passed("CP",2);
 Reset();QueueIntent();S::g_scratch=S::g_actors;++session.stops;S::Tick();CHECK(S::g_outbox.empty()&&session.sent.empty()&&S::g_actors.size()==2&&S::g_scratch.size()==2);Passed("CP",3);
 Reset();QueueIntent();session.mode=coop::net::Role::Host;S::Tick();CHECK(S::g_outbox.empty()&&Messages(Kind::CordIntent)==0&&S::g_actors.size()==2);Passed("CP",4);
 Reset();QueueIntent();S::g_scratch=S::g_actors;++world;S::Tick();CHECK(S::g_outbox.empty()&&session.sent.empty()&&S::g_actors.empty()&&S::g_scratch.empty());Passed("CP",5);
 Reset();QueueIntent();session.linked=false;S::Tick();CHECK(S::g_outbox.empty());session.linked=true;S::Tick();CHECK(session.sent.empty());Passed("CP",6);
 Reset();QueueIntent();objects[0].socket=nullptr;S::g_remoteDrags[0].actor.Set(&objects[0]);S::g_remoteDrags[0].at=0;now=3000;SendHook([]{++sessionSerial;});S::Tick();CHECK(Messages(Kind::CordIntent)==1&&!disconnects);
 Reset();auto repaint=Packet();repaint.powered=1;objects[0].socket=nullptr;objects[1].occupant=nullptr;S::Apply(repaint);callback=[]{++sessionSerial;};S::Tick();CHECK(powers==1&&!connects);Passed("CP",7);
 HostState();SendHook([]{S::g_actors.clear();for(int i=2;i<400;++i){objects[i].type=1;ue_wrap::CachedObjRef ref;ref.Set(&objects[i]);S::g_actors.push_back(ref);}});S::g_next=0;S::Tick();CHECK(Messages(Kind::CordState)==1&&S::g_actors.size()==398);Passed("CP",8);
 HostState();SendHook([]{S::g_actors.clear();for(int i=2;i<400;++i){objects[i].type=1;ue_wrap::CachedObjRef ref;ref.Set(&objects[i]);S::g_actors.push_back(ref);}});S::SendAllToSlot(31);CHECK(Messages(Kind::CordState)==1&&session.sent[0].slot==31);Passed("CP",9);
 HostState();SendHook([]{for(int i=2;i<300;++i){objects[i].type=1;objects[i].parent=nullptr;S::Publish(&objects[i],-1);}});CHECK(S::Publish(&objects[0],-1));CHECK(Cached()&&S::g_sent.size()==299&&Messages(Kind::CordState)==1);Passed("CP",10);
 HostState();bool installed=false;SendHook([&]{installed=!S::g_sent.empty()&&S::g_sent[0]->revision!=0&&!S::g_sent[0]->accepted;});CHECK(S::Publish(&objects[0],-1));CHECK(installed&&Cached());Passed("CP",11);
 HostState();SendHook([]{objects[1].power=true;S::Publish(&objects[0],-1);});CHECK(!S::Publish(&objects[0],-1));CHECK(!Cached()&&S::g_publications.size()==1);S::Tick();CHECK(Cached()&&S::g_sent[0]->p.powered==1&&Messages(Kind::CordState)==2);Passed("CP",12);
 HostState();SendHook([]{objects[1].power=true;S::Publish(&objects[0],31);});CHECK(!S::Publish(&objects[0],-1));S::Tick();bool targeted=false;for(const auto& p:session.sent)targeted|=p.slot==31;CHECK(targeted&&Cached()&&S::g_sent[0]->p.powered==1);Passed("CP",13);
 Reset();QueueIntent();SendHook([]{S::g_outbox.clear();QueueIntent(1);});S::Tick();CHECK(S::g_outbox.size()==1&&SentState(0).powered==0);S::Tick();CHECK(S::g_outbox.empty()&&SentState(1).powered==1);Passed("CP",14);
 HostState();CHECK(S::Publish(&objects[0],-1));--now;CHECK(S::Publish(&objects[0],-1));CHECK(Messages(Kind::CordState)==1);
 Reset();objects[0].socket=nullptr;S::g_remoteDrags[0].actor.Set(&objects[0]);S::g_remoteDrags[0].at=now+100;S::Tick();CHECK(!disconnects&&S::g_remoteDrags[0].actor.Get());Passed("CP",15);
 Reset();QueueIntent();auto old=S::g_outbox[0];QueueIntent(1);CHECK(S::g_outbox.size()==1&&S::g_outbox[0]!=old&&S::g_outbox[0]->revision!=old->revision&&S::g_outbox[0]->p.powered==1);Passed("CP",16);
 Reset();QueueIntent();SendHook([]{QueueIntent(1);});S::Tick();CHECK(S::g_outbox.size()==1&&S::g_outbox[0]->p.powered==1&&Messages(Kind::CordIntent)==1);S::Tick();CHECK(S::g_outbox.empty()&&Messages(Kind::CordIntent)==2);Passed("CP",17);
 Reset();QueueIntent();auto pending=S::g_outbox[0];session.sendOk=false;for(int i=0;i<8;++i)S::Tick();CHECK(S::g_outbox.size()==1&&S::g_outbox[0]==pending);session.sendOk=true;S::Tick();CHECK(S::g_outbox.empty()&&Messages(Kind::CordIntent)==1);Passed("CP",18);
 Reset();Release();objects[0].live=false;session.sendOk=false;S::Tick();CHECK(S::g_releases.size()==1);session.sendOk=true;S::Tick();CHECK(S::g_releases.empty()&&Messages(Kind::PropRelease)==1);
 Reset();session.perSlot=true;session.links.fill(false);QueueIntent();Release();CHECK(S::g_outbox.empty());session.links[0]=session.worlds[0]=true;session.generations[0]=2;S::Tick();CHECK(S::g_releases.empty()&&session.sent.empty());Passed("CP",19);
 Reset();Release();SendHook([]{S::g_releases.clear();auto next=std::make_shared<S::Release>();next->p.key=S::g_localDragPose.key;next->p.holdGen=8;S::g_releases.push_back(next);});S::Tick();CHECK(S::g_releases.size()==1&&S::g_releases[0]->p.holdGen==8);S::Tick();CHECK(S::g_releases.empty()&&Messages(Kind::PropRelease)==2);Passed("CP",20);
 Reset();auto p=Packet();objects[0].socket=nullptr;objects[1].occupant=nullptr;S::Apply(p);S::g_remoteDrags[0].actor.Set(&objects[0]);S::g_remoteDrags[0].at=0;now=3000;callback=[]{++sessionSerial;};S::Tick();CHECK(disconnects==1&&!connects);Passed("CP",21);
 HostState();objects[2].type=1;objects[2].parent=nullptr;ue_wrap::CachedObjRef extra;extra.Set(&objects[2]);S::g_actors.push_back(extra);SendHook([]{Change(9);});S::SendAllToSlot(31);CHECK(Messages(Kind::CordState)==1);
 HostState();SendHook([]{S::OnPeerLeft(31);});CHECK(!S::Publish(&objects[0],31));Passed("CP",22);
 Host();p=Packet();coop::element::authorize=[&](void*){p.socket={};return true;};S::OnIntent(p,31);CHECK(connects==1&&objects[0].socket==&objects[1]);Passed("CP",23);
 for(int cause=0;cause<5;++cause){Host();p=Packet();coop::element::authorize=[cause](void*){if(cause==0)++session.peerEpoch;if(cause==1)++session.stops;if(cause==2)++sessionSerial;if(cause==3)session.ready=false;if(cause==4)S::OnPeerLeft(31);return true;};S::OnIntent(p,31);CHECK(!connects&&session.sent.empty());}
 Host();session.perSlot=true;session.links.fill(true);session.worlds.fill(true);session.generations.fill(1);p=Packet();coop::element::authorize=[](void*){session.worlds[31]=false;return true;};S::OnIntent(p,31);CHECK(session.IsSlotConnected(31)&&!session.IsSlotWorldReady(31)&&!connects&&session.sent.empty());Passed("CP",24);
 for(int cause=0;cause<8;++cause){Host();if(cause>=6)objects[0].attached=&objects[2];p=Packet();callback=[cause]{if(cause==0)++objects[0].serial;if(cause==1)++objects[511].serial;if(cause==2)++objects[1].serial;if(cause==3)++objects[510].serial;if(cause==4)objects[0].key=L"rekeyed";if(cause==5)objects[1].key=L"rekeyed socket";if(cause==6)++objects[2].serial;if(cause==7)objects[0].attached=&objects[3];};S::OnIntent(p,31);CHECK(connects==1&&session.sent.empty());}Passed("CP",25);
 for(int boundary=0;boundary<5;++boundary)for(int cause=0;cause<10;++cause){
  if(boundary==2)Reset();else if(boundary>=3)Host();else HostState();
  bool completed=true;
  if(boundary<2){SendHook([cause]{Change(cause);});completed=S::Publish(&objects[0],boundary==0?-1:31);CHECK(!completed&&!Cached());}
  else if(boundary==2){QueueIntent();SendHook([cause]{Change(cause);},false);S::Tick();CHECK(S::g_outbox.empty()&&Messages(Kind::CordIntent)==0);session.allowSend={};S::Tick();CHECK(Messages(Kind::CordIntent)==0);}
  else {auto input=Packet();if(boundary==3)coop::element::authorize=[cause](void*){Change(cause);return true;};else callback=[cause]{Change(cause);};S::OnIntent(input,31);CHECK(connects==(boundary==4?1:0)&&session.sent.empty());}
  Passed("CQ",boundary*10+cause+1);
 }
 puts("PASS actual cord publication/outbox/intent continuations: 75 cases");
}
