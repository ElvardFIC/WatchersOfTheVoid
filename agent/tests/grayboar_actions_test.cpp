#include "../../src/votv-coop/src/coop/creatures/grayboar_actions.cpp"
#include <cassert>
#include <cstdio>
#include <cwchar>
#include <cstddef>
#include <limits>
namespace test {
struct Actor {uint8_t biting=0,roaring=0,dying=0,skipDeath=0;float rate=1;void* mesh=nullptr;void* attack=nullptr;void* roar=nullptr;} actor;
int mesh=1,anim=2,montage=3,audio=4,cls=5,stack=6;
void* objects[]={&actor,&mesh,&anim,&montage,&audio,&cls};int serials[]={1,1,1,1,1,1};
uint32_t world=1;bool live=true,boar=true,ready=true,refuseParam=false,refusePlay=false,recycle=false,disconnect=false;
int plays=0,jumps=0,sounds=0,section=0;float duration=2;
int hostSection=1,stops=0;bool hostPlaying=true;float hostPosition=0.4f,hostRate=1.5f,startPosition=-1;
int deathSounds=0,silences=0;
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return &test::world;}
void* WorldOf(void*){return &test::world;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<6;++i)if(test::objects[i]==p)return i;return -1;}
bool IsLive(void* p){return test::live && InternalIndexOf(p)>=0;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<6&&test::objects[i]==p&&test::live;}
int32_t SlotSerial(int32_t i){return i>=0&&i<6?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void*){return &test::cls;}
void* SuperStructOf(void*){return nullptr;}
const FName& NameOf(void*){static FName n{1,0};return n;}
bool NameEquals(const FName& name,const wchar_t* s){
 if(!wcscmp(s,L"atttack"))return name.ComparisonIndex==11;
 if(!wcscmp(s,L"roar"))return name.ComparisonIndex==12;
 if(!wcscmp(s,L"death"))return name.ComparisonIndex==13;
 return test::boar&&!wcscmp(s,L"grayboar_C");
}
bool FindBoolProperty(void*,const wchar_t* s,int32_t& off,uint8_t& mask){
 mask=1;
 if(!wcscmp(s,L"biting"))off=offsetof(test::Actor,biting);
 else if(!wcscmp(s,L"roaring"))off=offsetof(test::Actor,roaring);
 else if(!wcscmp(s,L"skipDeathAnimation"))off=offsetof(test::Actor,skipDeath);
 else {assert(!wcscmp(s,L"dying"));off=offsetof(test::Actor,dying);}
 return true;
}
int32_t FindPropertyOffset(void*,const wchar_t* s){
 if(!wcscmp(s,L"animationSpeed"))return offsetof(test::Actor,rate);
 if(!wcscmp(s,L"SkeletalMesh"))return offsetof(test::Actor,mesh);
 if(!wcscmp(s,L"StaticMesh"))return offsetof(test::Actor,mesh);
 if(!wcscmp(s,L"audio_idle"))return offsetof(test::Actor,attack);
 if(!wcscmp(s,L"audio_attack"))return offsetof(test::Actor,attack);
 assert(!wcscmp(s,L"audio_roar"));return offsetof(test::Actor,roar);
}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return test::ready?const_cast<wchar_t*>(name):nullptr;}
void* FindObject(const wchar_t* name,const wchar_t* type){
 if(!wcscmp(name,L"grayboar_idle4")){assert(!wcscmp(type,L"SoundWave"));return &test::audio;}
 if(!wcscmp(name,L"att_default")){assert(!wcscmp(type,L"SoundAttenuation"));return &test::audio;}
 assert(!wcscmp(name,L"grayboar_Skeleton_Montage")&&!wcscmp(type,L"AnimMontage"));return &test::montage;
}
void* FindClassDefaultObject(const wchar_t* name){assert(!wcscmp(name,L"GameplayStatics"));return &test::cls;}
}
namespace ue_wrap::fname_utils {
reflection::FName StringToFName(const std::wstring& name){test::section=name==L"atttack"?1:name==L"roar"?2:name==L"death"?3:0;assert(test::section);return {test::section,0};}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(64){}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t){
 if(!wcscmp(name,L"InTimeToStartMontageAt"))test::startPosition=*static_cast<const float*>(value);
 return !test::refuseParam;
}
bool ParamFrame::GetRaw(const wchar_t*,void* out,int32_t size)const {
 if(!wcscmp(static_cast<const wchar_t*>(fn_),L"GetAnimInstance")){assert(size==sizeof(void*));void* a=&test::anim;std::memcpy(out,&a,size);}
 else if(!wcscmp(static_cast<const wchar_t*>(fn_),L"Montage_IsPlaying")){assert(size==1);*static_cast<uint8_t*>(out)=test::hostPlaying;}
 else if(!wcscmp(static_cast<const wchar_t*>(fn_),L"Montage_GetCurrentSection")){
  assert(size==sizeof(reflection::FName));reflection::FName section{10+test::hostSection,0};std::memcpy(out,&section,size);
 }
 else {
  assert(size==sizeof(float));const auto* name=static_cast<const wchar_t*>(fn_);
  float value=!wcscmp(name,L"Montage_GetPosition")?test::hostPosition:!wcscmp(name,L"Montage_GetPlayRate")?test::hostRate:test::duration;
  std::memcpy(out,&value,size);
 }
 return true;
}
bool Call(void*,ParamFrame& f){
 const auto* name=static_cast<const wchar_t*>(f.function());
 if(!wcscmp(name,L"GetAnimInstance"))return true;
 if(!wcscmp(name,L"Montage_IsPlaying") || !wcscmp(name,L"Montage_GetCurrentSection") ||
    !wcscmp(name,L"Montage_GetPosition") || !wcscmp(name,L"Montage_GetPlayRate"))return true;
 if(!wcscmp(name,L"Montage_Stop")){++test::stops;return true;}
 if(!wcscmp(name,L"Stop")){++test::silences;return true;}
 if(!wcscmp(name,L"SpawnSoundAttached")){++test::deathSounds;return true;}
 if(!wcscmp(name,L"Montage_Play")){
  ++test::plays;if(test::recycle)++test::serials[0];
  if(test::disconnect)coop::grayboar_actions::OnDisconnect();return !test::refusePlay;
 }
 if(!wcscmp(name,L"Montage_JumpToSection")){++test::jumps;return true;}
 assert(!wcscmp(name,L"Play"));++test::sounds;return true;
}
}
int main(){
 namespace A=coop::grayboar_actions;using namespace test;
 coop::net::Session session;auto& registry=coop::element::Registry::Get();
 registry.row.actor=&actor;registry.row.mirror=false;registry.id=42;
 actor.mesh=&mesh;actor.attack=actor.roar=&audio;
 ue_wrap::script_gate::Call call{};call.object=&actor;call.stack=&stack;
 A::Install(&session);
 auto edge=[&](uint8_t kind){actor.biting=actor.roaring=0;A::Before(call,kind);(kind==1?actor.biting:actor.roaring)=1;A::After(call,kind);};
 edge(1);A::Tick();assert(session.sends==1&&session.sent.eid==42&&session.sent.action==1&&session.sent.rate==1);
 A::Before(call,1);A::After(call,1);A::Tick();assert(session.sends==1); // Active call is no new action.
 actor.biting=0;A::Before(call,1);A::After(call,1);assert(A::g_out.empty()); // Refused native start.
 actor.biting=0;A::Before(call,1); // Another gate cancels: no post callback.
 edge(1);assert(A::g_observations.empty()&&A::g_out.size()==1);A::g_out.clear();
 actor.dying=1;edge(2);assert(A::g_out.empty());actor.dying=0;
 registry.row.mirror=true;edge(1);assert(A::g_out.empty());registry.row.mirror=false;
 actor.rate=std::numeric_limits<float>::quiet_NaN();edge(1);assert(A::g_out.empty());actor.rate=1;
 session.sendOk=false;edge(2);A::Tick();assert(A::g_out.size()==1);
 session.sendOk=true;A::Tick();assert(A::g_out.empty()&&session.sent.action==2);
 session.readyMask=0;edge(1);assert(A::g_out.empty());session.readyMask=2;
 edge(1);A::OnPeerLeft(1);const int beforeLeft=session.sends;A::Tick();assert(session.sends==beforeLeft&&A::g_out.empty());
 edge(1);A::g_out.front().expires=0;const int sent=session.sends;A::Tick();assert(session.sends==sent);
 actor.biting=0;A::Before(call,1);++serials[0];actor.biting=1;A::After(call,1);assert(A::g_observations.empty()&&A::g_out.empty());
 puts("PASS host action edges: native refusal, active/dead/mirror boars, rate, recycled identity, send retry and expiry");
 // Join queries sample native montage truth, including an action not observed
 // by Before/After (e.g. begun before any peer was ready).
 actor.biting=1;actor.roaring=0;hostPlaying=true;hostSection=1;
 const int beforeJoin=session.sends;A::QueueConnect(1);A::QueueConnect(1);
 assert(A::g_joins.size()==1);A::Tick();
 assert(session.sends==beforeJoin+1&&session.sent.position>=hostPosition&&session.sent.position<hostPosition+0.1f&&session.sent.rate==hostRate);
 hostPlaying=false;A::QueueConnect(1);A::Tick();assert(session.sends==beforeJoin+1);hostPlaying=true;
 A::QueueConnect(1);++serials[0];A::Tick();assert(session.sends==beforeJoin+1);
 A::QueueConnect(1);A::OnPeerLeft(1);A::Tick();assert(session.sends==beforeJoin+1);
 actor.dying=1;A::QueueConnect(1);A::Tick();assert(session.sends==beforeJoin+1);actor.dying=0;
 puts("PASS join snapshots: actual active montage position/rate, stopped/dead actors, dedup, recycled candidates and departed slots");
 A::OnDisconnect();session.mode=coop::net::Role::Client;A::Install(&session);registry.row.mirror=true;
 coop::net::GrayboarActionPayload p{42,1,1,1};
 A::Receive(p,1);A::Tick();assert(!plays);
 auto invalid=p;invalid.eid=coop::element::kHostRangeSize;A::Receive(invalid,0);
 invalid=p;invalid.rate=std::numeric_limits<float>::infinity();A::Receive(invalid,0);
 invalid=p;invalid.action=4;A::Receive(invalid,0);assert(A::g_in.empty());
 ready=false;A::Receive(p,0);A::Tick();assert(!plays&&A::g_in.size()==1);
 ready=true;A::Tick();assert(plays==1&&jumps==1&&sounds==1&&section==1);
 A::Receive(p,0);A::Tick();assert(plays==1); // Duplicate sequence.
 ++p.sequence;p.action=2;registry.present=false;A::Receive(p,0);A::Tick();assert(plays==1);
 registry.present=true;A::Tick();assert(plays==2&&sounds==2&&section==2);
 ++p.sequence;registry.row.mirror=false;A::Receive(p,0);A::Tick();assert(plays==2);
 registry.row.mirror=true;A::Tick();assert(plays==3);
 ++p.sequence;A::Receive(p,0);A::g_in.front().expires=0;A::Tick();assert(plays==3);
 ++p.sequence;A::Receive(p,0);refuseParam=true;A::Tick();assert(plays==3);refuseParam=false;
 duration=0;A::Tick();assert(plays==4&&jumps==3);duration=2;
 A::Tick();assert(plays==5&&jumps==4&&sounds==4);
 // Full queues stay bounded, and an expired head cannot block later work forever.
 auto later=p;later.sequence+=1;A::Receive(later,0);
 for(int i=0;i<40;++i){++later.sequence;A::Receive(later,0);}
 assert(A::g_in.size()==32);
 for(auto& cue:A::g_in)cue.expires=0;
 while(!A::g_in.empty())A::Tick();
 p.sequence=later.sequence+1;
 ++p.sequence;A::Receive(p,0);recycle=true;A::Tick();assert(plays==6&&jumps==4);recycle=false;
 A::g_in.front().expires=0;A::Tick();
 ++p.sequence;A::Receive(p,0);disconnect=true;A::Tick();assert(plays==7&&jumps==4&&A::g_in.empty());disconnect=false;
 A::Install(&session);++world;A::Receive(p,0);A::Tick();assert(A::g_in.empty());
 A::Install(&session);p.sequence=1;A::Receive(p,0);A::Tick();assert(plays==8&&jumps==5);
 puts("PASS cosmetic receiver: host-only, validated payload, mirror/AnimInstance retry, dedup, direct montage/audio, refusal, recycle and world/session teardown");
 // Snapshot resumes at the sampled position plus local queue wait, without
 // jumping back to section start or replaying a one-shot sound.
 p={42,2,2.f,1,0.4f};hostSection=1;const int beforeResume=plays,beforeJumps=jumps,beforeSounds=sounds;
 A::Receive(p,0);A::g_in.front().sampled-=200;A::Tick();
 assert(plays==beforeResume+1&&jumps==beforeJumps&&sounds==beforeSounds&&startPosition>=0.8f&&startPosition<0.9f);
 ++p.sequence;hostSection=2;A::Receive(p,0);A::Tick();assert(stops==1&&A::g_in.empty());
 for(float invalidPosition:{-2.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),3601.f}){
  ++p.sequence;p.position=invalidPosition;A::Receive(p,0);assert(A::g_in.empty());
 }
 puts("PASS snapshot playback: position and queue-age seek, no sound restart, expired section stop and malformed positions");
 A::OnDisconnect();session.mode=coop::net::Role::Host;A::Install(&session);registry.row.mirror=false;
 auto death=[&]{actor.dying=0;A::Before(call,3);actor.dying=1;A::After(call,3);};
 actor.rate=1;death();A::Tick();assert(session.sent.action==3&&session.sent.rate==0.5f);
 A::Before(call,3);A::After(call,3);assert(A::g_out.empty()); // No duplicate death transition.
 actor.skipDeath=1;death();assert(A::g_out.empty());actor.skipDeath=0;
 actor.rate=.05f;death();A::Tick();assert(session.sent.rate==.025f);actor.rate=1;
 hostSection=3;hostRate=.5f;A::QueueConnect(1);A::Tick();assert(session.sent.action==3&&session.sent.position>=0);
 A::OnDisconnect();session.mode=coop::net::Role::Client;A::Install(&session);registry.row.mirror=true;
 p={42,1,.5f,3};A::Receive(p,0);A::Tick();assert(section==3&&deathSounds==1&&silences==3);
 ++p.sequence;p.position=.4f;A::Receive(p,0);A::Tick();assert(deathSounds==1&&silences==6);
 puts("PASS death cosmetics: half native rate, no repeated/skip-animation cue, mid-death join, direct audio and loop silence without gameplay delegates");
}
