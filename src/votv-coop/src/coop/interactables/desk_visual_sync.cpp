#include "coop/interactables/desk_visual_sync.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/interactables/desk_input_sync.h"
#include "coop/net/intent_bucket.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/coords_panel.h"
#include "ue_wrap/engine/world_identity.h"
#include <chrono>
#include <cmath>

namespace coop::desk_visual_sync {
namespace {
namespace CP = ue_wrap::coords_panel;
using Payload = net::DeskVisualPayload;
net::Session* g_session = nullptr;
Payload g_state{}, g_sent{}, g_applied{};
bool g_have = false, g_haveSent = false, g_haveApplied = false, g_tail = false, g_ticking = false;
bool g_seed[net::kMaxPeers]{};
net::IntentBucket g_budget[net::kMaxPeers];
ue_wrap::CachedObjRef g_widget, g_material;
uint32_t g_world = 0, g_epoch = 0, g_revision = 0;
uint64_t g_next = 0, g_lastSend = 0, g_lastInput = 0;
uint64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool SameAim(const Payload& a, const Payload& b) {
    return a.aim.c0X == b.aim.c0X && a.aim.c0Y == b.aim.c0Y && a.aim.c1X == b.aim.c1X &&
        a.aim.c1Y == b.aim.c1Y && a.aim.c2X == b.aim.c2X && a.aim.c2Y == b.aim.c2Y &&
        a.aim.selected == b.aim.selected && a.aim.direction == b.aim.direction;
}
bool Same(const Payload& a, const Payload& b) {
    return SameAim(a,b) && a.outer == b.outer && a.inner == b.inner && a.ping == b.ping &&
        a.active == b.active && a.owner == b.owner;
}
bool Valid(const Payload& p) {
    const float coords[] = {p.aim.c0X,p.aim.c0Y,p.aim.c1X,p.aim.c1Y,p.aim.c2X,p.aim.c2Y};
    for (float v : coords) if (!std::isfinite(v) || std::abs(v) > 1.e7f) return false;
    for (float v : {p.outer,p.inner,p.ping}) if (!std::isfinite(v) || v < 0 || v > 1) return false;
    return p.aim.selected >= 0 && p.aim.selected <= 3 && p.aim.direction <= 1 && p.active <= 1 &&
        (p.owner < net::kMaxPeers || p.owner == 0xFF);
}
CP::DishAim Aim(const Payload& p) {
    CP::DishAim a;
    a.c0X=p.aim.c0X; a.c0Y=p.aim.c0Y; a.c1X=p.aim.c1X; a.c1Y=p.aim.c1Y;
    a.c2X=p.aim.c2X; a.c2Y=p.aim.c2Y; a.selected=p.aim.selected; a.direction=p.aim.direction;
    return a;
}
bool Read(Payload& p) {
    if (!CP::PresentationTarget()) return false;
    CP::DishAim a; CP::PingVisual v;
    if (!CP::ReadDishAim(a) || !CP::ReadPingVisual(v)) return false;
    p={}; p.aim.c0X=a.c0X; p.aim.c0Y=a.c0Y; p.aim.c1X=a.c1X; p.aim.c1Y=a.c1Y;
    p.aim.c2X=a.c2X; p.aim.c2Y=a.c2Y; p.aim.selected=a.selected; p.aim.direction=a.direction;
    p.outer=v.outer; p.inner=v.inner; p.ping=v.ping; p.active=v.active;
    return Valid(p);
}
void Canonical(Payload p) {
    p.op=1;
    if (++g_revision==0) ++g_revision;
    p.revision=g_revision;
    if (!g_have || p.owner!=g_state.owner || p.active!=g_state.active)
        UE_LOGI("desk_visual: host owner=%u active=%u revision=%u",p.owner,p.active,p.revision);
    g_state=p; g_have=true;
    g_session->SendReliable(net::ReliableKind::DishAimState,&g_state,sizeof(g_state));
}
void ResetWorld() {
    ++g_epoch; g_world=ue_wrap::world_identity::Generation();
    g_state={}; g_sent={}; g_applied={}; g_have=g_haveSent=g_haveApplied=g_tail=false;
    g_next=g_lastSend=g_lastInput=0; g_revision=0;
    g_widget.Reset(); g_material.Reset();
    for (int i=0;i<net::kMaxPeers;++i) {g_seed[i]=false; g_budget[i].Reset();}
}
void World() { if (g_world != ue_wrap::world_identity::Generation()) ResetWorld(); }
bool Current(uint32_t epoch) {
    return g_epoch==epoch && g_world==ue_wrap::world_identity::Generation() &&
        g_session && g_session->connected();
}
void Apply(uint8_t local, uint32_t epoch) {
    if (!g_have || device_occupancy::LocalHolds(L"desk") || g_tail) return;
    // Don't apply our older echo while our final sample is in flight. Once it
    // matches, it can restore a reconstructed widget even for the former owner.
    if (g_state.owner==local && !Same(g_state,g_sent)) return;
    const Payload snapshot=g_state;
    void* widget=CP::Instance(); void* material=CP::PresentationTarget();
    if (!widget || !material || !Current(epoch)) return;
    ue_wrap::CachedObjRef w,m; w.Set(widget); m.Set(material);
    const auto same = [&] { return Current(epoch) && g_have && g_state.revision==snapshot.revision &&
        w.Get()==widget && m.Get()==material && CP::Instance()==widget && CP::PresentationTarget()==material; };
    CP::DishAim actual;
    if (!CP::ReadDishAim(actual) || !same()) return;
    // Construct may reinitialize coordinates while reusing its existing dynamic
    // materials. Detect that silent overwrite without repainting unchanged poses.
    const auto& a=snapshot.aim;
    const bool drift=actual.c0X!=a.c0X || actual.c0Y!=a.c0Y || actual.c1X!=a.c1X ||
        actual.c1Y!=a.c1Y || actual.c2X!=a.c2X || actual.c2Y!=a.c2Y || actual.selected!=a.selected;
    const bool changed = drift || !g_haveApplied || !g_widget.Is(widget) || !g_material.Is(material) || !SameAim(snapshot,g_applied);
    if (changed && (!CP::WriteDishCommitted(Aim(snapshot)) || !same())) return;
    // Native idle paint reads the mirrored accumulators. Retry while the host's
    // brief verdict simulation owns them; never overwrite an active machine.
    CP::PingVisual v{snapshot.outer,snapshot.inner,snapshot.ping,snapshot.active!=0};
    if (!CP::WritePingVisual(v) || !same()) return;
    g_applied=snapshot; g_haveApplied=true; g_widget=w; g_material=m;
}
}
void Install(net::Session* session) { g_session=session; World(); }
void Tick() {
    if (g_ticking || !g_session || !g_session->connected()) return;
    struct Scope { Scope(){g_ticking=true;} ~Scope(){g_ticking=false;} } scope;
    World(); const uint32_t epoch=g_epoch; const uint64_t now=NowMs();
    if (now<g_next) return;
    g_next=now+100;
    const bool host=g_session->role()==net::Role::Host;
    const uint8_t local=host?0:players::Registry::Get().LocalPeerId();
    if (local>=net::kMaxPeers) return;
    const uint8_t holder=device_occupancy::HolderOf(L"desk");
    const bool holds=device_occupancy::LocalHolds(L"desk");
    bool source=holds || g_tail || (host && desk_input_sync::PingActiveSlot()==0);
    if (holder!=0xFF && holder!=local) {source=false;g_tail=false;}
    if (host && !g_have && holder==0xFF) source=true; // initial idle snapshot
    if (source) {
        Payload p;
        if (Read(p) && Current(epoch)) {
            p.owner=local;
            g_tail=p.active!=0;
            if (host) {
                g_sent=p; g_haveSent=true;
                if (!g_have || !Same(p,g_state)) Canonical(p);
            } else if (!g_haveSent || !Same(p,g_sent) || now-g_lastSend>=1000) {
                if (g_session->SendReliableToSlot(0,net::ReliableKind::DishAimState,&p,sizeof(p))) {
                    g_sent=p; g_haveSent=true; g_lastSend=now;
                }
            }
        }
    } else g_haveSent=false;
    if (!Current(epoch)) return;
    if (host) {
        if (g_have && g_state.active && g_state.owner!=0 && now-g_lastInput>3000) {
            Payload ended=g_state; ended.owner=0xFF; ended.active=0; ended.outer=ended.inner=ended.ping=0;
            Canonical(ended);
        }
        for (int slot=1;slot<net::kMaxPeers;++slot) if (g_seed[slot] && g_have &&
            g_session->SendReliableToSlot(slot,net::ReliableKind::DishAimState,&g_state,sizeof(g_state))) g_seed[slot]=false;
    }
    Apply(local,epoch);
}
void OnState(const Payload& p,uint8_t sender) {
    if (!g_session || !g_session->connected() || !Valid(p)) return;
    World();
    if (g_session->role()==net::Role::Host) {
        if (p.op!=0 || sender==0 || sender>=net::kMaxPeers || p.owner!=sender) return;
        const uint64_t now=NowMs();
        const uint8_t holder=device_occupancy::HolderOf(L"desk");
        // A pinger may stand up while its machine completes. Its existing run
        // keeps a short lease even between physical release and FSM-hold updates.
        const bool tail=holder==0xFF && g_have && g_state.owner==sender && g_state.active && now-g_lastInput<=3000;
        if (holder!=sender && !tail) return;
        if (!g_budget[sender].Take({30.f,15.f},now)) return;
        g_lastInput=now;
        if (!g_have || !Same(p,g_state)) Canonical(p);
    } else {
        if (p.op!=1 || sender!=0 || !p.revision) return;
        if (g_have && static_cast<int32_t>(p.revision-g_state.revision)<=0) return;
        g_state=p; g_have=true;
    }
}
void QueueConnect(int slot) { World(); if(slot>0 && slot<net::kMaxPeers) g_seed[slot]=true; }
void OnPeerLeft(int slot) {
    if(slot<=0 || slot>=net::kMaxPeers) return;
    g_seed[slot]=false; g_budget[slot].Reset();
    if(g_session && g_session->connected() && g_session->role()==net::Role::Host && g_have && g_state.owner==slot) {
        Payload ended=g_state; ended.owner=0xFF; ended.active=0; ended.outer=ended.inner=ended.ping=0;
        Canonical(ended);
    }
}
void OnDisconnect() { ResetWorld(); g_session=nullptr; }
}
