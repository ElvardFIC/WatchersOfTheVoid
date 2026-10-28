#ifndef WISP_TEAR_SOURCE
#define WISP_TEAR_SOURCE "../../src/votv-coop/src/coop/creatures/wisp_tear_mirror.cpp"
#endif
#include WISP_TEAR_SOURCE
#include <cassert>
#include <cstdio>
namespace test {
int wisp=1,successor=2;int forces=0,plays=0,holds=0,deaths=0,releases=0,serial=1,releaseAction=0;
bool disconnectOnPlay=false,animationReady=true,live=true;
uint32_t world=1;bool dead=false;
bool forceReady=true,vitalsReady=true;int readAction=0;
std::function<void()> onForce,onPlay;
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return test::live&&(p==&test::wisp||p==&test::successor);}
int32_t InternalIndexOf(void*){return 1;}
bool IsLiveByIndex(void* p,int32_t i){return i==1&&IsLive(p);}
int32_t SlotSerial(int32_t){return test::serial;}
int32_t AllocateSlotSerial(int32_t){return test::serial;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::vitals {bool Read(Field field,float* out){
 assert(field==Field::Dead);*out=test::dead?1.f:0.f;
 const int action=test::readAction;test::readAction=0;
 if(action==1)coop::wisp_tear_mirror::OnWispGrab({17,100,3500},0);
 return test::vitalsReady;
}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::wisp {
bool IsKillerWisp(void* p){return p==&test::wisp;}
bool ForceMeshTick(void*){++test::forces;auto once=std::move(test::onForce);test::onForce={};if(once)once();return test::forceReady;}
bool PlayFatalityMontage(void*){++test::plays;auto once=std::move(test::onPlay);test::onPlay={};if(once)once();if(test::disconnectOnPlay)coop::wisp_tear_mirror::OnDisconnect();return test::animationReady;}
}
namespace ue_wrap::engine {
bool SetMainPlayerRagdollMode(void* actor,bool ragdoll,bool passOut,bool death){
 assert(actor==&test::wisp&&ragdoll&&!passOut&&death);++test::deaths;return true;
}
}
namespace coop::wisp_grab_hold {
void EngageSelf(uint32_t,uint32_t){}
void EngagePuppet(uint32_t,uint8_t){++test::holds;}
void ReleaseSelf(){
 ++test::releases;const int action=test::releaseAction;test::releaseAction=0;
 if(action==1)coop::wisp_tear_mirror::OnDisconnect();
 if(action==2)coop::players::Registry::Get().local=&test::successor;
 if(action==3)++test::serial;
 if(action==4)coop::wisp_tear_mirror::OnWispGrab({17,100,3500},0);
 if(action==5)test::dead=true;
 if(action==6)++test::world;
 if(action==7)test::readAction=1;
}
}
int main(){namespace T=coop::wisp_tear_mirror;using namespace test;
 coop::RemotePlayer puppet;puppet.actor=&successor;
 coop::players::Registry::Get().puppets[2]=&puppet;
 auto& registry=coop::element::Registry::Get();registry.element.actor=&wisp;
 coop::net::WispTearPayload p{100,2};
 T::OnWispTear(p,2);assert(T::g_pendingTears.empty());
 T::OnWispTear({100,4},0);assert(T::g_pendingTears.empty());
 T::OnWispTear(p,0);T::OnWispTear(p,0);assert(T::g_pendingTears.size()==1&&plays==0);
 T::Tick();assert(T::g_pendingTears.size()==1&&plays==0);
 registry.ready=true;T::g_pendingTears[0].nextTry=0;T::Tick();assert(T::g_pendingTears.empty()&&plays==1&&forces==1&&holds==1);
 registry.ready=false;T::OnWispTear(p,0);T::g_pendingTears[0].until=0;
 registry.ready=true;T::Tick();assert(plays==1&&T::g_pendingTears.empty());
 registry.ready=false;T::OnWispTear(p,0);T::OnDisconnect();assert(T::g_pendingTears.empty());
 T::OnWispTear(p,0);registry.ready=true;disconnectOnPlay=true;T::g_pendingTears[0].nextTry=0;T::Tick();
 assert(plays==2&&holds==1&&T::g_pendingTears.empty()); // No stale hold after reentrant teardown.
 disconnectOnPlay=false;animationReady=false;
 T::OnWispTear(p,0);assert(plays==3&&holds==2&&T::g_pendingTears.size()==1);
 const auto deadline=T::g_pendingTears[0].until;
 T::OnWispTear(p,0);T::Tick();assert(plays==3&&T::g_pendingTears[0].until==deadline);
 T::g_pendingTears[0].nextTry=0;T::Tick();assert(plays==4&&T::g_pendingTears.size()==1);
 animationReady=true;T::g_pendingTears[0].nextTry=0;T::Tick();
 assert(plays==5&&T::g_pendingTears.empty());T::Tick();assert(plays==5); // Success is never replayed by Tick.
 animationReady=false;T::OnWispTear(p,0);assert(plays==6);
 T::g_pendingTears[0].until=0;T::Tick();assert(plays==6&&T::g_pendingTears.empty());
 disconnectOnPlay=true;T::OnWispTear(p,0);
 assert(plays==7&&T::g_pendingTears.empty()); // Failed immediate attempt must not requeue after disconnect.
 disconnectOnPlay=false;auto& players=coop::players::Registry::Get();players.local=&wisp;
 auto arm=[](){T::OnWispGrab({17,100,3500},0);assert(T::g_killAtMs);T::g_killAtMs=T::NowMs()-1;};
 arm();releaseAction=1;T::Tick();assert(deaths==0&&T::g_killAtMs==0&&T::g_killWispEid==0);
 arm();releaseAction=2;T::Tick();assert(deaths==0);players.local=&wisp;
 arm();releaseAction=3;T::Tick();assert(deaths==0);
 arm();T::Tick();assert(deaths==1&&T::g_killAtMs==0&&T::g_killWispEid==0);
 const int released=releases;T::Tick();assert(deaths==1&&releases==released);
 arm();live=false;T::Tick();assert(deaths==1&&releases==released+1);live=true;
 arm();players.local=nullptr;T::Tick();assert(deaths==1&&T::g_killAtMs==0);
 // Replacement/recycling BEFORE the deadline must not retarget the old attack.
 players.local=&wisp;arm();players.local=&successor;T::Tick();assert(deaths==1);
 players.local=&wisp;arm();++serial;T::Tick();assert(deaths==1);
 players.local=nullptr;T::OnWispGrab({17,100,3500},0);assert(!T::g_killAtMs);
 players.local=&wisp;T::OnWispGrab({17,0,3500},0);assert(!T::g_killAtMs);
 T::OnWispGrab({17,100,3500},1);assert(!T::g_killAtMs);
 T::OnWispGrab({18,100,3500},0);assert(!T::g_killAtMs);
 arm();T::OnDisconnect();assert(!T::g_killAtMs&&!T::g_killVictim.Raw());
 // Cancellation also occurs before the deadline, including a same-address
 // pawn and a world transition whose object-array serial has not changed yet.
 T::OnWispGrab({17,100,3500},0);players.local=&successor;T::Tick();assert(!T::g_killAtMs);players.local=&wisp;
 T::OnWispGrab({17,100,3500},0);++serial;T::Tick();assert(!T::g_killAtMs);
 T::OnWispGrab({17,100,3500},0);dead=true;T::Tick();assert(!T::g_killAtMs&&deaths==1);dead=false;
 T::OnWispGrab({17,100,3500},0);++world;T::Tick();assert(!T::g_killAtMs&&deaths==1);
 arm();releaseAction=4;T::Tick();assert(T::g_killAtMs&&deaths==1);T::OnDisconnect();
 animationReady=false;T::OnWispTear(p,0);assert(!T::g_pendingTears.empty());++world;
 T::Tick();assert(T::g_pendingTears.empty());animationReady=true;
 animationReady=false;T::OnWispTear(p,0);++world;T::OnWispTear(p,0);
 assert(T::g_pendingTears.size()==1&&T::g_pendingTears[0].world==world);
 T::OnDisconnect();animationReady=true;
 for(int action:{5,6,7}) {
  const auto before=deaths;arm();releaseAction=action;T::Tick();assert(deaths==before);
  if(action==7)assert(T::g_killAtMs); // The read callback's newer grab survives.
  dead=false;T::OnDisconnect();
 }
 auto queue=[&]{T::OnDisconnect();animationReady=false;T::OnWispTear(p,0);assert(T::g_pendingTears.size()==1);};
 auto retry=[&]{if(!T::g_pendingTears.empty())T::g_pendingTears.front().nextTry=0;T::Tick();};
 queue();auto before=plays;puppet.actor=&wisp;retry();assert(plays==before&&T::g_pendingTears.empty());puppet.actor=&successor;
 queue();before=plays;puppet.health=0;retry();assert(plays==before&&T::g_pendingTears.empty());puppet.health=1;
 queue();before=plays;registry.element.actor=&successor;retry();assert(plays==before&&T::g_pendingTears.empty());registry.element.actor=&wisp;
 queue();before=plays;++serial;retry();assert(plays==before&&T::g_pendingTears.empty());
 queue();before=plays;T::OnPeerLeft(2);retry();assert(plays==before&&T::g_pendingTears.empty());
 T::OnDisconnect();before=plays;forceReady=false;T::OnWispTear(p,0);
 assert(plays==before&&T::g_pendingTears.size()==1);forceReady=true;animationReady=true;retry();assert(plays==before+1&&T::g_pendingTears.empty());
 T::OnDisconnect();before=plays;
 onForce=[&]{puppet.actor=&wisp;};T::OnWispTear(p,0);
 assert(plays==before&&T::g_pendingTears.empty());puppet.actor=&successor;
 T::OnDisconnect();before=plays;const auto held=holds;
 onPlay=[&]{T::OnPeerLeft(2);};T::OnWispTear(p,0);
 assert(plays==before+1&&holds==held&&T::g_pendingTears.empty());
 T::OnDisconnect();before=plays;
 onPlay=[&]{T::OnWispTear(p,0);T::Tick();};T::OnWispTear(p,0);
 assert(plays==before+1&&T::g_pendingTears.empty()); // No recursive playback or duplicate queue.
 T::OnDisconnect();before=plays;
 onPlay=[&]{T::OnDisconnect();T::OnWispTear(p,0);};T::OnWispTear(p,0);
 assert(plays==before+1&&T::g_pendingTears.size()==1);retry();
 assert(plays==before+2&&T::g_pendingTears.empty());
 T::OnDisconnect();players.puppets[2]=nullptr;before=plays;T::OnWispTear(p,0);
 assert(plays==before&&T::g_pendingTears.size()==1);T::OnPeerLeft(2);players.puppets[2]=&puppet;retry();
 assert(plays==before&&T::g_pendingTears.empty());
 T::OnDisconnect();vitalsReady=false;players.local=&wisp;before=plays;
 T::OnWispTear({100,1},0);assert(T::g_pendingTears.size()==1&&plays==before);
 players.local=&successor;retry();assert(T::g_pendingTears.empty()&&plays==before);
 vitalsReady=true;players.local=&wisp;
 T::OnDisconnect();
 puts("PASS wisp spawn/animation readiness retries, backoff, deadline, duplicate queue, sender guards and teardown");
 puts("PASS wisp death cancellation on release-time disconnect/pawn replacement/recycling, one-shot kill and absent victim cleanup");
 puts("PASS wisp death binds victim at receipt: pre-deadline replacement/recycling, absent pawn and invalid grab rejection");
 puts("PASS wisp immediate cancellation: native death/world change, pending tear world expiry and new grab during release");
 puts("PASS pending wisp tears bind victim/wisp bodies, cancel on death/replacement/leave, wait for mesh readiness and reserve requests across native reentry");
 puts("PASS cleanup-time death/world change and a newer grab inside the final vitals read cannot admit the old ragdoll kill");
}
