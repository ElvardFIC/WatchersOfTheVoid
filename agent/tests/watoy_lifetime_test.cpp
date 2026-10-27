#define WATOY_FIXTURE_ONLY
#include "watoy_sync_test.cpp"
#include <string>
int main(int argc,char** argv){
 using namespace test;using coop::net::Role;
 const std::string selected=argc>1?argv[1]:"";
 const auto run=[&](const char* name){return selected.empty() || selected==name;};
 coop::net::Session session;
 const auto reset=[&](Role role=Role::Host){
  callback=toyCallback={};coop::element::callback={};S::OnDisconnect();session={};key=L"watoy";
  ++sessionSerial;Start(session,role);invocations.clear();
 };
 const auto callFor=[&](uint64_t id){ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&ui;call.invocation=id;invocations.insert(id);return call;};
 if(run("uncertain")){
  reset();activationReady=false;S::Receive(World(1,1),1);
  CHECK(!activations && S::g_out.size()==1);
  activationReady=true;S::Tick();CHECK(activations==1 && std::none_of(S::g_out.begin(),S::g_out.end(),[](const auto& row){return row.state.op==S::ActivateWorld;}));
  reset();const auto before=activations;activationComplete=false;S::Receive(World(1,1),1);
  CHECK(activations==before+1 && !state.active && S::g_out.empty());
  S::Tick();S::Tick();CHECK(activations==before+1 && S::g_out.empty());
  puts("PASS watoy activation retries only before native dispatch, consumes uncertain completion");
 }
 if(run("function")){
  for(bool client:{false,true})for(int fault=0;fault<3;++fault){
   reset(client?Role::Client:Role::Host);auto call=callFor(90);
   if(fault==0)call.function=&actor;
   if(fault==1)activationFunction=&actor;
   if(fault==2)activationParameters=true;
   S::BeforeActivate(call);state.active=true;S::AfterActivate(call);
   CHECK(S::g_activation.empty() && S::g_out.empty());
  }
  reset();auto call=callFor(91);S::BeforeActivate(call);activationFunction=&actor;state.active=true;S::AfterActivate(call);
  CHECK(S::g_out.empty());
  reset();call=callFor(92);S::BeforeActivate(call);activationParameters=true;state.active=true;S::AfterActivate(call);
  CHECK(S::g_out.empty());
  puts("PASS watoy capture exact dispatch and parameterless schema before client requests and after native callbacks");
 }
 if(run("scope")){
  for(int cause=0;cause<5;++cause){reset();session.sendOk=false;S::QueueCue(&actor);
   if(cause==0)++sessionSerial;if(cause==1)session.active=false;if(cause==2)session.linked=false;
   if(cause==3)session.mode=Role::Client;if(cause==4)++world;
   S::Tick();CHECK(S::g_out.empty());
  }
  reset();S::g_worldViews[key].presentation=std::make_shared<ue_wrap::watoy::Presentation>();S::g_worldViews[key].presentation->actor.Set(&actor);coop::net::Session replacement;
  callback=[&]{S::Install(&replacement);S::QueueCue(&actor);};++sessionSerial;S::Install(&session);
  CHECK(S::g_session==&replacement && S::g_out.size()==1);S::OnDisconnect();
  puts("PASS watoy scope");
 }
 if(run("post")){
  reset();auto call=callFor(100);call.stack=&call;S::BeforeActivate(call);state.active=true;
  for(int cause=0;cause<4;++cause){auto wrong=call;
   if(cause==0)++wrong.invocation;if(cause==1)wrong.function=&actor;if(cause==2)wrong.stack=&actor;if(cause==3)wrong.object=&ui;
   S::AfterActivate(wrong);CHECK(S::g_activation.size()==1 && S::g_out.empty());
  }
  S::AfterActivate(call);CHECK(S::g_activation.empty() && S::g_out.size()==1);
  reset();call=callFor(101);S::BeforeActivate(call);++ui.serial;state.active=true;S::AfterActivate(call);CHECK(S::g_out.empty());
  reset();call=callFor(102);S::BeforeActivate(call);invocations.erase(call.invocation);state.active=true;S::AfterActivate(call);CHECK(S::g_out.empty());
  puts("PASS watoy post");
 }
 if(run("reservation")){
  reset();auto call=callFor(200);callback=[&]{CHECK(S::g_activation.size()==1 && !S::g_activation.front().ready);S::BeforeActivate(call);};
  S::BeforeActivate(call);CHECK(S::g_activation.size()==1 && S::g_activation.front().ready);
  invocations.clear();S::Tick();CHECK(S::g_activation.empty());
  reset();for(uint64_t i=1;i<=40;++i){auto frame=callFor(i);S::BeforeActivate(frame);}CHECK(S::g_activation.size()==32);
  reset();call=callFor(201);callback=[&]{S::OnDisconnect();S::Install(&session);};S::BeforeActivate(call);CHECK(S::g_activation.empty());
  puts("PASS watoy reservation");
 }
 if(run("source")){
  for(int cause=0;cause<3;++cause){reset();auto request=World(1,1);auto count=activations;
   coop::element::callback=[&,cause]{if(cause==0)++roster[1].playerNo;if(cause==1)++session.generations[1];if(cause==2)S::OnPeerLeft(1);};
   S::Receive(request,1);CHECK(activations==count);
  }
  reset();found=false;S::Receive(World(1,1),1);CHECK(S::g_out.size()==1);++session.generations[1];found=true;
  auto count=activations;S::Tick();CHECK(activations==count && S::g_out.empty());
  reset();mirror=&ui;auto hand=Hand(1,1,1);S::Receive(hand,1);coop::element::callback=[&]{++session.generations[1];};S::ApplyHands();
  CHECK(!S::g_hands[1].authorized);
  puts("PASS watoy source");
 }
 if(run("recipient")){
  reset();S::SampleWorld(&actor,now);++session.generations[2];S::FlushRows();
  for(const auto& sent:session.sent)CHECK(sent.slot!=2);
  session.sent.clear();S::QueueConnect(2);S::FlushRows();CHECK(session.sent.size()==1 && session.sent.front().slot==2);
  reset();S::QueueCue(&actor);++session.generations[2];S::Drain(now);
  for(const auto& sent:session.sent)CHECK(sent.slot!=2);
  reset();localHand=&actor;S::SampleLocal(now);++session.generations[2];S::FlushRows();
  for(const auto& sent:session.sent)CHECK(sent.slot!=2);
  puts("PASS watoy recipient");
 }
 if(run("flush")){
  for(bool hand:{false,true}){
   reset();if(hand){localHand=&actor;S::SampleLocal(now);}else S::SampleWorld(&actor,now);
   session.allowSend=[&](int,auto,const void*,int){S::OnDisconnect();S::Install(&session);return true;};
   S::FlushRows();CHECK(session.sent.size()==1 && S::g_worldRows.empty() && !S::g_hands[0].state.sequence);
  }
  reset();S::SampleWorld(&actor,now);const auto old=S::g_worldRows.at(key).state.sequence;bool replaced=false;
  session.allowSend=[&](int,auto,const void*,int){if(!replaced){replaced=true;state.volume=.8f;S::SampleWorld(&actor,now);}return true;};
  S::FlushRows();CHECK(session.sent.size()==1 && S::g_worldRows.at(key).state.sequence!=old && S::g_worldRows.at(key).pending==S::Mask());
  session.allowSend={};S::FlushRows();CHECK(!S::g_worldRows.at(key).pending);
  puts("PASS watoy flush");
 }
 if(run("drain")){
  reset();S::QueueCue(&actor);S::QueueCue(&actor);session.allowSend=[&](int,auto,const void*,int){
   S::OnDisconnect();S::Install(&session);S::QueueCue(&actor);return false;
  };
  S::Drain(now);CHECK(S::g_out.size()==1 && S::g_out.front().state.sequence==0);
  session.allowSend={};S::Drain(now);CHECK(S::g_out.empty());
  reset();mirror=&ui;S::Receive(Hand(1,1,1,4),1);callback=[]{S::OnPeerLeft(1);};S::Drain(now);
  CHECK(session.sent.empty() && S::g_out.empty());
  reset();mirror=&ui;S::Receive(Hand(1,1,1,4),1);S::Receive(Hand(2,1,1,4),1);
  callback=[]{S::OnPeerLeft(1);};S::Drain(now);CHECK(session.sent.empty() && S::g_out.empty());
  puts("PASS watoy drain");
 }
 if(run("prune")){
  reset();S::SampleWorld(&actor,now);const auto old=S::g_worldRows.at(key).state.sequence;
  toy=false;toyCallback=[&]{toy=true;state.volume=.9f;S::SampleWorld(&actor,now);};S::PruneRows(now);
  CHECK(S::g_worldRows.contains(key) && S::g_worldRows.at(key).state.sequence!=old);
  reset(Role::Client);S::Receive(World(1),0);S::g_worldViews[key].presentation=std::make_shared<ue_wrap::watoy::Presentation>();S::g_worldViews[key].presentation->actor.Set(&actor);now+=10001;
  callback=[&]{S::Receive(World(2),0);S::g_worldViews[key].presentation=std::make_shared<ue_wrap::watoy::Presentation>();S::g_worldViews[key].presentation->actor.Set(&actor);};S::PruneRows(now);
  CHECK(S::g_worldRows.at(key).state.sequence==2 && S::g_worldViews.at(key).Actor().Get());
  reset();S::SampleWorld(&actor,now);toyCallback=[&]{S::OnDisconnect();S::Install(&session);};S::PruneRows(now);CHECK(S::g_worldRows.empty());
  puts("PASS watoy prune");
 }
 if(run("admission")){
  reset(Role::Client);S::g_out.resize(64);auto cue=World(1,2);S::Receive(cue,0);CHECK(!S::g_lastCue);
  S::g_out.clear();S::Receive(cue,0);CHECK(S::g_lastCue==1 && S::g_out.size()==1);
  reset();S::g_out.resize(64);auto cueHand=Hand(1,1,1,4);S::Receive(cueHand,1);CHECK(!S::g_seen[1]);
  S::g_out.clear();S::Receive(cueHand,1);CHECK(S::g_seen[1]==1 && S::g_out.size()==1);
  puts("PASS watoy admission");
 }
 if(run("payload")){
  reset();readable=false;auto external=World(1,1);coop::element::callback=[&]{external.sequence=777;external.op=4;};
  S::Receive(external,1);CHECK(S::g_out.size()==1 && S::g_out.front().state.sequence==1 && S::g_out.front().state.op==1);
  puts("PASS watoy payload");
 }
 if(run("original-hand")){
  for(int cause=0;cause<5;++cause){reset();localHand=&actor;handGeneration=3;
   auto call=callFor(900+cause);S::BeforeActivate(call);state.active=true;
   if(cause==0)localHand=nullptr;if(cause==1)++handGeneration;
   if(cause==2)++roster[0].playerNo;if(cause==3)++session.generations[0];
   if(cause==4)++actor.serial;
   S::AfterActivate(call);CHECK(S::g_out.empty());
  }
  reset();localHand=&actor;handGeneration=3;auto call=callFor(910);state.active=false;
  toyCallback=[]{localHand=nullptr;};S::BeforeActivate(call);state.active=true;
  S::AfterActivate(call);CHECK(S::g_out.empty()); // Initial type lookup cannot reroute a hand to the world.
  reset();localHand=&actor;handGeneration=3;call=callFor(911);S::BeforeActivate(call);state.active=true;
  S::AfterActivate(call);CHECK(S::g_out.size()==1 && S::g_out.front().state.generation==3);
  handGeneration=4;S::Drain(now);CHECK(S::g_out.empty());
  puts("PASS watoy original hand, occupant, connection and class across native activation and delivery");
 }
 return 0;
}
