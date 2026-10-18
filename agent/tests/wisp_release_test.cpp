#include "../../src/votv-coop/src/coop/creatures/wisp_grab_hold.cpp"
#include <cassert>
#include <cstdio>
namespace test {int player=1,successor=2,wisp=3,releases=0,serial=1,attachAction=0,attaches=0;
int socketAction=0,moveAction=0,moves=0,wispSerial=1;bool reenter=false,rearm=false;}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p==&test::player||p==&test::successor||p==&test::wisp;}
int32_t InternalIndexOf(void* p){return p==&test::player?1:p==&test::wisp?3:2;}
bool IsLiveByIndex(void* p,int32_t i){return (i==1&&p==&test::player)||(i==2&&p==&test::successor)||(i==3&&p==&test::wisp);}
int32_t SlotSerial(int32_t i){return i==1?test::serial:i==3?test::wispSerial:1;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::wisp {
bool IsKillerWisp(void*){return true;}
bool ApplyGrabToLocalPlayer(void*,void*){
 ++test::attaches;
 const int action=test::attachAction;test::attachAction=0;
 if(action==1)coop::wisp_grab_hold::OnDisconnect();
 if(action==2)coop::wisp_grab_hold::EngageSelf(100,3500);
 if(action==3)coop::players::Registry::Get().local=&test::successor;
 if(action==4)++test::serial;
 if(action==5){coop::element::Registry::Get().ready=false;coop::wisp_grab_hold::EngageSelf(100,3500);}
 return true;
}
bool GrabSocketWorldLocation(void*,FVector& out){
 const int action=test::socketAction;test::socketAction=0;
 if(action==1)coop::wisp_grab_hold::OnDisconnect();
 if(action==2)coop::wisp_grab_hold::OnPeerLeft(2);
 if(action==3){coop::wisp_grab_hold::OnPeerLeft(2);coop::wisp_grab_hold::EngagePuppet(100,2);}
 if(action==4)++test::serial;
 if(action==5)++test::wispSerial;
 if(action==6)coop::players::Registry::Get().Puppet(2)->actor=&test::successor;
 out={1,2,3};return true;
}
bool ReleaseGrabOnLocalPlayer(void* p){
 assert(p==&test::player);++test::releases;
 assert(!coop::wisp_grab_hold::g_selfWispEid&&!coop::wisp_grab_hold::g_selfAttached);
 if(test::reenter)coop::wisp_grab_hold::OnDisconnect();
 if(test::rearm){ // New hold installed by a callback must survive the outer release.
  coop::wisp_grab_hold::g_selfWispEid=200;
  coop::wisp_grab_hold::g_selfRetryUntil=123;
 }
 return true;
}
}
namespace ue_wrap::engine {bool SetActorLocation(void* p,const FVector& location){
 assert(p==&test::player&&location.X==1&&location.Y==2&&location.Z==3);++test::moves;
 const int action=test::moveAction;test::moveAction=0;
 if(action==1)coop::wisp_grab_hold::OnDisconnect();
 if(action==2)coop::wisp_grab_hold::EngagePuppet(100,3);
 return true;
}}
int main(){namespace H=coop::wisp_grab_hold;using namespace test;
 auto& players=coop::players::Registry::Get();players.local=&player;
 auto& registry=coop::element::Registry::Get();registry.ready=true;registry.element.actor=&wisp;
 H::EngageSelf(100,3500);assert(H::g_selfAttached);
 reenter=true;H::ReleaseSelf();assert(releases==1&&!H::g_selfWispEid&&!H::g_selfRetryUntil);
 H::ReleaseSelf();assert(releases==1);
 reenter=false;rearm=true;H::EngageSelf(100,3500);
 H::ReleaseSelf();assert(releases==2&&H::g_selfWispEid==200&&H::g_selfRetryUntil==123);
 rearm=false;H::ReleaseSelf();assert(releases==2&&!H::g_selfWispEid&&!H::g_selfRetryUntil);
 H::EngageSelf(100,3500);players.local=nullptr;
 H::ReleaseSelf();assert(releases==3&&!H::g_selfAttached&&!H::g_selfWispEid);
 players.local=&player;H::EngageSelf(100,3500);players.local=&successor;
 H::ReleaseSelf();assert(releases==4); // Cleanup targets original pawn, never successor.
 players.local=&player;H::EngageSelf(100,3500);++serial;
 H::ReleaseSelf();assert(releases==4); // Recycled original slot is rejected.
 attachAction=1;H::EngageSelf(100,3500);assert(!H::g_selfAttached&&!H::g_selfWispEid);
 attachAction=2;H::EngageSelf(100,3500);assert(H::g_selfAttached&&H::g_selfWispEid==100);
 H::ReleaseSelf();assert(releases==5);
 attachAction=5;H::EngageSelf(100,3500);assert(!H::g_selfAttached&&H::g_selfWispEid==100);
 H::ReleaseSelf();assert(releases==5);registry.ready=true;
 attachAction=3;H::EngageSelf(100,3500);assert(!H::g_selfAttached);H::ReleaseSelf();assert(releases==5);
 players.local=&player;attachAction=4;H::EngageSelf(100,3500);assert(!H::g_selfAttached);H::ReleaseSelf();
 // The Tick retry path must observe the same attachment cancellation rules.
 registry.ready=false;H::EngageSelf(100,3500);registry.ready=true;attachAction=1;H::Tick();
 assert(!H::g_selfAttached&&!H::g_selfWispEid&&releases==5);
 registry.ready=false;H::EngageSelf(100,3500);const int previous=attaches;
 players.local=&successor;registry.ready=true;H::Tick();assert(attaches==previous&&!H::g_selfAttached);
 H::ReleaseSelf();players.local=&player;
 registry.ready=false;H::EngageSelf(100,3500);++serial;registry.ready=true;
 H::Tick();assert(attaches==previous&&!H::g_selfAttached);H::ReleaseSelf();
 registry.ready=false;H::EngageSelf(100,3500);registry.ready=true;H::Tick();
 assert(attaches==previous+1&&H::g_selfAttached);H::ReleaseSelf();assert(releases==6);
 puts("PASS actual wisp hold: original victim cleanup, recycled-slot rejection, reentrant release/attach and retry cancellation");
 coop::RemotePlayer puppet;puppet.actor=&player;players.puppets[2]=&puppet;players.puppets[3]=&puppet;
 H::EngagePuppet(100,4);H::EngagePuppet(100,255);assert(H::g_holds.empty());
 H::EngagePuppet(100,2);H::Tick();assert(moves==1);
 for(int action=1;action<=6;++action){
  H::OnDisconnect();puppet.actor=&player;H::EngagePuppet(100,2);socketAction=action;
  H::Tick();assert(moves==1);
 }
 H::OnDisconnect();puppet.actor=&player;H::EngagePuppet(100,2);H::EngagePuppet(100,3);
 moveAction=1;H::Tick();assert(moves==2&&H::g_holds.empty());
 H::EngagePuppet(100,2);moveAction=2;H::Tick();assert(moves==3&&H::g_holds.size()==2);
 H::Tick();assert(moves==5);H::OnDisconnect();
 registry.ready=false;H::EngagePuppet(100,2);H::Tick();assert(H::g_holds.empty());registry.ready=true;
 players.puppets[2]=nullptr;H::EngagePuppet(100,2);H::Tick();assert(H::g_holds.empty());
 puts("PASS puppet holds: socket callback disconnect/leave/re-engage, actor recycling/replacement, move callback teardown/growth and stale hold cleanup");
}
