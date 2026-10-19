// Exercise the real receiver, retry queue and script callbacks with engine doubles.
#include "../../src/votv-coop/src/coop/world/service_panel_sync.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Actor { int id; bool live=true; int serial=1; uint8_t state[16]{}; };
Actor desk{1}, radio{2}, part{3}, player{4};
uint32_t world=1;
bool body=true,reach=true,writeOk=true,spawnOk=true,held=false;
bool localGrab=false,canReturn=false,returnOk=false;
int holder=1,drops=0,ends=0,coverIndex=0;
int spawns=0, presses=0, applies=0, returns=0;
auto& Clears() { static auto* entries=new std::vector<std::pair<coop::roster_ledger::PerSlotClearFn,void*>>; return *entries; }
Actor* A(void* p) { return static_cast<Actor*>(p); }
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a) { return a ? test::A(a)->id : -1; }
int32_t SlotSerial(int32_t i) { return i==1 ? test::desk.serial : 1; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLiveByIndex(void* a,int32_t i) { return a && test::A(a)->live && test::A(a)->id==i; }
bool IsLive(void* a) { return a && test::A(a)->live; }
const FName& NameOf(void*) { static FName n{1,0}; return n; }
bool NameStartsWith(const FName&,const wchar_t*) { return false; }
int32_t ParamOffset::Of(void*) { return 0; }
}
namespace ue_wrap::world_identity {
void* CurrentWorld() { return reinterpret_cast<void*>(uintptr_t(test::world)); }
void* WorldOf(void*) { return CurrentWorld(); }
uint32_t Generation() { return test::world; }
}
namespace ue_wrap::object_index { uint64_t ClassSetVersion() { return 1; } }
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn) { return true; }
void ResolvePendingNames() {}
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::component_calls { bool CallParamlessNamed(void*,const wchar_t*) { return true; } }
namespace ue_wrap::game_rules { bool ReadLocal(Snapshot&) { return false; } }
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& p) { p={1,2,3}; return true; }
bool IsMainPlayerGrabbing(void*,void* a) { return a==&test::part && test::localGrab; }
bool ReleaseMainPlayerGrabIfHolding(void*,void*) { test::localGrab=false;++test::drops;return true; }
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& out) { out={};out.grabbingActor=test::localGrab ? &test::part : nullptr;return true; }
}
namespace ue_wrap::prop {
std::wstring GetInteractableKeyString(void*) { return L"cover"; }
void* GetStaticMesh(void* a) { return a; }
}
namespace ue_wrap::service_panel {
uint8_t Family(void* a) { return a==&test::desk ? 1 : (a==&test::radio ? 2 : 0); }
bool Read(void* a,uint8_t out[16]) { if (!a || !test::A(a)->live) return false; std::memcpy(out,test::A(a)->state,16); return true; }
bool Apply(void* a,const uint8_t in[16]) {
    if (!test::writeOk || !a) return false;
    std::memcpy(test::A(a)->state,in,16); ++test::applies; return true;
}
bool RadioLook(void*,uint8_t& op,uint8_t& index) { op=1; index=24; return true; }
bool RadioUse(void*,uint8_t op,uint8_t index) { if (op==1 && index>=test::radio.state[1]) return false; ++test::presses; return true; }
bool PartLocation(void*,uint8_t,uint8_t,FVector& at) { at={}; return true; }
bool FuseLook(void*,uint8_t& index) { index=0; return true; }
bool SetFuse(void* a,uint8_t index,uint8_t v) { if (!test::writeOk) return false; test::A(a)->state[4+index]=v; return true; }
bool DeskLook(void*,uint8_t& index) { index=0; return true; }
void* SpawnLoosePart(void*,uint8_t,bool,const FVector*) { if (!test::spawnOk) return nullptr; ++test::spawns; return &test::part; }
int CoverIndex(void*) { return test::coverIndex; }
bool CanReturnCover(void* a,void* c) { return a==&test::desk && c==&test::part && test::canReturn; }
bool TryReturnCover(void*,void*) { ++test::returns;if(test::returnOk) { test::desk.state[0]|=1<<test::coverIndex;test::part.live=false; }return test::returnOk; }
}
namespace coop::element::scan_hub { void Register(const Consumer&) {} }
namespace coop::net_pump { bool IsInAnnouncedWorld(void*) { return true; } }
namespace coop::remote_prop {
bool IsActorUnderAnyDrive(void*) { return test::held; }
void* DrivenActor(int slot) { return test::held && slot==test::holder ? &test::part : nullptr; }
element::ElementId ResolveMirrorEidByActor(void*,bool) { return 3; }
void* ResolveLiveActorByEid(uint32_t eid) { return eid==3 && test::part.live ? &test::part : nullptr; }
void EndAnyHoldOn(void*) { test::held=false;++test::ends; }
}
namespace coop::prop_element_tracker {
element::ElementId GetPropElementIdForActor(void*) { return 3; }
void* FindLiveActorByKey(const std::wstring& key) { return key==L"cover" && test::part.live ? &test::part : nullptr; }
}
namespace coop::prop_park { void Unpark(void*,void*,bool) {} }
namespace coop::roster_ledger {
void RegisterPerSlotClear(PerSlotClearFn fn,void* p) {
    // Registrations happen before main, before other globals' dynamic init.
    test::Clears().push_back({fn,p});
}
}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session&,uint8_t,float) { return {}; }
bool IntentTarget::HasBody() const { return test::body; }
IntentSubject IntentTarget::Authorize(void*) const { return {test::reach ? IntentOutcome::Ok : IntentOutcome::OutOfReach}; }
IntentSubject IntentTarget::AuthorizeSegment(const ue_wrap::FVector&,const ue_wrap::FVector&) const { return Authorize(nullptr); }
}
using namespace coop;
using namespace coop::service_panel_sync;
void Reset(net::Session& s,net::Role role=net::Role::Host) {
    OnDisconnect(); ++test::world;
    s=net::Session{}; s.mode=role;
    test::desk={1}; test::radio={2}; test::part={3};
    test::desk.state[0]=15; test::desk.state[1]=15;
    test::radio.state[0]=6; test::radio.state[1]=25; test::radio.state[2]=1;
    test::body=test::reach=test::writeOk=test::spawnOk=true; test::held=false;
    test::localGrab=test::canReturn=test::returnOk=false;test::drops=test::ends=test::coverIndex=0;test::holder=1;
    test::spawns=test::presses=test::applies=test::returns=0;
    players::Registry::Get().puppet.actor=&test::player;
    Install(&s); ResetWorld(); Observe(&test::desk); Observe(&test::radio);
}
net::ServicePanelPayload Pull(uint32_t seq=1) { net::ServicePanelPayload p{}; p.family=1;p.op=5;p.seq=seq;return p; }
int main() {
    net::Session s; Reset(s);
    auto p=Pull(); Receive(p,1); CHECK(test::spawns==1 && test::desk.state[0]==14);
    Receive(p,1); CHECK(test::spawns==1); // duplicate must never make a second cover
    Receive(Pull(1),2); CHECK(test::spawns==1); // two players pulling the same cover
    Reset(s); test::spawnOk=false; Receive(Pull(),1); CHECK(test::desk.state[0]==15);
    Reset(s); test::writeOk=false; Receive(Pull(),1); CHECK(test::spawns==0 && test::desk.state[0]==15);
    Reset(s); test::body=false; Receive(Pull(),1); CHECK(g_ack[1]==0);
    test::body=true; Receive(Pull(),1); CHECK(test::spawns==1);
    Reset(s); g_rows[1].actor.Reset(); Receive(Pull(),1); CHECK(g_ack[1]==0);
    Observe(&test::desk); Receive(Pull(),1); CHECK(test::spawns==1);
    Reset(s); test::reach=false; Receive(Pull(),1); CHECK(test::spawns==0 && g_ack[1]==1);
    Reset(s); p=Pull(); p.family=2; Receive(p,1); CHECK(g_ack[1]==0); // family/op mismatch
    p=Pull(); p.index=255; Receive(p,1); CHECK(g_ack[1]==0);
    Reset(s); p={};p.family=2;p.op=2;p.action=1;p.index=24;p.seq=1;
    Receive(p,1); CHECK(test::presses==1); Receive(p,1); CHECK(test::presses==1);
    Reset(s); p={};p.family=2;p.op=3;p.seq=1;p.index=0;
    std::memcpy(p.before,test::radio.state,16);std::memcpy(p.state,p.before,16);p.state[4]=1;
    Receive(p,1); CHECK(test::radio.state[4]==1 && test::spawns==0);
    Receive(p,2); CHECK(test::spawns==1); Receive(p,2); CHECK(test::spawns==1); // exactly one refund
    Reset(s);test::reach=false;Receive(p,1);CHECK(test::spawns==1 && test::radio.state[4]==0);
    Reset(s);test::reach=false;test::spawnOk=false;Receive(p,1);CHECK(g_ack[1]==0 && !g_applying);
    test::spawnOk=true;Receive(p,1);Receive(p,1);CHECK(test::spawns==1 && g_ack[1]==1);
    Reset(s); test::held=true;
    void* cover=&test::part; ue_wrap::script_gate::Call overlap{}; overlap.object=&test::desk;
    overlap.locals=reinterpret_cast<uint8_t*>(&cover);
    CoverOverlap(overlap); CHECK(g_coverReturns.size()==1);
    g_next=0;Tick();CHECK(test::returns==0);
    test::held=false;g_next=0;Tick();CHECK(test::returns==1 && g_coverReturns.empty());
    Reset(s); ++test::desk.serial; Receive(Pull(),1); CHECK(test::spawns==0); // recycled object slot
    Reset(s); p=Pull(); g_ack[1]=10;
    for (auto [clear,context]:test::Clears()) clear(context,1);
    Receive(p,1); CHECK(test::spawns==1); // real registered occupancy callback
    Reset(s,net::Role::Client);
    net::ServicePanelPayload canon{};canon.family=1;std::memcpy(canon.state,test::desk.state,16);
    Receive(canon,2); CHECK(!g_rows[1].haveCanonical); // clients cannot author rows
    Receive(canon,0); CHECK(g_rows[1].haveCanonical);
    net::ServicePanelPayload edit{};edit.family=1;edit.op=1;edit.action=16;
    std::memcpy(edit.before,canon.state,16);std::memcpy(edit.state,canon.state,16);edit.state[1]=14;
    s.sendOk=false; CHECK(Queue(edit)); CHECK(g_pending.size()==1 && s.panels.empty());
    s.sendOk=true;g_next=0;Tick();CHECK(s.panels.size()==1);
    Receive(canon,0); CHECK(test::desk.state[1]==14 && g_pending.size()==1); // stale row cannot undo unacked edit
    canon.state[1]=14;canon.ack[1]=1;Receive(canon,0);CHECK(g_pending.empty());
    CHECK(Queue(Pull())); CHECK(Queue(Pull())); const auto sent=s.panels.size();
    g_next=0;Tick();CHECK(s.panels.size()==sent); // one in flight, no reordering
    g_pending.front().sentAt=Now()-1001;g_next=0;Tick();CHECK(s.panels.size()==sent+1);
    CHECK(s.panels.back().seq==g_pending.front().packet.seq);
    ++test::world;g_next=0;Tick();CHECK(g_pending.empty() && !g_rows[1].actor.Alive());
    Reset(s,net::Role::Client);test::desk.state[0]=14;test::localGrab=test::canReturn=true;
    QueueCoverReturn(&test::desk,&test::part);
    CHECK(test::drops==1 && !test::localGrab && g_pending.size()==1);
    const auto restore=g_pending.front().packet;CHECK(restore.op==6 && restore.coverEid==3);
    Reset(s);test::desk.state[0]=14;test::canReturn=test::returnOk=test::held=true;
    Receive(restore,1);CHECK(test::returns==1 && test::ends==1 && test::desk.state[0]==15 && !test::part.live);
    Receive(restore,1);CHECK(test::returns==1);
    Reset(s);test::desk.state[0]=14;test::canReturn=test::returnOk=test::held=true;test::holder=2;
    Receive(restore,1);CHECK(test::returns==0 && g_ack[1]==0); // another peer owns the part
    g_returnAttempt[1].since=Now()-3001;Receive(restore,1);CHECK(g_ack[1]==restore.seq && test::part.live);
    Reset(s);test::desk.state[0]=14;test::returnOk=true;
    Receive(restore,1);CHECK(g_ack[1]==0);test::canReturn=true;Receive(restore,1);CHECK(test::returns==1);
    Reset(s,net::Role::Client);test::localGrab=test::canReturn=true;
    for (int i=0;i<64;++i) CHECK(Queue(Pull()));
    QueueCoverReturn(&test::desk,&test::part);CHECK(test::drops==0); // full queue cannot drop a held part
    Reset(s);test::coverIndex=2;test::desk.state[0]=11;test::localGrab=test::canReturn=test::returnOk=true;
    void* other=&test::part;SG::Call trOverlap{};trOverlap.object=&test::desk;trOverlap.locals=reinterpret_cast<uint8_t*>(&other);
    CHECK(CoverOverlap(trOverlap)==SG::Verdict::Cancel);
    CHECK(test::returns==1 && test::drops==1 && test::desk.state[0]==15 && !test::part.live);
    std::puts("PASS: production service panels: duplicate/race/rollback/retry/25 cells/authority/late actor/slot/world/prediction");
}
