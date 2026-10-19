#include "../../src/votv-coop/src/coop/creatures/grayboar_turn.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <cassert>
#include <cstdio>
#include <cwchar>
#include <limits>

namespace test {
struct Actor {void* audio=nullptr;} actor;
struct Animation {void* boar=&actor;float turn=0;} anim;
int audio=0;
void* objects[]={&actor,&anim,&audio};int serials[]={1,1,1};
uint32_t world=1;int watches=0,pitches=0,reenter=0;float pitch=0;
bool watchOk=false,ready=true,paramOk=true,live=true,boar=true,animation=true;
ue_wrap::script_gate::PostFn after=nullptr;
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return &test::world;}
void* WorldOf(void*){return &test::world;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<3;++i)if(p==test::objects[i])return i;return -1;}
bool IsLive(void* p){return test::live&&InternalIndexOf(p)>=0;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<3&&test::objects[i]==p&&test::live;}
int32_t SlotSerial(int32_t i){return i>=0&&i<3?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p;}
void* SuperStructOf(void*){return nullptr;}
const FName& NameOf(void* cls){static FName n;n.ComparisonIndex=cls==&test::actor?1:cls==&test::anim?2:3;return n;}
bool NameEquals(const FName& n,const wchar_t* name){return
 (n.ComparisonIndex==1&&test::boar&&!wcscmp(name,L"grayboar_C"))||
 (n.ComparisonIndex==2&&test::animation&&!wcscmp(name,L"grayboar_Skeleton_AnimBlueprint_C"));}
int32_t FindPropertyOffset(void*,const wchar_t* name){
 if(!wcscmp(name,L"grayBoar"))return offsetof(test::Animation,boar);
 if(!wcscmp(name,L"turn"))return offsetof(test::Animation,turn);
 assert(!wcscmp(name,L"audio_loop_turn"));return offsetof(test::Actor,audio);
}
void* FindDispatchFunctionCached(void*,const wchar_t* name){
 assert(!wcscmp(name,L"SetPitchMultiplier"));return test::ready?const_cast<wchar_t*>(name):nullptr;
}
}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* cls,const wchar_t* name,int,PreFn pre,PostFn post){
 assert(!wcscmp(cls,L"grayboar_Skeleton_AnimBlueprint_C")&&!wcscmp(name,L"BlueprintUpdateAnimation")&&!pre&&post);
 ++test::watches;test::after=post;return test::watchOk;
}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(4){}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t size){
 assert(!wcscmp(name,L"NewPitchMultiplier")&&size==4);test::pitch=*static_cast<const float*>(value);
 if(test::reenter==1)coop::grayboar_turn::OnDisconnect();
 if(test::reenter==2)++test::serials[0];
 return test::paramOk;
}
bool Call(void* object,ParamFrame&){
 assert(object==&test::audio);++test::pitches;
 if(test::reenter==3){script_gate::Call c{};c.object=&test::anim;test::after(c);}
 return true;
}
}
int main(){
 using namespace test;namespace T=coop::grayboar_turn;namespace N=coop::net;
 N::Session session;auto& entities=coop::element::Registry::Get();
 entities.row.actor=&actor;entities.row.mirror=false;entities.id=42;
 actor.audio=&audio;
 T::Install(&session);assert(watches==1);watchOk=true;T::Install(&session);T::Install(&session);assert(watches==2);
 ue_wrap::script_gate::Call c{};c.object=&anim;
 auto update=[&]{after(c);};
 anim.turn=.75f;update();assert(T::g_host.size()==1&&pitches==0);
 T::Tick();assert(session.turnSends==1&&session.turnSent.turn==.75f&&session.turnSent.eid==42);
 const auto first=session.turnSent.sequence;T::Tick();assert(session.turnSends==1);
 // Stable heartbeat repairs loss and supplies a join while the turn is constant.
 T::g_host[0].next=0;T::g_host[0].sent=0;T::Tick();assert(session.turnSends==2&&session.turnSent.sequence>first);
 session.sendOk=false;anim.turn=-.5f;update();T::g_host[0].next=0;T::Tick();assert(T::g_host[0].retry);
 anim.turn=-1;update();session.sendOk=true;T::g_host[0].next=0;T::Tick();
 assert(!T::g_host[0].retry&&session.turnSent.turn==-1&&session.turnSends==4);
 // No obsolete sample FIFO: a failed send is replaced by the latest animation.
 T::g_host[0].next=0;T::g_host[0].sent=0;session.readyMask=0;T::Tick();assert(session.turnSends==4);
 session.readyMask=6;T::Tick();assert(session.turnSends==6);session.readyMask=2;
 T::g_host[0].next=0;T::g_host[0].sent=0;entities.row.mirror=true;T::Tick();assert(session.turnSends==6);
 entities.row.mirror=false;entities.row.live=false;T::Tick();assert(session.turnSends==6);entities.row.live=true;
 ++serials[0];T::Tick();assert(T::g_host.empty());update();assert(T::g_host.size()==1);
 T::g_host[0].sampled=T::Now()-1501;T::Tick();assert(T::g_host.empty());
 for(float invalid:{2.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  anim.turn=invalid;update();assert(T::g_host.empty());
 }
 anim.turn=.5f;entities.row.mirror=true;update();assert(T::g_host.empty());entities.row.mirror=false;
 entities.id=N::kMaxPeers*coop::element::kHostRangeSize;update();assert(T::g_host.empty());entities.id=42;
 boar=false;update();assert(T::g_host.empty());boar=true;
 puts("PASS turn host: native sample, throttle, latest-state retry, stationary join heartbeat, readiness, live identity and invalid values");

 T::OnDisconnect();session.mode=N::Role::Client;entities.row.mirror=true;T::Install(&session);
 N::GrayboarTurnPayload packet{42,1,1};
 auto receive=[&]{T::Receive(packet,0);};
 T::Receive(packet,1);assert(T::g_client.empty());
 for(float invalid:{-1.01f,1.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  packet.turn=invalid;receive();assert(T::g_client.empty());
 }
 packet.turn=1;packet.eid=0;receive();assert(T::g_client.empty());
 packet.eid=coop::element::kHostRangeSize;receive();assert(T::g_client.empty());
 packet.eid=42;packet.sequence=0;receive();assert(T::g_client.empty());packet.sequence=1;
 receive();assert(T::g_client.size()==1);
 const auto sample=T::g_client[0];
 assert(T::Value(sample,sample.received)==0);
 assert(T::Value(sample,sample.received+50)==.5f);
 assert(T::Value(sample,sample.received+100)==1);
 T::g_client[0].received=T::Now()-100;anim.turn=-.9f;update();assert(anim.turn==1&&pitch==2);
 const int goodPitches=pitches;
 packet.turn=-1;receive();assert(T::g_client[0].to==1); // duplicate cannot undo current state
 packet.sequence=2;receive();assert(T::g_client[0].from==1&&T::g_client[0].to==-1);
 const auto reverse=T::g_client[0];assert(T::Value(reverse,reverse.received+50)==0);
 T::g_client[0].received=T::Now()-100;update();assert(anim.turn==-1&&pitch==2);
 packet.sequence=1;packet.turn=0;receive();assert(T::g_client[0].to==-1);
 T::g_seen=0xfffffffe;packet.sequence=1;receive();assert(T::g_seen==1&&T::g_client[0].to==0);
 entities.row.mirror=false;anim.turn=.9f;update();assert(anim.turn==.9f);entities.row.mirror=true;
 entities.row.live=false;update();assert(anim.turn==.9f);entities.row.live=true;
 animation=false;update();assert(anim.turn==.9f);animation=true;
 T::g_client[0].received=T::Now()-1501;update();assert(anim.turn==0&&pitch==.5f);
 T::Tick();assert(T::g_client.empty());anim.turn=.8f;update();assert(anim.turn==0); // no local steering before first state
 puts("PASS turn client: host-only finite/range validation, ordering/wrap, signed interpolation, native overwrite, idle pitch and stale neutral fallback");

 packet.sequence=2;packet.turn=.5f;receive();T::g_client[0].received=T::Now()-100;
 ready=false;const int before=pitches;update();assert(anim.turn==.5f&&pitches==before);ready=true;
 paramOk=false;update();assert(pitches==before);paramOk=true;
 reenter=2;update();assert(pitches==before);reenter=3;update();assert(pitches==before+1);
 reenter=1;update();assert(pitches==before+1&&T::g_client.empty());reenter=0;
 T::Install(&session);receive();assert(T::g_client.size()==1);++world;anim.turn=.9f;update();assert(anim.turn==.9f);
 T::Install(&session);assert(T::g_client.empty());
 session.linked=false;receive();assert(T::g_client.empty());session.linked=true;
 for(uint32_t i=1;i<=T::kLimit+1;++i){packet.eid=i;packet.sequence=i;receive();}
 assert(T::g_client.size()==T::kLimit);
 for(auto& row:T::g_client)row.received=T::Now()-1501;
 T::Tick();assert(T::g_client.empty());receive();assert(T::g_client.size()==1);
 assert(N::LaneForKind(N::ReliableKind::GrayboarTurn)==N::Lane::Normal);
 assert(!N::IsClientRelayableReliableKind(N::ReliableKind::GrayboarTurn));
 assert(!N::IsPreWorldSendableKind(N::ReliableKind::GrayboarTurn));
 puts("PASS turn lifecycle: missing audio metadata, recycled objects, recursive update, disconnect/world reset, bounded backlog and host-only lane");
}
