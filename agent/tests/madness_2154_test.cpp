#define TEST_CUSTOM_ACTION_REFS
#include "support_2135.h"
namespace test {inline bool overrideMasterClass=false;inline void* masterClass=nullptr;}
namespace ue_wrap::reflection {
inline int32_t SlotSerial(int32_t i){return i==1?test::actor.serial:test::ui.serial;}
inline void* ClassOf(void* p){return p==&test::actor&&test::overrideMasterClass?test::masterClass:p;}
}
#ifndef MADNESS_SYNC_SOURCE
#define MADNESS_SYNC_SOURCE "../../src/votv-coop/src/coop/world/madness_sync.cpp"
#endif
#include MADNESS_SYNC_SOURCE
#include <limits>
namespace test {
bool active=false,readOk=true;ue_wrap::madness::State state{true,true,true,7,20};
ue_wrap::madness::State applied;int applies=0,clears=0;
uint16_t people[coop::net::kMaxPeers]{10,11,12,13};std::function<void()> onClear;
uint32_t sessionSerial=1;bool clearOk=true;
}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace coop::roster_ledger {const Row& Get(int slot){static Row rows[coop::net::kMaxPeers];rows[slot].playerNo=test::people[slot];return rows[slot];}}
namespace ue_wrap::madness {
void* FindHost(){return test::active?&test::actor:nullptr;}
bool Read(void*,State& out){out=test::state;test::Callback();return test::readOk;}
bool Apply(View& v,const State& state,const std::function<bool()>& current){CHECK(!current || current());test::applied=state;++test::applies;v.started=state.playing;test::Callback();return !current || current();}
void Clear(View& v){if(!test::clearOk){++test::clears;return;}const bool active=v.started;v={};++test::clears;if(active){auto f=std::move(test::onClear);test::onClear={};if(f)f();}}
}
int main(){using namespace test;namespace S=coop::madness_sync;using P=coop::net::MadnessStatePayload;
 coop::net::Session session;S::Install(&session);S::Tick();CHECK(session.sent.size()==3&&!S::g_state.active);
 active=true;now+=100;session.sendOk=false;S::Tick();CHECK(S::g_state.active&&S::g_state.eventId==1&&S::g_pending);
 session.sendOk=true;S::Tick();CHECK(!S::g_pending&&session.sent.size()==6);P first=S::g_state;
 state.kills=8;now+=100;S::Tick();CHECK(S::g_state.kills==8&&S::g_state.sequence>first.sequence);
 active=false;actor.live=false;now+=100;S::Tick();CHECK(!S::g_state.active&&!S::g_state.playing);
 actor.live=true;active=true;now+=100;S::Tick();CHECK(S::g_state.eventId==2);
 S::QueueConnect(2);S::Tick();CHECK(session.sent.back().slot==2);S::OnPeerLeft(2);CHECK(!(S::g_pending&4));
 // Join/backpressure carries the event's age at send, not at its last heartbeat.
 now+=450;S::QueueConnect(2);S::Tick();
 P aged{};std::memcpy(&aged,session.sent.back().data.data(),sizeof(aged));CHECK(aged.elapsed==20.45f);
 const auto sampleAt=S::g_sentAt;const auto sampleAge=S::g_state.elapsed;
 session.sendOk=false;now+=100;S::QueueConnect(2);S::Tick();CHECK(S::g_pending&4);
 session.sendOk=true;now+=200;S::Tick();CHECK(!(S::g_pending&4));
 std::memcpy(&aged,session.sent.back().data.data(),sizeof(aged));
 CHECK(std::fabs(aged.elapsed-(sampleAge+float(now-sampleAt)/1000.f))<.0001f);
 S::OnDisconnect();session.mode=coop::net::Role::Client;S::Install(&session);
 S::Receive(first,1);CHECK(!S::g_have);S::Receive(first,0);now+=500;S::Tick();CHECK(applies==1&&applied.kills==7&&applied.elapsed==20.5f);
 session.testRtt=200;auto timed=first;timed.sequence++;S::Receive(timed,0);now+=250;S::Tick();CHECK(applied.elapsed==20.35f);
 for(int rtt:{-1,10001}){session.testRtt=rtt;timed.sequence++;S::Receive(timed,0);CHECK(S::g_transit==0);}
 first.sequence=timed.sequence;
 auto bad=first;bad.sequence++;bad.elapsed=std::numeric_limits<float>::quiet_NaN();S::Receive(bad,0);CHECK(S::g_state.sequence==first.sequence);
 bad=first;bad.sequence++;bad.reserved=1;S::Receive(bad,0);CHECK(S::g_state.sequence==first.sequence);
 auto stop=first;stop.sequence++;stop.active=stop.hud=stop.playing=0;stop.kills=0;S::Receive(stop,0);S::Tick();CHECK(!applied.active);
 S::Receive(first,0);CHECK(S::g_state.sequence==stop.sequence); // old start cannot restart an ended event
 stop.sequence++;stop.active=stop.hud=stop.playing=1;stop.eventId=2;S::Receive(stop,0);
 callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session&&!S::g_view.started);
 S::Install(&session);S::Receive(first,0);const auto beforeWorld=applies;++world;S::Tick();CHECK(applies==beforeWorld);S::Install(&session);CHECK(!S::g_have);
 // A native apply interrupted by a newer receipt cannot commit its old event as the new one.
 S::Receive(first,0);auto newer=first;newer.sequence++;newer.eventId++;newer.kills=12;
 callback=[&]{S::Receive(newer,0);};S::Tick();CHECK(S::g_state.sequence==newer.sequence && S::g_appliedEvent==first.eventId);
 S::Tick();CHECK(applied.kills==12 && S::g_appliedEvent==newer.eventId);
 // The moved-out view is cleaned by its owner after interruption, exactly once.
 S::OnDisconnect();S::Install(&session);S::Receive(first,0);S::Tick();auto beforeClear=clears;
 onClear=[&]{S::Receive(newer,0);};auto ended=newer;ended.sequence++;ended.eventId++;
 S::Receive(ended,0);S::Tick();CHECK(S::g_state.sequence==ended.sequence); // older nested receipt is refused
 coop::net::Session replacement,requested;replacement.mode=coop::net::Role::Client;
 onClear=[&]{S::Install(&replacement);S::Receive(first,0);};S::Install(&requested);CHECK(S::g_session==&replacement && S::g_have);
 S::OnDisconnect();session.mode=coop::net::Role::Host;session.sent.clear();session.allowSend={};S::Install(&session);
 callback=[]{++actor.serial;};S::Tick();CHECK(!S::g_have && session.sent.empty());
 now+=100;S::Tick();CHECK(S::g_have);session.sent.clear();S::QueueConnect(2);bool replaced=false;
 session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !replaced){replaced=true;++people[2];S::QueueConnect(2);}return true;};
 S::Tick();CHECK(replaced && (S::g_pending&4));session.allowSend={};S::Tick();CHECK(!(S::g_pending&4));
 S::QueueConnect(2);session.allowSend=[&](int,auto,const void*,int){S::OnDisconnect();return true;};S::Tick();CHECK(!S::g_session && !S::g_pending);
 session.allowSend={};S::Install(&session);now+=100;state.elapsed=NAN;S::Tick();CHECK(!S::g_have);state.elapsed=20;
 puts("PASS real madness controller: host start/end/restart/score, retry/join, sender/order/bounds, seek, world/reentrant teardown, latest receipt and recipient/source replacement");
 return 0;
}
