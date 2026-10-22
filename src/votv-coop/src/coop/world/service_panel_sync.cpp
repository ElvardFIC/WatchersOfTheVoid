#include "coop/world/service_panel_sync.h"
#include "coop/world/service_panel_rules.h"
#include "coop/element/intent_authority.h"
#include "coop/element/object_scan_hub.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/player/remote_player.h"
#include "coop/props/remote_prop.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_park.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/service_panel.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/game_rules.h"
#include <chrono>
#include <cstring>
#include <deque>
#include <vector>

namespace coop::service_panel_sync {
namespace {
namespace P=ue_wrap::service_panel;
namespace R=ue_wrap::reflection;
namespace SG=ue_wrap::script_gate;
using Packet=net::ServicePanelPayload;
net::Session* g_session=nullptr;
bool g_installed=false, g_applying=false;
static_assert(net::kMaxPeers==coop::net::kPeerCapacity,"ServicePanel ack wire layout");
uint32_t g_world=0, g_seq=0;
roster_ledger::PerSlotState<uint32_t> g_ack;
struct ReturnAttempt { uint32_t seq=0;uint64_t since=0; };
roster_ledger::PerSlotState<ReturnAttempt> g_returnAttempt;
uint64_t g_next=0;
struct Row {
    ue_wrap::CachedObjRef actor;
    uint8_t observed[16]{};
    bool seen=false, haveCanonical=false;
    Packet canonical{}, sent{};
    uint64_t sentAt=0;
    int loadingDepth=0;
};
Row g_rows[3];
struct Pending { Packet packet{}; uint64_t sentAt=0; };
std::deque<Pending> g_pending;
struct FuseEdit { void* stack; int depth; uint8_t before[16]; };
std::deque<FuseEdit> g_fuseEdits;
std::vector<ue_wrap::CachedObjRef> g_coverReturns;
uint64_t Now() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
bool Client() { return g_session && g_session->connected() && g_session->role()==net::Role::Client; }
bool After(uint32_t a,uint32_t b) { return static_cast<int32_t>(a-b)>0; }
void ResetWorld() {
    const auto world=ue_wrap::world_identity::Generation();
    if (world==g_world) return;
    g_world=world; g_seq=0; g_next=0; g_pending.clear();
    g_fuseEdits.clear(); g_coverReturns.clear();
    for (int slot=0;slot<net::kMaxPeers;++slot) { g_ack[slot]=0;g_returnAttempt[slot]={}; }
    for (auto& r:g_rows) r=Row{};
}
void Observe(void* actor) {
    const auto f=P::Family(actor);
    if (!f || R::NameStartsWith(R::NameOf(actor),L"Default__") ||
        ue_wrap::world_identity::WorldOf(actor)!=ue_wrap::world_identity::CurrentWorld()) return;
    auto& row=g_rows[f];
    if (row.actor.Get()!=actor) { row.actor.Set(actor); row.seen=false; row.sentAt=0; }
    if (!row.seen) row.seen=P::Read(actor,row.observed);
}
bool Ready() { return true; }
bool MatchClass(void* a) { return P::Family(a)!=0; }
uint64_t Classes() { return ue_wrap::object_index::ClassSetVersion(); }
void Begin(void*,bool) { ResetWorld(); }
void Match(void*,void* a) { Observe(a); }
size_t Complete(void*,bool,uint32_t) { return size_t(g_rows[1].actor.Alive())+size_t(g_rows[2].actor.Alive()); }
void Overlay(uint8_t state[16],const Packet& p) {
    if (p.op==1) {
        for (int n=0;n<2;++n) {
            const uint8_t mask=(p.action>>(4*n))&15;
            state[n]=static_cast<uint8_t>((state[n]&~mask)|(p.state[n]&mask));
        }
    } else if (p.op==3 && p.index<6) state[4+p.index]=1;
}
void Paint(uint8_t family) {
    auto& row=g_rows[family];
    void* a=row.actor.Get();
    if (!a || !row.haveCanonical || row.loadingDepth) return;
    uint8_t state[16]; std::memcpy(state,row.canonical.state,16);
    for (const auto& p:g_pending) if (p.packet.family==family) Overlay(state,p.packet);
    g_applying=true;
    const bool ok=P::Apply(a,state);
    g_applying=false;
    if (ok) { std::memcpy(row.observed,state,16); row.seen=true; }
}
void Publish(uint8_t family,int slot=-1) {
    if (!g_session || g_session->role()!=net::Role::Host) return;
    auto& row=g_rows[family];
    Packet p{}; p.family=family;
    if (!P::Read(row.actor.Get(),p.state) || !service_panel_rules::StateValid(family,p.state)) return;
    for (int peer=0;peer<net::kMaxPeers;++peer) p.ack[peer]=g_ack[peer];
    const auto now=Now();
    if (slot<0 && row.sentAt && now-row.sentAt<2000 && std::memcmp(&p,&row.sent,sizeof(p))==0) return;
    const bool ok=slot<0 ? g_session->SendReliable(net::ReliableKind::ServicePanel,&p,sizeof(p)) :
        g_session->SendReliableToSlot(slot,net::ReliableKind::ServicePanel,&p,sizeof(p));
    if (ok && slot<0) { row.sent=p; row.sentAt=now; }
}
bool Queue(Packet p) {
    if (!Client() || g_pending.size()>=64) return false;
    if (++g_seq==0) ++g_seq;
    p.seq=g_seq;
    // One outstanding operation per author preserves order even when the host
    // has not discovered the actor yet. Retries reuse the sequence (no replay).
    uint64_t sentAt=0;
    if (g_pending.empty() && g_session->SendReliable(net::ReliableKind::ServicePanel,&p,sizeof(p))) sentAt=Now();
    g_pending.push_back({p,sentAt});
    return true;
}
SG::Verdict LoadPre(const SG::Call& c) {
    ResetWorld(); Observe(c.object);
    const auto f=P::Family(c.object);
    if (f) g_rows[f].loadingDepth=c.depth;
    return SG::Verdict::Run;
}
void LoadPost(const SG::Call& c) {
    const auto f=P::Family(c.object);
    if (!f) return;
    auto& row=g_rows[f]; row.loadingDepth=0;
    row.seen=P::Read(c.object,row.observed);
}
// updPanels also runs at the end of the screwdriver's latent animation and at
// panel reinsertion. Snapshot just this array, never the module array or save.
void DeskRefresh(const SG::Call& c) {
    if (!Client() || g_applying || c.fromOurCode || !net_pump::IsInAnnouncedWorld(c.object)) return;
    ResetWorld();
    auto& row=g_rows[1];
    if (row.loadingDepth || row.actor.Get()!=c.object || !row.seen) { Observe(c.object); return; }
    Packet p{}; p.family=1; p.op=1;
    if (!P::Read(c.object,p.state)) return;
    std::memcpy(p.before,row.observed,16);
    p.action=static_cast<uint8_t>((p.before[1]^p.state[1])<<4);
    p.before[0]=p.state[0]; // a cover is never authored by this client
    if (p.action && Queue(p)) std::memcpy(row.observed,p.state,16);
}
SG::Verdict Press(const SG::Call& c) {
    if (!Client() || g_applying || c.fromOurCode || !net_pump::IsInAnnouncedWorld(c.object)) return SG::Verdict::Run;
    static R::ParamOffset action{L"action"};
    const int off=action.Of(c.function);
    if (!c.locals || off<0 || c.locals[off]!=4) return SG::Verdict::Run;
    ResetWorld(); Observe(c.object);
    Packet p{}; p.family=P::Family(c.object);
    if (p.family==1) {
        if (!P::DeskLook(c.object,p.index)) return SG::Verdict::Run;
        uint8_t state[16]{};
        if (!P::Read(c.object,state) || !(state[0]&(1u<<p.index)) || !(state[1]&(1u<<p.index)))
            return SG::Verdict::Run;
        p.op=5; // host creates exactly one loose cover; no speculative item in a hand
    } else if (p.family==2) {
        if (!P::RadioLook(c.object,p.action,p.index)) return SG::Verdict::Run;
        uint8_t state[16]{};
        if (!P::Read(c.object,state) || state[3] || (p.action!=4 && !state[2]) ||
            (p.action==4 && state[4+p.index]!=2)) return SG::Verdict::Run;
        p.op=p.action==4 ? 4 : 2;
    } else return SG::Verdict::Run;
    Queue(p);
    return SG::Verdict::Cancel;
}
void QueueCoverReturn(void* desk,void* actor) {
    if (!Client() || g_pending.size()>=64 || !actor ||
        !ue_wrap::engine::IsMainPlayerGrabbing(players::Registry::Get().Local(),actor) ||
        !P::CanReturnCover(desk,actor)) return;
    Packet p{};p.family=1;p.op=6;p.index=static_cast<uint8_t>(P::CoverIndex(actor));
    const auto key=ue_wrap::prop::GetInteractableKeyString(actor);
    if (!key.empty() && key!=L"None" && key.size()<=31) net::WireKeyFromString(key,p.coverKey);
    auto eid=prop_element_tracker::GetPropElementIdForActor(actor);
    if (eid==element::kInvalidId) eid=remote_prop::ResolveMirrorEidByActor(actor);
    p.coverEid=eid==element::kInvalidId ? 0 : eid;
    if (!(p.coverKey.len || p.coverEid)) return;
    // Release our physics handle before the host can consume the part. The
    // regular PropRelease may arrive after this request; the host validates the
    // actual socket overlap and closes the old hold before native reinsertion.
    if (ue_wrap::engine::ReleaseMainPlayerGrabIfHolding(players::Registry::Get().Local(),actor)) Queue(p);
}
SG::Verdict CoverOverlap(const SG::Call& c) {
    if (!g_session || !g_session->connected() || !net_pump::IsInAnnouncedWorld(c.object)) return SG::Verdict::Run;
    // Only cover reinsertion lives in this actor event (module ports have their
    // own component events). It must not consume the same part on both peers.
    static R::ParamOffset other{L"OtherActor"};
    const int off=other.Of(c.function);
    if (!c.locals || off<0) return SG::Verdict::Run;
    void* actor=nullptr; std::memcpy(&actor,c.locals+off,sizeof(actor));
    if (Client()) { ResetWorld();Observe(c.object);QueueCoverReturn(c.object,actor);return SG::Verdict::Cancel; }
    if (actor && P::CoverIndex(actor)==2 && !remote_prop::IsActorUnderAnyDrive(actor) && P::CanReturnCover(c.object,actor)) {
        ue_wrap::engine::ReleaseMainPlayerGrabIfHolding(players::Registry::Get().Local(),actor);
        P::TryReturnCover(c.object,actor);
        return SG::Verdict::Cancel;
    }
    if (!actor || P::CoverIndex(actor)<0 || !remote_prop::IsActorUnderAnyDrive(actor)) return SG::Verdict::Run;
    ResetWorld(); Observe(c.object);
    for (const auto& ref:g_coverReturns) if (ref.Get()==actor) return SG::Verdict::Run;
    if (g_coverReturns.size()<64) { ue_wrap::CachedObjRef ref; ref.Set(actor); g_coverReturns.push_back(ref); }
    return SG::Verdict::Run; // mirror_slot_entry refuses the actual premature consume
}
SG::Verdict FusePre(const SG::Call& c) {
    while (!g_fuseEdits.empty() && g_fuseEdits.back().depth>=c.depth) g_fuseEdits.pop_back();
    if (!Client() || c.fromOurCode || !net_pump::IsInAnnouncedWorld(c.object)) return SG::Verdict::Run;
    // Reserve queue space before the native body can consume the inventory item.
    if (g_pending.size()>=64) return SG::Verdict::Cancel;
    ResetWorld(); Observe(c.object);
    FuseEdit edit{c.stack,c.depth,{}};
    if (!P::Read(c.object,edit.before)) return SG::Verdict::Cancel;
    g_fuseEdits.push_back(edit);
    return SG::Verdict::Run;
}
void FusePost(const SG::Call& c) {
    if (g_fuseEdits.empty() || g_fuseEdits.back().stack!=c.stack) return;
    const auto edit=g_fuseEdits.back(); g_fuseEdits.pop_back();
    uint8_t state[16]{};
    if (!P::Read(c.object,state)) return;
    for (uint8_t i=0;i<6;++i) if (edit.before[4+i]==0 && state[4+i]==1) {
        Packet p{}; p.family=2; p.op=3; p.index=i;
        std::memcpy(p.before,edit.before,16); std::memcpy(p.state,state,16);
        Queue(p);
    }
    std::memcpy(g_rows[2].observed,state,16);
}
SG::Verdict Producer(const SG::Call& c) {
    return Client() && !g_applying && !c.fromOurCode && net_pump::IsInAnnouncedWorld(c.object)
        ? SG::Verdict::Cancel : SG::Verdict::Run;
}
bool Easy() {
    ue_wrap::game_rules::Snapshot rules;
    if (!ue_wrap::game_rules::ReadLocal(rules)) return false;
    for (const auto& f:rules.fields) if (f.key=="difficulty") return f.ival<=1;
    return false;
}
}
void Install(net::Session* session) {
    g_session=session;
    if (g_installed) return;
    g_installed=true;
    element::scan_hub::Register({"service-panels",nullptr,&Ready,&MatchClass,&Begin,&Match,&Complete,2,&Classes});
    SG::WatchClassName(L"analogDScreenTest_C",L"updPanels",0x53500001,nullptr,&DeskRefresh);
    SG::WatchClassName(L"analogDScreenTest_C",L"actionOptionIndex",0x53500002,&Press,nullptr);
    SG::WatchClassName(L"analogDScreenTest_C",L"setData",0x53500003,&LoadPre,&LoadPost);
    SG::WatchClassName(L"radiotower_C",L"loadData",0x53500004,&LoadPre,&LoadPost);
    SG::WatchClassName(L"radiotower_C",L"actionOptionIndex",0x53500005,&Press,nullptr);
    SG::WatchClassName(L"radiotower_C",L"playerUsedOn",0x53500006,&FusePre,&FusePost);
    SG::WatchClassName(L"analogDScreenTest_C",L"ReceiveActorBeginOverlap",0x53500007,&CoverOverlap,nullptr);
    const wchar_t* const producers[]={L"scramble",L"breakdown",L"broken",L"broken_fire",L"tryToFix",L"completeMinigame"};
    for (int i=0;i<6;++i) SG::WatchClassName(L"radiotower_C",producers[i],0x53500010+i,&Producer,nullptr);
}
void Tick() {
    if (!g_session || !g_session->connected()) return;
    ResetWorld(); SG::ResolvePendingNames();
    const auto now=Now(); if (now<g_next) return; g_next=now+100;
    // A failed script body has no post; never leave a loading guard latched forever.
    for (auto& row:g_rows) row.loadingDepth=0;
    if (Client()) {
        ue_wrap::engine::MainPlayerGrabState grab{};
        if (ue_wrap::engine::ReadMainPlayerGrabState(players::Registry::Get().Local(),grab))
            QueueCoverReturn(g_rows[1].actor.Get(),grab.grabbingActor);
        if (!g_pending.empty()) {
            auto& q=g_pending.front();
            if ((!q.sentAt || now-q.sentAt>=(q.packet.op==6 ? 200 : 1000)) &&
                g_session->SendReliable(net::ReliableKind::ServicePanel,&q.packet,sizeof(q.packet))) q.sentAt=now;
        }
        Paint(1); Paint(2);
    } else {
        for (auto it=g_coverReturns.begin();it!=g_coverReturns.end();) {
            void* a=it->Get();
            if (!a) { it=g_coverReturns.erase(it); continue; }
            if (remote_prop::IsActorUnderAnyDrive(a)) { ++it; continue; }
            if (!P::TryReturnCover(g_rows[1].actor.Get(),a)) it=g_coverReturns.erase(it);
            else ++it;
        }
        Publish(1); Publish(2);
    }
}
void Receive(const Packet& p,uint8_t slot) {
    if (!g_session || !g_session->connected() || !service_panel_rules::Valid(p)) return;
    ResetWorld();
    if (p.op==0) {
        if (!Client() || slot!=0) return;
        const auto my=players::Registry::Get().LocalPeerId();
        if (my>=net::kMaxPeers) return;
        for (auto it=g_pending.begin();it!=g_pending.end();) {
            if (it->packet.family==p.family && !After(it->packet.seq,p.ack[my])) it=g_pending.erase(it);
            else ++it;
        }
        auto& row=g_rows[p.family]; row.canonical=p; row.haveCanonical=true;
        Paint(p.family); return;
    }
    if (Client() || slot==0 || slot>=net::kMaxPeers || !p.seq) return;
    if (!After(p.seq,g_ack[slot])) { Publish(p.family,slot); return; }
    void* a=g_rows[p.family].actor.Get();
    uint8_t state[16]{};
    // Reliable delivery can beat actor discovery. Do not acknowledge an operation
    // against no actor; the sender retries unacknowledged operations below.
    if (!a || !P::Read(a,state)) return;
    ue_wrap::FVector at{};
    const uint8_t partOp=p.op==2 ? p.action : 4;
    auto auth=element::IntentTarget::ForClientIntent(*g_session,slot,500.f);
    // A just-connected player's first pose can arrive after this reliable request.
    // Retry without acknowledging until that body exists.
    if (!auth.HasBody()) return;
    const bool reached=p.family==1 ? bool(auth.Authorize(a)) :
        (P::PartLocation(a,partOp,p.index,at) && bool(auth.AuthorizeSegment(at,at)));
    bool ok=false;
    ue_wrap::FVector dropAt{};
    auto* puppet=players::Registry::Get().Puppet(slot);
    const bool haveDrop=puppet && ue_wrap::engine::TryGetActorLocation(puppet->GetActor(),dropAt);
    uint8_t original[16]; std::memcpy(original,state,16);
    bool refundFuse=false;
    if (reached && p.family==1 && p.op==6 && !(state[0]&(1<<p.index))) {
        auto& attempt=g_returnAttempt[slot];
        if (attempt.seq!=p.seq) attempt={p.seq,Now()};
        void* cover=p.coverKey.len ? prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.coverKey)) : nullptr;
        if (!cover && p.coverEid) cover=remote_prop::ResolveLiveActorByEid(p.coverEid);
        const bool own=cover && (!remote_prop::IsActorUnderAnyDrive(cover) || remote_prop::DrivenActor(slot)==cover);
        if (own && P::CoverIndex(cover)==p.index &&
            ue_wrap::world_identity::WorldOf(cover)==ue_wrap::world_identity::CurrentWorld() &&
            !ue_wrap::engine::IsMainPlayerGrabbing(players::Registry::Get().Local(),cover) && P::CanReturnCover(a,cover)) {
            void* mesh=ue_wrap::prop::GetStaticMesh(cover);
            remote_prop::EndAnyHoldOn(cover);
            coop::prop_park::Unpark(cover,mesh,false);
            g_applying=true;ok=P::TryReturnCover(a,cover);g_applying=false;
            if (!ok && R::IsLive(cover)) coop::prop_park::Unpark(cover,mesh,true);
        }
        // The request can overtake the last pose, or another cover's short
        // animation. Retry those races; never keep the author's queue forever.
        if (!ok && Now()-attempt.since<3000) return;
    }
    g_applying=true;
    if (reached && p.family==1 && p.op==1 && p.action) {
        // Only completed screwdriver edits come from a client. Both physical
        // removal and automatic reinsertion run on the host.
        service_panel_rules::MergeDesk(p,state);
        ok=P::Apply(a,state);
    } else if (reached && p.family==1 && p.op==5 && p.index<4 &&
               (state[0]&(1<<p.index)) && (state[1]&(1<<p.index))) {
        state[0]&=~(1<<p.index);
        if (P::Apply(a,state)) {
            ok=P::SpawnLoosePart(a,p.index,false,haveDrop ? &dropAt : nullptr)!=nullptr;
            if (!ok) P::Apply(a,original);
        }
    } else if (reached && p.family==2 && p.op==2) {
        ok=P::RadioUse(a,p.action,p.index);
    } else if (reached && p.family==2 && p.op==3 && p.index<6 && p.before[4+p.index]==0 && p.state[4+p.index]==1) {
        if (state[4+p.index]==0 && !state[3]) {
            ok=P::SetFuse(a,p.index,1);
            if (ok && Easy()) ue_wrap::component_calls::CallParamlessNamed(a,L"completeMinigame");
        }
        refundFuse=!ok; // native insert already spent the author's fuse
    } else if (reached && p.family==2 && p.op==4 && p.index<6 && state[4+p.index]==2 && !state[3]) {
        if (P::SetFuse(a,p.index,0)) {
            ok=P::SpawnLoosePart(a,p.index,false,haveDrop ? &dropAt : nullptr)!=nullptr;
            if (!ok) P::SetFuse(a,p.index,2);
        }
    }
    // A delayed input may arrive after the player walked away. Refuse the edit,
    // but return the native-consumed item once; the sequence fences duplicates.
    if (!reached && p.op==3) refundFuse=true;
    if (refundFuse && !P::SpawnLoosePart(a,p.index,true,haveDrop ? &dropAt : nullptr)) {
        // Nothing was accepted. Keep the request unacknowledged so its retry
        // can return the consumed fuse after a transient spawn failure.
        g_applying=false;
        UE_LOGE("service-panel: fuse refund spawn failed; awaiting request retry");
        return;
    }
    g_ack[slot]=p.seq;
    g_returnAttempt[slot]={};
    g_applying=false;
    if (P::Read(a,g_rows[p.family].observed)) g_rows[p.family].seen=true;
    UE_LOGI("service-panel: peer=%u family=%u op=%u index=%u applied=%d",slot,p.family,p.op,p.index,ok);
    Publish(p.family); Publish(p.family,slot);
}
void SendAllToSlot(int slot) { if (slot>0 && slot<net::kMaxPeers) { ResetWorld(); Publish(1,slot); Publish(2,slot); } }
void OnDisconnect() {
    g_session=nullptr; g_world=0; g_seq=0; g_next=0; g_applying=false;
    g_pending.clear(); g_fuseEdits.clear(); g_coverReturns.clear();
    for (int slot=0;slot<net::kMaxPeers;++slot) { g_ack[slot]=0;g_returnAttempt[slot]={}; }
    for (auto& r:g_rows) r=Row{};
}
}
