#include "../../src/votv-coop/src/coop/interactables/kitchen_sync.cpp"
#include "ue_wrap/devices/grill.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Actor { int id; bool live=true; int serial=1; };
Actor oven{1},door{2},local{3},puppet{4},oven2{5},door2{6};
uint32_t world=1;
void* user=nullptr;
bool reach=true,hand=false,announced=true,recordOK=true;
bool mirror=false,applyOK=true,readOK=true;
std::function<void()> callback;
std::function<void()> animateCallback;
std::function<void()> stockReadCallback,stockEditCallback;
bool stockReadOK=true;
bool mutationReady=true;
int stockWrites=0;
ue_wrap::grill::Edit stockResult=ue_wrap::grill::Edit::Applied;
uint8_t* out=nullptr;
void Callback(){auto f=std::move(callback);callback={};if(f)f();}
uint16_t occupancy=2;
int grillChanges=0,records=0;
int moves=0,applies=0,opens=0;
ue_wrap::FVector2D mouse{};
ue_wrap::kitchen_machine::State state{};
std::wstring keyOverride;
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
int32_t FindParamOffset(void*,const wchar_t* name) {return !wcscmp(name,L"player")?8:0;}
int32_t FunctionFrameSize(void*) {return 16;}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask){CHECK(!wcscmp(name,L"added"));off=0;mask=1;return true;}
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
uint8_t* OutParamPtr(const Call&,int32_t){return test::out;}
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::prop { std::wstring GetInteractableKeyString(void* a) { return a==&test::oven2?L"oven2":L"oven"; }
std::wstring GetKeyString(void* a){return a==&test::oven && !test::keyOverride.empty()?test::keyOverride:GetInteractableKeyString(a);} }
namespace ue_wrap::kitchen_machine {
Kind Type(void* a) { return a==&test::oven || a==&test::oven2 ? test::state.kind : Kind::None; }
bool Read(void* a,State& s) { if (a!=&test::oven || !test::oven.live) return false; s=test::state;test::Callback();return test::readOK; }
bool Apply(void*,const State& s,const std::function<bool()>& current) { ++test::applies;test::state=s;test::Callback();return test::applyOK && (!current || current()); }
bool Press(void*,void*,uint8_t,uint8_t) { return true; }
bool SetGrillFire(void*,bool on) {++test::grillChanges;if(on)test::state.flags|=Active;else test::state.flags&=~Active;return true;}
uint8_t ReadControl(void*) { return 0; }
void* DoorParent(void* a) { return a==&test::door ? &test::oven : (a==&test::door2?&test::oven2:nullptr); }
bool OwnsDoor(void* a) { return a==&test::oven || a==&test::oven2; }
void* DoorUser(void*) { return test::user; }
bool SetDoor(void*,bool on) { ++test::opens; if (on) test::state.flags|=Opened; else test::state.flags&=~Opened; return true; }
bool MoveDoor(void*,void* player,const FVector2D& d) { if (player!=&test::puppet) return false; ++test::moves;test::mouse=d;return true; }
void Animate(void*,float) {auto f=std::move(test::animateCallback);test::animateCallback={};if(f)f();}
}
namespace coop::element::scan_hub { void Register(const Consumer&) {} }
namespace coop::grill_hand_sync {void Install(net::Session*){}void Tick(){}void QueueConnect(int){}void OnPeerLeft(int){}void OnDisconnect(){}}
#ifndef GRILL_EDITS_ACTUAL_TEST
namespace coop::grill_edits {void Install(net::Session*){}void Tick(){}void OnPeerLeft(int){}void OnDisconnect(){}
void BeforeChange(const ue_wrap::script_gate::Call&,uint8_t){}void AfterChange(const ue_wrap::script_gate::Call&){}
}
#endif
namespace ue_wrap::grill {
bool IsGrill(void* actor){return kitchen_machine::Type(actor)==kitchen_machine::Kind::Grill;}
bool ReadStock(void*,Stock& out,const std::function<bool()>& current){
    out={test::state.coals,bool(test::state.flags&kitchen_machine::HasLid)};
    auto fn=std::move(test::stockReadCallback);test::stockReadCallback={};if(fn)fn();
    return test::stockReadOK && (!current || current());
}
Edit ChangeStock(void*,uint8_t operation,int32_t value,const std::function<bool()>& current){
    if(!current() || test::stockResult==Edit::Retry)return Edit::Retry;
    ++test::stockWrites;if(operation==1)test::state.coals=value;
    else if(value)test::state.flags|=kitchen_machine::HasLid;else test::state.flags&=~kitchen_machine::HasLid;
    auto fn=std::move(test::stockEditCallback);test::stockEditCallback={};if(fn)fn();return test::stockResult;
}
}
namespace coop::net_pump { bool IsInAnnouncedWorld(void*) { return test::announced; } }
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand || test::mirror;}bool IsMirrorActor(void*){return test::mirror;}}
namespace coop::prop_save_data {bool Publish(net::Session*,void*,const std::wstring&){++test::records;return test::recordOK;}
bool PrepareForMutation(void*,const std::wstring&){return test::mutationReady;}}
namespace coop::prop_element_tracker {
element::ElementId GetPropElementIdForActor(void*) { return 10; }
void* FindLiveActorByKey(const std::wstring& key) { return key==L"oven" && test::oven.live ? &test::oven : (key==L"oven2" && test::oven2.live?&test::oven2:nullptr); }
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
std::wstring PortableWireKey(void*) { return L"oven"; }
IntentTarget IntentTarget::ForClientIntent(net::Session&,uint8_t,float) { return {}; }
IntentSubject IntentTarget::Authorize(void*) const { return {test::reach ? IntentOutcome::Ok : IntentOutcome::OutOfReach}; }
}
namespace ue_wrap::cremator {
const std::vector<void*>& Instances() { static const std::vector<void*> rows; return rows; }
}
using namespace coop;
using namespace coop::kitchen_sync;
void Reset(net::Session& s,net::Role role) {
    OnDisconnect();++test::world;s=net::Session{};s.mode=role;
    test::oven={1};test::oven2={5};test::state={};test::state.kind=ue_wrap::kitchen_machine::Kind::Microwave;
    test::state.hasDoorPose=true;test::state.doorPose.TX=12;
    test::moves=test::applies=test::opens=0;test::reach=true;test::user=nullptr;
    test::hand=false;test::announced=true;test::occupancy=2;test::grillChanges=test::records=0;
    test::recordOK=true;
    test::mirror=false;test::applyOK=test::readOK=true;test::callback={};test::animateCallback={};
    test::stockReadCallback=test::stockEditCallback={};test::stockReadOK=test::mutationReady=true;test::stockWrites=0;test::stockResult=ue_wrap::grill::Edit::Applied;
    test::keyOverride.clear();
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
    call.tag=0x4B54000B;AfterGrillChange(call);Tick();CHECK(test::records==0); // clients no longer publish full grill records
    Reset(s,net::Role::Host);test::state.kind=K::Kind::Grill;
    test::recordOK=false;AfterGrillChange(call);CHECK(test::records==1&&!g_grillSaves.empty());
    test::recordOK=true;for(auto& row:g_grillSaves)row.second.retryAt=0;Tick();CHECK(test::records==2&&g_grillSaves.empty());
    OnGrillIgnite(call);OnDisconnect();CHECK(g_grillOps.empty()&&g_grillSaves.empty());
    Reset(s,net::Role::Host);test::state.kind=ue_wrap::kitchen_machine::Kind::Cremator;
    test::state.hasDoorPose=true;test::state.doorPose.TX=42;
    auto crematorIntent=motion;OnIntent(crematorIntent,1);CHECK(test::moves==1);
    Publish(&test::oven,-1);CHECK(!s.kitchens.empty()&&s.kitchens.back().kind==6);
    auto crematorState=s.kitchens.back();CHECK(crematorState.doorPose[4]==42);
    crematorIntent.op=1;const auto before= test::moves;OnIntent(crematorIntent,1);CHECK(test::moves==before);
    Reset(s,net::Role::Client);test::state.kind=ue_wrap::kitchen_machine::Kind::Cremator;
    Apply(crematorState);CHECK(test::applies==1&&test::state.doorPose.TX==42);
    crematorState.flags=ue_wrap::kitchen_machine::Active;CHECK(!Valid(crematorState));
    Reset(s,net::Role::Host);test::state={};test::state.kind=K::Kind::Grill;
    test::state.flags=K::Active|K::HasLid;test::state.cooking=true;test::state.cookVolume=.07f;
    Publish(&test::oven,-1);auto lid=s.kitchens.back();CHECK((lid.flags&K::HasLid) && lid.cooking && lid.cookVolume==.07f);
    test::state.cookVolume=.02f;Publish(&test::oven,-1);CHECK(s.kitchens.back().cookVolume==.02f);
    auto invalid=lid;invalid.cooking=2;CHECK(!Valid(invalid));invalid=lid;invalid.cookVolume=NAN;CHECK(!Valid(invalid));
    invalid=lid;invalid.kind=1;CHECK(!Valid(invalid));
    Reset(s,net::Role::Client);test::state.kind=K::Kind::Grill;Apply(lid);
    CHECK((test::state.flags&K::HasLid) && test::state.cooking && test::state.cookVolume==.07f);
    call.object=&test::oven;call.tag=0x4B54000D;AfterGrillChange(call);CHECK(test::records==0);
    call.fromOurCode=true;AfterGrillChange(call);CHECK(test::records==0);call.fromOurCode=false;
    test::mirror=true;CHECK(OnProducer(call)==sg::Verdict::Cancel);test::mirror=false;test::hand=true;CHECK(OnProducer(call)==sg::Verdict::Run);test::hand=false;
    for(const auto role:{net::Role::Client,net::Role::Host}){
        s.mode=role;test::mirror=true;CHECK(OnGrillIgnite(call)==sg::Verdict::Cancel);
        uint8_t frame[16]{0xAB},returned=0xAB;call.function=&test::local;call.locals=frame;test::out=&returned;
        CHECK(OnGrillCoal(call)==sg::Verdict::Cancel && frame[0]==0xAA && returned==0xAA);
        test::mirror=false;CHECK(OnGrillCoal(call)==sg::Verdict::Run);test::out=nullptr;call.locals=nullptr;
    }
    s.mode=net::Role::Client;
    // A newer state received during native paint is retained, never marked applied
    // by the older call; recursive paint of this same actor is deferred.
    auto latest=lid;latest.flags=0;latest.cooking=0;latest.cookVolume=0;
    test::callback=[&]{Apply(latest);};Apply(lid);g_next=0;Tick();CHECK(test::state.flags==0 && !test::state.cooking);
    test::applyOK=false;Apply(lid);CHECK(!g_applied.count(L"oven"));test::applyOK=true;g_retryAt.clear();g_next=0;Tick();CHECK(test::state.cooking);
    test::callback=[]{OnDisconnect();};Apply(lid);CHECK(g_pending.empty() && g_applied.empty());
    Reset(s,net::Role::Host);test::state={};test::state.kind=K::Kind::Grill;test::state.flags=K::HasLid;
    s.sendOk=false;SendAllToSlot(1);CHECK(g_joins.size()==1);test::state.flags=0;s.sendOk=true;Tick();
    CHECK(g_joins.empty() && !s.kitchens.empty() && !(s.kitchens.front().flags&K::HasLid));
    s.sendOk=false;SendAllToSlot(1);CHECK(g_joins.size()==1);++test::occupancy;Tick();CHECK(g_joins.empty() || !g_joins.begin()->second.pending);
    Reset(s,net::Role::Host);test::state={};test::state.kind=K::Kind::Grill;test::readOK=false;SendAllToSlot(1);CHECK(g_joins.size()==1);
    test::readOK=true;Tick();CHECK(g_joins.empty() && !s.kitchens.empty());
    test::callback=[]{OnDisconnect();};CHECK(!Publish(&test::oven,-1) && g_sent.empty());
    Reset(s,net::Role::Host);test::state={};test::state.kind=K::Kind::Grill;
    test::callback=[&]{test::state.flags=K::HasLid;Publish(&test::oven,-1);};
    CHECK(!Publish(&test::oven,-1) && s.kitchens.empty());Publish(&test::oven,-1);CHECK(s.kitchens.back().flags&K::HasLid);
    Reset(s,net::Role::Host);test::state={};test::state.kind=K::Kind::Grill;
    s.beforeSend=[&]{++test::occupancy;SendAllToSlot(1);};SendAllToSlot(1);
    CHECK(test::occupancy==3 && g_joins.count(L"oven") && (g_joins.at(L"oven").pending&(1u<<1)));Tick();CHECK(g_joins.empty());
    Reset(s,net::Role::Client);test::state={};test::state.kind=K::Kind::Grill;
    uint8_t args[16]{};args[0]=7;void* user=&test::local;std::memcpy(args+8,&user,sizeof(user));
    call.function=&test::local;call.locals=args;call.object=&test::oven;call.tag=0x4B54000C;
    CHECK(!IsLocalLidOutput(&test::oven));OnGrillAction(call);CHECK(IsLocalLidOutput(&test::oven) && !IsLocalLidOutput(&test::door));
    AfterGrillAction(call);CHECK(!IsLocalLidOutput(&test::oven));
    user=&test::puppet;std::memcpy(args+8,&user,sizeof(user));OnGrillAction(call);CHECK(!IsLocalLidOutput(&test::oven));
    user=&test::local;std::memcpy(args+8,&user,sizeof(user));OnGrillAction(call);++test::world;CHECK(!IsLocalLidOutput(&test::oven));
    Reset(s,net::Role::Client);ue_wrap::FVector2D movement{8,-4};call={};call.object=&test::door;
    call.function=&test::local;call.locals=reinterpret_cast<uint8_t*>(&movement);test::user=&test::local;OnMouse(call);
    s.beforeSend=[&]{movement={3,2};OnMouse(call);Tick();};Tick();
    CHECK(s.kitchenIntents.size()==1 && s.kitchenIntents.front().mouseX==8 && g_motion.count(L"oven") && g_motion.at(L"oven").x==3);
    g_next=0;Tick();CHECK(s.kitchenIntents.size()==2 && s.kitchenIntents.back().mouseX==3 && g_motion.empty());
    Reset(s,net::Role::Client);test::user=&test::local;movement={8,-4};OnMouse(call);
    ue_wrap::FVector2D secondMovement{4,0};auto secondCall=call;secondCall.object=&test::door2;
    secondCall.locals=reinterpret_cast<uint8_t*>(&secondMovement);OnMouse(secondCall);
    s.beforeSend=[&]{secondMovement={3,2};OnMouse(secondCall);};Tick();
    CHECK(s.kitchenIntents.size()==2 && s.kitchenIntents.back().mouseX==7 && s.kitchenIntents.back().mouseY==2 && g_motion.empty());
    Reset(s,net::Role::Client);movement={8,-4};test::user=&test::local;OnMouse(call);s.sendOk=false;
    s.beforeSend=[&]{movement={3,2};OnMouse(call);};Tick();CHECK(g_motion.at(L"oven").x==11 && g_motion.at(L"oven").y==-2);
    s.sendOk=true;g_next=0;Tick();CHECK(s.kitchenIntents.size()==1 && s.kitchenIntents.back().mouseX==11 && g_motion.empty());
    test::user=&test::local;OnMouse(call);s.beforeSend=[]{OnDisconnect();};g_next=0;Tick();CHECK(g_motion.empty() && !g_session.load());
    Reset(s,net::Role::Client);test::animateCallback=[]{OnDisconnect();};Tick();CHECK(!g_session.load() && g_actors.empty());
    Reset(s,net::Role::Host);test::callback=[]{OnDisconnect();};Tick();CHECK(!g_session.load() && g_sent.empty());
    Reset(s,net::Role::Host);test::state={};test::state.kind=K::Kind::Grill;test::state.coals=3;
    OnIntent(ignition,1);test::callback=[]{OnDisconnect();};Tick();CHECK(!g_session.load() && test::grillChanges==0);
    std::puts("PASS: kitchen tick reentrancy, native teardown and door input retained across successful/failed sends");
    std::puts("PASS: grill lid/cooking sound, hand producer boundaries, malformed state, latest/reentrant readiness, independent join retry and teardown");
    std::puts("PASS: production kitchen/cremator mouse batching, pose validation, parent identity, ownership, reach and travel");
    std::puts("PASS: grill ignition/extinguish, admission retries, delayed fuel, passive client, canonical paint, supersession, occupancy/reach/serial/world/teardown");
    return 0;
}
