#include "coop/world/radio_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/wire_key_util.h"
#include "coop/element/intent_authority.h"
#include "coop/element/object_scan_hub.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/remote_prop.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/world/radio_player.h"
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <functional>
#include <unordered_map>
#include <vector>

namespace coop::radio_sync {
namespace {
namespace R=ue_wrap::reflection;
namespace Radio=ue_wrap::radio_player;
namespace SG=ue_wrap::script_gate;
using Packet=net::RadioStatePayload;
static_assert(sizeof(Packet{}.ack)/sizeof(uint32_t)==net::kMaxPeers);
constexpr size_t kMaxRadios=256, kMaxPending=64;
constexpr int64_t kMaxTicks=864000000000ll;
net::Session* g_session=nullptr;
bool g_installed=false,g_ticking=false;
std::array<bool,net::kMaxPeers> g_receiving{}; // admission survives callback-driven resets
uint32_t g_world=0,g_seq=0,g_offered=0;
uint64_t g_next=0,g_epoch=0,g_revision=0;
struct Scope {
    net::Session* session=nullptr;
    uint32_t serial=0,world=0;
    uint64_t stopped=0,epoch=0;
    net::Role role=net::Role::Host;
};
Scope g_scope;
Scope Capture() {
    return {g_session,net::session_serial::Current(),ue_wrap::world_identity::Generation(),
        g_session?g_session->StopCount():0,g_epoch,g_session?g_session->role():net::Role::Host};
}
bool Same(const Scope& s) {
    return s.session==g_session && s.serial==net::session_serial::Current() &&
        s.world==ue_wrap::world_identity::Generation() && s.epoch==g_epoch &&
        (!g_session || (s.stopped==g_session->StopCount() && s.role==g_session->role()));
}
bool Current(const Scope& s) { return Same(s) && s.session && s.session->running() && s.session->connected(); }
bool Client() { return Current(g_scope) && g_scope.role==net::Role::Client; }
roster_ledger::PerSlotState<uint32_t> g_ack,g_generation;
struct Attempt { uint32_t seq=0;uint64_t since=0; };
roster_ledger::PerSlotState<Attempt> g_attempt;
struct Row { ue_wrap::CachedObjRef actor; Packet packet{}; uint64_t at=0,revision=0; };
using Rows=std::unordered_map<std::wstring,Row>;
Rows g_radios,g_canonical,g_dirty,g_sent;
struct Pending { Packet p{}; uint64_t at=0,revision=0; };
std::deque<Pending> g_pending;
struct UserCall { uint64_t invocation=0;Scope scope;ue_wrap::CachedObjRef widget,function,radio;bool permission=false; };
std::array<UserCall,64> g_users;
uint64_t Now() { using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count(); }
bool After(uint32_t a,uint32_t b) { return int32_t(a-b)>0; }
uint64_t Elapsed(uint64_t now,uint64_t then) { return now>=then?now-then:0; }
void Clear() {
    ++g_epoch;g_seq=g_offered=0;g_next=0;g_users={};
    g_radios.clear();g_canonical.clear();g_dirty.clear();g_sent.clear();g_pending.clear();
    for (int i=0;i<net::kMaxPeers;++i) { g_ack[i]=0;g_attempt[i]={};g_generation[i]=0; }
}
void ResetWorld() {
    if (Same(g_scope)) return;
    Clear();g_scope=Capture();g_world=g_scope.world;
}
bool KeyOk(const net::WireKey& key) {
    if (!key.len || key.len>sizeof(key.data)) return false;
    for (size_t i=0;i<key.len;++i) if (static_cast<unsigned char>(key.data[i])==0 || static_cast<unsigned char>(key.data[i])>127) return false;
    return net::StringFromWireKey(key)!=L"None";
}
bool KeyOk(const std::wstring& key) {
    if (key.empty() || key==L"None" || key.size()>sizeof(net::WireKey{}.data)) return false;
    for (auto c:key) if (c<=0 || c>127) return false;
    return true;
}
std::wstring Token(const Packet& p) { return p.key.len ? net::StringFromWireKey(p.key) : L"#"+std::to_wstring(p.elementId); }
bool Valid(const Packet& p) {
    if ((p.key.len && !KeyOk(p.key)) || !(p.key.len || p.elementId)) return false;
    if (p.op==2) return !p.seq && !p.track && !p.ticks && !p.playing && !p.mode;
    return p.track && p.playing<=1 && p.mode<=3 && p.ticks>=0 && p.ticks<=kMaxTicks &&
        ((p.op==0 && !p.seq) || (p.op==1 && p.seq));
}
bool Identity(void* a,Packet& p,const std::function<bool()>& allowed={}) {
    const auto scope=g_scope;
    ue_wrap::CachedObjRef ref;ref.Set(a);
    const auto current=[&] {return Current(scope) && ref.Get() && (!allowed || allowed());};
    if (!current()) return false;
    Packet next=p;next.key={};next.elementId=0;
    const auto key=ue_wrap::prop::GetInteractableKeyString(a);
    if (!current()) return false;
    if (KeyOk(key)) net::WireKeyFromString(key,next.key);
    else if (!key.empty() && key!=L"None") return false;
    auto eid=prop_element_tracker::GetPropElementIdForActor(a);
    if (!current()) return false;
    if (eid==element::kInvalidId) eid=remote_prop::ResolveMirrorEidByActor(a);
    if (!current()) return false;
    next.elementId=eid==element::kInvalidId?0:eid;
    if (!next.key.len && !next.elementId) return false;
    p=next;return true;
}
void* Resolve(const Packet& p,const std::function<bool()>& allowed={}) {
    const auto scope=g_scope;
    const auto current=[&] {return Current(scope) && (!allowed || allowed());};
    if (!current()) return nullptr;
    void* keyed=p.key.len?prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key)):nullptr;
    if (!current()) return nullptr;
    ue_wrap::CachedObjRef keyRef;keyRef.Set(keyed);
    void* numbered=p.elementId?remote_prop::ResolveLiveActorByEid(p.elementId):nullptr;
    if (!current() || (keyed && !keyRef.Get()) || (keyed && numbered && keyed!=numbered)) return nullptr;
    void* a=keyed?keyed:numbered;
    ue_wrap::CachedObjRef ref;ref.Set(a);
    if (!a || !Radio::IsRadio(a) || !current() || !ref.Get() ||
        ue_wrap::world_identity::WorldOf(a)!=ue_wrap::world_identity::CurrentWorld()) return nullptr;
    Packet actual{};
    if (!Identity(a,actual,current) || !current() || !ref.Get()) return nullptr;
    if ((p.key.len && (actual.key.len!=p.key.len || std::memcmp(actual.key.data,p.key.data,p.key.len))) ||
        (p.elementId && actual.elementId && p.elementId!=actual.elementId)) return nullptr;
    return a;
}
Radio::State State(const Packet& p,uint64_t elapsed=0) {
    const auto room=static_cast<uint64_t>((kMaxTicks-p.ticks)/10000);
    const auto delta=elapsed>room?room:elapsed;
    return {p.track,p.ticks+(p.playing?static_cast<int64_t>(delta)*10000:0),p.playing,p.mode};
}
bool Read(void* a,Packet& p,const std::function<bool()>& allowed={}) {
    const auto scope=g_scope;
    ue_wrap::CachedObjRef actor;actor.Set(a);
    const auto current=[&] {return Current(scope) && actor.Get() && (!allowed || allowed());};
    Radio::State s{};Packet next{};
    if (!current() || !Radio::Read(a,s,current) || !current() || !Identity(a,next,current)) return false;
    next.track=s.track;next.ticks=s.ticks;next.playing=s.playing;next.mode=s.mode;
    for (int i=0;i<net::kMaxPeers;++i) next.ack[i]=g_ack[i];
    if (!current() || !Valid(next)) return false;
    p=next;return true;
}
void Publish(void* a,int slot=-1,const std::function<bool()>& allowed={}) {
    const auto scope=g_scope;ue_wrap::CachedObjRef actor;actor.Set(a);
    const auto current=[&] {return Current(scope) && scope.role==net::Role::Host && actor.Get() && (!allowed || allowed());};
    Packet p{};if (!current() || !Read(a,p,current)) return;
    const auto id=Token(p);const auto now=Now();
    auto found=g_sent.find(id);const Row prev=found==g_sent.end()?Row{}:found->second;
    if (slot<0 && prev.actor.Is(a) && Elapsed(now,prev.at)<2000 && p.track==prev.packet.track &&
        p.playing==prev.packet.playing && p.mode==prev.packet.mode && std::memcmp(p.ack,prev.packet.ack,sizeof(p.ack))==0) return;
    const bool cache=slot<0 && (found!=g_sent.end() || g_sent.size()<kMaxRadios);
    const uint64_t revision=++g_revision;
    if (cache) g_sent[id].revision=revision;
    const bool ok=slot<0?scope.session->SendReliable(net::ReliableKind::RadioState,&p,sizeof(p)):
        scope.session->SendReliableToSlot(slot,net::ReliableKind::RadioState,&p,sizeof(p));
    if (!current() || !ok || !cache) return;
    found=g_sent.find(id);
    if (found!=g_sent.end() && found->second.revision==revision) found->second={actor,p,now,revision};
}
void Reply(void* a,const Packet& request,uint8_t sender,const std::function<bool()>& allowed) {
    const auto scope=g_scope;
    const auto current=[&] {return Current(scope) && (!allowed || allowed());};
    if (!current()) return;
    Packet p{};
    if (!a || !Read(a,p,current)) {
        if (!current()) return;
        p.key=request.key;p.elementId=request.elementId;p.op=2;
        for (int i=0;i<net::kMaxPeers;++i) p.ack[i]=g_ack[i];
    }
    if (current()) scope.session->SendReliableToSlot(sender,net::ReliableKind::RadioState,&p,sizeof(p));
}
bool Predicted(const std::wstring& id) {
    if (g_dirty.count(id)) return true;
    for (const auto& q:g_pending) if (Token(q.p)==id) return true;
    return false;
}
void Paint(const std::wstring& id,Row row) {
    if (Predicted(id)) return;
    const auto scope=g_scope;
    const auto current=[&] {const auto it=g_canonical.find(id);return Current(scope) && !Predicted(id) &&
        it!=g_canonical.end() && it->second.revision==row.revision;};
    void* a=Resolve(row.packet,current);ue_wrap::CachedObjRef actor;actor.Set(a);
    const auto owned=[&] {return current() && actor.Get();};
    if (a && owned()) Radio::Apply(a,State(row.packet,Elapsed(Now(),row.at)),owned);
}
bool Ready() { return true; }
bool MatchType(void* a) {return a && Radio::IsRadioClass(R::ClassOf(a));}
uint64_t Classes() { return ue_wrap::object_index::ClassSetVersion(); }
void Begin(void*,bool) { ResetWorld(); }
void Match(void*,void* a) {
    ResetWorld();const auto scope=g_scope;ue_wrap::CachedObjRef ref;ref.Set(a);
    if (!Current(scope) || !a || R::NameStartsWith(R::NameOf(a),L"Default__") ||
        !ref.Get() || ue_wrap::world_identity::WorldOf(a)!=ue_wrap::world_identity::CurrentWorld()) return;
    Packet p{};if (!Identity(a,p) || !Current(scope) || !ref.Get()) return;
    const auto id=Token(p);
    if (g_radios.size()>=kMaxRadios && !g_radios.count(id)) return;
    g_radios[id]={ref,{},0,++g_revision};
}
size_t Complete(void*,bool,uint32_t) { return g_radios.size(); }
void Dirty(void* a) {
    ResetWorld();const auto scope=g_scope;ue_wrap::CachedObjRef ref;ref.Set(a);
    if (!Client() || !a || !net_pump::IsInAnnouncedWorld(a) || !Current(scope) || !ref.Get() ||
        !Radio::IsRadio(a) || !Current(scope) || !ref.Get()) return;
    Packet p{};if (!Identity(a,p) || !Current(scope) || !ref.Get()) return;
    const auto id=Token(p);
    if (g_dirty.size()>=kMaxRadios && !g_dirty.count(id)) return;
    g_dirty[id]={ref,{},Now(),++g_revision};
}
void PruneUsers() {
    for (auto& u:g_users) if (u.invocation && (!Current(u.scope) || !u.widget.Get() || !u.function.Get() ||
        !SG::IsInvocationActive(u.invocation))) u={};
}
SG::Verdict Remember(const SG::Call& c,bool permission) {
    ResetWorld();PruneUsers();
    if (!Client() || c.fromOurCode || !c.invocation || !c.object || !c.function) return SG::Verdict::Run;
    UserCall note;note.invocation=c.invocation;note.scope=g_scope;note.permission=permission;
    note.widget.Set(c.object);note.function.Set(c.function);
    void* radio=permission?Radio::UiRadio(c.object):c.object;
    if (!Current(note.scope) || !note.widget.Get() || !note.function.Get()) return SG::Verdict::Run;
    note.radio.Set(radio);if (!note.radio.Get()) return SG::Verdict::Run;
    for (auto& u:g_users) if (!u.invocation) {
        u=std::move(note);break;
    }
    return SG::Verdict::Run;
}
SG::Verdict OpenPre(const SG::Call& c) {return Remember(c,false);}
SG::Verdict UiPre(const SG::Call& c) {return Remember(c,true);}
void OpenPost(const SG::Call& c) {
    ResetWorld();
    for (auto& slot:g_users) if (slot.invocation && !slot.permission && slot.invocation==c.invocation &&
        slot.widget.Raw()==c.object && slot.function.Raw()==c.function) {
        const UserCall u=slot;slot={};
        if (Current(u.scope) && u.widget.Get() && u.function.Get()) Dirty(c.object);
        return;
    }
}
void UiPost(const SG::Call& c) {
    ResetWorld();
    for (auto& slot:g_users) if (slot.invocation && slot.invocation==c.invocation &&
        slot.widget.Raw()==c.object && slot.function.Raw()==c.function) {
        const UserCall u=slot;slot={};
        if (!Current(u.scope) || !u.widget.Get() || !u.function.Get() || !u.radio.Get()) return;
        void* a=Radio::UiRadio(c.object);
        if (Current(u.scope) && u.widget.Get() && u.function.Get() && u.radio.Is(a)) Dirty(a);
        return;
    }
}
SG::Verdict Finish(const SG::Call& c) {
    ResetWorld();PruneUsers();
    if (!Client() || c.fromOurCode) return SG::Verdict::Run;
    for (const auto& u:g_users) if (u.invocation && u.permission && u.radio.Is(c.object)) return SG::Verdict::Run;
    const auto scope=g_scope;ue_wrap::CachedObjRef actor;actor.Set(c.object);
    if (!actor.Get() || !net_pump::IsInAnnouncedWorld(c.object) || !Current(scope) || !actor.Get()) return SG::Verdict::Run;
    const bool local=Radio::HasLocalTrack(c.object);
    return Current(scope) && actor.Get() && local ? SG::Verdict::Cancel : SG::Verdict::Run;
}
} // namespace
void Install(net::Session* session) {
    g_session=session;ResetWorld();if (g_installed) return;g_installed=true;
    element::scan_hub::Register({"radio",nullptr,&Ready,&MatchType,&Begin,&Match,&Complete,2,&Classes});
    SG::WatchClassName(L"prop_radio_internet_C",L"openLink",0x52410001,&OpenPre,&OpenPost);
    SG::WatchClassName(L"prop_radio_internet_C",L"fin",0x52410002,&Finish,nullptr);
    const wchar_t* buttons[]={
        L"BndEvt__umg_onlineRadio_Button_play_K2Node_ComponentBoundEvent_0_OnButtonClickedEvent__DelegateSignature",
        L"BndEvt__umg_onlineRadio_Button_stop_K2Node_ComponentBoundEvent_1_OnButtonClickedEvent__DelegateSignature",
        L"BndEvt__umg_onlineRadio_Button_skip_K2Node_ComponentBoundEvent_5_OnButtonClickedEvent__DelegateSignature",
        L"BndEvt__umg_onlineRadio_Button_104_K2Node_ComponentBoundEvent_2_OnButtonClickedEvent__DelegateSignature"};
    for (int i=0;i<4;++i) SG::WatchClassName(L"ui_radioInterface_C",buttons[i],0x52410010+i,&UiPre,&UiPost);
}
void Tick() {
    ResetWorld();if (!Current(g_scope) || g_ticking) return;
    struct Busy {Busy(){g_ticking=true;}~Busy(){g_ticking=false;}} busy;
    const auto scope=g_scope;
    SG::ResolvePendingNames();if (!Current(scope)) return;
    const auto now=Now();if (now<g_next) return;g_next=now+200;PruneUsers();
    for (auto it=g_radios.begin();it!=g_radios.end();) {
        if (!it->second.actor.Alive()) {g_sent.erase(it->first);it=g_radios.erase(it);}else ++it;
    }
    for (auto it=g_sent.begin();it!=g_sent.end();) {
        if (!it->second.actor.Get()) it=g_sent.erase(it);else ++it;
    }
    if (!Client()) {
        const std::vector<Rows::value_type> work(g_radios.begin(),g_radios.end());
        for (const auto& pair:work) {
            if (!Current(scope)) return;
            const auto current=[&] {auto it=g_radios.find(pair.first);return Current(scope) &&
                it!=g_radios.end() && it->second.revision==pair.second.revision;};
            if (current()) Publish(pair.second.actor.Get(),-1,current);
        }
        return;
    }
    const std::vector<Rows::value_type> work(g_dirty.begin(),g_dirty.end());
    for (const auto& pair:work) {
        if (!Current(scope)) return;
        const auto current=[&] {auto it=g_dirty.find(pair.first);return Current(scope) &&
            it!=g_dirty.end() && it->second.revision==pair.second.revision;};
        if (!current()) continue;
        void* a=pair.second.actor.Get();
        if (!a || Elapsed(now,pair.second.at)>15000) {g_dirty.erase(pair.first);continue;}
        Packet p{};
        if (Elapsed(now,pair.second.at)<200 || g_pending.size()>=kMaxPending || !Read(a,p,current) || !current()) continue;
        p.op=1;if (++g_seq==0) ++g_seq;p.seq=g_seq;
        g_pending.push_back({p,0,++g_revision});g_dirty.erase(pair.first);
    }
    if (!Current(scope)) return;
    if (!g_pending.empty()) {
        const Pending q=g_pending.front();
        if (!q.at || Elapsed(now,q.at)>=1000) {
            g_offered=q.p.seq;
            const bool sent=scope.session->SendReliable(net::ReliableKind::RadioState,&q.p,sizeof(q.p));
            if (!Current(scope)) return;
            if (sent && !g_pending.empty() && g_pending.front().revision==q.revision) g_pending.front().at=now;
        }
    }
    const std::vector<Rows::value_type> canonical(g_canonical.begin(),g_canonical.end());
    for (const auto& pair:canonical) {if (!Current(scope)) return;Paint(pair.first,pair.second);}
}
void Receive(const Packet& incoming,uint8_t sender) {
    ResetWorld();const auto scope=g_scope;
    if (!Current(scope) || !Valid(incoming) || sender>=net::kMaxPeers) return;
    const Packet p=incoming;
    if (p.op!=1) {
        if (!Client() || sender!=0) return;
        const auto me=players::Registry::Get().LocalPeerId();if (me>=net::kMaxPeers) return;
        if (After(p.ack[me],g_offered)) return;
        while (!g_pending.empty() && !After(g_pending.front().p.seq,p.ack[me])) g_pending.pop_front();
        if (p.op==2) return;
        const auto id=Token(p);if (g_canonical.size()>=kMaxRadios && !g_canonical.count(id)) return;
        Row row{};row.packet=p;row.at=Now();row.revision=++g_revision;
        g_canonical[id]=row;Paint(id,std::move(row));return;
    }
    if (scope.role!=net::Role::Host || !sender || g_receiving[sender]) return;
    const auto generation=scope.session->peerGenerationForSlot(sender);
    const auto current=[&] {return Current(scope) && scope.role==net::Role::Host && generation &&
        scope.session->peerGenerationForSlot(sender)==generation && scope.session->IsSlotConnected(sender) &&
        scope.session->IsSlotWorldReady(sender);};
    if (!current()) return;
    struct Busy {uint8_t slot;Busy(uint8_t s):slot(s){g_receiving[slot]=true;}~Busy(){g_receiving[slot]=false;}} busy(sender);
    if (g_generation[sender]!=generation) {g_generation[sender]=generation;g_ack[sender]=0;g_attempt[sender]={};}
    void* a=Resolve(p,current);if (!current()) return;
    ue_wrap::CachedObjRef actor;actor.Set(a);
    if (!After(p.seq,g_ack[sender])) {Reply(actor.Get(),p,sender,current);return;}
    Attempt attempt=g_attempt[sender];const auto now=Now();
    if (attempt.seq && After(attempt.seq,p.seq)) return;
    if (attempt.seq!=p.seq) {attempt={p.seq,now};g_attempt[sender]=attempt;}
    const auto target=element::IntentTarget::ForClientIntent(*scope.session,sender,500.f);
    const bool hasBody=target.HasBody();if (!current()) return;
    element::IntentSubject verdict{};
    if (a && actor.Get() && hasBody) verdict=target.Authorize(a);
    if (!current() || (a && !actor.Get())) return;
    const bool rejected=a && hasBody && !verdict;
    const auto owned=[&] {return current() && actor.Get();};
    const bool applied=a && hasBody && !rejected && Radio::Apply(a,State(p),owned);
    if (!current() || (a && !actor.Get())) return;
    if (!applied && !rejected && Elapsed(now,attempt.since)<15000) return;
    g_ack[sender]=p.seq;g_attempt[sender]={};
    if (a) Publish(a,-1,owned);
    if (current()) Reply(actor.Get(),p,sender,current);
}
void OnDisconnect() {g_session=nullptr;Clear();g_scope=Capture();g_world=g_scope.world;}
} // namespace coop::radio_sync
