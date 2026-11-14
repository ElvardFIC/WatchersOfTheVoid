#define HAND_TOY_FIXTURE_ONLY
#include "hand_toy_2157_test.cpp"
#include <string>
int main(int argc,char** argv){
 using namespace test;namespace S=coop::hand_toy_audio;using coop::net::Role;using Packet=coop::net::HandToyAudioPayload;
 const std::string selected=argc>1?argv[1]:"";const auto run=[&](const char* name){return selected.empty() || selected==name;};
 coop::net::Session session;
 const auto reset=[&](Role role=Role::Host){
  callback=pitchCallback=altCallback={};coop::element::callback={};S::OnDisconnect();++sessionSerial;
  for(int slot=0;slot<coop::net::kMaxPeers;++slot)people[slot]=uint16_t(slot+10);
  ResetToy(session);invocations.clear();coop::element::reach=true;
  if(role==Role::Client){session.mode=role;coop::players::Registry::Get().peer=1;S::Install(&session);}
  S::Reconcile();
 };
 const auto packet=[&](int slot=1,uint8_t op=1){return Packet{7,1,op?1.2f:.15f,uint8_t(slot),op,uint8_t(!op),uint8_t(!op),people[slot]};};
 const auto callFor=[&](uint64_t id){ue_wrap::script_gate::Call c{};c.object=&actor;c.function=&ui;c.invocation=id;c.tag=S::kRmb;invocations.insert(id);return c;};
 if(run("schema")){
  for(bool plushUse:{false,true})for(int fault=0;fault<7;++fault){
   reset();local=&actor;auto c=callFor(900+fault);uint8_t action=6;
   alt=true;usePlayer=&actor;
   c.tag=plushUse?S::kPlushUse:S::kAction;
   c.locals=plushUse?reinterpret_cast<uint8_t*>(&usePlayer):&action;
   if(fault<4)inputSchemaFault=fault+1;
   if(fault==4)inputFlags=0;
   if(fault==5)inputFlags=0x180;
   if(fault==6)inputFlags=0x480;
   S::BeforeCapture(c);S::Capture(c);CHECK(S::g_pulses.empty());
  }
  puts("PASS held-toy inputs reject duplicate/overlap/inconsistent spans and non-input flags");
 }
 if(run("scope")){
  for(int cause=0;cause<5;++cause){reset();local=&actor;session.sendOk=false;ue_wrap::script_gate::Call c{};c.object=&actor;c.tag=S::kRmb;NativeCapture(c);CHECK(S::g_pulses.size()==1);
   if(cause==0)++sessionSerial;if(cause==1)session.active=false;if(cause==2)session.linked=false;if(cause==3)session.mode=Role::Client;if(cause==4)++world;
   S::Tick();CHECK(S::g_pulses.empty());
  }
  puts("PASS hand toy scope");
 }
 if(run("source")){
  reset();auto p=packet();S::Receive(p,1);auto count=squeaks;coop::element::callback=[&]{++session.generations[1];};S::Tick();
  CHECK(squeaks==count && S::g_pulses.empty());
  reset();mirrors[1]=nullptr;p=packet();S::Receive(p,1);++session.generations[1];S::Tick();CHECK(S::g_pulses.empty());
  S::Receive(p,1);CHECK(S::g_pulses.size()==1);
  reset();p=packet(1,0);S::Receive(p,1);coop::element::callback=[&]{++session.generations[1];};S::Tick();CHECK(!S::g_rows[1].authorized);
  puts("PASS hand toy source");
 }
 if(run("recipient")){
  reset();local=&actor;S::SampleLocal(now);S::g_poll=now+10000;++session.generations[2];S::Tick();
  for(const auto& sent:session.sent)CHECK(sent.slot!=2);
  session.sent.clear();S::QueueConnect(2);S::Tick();CHECK(session.sent.size()==1 && session.sent.front().slot==2);
  reset();local=&actor;ue_wrap::script_gate::Call c{};c.object=&actor;c.tag=S::kRmb;NativeCapture(c);++session.generations[2];S::Tick();
  for(const auto& sent:session.sent)if(sent.slot==2){Packet p{};std::memcpy(&p,sent.data.data(),sizeof(p));CHECK(!p.op);}
  reset();S::Receive(packet(),1);callback=[&]{++session.generations[2];};S::Tick();
  for(const auto& sent:session.sent)CHECK(sent.slot!=2);
  puts("PASS hand toy recipient");
 }
 if(run("frozen-hand")){
  for(int cause=0;cause<4;++cause){reset();local=&actor;auto c=callFor(100);S::BeforeCapture(c);
   if(cause==0)++generations[0];if(cause==1)local=nullptr;if(cause==2)++people[0];if(cause==3)coop::players::Registry::Get().peer=1;
   S::Capture(c);CHECK(S::g_pulses.empty());
  }
  reset();local=&actor;generations[0]=0;auto c=callFor(101);S::BeforeCapture(c);generations[0]=9;S::Capture(c);CHECK(S::g_pulses.empty());
  reset();local=&actor;c=callFor(102);c.tag=S::kPlushUse;c.locals=reinterpret_cast<uint8_t*>(&usePlayer);alt=true;S::BeforeCapture(c);alt=false;S::Capture(c);CHECK(S::g_pulses.size()==1);
  reset();local=&actor;c=callFor(103);uint8_t action=6;c.tag=S::kAction;c.locals=&action;S::BeforeCapture(c);action=5;S::Capture(c);CHECK(S::g_pulses.size()==1);
  puts("PASS hand toy frozen-hand");
 }
 if(run("frames")){
  reset();local=&actor;auto c=callFor(200);c.stack=&c;S::BeforeCapture(c);
  for(int cause=0;cause<4;++cause){auto wrong=c;if(cause==0)wrong.stack=&actor;if(cause==1)wrong.function=&actor;if(cause==2)++wrong.invocation;if(cause==3)++wrong.tag;
   S::Capture(wrong);CHECK(S::g_frames.size()==1 && S::g_pulses.empty());}
  S::Capture(c);CHECK(S::g_frames.empty() && S::g_pulses.size()==1);
  reset();local=&actor;c=callFor(201);c.tag=S::kPlushUse;c.locals=reinterpret_cast<uint8_t*>(&usePlayer);alt=true;
  altCallback=[&]{CHECK(S::g_frames.size()==1 && !S::g_frames.front().ready);S::BeforeCapture(c);};S::BeforeCapture(c);CHECK(S::g_frames.size()==1);
  invocations.clear();S::Tick();CHECK(S::g_frames.empty());
  reset();local=&actor;for(uint64_t id=1;id<=40;++id){auto frame=callFor(id);S::BeforeCapture(frame);}CHECK(S::g_frames.size()==32);
  puts("PASS hand toy frames");
 }
 if(run("admission")){
  for(auto role:{Role::Host,Role::Client}){reset(role);const int owner=role==Role::Host?1:0;auto p=packet(owner);S::g_pulses.resize(32);
   S::Receive(p,role==Role::Host?1:0);CHECK(!S::g_seen[owner][1]);S::g_pulses.clear();S::Receive(p,role==Role::Host?1:0);
   CHECK(S::g_seen[owner][1]==1 && S::g_pulses.size()==1);
  }
  puts("PASS hand toy admission");
 }
 if(run("payload")){
  reset();auto p=packet(1,0);S::Receive(p,1);S::Tick();CHECK(S::g_rows[1].painted.Get());
  ++people[1];auto external=packet(1,0);callback=[&]{external.sequence=777;external.value=9;};S::Receive(external,1);
  CHECK(external.sequence==777 && S::g_rows[1].state.sequence==1 && S::g_rows[1].state.value==.15f);
  puts("PASS hand toy payload");
 }
 if(run("budget")){
  reset();local=&actor;ue_wrap::script_gate::Call c{};c.object=&actor;c.tag=S::kRmb;for(int i=0;i<4;++i)NativeCapture(c);
  S::Tick();CHECK(session.sent.size()==64 && !S::g_pulses.empty());session.sent.clear();S::Tick();CHECK(session.sent.size()<=64 && S::g_pulses.empty());
  puts("PASS hand toy budget");
 }
 if(run("clock")){
  reset();S::Receive(packet(),1);auto count=squeaks;callback=[]{now+=1500;};S::Tick();CHECK(squeaks==count && S::g_pulses.empty());
  reset();S::Receive(packet(),1);coop::element::callback=[]{now+=1500;};count=squeaks;S::Tick();CHECK(squeaks==count && S::g_pulses.empty());
  reset();auto p=packet();S::Receive(p,1);--now;count=squeaks;S::Tick();CHECK(squeaks==count && S::g_pulses.empty() && S::g_seen[1][1]==1);
  S::Receive(p,1);CHECK(S::g_pulses.empty());++p.sequence;S::Receive(p,1);CHECK(S::g_pulses.size()==1);
  puts("PASS hand toy clock");
 }
 if(run("completion")){
  reset();local=&actor;S::SampleLocal(now);S::g_poll=now+10000;bool replaced=false;
  session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !replaced){replaced=true;++session.generations[2];S::QueueConnect(2);}return true;};
  S::Tick();CHECK(replaced && (S::g_rows[0].pending&(1u<<2)));session.allowSend={};session.sent.clear();S::Tick();
  CHECK(session.sent.size()==1 && session.sent.front().slot==2 && !(S::g_rows[0].pending&(1u<<2)));
  puts("PASS hand toy completion");
 }
 if(run("schema-owner")){
  for(int cause=0;cause<6;++cause){reset();local=&actor;auto c=callFor(800+cause);
   uint8_t action=6;c.tag=S::kAction;c.locals=&action;
   if(cause==0)missingInput=true;if(cause==1)parameterSize=4;if(cause==2)parameterOffset=16;
   if(cause==3)frameSize=0;if(cause==4)dispatchFunction=&actor;if(cause==5)c.tag=123;
   S::BeforeCapture(c);S::Capture(c);CHECK(S::g_pulses.empty());
  }
  reset();local=&actor;auto c=callFor(820);c.tag=S::kPlushUse;c.locals=reinterpret_cast<uint8_t*>(&usePlayer);
  usePlayer=&ui;alt=true;S::BeforeCapture(c);S::Capture(c);CHECK(S::g_pulses.empty());
  reset();local=&actor;c=callFor(821);S::BeforeCapture(c);dispatchFunction=&actor;S::Capture(c);CHECK(S::g_pulses.empty());
  reset();local=&actor;c=callFor(822);S::BeforeCapture(c);++actor.serial;S::Capture(c);CHECK(S::g_pulses.empty());
  puts("PASS held-toy exact dispatch ownership, input spans/sizes, known tags and original local user");
 }
 return 0;
}
