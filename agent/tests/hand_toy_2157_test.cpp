#include "support_2135.h"
#include "../../src/votv-coop/src/coop/player/hand_toy_audio.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <limits>
namespace test {
void* local=&actor;void* mirrors[coop::net::kMaxPeers]{};
uint32_t generations[coop::net::kMaxPeers]{7,7,7,7};
uint32_t mirrorGenerations[coop::net::kMaxPeers]{};
ue_wrap::dingus_music::State music{true,true,.05f},applied;
int applies=0,squeaks=0;float heard=0,pitch=1.2f;bool applyOk=true;
bool plush=false,alt=false;
}
namespace ue_wrap::dingus_music {
bool IsDingus(void* a){return !test::plush && a && reflection::IsLive(a);}
bool IsSqueakyToy(void* a){return a && reflection::IsLive(a);}
bool IsAltUse(void*,const uint8_t*){return test::alt;}
bool Read(void*,State& s){s=test::music;test::Callback();return true;}
bool Apply(void*,const State& s){test::applied=s;++test::applies;test::Callback();return test::applyOk;}
bool ReadSqueakPitch(void*,float& p){p=test::pitch;return true;}
bool Squeak(void*,float p){test::heard=p;++test::squeaks;test::Callback();return true;}
}
namespace coop::hand_item {
void* LocalHandActor(){return test::local;}
uint32_t Generation(uint8_t slot){return test::generations[slot];}
uint32_t MirrorGeneration(uint8_t slot){return test::mirrorGenerations[slot]==test::generations[slot]?test::mirrorGenerations[slot]:0;}
void* MirrorActorForSlot(uint8_t slot){return test::mirrors[slot];}
}
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
int32_t FunctionFrameSize(void*){return 1;}
}
int main(){using namespace test;namespace S=coop::hand_toy_audio;namespace A=coop::element;using Packet=coop::net::HandToyAudioPayload;
 coop::net::Session session;auto& registry=coop::players::Registry::Get();registry.peer=0;
 CHECK(coop::net::LaneForKind(coop::net::ReliableKind::HandToyAudio)==coop::net::Lane::Normal);
 CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::HandToyAudio));
 S::Install(&session);S::Tick();CHECK(session.sent.size()==3&&applies==0);
 Packet first=S::g_rows[0].state;session.sent.clear();now+=250;S::Tick();CHECK(session.sent.empty());
 music.playing=false;session.sendOk=false;now+=250;S::Tick();CHECK(S::g_rows[0].pending);
 session.sendOk=true;S::Tick();CHECK(!S::g_rows[0].pending&&session.sent.size()==3);
 Packet p{7,10,.15f,1,0,1,1};S::Receive(p,2);CHECK(!S::g_rows[1].have);
 session.ready=false;S::Receive(p,1);CHECK(!S::g_rows[1].have);session.ready=true;
 S::Receive(p,1);S::Tick();CHECK(!S::g_rows[1].authorized); // HandItem/mirror arrives later
 mirrors[1]=&ui;mirrorGenerations[1]=7;A::reach=false;now+=250;S::Tick();CHECK(!S::g_rows[1].authorized);
 A::reach=true;now+=250;S::Tick();CHECK(S::g_rows[1].authorized&&applied.playing&&applied.volume==.15f);
 for(const auto& s:session.sent)if(s.data.size()==sizeof(Packet)){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));if(q.slot==1)CHECK(s.slot!=1);}
 S::QueueConnect(2);CHECK(S::g_rows[1].pending&4);S::Tick();CHECK(!S::g_rows[1].pending);
 auto bad=p;bad.sequence++;bad.value=std::numeric_limits<float>::infinity();S::Receive(bad,1);CHECK(S::g_rows[1].state.sequence==10);
 p.sequence=9;S::Receive(p,1);CHECK(S::g_rows[1].state.sequence==10);
 p.sequence=11;p.op=1;p.playing=p.used=0;p.value=1.75f;
 S::Receive(p,1);S::Receive(p,1);session.sendOk=false;S::Tick();CHECK(squeaks==1&&heard==1.75f&&S::g_pulses.size()==1);
 session.sendOk=true;S::Tick();CHECK(squeaks==1&&S::g_pulses.empty());
 p.sequence++;p.generation=6;S::Receive(p,1);now+=1600;S::Tick();CHECK(squeaks==1&&S::g_pulses.empty());
 // A mirror replaced after an old packet is accepted must never inherit that sound.
 p.sequence++;p.generation=7;S::Receive(p,1);generations[1]=8;mirrorGenerations[1]=8;
 now+=1600;S::Tick();CHECK(squeaks==1&&S::g_pulses.empty());
 // Native callback can disconnect during authorization or audio playback.
 p.sequence++;p.generation=8;S::Receive(p,1);A::callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session&&S::g_pulses.empty());
 session.mode=coop::net::Role::Client;registry.peer=1;generations[1]=0;S::Install(&session);
 ue_wrap::script_gate::Call c{};c.object=&actor;c.tag=S::kRmb;
 S::Capture(c);CHECK(S::g_pulses.size()==1&&S::g_pulses.front().state.generation==0);S::Tick();CHECK(S::g_pulses.size()==1);
 generations[1]=9;session.sent.clear();S::Tick();CHECK(S::g_pulses.empty()&&session.sent.size()==1);
 Packet sent{};std::memcpy(&sent,session.sent[0].data.data(),sizeof(sent));CHECK(sent.generation==9&&sent.slot==1&&sent.op==1&&sent.value==pitch);
 S::Capture(c);generations[1]=10;S::Tick();CHECK(S::g_pulses.empty()); // switch while queued
 c.fromOurCode=true;S::Capture(c);CHECK(S::g_pulses.empty());c.fromOurCode=false;
 uint8_t action=5;c.tag=S::kAction;c.function=&ui;c.locals=&action;S::Capture(c);CHECK(S::g_pulses.empty());
 action=6;S::Capture(c);CHECK(S::g_pulses.size()==1);local=nullptr;S::Tick();CHECK(S::g_pulses.empty());
 mirrors[0]=&ui;mirrorGenerations[0]=7;generations[0]=7;
 S::Receive(first,2);CHECK(!S::g_rows[0].have);S::Receive(first,0);S::Tick();CHECK(S::g_rows[0].authorized&&applied.playing);
 now+=250;auto before=applies;S::Tick();CHECK(applies==before+1); // repair later native loadData
 first.sequence++;first.value=-1;S::Receive(first,0);CHECK(S::g_rows[0].state.sequence!=first.sequence);
 p={7,20,1.f,0,1,0,0};S::Receive(p,0);callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session&&S::g_pulses.empty());
 S::Install(&session);p.sequence++;S::Receive(p,0);++world;S::Install(&session);CHECK(S::g_pulses.empty()&&!S::g_rows[0].have);
 S::OnPeerLeft(1);S::OnDisconnect();
 S::Install(&session);local=&actor;c.tag=S::kPlushUse;plush=true;S::Capture(c);CHECK(S::g_pulses.empty());
 alt=true;S::Capture(c);CHECK(S::g_pulses.size()==1);session.sent.clear();S::Tick();CHECK(!S::g_rows[1].have&&session.sent.size()==1);
 S::OnDisconnect();session.mode=coop::net::Role::Host;registry.peer=0;plush=false;local=&actor;S::Install(&session);
 c.tag=S::kRmb;S::Capture(c);S::Capture(c);auto older=S::g_pulses.front().state.sequence;session.sent.clear();
 session.allowSend=[&](int slot,coop::net::ReliableKind,const void* data,int){Packet q{};std::memcpy(&q,data,sizeof(q));return !(slot==2&&q.op==1&&q.sequence==older);};
 S::Tick();for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));CHECK(q.op!=1);}
 session.allowSend={};S::Tick();uint32_t last=0;int pulses=0;
 for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));if(q.op==1){CHECK(q.sequence>last);last=q.sequence;++pulses;}}
 CHECK(pulses==2);S::OnDisconnect();
 puts("PASS hand toy: music/pitch, owner exclusion, exact generation/late mirror, role/reach/bounds/order, retry, expiry, stow, reentry/world");
}
