#include "support_2135.h"
#ifndef HAND_TOY_SOURCE
#define HAND_TOY_SOURCE "../../src/votv-coop/src/coop/player/hand_toy_audio.cpp"
#endif
#include HAND_TOY_SOURCE
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <limits>
#include <set>
namespace test {
void* local=&actor;void* mirrors[coop::net::kMaxPeers]{};
uint32_t generations[coop::net::kMaxPeers]{7,7,7,7};
uint32_t mirrorGenerations[coop::net::kMaxPeers]{};
ue_wrap::dingus_music::State music{true,true,.05f},applied;
int applies=0,squeaks=0;float heard=0,pitch=1.2f;bool applyOk=true;
bool plush=false,alt=false;
uint16_t people[coop::net::kMaxPeers]{10,11,12,13};int stops=0;
uint32_t sessionSerial=1;std::set<uint64_t> invocations;
std::function<void()> pitchCallback,altCallback;
void* usePlayer=&actor;void* dispatchFunction=&ui;
int parameterOffset=0,parameterSize=-1,frameSize=16;
bool missingInput=false;
uint64_t inputFlags=0x80;
int inputSchemaFault=0;
std::wstring inputName;
}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace ue_wrap::script_gate {bool IsInvocationActive(uint64_t id){return test::invocations.contains(id);}}
namespace ue_wrap::dingus_music {
bool IsDingus(void* a){return !test::plush && a && reflection::IsLive(a);}
bool IsSqueakyToy(void* a){return a && reflection::IsLive(a);}
bool IsAltUse(void*,const uint8_t*){const auto alt=test::alt;auto callback=std::move(test::altCallback);test::altCallback={};if(callback)callback();return alt;}
bool Read(void*,State& s){s=test::music;test::Callback();return true;}
bool Apply(void*,const State& s,const std::function<bool()>& current){test::applied=s;++test::applies;test::Callback();return test::applyOk && (!current || current());}
bool StopMusic(void*,const std::function<bool()>& current){if(current && !current())return false;++test::stops;test::applied.playing=false;test::Callback();return true;}
bool ReadSqueakPitch(void*,float& p){p=test::pitch;auto callback=std::move(test::pitchCallback);test::pitchCallback={};if(callback)callback();return true;}
bool Squeak(void*,float p,const std::function<bool()>& current){if(current && !current())return false;test::Callback();if(current && !current())return false;test::heard=p;++test::squeaks;return true;}
}
namespace coop::hand_item {
void* LocalHandActor(){return test::local;}
uint32_t Generation(uint8_t slot){return test::generations[slot];}
uint32_t MirrorGeneration(uint8_t slot){return test::mirrorGenerations[slot]==test::generations[slot]?test::mirrorGenerations[slot]:0;}
void* MirrorActorForSlot(uint8_t slot){return test::mirrors[slot];}
bool IsMirrorActor(void* a){for(auto* mirror:test::mirrors)if(mirror==a && a)return true;return false;}
}
namespace coop::roster_ledger {const Row& Get(int slot){static Row rows[net::kMaxPeers];rows[slot].playerNo=test::people[slot];return rows[slot];}}
namespace ue_wrap::reflection {
void* FindDispatchFunctionCached(void*,const wchar_t*){return test::dispatchFunction;}
int32_t FindParamOffset(void*,const wchar_t* name){test::inputName=name;return test::missingInput?-1:test::parameterOffset;}
int32_t FindParamSize(void*,const wchar_t* name){return test::parameterSize>=0?test::parameterSize:!wcscmp(name,L"player")?sizeof(void*):1;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
std::vector<ParamInfo> FunctionParams(void*){
    using namespace test;std::vector<ParamInfo> params{{inputName,parameterOffset,FindParamSize(nullptr,inputName.c_str()),inputFlags}};
    if(inputSchemaFault==1)params.push_back(params.front());
    if(inputSchemaFault==2)params.push_back({L"Overlap",parameterOffset,1,0x80});
    if(inputSchemaFault==3)params.front().offset++;
    if(inputSchemaFault==4)params.front().size++;
    return params;
}
}
void ResetToy(coop::net::Session& session){
 namespace S=coop::hand_toy_audio;using namespace test;S::OnDisconnect();++world;session={};
 coop::players::Registry::Get().peer=0;coop::players::Registry::Get().local=&actor;local=nullptr;plush=alt=false;applyOk=true;music={true,true,.05f};callback={};
 pitchCallback=altCallback={};
 dispatchFunction=&ui;parameterOffset=0;parameterSize=-1;frameSize=16;missingInput=false;usePlayer=&actor;
 inputFlags=0x80;inputSchemaFault=0;inputName.clear();
 for(int slot=0;slot<coop::net::kMaxPeers;++slot){generations[slot]=mirrorGenerations[slot]=7;mirrors[slot]=nullptr;}
 mirrors[1]=&ui;S::Install(&session);
}
void NativeCapture(ue_wrap::script_gate::Call call){
 static uint64_t sequence=100;call.invocation=++sequence;if(!call.function)call.function=&test::ui;
 test::invocations.insert(call.invocation);coop::hand_toy_audio::BeforeCapture(call);coop::hand_toy_audio::Capture(call);test::invocations.erase(call.invocation);
}
#ifndef HAND_TOY_FIXTURE_ONLY
int main(){using namespace test;namespace S=coop::hand_toy_audio;namespace A=coop::element;using Packet=coop::net::HandToyAudioPayload;
 coop::net::Session session;auto& registry=coop::players::Registry::Get();registry.peer=0;registry.local=&actor;
 CHECK(coop::net::LaneForKind(coop::net::ReliableKind::HandToyAudio)==coop::net::Lane::Normal);
 CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::HandToyAudio));
 S::Install(&session);S::Tick();CHECK(session.sent.size()==3&&applies==0);
 Packet first=S::g_rows[0].state;session.sent.clear();now+=250;S::Tick();CHECK(session.sent.empty());
 music.playing=false;session.sendOk=false;now+=250;S::Tick();CHECK(S::g_rows[0].pending);
 session.sendOk=true;S::Tick();CHECK(!S::g_rows[0].pending&&session.sent.size()==3);
 Packet p{7,10,.15f,1,0,1,1,people[1]};S::Receive(p,2);CHECK(!S::g_rows[1].have);
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
 NativeCapture(c);CHECK(S::g_pulses.empty());S::Tick();CHECK(S::g_pulses.empty());
 generations[1]=9;session.sent.clear();NativeCapture(c);S::Tick();CHECK(S::g_pulses.empty()&&session.sent.size()==1);
 Packet sent{};std::memcpy(&sent,session.sent[0].data.data(),sizeof(sent));CHECK(sent.generation==9&&sent.slot==1&&sent.op==1&&sent.value==pitch);
 NativeCapture(c);generations[1]=10;S::Tick();CHECK(S::g_pulses.empty()); // switch while queued
 c.fromOurCode=true;NativeCapture(c);CHECK(S::g_pulses.empty());c.fromOurCode=false;
 uint8_t action=5;c.tag=S::kAction;c.function=&ui;c.locals=&action;NativeCapture(c);CHECK(S::g_pulses.empty());
 action=6;NativeCapture(c);CHECK(S::g_pulses.size()==1);local=nullptr;S::Tick();CHECK(S::g_pulses.empty());
 mirrors[0]=&ui;mirrorGenerations[0]=7;generations[0]=7;
 S::Receive(first,2);CHECK(!S::g_rows[0].have);S::Receive(first,0);S::Tick();CHECK(S::g_rows[0].authorized&&applied.playing);
 now+=250;auto before=applies;S::Tick();CHECK(applies==before+1); // repair later native loadData
 first.sequence++;first.value=-1;S::Receive(first,0);CHECK(S::g_rows[0].state.sequence!=first.sequence);
 p={7,20,1.f,0,1,0,0,people[0]};S::Receive(p,0);callback=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session&&S::g_pulses.empty());
 S::Install(&session);p.sequence++;S::Receive(p,0);++world;S::Install(&session);CHECK(S::g_pulses.empty()&&!S::g_rows[0].have);
 S::OnPeerLeft(1);S::OnDisconnect();
 S::Install(&session);local=&actor;c.tag=S::kPlushUse;c.locals=reinterpret_cast<uint8_t*>(&usePlayer);plush=true;NativeCapture(c);CHECK(S::g_pulses.empty());
 alt=true;NativeCapture(c);CHECK(S::g_pulses.size()==1);session.sent.clear();S::Tick();CHECK(!S::g_rows[1].have&&session.sent.size()==1);
 S::OnDisconnect();session.mode=coop::net::Role::Host;registry.peer=0;plush=false;local=&actor;S::Install(&session);
 c.tag=S::kRmb;NativeCapture(c);NativeCapture(c);auto older=S::g_pulses.front().state.sequence;session.sent.clear();
 session.allowSend=[&](int slot,coop::net::ReliableKind,const void* data,int){Packet q{};std::memcpy(&q,data,sizeof(q));return !(slot==2&&q.op==1&&q.sequence==older);};
 S::Tick();for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));CHECK(q.op!=1);}
 session.allowSend={};S::Tick();uint32_t last=0;int pulses=0;
 for(const auto& s:session.sent)if(s.slot==2){Packet q{};std::memcpy(&q,s.data.data(),sizeof(q));if(q.op==1){CHECK(q.sequence>last);last=q.sequence;++pulses;}}
 CHECK(pulses==2);S::OnDisconnect();
 ResetToy(session);p={7,1,.15f,1,0,1,1,people[1]};S::Receive(p,1);S::Tick();CHECK(applied.playing && S::g_rows[1].painted.Get());
 auto stopped=stops;mirrors[1]=nullptr;now+=250;S::Tick();CHECK(stops==stopped+1 && !applied.playing);
 mirrors[1]=&ui;now+=250;S::Tick();CHECK(applied.playing);++people[1];now+=250;S::Tick();
 CHECK(!applied.playing && !S::g_rows[1].have);S::Receive(p,1);CHECK(!S::g_rows[1].have);
 p.playerNo=people[1];p.sequence=1;S::Receive(p,1);S::Tick();CHECK(S::g_rows[1].authorized && applied.playing);
 now+=5001;S::Tick();CHECK(!applied.playing && !S::g_rows[1].have);S::QueueConnect(2);CHECK(!S::g_rows[1].pending);
 ResetToy(session);p.playerNo=people[1];auto latest=p;latest.sequence++;latest.playing=0;
 callback=[&]{S::Receive(latest,1);};S::Receive(p,1);S::Tick();CHECK(!S::g_rows[1].authorized);
 now+=250;S::Tick();CHECK(!applied.playing && S::g_rows[1].authorized);
 ResetToy(session);bool recycled=false;session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !recycled){recycled=true;++people[2];S::QueueConnect(2);}return true;};
 S::Receive(p,1);S::Tick();CHECK(recycled && (S::g_rows[1].pending&(1u<<2)));session.allowSend={};S::Tick();CHECK(!(S::g_rows[1].pending&(1u<<2)));
 ResetToy(session);S::Receive(p,1);S::Tick();coop::net::Session replacement,requested;
 callback=[&]{S::Install(&replacement);S::Receive(p,1);S::Tick();};S::Install(&requested);CHECK(S::g_session==&replacement && S::g_rows[1].authorized);
 S::OnDisconnect();
 ResetToy(session);p={7,1,1.2f,1,1,0,0,people[1]};S::Receive(p,1);p.sequence++;S::Receive(p,1);auto prior=squeaks;
 callback=[&]{S::OnPeerLeft(1);};S::Tick();CHECK(S::g_pulses.empty() && squeaks==prior);
 ResetToy(session);p={7,1,.15f,1,0,1,1,people[1]};session.allowSend=[&](int,auto,const void*,int){S::OnDisconnect();return true;};
 S::Receive(p,1);S::Tick();CHECK(!S::g_session && !S::g_rows[1].have);
 worldUnavailable=true;ResetToy(session);S::Receive(p,1);S::Tick();CHECK(S::g_rows[1].painted.Get());
 stopped=stops;++world;S::Install(&session);CHECK(stops==stopped && !S::g_rows[1].have);worldUnavailable=false;
 puts("PASS hand toy identity: player reuse, old packet refusal, vanished-mirror/stale-loop cleanup, newer native receipt, recipient replacement and reentrant session installation");
 puts("PASS hand toy: music/pitch, owner exclusion, exact generation/late mirror, role/reach/bounds/order, retry, expiry, stow, reentry/world");
}
#endif
