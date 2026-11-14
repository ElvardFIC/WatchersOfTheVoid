#define DINGUS_WORLD_FIXTURE_ONLY
#include "dingus_2155_test.cpp"
int main(){
 using namespace test;namespace S=coop::dingus_music_sync;using coop::net::Role;using Packet=coop::net::DingusMusicPayload;
 coop::net::Session session;
 for(int i=0;i<coop::net::kMaxPeers;++i)people[i]=uint16_t(10+i);
 const auto reset=[&](Role role=Role::Host){ResetWorldToy(session,role);key=L"maxwell";S::Reconcile();};
 Packet pulse{};coop::net::WireKeyFromString(L"maxwell",pulse.key);pulse.op=3;pulse.sequence=1;pulse.volume=1.25f;pulse.playerNo=people[1];
 // 1. Same pointer, new start serial/running/link/world/role retires pending work.
 for(int cause=0;cause<5;++cause){reset();session.sendOk=false;S::QueuePulse(pulse,0);CHECK(!S::g_pulses.empty());
  if(cause==0)++sessionSerial;if(cause==1)session.active=false;if(cause==2)session.linked=false;
  if(cause==3)++world;if(cause==4)session.mode=Role::Client;
  S::Tick();CHECK(S::g_pulses.empty());
 }
 puts("PASS toy lifetime 1: session scope");
 // 2. A transport replacement with the same player number cannot finish an intent.
 reset();auto count=interactions;auto request=pulse;request.op=1;request.volume=0;
 coop::element::callback=[&]{++session.generations[1];};S::Receive(request,1);CHECK(interactions==count);
 // Reconciliation permits the replacement transport to start its own sequence.
 S::Receive(request,1);CHECK(interactions==count+1);
 puts("PASS toy lifetime 2: source connection");
 // 3. Old recipient debt cannot be delivered to the replacement connection.
 reset();S::QueuePulse(pulse,0);++session.generations[2];S::Tick();
 for(const auto& sent:session.sent)if(sent.slot==2){Packet p{};std::memcpy(&p,sent.data.data(),sizeof(p));CHECK(p.op!=4);}
 reset();S::QueuePulse(pulse,1);callback=[&]{++session.generations[2];};S::Tick();
 for(const auto& sent:session.sent)if(sent.slot==2){Packet p{};std::memcpy(&p,sent.data.data(),sizeof(p));CHECK(p.op!=4);}
 puts("PASS toy lifetime 3: recipient connection");
 // 4. A forged POST cannot consume the actual frame's PRE.
 reset();uint8_t action=6;ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&ui;call.locals=&action;
 call.invocation=1001;call.stack=&call;invocations.insert(call.invocation);S::BeforeAction(call);
 auto wrong=call;wrong.stack=&action;S::AfterAction(wrong);CHECK(S::g_captures.size()==1 && S::g_pulses.empty());
 wrong=call;wrong.function=&actor;S::AfterAction(wrong);CHECK(S::g_captures.size()==1 && S::g_pulses.empty());
 S::AfterAction(call);CHECK(S::g_captures.empty() && S::g_pulses.size()==1);invocations.erase(call.invocation);
 puts("PASS toy lifetime 4: exact POST");
 // 5. Nested PRE observes reserved capacity; replacement teardown owns the new vector.
 reset();call.invocation=1002;invocations.insert(call.invocation);
 keyCallback=[&]{CHECK(S::g_captures.size()==1 && !S::g_captures.front().ready);S::BeforeAction(call);};
 S::BeforeAction(call);CHECK(S::g_captures.size()==1);S::AfterAction(call);CHECK(S::g_pulses.size()==1);
 invocations.erase(call.invocation);
 reset();call.invocation=1003;invocations.insert(call.invocation);keyCallback=[&]{S::OnDisconnect();S::Install(&session);};
 S::BeforeAction(call);CHECK(S::g_captures.empty());invocations.erase(call.invocation);
 puts("PASS toy lifetime 5: reserved PRE");
 // 6. Retry the exact rejected sequence once a bounded queue has room.
 reset(Role::Client);auto canonical=pulse;canonical.op=4;canonical.playerNo=0;
 S::g_pulses.resize(32);S::Receive(canonical,0);CHECK(!S::g_lastPulse);S::g_pulses.clear();S::Receive(canonical,0);
 CHECK(S::g_lastPulse==1 && S::g_pulses.size()==1);
 reset();S::g_pulses.resize(32);S::Receive(pulse,1);CHECK(!S::g_seen[1]);S::g_pulses.clear();S::Receive(pulse,1);CHECK(S::g_seen[1]==1 && S::g_pulses.size()==1);
 reset();coop::element::callback=[]{S::g_pulses.resize(32);};S::Receive(pulse,1);CHECK(!S::g_seen[1]);
 S::g_pulses.clear();S::Receive(pulse,1);CHECK(S::g_seen[1]==1 && S::g_pulses.size()==1);
 reset();auto newer=pulse;newer.sequence=2;coop::element::callback=[&]{S::Receive(newer,1);};S::Receive(pulse,1);
 CHECK(S::g_seen[1]==2 && S::g_pulses.size()==1);
 puts("PASS toy lifetime 6: admission before dedup");
 // 7. Native authorization may overwrite a caller-owned decode buffer.
 reset();auto external=request;count=interactions;coop::element::callback=[&]{external.op=3;external.volume=9;external.sequence=999;};
 S::Receive(external,1);CHECK(interactions==count+1 && S::g_pulses.empty() && S::g_seen[1]==request.sequence);
 puts("PASS toy lifetime 7: packet ownership");
 // 8. Thirty-one recipients and four queued sounds exceed a single tick's budget.
 reset();for(int i=0;i<4;++i)S::QueuePulse(pulse,0);S::g_poll=now+100000;S::Tick();CHECK(session.sent.size()==64 && !S::g_pulses.empty());
 session.sent.clear();S::Tick();CHECK(session.sent.size()<=64 && S::g_pulses.empty());
 puts("PASS toy lifetime 8: send budget");
 // 9. Expiry is rechecked after native calls; clock rollback retires pending transients.
 reset();S::QueuePulse(pulse,1);count=squeaks;callback=[]{now+=1500;};S::Tick();CHECK(squeaks==count && S::g_pulses.empty());
 reset();S::QueuePulse(pulse,0);--now;S::Tick();CHECK(S::g_pulses.empty());
 reset(Role::Client);S::Receive(canonical,0);count=squeaks;now+=1500;S::Tick();CHECK(squeaks==count && S::g_pulses.empty());
 puts("PASS toy lifetime 9: expiry and rollback");
 // 10. Identical ASCII-domain restrictions in local sampling and wire receipt.
 for(const auto& invalid:{std::wstring(L"bad key"),std::wstring(L"bad\nkey"),std::wstring(L"\x100"),std::wstring(L"a\0b",3)}){
  reset();key=invalid;S::Sample(&actor,now);CHECK(S::g_rows.empty());
  reset(Role::Client);auto bad=canonical;bad.key.len=uint8_t(invalid.size());for(size_t i=0;i<invalid.size();++i)bad.key.data[i]=char(invalid[i]);
  S::Receive(bad,0);CHECK(S::g_pulses.empty());
 }
 reset();S::Sample(&actor,now);CHECK(!S::g_rows.empty());puts("PASS toy lifetime 10: key domain");
 return 0;
}
