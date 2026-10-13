#include "coop/player/hand_toy_audio.h"
#include "coop/net/session.h"
#include "coop/element/intent_authority.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/actors/dingus_music.h"
#include "ue_wrap/core/script_gate.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
namespace coop::hand_toy_audio {
namespace {
namespace M=ue_wrap::dingus_music;
namespace SG=ue_wrap::script_gate;
namespace R=ue_wrap::reflection;
using Packet=net::HandToyAudioPayload;
struct Row {Packet state{};uint64_t at=0;uint32_t pending=0;bool have=false,authorized=false;};
struct Pulse {
    Packet state{};ue_wrap::CachedObjRef localActor;
    uint64_t expires=0;uint32_t pending=0;
    bool local=false,authorized=false,played=false;
};
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0;
uint64_t g_epoch=0,g_poll=0;
bool g_ticking=false;
std::array<Row,net::kMaxPeers> g_rows{};
std::deque<Pulse> g_pulses;
uint32_t g_seen[net::kMaxPeers][2]{};
uint64_t g_rateAt[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};
constexpr int kRmb=0x48544130,kAction=0x48544131,kPlushUse=0x48544132;
bool Current(){return g_session && g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
uint32_t Next(){if(!++g_sequence)++g_sequence;return g_sequence;}
uint8_t Local(){return players::Registry::Get().LocalPeerId();}
uint32_t Mask(uint8_t owner){uint32_t m=0;for(int s=1;s<net::kMaxPeers;++s)
    if(s!=owner && g_session->IsSlotWorldReady(s))m|=1u<<s;return m;}
void* RemoteActor(const Packet& p) {
    if(p.slot>=net::kMaxPeers || p.slot==Local() || hand_item::MirrorGeneration(p.slot)!=p.generation)return nullptr;
    void* actor=hand_item::MirrorActorForSlot(p.slot);
    return (p.op?M::IsSqueakyToy(actor):M::IsDingus(actor))?actor:nullptr;
}
bool StillRemote(void* actor,const Packet& p){return Current() && RemoteActor(p)==actor;}
bool Flush(const Packet& p,uint32_t& pending) {
    // An unsuccessful older sound must not be overtaken by a newer one on the
    // same recipient's reliable lane. The caller carries the blocked mask.
    for(int slot=0;slot<net::kMaxPeers;++slot)if(pending&(1u<<slot)) {
        if(!g_session->IsSlotWorldReady(slot)){pending&=~(1u<<slot);continue;}
        if(g_session->SendReliableToSlot(slot,net::ReliableKind::HandToyAudio,&p,sizeof(p)))pending&=~(1u<<slot);
    }
    return pending==0;
}
void SampleLocal(uint64_t now) {
    const auto slot=Local();if(slot>=net::kMaxPeers)return;
    void* actor=hand_item::LocalHandActor();const auto generation=hand_item::Generation(slot);
    if(!generation || !M::IsDingus(actor)){g_rows[slot]={};return;}
    const auto epoch=g_epoch;ue_wrap::CachedObjRef live;live.Set(actor);M::State state{};
    if(!M::Read(actor,state) || epoch!=g_epoch || !Current() || !live.Get() ||
       actor!=hand_item::LocalHandActor() || generation!=hand_item::Generation(slot))return;
    auto& row=g_rows[slot];
    if(row.have && row.state.generation==generation && bool(row.state.playing)==state.playing &&
       bool(row.state.used)==state.used && row.state.value==state.volume && now-row.at<1000)return;
    row.state={generation,Next(),state.volume,slot,0,uint8_t(state.playing),uint8_t(state.used)};
    row.have=row.authorized=true;row.at=now;
    row.pending=g_session->role()==net::Role::Host?Mask(slot):1u;
}
void ApplyStates(uint64_t now) {
    const auto epoch=g_epoch;
    for(uint8_t slot=0;slot<net::kMaxPeers;++slot) {
        const auto row=g_rows[slot];if(!row.have || slot==Local())continue;
        if(now-row.at>5000){g_rows[slot]={};continue;}
        void* actor=RemoteActor(row.state);if(!actor)continue;
        ue_wrap::CachedObjRef live;live.Set(actor);
        if(g_session->role()==net::Role::Host && !row.authorized) {
            const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,slot,600.f).Authorize(actor));
            if(epoch!=g_epoch || !Current())return;
            if(!allowed || !live.Get() || !StillRemote(actor,row.state))continue;
        }
        const bool ok=M::Apply(actor,{row.state.playing!=0,row.state.used!=0,row.state.value});
        if(epoch!=g_epoch || !Current())return;
        if(!ok || !live.Get() || !StillRemote(actor,row.state) || !g_rows[slot].have ||
           g_rows[slot].state.sequence!=row.state.sequence)continue;
        if(!row.authorized && g_session->role()==net::Role::Host)g_rows[slot].pending=Mask(slot);
        g_rows[slot].authorized=true;
    }
}
void Capture(const SG::Call& c) {
    if(!Current() || c.fromOurCode || Local()>=net::kMaxPeers || g_pulses.size()>=32 ||
       !c.object || c.object!=hand_item::LocalHandActor() || !M::IsSqueakyToy(c.object))return;
    if(c.tag==kPlushUse) {if(!M::IsAltUse(c.function,c.locals))return;}
    else if(c.tag==kAction) {
        if(!c.locals || !c.function)return;
        const int off=R::FindParamOffset(c.function,L"action"),size=R::FunctionFrameSize(c.function);
        if(off<0 || off>=size || c.locals[off]!=6)return;
    }
    float pitch=0;if(!M::ReadSqueakPitch(c.object,pitch))return;
    Pulse pulse{};pulse.state={hand_item::Generation(Local()),Next(),pitch,Local(),1,0,0};
    pulse.localActor.Set(c.object);pulse.local=pulse.authorized=pulse.played=true;
    pulse.expires=active_drive::NowMs()+1500;
    pulse.pending=g_session->role()==net::Role::Host?Mask(Local()):1u;
    g_pulses.push_back(pulse);
}
void DrainPulses(uint64_t now) {
    const auto epoch=g_epoch;const auto count=g_pulses.size();
    uint32_t blocked=0,waitingOwners=0;
    for(size_t i=0;i<count;++i) {
        auto pulse=g_pulses.front();g_pulses.pop_front();auto& p=pulse.state;
        if(now>pulse.expires)continue;
        if(waitingOwners&(1u<<p.slot)){g_pulses.push_back(pulse);blocked|=pulse.pending;continue;}
        if(pulse.local) {
            if(!pulse.localActor.Get() || pulse.localActor.Get()!=hand_item::LocalHandActor())continue;
            const auto generation=hand_item::Generation(p.slot);
            if(!p.generation)p.generation=generation; // wait for initial HandItem announcement
            if(!generation){g_pulses.push_back(pulse);blocked|=pulse.pending;waitingOwners|=1u<<p.slot;continue;}
            if(p.generation!=generation)continue;
        } else {
            void* actor=RemoteActor(p);
            if(!actor){g_pulses.push_back(pulse);waitingOwners|=1u<<p.slot;continue;}
            ue_wrap::CachedObjRef live;live.Set(actor);
            if(!pulse.authorized && g_session->role()==net::Role::Host) {
                const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,p.slot,600.f).Authorize(actor));
                if(epoch!=g_epoch || !Current())return;
                if(!allowed || !live.Get() || !StillRemote(actor,p))continue;
                pulse.authorized=true;pulse.pending=Mask(p.slot);
            }
            if(!pulse.played) {
                const bool ok=M::Squeak(actor,p.value);
                if(epoch!=g_epoch || !Current())return;
                if(!ok){g_pulses.push_back(pulse);blocked|=pulse.pending;waitingOwners|=1u<<p.slot;continue;}
                pulse.played=true;
            }
        }
        const auto held=pulse.pending&blocked;
        pulse.pending&=~blocked;Flush(p,pulse.pending);pulse.pending|=held;
        blocked|=pulse.pending;
        if(pulse.pending)g_pulses.push_back(pulse);
    }
}
}
void Install(net::Session* s) {
    const auto world=ue_wrap::world_identity::Generation();
    if(g_session!=s || world!=g_world){OnDisconnect();g_session=s;g_world=world;}
    SG::WatchClassName(L"prop_dingus_C",L"playerHandUse_RMB",kRmb,nullptr,&Capture);
    SG::WatchClassName(L"prop_dingus_C",L"actionOptionIndex",kAction,nullptr,&Capture);
    SG::WatchClassName(L"prop_abplush_C",L"player_use",kPlushUse,nullptr,&Capture);
    SG::ResolvePendingNames();
}
void Tick() {
    if(!Current() || g_ticking)return;
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    const auto now=active_drive::NowMs(),epoch=g_epoch;
    if(now>=g_poll) {
        g_poll=now+250;SampleLocal(now);if(epoch!=g_epoch || !Current())return;
        ApplyStates(now);if(epoch!=g_epoch || !Current())return;
    }
    DrainPulses(now);if(epoch!=g_epoch || !Current())return;
    for(auto& row:g_rows)if(row.have && row.authorized && row.pending) {
        const bool local=row.state.slot==Local();
        if((local && row.state.generation!=hand_item::Generation(row.state.slot)) ||
           (!local && !RemoteActor(row.state)))continue;
        Flush(row.state,row.pending);
    }
}
void Receive(const Packet& p,uint8_t sender) {
    if(!Current() || p.slot>=net::kMaxPeers || p.slot==Local() || !p.generation || !p.sequence ||
       p.op>1 || p.playing>1 || p.used>1 || !std::isfinite(p.value) || p.value<0 || p.value>10 ||
       (p.op && (p.value==0 || p.playing || p.used)))return;
    const auto now=active_drive::NowMs();
    if(g_session->role()==net::Role::Host) {
        if(!sender || sender!=p.slot || !g_session->IsSlotWorldReady(sender))return;
        if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rateCount[sender]=0;}
        if(g_rateCount[sender]>=20)return;
        ++g_rateCount[sender];
    } else if(sender!=0)return;
    auto& seen=g_seen[p.slot][p.op];if(seen && !After(p.sequence,seen))return;seen=p.sequence;
    if(p.op) {
        if(g_pulses.size()<32)g_pulses.push_back({p,{},now+1500,0,false,g_session->role()==net::Role::Client,false});
    } else {g_rows[p.slot]={p,now,0,true,false};g_poll=0;}
}
void QueueConnect(int slot) {
    if(!Current() || g_session->role()!=net::Role::Host || slot<=0 || slot>=net::kMaxPeers)return;
    for(auto& row:g_rows)if(row.have && row.authorized && row.state.slot!=slot &&
        row.state.generation==hand_item::Generation(row.state.slot))row.pending|=1u<<slot;
    g_poll=0;
}
void OnPeerLeft(int slot) {
    if(slot<=0 || slot>=net::kMaxPeers)return;
    g_rows[slot]={};g_seen[slot][0]=g_seen[slot][1]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;
    for(auto& row:g_rows)row.pending&=~(1u<<slot);
    for(auto it=g_pulses.begin();it!=g_pulses.end();) {
        if(it->state.slot==slot)it=g_pulses.erase(it);
        else {it->pending&=~(1u<<slot);++it;}
    }
}
void OnDisconnect() {
    ++g_epoch;g_session=nullptr;g_world=g_sequence=0;g_poll=0;g_rows={};g_pulses.clear();
    for(int i=0;i<net::kMaxPeers;++i){g_seen[i][0]=g_seen[i][1]=0;g_rateAt[i]=0;g_rateCount[i]=0;}
}
}
