#ifndef RADIO_SYNC_SOURCE
#define RADIO_SYNC_SOURCE "../../src/votv-coop/src/coop/world/radio_sync.cpp"
#endif
#include RADIO_SYNC_SOURCE
#include <cstdio>
#include <cstdlib>
#include <set>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace test {
struct Actor { int id=1;bool live=true;int serial=1; } radio;
std::array<Actor,300> extras;
Actor* ById(int i){return i==1?&radio:i>=2 && i<302?&extras[i-2]:nullptr;}
uint32_t world=1;
uint32_t serial=1;
std::set<uint64_t> active;
bool body=true,reach=true,ready=true,localTrack=true;
int applies=0;
int reads=0;
int completedApplies=0;
std::wstring keyOverride;
bool overrideKey=false,conflict=false;
void* uiRadio=&radio;
std::function<void()> onRead,onApply,onKey,onEid,onFind,onBody,onReach,onUi,onLocal;
void Fire(std::function<void()>& cb){auto run=std::move(cb);cb={};if(run)run();}
ue_wrap::radio_player::State state{123,40000000,1,1};
}
namespace ue_wrap::reflection {
void* ClassOf(void* a){return a;}
int32_t InternalIndexOf(void* a) { return a ? static_cast<test::Actor*>(a)->id : -1; }
int32_t SlotSerial(int32_t i) { auto* a=test::ById(i);return a?a->serial:0; }
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
bool IsInvocationActive(uint64_t id){return test::active.count(id)!=0;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}uint32_t Next(){return ++test::serial;}}
namespace ue_wrap::prop { std::wstring GetInteractableKeyString(void* a) {
    auto key=test::overrideKey?test::keyOverride:a==&test::radio?L"radio":L"radio"+std::to_wstring(static_cast<test::Actor*>(a)->id);
    test::Fire(test::onKey);return key;
} }
namespace ue_wrap::radio_player {
bool IsRadio(void* a) { return a && test::ById(static_cast<test::Actor*>(a)->id)==a; }
bool IsRadioClass(void* a){return IsRadio(a);}
bool HasLocalTrack(void*) {test::Fire(test::onLocal);return test::localTrack; }
void* UiRadio(void*) {test::Fire(test::onUi);return test::uiRadio; }
bool Read(void*,State& out,const std::function<bool()>& current) {
    if(current && !current())return false;
    ++test::reads;if (!test::ready) return false;out=test::state;test::Fire(test::onRead);return !current || current();
}
bool Apply(void*,const State& s,const std::function<bool()>& current){
    if((current && !current()) || !test::ready)return false;
    ++test::applies;test::state=s;test::Fire(test::onApply);
    if(current && !current())return false;++test::completedApplies;return true;
}
}
namespace coop::element::scan_hub { void Register(const Consumer&) {} }
namespace coop::net_pump { bool IsInAnnouncedWorld(void*) { return true; } }
namespace coop::prop_element_tracker {
element::ElementId GetPropElementIdForActor(void* a) {test::Fire(test::onEid);return static_cast<test::Actor*>(a)->id+41; }
void* FindLiveActorByKey(const std::wstring& k) {test::Fire(test::onFind);
    if(k==L"radio")return test::radio.live?&test::radio:nullptr;
    for(auto& a:test::extras)if(k==L"radio"+std::to_wstring(a.id)&&a.live)return &a;
    return nullptr;
}
}
namespace coop::remote_prop {
element::ElementId ResolveMirrorEidByActor(void*,bool) { return 42; }
void* ResolveLiveActorByEid(uint32_t id) { if(test::conflict)return &test::extras[0];auto* a=test::ById(int(id)-41);return a&&a->live?a:nullptr; }
}
namespace coop::roster_ledger { void RegisterPerSlotClear(PerSlotClearFn,void*) {} }
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session&,uint8_t,float) { return {}; }
bool IntentTarget::HasBody() const {test::Fire(test::onBody);return test::body; }
IntentSubject IntentTarget::Authorize(void*) const {test::Fire(test::onReach);return {test::reach ? IntentOutcome::Ok : IntentOutcome::OutOfReach}; }
}
using namespace coop;
using namespace coop::radio_sync;
void Reset(net::Session& s,net::Role role) {
    OnDisconnect();++test::world;s=net::Session{};s.mode=role;test::radio={};
    test::ready=test::reach=test::body=true;test::applies=0;test::state={123,40000000,1,1};
    test::reads=test::completedApplies=0;test::overrideKey=test::conflict=false;test::active.clear();
    test::uiRadio=&test::radio;
    test::onRead=test::onApply=test::onKey=test::onEid=test::onFind=test::onBody=test::onReach=test::onUi=test::onLocal={};
    for(int i=0;i<300;++i)test::extras[i]={i+2,true,1};
    players::Registry::Get().peer=1;
    Install(&s);ResetWorld();Match(nullptr,&test::radio);
}
void TickNow() { g_next=0;Tick(); }
void Adversarial();
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
    ue_wrap::script_gate::Call call{};call.object=&test::radio;call.function=&test::radio;call.depth=3;call.invocation=1;test::active.insert(1);
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
    Adversarial();return 0;
}
void Adversarial(){
    using namespace test;net::Session s,next;
    auto request=[] {Packet p{};net::WireKeyFromString(L"radio",p.key);p.elementId=42;p.op=1;p.seq=1;p.track=555;p.playing=1;p.mode=1;return p;};
    Reset(s,net::Role::Host);auto p=request();Receive(p,31);CHECK(applies==1&&g_ack[31]==1);
    CHECK(!s.radios.empty()&&s.radios.back().ack[31]==1);Receive(p,32);CHECK(applies==1);
    for(int i=0;i<32;++i){g_ack[i]=99;g_attempt[i]={5,6};}++world;ResetWorld();
    for(int i=0;i<32;++i)CHECK(g_ack[i]==0&&g_attempt[i].seq==0);
    for(int i=0;i<32;++i){g_ack[i]=99;g_attempt[i]={5,6};}OnDisconnect();
    for(int i=0;i<32;++i)CHECK(g_ack[i]==0&&g_attempt[i].seq==0);
    Reset(s,net::Role::Client);players::Registry::Get().peer=31;Dirty(&radio);
    for(auto& pair:g_dirty)pair.second.at=Now()-250;TickNow();CHECK(g_pending.size()==1);
    Packet ack=request();ack.op=2;ack.seq=0;ack.track=0;ack.playing=ack.mode=0;ack.ack[31]=g_seq;
    Receive(ack,0);CHECK(g_pending.empty());
    for(int kind=0;kind<6;++kind){Reset(s,net::Role::Host);p=request();switch(kind){
        case 0:p.key.len=32;break;case 1:p.key.data[0]=0;break;case 2:p.key.data[0]=char(255);break;
        case 3:net::WireKeyFromString(L"None",p.key);break;case 4:p.key.len=0;p.elementId=0;break;case 5:conflict=true;break;}
        Receive(p,1);CHECK(applies==0);
    }
    for(int kind=0;kind<3;++kind){Reset(s,net::Role::Host);Packet captured{};
        if(kind==0)onKey=[] {++radio.serial;};if(kind==1)onEid=[] {++world;};if(kind==2)onEid=[] {++serial;};
        CHECK(!Identity(&radio,captured)&&captured.key.len==0&&captured.elementId==0);
    }
    for(int kind=0;kind<6;++kind){Reset(s,net::Role::Client);Dirty(&radio);CHECK(!g_dirty.empty());
        switch(kind){case 0:next={};next.mode=net::Role::Client;Install(&next);break;case 1:++serial;break;
            case 2:++s.stops;break;case 3:s.mode=net::Role::Host;break;case 4:++world;break;case 5:OnDisconnect();break;}
        TickNow();CHECK(g_dirty.empty()&&g_pending.empty());
    }
    for(int kind=0;kind<4;++kind){Reset(s,net::Role::Host);p=request();switch(kind){
        case 0:s.ready=false;break;case 1:s.online[1]=false;break;case 2:s.generations[1]=0;break;case 3:s.linked=false;break;}
        Receive(p,1);CHECK(applies==0);
    }
    Reset(s,net::Role::Host);p=request();Receive(p,31);++s.generations[31];p.track=666;Receive(p,31);
    CHECK(applies==2&&state.track==666&&g_ack[31]==1);
    for(int kind=0;kind<4;++kind){Reset(s,net::Role::Host);p=request();onBody=[kind,&s]{
        if(kind==0)++s.generations[1];if(kind==1)++serial;if(kind==2)++world;if(kind==3)OnDisconnect();};
        Receive(p,1);CHECK(applies==0&&g_ack[1]==0);
    }
    for(int kind=0;kind<3;++kind){Reset(s,net::Role::Host);p=request();onReach=[kind,&s]{
        if(kind==0)++radio.serial;if(kind==1)++s.generations[1];if(kind==2)++serial;};
        Receive(p,1);CHECK(applies==0&&g_ack[1]==0);
    }
    Reset(s,net::Role::Host);p=request();onApply=[&]{Receive(p,1);};Receive(p,1);CHECK(applies==1&&g_ack[1]==1);
    Reset(s,net::Role::Host);p=request();onApply=[&]{++s.generations[1];Receive(p,1);};Receive(p,1);
    CHECK(applies==1&&g_ack[1]==0&&!g_receiving[1]);Receive(p,1);CHECK(applies==2&&g_ack[1]==1);
    Reset(s,net::Role::Host);p=request();onApply=[&]{Install(&next);};Receive(p,1);CHECK(applies==1&&g_ack[1]==0);
    Reset(s,net::Role::Host);s.beforeSend=[&]{state.track=777;Publish(&radio);};Publish(&radio);
    CHECK(g_sent.at(L"radio").packet.track==777&&s.radios.size()==2);
    Reset(s,net::Role::Host);s.beforeSend=[] {OnDisconnect();};Publish(&radio);CHECK(g_sent.empty());
    Reset(s,net::Role::Host);onRead=[] {OnDisconnect();};TickNow();CHECK(g_radios.empty()&&g_sent.empty());
    Reset(s,net::Role::Host);onRead=[] {TickNow();};TickNow();CHECK(s.radios.size()==1&&!g_ticking&&reads==1);
    Reset(s,net::Role::Host);onRead=[&]{OnDisconnect();Install(&next);Match(nullptr,&radio);TickNow();};TickNow();CHECK(!g_ticking&&next.radios.empty());
    Reset(s,net::Role::Host);for(auto& a:extras)Match(nullptr,&a);CHECK(g_radios.size()==256&&g_radios.count(L"radio"));
    for(auto& a:extras)Publish(&a);CHECK(g_sent.size()==256&&s.radios.size()==300);
    Reset(s,net::Role::Client);for(auto& a:extras)Dirty(&a);CHECK(g_dirty.size()==256);
    Reset(s,net::Role::Client);Packet canonical=request();canonical.op=0;canonical.seq=0;
    onApply=[&]{auto newer=canonical;newer.track=888;Receive(newer,0);};Receive(canonical,0);
    CHECK(state.track==888&&g_canonical.at(L"radio").packet.track==888&&completedApplies==1);
    Reset(s,net::Role::Client);Dirty(&radio);for(auto& pair:g_dirty)pair.second.at=Now()-250;
    s.beforeSend=[&]{g_pending.clear();auto later=request();later.seq=2;g_pending.push_back({later,0,++g_revision});};TickNow();
    CHECK(g_pending.size()==1&&g_pending.front().p.seq==2&&g_pending.front().at==0);
    ack.ack[1]=99;Receive(ack,0);CHECK(g_pending.size()==1); // cannot acknowledge a request never offered
    Reset(s,net::Role::Host);ready=false;p=request();p.seq=2;Receive(p,1);const auto since=g_attempt[1].since;
    p.seq=1;Receive(p,1);CHECK(g_attempt[1].seq==2&&g_attempt[1].since==since);
    p.seq=2;g_attempt[1].since=Now()-15001;Receive(p,1);CHECK(g_ack[1]==2);
    Reset(s,net::Role::Client);SG::Call outer{};outer.object=&radio;outer.function=&extras[0];outer.invocation=11;
    active.insert(11);UiPre(outer);TickNow();CHECK(Finish(outer)==SG::Verdict::Run);
    auto unrelated=outer;unrelated.object=&extras[0];CHECK(Finish(unrelated)==SG::Verdict::Cancel);
    auto inner=outer;inner.invocation=12;active.insert(12);UiPre(inner);UiPost(inner);active.erase(12);
    CHECK(Finish(outer)==SG::Verdict::Run);UiPost(outer);active.erase(11);CHECK(Finish(outer)==SG::Verdict::Cancel);
    active.insert(11);UiPre(outer);active.erase(11);CHECK(Finish(outer)==SG::Verdict::Cancel);
    for(int kind=0;kind<4;++kind){Reset(s,net::Role::Client);active.insert(11);UiPre(outer);
        if(kind==0)++radio.serial;if(kind==1)++extras[0].serial;if(kind==2)++serial;if(kind==3)++world;
        UiPost(outer);CHECK(g_dirty.empty());
    }
    Reset(s,net::Role::Client);active.insert(11);UiPre(outer);onUi=[] {OnDisconnect();};UiPost(outer);CHECK(g_dirty.empty());
    Reset(s,net::Role::Client);active.insert(11);UiPre(outer);uiRadio=&extras[0];UiPost(outer);CHECK(g_dirty.empty());
    Reset(s,net::Role::Client);onLocal=[] {OnDisconnect();};CHECK(Finish(outer)==SG::Verdict::Run);
    Reset(s,net::Role::Client);active.insert(11);OpenPre(outer);++serial;OpenPost(outer);CHECK(g_dirty.empty());
    Packet huge=request();huge.ticks=kMaxTicks-100;auto playback=State(huge,UINT64_MAX);
    CHECK(playback.ticks<=kMaxTicks&&playback.ticks>=huge.ticks);
    std::puts("PASS radio R01-R45: 32 peers, scopes, authority, generation/reentry, owned maps/queues/UI invocations, caps and time bounds");
}
