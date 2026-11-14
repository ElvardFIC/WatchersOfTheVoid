#include "../../src/votv-coop/src/coop/world/placed_save_sync.cpp"
#include "../../src/votv-coop/src/coop/world/garden_actions.cpp"
#include "../../src/votv-coop/src/coop/items/save_record_wire.cpp"
#include "../../src/votv-coop/src/coop/net/blob_chunks.cpp"
#include "llama_marker_boundary_stubs.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
using Rec = ue_wrap::save_record::SaveRecord;
struct Actor { int id; bool live = true; int serial = 1; Rec record; bool discard = false; } actors[20];
uint32_t world = 1; uint16_t playerNo = 2;
int spawned = 0, destroyed = 0, applied = 0, linked = 0, captures = 0, harvested = 0, nextActor = 1;
int toggles = 0, detaches = 0;
void* notReady=nullptr;
std::deque<bool> gardenReadyAnswers;
std::function<void()> gardenResolve,gardenReady,gardenKey,gardenAction,gardenReach;
void GardenCallback(std::function<void()>& f){auto callback=std::move(f);f={};if(callback)callback();}
int paramPlayerOff = 0;
bool ready = true, reach = true, registry = true, spawnOK = true, linkOK = true, captureOK = true;
bool travelOnSpawn = false, failDestroy = false;
bool holeWatchLive = true;
bool applyOK = true;
void (*captureCallback)() = nullptr;
void (*harvestCallback)() = nullptr;
void (*applyCallback)() = nullptr;
ue_wrap::placed_save::CareUse careUse{};
bool careReadable=false,careSpent=false;void* hand=nullptr;
std::set<uint64_t> careInvocations;
std::function<void()> onCareRead,onCareSpent;
std::wstring lastHeld=L"prop_bucket_C";
std::map<std::wstring, void*> keys;
void* caller = reinterpret_cast<void*>(1);
Rec Make(const wchar_t* key=L"mvpl_12345678_00000001",const wchar_t* cls=L"faucet_C") {
    Rec r; r.key = key; r.className = cls; r.bools={{1}}; r.names={{L"None"}}; return r;
}
Actor* New(const Rec& r) { auto* a = &actors[nextActor]; *a = {nextActor}; ++nextActor; a->record=r; keys[r.key]=a; return a; }
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a) { return a ? static_cast<test::Actor*>(a)->id : -1; }
int32_t SlotSerial(int32_t i) { return i >= 0 && i < 20 ? test::actors[i].serial : 0; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLiveByIndex(void* a, int32_t i) { return i >= 0 && i < 20 && a == &test::actors[i] && test::actors[i].live; }
bool IsLive(void* a) { return a && static_cast<test::Actor*>(a)->live; }
std::wstring ClassNameOf(void* a) { return static_cast<test::Actor*>(a)->record.className; }
void* FindFunction(void*,const wchar_t*) { return nullptr; }
int32_t FindParamOffset(void*,const wchar_t* name) { return !wcscmp(name,L"player")?test::paramPlayerOff:0; }
int32_t FunctionFrameSize(void*) { return 16; }
}
namespace ue_wrap::game_thread { bool IsGameThread() { return true; } }
namespace ue_wrap::world_identity {
void* CurrentWorld() { return reinterpret_cast<void*>(uintptr_t(test::world)); }
void* WorldOf(void*) { return CurrentWorld(); }
uint32_t Generation() { return test::world; }
}
namespace ue_wrap::object_index { void* ClassByName(const wchar_t*) { return nullptr; } }
namespace ue_wrap::facts {
Function::Function(detail::MacroToken, const char* ident, const char* feature, const wchar_t* cls,
                   const wchar_t* fn, Mode mode)
    : ident_(ident), feature_(feature), cls_(cls), fn_(fn), mode_(mode), next_(nullptr) {}
void Function::NoteUse(Use, bool) const {}
Resolved Resolve(const Function&) { return {}; }
Resolved ResolveOn(const Function&, void*) { return {}; }
}
namespace coop::element::scan_hub { void Register(const Consumer&) {} }
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn) { return true; }
bool ClassNameWatchLive(const wchar_t* cls,const wchar_t*,int) { return wcscmp(cls,L"dirthole_C") || test::holeWatchLive; }
bool IsObjectBodyActive(void*,void*) { return false; }
bool IsInvocationActive(uint64_t id){return test::careInvocations.contains(id);}
uint8_t* OutParamPtr(const Call& c,int32_t off) { return c.locals+off; }
}
namespace ue_wrap::ufunction_hook {
CallerFrame CurrentCallerFrame() { return {test::caller, nullptr}; }
bool InstallPostHook(const facts::Resolved&,PostNativeCallback,bool) { return true; }
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace coop::wall_build_sync {
void Install(net::Session*) {} void Tick() {} void OnDisconnect() {}
}
namespace coop::hand_item {
void* LocalHandActor(){return test::hand;}
bool LastHeldClassIs(uint8_t,const wchar_t* cls){return test::lastHeld==cls;}
}
namespace ue_wrap::keyed_objects {
bool Available() { return test::registry; }
void* Resolve(const wchar_t* key) { auto i=test::keys.find(key);void* actor=i==test::keys.end() || !reflection::IsLive(i->second) ? nullptr : i->second;test::GardenCallback(test::gardenResolve);return actor; }
}
namespace ue_wrap::engine {
bool DestroyActor(void* p) { ++test::destroyed; if(!test::failDestroy)static_cast<test::Actor*>(p)->live=false;return !test::failDestroy; }
}
namespace ue_wrap::placed_save {
Kind ClassKind(const std::wstring& s) { return s==L"llamaSoul_C" ? Kind::Marker : s==L"dirthole_C" ? Kind::DirtHole : s==L"faucet_C" ? Kind::Faucet : s==L"groundHose_C" ? Kind::Hose : (s==L"growingPlant_C"||s==L"growingPlant_pine_C"||s==L"growingPlant_stolasHerb_C") ? Kind::Plant : s==L"customWall_unfinished_C" ? Kind::WallOutline : (s==L"customWall_C"||s==L"customWall_wetConcrete_C") ? Kind::Wall : Kind::None; }
Kind Type(void* a) { return a ? ClassKind(static_cast<test::Actor*>(a)->record.className) : Kind::None; }
bool PlacementCaller(Kind k,void* f) { return k!=Kind::None && k!=Kind::Marker && f==reinterpret_cast<void*>(1); }
bool Ready(void* a) {test::GardenCallback(test::gardenReady);if(!test::gardenReadyAnswers.empty()){const bool result=test::gardenReadyAnswers.front();test::gardenReadyAnswers.pop_front();return result;}return test::ready && a!=test::notReady; }
bool Capture(void* a,test::Rec& r) { ++test::captures;r=static_cast<test::Actor*>(a)->record;if(test::captureCallback)test::captureCallback();return test::captureOK; }
std::wstring ExistingKey(void* a) {auto key=static_cast<test::Actor*>(a)->record.key;test::GardenCallback(test::gardenKey);return key; }
bool CanHarvest(void* a) { return Type(a)==Kind::Plant && !static_cast<test::Actor*>(a)->discard; }
bool Harvest(void* a,void* collector) { CHECK(collector==&test::actors[19]);++test::harvested;static_cast<test::Actor*>(a)->record.floats[1][3]=0;if(test::harvestCallback)test::harvestCallback();return true; }
bool ReadCareUse(void*,void*,CareKind,CareUse& use){use=test::careUse;test::GardenCallback(test::onCareRead);return test::careReadable;}
bool CareWasSpent(void*,CareKind){test::GardenCallback(test::onCareSpent);return test::careSpent;}
bool ApplyCare(void* a,CareKind kind,float amount){auto& state=static_cast<test::Actor*>(a)->record.floats[1];if(kind==CareKind::Water)state[2]=amount;else if(state[1]<=0)state[1]=amount;return true;}
bool Normalize(const test::Rec& a,test::Rec& b) { b=a;return ClassKind(a.className)!=Kind::None; }
bool SetKey(void* a,const std::wstring& key) { static_cast<test::Actor*>(a)->record.key=key;test::keys[key]=a;return true; }
void* Spawn(const test::Rec& r) { if(!test::spawnOK)return nullptr;++test::spawned;auto* a=test::New(r);if(test::travelOnSpawn){++test::world;coop::placed_save_sync::World();}return a; }
bool Apply(void* a,const test::Rec& r) { ++test::applied;static_cast<test::Actor*>(a)->record=r;if(test::applyCallback)test::applyCallback();return test::applyOK; }
bool Link(void*,const test::Rec&) { ++test::linked;return test::linkOK; }
bool HoseLinksAvailable(void*,const test::Rec&) { return test::linkOK; }
bool ToggleFaucet(void* a){++test::toggles;auto& flag=static_cast<test::Actor*>(a)->record.bools[0][0];flag=!flag;test::GardenCallback(test::gardenAction);return true;}
bool DetachSprinkler(void* a,const std::wstring& expected){auto& key=static_cast<test::Actor*>(a)->record.names[0][1];if(key!=expected)return false;key=L"None";++test::detaches;return true;}
}
namespace coop::signal_wire {
std::vector<uint8_t> Serialize(const ue_wrap::signal_dynamic::Row&) { return {}; }
bool Deserialize(const std::vector<uint8_t>&,ue_wrap::signal_dynamic::Row&) { return false; }
}
namespace coop::roster_ledger { const Row& Get(int) { static Row r;r.playerNo=test::playerNo;return r; } }
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session&,uint8_t,float) { return {}; }
IntentSubject IntentTarget::Authorize(void* a) const {test::GardenCallback(test::gardenReach);IntentSubject s;s.actor=a;s.outcome=test::reach?IntentOutcome::Ok:IntentOutcome::OutOfReach;return s; }
IntentSubject IntentTarget::AuthorizeSegment(const ue_wrap::FVector&,const ue_wrap::FVector&) const { return Authorize(nullptr); }
}
using namespace coop;
using namespace coop::placed_save_sync;
void Reset(net::Session& s, net::Role role) {
    coop::garden_actions::OnDisconnect();
    OnDisconnect();s.mode=role;s.sent.clear();s.recipients.clear();s.sendBudget=-1;s.linked=true;s.worldReady=true;s.onSend={};
    test::keys.clear();test::nextActor=1;test::spawned=test::destroyed=test::applied=test::linked=test::captures=test::harvested=0;
    test::ready=test::reach=test::registry=test::spawnOK=test::linkOK=test::captureOK=true;
    test::travelOnSpawn=test::failDestroy=false;test::playerNo=2;test::caller=reinterpret_cast<void*>(1);
    test::captureCallback=test::harvestCallback=nullptr;
    test::applyCallback=nullptr;test::applyOK=true;
    test::toggles=test::detaches=0;
    test::notReady=nullptr;test::gardenResolve=test::gardenReady=test::gardenKey=test::gardenAction=test::gardenReach={};
    test::gardenReadyAnswers.clear();
    test::paramPlayerOff=0;
    test::actors[19]={19};players::Registry::Get().remote.actor=&test::actors[19];
    players::Registry::Get().local=&test::actors[19];test::hand=nullptr;
    test::careReadable=test::careSpent=false;test::lastHeld=L"prop_bucket_C";test::careInvocations.clear();test::onCareRead=test::onCareSpent={};
    Install(&s);
    coop::garden_actions::Install(&s);
}
void Pump(int n=3) { for(int i=0;i<n;++i){g_tickAt=0;for(auto& [k,r]:g_rows){r.next=0;r.linkAt=0;}Tick();} }
std::vector<net::BlobChunkPayload> Packet(Op op,const test::Rec& r,bool junk=false,uint32_t useSeq=1,uint32_t person=test::playerNo) {
    net::Session s;std::vector<uint8_t> b;W::AppU8(b,op);
    if(op==Remove||op==Retired||op==Refresh||op==Harvest)W::AppWStr(b,r.key);else W::SerSave(b,r);
    if(op==Harvest){W::AppU32(b,useSeq);W::AppU32(b,person);}
    if(junk)b.push_back(1);
    static uint32_t seq=100;CHECK(blob_chunks::SendBlob(&s,net::ReliableKind::PlacedSave,++seq,b));return s.sent;
}
void Deliver(const std::vector<net::BlobChunkPayload>& b,uint8_t slot) { for(auto& p:b)Receive(p,slot); }
int main() {
    net::Session s;auto r=test::Make();
    Reset(s,net::Role::Host);Deliver(Packet(Create,r),1);Pump();
    CHECK(test::spawned==1 && g_rows.at(r.key).accepted && !s.sent.empty());
    auto* a=static_cast<test::Actor*>(g_rows.at(r.key).ref.Get());a->record.bools[0][0]=0;
    Deliver(Packet(Create,r),1);Pump();CHECK(test::spawned==1&&test::applied==0&&g_rows.at(r.key).record.bools[0][0]==0);
    // Fresh native state is captured for a late join; no duplicate births.
    QueueConnectBroadcastForSlot(2);Pump();CHECK(test::spawned==1);
    test::failDestroy=true;Deliver(Packet(Remove,r),1);CHECK(!g_rows.at(r.key).dead);
    test::failDestroy=false;Deliver(Packet(Remove,r),1);CHECK(g_rows.at(r.key).dead);Pump();
    Deliver(Packet(Create,r),1);Pump();CHECK(test::spawned==1);
    Reset(s,net::Role::Host);Deliver(Packet(Remove,r),1);Deliver(Packet(Create,r),1);Pump();CHECK(test::spawned==0&&g_rows.at(r.key).dead);
    Reset(s,net::Role::Host);test::reach=false;Deliver(Packet(Create,r),1);CHECK(g_rows.empty());
    test::reach=true;Deliver(Packet(State,r),1);Deliver(Packet(Create,r),0);Deliver(Packet(Create,r,true),1);CHECK(g_rows.empty());
    // Partial blob never creates anything; retry from a fresh sequence creates once.
    auto large=r;large.className=L"groundHose_C";large.names={{L"tap",L"None"}};large.vectors.resize(1);large.vectors[0].resize(50);
    auto chunks=Packet(Create,large);CHECK(chunks.size()>1);Receive(chunks.front(),1);Pump();CHECK(test::spawned==0);
    Deliver(Packet(Create,r),1);Pump();CHECK(test::spawned==1);
    Reset(s,net::Role::Host);test::spawnOK=false;Deliver(Packet(Create,r),1);Pump();CHECK(test::spawned==0);
    test::spawnOK=true;Pump();CHECK(test::spawned==1);
    Reset(s,net::Role::Host);Deliver(Packet(Create,r),1);++test::playerNo;Pump();CHECK(test::spawned==0&&g_rows.empty());
    Reset(s,net::Role::Host);Deliver(Packet(Create,r),1);test::travelOnSpawn=true;Pump();CHECK(g_rows.empty());
    Reset(s,net::Role::Client);auto* local=test::New(r);Finished(nullptr,nullptr,local);CHECK(g_rows.size()==1);
    const auto key=g_rows.begin()->first;CHECK(Ours(key));s.sendBudget=1;Pump(2);CHECK(!g_rows.at(key).accepted&&!s.sent.empty());
    s.sendBudget=-1;Pump();auto canonical=g_rows.at(key).record;Deliver(Packet(State,canonical),0);
    CHECK(test::spawned==0&&test::applied==0&&g_rows.at(key).accepted&&g_rows.at(key).ref.Get()==local);
    ue_wrap::script_gate::Call manual{};manual.object=local;BeforeAction(manual);
    local->live=false;Deliver(Packet(State,canonical),0);Pump();CHECK(g_rows.at(key).remove&&test::spawned==0);
    Deliver(Packet(Retired,canonical),0);Pump();CHECK(g_rows.at(key).dead);
    // Save-loaded twin is rebound; an unavailable registry must never produce a duplicate.
    Reset(s,net::Role::Client);local=test::New(r);Deliver(Packet(State,r),0);test::registry=false;Pump();CHECK(test::spawned==0&&test::applied==0);
    test::registry=true;test::linkOK=false;Pump();CHECK(test::spawned==0&&test::applied==1&&g_rows.at(r.key).link);
    test::linkOK=true;Pump();CHECK(!g_rows.at(r.key).link);
    Reset(s,net::Role::Client);local=test::New(r);test::failDestroy=true;Deliver(Packet(Retired,r),0);Pump();CHECK(local->live);
    test::failDestroy=false;Pump();CHECK(!local->live&&g_rows.at(r.key).dead);
    Reset(s,net::Role::Client);Deliver(Packet(State,r),0);Pump();local=static_cast<test::Actor*>(g_rows.at(r.key).ref.Get());
    local->live=false;Pump();CHECK(!g_rows.at(r.key).remove&&!g_rows.at(r.key).accepted);Deliver(Packet(State,r),0);Pump();CHECK(g_rows.at(r.key).ref.Get());
    Reset(s,net::Role::Client);r=test::Make(L"ignored",L"groundHose_C");local=test::New(r);Finished(nullptr,nullptr,local);Pump();CHECK(s.sent.empty());
    ue_wrap::script_gate::Call c{};c.object=local;HoseFinished(c);Pump();CHECK(!s.sent.empty());
    Reset(s,net::Role::Client);test::caller=nullptr;local=test::New(test::Make());Finished(nullptr,nullptr,local);CHECK(g_rows.empty());
    puts("PASS placed save: real record/chunk transport, adoption/retry, stale creation, tombstones, role/reach, save twin, hose completion, slot/world reset");
    return 0;
}
