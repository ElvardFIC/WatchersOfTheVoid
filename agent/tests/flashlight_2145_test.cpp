#include "support_2135.h"
#include "nonentity_facts_support.h"
#include <limits>
#include "../../src/votv-coop/src/coop/player/item_activate.cpp"
namespace test {bool haveLight=true,settersReady=true;int applies=0,cones=0;std::function<void()> onSnapshot;ue_wrap::engine::MainPlayerFlashlightState lightState{true,true,false,0};ue_wrap::engine::FlashlightSnapshot light{5,45,20,true};}
namespace ue_wrap::engine {
void* GetController(void* a){return a;}void* GetMainPlayerLightR(void* a){return test::haveLight?a:nullptr;}
bool ReadMainPlayerFlashlightState(void*,MainPlayerFlashlightState& out){out=test::lightState;return true;}
bool ReadFlashlightSnapshot(void*,FlashlightSnapshot& out){out=test::light;auto cb=std::move(test::onSnapshot);test::onSnapshot={};if(cb)cb();return true;}
bool SetSceneComponentVisibility(void*,bool,bool){return test::settersReady;}bool SetLightIntensity(void*,float){++test::applies;test::Callback();return test::settersReady;}
bool SetSpotLightOuterConeAngle(void*,float){++test::cones;return true;}bool SetSpotLightInnerConeAngle(void*,float){++test::cones;return true;}
}
namespace coop::flashlight_click_sound {void PlayIfStateChanged(void*,uint8_t,bool){}}
namespace test {uint32_t sessionSerial=1;std::array<coop::roster_ledger::Row,coop::players::kMaxPeers> people;}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace coop::roster_ledger {const Row& Get(int slot){return test::people.at(slot);}}
int main(){namespace S=coop::item_activate;using namespace test;coop::net::Session session;
 coop::players::Registry::Get().local=&actor;S::g_session=&session;S::g_flashlightClassHash=42;
 auto poll=[] {S::g_nextStatePoll=0;S::TickConnect();};
 poll();CHECK(session.sent.size()==1);poll();CHECK(session.sent.size()==1);
 lightState.flashlight=false;light.visible=false;session.sendOk=false;poll();CHECK(session.sent.size()==1);
 session.sendOk=true;poll();CHECK(session.sent.size()==2);coop::net::ItemActivatePayload p{};std::memcpy(&p,session.sent.back().data.data(),sizeof(p));CHECK(!p.state);
 lightState.flashlight=true;light.visible=true;poll();CHECK(session.sent.size()==3);
 lightState.hasFlashlight=false;poll();std::memcpy(&p,session.sent.back().data.data(),sizeof(p));CHECK(!p.state&&session.sent.size()==4);
 poll();CHECK(session.sent.size()==4);++world;poll();CHECK(session.sent.size()==5);
 S::OnDisconnect();CHECK(S::g_lastSentSig==S::kNoSendYet&&S::g_session.load()==nullptr);
 S::g_flashlightClassHash=0;S::Install(&session);
 CHECK(S::g_flashlightClassHash==S::HashClassName(L"prop_equipment_flashlight_C"));
 lightState.hasFlashlight=true;poll();CHECK(session.sent.size()==6);
 session.sendOk=false;S::QueueConnectBroadcastForSlot(2);CHECK(S::g_lastSentSig==S::kNoSendYet);
 session.sendOk=true;poll();CHECK(session.sent.size()==7);
 p={};p.itemClassHash=S::g_flashlightClassHash;p.state=1;p.intensity=5;p.outerConeAngle=45;p.innerConeAngle=20;
 coop::players::Registry::Get().puppet.actor=&ui;
 haveLight=false;S::ApplyToPuppetOrDefer(0,&ui,p);CHECK(S::g_pendingApplyValid[0]);
 S::TickConnect();CHECK(S::g_pendingApplyValid[0]);
 haveLight=true;settersReady=false;S::TickConnect();CHECK(S::g_pendingApplyValid[0]);
 settersReady=true;S::TickConnect();CHECK(!S::g_pendingApplyValid[0]&&applies==2);
 haveLight=false;S::ApplyToPuppetOrDefer(0,&ui,p);p.state=0;S::ApplyToPuppetOrDefer(0,&ui,p);
 CHECK(S::g_pendingApplyValid[0]&&!S::g_pendingApplyPayload[0].state);
 S::OnDisconnectForSlot(0);CHECK(!S::g_pendingApplyValid[0]);
 haveLight=true;p.state=1;callback=[&]{auto newer=p;newer.state=0;S::ApplyToPuppetOrDefer(0,nullptr,newer);};
 S::ApplyToPuppetOrDefer(0,&ui,p);CHECK(S::g_pendingApplyValid[0]&&!S::g_pendingApplyPayload[0].state);
 S::TickConnect();CHECK(!S::g_pendingApplyValid[0]);
 callback=[]{++world;};S::ApplyToPuppetOrDefer(0,&ui,p);
 CHECK(S::g_pendingApplyValid[0]&&!S::g_echoSuppress);S::OnDisconnect();
 CHECK(!S::g_pendingApplyValid[0]);
 // A controller on a foreign pawn is not the local sender.
 S::Install(&session);auto before=session.sent.size();S::OnUpdateFlashlightPost(&ui,nullptr,nullptr);CHECK(session.sent.size()==before);
 coop::net::ItemActivatePayload sample{};onSnapshot=[]{++actor.serial;};
 CHECK(!S::BuildPayloadFromLocal(&actor,sample,&session));
 // Reject non-finite/invalid light samples on both wire directions, before caching.
 light.intensity=std::numeric_limits<float>::quiet_NaN();CHECK(!S::BuildPayloadFromLocal(&actor,sample,&session));light.intensity=5;
 p.itemClassHash=S::g_flashlightClassHash;p.state=1;p.intensity=5;p.outerConeAngle=45;p.innerConeAngle=20;
 auto bad=p;bad.innerConeAngle=50;S::ApplyToPuppetOrDefer(0,&ui,bad);CHECK(!S::g_pendingApplyValid[0]);
 bad=p;bad.intensity=std::numeric_limits<float>::infinity();CHECK(!S::ApplyToPuppet(&ui,bad,0));
 // A new state interrupts the remaining setters of the previous state.
 const auto previousCones=cones;callback=[&]{auto newer=p;newer.state=0;S::ApplyToPuppetOrDefer(0,nullptr,newer);};
 S::ApplyToPuppetOrDefer(0,&ui,p);CHECK(cones==previousCones&&S::g_pendingApplyValid[0]);S::TickConnect();
 // Nested applications preserve the enclosing echo-suppression scope.
 callback=[&]{CHECK(S::g_echoSuppress);auto off=p;off.state=0;CHECK(S::ApplyToPuppet(&ui,off,0));CHECK(S::g_echoSuppress);};
 CHECK(S::ApplyToPuppet(&ui,p,0));CHECK(!S::g_echoSuppress);
 // Recreating the local pawn at the same address republishes unchanged light state.
 poll();before=session.sent.size();++actor.serial;poll();CHECK(session.sent.size()==before+1);
 puts("PASS flashlight local ownership, coherent snapshots, finite ranges, newer-state interruption, nested echo and pawn recreation");
 puts("PASS flashlight install before player-class readiness and deferred component/setter apply, latest-state and departure");
 puts("PASS actual flashlight battery-empty edge without input, send retry, dedupe, unequip and world/disconnect reset");
}
namespace ue_wrap::reflection {void* FindClass(const wchar_t*){return nullptr;}void* FindFunction(void*,const wchar_t*){std::abort();}}
namespace ue_wrap::engine {bool WriteMainPlayerFlashlight(void*,bool){std::abort();}}
namespace ue_wrap::game_thread {bool IsGameThread(){return true;}void Post(Task){std::abort();}bool RegisterPostObserver(const facts::Resolved&,ProcessEventObserverFn){std::abort();}}
