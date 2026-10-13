#include "support_2135.h"
#include "../../src/votv-coop/src/coop/player/item_activate.cpp"
namespace test {ue_wrap::engine::MainPlayerFlashlightState lightState{true,true,false,0};ue_wrap::engine::FlashlightSnapshot light{5,45,20,true};}
namespace ue_wrap::engine {
void* GetController(void* a){return a;}void* GetMainPlayerLightR(void* a){return a;}
bool ReadMainPlayerFlashlightState(void*,MainPlayerFlashlightState& out){out=test::lightState;return true;}
bool ReadFlashlightSnapshot(void*,FlashlightSnapshot& out){out=test::light;return true;}
bool SetSceneComponentVisibility(void*,bool,bool){return true;}bool SetLightIntensity(void*,float){return true;}
bool SetSpotLightOuterConeAngle(void*,float){return true;}bool SetSpotLightInnerConeAngle(void*,float){return true;}
}
namespace coop::flashlight_click_sound {void PlayIfStateChanged(void*,uint8_t,bool){}}
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
 puts("PASS actual flashlight battery-empty edge without input, send retry, dedupe, unequip and world/disconnect reset");
}
namespace ue_wrap::reflection {void* FindClass(const wchar_t*){std::abort();}void* FindFunction(void*,const wchar_t*){std::abort();}}
namespace ue_wrap::engine {bool WriteMainPlayerFlashlight(void*,bool){std::abort();}}
namespace ue_wrap::game_thread {void Post(Task){std::abort();}bool RegisterPostObserver(void*,ProcessEventObserverFn){std::abort();}}
