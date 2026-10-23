#define TEST_CUSTOM_ACTION_REFS
#define main PriorPropActionsMain
#include "prop_action_2143_test.cpp"
#undef main
namespace test {
Obj cls{8},otherClass{9},mesh{10},otherPlayer{11};void* actorType=&cls;
}
namespace ue_wrap::reflection {
int32_t SlotSerial(int32_t i){using namespace test;for(auto* o:{&actor,&ui,&damageFn,&sliceFn,&mreFn,&spawnFn,&mreOwner,&sawFn,&cls,&otherClass,&mesh,&otherPlayer})if(o->id==i)return o->serial;return 0;}
void* ClassOf(void* a){return a==&test::actor?test::actorType:a;}
}
const char* actionCase="setup";
#undef CHECK
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s line %d: %s\n",actionCase,__LINE__,#x);std::exit(1);}}while(0)
using Role=coop::net::Role;using V=ue_wrap::script_gate::Verdict;using Call=ue_wrap::script_gate::Call;
void Start(Role role=Role::Client){
 using namespace test;Reset();session.mode=role;actorType=&cls;meshActor=&mesh;
 for(auto* o:{&actor,&ui,&damageFn,&sliceFn,&mreFn,&spawnFn,&mreOwner,&sawFn,&cls,&otherClass,&mesh,&otherPlayer})o->live=true;
 coop::players::Registry::Get().local=&ui;
 for(int i=0;i<4;++i){auto& row=coop::roster_ledger::testRows[i];row.playerNo=uint16_t(10+i);row.bornGeneration=1;}
 ++coop::net::session_serial::testSerial;A::EnsureWorld();
}
coop::net::PropActionIntentPayload Payload(uint8_t action=3,float damage=0){coop::net::PropActionIntentPayload p{};coop::net::WireKeyFromString(L"box",p.key);p.action=action;p.damage=damage;return p;}
coop::net::PropActionIntentPayload Sent(size_t i){coop::net::PropActionIntentPayload p{};std::memcpy(&p,session.sent.at(i).data.data(),sizeof(p));return p;}
test::Params args{};
Call Open(){args={&test::ui,0,{}};Call c{};c.object=&test::actor;c.function=&test::mreFn;c.locals=reinterpret_cast<uint8_t*>(&args);c.invocation=1;return c;}
Call Spawn(){auto c=Open();c.function=&test::spawnFn;c.invocation=2;return c;}
bool Queue(float damage=1){return A::SendIntent(&test::actor,A::kDamage,damage,{},false);}
template<class Fn>void Case(const char* name,const char* selected,Fn fn,int& count){if(selected && std::strcmp(selected,name))return;actionCase=name;fn();++count;}
#ifndef PROP_ACTION_DELIVERY_NO_MAIN
int main(int argc,char** argv){using namespace test;const char* selected=argc>1?argv[1]:nullptr;int count=0;
 Case("owned-send",selected,[&]{Start();session.beforeSend=[]{A::OnDisconnect();A::Install(&session);CHECK(Queue(22));};
  CHECK(!Queue(11));CHECK(session.sent.size()==1 && Sent(0).damage==11 && A::g_pending.size()==1);
  A::Tick();CHECK(session.sent.size()==2 && Sent(1).damage==22 && A::g_pending.empty());
 },count);
 Case("nested-fifo",selected,[&]{Start();session.beforeSend=[]{CHECK(Queue(22));A::Tick();};
  CHECK(Queue(11));CHECK(session.sent.size()==2 && Sent(0).damage==11 && Sent(1).damage==22 && A::g_pending.empty());
 },count);
 Case("refused-order",selected,[&]{Start();session.sendOk=false;CHECK(Queue(11));CHECK(Queue(22));
  CHECK(session.sent.empty() && A::g_pending.size()==2);session.sendOk=true;A::Tick();
  CHECK(session.sent.size()==2 && Sent(0).damage==11 && Sent(1).damage==22);
 },count);
 Case("bounded-drain",selected,[&]{Start();session.sendOk=false;for(int i=0;i<20;++i)CHECK(Queue(float(i+1)));
  session.sendOk=true;A::Tick();CHECK(session.sent.size()==8 && A::g_pending.size()==12);
  A::Tick();CHECK(session.sent.size()==16);A::Tick();CHECK(session.sent.size()==20 && A::g_pending.empty());
  Start();session.sendOk=false;for(int i=0;i<256;++i)CHECK(Queue());CHECK(!Queue() && A::g_pending.size()==256);
 },count);
 Case("context-retirement",selected,[&]{for(int cause=0;cause<6;++cause){Start();session.sendOk=false;CHECK(Queue());
  if(cause==0)++coop::net::session_serial::testSerial;if(cause==1)session.mode=Role::Host;if(cause==2)session.active=false;
  if(cause==3)session.linked=false;if(cause==4)++world;if(cause==5){A::OnDisconnect();A::Install(&session);}
  session.sendOk=true;A::Tick();CHECK(session.sent.empty() && A::g_pending.empty());}
 },count);
 Case("capture-retirement",selected,[&]{for(int cause=0;cause<4;++cause){Start();keyCallback=[&,cause]{
  if(cause==0)A::OnDisconnect();if(cause==1)++actor.serial;if(cause==2)actorType=&otherClass;if(cause==3)++coop::net::session_serial::testSerial;};
  CHECK(!Queue() && session.sent.empty() && A::g_pending.empty());}
 },count);
 Case("queued-target",selected,[&]{for(int cause=0;cause<5;++cause){Start();session.sendOk=false;CHECK(Queue());
  if(cause==0)++actor.serial;if(cause==1)actorType=&otherClass;if(cause==2)propKey=L"changed";
  if(cause==3)A::g_pending.front().queuedAt=A::NowMs()+10000;if(cause==4)A::g_pending.front().queuedAt=A::NowMs()-6000;
  session.sendOk=true;A::Tick();CHECK(session.sent.empty() && A::g_pending.empty());}
 },count);
 Case("key-callback-retirement",selected,[&]{Start();session.sendOk=false;CHECK(Queue(11));
  keyCallback=[]{A::OnDisconnect();A::Install(&session);CHECK(Queue(22));};session.sendOk=true;A::Tick();
  CHECK(session.sent.empty() && A::g_pending.size()==1);A::Tick();CHECK(session.sent.size()==1 && Sent(0).damage==22);
 },count);
 Case("mre-request",selected,[&]{Start();auto c=Open();session.sendOk=false;
  CHECK(A::OnMreOpen(c)==V::Cancel && A::g_pending.size()==1 && cues==1);
  A::g_lastMreRequestAt=0;CHECK(A::OnMreOpen(c)==V::Cancel && A::g_pending.size()==1 && cues==1);
  handAxis=true;session.sendOk=true;A::Tick();CHECK(session.sent.empty() && A::g_pending.empty());
  Start();c=Open();c.fromOurCode=true;CHECK(A::OnMreOpen(c)==V::Cancel && session.sent.empty());
  c.fromOurCode=false;args.actor=&actor;CHECK(A::OnMreOpen(c)==V::Cancel && session.sent.empty());
  c.locals=nullptr;CHECK(A::OnMreOpen(c)==V::Cancel && session.sent.empty());
 },count);
 Case("cue-retirement",selected,[&]{Start();session.beforeSend=[]{A::OnDisconnect();};
  CHECK(A::OnMreOpen(Open())==V::Cancel && cues==0);
  Start();locationCallback=[]{++world;};CHECK(A::OnMreOpen(Open())==V::Cancel && cues==0);
 },count);
 Case("hand-mre-scope",selected,[&]{Start();localHand=&actor;auto c=Spawn();
  CHECK(!A::IsLocalMreOutput(&actor,&spawnFn));CHECK(A::OnMreSpawn(c)==V::Run);
  CHECK(A::IsLocalMreOutput(&actor,&spawnFn));CHECK(!A::IsLocalMreOutput(&actor,&mreFn));
  A::AfterMreSpawn(Open());CHECK(A::IsLocalMreOutput(&actor,&spawnFn));A::AfterMreSpawn(c);CHECK(!A::IsLocalMreOutput(&actor,&spawnFn));
  Start();CHECK(A::OnMreSpawn(Spawn())==V::Cancel);
 },count);
 Case("hand-cleared-mre",selected,[&]{Start();localHand=&actor;auto c=Open();CHECK(A::OnMreOpen(c)==V::Run);
  localHand=nullptr;auto spawn=Spawn();CHECK(A::OnMreSpawn(spawn)==V::Run && A::IsLocalMreOutput(&actor,&spawnFn));
  A::AfterMreSpawn(spawn);A::AfterMreOpen(c);CHECK(A::OnMreSpawn(spawn)==V::Cancel);
 },count);
 Case("scope-retirement",selected,[&]{for(int cause=0;cause<9;++cause){Start();localHand=&actor;CHECK(A::OnMreSpawn(Spawn())==V::Run);
  if(cause==0)invocations.clear();if(cause==1)++actor.serial;if(cause==2)++spawnFn.serial;if(cause==3)++mreOwner.serial;
  if(cause==4)actorType=&otherClass;if(cause==5)++coop::net::session_serial::testSerial;if(cause==6)++coop::roster_ledger::testRows[1].bornGeneration;
  if(cause==7)session.active=false;if(cause==8)functionOwner=&otherClass;
  CHECK(!A::IsLocalMreOutput(&actor,&spawnFn));}
 },count);
 Case("host-mre-once",selected,[&]{Start(Role::Host);frameCallback=[]{A::OnReliable(Payload(),1);};
  A::OnReliable(Payload(),1);CHECK(mreCalls==1);A::OnReliable(Payload(),1);CHECK(mreCalls==1);
  Start(Role::Host);parametersOk=false;A::OnReliable(Payload(),1);CHECK(mreCalls==0 && A::g_mreAttempts.empty());
  parametersOk=true;A::OnReliable(Payload(),1);CHECK(mreCalls==1);
 },count);
 Case("host-mre-preparation",selected,[&]{for(int stage=0;stage<4;++stage)for(int cause=0;cause<9;++cause){Start(Role::Host);
  auto retire=[&,cause]{if(cause==0)++coop::net::session_serial::testSerial;if(cause==1)session.mode=Role::Client;if(cause==2)++actor.serial;
   if(cause==3)actorType=&otherClass;if(cause==4)++mreFn.serial;if(cause==5)++mreOwner.serial;if(cause==6)++mesh.serial;
   if(cause==7)coop::players::Registry::Get().puppet.actor=&otherPlayer;if(cause==8)++coop::roster_ledger::testRows[1].bornGeneration;};
  if(stage==0)frameCallback=retire;if(stage==1)locationCallback=retire;if(stage==2)bindCallback=retire;if(stage==3)hitCallback=retire;
  // Mesh capture follows frame construction; retirement there is a fresh mesh,
  // while actor/function/player/source retirement must always stop the old call.
  A::OnReliable(Payload(),1);CHECK(mreCalls==int(stage==0 && cause==6));}
 },count);
 Case("host-authorization",selected,[&]{for(int cause=0;cause<6;++cause){Start(Role::Host);coop::element::callback=[&,cause]{
  if(cause==0)session.ready=false;if(cause==1)++coop::roster_ledger::testRows[1].playerNo;if(cause==2)++coop::net::session_serial::testSerial;
  if(cause==3)++actor.serial;if(cause==4)propKey=L"new-package";if(cause==5)coop::players::Registry::Get().puppet.actor=&otherPlayer;};A::OnReliable(Payload(),1);CHECK(mreCalls==0);}
 },count);
 Case("safe-requeue",selected,[&]{Start(Role::Host);safe=true;auto c=Open();A::AfterSafeOpened(c);
  keyCallback=[&]{A::AfterSafeOpened(c);};A::Tick();CHECK(records==0 && A::g_safePublish.size()==1);
  A::Tick();CHECK(records==1 && A::g_safePublish.empty());
  A::AfterSafeOpened(c);publishCallback=[&]{A::AfterSafeOpened(c);};A::Tick();CHECK(records==2 && A::g_safePublish.size()==1);
  A::Tick();CHECK(records==3 && A::g_safePublish.empty());
 },count);
 Case("safe-retirement",selected,[&]{for(int cause=0;cause<4;++cause){Start(Role::Host);safe=true;auto c=Open();
  keyCallback=[&,cause]{if(cause==0)A::OnDisconnect();if(cause==1)++actor.serial;if(cause==2)actorType=&otherClass;if(cause==3)++coop::net::session_serial::testSerial;};
  A::AfterSafeOpened(c);CHECK(A::g_safePublish.empty());}
 },count);
 CHECK(count>0);std::printf("PASS prop action delivery: %d cases, owned FIFO/reentry/lifecycle, exact MRE spawn provenance, one native opening and safe-record completion ownership\n",count);
}
#endif
