#include <cstdlib>
#include "../../src/votv-coop/src/coop/interactables/desk_visual_sync.cpp"
#include <cstdio>
#include <functional>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace V=coop::desk_visual_sync;
namespace CP=ue_wrap::coords_panel;
namespace test {
struct Obj {int id,serial=1;bool live=true;} widget{1},material{2},replacement{3};
void* currentWidget=&widget;void* currentMaterial=&material;
uint32_t world=1;
bool held=false,ready=true,writeAim=true,writeProgress=true;
uint8_t holder=0xFF,pinger=0xFF;
int aims=0,progress=0;
CP::DishAim aim;CP::PingVisual visual;
std::function<void()> callback;
void Callback(){auto cb=std::move(callback);callback={};if(cb)cb();}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i==1?test::widget.serial:i==2?test::material.serial:test::replacement.serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* a,int32_t i){return a && static_cast<test::Obj*>(a)->live && InternalIndexOf(a)==i;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace coop::device_occupancy {bool LocalHolds(const wchar_t*){return test::held;}uint8_t HolderOf(const wchar_t*){return test::holder;}}
namespace coop::desk_input_sync {uint8_t PingActiveSlot(){return test::pinger;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::coords_panel {
void* Instance(){return test::ready?test::currentWidget:nullptr;}
void* PresentationTarget(){return test::ready?test::currentMaterial:nullptr;}
bool ReadDishAim(DishAim& a){a=test::aim;return test::ready;}
bool ReadPingVisual(PingVisual& v){v=test::visual;return test::ready;}
bool WriteDishCommitted(const DishAim& a){++test::aims;test::Callback();if(!test::writeAim)return false;test::aim=a;return true;}
bool WritePingVisual(const PingVisual& v){++test::progress;test::Callback();if(!test::writeProgress)return false;test::visual=v;return true;}
}
using namespace test;
coop::net::Session session;
void Reset(coop::net::Role role=coop::net::Role::Host){
 V::OnDisconnect();++world;session={};session.mode=role;held=false;ready=writeAim=writeProgress=true;
 holder=pinger=0xFF;aims=progress=0;aim={};aim.c0X=10;aim.c1X=20;aim.c2Y=20;visual={};callback={};
 widget={1};material={2};replacement={3};currentWidget=&widget;currentMaterial=&material;V::Install(&session);
}
void Tick(){V::g_next=0;V::Tick();}
coop::net::DeskVisualPayload Sample(uint8_t owner=1){coop::net::DeskVisualPayload p{};
 p.aim.c0X=100;p.aim.c1X=200;p.aim.c2Y=200;p.outer=.5f;p.inner=.25f;p.ping=.1f;p.active=1;p.owner=owner;return p;}
int main(){using coop::net::Role;
 Reset();Tick();CHECK(session.sent.size()==1 && V::g_have && V::g_state.owner==0);
 Tick();CHECK(session.sent.size()==1);V::QueueConnect(2);Tick();CHECK(session.sent.back().slot==2);
 Reset();holder=1;auto p=Sample();V::OnState(p,1);CHECK(session.sent.size()==1 && V::g_state.op==1);
 Tick();CHECK(aims==1 && progress==1 && visual.outer==.5f);Tick();CHECK(aims==1 && progress==2);
 V::QueueConnect(2);Tick();CHECK(session.sent.back().slot==2 && session.sent.back().state.owner==1);
 for(int bad=0;bad<6;++bad){Reset();holder=1;p=Sample();
  if(bad==0)p.op=1;if(bad==1)p.owner=2;if(bad==2)p.outer=2;if(bad==3)p.aim.c0X=std::numeric_limits<float>::quiet_NaN();
  if(bad==4)p.aim.selected=4;if(bad==5)holder=2;
  V::OnState(p,1);CHECK(!V::g_have && session.sent.empty());}
 Reset();holder=1;p=Sample();V::OnState(p,1);holder=0xFF;p.active=0;p.outer=1;V::OnState(p,1);CHECK(V::g_state.outer==1 && !V::g_state.active);
 p.active=1;V::OnState(p,1);CHECK(!V::g_state.active); // cannot restart an unclaimed ended run
 Reset();holder=1;p=Sample();V::OnState(p,1);V::g_lastInput=V::NowMs()-4000;Tick();CHECK(!V::g_state.active && V::g_state.outer==0 && V::g_state.owner==0xFF);
 Reset();holder=1;V::OnState(Sample(),1);V::OnPeerLeft(1);CHECK(!V::g_state.active && V::g_state.owner==0xFF);
 Reset();held=true;holder=0;visual.active=true;Tick();held=false;holder=0xFF;visual.outer=.8f;Tick();CHECK(V::g_state.outer==.8f);visual.active=false;Tick();CHECK(!V::g_tail && !V::g_state.active);
 Reset();held=true;holder=1;pinger=1;visual.active=true;Tick();CHECK(!V::g_have); // host verdict must not become host-origin animation
 Reset(Role::Client);p=Sample(0);p.op=1;p.revision=3;ready=false;V::OnState(p,0);Tick();CHECK(aims==0);ready=true;Tick();CHECK(aims==1 && progress==1);
 auto old=p;old.revision=2;old.outer=.9f;V::OnState(old,0);CHECK(V::g_state.outer==.5f);
 p.revision=4;V::OnState(p,2);CHECK(V::g_state.revision==3);
 currentWidget=&replacement;Tick();CHECK(aims==2);++replacement.serial;Tick();CHECK(aims==3);++material.serial;Tick();CHECK(aims==4);
 Reset(Role::Client);p=Sample(0);p.op=1;p.revision=1;V::OnState(p,0);writeAim=false;Tick();CHECK(!V::g_haveApplied && progress==0);writeAim=true;Tick();CHECK(V::g_haveApplied);
 Reset(Role::Client);held=true;holder=1;Tick();CHECK(session.sent.size()==1 && session.sent[0].state.op==0);Tick();CHECK(session.sent.size()==1);
 V::g_lastSend=V::NowMs()-1500;Tick();CHECK(session.sent.size()==2);
 Reset(Role::Client);held=true;holder=1;session.sendOk=false;Tick();CHECK(!V::g_haveSent);session.sendOk=true;Tick();CHECK(V::g_haveSent);
 // Source stays local while held; after final echo it can restore its rebuilt UI.
 auto own=V::g_sent;own.op=1;own.revision=1;V::OnState(own,0);Tick();CHECK(aims==0);held=false;holder=0xFF;currentWidget=&replacement;Tick();CHECK(aims==1);
 for(int change=0;change<5;++change){Reset(Role::Client);p=Sample(0);p.op=1;p.revision=1;V::OnState(p,0);
  callback=[change]{if(change==0)V::OnDisconnect();if(change==1)++world;if(change==2)++widget.serial;if(change==3)currentWidget=&replacement;if(change==4){auto newer=V::g_state;newer.revision++;newer.outer=.9f;V::OnState(newer,0);}};
  Tick();CHECK(!V::g_haveApplied && progress==0);}
 Reset(Role::Client);p=Sample(0);p.op=1;p.revision=1;V::OnState(p,0);callback=[]{Tick();};Tick();CHECK(aims==1 && progress==1);
 Reset(Role::Client);V::OnState(p,0);callback=[p]{V::OnDisconnect();V::Install(&session);V::OnState(p,0);};Tick();CHECK(!V::g_haveApplied && progress==0);
 Reset(Role::Client);V::OnState(p,0);++world;Tick();CHECK(!V::g_have && aims==0);
 Reset(Role::Client);V::OnState(p,0);Tick();aim.c0X=-90;Tick();CHECK(aims==2 && aim.c0X==100);Tick();CHECK(aims==2);
 Reset();holder=1;p=Sample();p.aim.selected=3;V::OnState(p,1);CHECK(V::g_have && V::g_state.aim.selected==3);
 puts("PASS: actual desk visual controller; authority, tail/end, timeout/departure, seeds, readiness, widget identity, retry, echo, reset and reentry");
}
