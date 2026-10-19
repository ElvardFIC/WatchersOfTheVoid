#include "support_2135.h"
#include "../../src/votv-coop/src/coop/world/desk_short_circuit_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
namespace test { bool active=false, available=true, callOk=true; int starts=0; }
namespace ue_wrap::console_desk {
void* Instance(){return test::available?&test::actor:nullptr;}
bool ReadShortCircuit(void* a,bool& active){active=test::active;return a&&test::available&&test::fields;}
}
namespace ue_wrap::component_calls {
bool CallParamlessNamed(void* a,const wchar_t* name){
 CHECK(a==&test::actor&&std::wstring(name)==L"shortCircuit");
 script_gate::Call c{};c.object=a;c.fromOurCode=true;
 CHECK(coop::desk_short_circuit_sync::OnShortCircuit(c)==script_gate::Verdict::Run);
 if(!test::callOk)return false;
 ++test::starts;test::active=true;test::Callback();return true;
}
}
int main(){
 namespace S=coop::desk_short_circuit_sync;namespace N=coop::net;
 CHECK(!N::IsClientRelayableReliableKind(N::ReliableKind::DeskShortCircuit));
 CHECK(N::LaneForKind(N::ReliableKind::DeskShortCircuit)==N::Lane::Normal);
 using V=ue_wrap::script_gate::Verdict;using P=N::DeskShortCircuitPayload;
 N::Session s;S::Install(&s);S::ResetWorld();
 auto tick=[&]{S::g_nextTick=0;S::Tick();};
 auto count=[&](int op){int n=0;for(auto& m:s.sent){P p{};std::memcpy(&p,m.data.data(),sizeof(p));n+=p.op==op;}return n;};
 P p{1,0,{}};S::Receive(p,1);tick();CHECK(test::starts==1&&count(1)==3&&count(2)==1);
 S::Receive(p,1);tick();CHECK(test::starts==1&&count(1)==3&&count(2)==2); // duplicate request only ACK
 // Already-active requests must not replay effects; the next circuit must still publish.
 p.sequence=2;S::Receive(p,1);tick();CHECK(test::starts==1&&count(1)==3);
 test::active=false;p.sequence=3;S::Receive(p,1);tick();CHECK(test::starts==2&&count(1)==6);
 test::active=false;coop::element::reach=false;p.sequence=4;S::Receive(p,1);tick();CHECK(test::starts==2);
 coop::element::reach=true;coop::element::body=false;p.sequence=5;S::Receive(p,1);tick();CHECK(S::g_peers[1].request.p.sequence==5);
 coop::element::body=true;tick();CHECK(test::starts==3);
 test::active=false;s.sendOk=false;p.sequence=6;S::Receive(p,1);tick();CHECK(test::starts==4&&S::g_oweEffect);
 s.sendOk=true;tick();CHECK(!S::g_oweEffect&&!S::g_peers[1].oweAck&&test::starts==4);
 test::active=false;test::callOk=false;p.sequence=7;S::Receive(p,1);tick();CHECK(S::g_peers[1].request.p.sequence==7);
 S::g_peers[1].request.since=S::Now()-9000;tick();CHECK(!S::g_peers[1].request.p.sequence);test::callOk=true;
 p.sequence=8;S::Receive(p,1);coop::element::callback=[]{++test::world;};tick();CHECK(test::starts==4);
 tick();CHECK(!S::g_peers[1].ack&&!S::g_oweEffect); // old world request discarded
 S::OnPeerLeft(1);S::OnDisconnect();++test::world;s={};s.mode=N::Role::Client;S::Install(&s);
 ue_wrap::script_gate::Call c{};c.object=&test::actor;
 CHECK(S::OnShortCircuit(c)==V::Cancel);CHECK(S::g_request.p.sequence==1);
 s.sendOk=false;tick();CHECK(s.sent.empty()&&S::g_request.p.sequence);
 s.sendOk=true;tick();CHECK(count(0)==1);
 p={1,2,{}};S::Receive(p,2);CHECK(S::g_request.p.sequence);S::Receive(p,0);CHECK(!S::g_request.p.sequence);
 p={1,1,{}};S::Receive(p,2);CHECK(!S::g_effect.p.sequence);S::Receive(p,0);
 test::available=false;tick();CHECK(S::g_effect.p.sequence);test::available=true;tick();CHECK(test::starts==5&&!S::g_effect.p.sequence);
 S::Receive(p,0);tick();CHECK(test::starts==5); // event duplicate never replays
 p.sequence=2;S::Receive(p,0);tick();CHECK(S::g_effect.p.sequence==2); // previous spark loop still busy
 test::active=false;tick();CHECK(test::starts==6&&!S::g_effect.p.sequence);
 c.callerObject=&test::actor;c.fromOurCode=true;CHECK(S::OnDeskBlackout(c)==V::Cancel);
 c.callerObject=&test::ui;CHECK(S::OnDeskBlackout(c)==V::Run);
 test::announced=false;c.callerObject=&test::actor;CHECK(S::OnDeskBlackout(c)==V::Run);CHECK(S::OnShortCircuit(c)==V::Run);
 test::announced=true;p.sequence=3;S::Receive(p,0);S::g_effect.since=S::Now()-9000;tick();CHECK(!S::g_effect.p.sequence);
 p.sequence=4;p.reserved[1]=1;S::Receive(p,0);CHECK(S::g_seenEvent==3);
 S::OnDisconnect();CHECK(!S::g_seenEvent&&!S::g_request.p.sequence);
 std::puts("desk circuit: authority, retries, duplicates, consecutive events, expiry and world lifetime PASS");
}
