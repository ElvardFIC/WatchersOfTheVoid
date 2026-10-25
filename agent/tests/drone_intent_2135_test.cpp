#include "support_2135.h"
#include "../../src/votv-coop/src/coop/interactables/drone_call_intent.cpp"
namespace C=coop::drone_call_intent;
namespace test {inline bool leave=false,lid=false,departure=true,keyboard=true,writeOk=true;inline int writes=0,flights=0;}
namespace ue_wrap::drone_console {
int32_t LiveConsoles(void** out,int32_t){out[0]=&test::actor;return 1;}
bool PlayChangeCue(void*){return true;}
bool IsLidOpen(void*){return test::lid;}bool IsCursorOnKeyboard(void*){return test::keyboard;}
bool IsCursorOnDeparture(void*){return test::departure;}
bool TriggerFly(void*){++test::flights;test::Callback();return true;}
}
namespace ue_wrap::drone {
void* Find(){return &test::actor;}
bool ReadDeparturePreference(void*,bool& v){v=test::leave;return true;}
bool WriteDeparturePreference(void*,bool v){if(!test::writeOk)return false;test::leave=v;++test::writes;return true;}
}
coop::net::Session session;
void Reset(){C::OnDisconnect();session={};test::actor={};test::leave=test::lid=false;test::departure=test::keyboard=test::writeOk=true;test::writes=test::flights=0;test::callback={};coop::element::body=coop::element::reach=true;coop::element::callback={};C::Install(&session);}
int main(){using namespace test;coop::net::DroneFlyIntentPayload p{};p.verb=1;p.leaveAfter5min=1;
 Reset();C::OnDroneFlyIntent(session,p,1);C::Tick(session);CHECK(leave&&writes==1&&flights==0);C::OnDroneFlyIntent(session,p,1);C::Tick(session);CHECK(leave); // duplicate sets, never toggles
 Reset();auto fly=p;fly.verb=0;C::OnDroneFlyIntent(session,fly,1);C::Tick(session);CHECK(flights==0);lid=true;C::OnDroneFlyIntent(session,fly,1);C::Tick(session);CHECK(flights==1);
 Reset();coop::element::reach=false;C::OnDroneFlyIntent(session,p,1);C::Tick(session);CHECK(writes==0);
 Reset();coop::element::body=false;C::OnDroneFlyIntent(session,p,1);C::Tick(session);CHECK(writes==0);coop::element::body=true;C::Tick(session);CHECK(writes==1);
 Reset();C::OnDroneFlyIntent(session,p,1);now+=4000;C::Tick(session);CHECK(writes==0);
 for(int i=0;i<4;++i){Reset();C::OnDroneFlyIntent(session,p,1);coop::element::callback=[i]{if(i==0)C::OnDisconnect();if(i==1)C::OnPeerLeft(1);if(i==2)++world;if(i==3)++actor.serial;};C::Tick(session);CHECK(writes==0);}
 Reset();lid=true;C::OnDroneFlyIntent(session,fly,1);callback=[]{C::OnDisconnect();};C::Tick(session);CHECK(flights==1&&C::g_pending[1].empty());
 Reset();auto bad=p;bad.leaveAfter5min=2;C::OnDroneFlyIntent(session,bad,1);C::OnDroneFlyIntent(session,p,0);C::Tick(session);CHECK(writes==0);
 Reset();writeOk=false;C::OnDroneFlyIntent(session,p,1);C::Tick(session);CHECK(writes==0&&!C::g_pending[1].empty());writeOk=true;C::Tick(session);CHECK(writes==1);
 Reset();session.mode=coop::net::Role::Client;uint8_t local[32]{};local[0]=4;void* who=coop::players::Registry::Get().Local();std::memcpy(local+8,&who,sizeof(who));ue_wrap::script_gate::Call c{};c.object=&actor;c.locals=local;
 CHECK(C::OnActionPre(c)==ue_wrap::script_gate::Verdict::Cancel);CHECK(!leave&&C::g_pending[0].size()==1);session.sendOk=false;C::Tick(session);CHECK(C::g_pending[0].size()==1);session.sendOk=true;C::Tick(session);CHECK(C::g_pending[0].empty()&&session.sent.size()==1);
 coop::net::DroneFlyIntentPayload sent{};std::memcpy(&sent,session.sent[0].data.data(),sizeof(sent));CHECK(sent.verb==coop::net::drone_verb::kLeaveTimer);
 Reset();auto toggle=p;toggle.verb=coop::net::drone_verb::kLeaveTimer;
 C::OnDroneFlyIntent(session,toggle,1);C::OnDroneFlyIntent(session,toggle,1);
 C::Tick(session);CHECK(leave&&writes==1);C::Tick(session);CHECK(!leave&&writes==2);
 puts("PASS actual drone intents: native timer priority/closed lid, reach, set semantics, backpressure, expiry, reentry, peer/world/serial lifetime");
}
