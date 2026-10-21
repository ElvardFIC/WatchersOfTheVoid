#include "coop/world/madness_sync.h"
#include "coop/net/session.h"
#include "coop/props/active_drive.h"
#include "coop/player/roster_ledger.h"
#include "ue_wrap/world/madness.h"
#include "ue_wrap/engine/world_identity.h"
#include <cmath>
namespace coop::madness_sync {
namespace {
namespace M=ue_wrap::madness;
using Packet=net::MadnessStatePayload;
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0,g_event=0,g_pending=0,g_appliedEvent=0;
uint64_t g_epoch=0,g_pollAt=0,g_sentAt=0,g_receivedAt=0;
float g_transit=0;
bool g_have=false,g_ticking=false;
ue_wrap::CachedObjRef g_master;
Packet g_state{};
M::View g_view;
uint32_t Next(uint32_t& n){if(!++n)++n;return n;}
bool Current(){return g_session && g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
uint32_t ReadyMask(){uint32_t mask=0;for(int i=1;i<net::kMaxPeers;++i)if(g_session->IsSlotWorldReady(i))mask|=1u<<i;return mask;}
}
void Install(net::Session* s) {
    const auto world=ue_wrap::world_identity::Generation();
    if(g_session!=s || world!=g_world){const auto epoch=g_epoch;OnDisconnect();
        if(g_epoch!=epoch+1 || g_session)return;g_session=s;g_world=world;}
}
void Tick() {
    if(!Current() || g_ticking)return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    const auto now=active_drive::NowMs(),epoch=g_epoch;
    if(g_session->role()==net::Role::Host) {
        if(now>=g_pollAt) {
            g_pollAt=now+100;
            void* actor=g_master.Get();if(!actor)actor=M::FindHost();
            const bool newEvent=actor && !g_master.Is(actor);
            ue_wrap::CachedObjRef live;live.Set(actor);M::State state{};
            if(actor && !M::Read(actor,state))return;
            if(epoch!=g_epoch || !Current() || (actor && !live.Get()) || state.kills<0 ||
               !std::isfinite(state.elapsed) || state.elapsed<0 || state.elapsed>86400)return;
            if(newEvent)Next(g_event);
            g_master.Set(actor);
            if(!g_have || newEvent || state.active!=bool(g_state.active) || state.hud!=bool(g_state.hud) ||
               state.playing!=bool(g_state.playing) || state.kills!=g_state.kills || now-g_sentAt>=1000) {
                g_state={Next(g_sequence),g_event,state.kills,state.elapsed,uint8_t(state.active),uint8_t(state.hud),uint8_t(state.playing),0};
                g_pending=ReadyMask();g_have=true;g_sentAt=now;
            }
        }
        // A retained snapshot's event age was sampled before transport accepted
        // it. Advance that age on each attempt, including late-join unicast.
        auto outgoing=g_state;
        if(outgoing.active && outgoing.playing)outgoing.elapsed+=float(now-g_sentAt)/1000.f;
        const auto pending=g_pending;
        for(int slot=1;slot<net::kMaxPeers;++slot)if(pending&(1u<<slot)) {
            if(!g_session->IsSlotWorldReady(slot)){g_pending&=~(1u<<slot);continue;}
            const auto person=roster_ledger::Get(slot).playerNo;
            const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::MadnessState,&outgoing,sizeof(outgoing));
            if(epoch!=g_epoch || !Current())return;
            if(sent && g_state.sequence==outgoing.sequence && roster_ledger::Get(slot).playerNo==person)g_pending&=~(1u<<slot);
        }
        return;
    }
    if(!g_have || now<g_pollAt)return;
    g_pollAt=now+100;
    auto view=g_view;g_view={};const auto state=g_state;
    const auto valid=[&]{return epoch==g_epoch && Current() && g_state.sequence==state.sequence;};
    if(g_appliedEvent!=state.eventId){M::Clear(view);if(!valid()){
        if(epoch==g_epoch && Current())g_view=view;else M::Clear(view);return;
    }}
    M::State desired{state.active!=0,state.hud!=0,state.playing!=0,state.kills,state.elapsed};
    // Seek a late join/retry to the host event's current age, not to track start.
    desired.elapsed+=float(now-g_receivedAt)/1000.f+g_transit;
    M::Apply(view,desired,valid);
    if(epoch!=g_epoch || !Current()){M::Clear(view);return;}
    g_view=view;g_appliedEvent=state.eventId;
}
void Receive(const Packet& p,uint8_t sender) {
    if(!Current() || g_session->role()!=net::Role::Client || sender!=0 || !p.sequence ||
       p.active>1 || p.hud>1 || p.playing>1 || p.reserved || p.kills<0 ||
       !std::isfinite(p.elapsed) || p.elapsed<0 || p.elapsed>86400 ||
       (p.active && !p.eventId) || (!p.active && (p.hud || p.playing || p.kills)) ||
       (g_have && !After(p.sequence,g_state.sequence)))return;
    g_state=p;g_have=true;g_receivedAt=active_drive::NowMs();g_pollAt=0;
    // GNS's measured RTT estimates one-way transit; it does not synchronize
    // clocks or establish sample-accurate playback on asymmetric paths.
    const int rtt=g_session->rttMsForSlot(0);
    g_transit=rtt>=0 && rtt<=10000?float(rtt)/2000.f:0.f;
}
void QueueConnect(int slot){if(Current() && g_session->role()==net::Role::Host && slot>0 && slot<net::kMaxPeers){g_pending|=1u<<slot;g_pollAt=0;}}
void OnPeerLeft(int slot){if(slot>0 && slot<net::kMaxPeers)g_pending&=~(1u<<slot);}
void OnDisconnect(){auto view=g_view;g_view={};++g_epoch;g_session=nullptr;g_world=0;g_sequence=g_event=g_pending=g_appliedEvent=0;g_pollAt=g_sentAt=g_receivedAt=0;g_transit=0;g_have=false;g_master={};g_state={};M::Clear(view);}
}
