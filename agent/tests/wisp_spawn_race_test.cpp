#include "../../src/votv-coop/src/coop/creatures/wisp_tear_mirror.cpp"
#include <cassert>
#include <cstdio>
namespace test {
int wisp=1,successor=2;int forces=0,plays=0,holds=0,deaths=0,releases=0,serial=1,releaseAction=0;
bool disconnectOnPlay=false,animationReady=true,live=true;
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return test::live&&(p==&test::wisp||p==&test::successor);}
int32_t InternalIndexOf(void*){return 1;}
bool IsLiveByIndex(void* p,int32_t i){return i==1&&IsLive(p);}
int32_t SlotSerial(int32_t){return test::serial;}
int32_t AllocateSlotSerial(int32_t){return test::serial;}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::wisp {
bool IsKillerWisp(void* p){return p==&test::wisp;}
bool ForceMeshTick(void*){++test::forces;return true;}
bool PlayFatalityMontage(void*){++test::plays;if(test::disconnectOnPlay)coop::wisp_tear_mirror::OnDisconnect();return test::animationReady;}
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
}
}
int main(){namespace T=coop::wisp_tear_mirror;using namespace test;
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
 puts("PASS wisp spawn/animation readiness retries, backoff, deadline, duplicate queue, sender guards and teardown");
 puts("PASS wisp death cancellation on release-time disconnect/pawn replacement/recycling, one-shot kill and absent victim cleanup");
 puts("PASS wisp death binds victim at receipt: pre-deadline replacement/recycling, absent pawn and invalid grab rejection");
}
