#include "../../src/votv-coop/src/coop/world/radio_sync.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace test {
struct Actor { int id=1;bool live=true;int serial=1; } radio;
uint32_t world=1;
bool body=true,reach=true,ready=true,localTrack=true;
int applies=0;
ue_wrap::radio_player::State state{123,40000000,1,1};
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a) { return a ? static_cast<test::Actor*>(a)->id : -1; }
int32_t SlotSerial(int32_t) { return test::radio.serial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLiveByIndex(void* a,int32_t i) { return a && static_cast<test::Actor*>(a)->live && InternalIndexOf(a)==i; }
const FName& NameOf(void*) { static FName n{1,0};return n; }
bool NameStartsWith(const FName&,const wchar_t*) { return false; }
}
namespace ue_wrap::world_identity {
void* CurrentWorld() { return reinterpret_cast<void*>(uintptr_t(test::world)); }
void* WorldOf(void*) { return CurrentWorld(); }
uint32_t Generation() { return test::world; }
}
namespace ue_wrap::object_index { uint64_t ClassSetVersion() { return 1; } }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn) { return true; }
void ResolvePendingNames() {}
}
namespace ue_wrap::prop { std::wstring GetInteractableKeyString(void*) { return L"radio"; } }
namespace ue_wrap::radio_player {
bool IsRadio(void* a) { return a==&test::radio; }
bool HasLocalTrack(void*) { return test::localTrack; }
void* UiRadio(void*) { return &test::radio; }
bool Read(void*,State& out) { if (!test::ready) return false;out=test::state;return true; }
bool Apply(void*,const State& s) { if (!test::ready) return false;++test::applies;test::state=s;return true; }
}
namespace coop::element::scan_hub { void Register(const Consumer&) {} }
namespace coop::net_pump { bool IsInAnnouncedWorld(void*) { return true; } }
namespace coop::prop_element_tracker {
element::ElementId GetPropElementIdForActor(void*) { return 42; }
void* FindLiveActorByKey(const std::wstring& k) { return k==L"radio" && test::radio.live ? &test::radio : nullptr; }
}
namespace coop::remote_prop {
element::ElementId ResolveMirrorEidByActor(void*,bool) { return 42; }
void* ResolveLiveActorByEid(uint32_t id) { return id==42 && test::radio.live ? &test::radio : nullptr; }
}
namespace coop::roster_ledger { void RegisterPerSlotClear(PerSlotClearFn,void*) {} }
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session&,uint8_t,float) { return {}; }
bool IntentTarget::HasBody() const { return test::body; }
IntentSubject IntentTarget::Authorize(void*) const { return {test::reach ? IntentOutcome::Ok : IntentOutcome::OutOfReach}; }
}
using namespace coop;
using namespace coop::radio_sync;
void Reset(net::Session& s,net::Role role) {
    OnDisconnect();++test::world;s=net::Session{};s.mode=role;test::radio={};
    test::ready=test::reach=test::body=true;test::applies=0;test::state={123,40000000,1,1};
    Install(&s);ResetWorld();Match(nullptr,&test::radio);
}
void TickNow() { g_next=0;Tick(); }
int main() {
    using ue_wrap::radio_player::TrackId;
    CHECK(TrackId(L"C:\\Alice\\radio\\album\\\u0442\u0440\u0435\u043a.ogg")==TrackId(L"D:/Bob/RADIO/album/\u0442\u0440\u0435\u043a.ogg"));
    CHECK(TrackId(L"radio/a/song.ogg")!=TrackId(L"radio/b/song.ogg"));
    CHECK(TrackId(L"C:/radio/\u0100.ogg")!=TrackId(L"C:/radio/\u0200.ogg"));
    net::Session s;Reset(s,net::Role::Host);TickNow();CHECK(s.radios.size()==1);
    const auto canonical=s.radios.back();TickNow();CHECK(s.radios.size()==1);
    test::state.playing=0;TickNow();CHECK(s.radios.back().playing==0 && s.radios.size()==2);
    auto p=canonical;p.op=1;p.seq=1;p.track=456;p.playing=0;
    Receive(p,1);CHECK(test::state.track==456 && !test::state.playing && g_ack[1]==1);
    const auto applied=test::applies;Receive(p,1);CHECK(test::applies==applied);
    p.seq=2;p.playing=1;test::ready=false;Receive(p,1);CHECK(g_ack[1]==1);
    test::ready=true;Receive(p,1);CHECK(g_ack[1]==2 && test::state.playing);
    p.seq=3;test::body=false;Receive(p,1);CHECK(g_ack[1]==2);
    test::body=true;test::reach=false;Receive(p,1);CHECK(g_ack[1]==3 && test::applies==applied+1);
    p.seq=4;p.mode=4;Receive(p,1);CHECK(g_ack[1]==3);
    p.mode=1;test::ready=false;Receive(p,1); // unreachable with no host track still acknowledges
    CHECK(g_ack[1]==4 && s.radios.back().op==2);
    p.seq=5;test::reach=true;Receive(p,1);CHECK(g_ack[1]==4);
    g_attempt[1].since=Now()-15001;Receive(p,1);CHECK(g_ack[1]==5 && s.radios.back().op==2);
    Receive(p,1);CHECK(s.radios.back().op==2); // retry lost ack without a readable host player
    Reset(s,net::Role::Client);Receive(canonical,2);CHECK(test::applies==0);
    Receive(canonical,0);CHECK(test::applies==1);
    test::state={999,20000000,0,2};Dirty(&test::radio);TickNow();CHECK(s.radios.empty());
    for (auto& row:g_dirty) row.second.at=Now()-250;
    s.sendOk=false;TickNow();CHECK(g_pending.size()==1 && s.radios.empty());
    Receive(canonical,0);CHECK(test::state.track==999); // old row cannot erase local input
    s.sendOk=true;TickNow();CHECK(s.radios.size()==1 && s.radios.back().track==999);
    g_pending.front().at=Now()-1001;TickNow();CHECK(s.radios.size()==2 && s.radios[0].seq==s.radios[1].seq);
    auto ack=canonical;ack.ack[1]=1;ack.track=999;ack.playing=0;Receive(ack,0);CHECK(g_pending.empty());
    ue_wrap::script_gate::Call call{};call.object=&test::radio;call.depth=3;
    CHECK(Finish(call)==SG::Verdict::Cancel);UiPre(call);CHECK(Finish(call)==SG::Verdict::Run);UiPost(call);
    CHECK(Finish(call)==SG::Verdict::Cancel);
    test::localTrack=false;CHECK(Finish(call)==SG::Verdict::Run);test::localTrack=true;
    test::state.track=777;Dirty(&test::radio);
    for (auto& row:g_dirty) row.second.at=Now()-250;
    TickNow();CHECK(g_pending.size()==1);
    auto onlyAck=canonical;onlyAck.op=2;onlyAck.seq=0;onlyAck.track=0;onlyAck.ticks=0;onlyAck.playing=onlyAck.mode=0;onlyAck.ack[1]=g_seq;
    Receive(onlyAck,2);CHECK(g_pending.size()==1);
    Receive(onlyAck,0);CHECK(g_pending.empty());
    ++test::world;TickNow();CHECK(g_canonical.empty() && g_dirty.empty() && g_pending.empty());
    std::puts("PASS: production radio play/pause/track, targeted authority, async retry, duplicate fence, prediction, unicode, world");
}
