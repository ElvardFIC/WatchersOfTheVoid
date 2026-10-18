#include "../../src/votv-coop/src/coop/interactables/kitchen_sync.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Actor { int id; bool live=true; int serial=1; };
Actor oven{1},door{2},local{3},puppet{4};
uint32_t world=1;
void* user=nullptr;
bool reach=true,hand=false,announced=true,recordOK=true;
uint16_t occupancy=2;
int grillChanges=0,records=0;
int moves=0,applies=0,opens=0;
ue_wrap::FVector2D mouse{};
ue_wrap::kitchen_machine::State state{};
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a) { return a ? static_cast<test::Actor*>(a)->id : -1; }
int32_t SlotSerial(int32_t i) { return i==1 ? test::oven.serial : 1; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLiveByIndex(void* a,int32_t i) { return a && static_cast<test::Actor*>(a)->live && InternalIndexOf(a)==i; }
bool IsLive(void* a) { return a && static_cast<test::Actor*>(a)->live; }
const FName& NameOf(void*) { static FName n{1,0}; return n; }
bool NameStartsWith(const FName&,const wchar_t*) { return false; }
int32_t ParamOffset::Of(void*) { return 0; }
int32_t FindParamOffset(void*,const wchar_t*) {return 0;}
int32_t FunctionFrameSize(void*) {return 16;}
}
namespace ue_wrap::world_identity {
void* CurrentWorld() { return reinterpret_cast<void*>(uintptr_t(test::world)); }
void* WorldOf(void*) { return CurrentWorld(); }
uint32_t Generation() { return test::world; }
}
namespace ue_wrap::object_index { uint64_t ClassSetVersion() { return 1; } }
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn) { return true; }
bool WatchName(const wchar_t*,int,PreFn,PostFn) { return true; }
void ResolvePendingNames() {}
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::prop { std::wstring GetInteractableKeyString(void*) { return L"oven"; } std::wstring GetKeyString(void*){return L"oven";} }
namespace ue_wrap::kitchen_machine {
Kind Type(void* a) { return a==&test::oven ? test::state.kind : Kind::None; }
bool Read(void* a,State& s) { if (a!=&test::oven || !test::oven.live) return false; s=test::state;return true; }
bool Apply(void*,const State& s) { ++test::applies;test::state=s;return true; }
bool Press(void*,void*,uint8_t,uint8_t) { return true; }
bool SetGrillFire(void*,bool on) {++test::grillChanges;if(on)test::state.flags|=Active;else test::state.flags&=~Active;return true;}
uint8_t ReadControl(void*) { return 0; }
void* DoorParent(void* a) { return a==&test::door ? &test::oven : nullptr; }
bool OwnsDoor(void* a) { return a==&test::oven; }
void* DoorUser(void*) { return test::user; }
bool SetDoor(void*,bool on) { ++test::opens; if (on) test::state.flags|=Opened; else test::state.flags&=~Opened; return true; }
bool MoveDoor(void*,void* player,const FVector2D& d) { if (player!=&test::puppet) return false; ++test::moves;test::mouse=d;return true; }
void Animate(void*,float) {}
}
namespace coop::element::scan_hub { void Register(const Consumer&) {} }
namespace coop::net_pump { bool IsInAnnouncedWorld(void*) { return test::announced; } }
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace coop::prop_save_data {bool Publish(net::Session*,void*,const std::wstring&){++test::records;return test::recordOK;}}
namespace coop::prop_element_tracker {
element::ElementId GetPropElementIdForActor(void*) { return 10; }
void* FindLiveActorByKey(const std::wstring& key) { return key==L"oven" && test::oven.live ? &test::oven : nullptr; }
}
namespace coop::remote_prop {
element::ElementId ResolveMirrorEidByActor(void*,bool) { return 10; }
void* ResolveLiveActorByEid(uint32_t id) { return id==10 && test::oven.live ? &test::oven : nullptr; }
}
namespace coop::roster_ledger {
void RegisterPerSlotClear(PerSlotClearFn,void*) {}
const Row& Get(int i) { static Row r; r.playerNo=i==1?test::occupancy:uint16_t(i+1); return r; }
}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session&,uint8_t,float) { return {}; }
IntentSubject IntentTarget::Authorize(void*) const { return {test::reach ? IntentOutcome::Ok : IntentOutcome::OutOfReach}; }
}
using namespace coop;
using namespace coop::kitchen_sync;
void Reset(net::Session& s,net::Role role) {
    OnDisconnect();++test::world;s=net::Session{};s.mode=role;
    test::oven={1};test::state={};test::state.kind=ue_wrap::kitchen_machine::Kind::Microwave;
    test::state.hasDoorPose=true;test::state.doorPose.TX=12;
    test::moves=test::applies=test::opens=0;test::reach=true;test::user=nullptr;
    test::hand=false;test::announced=true;test::occupancy=2;test::grillChanges=test::records=0;
    test::recordOK=true;
    players::Registry::Get().local=&test::local;players::Registry::Get().puppet.actor=&test::puppet;
    Install(&s);ResetWorld();Match(nullptr,&test::oven);g_actors=g_scratch;
}
int main() {
    net::Session s;Reset(s,net::Role::Client);
    ue_wrap::script_gate::Call call{};call.object=&test::door;call.tag=0x4B540007;
    CHECK(OnDoor(call)==sg::Verdict::Cancel && s.kitchenIntents.empty()); // autonomous client close must not echo
    test::user=&test::local;ue_wrap::FVector2D mouse{4,-2};call.locals=reinterpret_cast<uint8_t*>(&mouse);
    CHECK(OnMouse(call)==sg::Verdict::Cancel);CHECK(test::moves==0);
    OnMouse(call);test::user=nullptr;g_next=0;Tick();
    CHECK(s.kitchenIntents.size()==1 && s.kitchenIntents[0].mouseX==8 && s.kitchenIntents[0].mouseY==-4);
    auto motion=s.kitchenIntents[0]; // final mouse delta survives release before the send tick
    Reset(s,net::Role::Host);
    auto open=motion;open.op=2;open.mouseX=open.mouseY=0;OnIntent(open,1);OnIntent(motion,1);
    CHECK(test::opens==1 && test::moves==1 && test::mouse.X==8); // same-frame open must not rate-limit movement
    OnIntent(motion,2);CHECK(test::moves==1); // another player cannot steal a live hinge drag
    for (auto& row:g_doorOwners) row.second.until=Now()-1;
    OnIntent(motion,2);CHECK(test::moves==2);
    test::user=&test::local;OnIntent(motion,2);CHECK(test::moves==2); // host's local hand wins
    test::user=nullptr;test::reach=false;OnIntent(motion,2);CHECK(test::moves==2);
    test::reach=true;auto bad=motion;bad.mouseX=201;OnIntent(bad,2);CHECK(test::moves==2);
    Publish(&test::oven,-1);CHECK(!s.kitchens.empty() && s.kitchens.back().hasDoorPose);
    auto pose=s.kitchens.back();CHECK(pose.doorPose[4]==12);
    Reset(s,net::Role::Client);Apply(pose);CHECK(test::applies==1 && test::state.doorPose.TX==12);
    pose.doorPose[3]=0;Apply(pose);CHECK(test::applies==1); // reject zero quaternion
    test::user=&test::local;OnMouse(call);CHECK(!g_motion.empty());
    ++test::world;g_next=0;Tick();CHECK(g_motion.empty() && g_pending.empty());
    // Actual grill request queue, fuel-before-fire arrival race, validation and replica gate.
    Reset(s,net::Role::Client);test::state={};test::state.kind=ue_wrap::kitchen_machine::Kind::Grill;
    call.object=&test::oven;call.fromOurCode=false;s.sendOk=false;
    CHECK(OnGrillIgnite(call)==sg::Verdict::Cancel);Tick();CHECK(s.kitchenIntents.empty()&&!g_grillOps.empty());
    s.sendOk=true;for(auto& row:g_grillOps)row.second.retryAt=0;Tick();CHECK(s.kitchenIntents.size()==1&&g_grillOps.empty());
    for(int i=0;i<20;++i)OnGrillIgnite(call);Tick();CHECK(s.kitchenIntents.size()==1); // held lighter cannot flood each frame
    auto ignition=s.kitchenIntents[0];CHECK(ignition.op==5&&OnProducer(call)==sg::Verdict::Cancel);
    test::hand=true;CHECK(OnGrillIgnite(call)==sg::Verdict::Run);test::hand=false;
    test::announced=false;CHECK(OnGrillIgnite(call)==sg::Verdict::Run&&OnProducer(call)==sg::Verdict::Run);test::announced=true;
    Reset(s,net::Role::Host);test::state={};test::state.kind=ue_wrap::kitchen_machine::Kind::Grill;
    OnIntent(ignition,1);Tick();CHECK(test::grillChanges==0&&!g_grillOps.empty()); // fuel record still in flight
    test::state.coals=3;for(auto& row:g_grillOps)row.second.retryAt=0;Tick();CHECK(test::grillChanges==1&&g_grillOps.empty());
    CHECK(!s.kitchens.empty()&&s.kitchens.back().coals==3&&(s.kitchens.back().flags&1));
    auto grillState=s.kitchens.back();auto badState=grillState;badState.coals=17;CHECK(!Valid(badState));badState=grillState;badState.burn=-1;CHECK(!Valid(badState));
    auto off=ignition;off.op=6;OnIntent(ignition,1);OnIntent(off,1);Tick();CHECK(test::grillChanges==2&&!(test::state.flags&1));
    OnIntent(ignition,1);++test::occupancy;Tick();CHECK(test::grillChanges==2&&g_grillOps.empty());
    OnIntent(ignition,1);test::reach=false;Tick();CHECK(g_grillOps.empty()&&test::grillChanges==2);test::reach=true;
    OnIntent(ignition,1);++test::oven.serial;Tick();CHECK(g_grillOps.empty());
    OnIntent(ignition,1);++test::world;Tick();CHECK(g_grillOps.empty());
    Reset(s,net::Role::Client);test::state={};test::state.kind=ue_wrap::kitchen_machine::Kind::Grill;
    Apply(grillState);CHECK(test::state.coals==3&&(test::state.flags&1)&&test::grillChanges==0);
    call.tag=0x4B54000B;AfterGrillChange(call);Tick();CHECK(test::records==1); // coal addition saved once
    test::recordOK=false;AfterGrillChange(call);CHECK(test::records==2&&!g_grillSaves.empty());
    test::recordOK=true;for(auto& row:g_grillSaves)row.second.retryAt=0;Tick();CHECK(test::records==3&&g_grillSaves.empty());
    OnGrillIgnite(call);OnDisconnect();CHECK(g_grillOps.empty()&&g_grillSaves.empty());
    std::puts("PASS: production kitchen mouse batching/release, no close echo, pose validation, ownership, reach and travel");
    std::puts("PASS: grill ignition/extinguish, admission retries, delayed fuel, passive client, canonical paint, supersession, occupancy/reach/serial/world/teardown");
}
