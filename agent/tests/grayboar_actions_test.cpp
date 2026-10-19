#include "../../src/votv-coop/src/coop/creatures/grayboar_actions.cpp"
#include <cassert>
#include <cstdio>
#include <cwchar>
#include <cstddef>
#include <limits>
namespace test {
struct Actor {uint8_t biting=0,roaring=0,dying=0,skipDeath=0;float rate=1,health=100;void* mesh=nullptr;void* attack=nullptr;void* roar=nullptr;} actor;
Actor others[8];
struct DamageArgs {float sub=10;uint8_t skip=0;} damageArgs;
int skipOffset=offsetof(DamageArgs,skip),damageFrameSize=sizeof(DamageArgs),damageSets=0;
bool disconnectOnSet=false,recycleOnSet=false;
int mesh=1,anim=2,montage=3,audio=4,cls=5,stack=6;
void* objects[]={&actor,&mesh,&anim,&montage,&audio,&cls,&others[0],&others[1],&others[2],&others[3],&others[4],&others[5],&others[6],&others[7]};
int serials[]={1,1,1,1,1,1,1,1,1,1,1,1,1,1};
uint32_t world=1;bool live=true,boar=true,ready=true,refuseParam=false,refusePlay=false,recycle=false,disconnect=false;
int plays=0,jumps=0,sounds=0,section=0;float duration=2;
int hostSection=1,stops=0;bool hostPlaying=true;float hostPosition=0.4f,hostRate=1.f,startPosition=-1;
const wchar_t* failQuery=nullptr;
int queryTeardown=0;
int deathSounds=0,silences=0;
std::vector<int> playedSections;
void QueueRegressions();
void PlaybackRegressions();
const wchar_t* missingFunction=nullptr;
const wchar_t* failedCall=nullptr;
const wchar_t* callbackFunction=nullptr;
int callbackMode=0;
bool jumpNoEffect=false,returnReadFails=false;
void* returnedAnim=&anim;
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return &test::world;}
void* WorldOf(void*){return &test::world;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<int(std::size(test::objects));++i)if(test::objects[i]==p)return i;return -1;}
bool IsLive(void* p){return test::live && InternalIndexOf(p)>=0;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<int(std::size(test::objects))&&test::objects[i]==p&&test::live;}
int32_t SlotSerial(int32_t i){return i>=0&&i<int(std::size(test::objects))?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void*){return &test::cls;}
void* SuperStructOf(void*){return nullptr;}
const FName& NameOf(void*){static FName n{1,0};return n;}
bool NameEquals(const FName& name,const wchar_t* s){
 if(!wcscmp(s,L"atttack"))return name.ComparisonIndex==11;
 if(!wcscmp(s,L"roar"))return name.ComparisonIndex==12;
 if(!wcscmp(s,L"death"))return name.ComparisonIndex==13;
 for(int i=4;i<=7;++i)if(!wcscmp(s,coop::grayboar_actions::Section(uint8_t(i))))return name.ComparisonIndex==10+i;
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
 if(!wcscmp(s,L"health"))return offsetof(test::Actor,health);
 if(!wcscmp(s,L"animationSpeed"))return offsetof(test::Actor,rate);
 if(!wcscmp(s,L"SkeletalMesh"))return offsetof(test::Actor,mesh);
 if(!wcscmp(s,L"StaticMesh"))return offsetof(test::Actor,mesh);
 if(!wcscmp(s,L"audio_idle"))return offsetof(test::Actor,attack);
 if(!wcscmp(s,L"audio_attack"))return offsetof(test::Actor,attack);
 assert(!wcscmp(s,L"audio_roar"));return offsetof(test::Actor,roar);
}
int32_t FindParamOffset(void*,const wchar_t* name){assert(!wcscmp(name,L"skipAnim"));return test::skipOffset;}
int32_t FunctionFrameSize(void*){return test::damageFrameSize;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){
 return test::ready && !(test::missingFunction && !wcscmp(name,test::missingFunction))?const_cast<wchar_t*>(name):nullptr;
}
void* FindObject(const wchar_t* name,const wchar_t* type){
 if(!wcscmp(name,L"grayboar_damage_Cue")){assert(!wcscmp(type,L"SoundCue"));return &test::audio;}
 if(!wcscmp(name,L"grayboar_idle4")){assert(!wcscmp(type,L"SoundWave"));return &test::audio;}
 if(!wcscmp(name,L"att_default")){assert(!wcscmp(type,L"SoundAttenuation"));return &test::audio;}
 assert(!wcscmp(name,L"grayboar_Skeleton_Montage")&&!wcscmp(type,L"AnimMontage"));return &test::montage;
}
void* FindClassDefaultObject(const wchar_t* name){assert(!wcscmp(name,L"GameplayStatics"));return &test::cls;}
}
namespace ue_wrap::fname_utils {
reflection::FName StringToFName(const std::wstring& name){
 test::section=0;for(int i=1;i<=7;++i)if(name==coop::grayboar_actions::Section(uint8_t(i)))test::section=i;
 assert(test::section);return {test::section,0};
}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(64){}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t){
 if(!wcscmp(name,L"InTimeToStartMontageAt"))test::startPosition=*static_cast<const float*>(value);
 return !test::refuseParam;
}
bool ParamFrame::GetRaw(const wchar_t*,void* out,int32_t size)const {
 if(!wcscmp(static_cast<const wchar_t*>(fn_),L"Montage_Play") && test::returnReadFails)return false;
 if(!wcscmp(static_cast<const wchar_t*>(fn_),L"GetAnimInstance")){assert(size==sizeof(void*));std::memcpy(out,&test::returnedAnim,size);}
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
 if(test::callbackFunction && !wcscmp(name,test::callbackFunction)) {
  switch(test::callbackMode) {
   case 1:++test::serials[0];break;
   case 2:++test::serials[2];break;
   case 3:++test::serials[3];break;
   case 4:++test::world;break;
   case 5:coop::grayboar_actions::OnDisconnect();break;
   case 6:coop::element::Registry::Get().row.mirror=false;break;
   case 7:coop::element::Registry::Get().row.actor=&test::others[0];break;
   case 8:test::actor.mesh=nullptr;break;
   case 9:test::returnReadFails=true;break;
   case 10:
    coop::grayboar_actions::Receive({42,2,1,2},0);
    coop::grayboar_actions::Tick();break;
  }
 }
 const bool failed=test::failedCall && !wcscmp(name,test::failedCall);
 if(failed && wcscmp(name,L"Montage_Stop") && wcscmp(name,L"Montage_JumpToSection"))return false;
 if(!wcscmp(name,L"GetAnimInstance"))return true;
 if(!wcscmp(name,L"Montage_IsPlaying") || !wcscmp(name,L"Montage_GetCurrentSection") ||
    !wcscmp(name,L"Montage_GetPosition") || !wcscmp(name,L"Montage_GetPlayRate")) {
  if(test::failQuery && !wcscmp(name,test::failQuery))return false;
  if(test::queryTeardown && !wcscmp(name,L"Montage_GetPlayRate")) {
   if(test::queryTeardown==1)++test::serials[0];
   if(test::queryTeardown==2)coop::grayboar_actions::OnDisconnect();
   if(test::queryTeardown==3)++test::world;
   if(test::queryTeardown==4)coop::element::Registry::Get().row.mirror=true;
   if(test::queryTeardown==5)coop::grayboar_actions::OnPeerLeft(1);
   if(test::queryTeardown==6)coop::element::Registry::Get().row.actor=&test::others[0];
  }
  return true;
 }
 if(!wcscmp(name,L"Montage_Stop")){++test::stops;return !failed;}
 if(!wcscmp(name,L"Stop")){++test::silences;return true;}
 if(!wcscmp(name,L"SetSound")){
  ++test::damageSets;if(test::recycleOnSet)++test::serials[0];
  if(test::disconnectOnSet)coop::grayboar_actions::OnDisconnect();return true;
 }
 if(!wcscmp(name,L"SpawnSoundAttached")){++test::deathSounds;return true;}
 if(!wcscmp(name,L"Montage_Play")){
  ++test::plays;if(test::recycle)++test::serials[0];
  if(test::disconnect)coop::grayboar_actions::OnDisconnect();return !test::refusePlay;
 }
 if(!wcscmp(name,L"Montage_JumpToSection")){
  ++test::jumps;
  if(!failed && !test::jumpNoEffect){test::playedSections.push_back(test::section);test::hostSection=test::section;}
  return !failed;
 }
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
 auto edge=[&](uint8_t kind){hostSection=kind;actor.biting=actor.roaring=0;A::Before(call,kind);(kind==1?actor.biting:actor.roaring)=1;A::After(call,kind);};
 edge(1);A::Tick();assert(session.sends==1&&session.sent.eid==42&&session.sent.action==1&&session.sent.rate==1);
 A::Before(call,1);A::After(call,1);A::Tick();assert(session.sends==1); // Active call is no new action.
 actor.biting=0;A::Before(call,1);A::After(call,1);assert(A::g_out.empty()); // Refused native start.
 actor.biting=0;A::Before(call,1); // Another gate cancels: no post callback.
 edge(1);assert(A::g_observations.empty()&&A::g_out.size()==1);A::g_out.clear();
 actor.dying=1;edge(2);assert(A::g_out.empty());actor.dying=0;
 registry.row.mirror=true;edge(1);assert(A::g_out.empty());registry.row.mirror=false;
 hostRate=std::numeric_limits<float>::quiet_NaN();edge(1);assert(A::g_out.empty());hostRate=1;
 session.sendOk=false;edge(2);A::Tick();assert(A::g_out.size()==1);
 session.sendOk=true;A::Tick();assert(A::g_out.empty()&&session.sent.action==2);
 session.readyMask=0;edge(1);assert(A::g_out.empty());session.readyMask=2;
 edge(1);A::OnPeerLeft(1);const int beforeLeft=session.sends;A::Tick();assert(session.sends==beforeLeft&&A::g_out.empty());
 edge(1);A::g_out.front().expires=0;const int sent=session.sends;A::Tick();assert(session.sends==sent);
 actor.biting=0;A::Before(call,1);++serials[0];actor.biting=1;A::After(call,1);assert(A::g_observations.empty()&&A::g_out.empty());
 puts("PASS host action edges: native refusal, active/dead/mirror boars, rate, recycled identity, send retry and expiry");
 // Join queries sample native montage truth, including an action not observed
 // by Before/After (e.g. begun before any peer was ready).
 actor.biting=1;actor.roaring=0;hostPlaying=true;hostSection=1;hostRate=1.5f;
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
 invalid=p;invalid.action=8;A::Receive(invalid,0);assert(A::g_in.empty());
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
 assert(A::g_in.empty());
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
 auto death=[&]{hostSection=3;actor.dying=0;A::Before(call,3);actor.dying=1;A::After(call,3);};
 actor.rate=1;hostRate=.5f;death();A::Tick();assert(session.sent.action==3&&session.sent.rate==0.5f);
 A::Before(call,3);A::After(call,3);assert(A::g_out.empty()); // No duplicate death transition.
 actor.skipDeath=1;death();assert(A::g_out.empty());actor.skipDeath=0;
 actor.rate=.05f;hostRate=.025f;death();A::Tick();assert(session.sent.rate==.025f);actor.rate=1;
 hostSection=3;hostRate=.5f;A::QueueConnect(1);A::Tick();assert(session.sent.action==3&&session.sent.position>=0);
 A::OnDisconnect();session.mode=coop::net::Role::Client;A::Install(&session);registry.row.mirror=true;
 p={42,1,.5f,3};A::Receive(p,0);A::Tick();assert(section==3&&deathSounds==1&&silences==3);
 ++p.sequence;p.position=.4f;A::Receive(p,0);A::Tick();assert(deathSounds==1&&silences==6);
 puts("PASS death cosmetics: half native rate, no repeated/skip-animation cue, mid-death join, direct audio and loop silence without gameplay delegates");
 A::OnDisconnect();session.mode=coop::net::Role::Host;A::Install(&session);registry.row.mirror=false;
 actor.dying=0;actor.health=100;actor.biting=actor.roaring=0;hostRate=1.25f;
 call.function=&cls;call.locals=reinterpret_cast<uint8_t*>(&damageArgs);
 auto hit=[&](float amount){A::Before(call,3);actor.health-=amount;A::After(call,3);};
 for(int variant=4;variant<=7;++variant){
  hostSection=variant;hit(1);assert(A::g_out.size()==1);A::Tick();
  assert(session.sent.action==variant&&session.sent.rate==hostRate&&session.sent.position==-1);
 }
 hostSection=4;hit(1);A::Tick();const auto firstHit=session.sent.sequence;
 hit(1);A::Tick();assert(session.sent.action==4&&session.sent.sequence>firstHit); // Repeated same-section hits are distinct.
 hit(0);assert(A::g_out.empty());hit(-5);assert(A::g_out.empty()); // Refused hit / healing with an old damage montage.
 damageArgs.skip=1;hit(1);assert(A::g_out.empty());damageArgs.skip=0;
 skipOffset=-1;hit(1);assert(A::g_out.empty());skipOffset=damageFrameSize;hit(1);assert(A::g_out.empty());skipOffset=offsetof(DamageArgs,skip);
 call.locals=nullptr;hit(1);assert(A::g_out.empty());call.locals=reinterpret_cast<uint8_t*>(&damageArgs);
 hostPlaying=false;hit(1);assert(A::g_out.empty());hostPlaying=true;
 hostSection=1;hit(1);assert(A::g_out.empty());hostSection=4;
 registry.row.mirror=true;hit(1);assert(A::g_out.empty());registry.row.mirror=false;
 A::Before(call,3);++serials[0];actor.health-=1;A::After(call,3);assert(A::g_out.empty());
 hostRate=std::numeric_limits<float>::quiet_NaN();hit(1);assert(A::g_out.empty());hostRate=1.25f;
 actor.health=std::numeric_limits<float>::quiet_NaN();hit(1);assert(A::g_out.empty());actor.health=100;
 A::Before(call,3);actor.health=0;actor.dying=1;hostSection=3;A::After(call,3);A::Tick();assert(session.sent.action==3);
 actor.dying=0;actor.health=100;
 for(int variant=4;variant<=7;++variant){hostSection=variant;A::QueueConnect(1);A::Tick();assert(session.sent.action==variant&&session.sent.position>=0);}
 puts("PASS nonlethal host reactions: native damage1..4 choice/rate, repeated hits, health decrease, skip/malformed refusal, live montage, identity, lethal precedence and joined reactions");

 A::OnDisconnect();session.mode=coop::net::Role::Client;A::Install(&session);registry.row.mirror=true;
 p={42,1,1.25f,4};const float healthBefore=actor.health;
 for(uint8_t variant=4;variant<=7;++variant){
  p.action=variant;++p.sequence;const int oldSounds=sounds,oldStops=silences;
  A::Receive(p,0);A::Tick();assert(section==variant&&sounds==oldSounds+1&&silences==oldStops+1&&A::g_in.empty());
 }
 assert(damageSets==4&&actor.health==healthBefore);
 const int snapshotSounds=sounds,snapshotSets=damageSets,snapshotJumps=jumps;
 for(uint8_t variant=4;variant<=7;++variant){
  p.action=variant;p.position=.4f;hostSection=variant;++p.sequence;
  A::Receive(p,0);A::Tick();assert(sounds==snapshotSounds&&damageSets==snapshotSets&&jumps==snapshotJumps);
 }
 ++p.sequence;p.position=-1;const int beforeBadSound=sounds;
 recycleOnSet=true;A::Receive(p,0);A::Tick();assert(sounds==beforeBadSound&&A::g_in.empty());recycleOnSet=false;
 disconnectOnSet=true;++p.sequence;A::Receive(p,0);A::Tick();assert(sounds==beforeBadSound&&A::g_in.empty());disconnectOnSet=false;
 assert(actor.health==healthBefore);
 puts("PASS nonlethal client cosmetics: selected four sections, damage cue and attack-audio stop, no health writes, silent join seek and audio teardown");

 A::OnDisconnect();session.mode=coop::net::Role::Host;A::Install(&session);registry.row.mirror=false;
 actor.dying=0;actor.rate=9;hostRate=.75f;hostPosition=.4f;
 auto nativeEdge=[&](uint8_t action,int nativeSection){
  actor.biting=actor.roaring=actor.dying=0;hostSection=nativeSection;
  A::Before(call,action);
  (action==1?actor.biting:action==2?actor.roaring:actor.dying)=1;
  A::After(call,action);
 };
 for(uint8_t action=1;action<=3;++action) {
  const int sentBefore=session.sends;
  hostPlaying=false;nativeEdge(action,action);A::Tick();assert(session.sends==sentBefore&&A::g_out.empty());
  hostPlaying=true;nativeEdge(action,action==1?2:1);A::Tick();assert(session.sends==sentBefore&&A::g_out.empty());
  for(const auto* query:{L"Montage_IsPlaying",L"Montage_GetCurrentSection",L"Montage_GetPosition",L"Montage_GetPlayRate"}) {
   failQuery=query;nativeEdge(action,action);assert(A::g_out.empty());
  }
  failQuery=nullptr;
  ready=false;nativeEdge(action,action);assert(A::g_out.empty());ready=true;
  actor.mesh=nullptr;nativeEdge(action,action);assert(A::g_out.empty());actor.mesh=&mesh;
  for(float rate:{0.f,-1.f,11.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
   hostRate=rate;nativeEdge(action,action);assert(A::g_out.empty());
  }
  hostRate=.75f;
  for(float position:{-1.f,3601.f,std::numeric_limits<float>::quiet_NaN()}) {
   hostPosition=position;nativeEdge(action,action);assert(A::g_out.empty());
  }
  hostPosition=.4f;
  for(int teardown=1;teardown<=4;++teardown) {
   queryTeardown=teardown;nativeEdge(action,action);assert(A::g_out.empty());
   queryTeardown=0;registry.row.mirror=false;A::Install(&session);
  }
  nativeEdge(action,action);A::Tick();
  assert(session.sends==sentBefore+1&&session.sent.action==action&&session.sent.rate==hostRate&&session.sent.position==-1.f);
 }
 puts("PASS live host montage truth: attack/roar/death require matching playback, use native rate, reject unavailable/failed queries and revalidate identity/session/world/authority");
 QueueRegressions();
 PlaybackRegressions();
}

void test::QueueRegressions() {
 namespace A=coop::grayboar_actions;
 using coop::net::Role;using coop::net::GrayboarActionPayload;
 coop::net::Session session;auto& registry=coop::element::Registry::Get();
 auto reset=[&](Role role) {
  A::OnDisconnect();session=coop::net::Session{};session.mode=role;
  registry.extra.clear();registry.id=42;registry.present=true;
  registry.row={&actor,role==Role::Client,true};
  actor=Actor{};actor.mesh=&mesh;actor.attack=actor.roar=&audio;
  for(auto& other:others){other=actor;}
  hostPlaying=ready=live=boar=true;refusePlay=refuseParam=recycle=disconnect=false;
  queryTeardown=0;failQuery=nullptr;hostRate=1;hostPosition=.4f;hostSection=1;duration=2;
  playedSections.clear();A::Install(&session);
 };
 ue_wrap::script_gate::Call call{};call.object=&actor;call.stack=&stack;
 auto edge=[&](uint8_t action) {
  actor.biting=actor.roaring=actor.dying=0;hostSection=action;
  A::Before(call,action);(action==1?actor.biting:actor.roaring)=1;A::After(call,action);
 };
 auto sequences=[&](int slot) {
  std::vector<uint32_t> result;
  for(const auto& send:session.delivered)if(send.slot==slot)result.push_back(send.packet.sequence);
  return result;
 };
 reset(Role::Host);session.readyMask=14;session.failMask=4;
 A::g_sequence=0xfffffffeu;edge(1);edge(2);A::Tick();
 const std::vector<uint32_t> ordered{0xffffffffu,1};
 assert(sequences(1)==ordered&&sequences(3)==ordered&&sequences(2).empty());
 assert(A::g_out.size()==2&&A::g_out[0].peers==4&&A::g_out[1].peers==4);
 assert(session.attempts.size()==5); // One failed attempt; no later sequence can bypass it for this peer.
 A::Tick();assert(session.attempts.size()==6&&session.delivered.size()==4);
 session.failMask=0;A::Tick();
 assert(A::g_out.empty()&&sequences(2)==ordered&&sequences(1)==ordered&&sequences(3)==ordered);

 reset(Role::Host);session.readyMask=6;session.failMask=4;edge(1);edge(2);A::Tick();
 const uint32_t later=A::g_out.back().packet.sequence;
 A::g_out.front().expires=0;session.failMask=0;A::Tick();
 assert(sequences(2)==std::vector<uint32_t>{later}&&A::g_out.empty());
 reset(Role::Host);session.readyMask=6;session.failMask=4;edge(1);edge(2);A::Tick();
 session.readyMask=2;A::Tick();session.readyMask=6;session.failMask=0;A::Tick();
 assert(sequences(2).empty()&&A::g_out.empty());
 reset(Role::Host);session.readyMask=6;session.failMask=4;edge(1);edge(2);A::Tick();
 A::OnPeerLeft(2);session.failMask=0;edge(1);A::Tick();
 assert(sequences(2)==std::vector<uint32_t>{3});
 puts("PASS per-peer delivery: failed recipient isolation, FIFO across sequence wrap, no duplicates, expiry, readiness loss and recycled slots");

 reset(Role::Host);edge(1);session.sendHook=[] {A::OnPeerLeft(1);};
 A::Tick();assert(A::g_out.empty()&&session.delivered.size()==1);
 reset(Role::Host);session.readyMask=6;edge(1);edge(2);
 session.sendHook=[] {A::OnPeerLeft(1);};A::Tick();
 assert(sequences(2).empty()&&A::g_out.size()==2);
 session.sendHook=nullptr;A::Tick();assert(A::g_out.empty()&&sequences(2)==std::vector<uint32_t>({1,2}));
 reset(Role::Host);edge(1);session.sendHook=[] {A::OnDisconnect();};A::Tick();assert(A::g_out.empty());
 puts("PASS shared fanout reentrancy: recipient departure/queue erasure during send, other-peer FIFO and disconnect");

 reset(Role::Host);session.readyMask=6;session.failMask=4;
 for(int i=0;i<42;++i){edge(1);A::Tick();}
 assert(A::g_out.size()==32&&sequences(1).size()==42&&sequences(2).empty());
 for(const auto& cue:A::g_out)assert(cue.peers==4);
 session.failMask=0;A::Tick();assert(A::g_out.empty()&&sequences(2).size()==32&&sequences(1).size()==42);
 reset(Role::Host);
 for(int slot=1;slot<=3;++slot) {
  session.readyMask=1u<<slot;
  for(int i=0;i<32;++i)edge(1);
 }
 assert(A::g_out.size()==96);
 session.readyMask=14;edge(1);assert(A::g_out.size()==96);
 for(auto& cue:A::g_out)cue.expires=0;
 edge(1);assert(A::g_out.size()==1&&A::g_out.front().peers==14);
 reset(Role::Host);session.readyMask=4;
 for(int i=0;i<32;++i)edge(1);
 session.readyMask=6;session.failMask=4;A::QueueConnect(2);A::QueueConnect(1);A::Tick();
 assert(sequences(1).size()==1&&session.delivered.front().packet.position>=0&&A::g_out.size()==32);
 assert(A::g_joins.size()==1&&A::g_joins.front().slot==2);
 session.failMask=0;A::Tick();A::Tick();
 assert(sequences(2).size()==33&&A::g_joins.empty()&&A::g_out.empty());
 puts("PASS host saturation: per-peer capacity, fast-peer progress past 32 failed cues, 96-row total bound, expired capacity and independent join retries");

 reset(Role::Host);session.sendOk=false;actor.biting=1;A::QueueConnect(1);A::Tick();
 assert(A::g_out.size()==1&&A::g_out.front().packet.position>=0);
 const float sampled=A::g_out.front().packet.position;A::g_out.front().sampled-=250;
 session.sendOk=true;A::Tick();
 assert(session.sent.position>=sampled+.25f&&session.sent.position<sampled+.5f);
 for(int invalidation=0;invalidation<3;++invalidation) {
  reset(Role::Host);edge(1);
  if(invalidation==0)++serials[0];
  if(invalidation==1)registry.row.actor=&others[0];
  if(invalidation==2)registry.row.mirror=true;
  A::Tick();assert(session.delivered.empty()&&A::g_out.empty());
 }
 for(int teardown:{1,2,3,4,5,6}) {
  reset(Role::Host);actor.biting=1;A::QueueConnect(1);queryTeardown=teardown;A::Tick();
  assert(session.delivered.empty()&&A::g_out.empty());queryTeardown=0;
 }
 puts("PASS queued host identity and join lifecycle: retry age, recycled/remapped actors, authority loss and peer/session/world teardown during sampling");

 reset(Role::Client);registry.extra.push_back({43,{&others[0],true,true}});actor.mesh=nullptr;
 A::Receive({42,1,1,1},0);A::Receive({42,2,1,2},0);A::Receive({43,3,1,4},0);A::Tick();
 assert(playedSections==std::vector<int>{4}&&A::g_in.size()==2);
 actor.mesh=&mesh;A::Tick();assert((playedSections==std::vector<int>{4,1})&&A::g_in.size()==1);
 A::Tick();assert((playedSections==std::vector<int>{4,1,2})&&A::g_in.empty());
 // A missing first entity must not block an already materialized second one.
 reset(Role::Client);registry.extra.push_back({43,{&others[0],true,true}});registry.present=false;
 A::Receive({42,1,1,1},0);A::Receive({43,2,1,2},0);A::Tick();
 assert(playedSections==std::vector<int>{2}&&A::g_in.size()==1);
 registry.present=true;A::Tick();assert((playedSections==std::vector<int>{2,1})&&A::g_in.empty());
 puts("PASS client entity isolation: unloaded/missing first actor does not block another; each actor retains FIFO and one action per tick");

 for(bool wrap:{false,true}) {
  reset(Role::Client);actor.mesh=nullptr;
  uint32_t sequence=wrap?0xfffffffbu:1u;
  for(int i=0;i<8;++i) {
   others[i].mesh=i==7?&mesh:nullptr;
   registry.extra.push_back({uint32_t(43+i),{&others[i],true,true}});
  }
  auto receive=[&](uint32_t eid) {if(!sequence)++sequence;A::Receive({eid,sequence++,1,1},0);};
  receive(42);for(uint32_t eid=43;eid<=50;++eid)receive(eid);
  const int before=plays;
  A::Tick();assert(plays==before);
  A::Tick();assert(plays==before);
  A::Tick();assert(plays==before+1&&A::g_in.size()==8);
 }
 reset(Role::Client);refusePlay=true;
 for(uint32_t i=0;i<8;++i)registry.extra.push_back({43+i,{&others[i],true,true}});
 for(uint32_t i=0;i<9;++i)A::Receive({42+i,1+i,1,1},0);
 const int beforeAttempts=plays;A::Tick();assert(plays==beforeAttempts+4);
 A::Tick();assert(plays==beforeAttempts+8);refusePlay=false;
 // Four newly queued events per tick cannot jump ahead of an older retry.
 reset(Role::Client);actor.mesh=nullptr;
 for(uint32_t i=0;i<4;++i)registry.extra.push_back({43+i,{&others[i],true,true}});
 uint32_t nextSequence=1;A::Receive({42,nextSequence++,1,7},0);
 for(uint32_t i=0;i<4;++i)A::Receive({43+i,nextSequence++,1,1},0);
 A::Tick();actor.mesh=&mesh;
 for(int tick=0;tick<3;++tick) {
  for(uint32_t i=0;i<4;++i)A::Receive({43+i,nextSequence++,1,1},0);
  A::Tick();
 }
 assert(std::count(playedSections.begin(),playedSections.end(),7)==1);
 // Flooding one unavailable entity does not consume every playback attempt.
 reset(Role::Client);registry.extra.push_back({43,{&others[0],true,true}});actor.mesh=nullptr;
 for(uint32_t sequence=1;sequence<=31;++sequence)A::Receive({42,sequence,1,1},0);
 A::Receive({43,32,1,2},0);A::Tick();
 assert(playedSections==std::vector<int>{2}&&A::g_in.size()==31);
 for(auto& cue:A::g_in)cue.expires=0;
 A::Tick();assert(A::g_in.empty());
 puts("PASS bounded client fairness: four entity heads per tick, rotation/wrap, full queue, one-entity flood and expiry cleanup");

 for(int invalidation=0;invalidation<4;++invalidation) {
  reset(Role::Client);A::Receive({42,1,1,1},0);
  if(invalidation==0)++serials[0];
  if(invalidation==1)registry.row.actor=&others[0];
  if(invalidation==2)registry.row.mirror=false;
  if(invalidation==3)registry.present=false;
  const int before=plays;A::Tick();assert(plays==before&&A::g_in.empty());
 }
 reset(Role::Client);registry.present=false;A::Receive({42,1,1,1},0);
 registry.present=true;ready=false;A::Tick();++serials[0];ready=true;
 const int before=plays;A::Tick();assert(plays==before&&A::g_in.empty());
 reset(Role::Client);registry.extra.push_back({43,{&others[0],true,true}});
 A::Receive({42,1,1,1},0);A::Receive({43,2,1,2},0);disconnect=true;A::Tick();disconnect=false;
 assert(A::g_in.empty()&&A::g_retryActors.empty());
 A::Install(&session);A::Receive({42,1,1,2},0);A::Tick();assert(playedSections==std::vector<int>{2});
 puts("PASS client bound identity: recycled/remapped/lost mirrors, late binding, disconnect during playback and fresh-session scheduling reset");
 A::OnDisconnect();registry.extra.clear();
}

void test::PlaybackRegressions() {
 namespace A=coop::grayboar_actions;
 using coop::net::Role;using coop::net::GrayboarActionPayload;
 coop::net::Session session;auto& registry=coop::element::Registry::Get();
 auto reset=[&] {
  A::OnDisconnect();session=coop::net::Session{};session.mode=Role::Client;
  registry.extra.clear();registry.id=42;registry.present=true;registry.row={&actor,true,true};
  actor=Actor{};actor.mesh=&mesh;actor.attack=actor.roar=&audio;for(auto& other:others)other=actor;
  hostPlaying=ready=live=boar=true;refusePlay=refuseParam=recycle=disconnect=false;
  disconnectOnSet=recycleOnSet=jumpNoEffect=returnReadFails=false;
  failQuery=missingFunction=failedCall=callbackFunction=nullptr;callbackMode=queryTeardown=0;
  returnedAnim=&anim;hostSection=1;hostRate=1;hostPosition=.4f;duration=2;
  plays=jumps=sounds=stops=silences=deathSounds=damageSets=0;playedSections.clear();
  A::Install(&session);
 };
 for(bool resume:{false,true})for(const auto* query:{L"Montage_IsPlaying",L"Montage_GetCurrentSection"}) {
  reset();failedCall=query;A::Receive({42,1,1,1,resume?.4f:-1.f},0);A::Tick();
  assert(plays==1&&jumps==(resume?0:1)&&sounds==0&&A::g_in.size()==1);
  A::Tick();assert(plays==1&&jumps==(resume?0:1)&&sounds==0);
  failedCall=nullptr;A::Tick();
  assert(plays==1&&jumps==(resume?0:1)&&sounds==(resume?0:1)&&A::g_in.empty());
 }
 reset();failedCall=L"Montage_GetCurrentSection";A::Receive({42,1,1,1},0);A::Tick();
 failedCall=nullptr;hostPlaying=false;A::Tick();assert(plays==1&&sounds==0&&A::g_in.empty());
 puts("PASS playback verification retries: accepted montage/seek run once, query recovery resumes verification, stopped playback emits no sound");

 for(bool resume:{false,true}) {
  reset();hostSection=2;jumpNoEffect=true;failedCall=L"Montage_Stop";
  A::Receive({42,1,1,1,resume?.4f:-1.f},0);A::Tick();
  assert(plays==1&&stops==1&&sounds==0&&A::g_in.size()==1);
  A::Tick();assert(plays==1&&stops==2&&sounds==0);
  failedCall=nullptr;A::Tick();assert(plays==1&&stops==3&&sounds==0&&A::g_in.empty());
 }
 reset();failedCall=L"Montage_JumpToSection";A::Receive({42,1,1,1},0);A::Tick();
 assert(plays==1&&jumps==1&&stops==1&&sounds==0&&A::g_in.empty());
 reset();failedCall=L"Montage_GetCurrentSection";A::Receive({42,1,1,1},0);A::Tick();
 A::g_in.front().expires=0;failedCall=L"Montage_Stop";A::Tick();
 assert(plays==1&&stops==1&&sounds==0&&A::g_in.empty());
 reset();hostSection=2;failedCall=L"Montage_Stop";A::Receive({42,1,1,1,.4f},0);A::Tick();
 A::g_in.front().expires=0;A::Tick();assert(plays==1&&stops==2&&A::g_in.empty());
 reset();failedCall=L"Montage_GetCurrentSection";A::Receive({42,1,1,1},0);A::Tick();
 boar=false;A::g_in.front().expires=0;A::Tick();assert(plays==1&&stops==0&&A::g_in.empty());
 puts("PASS targeted cancellation: failed/no-op seek and wrong joined section stop only the stock montage; failed stops retry without replay and expire boundedly");

 reset();missingFunction=L"Montage_JumpToSection";A::Receive({42,1,1,1,.4f},0);A::Tick();
 assert(plays==1&&jumps==0&&sounds==0&&A::g_in.empty());
 reset();missingFunction=L"Montage_JumpToSection";A::Receive({42,1,1,1},0);A::Tick();
 assert(!plays&&A::g_in.size()==1);missingFunction=nullptr;A::Tick();assert(plays==1&&sounds==1);
 for(const auto* fn:{L"Montage_Stop",L"Montage_IsPlaying",L"Montage_GetCurrentSection"}) {
  reset();missingFunction=fn;A::Receive({42,1,1,1},0);A::Tick();assert(!plays&&A::g_in.size()==1);
 }
 reset();returnReadFails=true;A::Receive({42,1,1,1},0);A::Tick();assert(!plays&&A::g_in.size()==1);
 returnReadFails=false;A::Tick();assert(plays==1&&sounds==1&&A::g_in.empty());
 for(float invalidDuration:{-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
  reset();duration=invalidDuration;A::Receive({42,1,1,1},0);A::Tick();
  assert(plays==1&&stops==1&&jumps==0&&sounds==0&&A::g_in.empty());
 }
 reset();callbackFunction=L"Montage_Play";callbackMode=9;A::Receive({42,1,1,1},0);A::Tick();
 assert(plays==1&&stops==1&&jumps==0&&sounds==0&&A::g_in.empty());
 reset();refusePlay=true;A::Receive({42,1,1,1},0);A::Tick();assert(plays==1&&A::g_in.size()==1);
 refusePlay=false;A::Tick();assert(plays==2&&sounds==1&&A::g_in.empty());
 puts("PASS playback prerequisites: snapshots need no jump function, cancellation/query/return metadata preflight, zero/refused start retry and uncertain result cleanup");

 for(const auto* stage:{L"Montage_Play",L"Montage_JumpToSection",L"Montage_IsPlaying",L"Montage_GetCurrentSection"})
  for(int mode=1;mode<=8;++mode) {
   reset();callbackFunction=stage;callbackMode=mode;A::Receive({42,1,1,1},0);A::Tick();
   assert(sounds==0&&deathSounds==0&&damageSets==0);
   if(mode!=4)assert(A::g_in.empty()); // World travel is retired by Install at the new generation.
   callbackFunction=nullptr;A::Install(&session);
  }
 for(int mode=1;mode<=8;++mode) {
  reset();callbackFunction=L"Stop";callbackMode=mode;A::Receive({42,1,1,3},0);A::Tick();
  assert(plays==1&&silences==1&&deathSounds==0);
  reset();callbackFunction=L"SetSound";callbackMode=mode;A::Receive({42,1,1,4},0);A::Tick();
  assert(damageSets==1&&sounds==0&&silences==0);
 }
 for(int replacement=0;replacement<4;++replacement) {
  reset();failedCall=L"Montage_GetCurrentSection";A::Receive({42,1,1,1},0);A::Tick();
  if(replacement==0)++serials[2];
  if(replacement==1)++serials[3];
  if(replacement==2)returnedAnim=&cls;
  if(replacement==3)actor.mesh=&cls;
  failedCall=nullptr;A::Tick();assert(plays==1&&sounds==0&&stops==0&&A::g_in.empty());
 }
 for(int mode:{2,3}) {
  reset();failedCall=L"Montage_GetCurrentSection";A::Receive({42,1,1,1},0);A::Tick();
  failedCall=nullptr;callbackFunction=L"GetAnimInstance";callbackMode=mode;A::Tick();
  assert(plays==1&&sounds==0&&stops==0&&A::g_in.empty());
 }
 puts("PASS playback ownership: actor/animation/asset/world/session/authority/mesh changes stop subsequent calls and audio; replacement instances never inherit pending verification");

 reset();registry.extra.push_back({43,{&others[0],true,true}});
 callbackFunction=L"Montage_GetCurrentSection";callbackMode=10;failedCall=L"Montage_GetCurrentSection";
 A::Receive({42,1,1,1},0);A::Tick();assert(plays==1&&A::g_in.size()==2);
 callbackFunction=failedCall=nullptr;A::Tick();
 assert(plays==1&&sounds==1&&A::g_in.size()==1);A::Tick();
 assert(plays==2&&sounds==2&&A::g_in.empty());
 puts("PASS reentrant receipt: packets appended during reflection preserve playback progress; nested Tick does not replay the in-flight cue");
 A::OnDisconnect();registry.extra.clear();
}
