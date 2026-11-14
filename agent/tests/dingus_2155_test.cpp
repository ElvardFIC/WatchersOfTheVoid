#include "support_2135.h"
#ifndef TOY_MUSIC_SOURCE
#define TOY_MUSIC_SOURCE "../../src/votv-coop/src/coop/props/dingus_music_sync.cpp"
#endif
#include TOY_MUSIC_SOURCE
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <limits>
#include <set>
namespace test {
bool hand=false,found=true;int applies=0,interactions=0;
bool plush=false,alt=false;
int squeaks=0;float pitch=1.25f,heardPitch=0;
ue_wrap::dingus_music::State music{true,false,.3f};
std::wstring key=L"maxwell";
uint16_t people[coop::net::kMaxPeers]{10,11,12,13};
std::function<void()> pitchCallback;
bool clockReady=false;std::function<void()> clockCallback;
std::set<uint64_t> invocations;
uint32_t sessionSerial=1;
std::function<void()> keyCallback;
}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace ue_wrap::dingus_music {
bool IsDingus(void* actor){return !test::plush && actor==&test::actor && test::actor.live;}
bool IsSqueakyToy(void* actor){return actor==&test::actor && test::actor.live;}
bool IsAltUse(void*,const uint8_t*){return test::alt;}
size_t Collect(CachedObjRef* out,size_t cap){if(!cap || !test::found)return 0;out[0].Set(&test::actor);return 1;}
bool Read(void*,State& s){s=test::music;test::Callback();return true;}
bool Apply(void*,const State& s,const std::function<bool()>& current){++test::applies;test::music=s;test::Callback();return !current || current();}
bool ReadClock(void*,Clock& clock,const std::function<bool()>& current){clock.audio.Set(&test::actor);clock.sound.Set(&test::ui);clock.duration=69.1716f;clock.pitch=test::music.pitch;
 auto callback=std::move(test::clockCallback);test::clockCallback={};if(callback)callback();return test::clockReady && (!current || current());}
bool ApplyPlayback(void* actor,const State& state,Playback& playback,const std::function<bool()>& current){const bool ok=Apply(actor,state,current);if(ok)playback.episode=state.episode;return ok;}
bool ReadSqueakPitch(void*,float& pitch){pitch=test::pitch;auto callback=std::move(test::pitchCallback);test::pitchCallback={};if(callback)callback();return true;}
bool Squeak(void*,float pitch,const std::function<bool()>& current){if(current && !current())return false;
 // SetPitch may re-enter the source before the final, once-only Activate dispatch.
 test::Callback();if(current && !current())return false;++test::squeaks;test::heardPitch=pitch;return true;}
bool Interact(void*,bool toggle,const std::function<bool()>& current){if(current && !current())return false;++test::interactions;if(!test::music.used)test::music.playing=false;
 test::music.used=true;if(toggle)test::music.playing=!test::music.playing;test::Callback();return !current || current();}
}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace coop::roster_ledger {const Row& Get(int slot){static Row rows[net::kMaxPeers];rows[slot].playerNo=test::people[slot];return rows[slot];}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){auto key=test::key;auto callback=std::move(test::keyCallback);test::keyCallback={};if(callback)callback();return key;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& k){return test::found && k==test::key?&test::actor:nullptr;}}
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
int32_t FunctionFrameSize(void*){return 1;}
}
namespace ue_wrap::script_gate {bool IsInvocationActive(uint64_t id){return test::invocations.contains(id);}}
void NativeAfterAction(ue_wrap::script_gate::Call call){
 static uint64_t sequence=100;call.invocation=++sequence;test::invocations.insert(call.invocation);
 coop::dingus_music_sync::BeforeAction(call);coop::dingus_music_sync::AfterAction(call);test::invocations.erase(call.invocation);
}
void ResetWorldToy(coop::net::Session& session,coop::net::Role role=coop::net::Role::Host){
    using namespace test;coop::dingus_music_sync::OnDisconnect();++world;session={};session.mode=role;
    actor.live=true;found=true;hand=plush=alt=clockReady=false;music={true,false,.3f};callback=pitchCallback=clockCallback=keyCallback={};
    coop::players::Registry::Get().peer=role==coop::net::Role::Host?0:1;
    coop::element::reach=true;coop::element::callback={};coop::dingus_music_sync::Install(&session);
}
#ifndef DINGUS_WORLD_FIXTURE_ONLY
int main(){using namespace test;namespace S=coop::dingus_music_sync;namespace A=coop::element;
 using Packet=coop::net::DingusMusicPayload;using V=ue_wrap::script_gate::Verdict;
 CHECK(coop::net::LaneForKind(coop::net::ReliableKind::DingusMusic)==coop::net::Lane::Normal);
 CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::DingusMusic));
 coop::net::Session session;S::Install(&session);S::Tick();CHECK(session.sent.size()==3);
 Packet initial=S::g_rows.at(key).state;
 Packet request{};coop::net::WireKeyFromString(key,request.key);request.op=1;request.sequence=1;request.playerNo=people[1];
 S::Receive(request,1);CHECK(interactions==1 && music.used && music.playing); // touch then toggle, not toggle then touch
 S::Receive(request,1);CHECK(interactions==1); // retry cannot toggle twice
 request.sequence++;session.ready=false;S::Receive(request,1);CHECK(interactions==1);session.ready=true;
 A::reach=false;S::Receive(request,1);CHECK(interactions==1);A::reach=true;
 request.sequence++;S::Receive(request,1);CHECK(interactions==2 && !music.playing);
 session.sendOk=false;S::Tick();CHECK(S::g_rows.at(key).pending);
 session.sendOk=true;S::Tick();CHECK(!S::g_rows.at(key).pending);
 S::QueueConnect(2);S::Tick();CHECK(session.sent.back().slot==2);
 request.sequence++;A::callback=[&]{S::OnDisconnect();};S::Receive(request,1);CHECK(!S::g_session && interactions==2);
 session.mode=coop::net::Role::Client;S::Install(&session);
 S::Receive(initial,2);CHECK(S::g_rows.empty());
 auto bad=initial;bad.volume=std::numeric_limits<float>::quiet_NaN();S::Receive(bad,0);CHECK(S::g_rows.empty());
 bad=initial;bad.key.len=32;S::Receive(bad,0);CHECK(S::g_rows.empty());
 S::Receive(initial,0);found=false;S::Tick();CHECK(applies==0);
 found=true;now+=250;S::Tick();CHECK(applies==1 && music.playing && !music.used);
 music.playing=false;now+=250;S::Tick();CHECK(music.playing); // native loadData after state is repaired
 auto newer=initial;newer.sequence++;newer.playing=0;S::Receive(newer,0);S::Receive(initial,0);S::Tick();CHECK(!music.playing);
 uint8_t action=5;ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&ui;call.locals=&action;call.invocation=1;invocations.insert(1);
 CHECK(S::BeforeAction(call)==V::Cancel && S::g_intents.size()==1);
 session.sendOk=false;S::Tick();CHECK(S::g_intents.size()==1);
 session.sendOk=true;S::Tick();CHECK(S::g_intents.empty());
 action=6;CHECK(S::BeforeAction(call)==V::Run);action=5;hand=true;CHECK(S::BeforeAction(call)==V::Run);
 hand=false;call.invocation=2;invocations.insert(2);S::BeforeTouch(call);CHECK(S::g_intents.size()==1 && S::g_intents.front().state.op==2);
 music.used=true;S::BeforeTouch(call);CHECK(S::g_intents.size()==1);
 now+=4000;S::Tick();CHECK(S::g_intents.empty());
 now+=250;callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session && S::g_rows.empty());
 S::Install(&session);S::Receive(initial,0);++world;S::Tick();CHECK(S::Current() && S::g_rows.empty());S::Install(&session);CHECK(S::g_rows.empty());
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);session.sent.clear();
 Packet pulse{};coop::net::WireKeyFromString(key,pulse.key);pulse.op=3;pulse.sequence=1;pulse.volume=1.75f;pulse.playerNo=people[1];
 auto nativeBefore=interactions;S::Receive(pulse,1);S::Receive(pulse,1);CHECK(S::g_pulses.size()==1);
 session.sendOk=false;S::Tick();CHECK(squeaks==1&&heardPitch==1.75f&&interactions==nativeBefore);
 CHECK(S::g_pulses.size()==1&&!S::g_pulses.front().play); // send retry cannot repeat local sound
 auto canonical=S::g_pulses.front().state;session.sendOk=true;S::Tick();CHECK(squeaks==1&&S::g_pulses.empty());
 int deliveries=0;for(const auto& s:session.sent){Packet p{};std::memcpy(&p,s.data.data(),sizeof(p));if(p.op==4){CHECK(s.slot!=1);++deliveries;}}
 CHECK(deliveries==2);
 S::QueueConnect(3);S::Tick();CHECK(squeaks==1&&S::g_pulses.empty()); // no old sound in join replay
 action=6;NativeAfterAction(call);CHECK(S::g_pulses.size()==1&&!S::g_pulses.front().play);S::Tick();CHECK(squeaks==1);
 S::OnDisconnect();session.mode=coop::net::Role::Client;S::Install(&session);
 S::Receive(canonical,1);CHECK(S::g_pulses.empty());S::Receive(canonical,0);S::Receive(canonical,0);CHECK(S::g_pulses.size()==1);
 found=false;S::Tick();CHECK(squeaks==1);found=true;S::Tick();CHECK(squeaks==2&&S::g_pulses.empty());
 canonical.sequence++;S::Receive(canonical,0);found=false;now+=1600;S::Tick();found=true;S::Tick();CHECK(squeaks==2);
 NativeAfterAction(call);CHECK(S::g_intents.size()==1&&S::g_intents.front().state.op==3&&S::g_intents.front().state.volume==pitch);
 hand=true;NativeAfterAction(call);CHECK(S::g_intents.size()==1);hand=false;
 canonical.sequence++;S::Receive(canonical,0);callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session&&S::g_pulses.empty());
 S::Install(&session);plush=true;call.tag=S::kPlushUse;
 NativeAfterAction(call);CHECK(S::g_intents.empty());alt=true;NativeAfterAction(call);CHECK(S::g_intents.size()==1);
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);
 pulse.sequence=1;pulse.op=3;S::Receive(pulse,1);CHECK(S::g_pulses.size()==1);S::Tick();CHECK(S::g_rows.empty());
 pulse.sequence++;pulse.op=1;pulse.volume=0;auto gameplay=interactions;S::Receive(pulse,1);CHECK(interactions==gameplay); // qwak toy cannot accept music toggles
 session.sent.clear();plush=false;call.tag=0;NativeAfterAction(call);NativeAfterAction(call);
 auto older=S::g_pulses.front().state.sequence;
 session.allowSend=[&](int slot,coop::net::ReliableKind,const void* data,int){Packet q{};std::memcpy(&q,data,sizeof(q));return !(slot==2&&q.op==4&&q.sequence==older);};
 S::Tick();for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));CHECK(q.op!=4);}
 session.allowSend={};S::Tick();uint32_t last=0;int pulses=0;
 for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));if(q.op==4){CHECK(q.sequence>last);last=q.sequence;++pulses;}}
 CHECK(pulses==2);
 // Retrying a four-item batch preserves its place ahead of the fifth sound.
 session.sent.clear();for(int i=0;i<5;++i)NativeAfterAction(call);session.sendOk=false;S::Tick();
 session.sendOk=true;S::Tick();S::Tick();last=0;pulses=0;
 for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));if(q.op==4){CHECK(q.sequence>last);last=q.sequence;++pulses;}}
 CHECK(pulses==5);S::OnDisconnect();
 ResetWorldToy(session);S::Tick();session.sent.clear();bool recycled=false;
 session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !recycled){recycled=true;++people[2];S::QueueConnect(2);}return true;};
 S::QueueConnect(2);S::Tick();CHECK(recycled && (S::g_rows.at(key).pending&(1u<<2)));session.allowSend={};S::Tick();CHECK(!S::g_rows.at(key).pending);
 S::QueueConnect(2);session.allowSend=[&](int,auto,const void*,int){S::OnDisconnect();return true;};S::Tick();CHECK(!S::g_session && S::g_rows.empty());
 ResetWorldToy(session,coop::net::Role::Client);action=5;call.locals=&action;CHECK(S::BeforeAction(call)==V::Cancel);
 session.allowSend=[&](int,auto,const void*,int){S::OnDisconnect();return true;};S::Tick();CHECK(!S::g_session && S::g_intents.empty());
 ResetWorldToy(session,coop::net::Role::Client);S::Receive(initial,0);auto newest=initial;newest.sequence++;newest.playing=0;
 callback=[&]{S::Receive(newest,0);};S::Tick();S::Tick();CHECK(!music.playing && S::g_rows.at(key).state.sequence==newest.sequence);
 ResetWorldToy(session);pulse.op=3;pulse.sequence=1;pulse.volume=1.75f;A::callback=[&]{++people[1];};auto prior=squeaks;
 S::Receive(pulse,1);S::Tick();CHECK(squeaks==prior && S::g_pulses.empty());
 S::QueuePulse(pulse,0);session.allowSend=[&](int,auto,const void*,int){S::OnDisconnect();return true;};S::Tick();CHECK(!S::g_session && S::g_pulses.empty());
 // Nested captures and role changes cannot publish an older world sample.
 ResetWorldToy(session);callback=[&]{music.volume=.8f;S::Sample(&actor,now);};S::Sample(&actor,now);CHECK(S::g_rows.at(key).state.volume==.8f);
 for(int cause=0;cause<4;++cause){ResetWorldToy(session);callback=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)session.mode=coop::net::Role::Client;
  if(cause==2)++world;if(cause==3)hand=true;};S::Sample(&actor,now);CHECK(S::g_rows.empty());}
 // A departure/rejoin of the same person during native sound drops that old cue.
 ResetWorldToy(session);S::Reconcile();S::QueuePulse(pulse,1);callback=[&]{S::OnPeerLeft(2);S::QueueConnect(2);};S::Tick();
 for(const auto& sent:session.sent)if(sent.slot==2){Packet p{};std::memcpy(&p,sent.data.data(),sizeof(p));CHECK(p.op!=4);}
 ResetWorldToy(session);S::Reconcile();auto before=interactions;request.op=1;request.sequence=1;
 A::callback=[]{S::OnPeerLeft(1);};S::Receive(request,1);CHECK(interactions==before);
 ResetWorldToy(session);action=6;call.tag=0;pitchCallback=[]{S::OnDisconnect();};NativeAfterAction(call);CHECK(!S::g_session && S::g_pulses.empty());
 ResetWorldToy(session,coop::net::Role::Client);pitchCallback=[&]{session.mode=coop::net::Role::Host;};NativeAfterAction(call);CHECK(S::g_intents.empty() && S::g_pulses.empty());
 puts("PASS world toy lifecycle: recipient replacement, state/intent/pulse send teardown, newer native receipt and requester replacement");
 puts("PASS world toy2157: music/squeak, abplush Alt only, no gameplay replay, per-peer retry FIFO/batch order, validation/world/teardown");
}
#endif
