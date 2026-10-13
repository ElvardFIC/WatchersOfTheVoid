// Production weapon reader + pawn grab-state reader + common ownership query.
#include "../../src/votv-coop/src/ue_wrap/actors/physics_gun.cpp"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/core/reflected_offset.h"
namespace ue_wrap::engine {
namespace R = reflection;
#include "../../_research/tests_2158/grab_state.h"
}
#include "coop/player/local_prop_hold.h"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
namespace test {
struct Obj {int id=0, serial=1; bool live=true; void* cls=nullptr; void* parent=nullptr;
 ue_wrap::reflection::FName name{}; void* world=reinterpret_cast<void*>(1);
 uint8_t flags=0; void* held=nullptr; void* target=nullptr; void* component=nullptr;};
Obj player{0},gun{1},target{2},component{3},base{4},derived{5},foreign{6};
Obj* objects[]={&player,&gun,&target,&component,&base,&derived,&foreign};
void* world=reinterpret_cast<void*>(1);bool missing=false;int nameReads=0;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return static_cast<test::Obj*>(p)->id;}
int32_t SlotSerial(int32_t i){return test::objects[i]->serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return p&&i>=0&&i<7&&test::objects[i]==p&&test::objects[i]->live;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
void* SuperStructOf(void* p){return static_cast<test::Obj*>(p)->parent;}
const FName& NameOf(void* p){++test::nameReads;return static_cast<test::Obj*>(p)->name;}
bool NameEquals(const FName& n,const wchar_t* s){return n.ComparisonIndex==10&&!wcscmp(s,L"prop_physgun_C");}
int32_t FindPropertyOffset(void*,const wchar_t* n){return test::missing?-1:!wcscmp(n,L"objectPickup")?offsetof(test::Obj,target):offsetof(test::Obj,component);}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,flags);mask=4;return !test::missing;}
int32_t InstanceOffset::Of(void*){return -1;} // no gravity component in this pawn
}
namespace ue_wrap::world_identity {
void* CurrentWorld(){return test::world;}
void* WorldOf(void* p){return static_cast<test::Obj*>(p)->world;}
}
namespace ue_wrap::reflected_offset {
int32_t MainPlayer_grabbing_actor(){return -1;}
int32_t MainPlayer_holding_actor(){return offsetof(test::Obj,held);}
int32_t MainPlayer_grabsHeavy(){return -1;}
int32_t MainPlayer_Heavy(){return -1;}
int32_t MainPlayer_grabLen(){return -1;}
}
namespace ue_wrap::prop {bool IsDescendantOfProp(void* p){return p==&test::target;}}
namespace coop::gravity_motion {void* Target(){return nullptr;}}
int main(){
 using namespace test;namespace G=ue_wrap::physics_gun;namespace E=ue_wrap::engine;
 base.name.ComparisonIndex=10;base.world=derived.world=foreign.world=nullptr;
 derived.parent=&base;gun.cls=&derived;gun.target=&target;gun.component=&component;gun.flags=4;
 player.held=&gun;coop::players::Registry::Get().local=&player;
 CHECK(G::HeldProp(&gun)==&target);int n=nameReads;CHECK(G::HeldProp(&gun)==&target&&nameReads==n);
 E::MainPlayerGrabState state{};CHECK(E::ReadMainPlayerGrabState(&player,state));
 CHECK(state.holdingActor==&gun&&state.physicsActor==&target&&!state.gravityActor);
 CHECK(coop::local_prop_hold::IsHeld(&target));
 gun.flags=0;CHECK(!G::HeldProp(&gun));CHECK(!coop::local_prop_hold::IsHeld(&target)); // native release keeps pointer until tick
 gun.flags=4;player.held=nullptr;CHECK(!coop::local_prop_hold::IsHeld(&target));player.held=&gun;
 target.live=false;CHECK(!G::HeldProp(&gun));target.live=true;
 component.live=false;CHECK(!G::HeldProp(&gun));component.live=true;
 target.world=reinterpret_cast<void*>(2);CHECK(!G::HeldProp(&gun));target.world=world;
 gun.target=&gun;CHECK(!G::HeldProp(&gun));gun.target=&target;
 gun.cls=&foreign;CHECK(!G::HeldProp(&gun));gun.cls=&derived;
 CHECK(G::HeldProp(&gun)==&target);
 ++derived.serial;derived.parent=nullptr;CHECK(!G::HeldProp(&gun)); // recycled class address
 ++derived.serial;derived.parent=&base;CHECK(G::HeldProp(&gun)==&target);
 ++derived.serial;missing=true;CHECK(!G::HeldProp(&gun));missing=false;++derived.serial;
 CHECK(G::HeldProp(&gun)==&target);
 world=reinterpret_cast<void*>(2);CHECK(!G::HeldProp(&gun));world=nullptr;CHECK(!G::HeldProp(&gun));
 CHECK(!G::HeldProp(nullptr));
 puts("PASS physics-gun native ancestry/fields, pawn stream input, ownership, release/stow, invalid world/lifetime/metadata");
}
