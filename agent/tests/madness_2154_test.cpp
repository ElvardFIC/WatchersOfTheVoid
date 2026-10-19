#include "support_2135.h"
#include "../../src/votv-coop/src/coop/world/madness_sync.cpp"
#include <limits>
namespace test {
bool active=false,readOk=true;ue_wrap::madness::State state{true,true,true,7,20};
ue_wrap::madness::State applied;int applies=0,clears=0;
}
namespace ue_wrap::madness {
void* FindHost(){return test::active?&test::actor:nullptr;}
bool Read(void*,State& out){out=test::state;test::Callback();return test::readOk;}
bool Apply(View& v,const State& state){test::applied=state;++test::applies;v.started=state.playing;test::Callback();return true;}
void Clear(View& v){v={};++test::clears;}
}
int main(){using namespace test;namespace S=coop::madness_sync;using P=coop::net::MadnessStatePayload;
 coop::net::Session session;S::Install(&session);S::Tick();CHECK(session.sent.size()==3&&!S::g_state.active);
 active=true;now+=100;session.sendOk=false;S::Tick();CHECK(S::g_state.active&&S::g_state.eventId==1&&S::g_pending);
 session.sendOk=true;S::Tick();CHECK(!S::g_pending&&session.sent.size()==6);P first=S::g_state;
 state.kills=8;now+=100;S::Tick();CHECK(S::g_state.kills==8&&S::g_state.sequence>first.sequence);
 active=false;actor.live=false;now+=100;S::Tick();CHECK(!S::g_state.active&&!S::g_state.playing);
 actor.live=true;active=true;now+=100;S::Tick();CHECK(S::g_state.eventId==2);
 S::QueueConnect(2);S::Tick();CHECK(session.sent.back().slot==2);S::OnPeerLeft(2);CHECK(!(S::g_pending&4));
 S::OnDisconnect();session.mode=coop::net::Role::Client;S::Install(&session);
 S::Receive(first,1);CHECK(!S::g_have);S::Receive(first,0);now+=500;S::Tick();CHECK(applies==1&&applied.kills==7&&applied.elapsed==20.5f);
 auto bad=first;bad.sequence++;bad.elapsed=std::numeric_limits<float>::quiet_NaN();S::Receive(bad,0);CHECK(S::g_state.sequence==first.sequence);
 bad=first;bad.sequence++;bad.reserved=1;S::Receive(bad,0);CHECK(S::g_state.sequence==first.sequence);
 auto stop=first;stop.sequence++;stop.active=stop.hud=stop.playing=0;stop.kills=0;S::Receive(stop,0);S::Tick();CHECK(!applied.active);
 S::Receive(first,0);CHECK(S::g_state.sequence==stop.sequence); // old start cannot restart an ended event
 stop.sequence++;stop.active=stop.hud=stop.playing=1;stop.eventId=2;S::Receive(stop,0);
 callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session&&!S::g_view.started);
 S::Install(&session);S::Receive(first,0);++world;S::Install(&session);CHECK(!S::g_have);
 puts("PASS real madness controller: host start/end/restart/score, retry/join, sender/order/bounds, seek, world/reentrant teardown");
}
