#include "coop/player/hand_toy_audio.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/element/intent_authority.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
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
struct Source {uint16_t person=0;uint32_t connection=0;uint64_t peer=0;};
struct Targets {std::array<Source,net::kMaxPeers> slots{};};
struct Row {Packet state{};uint64_t at=0;uint32_t pending=0;bool have=false,authorized=false;ue_wrap::CachedObjRef painted;Source source{};Targets targets{};};
struct Pulse {
    Packet state{};ue_wrap::CachedObjRef localActor;
    uint64_t expires=0;uint32_t pending=0;
    bool local=false,authorized=false,played=false;
    Source source{};Targets targets{};
};
struct Frame {ue_wrap::CachedObjRef actor,function;void* stack=nullptr;uint64_t invocation=0,epoch=0;uint32_t generation=0;uint8_t slot=0;Source source{};bool ready=false;int tag=0;};
std::deque<Frame> g_frames;
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0;
uint64_t g_epoch=0,g_poll=0;
bool g_ticking=false;
std::array<Row,net::kMaxPeers> g_rows{};
std::deque<Pulse> g_pulses;
uint32_t g_seen[net::kMaxPeers][2]{};
uint64_t g_rateAt[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};
uint16_t g_people[net::kMaxPeers]{};
uint32_t g_serial=0,g_connections[net::kMaxPeers]{};
uint64_t g_peerEpoch[net::kMaxPeers]{},g_now=0;
net::Role g_role=net::Role::Host;
bool g_running=false,g_linked=false;
size_t g_sendBudget=0;
constexpr int kRmb=0x48544130,kAction=0x48544131,kPlushUse=0x48544132;
bool Current(){return g_session && g_session->running() && g_session->connected() && g_role==g_session->role() &&
    g_serial==net::session_serial::Current() && g_running==g_session->running() && g_linked==g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
Source Address(int slot){return {roster_ledger::Get(slot).playerNo,g_session->peerGenerationForSlot(slot),g_peerEpoch[slot]};}
bool Same(const Source& a,const Source& b){return a.person==b.person && a.connection==b.connection && a.peer==b.peer;}
bool SourceCurrent(const Source& source,int slot){return Current() && slot>=0 && slot<net::kMaxPeers && source.person && Same(source,Address(slot));}
void Bind(Targets& targets,uint32_t mask){for(int slot=0;slot<net::kMaxPeers;++slot)if(mask&(1u<<slot))targets.slots[slot]=Address(slot);}
bool Fresh(uint64_t expires){return Current() && active_drive::NowMs()>=g_now && active_drive::NowMs()<expires;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
uint32_t Next(){if(!++g_sequence)++g_sequence;return g_sequence;}
uint8_t Local(){return players::Registry::Get().LocalPeerId();}
uint32_t Mask(uint8_t owner){uint32_t m=0;for(int s=1;s<net::kMaxPeers;++s)
    if(s!=owner && g_session->IsSlotWorldReady(s))m|=1u<<s;return m;}
void* RemoteActor(const Packet& p) {
    if(p.slot>=net::kMaxPeers || !p.playerNo || roster_ledger::Get(p.slot).playerNo!=p.playerNo ||
        p.slot==Local() || hand_item::MirrorGeneration(p.slot)!=p.generation)return nullptr;
    void* actor=hand_item::MirrorActorForSlot(p.slot);
    return (p.op?M::IsSqueakyToy(actor):M::IsDingus(actor))?actor:nullptr;
}
bool StillRemote(void* actor,const Packet& p){return Current() && RemoteActor(p)==actor;}
void Stop(ue_wrap::CachedObjRef actor,uint32_t world){
    const auto detached=[&]{if(world!=ue_wrap::world_identity::Generation() || !actor.Get() || actor.Get()==hand_item::LocalHandActor())return false;
        for(const auto& row:g_rows)if(row.painted.Is(actor.Raw()))return false;return true;};
    if(detached())M::StopMusic(actor.Get(),detached);
}
void Scope(net::Session* session){
    if(session!=g_session || g_world!=ue_wrap::world_identity::Generation() || g_serial!=net::session_serial::Current() ||
       (session && (g_role!=session->role() || g_running!=session->running() || g_linked!=session->connected()))){
        const auto epoch=g_epoch;OnDisconnect();if(g_epoch!=epoch+1 || g_session)return;
        g_session=session;g_world=ue_wrap::world_identity::Generation();g_serial=net::session_serial::Current();
        if(session){g_role=session->role();g_running=session->running();g_linked=session->connected();}
    }
    const auto now=active_drive::NowMs();
    if(now<g_now){const auto previous=g_rows;const auto world=g_world;++g_epoch;g_rows={};g_pulses.clear();g_frames.clear();g_poll=0;
        for(int slot=0;slot<net::kMaxPeers;++slot){g_rateAt[slot]=0;g_rateCount[slot]=0;}
        g_now=now;for(const auto& row:previous)Stop(row.painted,world);
    }else g_now=now;
}
bool Reconcile(){
    const auto epoch=g_epoch;
    for(int slot=0;slot<net::kMaxPeers;++slot){const auto person=roster_ledger::Get(slot).playerNo;
        const auto connection=g_session->peerGenerationForSlot(slot);
        if(g_people[slot]==person && g_connections[slot]==connection)continue;if(g_people[slot])OnPeerLeft(slot);
        if(epoch!=g_epoch || !Current() || roster_ledger::Get(slot).playerNo!=person)return false;g_people[slot]=person;g_connections[slot]=connection;}
    return true;
}
bool Flush(const Packet& p,uint32_t& pending,const Targets& targets,const Source& source,uint64_t expires=0) {
    // An unsuccessful older sound must not be overtaken by a newer one on the
    // same recipient's reliable lane. The caller carries the blocked mask.
    const auto epoch=g_epoch;
    for(int slot=0;slot<net::kMaxPeers;++slot)if(pending&(1u<<slot)) {
        if(!SourceCurrent(source,p.slot)){pending=0;break;}
        if(expires && !Fresh(expires))break;
        if(!g_session->IsSlotWorldReady(slot) || !SourceCurrent(targets.slots[slot],slot)){pending&=~(1u<<slot);continue;}
        if(!g_sendBudget)break;--g_sendBudget;
        const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::HandToyAudio,&p,sizeof(p));
        if(epoch!=g_epoch || !Current())return false;
        if(sent || !SourceCurrent(targets.slots[slot],slot))pending&=~(1u<<slot);
    }
    return pending==0;
}
void SampleLocal(uint64_t now) {
    const auto slot=Local();if(slot>=net::kMaxPeers)return;
    void* actor=hand_item::LocalHandActor();const auto generation=hand_item::Generation(slot);
    const auto person=roster_ledger::Get(slot).playerNo;
    if(!person || !generation || !M::IsDingus(actor)){g_rows[slot]={};return;}
    const auto epoch=g_epoch;ue_wrap::CachedObjRef live;live.Set(actor);M::State state{};
    if(!M::Read(actor,state) || epoch!=g_epoch || !Current() || !live.Get() ||
       actor!=hand_item::LocalHandActor() || generation!=hand_item::Generation(slot) || roster_ledger::Get(slot).playerNo!=person)return;
    auto& row=g_rows[slot];
    if(row.have && row.state.generation==generation && bool(row.state.playing)==state.playing &&
       bool(row.state.used)==state.used && row.state.value==state.volume && now-row.at<1000)return;
    row.state={generation,Next(),state.volume,slot,0,uint8_t(state.playing),uint8_t(state.used),person};
    row.have=row.authorized=true;row.at=now;
    row.pending=g_session->role()==net::Role::Host?Mask(slot):1u;
    row.source=Address(slot);Bind(row.targets,row.pending);
}
void ApplyStates(uint64_t now) {
    const auto epoch=g_epoch;
    for(uint8_t slot=0;slot<net::kMaxPeers;++slot) {
        const auto row=g_rows[slot];if(!row.have || slot==Local())continue;
        if(!SourceCurrent(row.source,slot))continue;
        void* actor=RemoteActor(row.state);
        if(row.painted.Raw() && (row.painted.Get()!=actor || now-row.at>5000)){
            g_rows[slot].painted={};Stop(row.painted,g_world);if(epoch!=g_epoch || !Current())return;
            if(!g_rows[slot].have || g_rows[slot].state.sequence!=row.state.sequence ||
                g_rows[slot].state.playerNo!=row.state.playerNo || g_rows[slot].state.generation!=row.state.generation)continue;
        }
        if(now-row.at>5000){g_rows[slot]={};continue;}
        if(!actor)continue;
        ue_wrap::CachedObjRef live;live.Set(actor);
        if(g_session->role()==net::Role::Host && !row.authorized) {
            const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,slot,600.f).Authorize(actor));
            if(epoch!=g_epoch || !Current())return;
            if(!allowed || !live.Get() || !StillRemote(actor,row.state) || !SourceCurrent(row.source,slot))continue;
        }
        const auto valid=[&]{return epoch==g_epoch && SourceCurrent(row.source,slot) && StillRemote(actor,row.state) && live.Get() &&
            g_rows[slot].have && g_rows[slot].state.sequence==row.state.sequence &&
            g_rows[slot].state.playerNo==row.state.playerNo && g_rows[slot].state.generation==row.state.generation;};
        if(!valid())continue;g_rows[slot].painted=live;
        const bool ok=M::Apply(actor,{row.state.playing!=0,row.state.used!=0,row.state.value},valid);
        if(epoch!=g_epoch || !Current())return;
        if(!ok || !valid())continue;
        if(!row.authorized && g_session->role()==net::Role::Host){g_rows[slot].pending=Mask(slot);Bind(g_rows[slot].targets,g_rows[slot].pending);}
        g_rows[slot].authorized=true;
    }
}
bool FrameCurrent(const Frame& frame){return Current() && frame.epoch==g_epoch && frame.actor.Get() && frame.function.Get() &&
    frame.slot==Local() && SourceCurrent(frame.source,frame.slot) && frame.actor.Get()==hand_item::LocalHandActor() &&
    frame.generation && frame.generation==hand_item::Generation(frame.slot) && SG::IsInvocationActive(frame.invocation);}
void PruneFrames(){std::erase_if(g_frames,[](const Frame& frame){return frame.epoch!=g_epoch || !frame.actor.Get() || !frame.function.Get() || !SG::IsInvocationActive(frame.invocation);});}
SG::Verdict BeforeCapture(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !c.invocation || !c.function || Local()>=net::kMaxPeers ||
       !c.object || c.object!=hand_item::LocalHandActor() || !M::IsSqueakyToy(c.object))return SG::Verdict::Run;
    if(!Reconcile())return SG::Verdict::Run;PruneFrames();
    if(g_frames.size()>=32 || std::any_of(g_frames.begin(),g_frames.end(),[&](const Frame& frame){return frame.invocation==c.invocation;}))return SG::Verdict::Run;
    Frame frame{};frame.actor.Set(c.object);frame.function.Set(c.function);frame.stack=c.stack;frame.invocation=c.invocation;frame.epoch=g_epoch;
    frame.slot=Local();frame.generation=hand_item::Generation(frame.slot);frame.source=Address(frame.slot);frame.tag=c.tag;
    if(!FrameCurrent(frame))return SG::Verdict::Run;
    g_frames.push_back(frame);
    bool selected=true;
    if(c.tag==kPlushUse)selected=M::IsAltUse(c.function,c.locals);
    else if(c.tag==kAction) {
        if(!c.locals)selected=false;
        else {const int off=R::FindParamOffset(c.function,L"action"),size=R::FunctionFrameSize(c.function);
            selected=off>=0 && off<size && FrameCurrent(frame) && c.locals[off]==6;}
    }
    const bool valid=selected && FrameCurrent(frame);
    const auto found=std::find_if(g_frames.begin(),g_frames.end(),[&](const Frame& item){return item.invocation==frame.invocation && item.epoch==frame.epoch;});
    if(found!=g_frames.end()){if(valid)found->ready=true;else g_frames.erase(found);}
    return SG::Verdict::Run;
}
void Capture(const SG::Call& c) {
    if(c.fromOurCode)return;
    const auto found=std::find_if(g_frames.begin(),g_frames.end(),[&](const Frame& frame){return frame.ready && frame.invocation==c.invocation &&
        frame.actor.Get()==c.object && frame.function.Get()==c.function && frame.stack==c.stack && frame.tag==c.tag;});
    if(found==g_frames.end())return;const auto frame=*found;g_frames.erase(found);
    if(!FrameCurrent(frame) || g_pulses.size()>=32)return;
    float pitch=0;if(!M::ReadSqueakPitch(c.object,pitch) || !FrameCurrent(frame) || g_pulses.size()>=32)return;
    Pulse pulse{};pulse.state={frame.generation,Next(),pitch,frame.slot,1,0,0,frame.source.person};
    pulse.localActor=frame.actor;pulse.local=pulse.authorized=pulse.played=true;pulse.source=frame.source;
    pulse.expires=active_drive::NowMs()+1500;
    pulse.pending=g_session->role()==net::Role::Host?Mask(frame.slot):1u;Bind(pulse.targets,pulse.pending);
    g_pulses.push_back(pulse);
}
void DrainPulses(uint64_t now) {
    const auto epoch=g_epoch;const auto count=g_pulses.size();
    uint32_t blocked=0,waitingOwners=0;
    for(size_t i=0;i<count && !g_pulses.empty();++i) {
        auto pulse=g_pulses.front();g_pulses.pop_front();auto& p=pulse.state;
        if(!Fresh(pulse.expires) || !SourceCurrent(pulse.source,p.slot))continue;
        if(waitingOwners&(1u<<p.slot)){g_pulses.push_back(pulse);blocked|=pulse.pending;continue;}
        if(pulse.local) {
            if(!pulse.localActor.Get() || pulse.localActor.Get()!=hand_item::LocalHandActor())continue;
            const auto person=roster_ledger::Get(p.slot).playerNo;
            if(!p.playerNo || p.playerNo!=person)continue;
            const auto generation=hand_item::Generation(p.slot);
            if(!generation || !p.generation || p.generation!=generation)continue;
        } else {
            void* actor=RemoteActor(p);
            if(!actor){g_pulses.push_back(pulse);waitingOwners|=1u<<p.slot;continue;}
            ue_wrap::CachedObjRef live;live.Set(actor);
            if(!pulse.authorized && g_session->role()==net::Role::Host) {
                const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,p.slot,600.f).Authorize(actor));
                if(epoch!=g_epoch || !Current())return;
                if(!allowed || !live.Get() || !StillRemote(actor,p) || !SourceCurrent(pulse.source,p.slot) || !Fresh(pulse.expires))continue;
                pulse.authorized=true;pulse.pending=Mask(p.slot);Bind(pulse.targets,pulse.pending);
            }
            if(!pulse.played) {
                const auto valid=[&]{return epoch==g_epoch && SourceCurrent(pulse.source,p.slot) && Fresh(pulse.expires) && live.Get() && StillRemote(actor,p);};
                const bool ok=M::Squeak(actor,p.value,valid);
                if(epoch!=g_epoch || !Current())return;
                if(!ok){if(SourceCurrent(pulse.source,p.slot) && Fresh(pulse.expires))g_pulses.push_back(pulse);blocked|=pulse.pending;waitingOwners|=1u<<p.slot;continue;}
                pulse.played=true;
            }
        }
        const auto held=pulse.pending&blocked;
        pulse.pending&=~blocked;Flush(p,pulse.pending,pulse.targets,pulse.source,pulse.expires);pulse.pending|=held;
        if(epoch!=g_epoch || !Current())return;
        if(!SourceCurrent(pulse.source,p.slot) || !Fresh(pulse.expires))continue;
        blocked|=pulse.pending;
        if(pulse.pending)g_pulses.push_back(pulse);
    }
}
}
void Install(net::Session* s) {
    Scope(s);
    SG::WatchClassName(L"prop_dingus_C",L"playerHandUse_RMB",kRmb,&BeforeCapture,&Capture);
    SG::WatchClassName(L"prop_dingus_C",L"actionOptionIndex",kAction,&BeforeCapture,&Capture);
    SG::WatchClassName(L"prop_abplush_C",L"player_use",kPlushUse,&BeforeCapture,&Capture);
    SG::ResolvePendingNames();
}
void Tick() {
    Scope(g_session);
    if(!Current() || g_ticking)return;
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    if(!Reconcile())return;
    const auto now=active_drive::NowMs(),epoch=g_epoch;
    g_sendBudget=64;PruneFrames();
    if(now>=g_poll) {
        g_poll=now+250;SampleLocal(now);if(epoch!=g_epoch || !Current())return;
        ApplyStates(now);if(epoch!=g_epoch || !Current())return;
    }
    DrainPulses(now);if(epoch!=g_epoch || !Current())return;
    for(uint8_t owner=0;owner<net::kMaxPeers;++owner){const auto row=g_rows[owner];if(!row.have || !row.authorized || !row.pending)continue;
        const bool local=row.state.slot==Local();
        if((local && row.state.generation!=hand_item::Generation(row.state.slot)) ||
           (!local && !RemoteActor(row.state)))continue;
        if(!SourceCurrent(row.source,owner))continue;
        auto pending=row.pending;Flush(row.state,pending,row.targets,row.source);if(epoch!=g_epoch || !Current())return;
        auto& latest=g_rows[owner];if(latest.state.sequence==row.state.sequence && latest.state.playerNo==row.state.playerNo &&
            latest.state.generation==row.state.generation){
                for(int slot=0;slot<net::kMaxPeers;++slot)if(!(pending&(1u<<slot)) && Same(latest.targets.slots[slot],row.targets.slots[slot]))latest.pending&=~(1u<<slot);
            }
    }
}
void Receive(const Packet& packet,uint8_t sender) {
    const Packet p=packet;Scope(g_session);
    if(!Current() || p.slot>=net::kMaxPeers || p.slot==Local() || !p.generation || !p.sequence || !p.playerNo ||
       p.op>1 || p.playing>1 || p.used>1 || !std::isfinite(p.value) || p.value<0 || p.value>10 ||
       (p.op && (p.value==0 || p.playing || p.used)))return;
    const auto epoch=g_epoch;const auto connection=g_session->peerGenerationForSlot(p.slot);
    if(!Reconcile() || !Current() || epoch!=g_epoch || g_session->peerGenerationForSlot(p.slot)!=connection || roster_ledger::Get(p.slot).playerNo!=p.playerNo)return;
    const auto now=active_drive::NowMs();
    if(p.op && g_pulses.size()>=32)return;
    if(g_session->role()==net::Role::Host) {
        if(!sender || sender!=p.slot || !g_session->IsSlotWorldReady(sender))return;
        if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rateCount[sender]=0;}
        if(g_rateCount[sender]>=20)return;
        ++g_rateCount[sender];
    } else if(sender!=0)return;
    auto& seen=g_seen[p.slot][p.op];if(seen && !After(p.sequence,seen))return;
    seen=p.sequence;
    if(p.op) {
        g_pulses.push_back({p,{},now+1500,0,false,g_session->role()==net::Role::Client,false});g_pulses.back().source=Address(p.slot);
    } else {const auto painted=g_rows[p.slot].painted;g_rows[p.slot]={p,now,0,true,false,painted};g_rows[p.slot].source=Address(p.slot);g_poll=0;}
}
void QueueConnect(int slot) {
    if(!Current() || g_session->role()!=net::Role::Host || slot<=0 || slot>=net::kMaxPeers)return;
    if(!Reconcile())return;
    for(auto& row:g_rows)if(row.have && row.authorized && row.state.slot!=slot &&
        row.state.generation==hand_item::Generation(row.state.slot)){row.pending|=1u<<slot;Bind(row.targets,1u<<slot);}
    g_poll=0;
}
void OnPeerLeft(int slot) {
    if(slot<0 || slot>=net::kMaxPeers)return;const auto painted=g_rows[slot].painted;++g_peerEpoch[slot];
    std::erase_if(g_frames,[slot](const Frame& frame){return frame.slot==slot;});
    g_rows[slot]={};g_seen[slot][0]=g_seen[slot][1]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;
    for(auto& row:g_rows)row.pending&=~(1u<<slot);
    for(auto it=g_pulses.begin();it!=g_pulses.end();) {
        if(it->state.slot==slot)it=g_pulses.erase(it);
        else {it->pending&=~(1u<<slot);++it;}
    }
    g_people[slot]=0;Stop(painted,g_world);
}
void OnDisconnect() {
    const auto previous=g_rows;const auto world=g_world;++g_epoch;g_session=nullptr;g_world=g_sequence=g_serial=0;g_poll=g_now=0;g_running=g_linked=false;g_rows={};g_pulses.clear();g_frames.clear();
    for(int i=0;i<net::kMaxPeers;++i){g_seen[i][0]=g_seen[i][1]=0;g_rateAt[i]=0;g_rateCount[i]=0;g_people[i]=0;}
    for(const auto& row:previous)Stop(row.painted,world);
}
}
