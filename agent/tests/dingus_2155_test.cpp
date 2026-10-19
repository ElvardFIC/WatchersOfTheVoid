#include "support_2135.h"
#include "../../src/votv-coop/src/coop/props/dingus_music_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <limits>
namespace test {
bool hand=false,found=true;int applies=0,interactions=0;
bool plush=false,alt=false;
int squeaks=0;float pitch=1.25f,heardPitch=0;
ue_wrap::dingus_music::State music{true,false,.3f};
std::wstring key=L"maxwell";
}
namespace ue_wrap::dingus_music {
bool IsDingus(void* actor){return !test::plush && actor==&test::actor && test::actor.live;}
bool IsSqueakyToy(void* actor){return actor==&test::actor && test::actor.live;}
bool IsAltUse(void*,const uint8_t*){return test::alt;}
size_t Collect(CachedObjRef* out,size_t cap){if(!cap || !test::found)return 0;out[0].Set(&test::actor);return 1;}
bool Read(void*,State& s){s=test::music;test::Callback();return true;}
bool Apply(void*,const State& s){++test::applies;test::music=s;test::Callback();return true;}
bool ReadSqueakPitch(void*,float& pitch){pitch=test::pitch;return true;}
bool Squeak(void*,float pitch){++test::squeaks;test::heardPitch=pitch;test::Callback();return true;}
bool Interact(void*,bool toggle){++test::interactions;if(!test::music.used)test::music.playing=false;
 test::music.used=true;if(toggle)test::music.playing=!test::music.playing;test::Callback();return true;}
}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::key;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& k){return test::found && k==test::key?&test::actor:nullptr;}}
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
int32_t FunctionFrameSize(void*){return 1;}
}
int main(){using namespace test;namespace S=coop::dingus_music_sync;namespace A=coop::element;
 using Packet=coop::net::DingusMusicPayload;using V=ue_wrap::script_gate::Verdict;
 CHECK(coop::net::LaneForKind(coop::net::ReliableKind::DingusMusic)==coop::net::Lane::Normal);
 CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::DingusMusic));
 coop::net::Session session;S::Install(&session);S::Tick();CHECK(session.sent.size()==3);
 Packet initial=S::g_rows.at(key).state;
 Packet request{};coop::net::WireKeyFromString(key,request.key);request.op=1;request.sequence=1;
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
 uint8_t action=5;ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&ui;call.locals=&action;
 CHECK(S::BeforeAction(call)==V::Cancel && S::g_intents.size()==1);
 session.sendOk=false;S::Tick();CHECK(S::g_intents.size()==1);
 session.sendOk=true;S::Tick();CHECK(S::g_intents.empty());
 action=6;CHECK(S::BeforeAction(call)==V::Run);action=5;hand=true;CHECK(S::BeforeAction(call)==V::Run);
 hand=false;S::BeforeTouch(call);CHECK(S::g_intents.size()==1 && S::g_intents.front().state.op==2);
 music.used=true;S::BeforeTouch(call);CHECK(S::g_intents.size()==1);
 now+=4000;S::Tick();CHECK(S::g_intents.empty());
 now+=250;callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session && S::g_rows.empty());
 S::Install(&session);S::Receive(initial,0);++world;S::Tick();CHECK(!S::Current());S::Install(&session);CHECK(S::g_rows.empty());
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);session.sent.clear();
 Packet pulse{};coop::net::WireKeyFromString(key,pulse.key);pulse.op=3;pulse.sequence=1;pulse.volume=1.75f;
 auto nativeBefore=interactions;S::Receive(pulse,1);S::Receive(pulse,1);CHECK(S::g_pulses.size()==1);
 session.sendOk=false;S::Tick();CHECK(squeaks==1&&heardPitch==1.75f&&interactions==nativeBefore);
 CHECK(S::g_pulses.size()==1&&!S::g_pulses.front().play); // send retry cannot repeat local sound
 auto canonical=S::g_pulses.front().state;session.sendOk=true;S::Tick();CHECK(squeaks==1&&S::g_pulses.empty());
 int deliveries=0;for(const auto& s:session.sent){Packet p{};std::memcpy(&p,s.data.data(),sizeof(p));if(p.op==4){CHECK(s.slot!=1);++deliveries;}}
 CHECK(deliveries==2);
 S::QueueConnect(3);S::Tick();CHECK(squeaks==1&&S::g_pulses.empty()); // no old sound in join replay
 action=6;S::AfterAction(call);CHECK(S::g_pulses.size()==1&&!S::g_pulses.front().play);S::Tick();CHECK(squeaks==1);
 S::OnDisconnect();session.mode=coop::net::Role::Client;S::Install(&session);
 S::Receive(canonical,1);CHECK(S::g_pulses.empty());S::Receive(canonical,0);S::Receive(canonical,0);CHECK(S::g_pulses.size()==1);
 found=false;S::Tick();CHECK(squeaks==1);found=true;S::Tick();CHECK(squeaks==2&&S::g_pulses.empty());
 canonical.sequence++;S::Receive(canonical,0);found=false;now+=1600;S::Tick();found=true;S::Tick();CHECK(squeaks==2);
 S::AfterAction(call);CHECK(S::g_intents.size()==1&&S::g_intents.front().state.op==3&&S::g_intents.front().state.volume==pitch);
 hand=true;S::AfterAction(call);CHECK(S::g_intents.size()==1);hand=false;
 canonical.sequence++;S::Receive(canonical,0);callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session&&S::g_pulses.empty());
 S::Install(&session);plush=true;call.tag=S::kPlushUse;
 S::AfterAction(call);CHECK(S::g_intents.empty());alt=true;S::AfterAction(call);CHECK(S::g_intents.size()==1);
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::Install(&session);
 pulse.sequence=1;pulse.op=3;S::Receive(pulse,1);CHECK(S::g_pulses.size()==1);S::Tick();CHECK(S::g_rows.empty());
 pulse.sequence++;pulse.op=1;pulse.volume=0;auto gameplay=interactions;S::Receive(pulse,1);CHECK(interactions==gameplay); // qwak toy cannot accept music toggles
 session.sent.clear();plush=false;call.tag=0;S::AfterAction(call);S::AfterAction(call);
 auto older=S::g_pulses.front().state.sequence;
 session.allowSend=[&](int slot,coop::net::ReliableKind,const void* data,int){Packet q{};std::memcpy(&q,data,sizeof(q));return !(slot==2&&q.op==4&&q.sequence==older);};
 S::Tick();for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));CHECK(q.op!=4);}
 session.allowSend={};S::Tick();uint32_t last=0;int pulses=0;
 for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));if(q.op==4){CHECK(q.sequence>last);last=q.sequence;++pulses;}}
 CHECK(pulses==2);
 // Retrying a four-item batch preserves its place ahead of the fifth sound.
 session.sent.clear();for(int i=0;i<5;++i)S::AfterAction(call);session.sendOk=false;S::Tick();
 session.sendOk=true;S::Tick();S::Tick();last=0;pulses=0;
 for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));if(q.op==4){CHECK(q.sequence>last);last=q.sequence;++pulses;}}
 CHECK(pulses==5);S::OnDisconnect();
 puts("PASS world toy2157: music/squeak, abplush Alt only, no gameplay replay, per-peer retry FIFO/batch order, validation/world/teardown");
}
