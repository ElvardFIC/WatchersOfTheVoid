#include "support_2135.h"
#include "../../src/votv-coop/src/coop/props/prop_knock.cpp"
#include <vector>
namespace test {
void* held=nullptr;void* remote=nullptr;void* hand=nullptr;void* fixed=nullptr;void* frozen=nullptr;void* trash=nullptr;
int frameSize=sizeof(void*);int paramOff=0;
std::vector<void*> driven,coasted;
Obj component{4};void* componentOwner=&actor;
Obj nested[8];
Obj player{11},puppetBody{12};
bool Nested(void* a){for(auto& o:nested)if(a==&o)return true;return false;}
bool prop(void* a){return a && a!=&component && a!=&player && a!=&puppetBody && !Nested(a) && static_cast<Obj*>(a)->live;}
}
namespace coop::gravity_motion {void* Target(){return nullptr;}}
namespace ue_wrap::engine {bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& state){state={};state.grabbingActor=test::held;return true;}}
namespace coop::hand_item {bool IsHandAxisActor(void* a){return a && a==test::hand;}}
namespace coop::remote_prop {bool IsActorUnderAnyDrive(void* a){return a && a==test::remote;}}
namespace coop::prop_drive_host {
bool IsDriven(void* a){return std::find(test::driven.begin(),test::driven.end(),a)!=test::driven.end();}
void Coast(void* a,const char*,bool){test::coasted.push_back(a);test::driven.push_back(a);test::Callback();}
}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void* a){return test::prop(a);}
bool IsStatic(void* a){return a==test::fixed;}
bool IsFrozen(void* a){return a==test::frozen;}
bool IsGarbageClump(void* a){return a==test::trash;}
bool IsTrashActor(void*){return false;}
bool IsTrashBitsPile(void*){return false;}
}
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*){return test::paramOff;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
void* OuterOf(void* a){
 for(int i=0;i<8;++i)if(a==&test::nested[i])return i==7?&test::actor:&test::nested[i+1];
 return a==&test::component?test::componentOwner:nullptr;
}
}
int main(){using namespace test;namespace S=coop::prop_knock;
 coop::net::Session session;S::Tick(session);
 CHECK(ue_wrap::script_gate::lastWatchClass==L"comp_physicsImpact_C"&&ue_wrap::script_gate::lastWatchFunction==L"hitCpp");
 Obj third{3};void* other=&ui;ue_wrap::script_gate::Call call{};
 call.object=&component;call.function=&actor;call.locals=reinterpret_cast<uint8_t*>(&other);
 S::OnKnock(call);CHECK(!S::g_count); // ordinary idle contacts are not all streamed
 held=&actor;S::OnKnock(call);S::OnKnock(call);CHECK(S::g_count==1&&coasted.empty());
 S::g_count=0;call.object=&nested[0];S::OnKnock(call);CHECK(S::g_count==1);
 call.object=&component;
 // A queued victim can transfer the same chain before the next network tick.
 call.object=&ui;other=&third;S::OnKnock(call);CHECK(S::g_count==2);
 S::Tick(session);CHECK(coasted.size()==2&&coasted[0]==&ui&&coasted[1]==&third&&!S::g_count);
 driven.clear();coasted.clear();held=nullptr;remote=&actor;call.object=&ui;other=&actor;
 S::OnKnock(call);CHECK(S::g_count==1);S::Tick(session);CHECK(coasted.size()==1&&coasted[0]==&ui);
 driven.clear();coasted.clear();fixed=&ui;S::OnKnock(call);CHECK(!S::g_count);fixed=nullptr;
 frozen=&ui;S::OnKnock(call);CHECK(!S::g_count);frozen=nullptr;
 trash=&ui;S::OnKnock(call);CHECK(!S::g_count);trash=nullptr;
 hand=&actor;S::OnKnock(call);CHECK(!S::g_count);hand=nullptr;
 frameSize=1;S::OnKnock(call);CHECK(!S::g_count);frameSize=sizeof(void*);paramOff=99;S::OnKnock(call);CHECK(!S::g_count);paramOff=0;
 call.fromOurCode=true;S::OnKnock(call);CHECK(!S::g_count);call.fromOurCode=false;
 S::OnKnock(call);held=&ui;S::Tick(session);CHECK(coasted.empty());held=nullptr; // picked up between contact and drain
 S::OnKnock(call);++ui.serial;S::Tick(session);CHECK(coasted.empty()); // recycled object must not inherit motion
 S::OnKnock(call);++world;S::Tick(session);CHECK(coasted.empty()&&!S::g_count);
 session.mode=coop::net::Role::Client;S::OnKnock(call);CHECK(!S::g_count);session.mode=coop::net::Role::Host;
 S::OnKnock(call);callback=[&]{S::OnDisconnect();};S::Tick(session);CHECK(!S::g_session&&!S::g_count&&coasted.size()==1);
 S::Tick(session);driven.clear();S::OnKnock(call);callback=[&]{other=&third;call.object=&ui;S::OnKnock(call);};S::Tick(session);
 CHECK(S::g_count==1); // reentrant contact waits for next drain, never edits the iterated drive vector
 auto before=coasted.size();S::Tick(session);CHECK(coasted.size()==before+1&&coasted.back()==&third);
 S::OnDisconnect();S::Tick(session);driven.clear();coasted.clear();remote=nullptr;
 auto& players=coop::players::Registry::Get();players.local=&player;players.puppet.actor=&puppetBody;
 call.object=&component;other=&player;S::OnKnock(call);S::OnKnock(call);CHECK(S::g_count==1);
 S::Tick(session);CHECK(coasted.size()==1&&coasted.back()==&actor); // host body -> touched prop
 driven.clear();coasted.clear();other=&puppetBody;S::OnKnock(call);S::Tick(session);
 CHECK(coasted.size()==1&&coasted.back()==&actor); // remote body -> touched prop
 driven.clear();coasted.clear();fixed=&actor;S::OnKnock(call);CHECK(!S::g_count);fixed=nullptr;
 frozen=&actor;S::OnKnock(call);CHECK(!S::g_count);frozen=nullptr;
 hand=&actor;S::OnKnock(call);CHECK(!S::g_count);hand=nullptr;
 held=&actor;S::OnKnock(call);CHECK(!S::g_count);held=nullptr;
 puppetBody.live=false;S::OnKnock(call);CHECK(!S::g_count);puppetBody.live=true;
 other=&player;S::OnKnock(call);++actor.serial;S::Tick(session);CHECK(coasted.empty());
 S::OnKnock(call);held=&actor;S::Tick(session);CHECK(coasted.empty());held=nullptr;
 session.mode=coop::net::Role::Client;S::OnKnock(call);CHECK(!S::g_count);
 S::OnDisconnect();
 puts("PASS player-body contact coasting: local/remote, dedupe, dead body, static/frozen/hand/held exclusions, serial retirement and host-only admission");
 puts("PASS host contact feeder: physical local/remote holds, chained push, deferred/dedup, static/frozen/hand exclusions, frame/role, recycled/world/reentrant safety");
}
