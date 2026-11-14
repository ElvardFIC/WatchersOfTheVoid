#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/types.h"
#include <atomic>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#define UE_LOGI(...) ((void)0)
#define UE_LOGW(...) ((void)0)
namespace test {
int original=1,replacement=2;int serial[2]={1,1};bool live[2]={true,true},killer[2]={true,true};
uint32_t world=1;uint64_t now=1000;int reads=0,moves=0,destroys=0;
int npcDestroyed=0;
bool readable=true;ue_wrap::FVector location{10,20,30};
bool dead=false,vitalsReady=true;
bool mapReady=true;
bool hasPlayer=true,sendGrab=true;
int grabAttempts=0,grabPackets=0,tearPackets=0,localTears=0;
std::function<void()> onRead,onMove,onDestroy,onClear;
std::function<void()> onNpcDestroy;
std::function<void()> onVitals;
std::function<void()> onVictimSend,onBroadcast,onLocalTear;
void Callback(std::function<void()>& fn){auto once=std::move(fn);fn={};if(once)once();}
int Index(void* p){return p==&original?0:p==&replacement?1:-1;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return test::Index(p);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<2&&test::live[i]&&test::Index(p)==i;}
bool IsLive(void* p){return IsLiveByIndex(p,test::Index(p));}
int32_t SlotSerial(int32_t i){return i>=0&&i<2?test::serial[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* WorldOf(void*){return nullptr;}void* CurrentWorld(){return nullptr;}}
namespace ue_wrap::wisp {bool IsKillerWisp(void* p){const int i=test::Index(p);return i>=0&&test::killer[i];}}
namespace ue_wrap::engine {
bool TryGetActorLocation(void* p,FVector& out){assert(p==&test::original||p==&test::replacement);++test::reads;out=test::location;test::Callback(test::onRead);return test::readable;}
bool SetActorLocation(void* p,const FVector& out){assert(p==&test::original||p==&test::replacement);assert(std::isfinite(out.Z));++test::moves;test::Callback(test::onMove);return true;}
bool DestroyActor(void* p){assert(p==&test::original||p==&test::replacement);++test::destroys;test::Callback(test::onDestroy);return true;}
}
namespace coop::element {
using ElementId=uint32_t;
struct Player {ElementId id=200;ElementId GetId()const{return id;}};
struct Element {
 void* actor=nullptr;int serial=0;
 void Bind(void* p){actor=p;serial=ue_wrap::reflection::SlotSerial(test::Index(p));}
 void* LiveActor(){return ue_wrap::reflection::IsLive(actor)&&serial==ue_wrap::reflection::SlotSerial(test::Index(actor))?actor:nullptr;}
};
struct Registry {Element rows[2];static Registry& Get(){static Registry r;return r;}
 Element* Get(ElementId eid){return eid>=100&&eid<102?&rows[eid-100]:nullptr;}};
}
namespace coop::npc_sync {
coop::element::ElementId GetNpcIdForActor(void* actor){
 if(!test::mapReady)return 0;
 const auto& rows=coop::element::Registry::Get().rows;
 for(unsigned i=0;i<2;++i)if(rows[i].actor==actor)return 100+i;return 0;
}
void SyncDestroyedNpcByEid(coop::element::ElementId eid,void* actor){
 assert(eid==100&&(actor==&test::original||actor==&test::replacement));++test::npcDestroyed;test::Callback(test::onNpcDestroy);}}
namespace coop::net {
enum class Role {Host,Client};
enum class ReliableKind {WispGrab,WispTear};
struct WispGrabPayload {uint32_t victimElementId=0,wispElementId=0,killDelayMs=0;};
struct WispTearPayload {uint32_t wispElementId=0,victimSlot=0;};
struct Session {
 bool active=true,ready=true;uint32_t generation=1;Role side=Role::Host;
 bool connected() const{return active;}Role role() const{return side;}
 bool IsSlotWorldReady(int slot)const{assert(slot==1);return ready;}
 uint32_t peerGenerationForSlot(int slot)const{assert(slot==1);return generation;}
 bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t size){
  assert(slot==1&&kind==ReliableKind::WispGrab&&size==sizeof(WispGrabPayload));
  const auto& p=*static_cast<const WispGrabPayload*>(data);assert(p.victimElementId==200&&p.wispElementId==100&&p.killDelayMs==3500);
  ++test::grabAttempts;if(!test::sendGrab)return false;++test::grabPackets;test::Callback(test::onVictimSend);return true;
 }
 bool SendReliable(ReliableKind kind,const void* data,size_t size){
  assert(kind==ReliableKind::WispTear&&size==sizeof(WispTearPayload));
  const auto& p=*static_cast<const WispTearPayload*>(data);assert(p.wispElementId==100&&p.victimSlot==1);
  ++test::tearPackets;test::Callback(test::onBroadcast);return true;
 }
};
}
namespace coop::wisp_tear_mirror {void OnWispTear(const coop::net::WispTearPayload&,uint8_t sender){assert(sender==0);++test::localTears;test::Callback(test::onLocalTear);}}
namespace coop {
struct RemotePlayer {void* actor=nullptr;float health=1;bool valid()const{return actor!=nullptr;}
 void* GetActor()const{return actor;}float GetHealth()const{return health;}};
}
namespace coop::players {
constexpr uint8_t kPeerIdHost=0;
constexpr uint8_t kPeerIdUnknown=255;
struct Registry {void* local=&test::original;coop::RemotePlayer* puppets[4]{};
 static Registry& Get(){static Registry r;return r;}void* Local(){return local;}
 coop::RemotePlayer* Puppet(uint8_t slot){return slot<4?puppets[slot]:nullptr;}
 uint8_t PeerIdOfActor(void* actor){if(local==actor)return 0;for(uint8_t s=1;s<4;++s)if(puppets[s]&&puppets[s]->actor==actor)return s;return 255;}
 coop::element::Player* GetPlayerElement(uint8_t slot){static coop::element::Player player;return slot==1&&test::hasPlayer?&player:nullptr;}
};
}
namespace ue_wrap::vitals {
enum class Field {Dead};
bool Read(Field field,float* out){assert(field==Field::Dead);*out=test::dead?1.f:0.f;test::Callback(test::onVitals);return test::vitalsReady;}
}
namespace coop::wisp_attack_sync {
namespace R=ue_wrap::reflection;namespace E=ue_wrap::engine;
// The row, limits and both production control bodies are extracted unchanged.
// The globals and engine/session boundaries are fixture-owned.
#include "wisp_host_types.inc"
std::vector<PendingDestroy> g_pendingDestroy;
std::unordered_map<uint32_t,NpcWatch> g_npcKillWatch;
std::unordered_map<uint32_t,Aggro> g_aggro;
std::unordered_map<uint32_t,Closing> g_closing;
std::unordered_map<uint32_t,ue_wrap::CachedObjRef> g_wispBodies;
std::unordered_map<uint32_t,bool> g_lastNativeGrab;
std::unordered_set<uint32_t> g_relayed;
uint64_t g_pendingEpoch=0,g_nextPendingToken=0;
uint32_t g_hostWorld=0;bool g_pendingTicking=false;
std::atomic<coop::net::Session*> g_session{nullptr};
uint64_t NowMs(){return test::now;}
void OnDisconnect();
#include "wisp_host_pending.inc"
void OnDisconnect(){++g_pendingEpoch;g_pendingDestroy.clear();g_npcKillWatch.clear();g_wispBodies.clear();
 g_aggro.clear();g_closing.clear();g_relayed.clear();g_lastNativeGrab.clear();g_hostWorld=0;test::Callback(test::onClear);}
}
int main() {
 namespace H=coop::wisp_attack_sync;using namespace test;
 coop::net::Session session,other;
 auto& registry=coop::element::Registry::Get();
 auto reset=[&]{H::OnDisconnect();world=1;now=1000;H::g_hostWorld=world;H::g_session=&session;
  session.active=session.ready=true;session.generation=1;session.side=coop::net::Role::Host;
  live[0]=live[1]=killer[0]=killer[1]=true;reads=moves=destroys=npcDestroyed=0;readable=true;
  dead=false;vitalsReady=mapReady=true;location={10,20,30};onRead=onMove=onDestroy=onClear=onNpcDestroy=onVitals={};
  hasPlayer=sendGrab=true;grabAttempts=grabPackets=tearPackets=localTears=0;onVictimSend=onBroadcast=onLocalTear={};
  registry.rows[0].Bind(&original);registry.rows[1].Bind(&replacement);};
 auto add=[&](void* actor=&original,uint32_t eid=100){H::PendingDestroy p;p.eid=eid;p.token=++H::g_nextPendingToken;
  p.world=world;p.actor.Set(actor);p.deadlineMs=now+1000;p.lastLiftMs=now-100;H::g_pendingDestroy.push_back(p);};
 reset();add();H::DischargePendingDestroys();assert(moves==1&&H::g_pendingDestroy[0].lastLiftMs==now);
 H::DischargePendingDestroys();assert(moves==1);now+=1000;H::DischargePendingDestroys();assert(destroys==1&&H::g_pendingDestroy.empty());
 H::DischargePendingDestroys();assert(destroys==1);
 for(int change=0;change<4;++change){reset();add();
  if(change==0)registry.rows[0].Bind(&replacement);if(change==1)++serial[0];if(change==2)++world;if(change==3)killer[0]=false;
  H::DischargePendingDestroys();assert(reads==0&&moves==0&&destroys==0&&H::g_pendingDestroy.empty());}
 for(int change=0;change<4;++change){reset();add();onRead=[&,change]{
  if(change==0)H::OnDisconnect();if(change==1)registry.rows[0].Bind(&replacement);if(change==2)++world;if(change==3)++serial[0];};
  H::DischargePendingDestroys();assert(reads==1&&moves==0&&destroys==0&&H::g_pendingDestroy.empty());}
 reset();add();onMove=[]{H::DischargePendingDestroys();};H::DischargePendingDestroys();assert(moves==1);
 reset();add();onMove=[&]{H::OnDisconnect();registry.rows[0].Bind(&replacement);add(&replacement);};
 H::DischargePendingDestroys();assert(moves==1&&H::g_pendingDestroy.size()==1&&H::g_pendingDestroy[0].actor.Get()==&replacement);
 reset();add();H::g_pendingDestroy[0].deadlineMs=now;onDestroy=[&]{H::OnDisconnect();registry.rows[0].Bind(&replacement);add(&replacement);};
 H::DischargePendingDestroys();assert(destroys==1&&H::g_pendingDestroy.size()==1&&H::g_pendingDestroy[0].actor.Get()==&replacement);
 reset();add();H::g_pendingDestroy[0].lastLiftMs=now+1;H::DischargePendingDestroys();assert(reads==0&&moves==0);
 reset();add();location.Z=std::numeric_limits<float>::infinity();H::DischargePendingDestroys();assert(reads==1&&moves==0);
 reset();add();readable=false;H::DischargePendingDestroys();assert(reads==1&&moves==0);
 puts("PASS actual host lift/destruction: original actor/serial/world, eid reuse, native callback cleanup, recursive tick refusal, consume-before-destroy and finite/backward-clock guards");
 reset();add();++world;assert(H::RefreshHostWorld(&session));assert(H::g_pendingDestroy.empty()&&H::g_hostWorld==world);
 for(int change=0;change<3;++change){reset();add();++world;onClear=[&,change]{
  if(change==0)H::g_session=&other;if(change==1)++world;if(change==2)H::OnDisconnect();};
  assert(!H::RefreshHostWorld(&session)&&H::g_hostWorld==0);}
 puts("PASS actual host world scope: old queues retire before adoption, callback session/world/newer cleanup prevents stale scope publication");
 reset();H::EnrollNpcKillWatch(100,&original);const auto token=H::g_npcKillWatch.at(100).token;
 now+=100;H::EnrollNpcKillWatch(100,&original);assert(H::g_npcKillWatch.at(100).token==token);
 H::DischargeNpcKillWatch();assert(npcDestroyed==0);live[0]=false;H::DischargeNpcKillWatch();
 assert(npcDestroyed==1&&H::g_npcKillWatch.empty());H::DischargeNpcKillWatch();assert(npcDestroyed==1);
 for(int change=0;change<4;++change){reset();H::EnrollNpcKillWatch(100,&original);
  if(change==0)registry.rows[0].Bind(&replacement);
  if(change==1){++serial[0];registry.rows[0].Bind(&original);}
  if(change==2)++world;if(change==3)now+=6000;
  H::DischargeNpcKillWatch();assert(npcDestroyed==0&&H::g_npcKillWatch.empty());}
 reset();H::EnrollNpcKillWatch(100,&original);live[0]=false;
 onNpcDestroy=[&]{H::OnDisconnect();registry.rows[0].Bind(&replacement);H::EnrollNpcKillWatch(100,&replacement);};
 H::DischargeNpcKillWatch();assert(npcDestroyed==1&&H::g_npcKillWatch.size()==1&&H::g_npcKillWatch.at(100).actor.Get()==&replacement);
 reset();H::EnrollNpcKillWatch(100,&original);live[0]=false;
 onNpcDestroy=[&]{registry.rows[0].Bind(&replacement);H::EnrollNpcKillWatch(100,&replacement);};
 H::DischargeNpcKillWatch();assert(npcDestroyed==1&&H::g_npcKillWatch.size()==1&&H::g_npcKillWatch.at(100).actor.Get()==&replacement);
 puts("PASS actual NPC death watch: original id/body/world, same-address/id reuse, exact refresh/expiry and consume-before-notify preserve a reentrant newer watch");
 reset();H::EnrollNpcKillWatch(100,&original);live[0]=false;mapReady=false;
 H::DischargeNpcKillWatch();assert(npcDestroyed==0&&H::g_npcKillWatch.empty());
 reset();H::ObserveWispBody(100,&original);
 const auto seed=[] {H::g_aggro.try_emplace(100);H::g_closing.try_emplace(100);H::g_relayed.insert(100);H::g_lastNativeGrab[100]=true;};
 seed();H::ObserveWispBody(100,&original);assert(H::g_aggro.size()==1&&H::g_closing.size()==1&&H::g_relayed.size()==1);
 ++serial[0];H::ObserveWispBody(100,&original);assert(H::g_aggro.empty()&&H::g_closing.empty()&&H::g_relayed.empty()&&H::g_lastNativeGrab.empty());
 seed();H::ObserveWispBody(100,&replacement);assert(H::g_aggro.empty()&&H::g_closing.empty()&&H::g_relayed.empty());
 auto& players=coop::players::Registry::Get();players.local=&original;
 coop::RemotePlayer puppet;puppet.actor=&replacement;players.puppets[1]=&puppet;
 assert(H::PlayerAlive(0,&original)&&H::PlayerAlive(1,&replacement));
 dead=true;assert(!H::PlayerAlive(0,&original));dead=false;
 puppet.health=0;assert(!H::PlayerAlive(1,&replacement));puppet.health=std::numeric_limits<float>::quiet_NaN();assert(!H::PlayerAlive(1,&replacement));puppet.health=1;
 assert(!H::PlayerAlive(1,&original)&&!H::PlayerAlive(255,&replacement));
 onVitals=[]{++world;};assert(!H::PlayerAlive(0,&original));
 onVitals=[]{++serial[0];};assert(!H::PlayerAlive(0,&original));
 onVitals=[&]{players.local=&replacement;};assert(!H::PlayerAlive(0,&original));
 puts("PASS actual host body/target admission: recycled wisp ids clear old target/relay state; dead, replaced, nonfinite-health and callback-retired players are ineligible");
 players.local=&original;puppet.actor=&replacement;puppet.health=1;players.puppets[1]=&puppet;
 reset();session.ready=false;assert(!H::RelayGrab(&session,&original,100,&replacement)&&grabAttempts==0);
 reset();hasPlayer=false;assert(!H::RelayGrab(&session,&original,100,&replacement)&&grabAttempts==0);
 reset();sendGrab=false;assert(!H::RelayGrab(&session,&original,100,&replacement)&&grabAttempts==1&&tearPackets==0&&localTears==0&&H::g_pendingDestroy.empty());
 reset();assert(H::RelayGrab(&session,&original,100,&replacement)&&grabPackets==1&&tearPackets==1&&localTears==1&&H::g_pendingDestroy.size()==1);
 for(int change=0;change<4;++change){reset();onVictimSend=[&,change]{
  if(change==0)H::OnDisconnect();if(change==1)++session.generation;if(change==2)++serial[1];if(change==3)session.side=coop::net::Role::Client;};
  assert(!H::RelayGrab(&session,&original,100,&replacement)&&grabPackets==1&&tearPackets==0&&localTears==0&&H::g_pendingDestroy.empty());}
 reset();onBroadcast=[]{H::OnDisconnect();};assert(!H::RelayGrab(&session,&original,100,&replacement)&&localTears==0&&H::g_pendingDestroy.empty());
 reset();onLocalTear=[]{H::OnDisconnect();};assert(!H::RelayGrab(&session,&original,100,&replacement)&&localTears==1&&H::g_pendingDestroy.empty());
 reset();onLocalTear=[]{test::now+=1500;};assert(H::RelayGrab(&session,&original,100,&replacement));
 assert(H::g_pendingDestroy[0].deadlineMs==4500&&H::g_pendingDestroy[0].lastLiftMs==1000);
 puts("PASS actual host relay: refused/unready victim admission has no tear/kill side effects; callback recipient/body/scope changes abort and cosmetic wait cannot extend the committed deadline");
}
