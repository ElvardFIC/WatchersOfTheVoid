#include "coop/world/radio_sync.h"
#include "coop/net/session.h"
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
#include <chrono>
#include <cstring>
#include <deque>
#include <unordered_map>

namespace coop::radio_sync {
namespace {
namespace R=ue_wrap::reflection;
namespace Radio=ue_wrap::radio_player;
namespace SG=ue_wrap::script_gate;
using Packet=net::RadioStatePayload;
net::Session* g_session=nullptr;
bool g_installed=false;
uint32_t g_world=0,g_seq=0;
uint64_t g_next=0;
int g_userDepth=0;
roster_ledger::PerSlotState<uint32_t> g_ack;
struct Attempt { uint32_t seq=0;uint64_t since=0; };
roster_ledger::PerSlotState<Attempt> g_attempt;
struct Row { ue_wrap::CachedObjRef actor; Packet packet{}; uint64_t at=0; };
std::unordered_map<std::wstring,Row> g_radios,g_canonical,g_dirty,g_sent;
struct Pending { Packet p{}; uint64_t at=0; };
std::deque<Pending> g_pending;
uint64_t Now() { using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count(); }
bool Client() { return g_session && g_session->connected() && g_session->role()==net::Role::Client; }
bool After(uint32_t a,uint32_t b) { return int32_t(a-b)>0; }
std::wstring Token(const Packet& p) { return p.key.len ? net::StringFromWireKey(p.key) : L"#"+std::to_wstring(p.elementId); }
void ResetWorld() {
    const auto gen=ue_wrap::world_identity::Generation(); if (gen==g_world) return;
    g_world=gen;g_seq=0;g_next=0;g_userDepth=0;
    g_radios.clear();g_canonical.clear();g_dirty.clear();g_sent.clear();g_pending.clear();
    for (int i=0;i<4;++i) { g_ack[i]=0;g_attempt[i]={}; }
}
bool Identity(void* a,Packet& p) {
    const auto key=ue_wrap::prop::GetInteractableKeyString(a);
    if (!key.empty() && key!=L"None" && key.size()<=31) net::WireKeyFromString(key,p.key);
    auto eid=prop_element_tracker::GetPropElementIdForActor(a);
    if (eid==element::kInvalidId) eid=remote_prop::ResolveMirrorEidByActor(a);
    p.elementId=eid==element::kInvalidId ? 0 : eid;
    return p.key.len || p.elementId;
}
void* Resolve(const Packet& p) {
    void* a=p.key.len ? prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key)) : nullptr;
    if (!a && p.elementId) a=remote_prop::ResolveLiveActorByEid(p.elementId);
    return a && Radio::IsRadio(a) && ue_wrap::world_identity::WorldOf(a)==ue_wrap::world_identity::CurrentWorld() ? a : nullptr;
}
bool Valid(const Packet& p) {
    if (p.key.len>31 || !(p.key.len || p.elementId)) return false;
    if (p.op==2) return !p.seq && !p.track && !p.ticks && !p.playing && !p.mode;
    return p.track && p.playing<=1 && p.mode<=3 &&
        p.ticks>=0 && p.ticks<=864000000000ll && ((p.op==0 && !p.seq) || (p.op==1 && p.seq));
}
Radio::State State(const Packet& p,uint64_t elapsed=0) {
    return {p.track,p.ticks+(p.playing ? int64_t(elapsed)*10000 : 0),p.playing,p.mode};
}
bool Read(void* a,Packet& p) {
    Radio::State s{};
    if (!Radio::Read(a,s) || !Identity(a,p)) return false;
    p.track=s.track;p.ticks=s.ticks;p.playing=s.playing;p.mode=s.mode;
    for (int i=0;i<4;++i) p.ack[i]=g_ack[i];
    return Valid(p);
}
void Publish(void* a,int slot=-1) {
    Packet p{}; if (!g_session || !Read(a,p)) return;
    auto& prev=g_sent[Token(p)]; const auto now=Now();
    if (slot<0 && prev.actor.Get()==a && now-prev.at<2000 && p.track==prev.packet.track &&
        p.playing==prev.packet.playing && p.mode==prev.packet.mode &&
        std::memcmp(p.ack,prev.packet.ack,sizeof(p.ack))==0) return;
    const bool ok=slot<0 ? g_session->SendReliable(net::ReliableKind::RadioState,&p,sizeof(p)) :
        g_session->SendReliableToSlot(slot,net::ReliableKind::RadioState,&p,sizeof(p));
    if (ok && slot<0) {
        if (prev.actor.Get()!=a || p.track!=prev.packet.track || p.playing!=prev.packet.playing || p.mode!=prev.packet.mode)
            UE_LOGI("radio: publish key='%ls' eid=%u track=%llu play=%u mode=%u",Token(p).c_str(),p.elementId,
                static_cast<unsigned long long>(p.track),unsigned(p.playing),unsigned(p.mode));
        prev.actor.Set(a);prev.packet=p;prev.at=now;
    }
}
void Reply(void* a,const Packet& request,uint8_t sender) {
    Packet p{};
    if (!a || !Read(a,p)) {
        p.key=request.key;p.elementId=request.elementId;p.op=2;
        for (int i=0;i<4;++i) p.ack[i]=g_ack[i];
    }
    g_session->SendReliableToSlot(sender,net::ReliableKind::RadioState,&p,sizeof(p));
}
bool Predicted(const std::wstring& id) {
    if (g_dirty.count(id)) return true;
    for (const auto& q:g_pending) if (Token(q.p)==id) return true;
    return false;
}
void Paint(const std::wstring& id,Row& row) {
    if (Predicted(id)) return;
    if (void* a=Resolve(row.packet)) Radio::Apply(a,State(row.packet,Now()-row.at));
}
bool Ready() { return true; }
uint64_t Classes() { return ue_wrap::object_index::ClassSetVersion(); }
void Begin(void*,bool) { ResetWorld(); }
void Match(void*,void* a) {
    if (R::NameStartsWith(R::NameOf(a),L"Default__") ||
        ue_wrap::world_identity::WorldOf(a)!=ue_wrap::world_identity::CurrentWorld()) return;
    Packet p{}; if (Identity(a,p)) {
        auto& row=g_radios[Token(p)];
        if (row.actor.Get()!=a) UE_LOGI("radio: discovered key='%ls' eid=%u",Token(p).c_str(),p.elementId);
        row.actor.Set(a);
    }
}
size_t Complete(void*,bool,uint32_t) { return g_radios.size(); }
void Dirty(void* a) {
    if (!Client() || !a || !net_pump::IsInAnnouncedWorld(a)) return;
    ResetWorld();Packet p{};if (!Identity(a,p)) return;
    auto& row=g_dirty[Token(p)];row.actor.Set(a);row.at=Now();
}
void OpenPost(const SG::Call& c) { if (!c.fromOurCode) Dirty(c.object); }
SG::Verdict UiPre(const SG::Call& c) {
    if (Client() && !c.fromOurCode) g_userDepth=c.depth;
    return SG::Verdict::Run;
}
void UiPost(const SG::Call& c) {
    if (!c.fromOurCode) Dirty(Radio::UiRadio(c.object));
    g_userDepth=0;
}
SG::Verdict Finish(const SG::Call& c) {
    // End-of-track rolls happen only on the host. The explicit Skip button is
    // still a local user edit and its chosen track is sent after loading.
    return Client() && !c.fromOurCode && !g_userDepth && net_pump::IsInAnnouncedWorld(c.object) && Radio::HasLocalTrack(c.object)
        ? SG::Verdict::Cancel : SG::Verdict::Run;
}
}
void Install(net::Session* session) {
    g_session=session;if (g_installed) return;g_installed=true;
    element::scan_hub::Register({"radio",nullptr,&Ready,&Radio::IsRadio,&Begin,&Match,&Complete,2,&Classes});
    SG::WatchClassName(L"prop_radio_internet_C",L"openLink",0x52410001,nullptr,&OpenPost);
    SG::WatchClassName(L"prop_radio_internet_C",L"fin",0x52410002,&Finish,nullptr);
    const wchar_t* buttons[]={
        L"BndEvt__umg_onlineRadio_Button_play_K2Node_ComponentBoundEvent_0_OnButtonClickedEvent__DelegateSignature",
        L"BndEvt__umg_onlineRadio_Button_stop_K2Node_ComponentBoundEvent_1_OnButtonClickedEvent__DelegateSignature",
        L"BndEvt__umg_onlineRadio_Button_skip_K2Node_ComponentBoundEvent_5_OnButtonClickedEvent__DelegateSignature",
        L"BndEvt__umg_onlineRadio_Button_104_K2Node_ComponentBoundEvent_2_OnButtonClickedEvent__DelegateSignature"};
    for (int i=0;i<4;++i) SG::WatchClassName(L"ui_radioInterface_C",buttons[i],0x52410010+i,&UiPre,&UiPost);
}
void Tick() {
    if (!g_session || !g_session->connected()) return;
    ResetWorld();SG::ResolvePendingNames();const auto now=Now();if (now<g_next) return;g_next=now+200;g_userDepth=0;
    for (auto it=g_radios.begin();it!=g_radios.end();) {
        if (!it->second.actor.Alive()) { g_sent.erase(it->first);it=g_radios.erase(it); } else ++it;
    }
    if (!Client()) { for (auto& pair:g_radios) Publish(pair.second.actor.Get());return; }
    for (auto it=g_dirty.begin();it!=g_dirty.end();) {
        Packet p{};void* a=it->second.actor.Get();
        if (!a || now-it->second.at>15000) {
            if (a) UE_LOGW("radio: local edit expired before playback was readable key='%ls'",it->first.c_str());
            it=g_dirty.erase(it);continue;
        }
        if (now-it->second.at<200 || g_pending.size()>=64 || !Read(a,p)) { ++it;continue; }
        p.op=1;if (++g_seq==0) ++g_seq;p.seq=g_seq;g_pending.push_back({p,0});
        UE_LOGI("radio: queued seq=%u key='%ls' track=%llu play=%u",p.seq,Token(p).c_str(),
            static_cast<unsigned long long>(p.track),unsigned(p.playing));
        it=g_dirty.erase(it);
    }
    if (!g_pending.empty()) {
        auto& q=g_pending.front();
        if ((!q.at || now-q.at>=1000) && g_session->SendReliable(net::ReliableKind::RadioState,&q.p,sizeof(q.p))) q.at=now;
    }
    for (auto& pair:g_canonical) Paint(pair.first,pair.second);
}
void Receive(const net::RadioStatePayload& p,uint8_t sender) {
    if (!g_session || !g_session->connected() || !Valid(p) || sender>=4) return;
    ResetWorld();
    if (p.op!=1) {
        if (!Client() || sender!=0) return;
        const auto me=players::Registry::Get().LocalPeerId();if (me>=4) return;
        while (!g_pending.empty() && !After(g_pending.front().p.seq,p.ack[me])) g_pending.pop_front();
        if (p.op==2) return; // acknowledgement can arrive even when no file is loaded on the host
        const auto id=Token(p);if (g_canonical.size()>=256 && !g_canonical.count(id)) return;
        auto& row=g_canonical[id];row.packet=p;row.at=Now();Paint(id,row);return;
    }
    if (Client() || !sender) return;
    void* a=Resolve(p);
    if (!After(p.seq,g_ack[sender])) { Reply(a,p,sender);return; }
    auto& attempt=g_attempt[sender];const auto now=Now();
    if (attempt.seq!=p.seq) attempt={p.seq,now};
    auto target=element::IntentTarget::ForClientIntent(*g_session,sender,500.f);
    const bool rejected=a && target.HasBody() && !target.Authorize(a);
    const bool applied=a && target.HasBody() && !rejected && Radio::Apply(a,State(p));
    if (!applied && !rejected && now-attempt.since<15000) return;
    // Bound missing actor/file/decoder retries so one failed edit cannot block the next radio.
    UE_LOGI("radio: peer=%u seq=%u applied=%u timeout=%u",unsigned(sender),unsigned(p.seq),unsigned(applied),unsigned(!applied && !rejected));
    g_ack[sender]=p.seq;attempt={};if (a) Publish(a);Reply(a,p,sender);
}
void OnDisconnect() {
    g_session=nullptr;g_world=0;g_seq=0;g_next=0;g_userDepth=0;
    g_radios.clear();g_canonical.clear();g_dirty.clear();g_sent.clear();g_pending.clear();
    for (int i=0;i<4;++i) { g_ack[i]=0;g_attempt[i]={}; }
}
}
