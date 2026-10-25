#include "coop/props/dingus_music_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/wire_key_util.h"
#include "coop/element/intent_authority.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_element_tracker.h"
#include "ue_wrap/actors/dingus_music.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/script_gate.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <vector>
namespace coop::dingus_music_sync {
namespace {
namespace M=ue_wrap::dingus_music;
namespace SG=ue_wrap::script_gate;
namespace R=ue_wrap::reflection;
using Packet=net::DingusMusicPayload;
struct Targets {std::array<uint16_t,net::kMaxPeers> people{};std::array<uint64_t,net::kMaxPeers> epochs{};std::array<uint32_t,net::kMaxPeers> connections{};};
struct Row {Packet state{};ue_wrap::CachedObjRef actor;uint32_t pending=0;uint64_t at=0;Targets targets{};
    M::Clock clock{};M::Playback playback{};uint64_t clockAt=0;double position=0;uint32_t episode=0;float transit=0;};
struct Intent {Packet state{};uint64_t expires=0;uint8_t slot=0;uint64_t peerEpoch=0;};
struct Pulse {Packet state{};uint64_t expires=0;uint32_t pending=0;bool play=false;Targets targets{};};
struct Capture {ue_wrap::CachedObjRef actor,function;void* stack=nullptr;std::wstring key;uint64_t invocation=0,epoch=0,peerEpoch=0;uint16_t person=0;uint8_t slot=0;bool alt=false,ready=false;};
std::unordered_map<std::wstring,Row> g_rows;
std::deque<Intent> g_intents;
std::deque<Pulse> g_pulses;
std::vector<Capture> g_captures;
net::Session* g_session=nullptr;
net::Role g_role=net::Role::Host;
std::array<uint64_t,net::kMaxPeers> g_peerEpoch{};
uint64_t g_sampleSequence=0;
std::unordered_map<std::wstring,uint64_t> g_sampleRevision;
uint32_t g_world=0,g_sequence=0,g_request=0,g_seen[net::kMaxPeers]{};
uint32_t g_lastPulse=0;
uint32_t g_episode=0;
uint64_t g_epoch=0,g_poll=0,g_rateAt[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};
uint16_t g_people[net::kMaxPeers]{};
bool g_ticking=false;
uint32_t g_serial=0,g_connections[net::kMaxPeers]{};
bool g_running=false,g_linked=false;
uint64_t g_now=0;
size_t g_sendBudget=0;
uint32_t Next(uint32_t& n){if(!++n)++n;return n;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
bool Current(){return g_session && g_session->running() && g_session->connected() && g_session->role()==g_role &&
    g_serial==net::session_serial::Current() && g_running==g_session->running() && g_linked==g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
void Target(Targets& targets,int slot){targets.people[slot]=roster_ledger::Get(slot).playerNo;targets.epochs[slot]=g_peerEpoch[slot];targets.connections[slot]=g_session->peerGenerationForSlot(slot);}
bool Recipient(const Targets& targets,int slot){return targets.people[slot] && targets.people[slot]==roster_ledger::Get(slot).playerNo && targets.epochs[slot]==g_peerEpoch[slot] && targets.connections[slot]==g_session->peerGenerationForSlot(slot);}
void Reconcile(){for(int slot=1;slot<net::kMaxPeers;++slot){const auto person=roster_ledger::Get(slot).playerNo;
    const auto connection=g_session->peerGenerationForSlot(slot);
    if(person!=g_people[slot] || connection!=g_connections[slot]){if(g_people[slot])OnPeerLeft(slot);g_people[slot]=person;g_connections[slot]=connection;}}}
bool Fresh(uint64_t expires){return Current() && active_drive::NowMs()>=g_now && active_drive::NowMs()<expires;}
void Scope(net::Session* session){
    if(session!=g_session || g_world!=ue_wrap::world_identity::Generation() || g_serial!=net::session_serial::Current() ||
       (session && (session->role()!=g_role || session->running()!=g_running || session->connected()!=g_linked))){
        OnDisconnect();g_session=session;g_world=ue_wrap::world_identity::Generation();g_serial=net::session_serial::Current();
        if(session){g_role=session->role();g_running=session->running();g_linked=session->connected();}
    }
    const auto now=active_drive::NowMs();
    if(now<g_now){++g_epoch;g_captures.clear();g_intents.clear();g_pulses.clear();g_rows.clear();g_sampleRevision.clear();g_poll=0;
        std::fill(std::begin(g_rateAt),std::end(g_rateAt),0);std::fill(std::begin(g_rateCount),std::end(g_rateCount),0);}
    g_now=now;
}
bool Eligible(void* a){return M::IsDingus(a) && !hand_item::IsHandAxisActor(a) &&
    ue_wrap::world_identity::CurrentWorld() && ue_wrap::world_identity::WorldOf(a)==ue_wrap::world_identity::CurrentWorld();}
bool SoundEligible(void* a){return M::IsSqueakyToy(a) && !hand_item::IsHandAxisActor(a) &&
    ue_wrap::world_identity::CurrentWorld() && ue_wrap::world_identity::WorldOf(a)==ue_wrap::world_identity::CurrentWorld();}
bool ValidKey(const std::wstring& key){if(key.empty() || key==L"None" || key.size()>sizeof(Packet{}.key.data))return false;
    return std::all_of(key.begin(),key.end(),[](wchar_t c){return c>=33 && c<=126;});}
std::wstring Key(void* actor){auto key=ue_wrap::prop::GetInteractableKeyString(actor);return ValidKey(key)?key:std::wstring{};}
bool SameKey(void* actor,const std::wstring& key,uint64_t epoch){
    ue_wrap::CachedObjRef identity;identity.Set(actor);
    return Current() && epoch==g_epoch && identity.Get() && Key(actor)==key &&
        Current() && epoch==g_epoch && identity.Get();
}
constexpr int kPlushUse=0x44494E48;
bool CaptureCurrent(const Capture& capture){return capture.ready && Current() && capture.epoch==g_epoch && capture.actor.Get() && capture.function.Get() &&
    !capture.key.empty() && SameKey(capture.actor.Get(),capture.key,capture.epoch) &&
    capture.person && capture.person==roster_ledger::Get(capture.slot).playerNo && capture.peerEpoch==g_peerEpoch[capture.slot] &&
    (g_role==net::Role::Host?capture.slot==0:capture.slot==players::Registry::Get().LocalPeerId()) && SG::IsInvocationActive(capture.invocation);}
void PruneCaptures(){std::erase_if(g_captures,[](const Capture& row){return row.epoch!=g_epoch || !SG::IsInvocationActive(row.invocation) || !row.actor.Get() || !row.function.Get();});}
Capture CaptureAction(const SG::Call& call){
    if(!Current() || call.fromOurCode || !call.invocation || !call.function || !SoundEligible(call.object))return {};
    Reconcile();PruneCaptures();const auto slot=g_role==net::Role::Host?0:players::Registry::Get().LocalPeerId();
    if(slot>=net::kMaxPeers || g_captures.size()>=32)return {};Capture row{};row.actor.Set(call.object);row.invocation=call.invocation;
    row.function.Set(call.function);row.stack=call.stack;
    row.slot=uint8_t(slot);row.person=roster_ledger::Get(slot).playerNo;row.peerEpoch=g_peerEpoch[slot];row.epoch=g_epoch;
    if(std::any_of(g_captures.begin(),g_captures.end(),[&](const Capture& c){return c.invocation==row.invocation;}))return {};
    g_captures.push_back(row); // Own capacity before key/parameter preparation can re-enter.
    row.key=Key(call.object);
    row.ready=true;
    if(CaptureCurrent(row) && call.tag==kPlushUse)row.alt=M::IsAltUse(call.function,call.locals);
    const bool valid=CaptureCurrent(row);
    auto found=std::find_if(g_captures.begin(),g_captures.end(),[&](const Capture& c){return c.invocation==row.invocation && c.epoch==row.epoch;});
    if(found==g_captures.end())return {};
    if(valid){*found=row;return row;}g_captures.erase(found);return {};
}
Capture TakeCapture(const SG::Call& call){
    const auto it=std::find_if(g_captures.begin(),g_captures.end(),[&](const Capture& row){return row.ready && row.invocation==call.invocation && row.actor.Raw()==call.object && row.function.Get()==call.function && row.stack==call.stack;});
    if(it==g_captures.end())return {};auto row=*it;g_captures.erase(it);return row;
}
uint32_t Mask(){uint32_t m=0;for(int s=1;s<net::kMaxPeers;++s)if(g_session->IsSlotWorldReady(s))m|=1u<<s;return m;}
void Sample(void* actor,uint64_t now,bool restarted=false) {
    const auto epoch=g_epoch,capture=++g_sampleSequence;if(!Current() || g_role!=net::Role::Host || !Eligible(actor) || epoch!=g_epoch || !Current())return;
    ue_wrap::CachedObjRef live;live.Set(actor);
    const auto key=Key(actor);M::State state{};
    if(key.empty() || !live.Get() || !Current() || epoch!=g_epoch || !M::Read(actor,state) || !SameKey(actor,key,epoch) || !Eligible(actor))return;
    M::Clock clock{};const bool clockReady=M::ReadClock(actor,clock,[&]{return SameKey(actor,key,epoch) && live.Get() && Eligible(actor);});
    if(!SameKey(actor,key,epoch) || !live.Get())return;
    auto found=g_rows.find(key);if(found==g_rows.end() && g_rows.size()>=256)return;
    if(g_sampleRevision[key]>capture)return;g_sampleRevision[key]=capture;
    auto& row=g_rows[key];const bool sameActor=row.actor.Is(actor);
    const bool sameClock=clockReady && row.clock.audio.Is(clock.audio.Get()) && row.clock.sound.Is(clock.sound.Get());
    if(state.playing && clockReady && (restarted || (sameActor && !row.state.playing))){row.episode=Next(g_episode);row.position=0;}
    else if(state.playing && sameActor && sameClock && row.episode){
        row.position=std::fmod(row.position+double(now>=row.clockAt?now-row.clockAt:0)/1000.*row.clock.pitch,double(row.clock.duration));
    }else {row.episode=0;row.position=0;}
    row.clock=clock;row.clockAt=now;
    const bool changed=!sameActor || bool(row.state.playing)!=state.playing || row.state.episode!=row.episode ||
        bool(row.state.used)!=state.used || row.state.volume!=state.volume || row.state.pitch!=state.pitch;
    row.actor=live;
    if(changed || !row.state.sequence || now-row.at>=1000) {
        row.state={};net::WireKeyFromString(key,row.state.key);
        row.state.sequence=Next(g_sequence);row.state.playing=uint8_t(state.playing);
        row.state.used=uint8_t(state.used);row.state.volume=state.volume;
        row.state.pitch=state.pitch;row.state.episode=row.episode;row.state.elapsed=row.episode?float(row.position):0;
        row.pending=Mask();row.at=now;
        for(int slot=1;slot<net::kMaxPeers;++slot)if(row.pending&(1u<<slot))Target(row.targets,slot);
    }
}
Intent LocalIntent(Packet packet,uint64_t ttl){
    Reconcile();Intent item{};item.state=packet;item.expires=active_drive::NowMs()+ttl;item.slot=players::Registry::Get().LocalPeerId();
    if(item.slot<net::kMaxPeers){item.state.playerNo=roster_ledger::Get(item.slot).playerNo;item.peerEpoch=g_peerEpoch[item.slot];}
    return item;
}
bool OwnIntent(const Intent& item){return Current() && item.slot>0 && item.slot<net::kMaxPeers &&
    item.slot==players::Registry::Get().LocalPeerId() && item.state.playerNo && item.state.playerNo==roster_ledger::Get(item.slot).playerNo && item.peerEpoch==g_peerEpoch[item.slot];}
void Request(void* actor,uint8_t op,const Capture& captured) {
    if(!Eligible(actor) || g_intents.size()>=32)return;
    const auto epoch=g_epoch;ue_wrap::CachedObjRef live;live.Set(actor);auto item=LocalIntent({},3000);
    const auto key=Key(actor);if(key.empty() || epoch!=g_epoch || !Current() || !live.Get())return;
    if(!OwnIntent(item) || !CaptureCurrent(captured) || item.slot!=captured.slot || item.state.playerNo!=captured.person || item.peerEpoch!=captured.peerEpoch)return;
    auto& p=item.state;net::WireKeyFromString(key,p.key);p.sequence=Next(g_request);p.op=op;
    g_intents.push_back(item);
}
SG::Verdict BeforeAction(const SG::Call& c) {
    const auto captured=CaptureAction(c);
    if(!Current() || c.fromOurCode || !Eligible(c.object) || g_session->role()!=net::Role::Client)return SG::Verdict::Run;
    if(!c.function || !c.locals)return SG::Verdict::Cancel;
    const int off=R::FindParamOffset(c.function,L"action"),size=R::FunctionFrameSize(c.function);
    if(off<0 || off>=size)return SG::Verdict::Cancel;
    if(c.locals[off]!=5)return SG::Verdict::Run;
    Request(c.object,1,captured);TakeCapture(c);return SG::Verdict::Cancel;
}
SG::Verdict BeforeTouch(const SG::Call& c) {
    const auto captured=CaptureAction(c);
    if(Current() && !c.fromOurCode && g_session->role()==net::Role::Client && Eligible(c.object)) {
        M::State state{};const auto epoch=g_epoch;
        if(M::Read(c.object,state) && !state.used && Current() && epoch==g_epoch)Request(c.object,2,captured);
    }
    return SG::Verdict::Run;
}
void QueuePulse(Packet p,int origin) {
    if(g_pulses.size()>=32)return;
    p.op=4;p.sequence=Next(g_sequence);p.playerNo=0;
    Pulse pulse{p,active_drive::NowMs()+1500,Mask()&~(1u<<origin),origin!=0};
    for(int slot=1;slot<net::kMaxPeers;++slot)if(pulse.pending&(1u<<slot))Target(pulse.targets,slot);g_pulses.push_back(pulse);
}
void AfterAction(const SG::Call& c) {
    const auto captured=TakeCapture(c);
    if(!CaptureCurrent(captured) || c.fromOurCode || !SoundEligible(c.object) || !c.function || !c.locals)return;
    if(c.tag==kPlushUse) {if(!captured.alt)return;}
    else {
        const int off=R::FindParamOffset(c.function,L"action"),size=R::FunctionFrameSize(c.function);
        if(off<0 || off>=size)return;
        if(c.locals[off]==5 && g_role==net::Role::Host && Eligible(c.object)){if(CaptureCurrent(captured))Sample(c.object,active_drive::NowMs(),true);return;}
        if(c.locals[off]!=6)return;
    }
    const auto epoch=g_epoch;ue_wrap::CachedObjRef live;live.Set(c.object);auto item=LocalIntent({},1500);
    const auto key=Key(c.object);float pitch=0;
    if(key.empty() || epoch!=g_epoch || !Current() || !live.Get() || !M::ReadSqueakPitch(c.object,pitch) || epoch!=g_epoch || !Current() || !live.Get() || !CaptureCurrent(captured))return;
    Packet p{};net::WireKeyFromString(key,p.key);p.volume=pitch;
    if(g_session->role()==net::Role::Host)QueuePulse(p,0);
    else if(g_intents.size()<32) {
        if(!OwnIntent(item))return;p.playerNo=item.state.playerNo;p.op=3;p.sequence=Next(g_request);item.state=p;
        g_intents.push_back(item);
    }
}
void AfterTouch(const SG::Call& c){const auto captured=TakeCapture(c);if(CaptureCurrent(captured) && g_role==net::Role::Host && !c.fromOurCode && Eligible(c.object))Sample(c.object,active_drive::NowMs());}
void DrainPulses() {
    const auto epoch=g_epoch;const size_t budget=(std::min)(size_t(4),g_pulses.size());
    uint32_t blocked=0;std::deque<Pulse> retained;
    for(size_t i=0;i<budget;++i) {
        auto pulse=g_pulses.front();g_pulses.pop_front();
        if(!Fresh(pulse.expires))continue;
        if(pulse.play) {
            const auto key=net::StringFromWireKey(pulse.state.key);
            void* actor=prop_element_tracker::FindLiveActorByKey(key);
            ue_wrap::CachedObjRef live;live.Set(actor);
            const auto valid=[&]{return SameKey(actor,key,epoch) && live.Get() && SoundEligible(actor) && Fresh(pulse.expires);};
            if(SoundEligible(actor) && M::Squeak(actor,pulse.state.volume,valid))pulse.play=false;
            if(epoch!=g_epoch || !Current())return;
        }
        for(int slot=1;slot<net::kMaxPeers;++slot)if((pulse.pending&~blocked)&(1u<<slot)) {
            if(!Fresh(pulse.expires))break;
            if(!g_session->IsSlotWorldReady(slot) || !Recipient(pulse.targets,slot)){pulse.pending&=~(1u<<slot);continue;}
            if(!g_sendBudget)break;--g_sendBudget;
            const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::DingusMusic,&pulse.state,sizeof(pulse.state));
            if(epoch!=g_epoch || !Current())return;
            if(sent || !Recipient(pulse.targets,slot))pulse.pending&=~(1u<<slot);
        }
        blocked|=pulse.pending;
        if(Fresh(pulse.expires) && (pulse.play || pulse.pending))retained.push_back(pulse);
    }
    // Keep retries before the untouched tail; rotating the first four to the
    // back lets a later sequence overtake them and the receiver rejects them.
    g_pulses.insert(g_pulses.begin(),retained.begin(),retained.end());
}
}
void Install(net::Session* s) {
    Scope(s);
    SG::WatchClassName(L"prop_dingus_C",L"actionOptionIndex",0x44494E47,&BeforeAction,&AfterAction);
    SG::WatchClassName(L"prop_dingus_C",L"touch",0x44494E47,&BeforeTouch,&AfterTouch);
    SG::WatchClassName(L"prop_abplush_C",L"player_use",kPlushUse,&BeforeAction,&AfterAction);
    SG::ResolvePendingNames();
}
void Tick() {
    Scope(g_session);
    if(!Current() || g_ticking)return;
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    Reconcile();
    PruneCaptures();
    const auto epoch=g_epoch,now=active_drive::NowMs();
    g_sendBudget=64;
    if(g_session->role()==net::Role::Host && !g_session->AnyWorldReadyPeer())return;
    DrainPulses();if(epoch!=g_epoch || !Current())return;
    if(g_session->role()==net::Role::Client) {
        while(!g_intents.empty()) {
            const auto item=g_intents.front();
            if(!Fresh(item.expires) || !OwnIntent(item)){g_intents.pop_front();continue;}
            if(!g_session->IsSlotWorldReady(0))break;
            if(!g_sendBudget)break;--g_sendBudget;
            const bool sent=g_session->SendReliableToSlot(0,net::ReliableKind::DingusMusic,&item.state,sizeof(item.state));
            if(epoch!=g_epoch || !Current())return;if(!OwnIntent(item))continue;if(!sent)break;
            if(!g_intents.empty() && g_intents.front().state.sequence==item.state.sequence)g_intents.pop_front();
        }
    }
    if(now>=g_poll) {
        g_poll=now+250;
        if(g_session->role()==net::Role::Host) {
            std::array<ue_wrap::CachedObjRef,256> actors{};const auto count=M::Collect(actors.data(),actors.size());
            for(size_t i=0;i<count;++i){Sample(actors[i].Get(),now);if(epoch!=g_epoch || !Current())return;}
        } else {
            // Native audio calls can re-enter teardown: iterate a copy and never
            // retain an unordered_map iterator/reference across engine calls.
            const auto rows=g_rows;
            for(const auto& [key,row]:rows) {
                void* actor=row.actor.Get();if(!actor || !SameKey(actor,key,epoch))actor=prop_element_tracker::FindLiveActorByKey(key);
                if(epoch!=g_epoch || !Current())return;
                ue_wrap::CachedObjRef live;live.Set(actor);
                const auto valid=[&]{if(!live.Get() || !Eligible(actor) || !SameKey(actor,key,epoch))return false;
                    const auto found=g_rows.find(key);return found!=g_rows.end() && found->second.state.sequence==row.state.sequence;};
                if(!valid())continue;
                M::State desired{row.state.playing!=0,row.state.used!=0,row.state.volume,row.state.pitch,row.state.elapsed,row.state.episode};
                if(desired.episode)desired.elapsed+=float(now>=row.at?now-row.at:0)/1000.f*desired.pitch+row.transit*desired.pitch;
                auto playback=row.playback;M::ApplyPlayback(actor,desired,playback,valid);
                if(epoch!=g_epoch || !Current())return;
                const auto it=g_rows.find(key);
                if(it!=g_rows.end() && it->second.state.sequence==row.state.sequence){it->second.actor=live;it->second.playback=playback;}
            }
        }
    }
    std::vector<std::wstring> keys;for(const auto& [key,row]:g_rows)keys.push_back(key);
    for(const auto& key:keys) {
        auto it=g_rows.find(key);if(it==g_rows.end())continue;const auto row=it->second;
        const auto sourceCurrent=[&]{if(!Eligible(row.actor.Get()) || !SameKey(row.actor.Get(),key,epoch))return false;
            const auto found=g_rows.find(key);return found!=g_rows.end() && found->second.state.sequence==row.state.sequence;};
        const bool stale=g_session->role()==net::Role::Host && !sourceCurrent();
        if(epoch!=g_epoch || !Current())return;
        it=g_rows.find(key);if(it==g_rows.end() || it->second.state.sequence!=row.state.sequence)continue;
        if(stale || now-row.at>10000){g_rows.erase(it);g_sampleRevision.erase(key);continue;}
        if(g_session->role()!=net::Role::Host)continue;
        for(int slot=1;slot<net::kMaxPeers;++slot)if(row.pending&(1u<<slot)) {
            if(!sourceCurrent()){if(epoch!=g_epoch || !Current())return;break;}
            const bool ready=g_session->IsSlotWorldReady(slot) && Recipient(row.targets,slot);
            if(ready && !g_sendBudget)break;
            if(ready)--g_sendBudget;
            auto outgoing=row.state;if(outgoing.episode)outgoing.elapsed+=float(now>=row.at?now-row.at:0)/1000.f*outgoing.pitch;
            const bool sent=!ready || g_session->SendReliableToSlot(slot,net::ReliableKind::DingusMusic,&outgoing,sizeof(outgoing));
            if(epoch!=g_epoch || !Current())return;
            it=g_rows.find(key);if(sent && it!=g_rows.end() && it->second.state.sequence==row.state.sequence &&
                Recipient(row.targets,slot) && it->second.targets.epochs[slot]==row.targets.epochs[slot] && it->second.targets.people[slot]==row.targets.people[slot])it->second.pending&=~(1u<<slot);
        }
    }
}
void Receive(const Packet& packet,uint8_t sender) {
    const Packet p=packet;
    Scope(g_session);
    if(!Current() || !p.sequence || !p.key.len || p.key.len>sizeof(p.key.data) || p.op>4 ||
       p.playing>1 || p.used>1 || p.reserved || !std::isfinite(p.volume) || p.volume<0 || p.volume>10)return;
    if(p.op>=3 && (p.volume<=0 || p.playing || p.used))return;
    if(p.op==0){if(p.playerNo || !std::isfinite(p.pitch) || p.pitch<=0 || p.pitch>10 || !std::isfinite(p.elapsed) || p.elapsed<0 || p.elapsed>1.e6f ||
        (p.episode && !p.playing) || (!p.episode && p.elapsed))return;}
    else if(p.elapsed || p.pitch || p.episode || (p.op==4?p.playerNo!=0:p.playerNo==0))return;
    const auto key=net::StringFromWireKey(p.key);if(!ValidKey(key))return;
    const auto now=active_drive::NowMs(),epoch=g_epoch;
    Reconcile();
    if(g_session->role()==net::Role::Host) {
        if(sender==0 || sender>=net::kMaxPeers || !g_session->IsSlotWorldReady(sender) || !p.op || p.op>3 ||
           p.playerNo!=roster_ledger::Get(sender).playerNo ||
           p.playing || p.used || (p.op<3 && p.volume) || (g_seen[sender] && !After(p.sequence,g_seen[sender])))return;
        if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rateCount[sender]=0;}
        if(g_rateCount[sender]>=8)return;
        if(p.op==3 && g_pulses.size()>=32)return;
        ++g_rateCount[sender];if(p.op!=3)g_seen[sender]=p.sequence;
        const auto person=roster_ledger::Get(sender).playerNo;const auto peerEpoch=g_peerEpoch[sender];
        const auto connection=g_session->peerGenerationForSlot(sender);
        void* actor=prop_element_tracker::FindLiveActorByKey(key);if(!(p.op==3?SoundEligible(actor):Eligible(actor)))return;
        ue_wrap::CachedObjRef live;live.Set(actor);
        if(!element::IntentTarget::ForClientIntent(*g_session,sender,400.f).Authorize(actor) ||
           !Current() || epoch!=g_epoch || !live.Get() || !person || roster_ledger::Get(sender).playerNo!=person || g_peerEpoch[sender]!=peerEpoch || g_session->peerGenerationForSlot(sender)!=connection)return;
        const auto sourceCurrent=[&]{return Current() && epoch==g_epoch && live.Get() && g_peerEpoch[sender]==peerEpoch && roster_ledger::Get(sender).playerNo==person &&
            g_session->peerGenerationForSlot(sender)==connection && g_session->IsSlotWorldReady(sender) && (p.op==3?SoundEligible(actor):Eligible(actor));};
        const auto valid=[&]{return sourceCurrent() && Key(actor)==key && sourceCurrent();};
        if(!valid())return;
        if(p.op==3){if(g_pulses.size()>=32 || (g_seen[sender] && !After(p.sequence,g_seen[sender])))return;
            QueuePulse(p,sender);g_seen[sender]=p.sequence;return;}
        if(M::Interact(actor,p.op==1,valid) && valid())Sample(actor,active_drive::NowMs(),p.op==1);
        return;
    }
    if(sender!=0)return;
    if(p.op==4) {
        if(g_lastPulse && !After(p.sequence,g_lastPulse))return;
        if(g_pulses.size()>=32)return;
        g_pulses.push_back({p,now+1500,0,true});g_lastPulse=p.sequence;
        return;
    }
    if(p.op)return;
    auto it=g_rows.find(key);if(it==g_rows.end() && g_rows.size()>=256)return;
    if(it!=g_rows.end() && !After(p.sequence,it->second.state.sequence))return;
    auto& row=g_rows[key];row.state=p;row.at=now;g_poll=0;
    const int rtt=g_session->rttMsForSlot(0);row.transit=rtt>=0 && rtt<=10000?float(rtt)/2000.f:0;
}
void QueueConnect(int slot){if(Current() && g_session->role()==net::Role::Host && slot>0 && slot<net::kMaxPeers){Reconcile();for(auto& [key,row]:g_rows){row.pending|=1u<<slot;Target(row.targets,slot);}g_poll=0;}}
void OnPeerLeft(int slot){if(slot>0 && slot<net::kMaxPeers){++g_peerEpoch[slot];g_people[slot]=0;g_seen[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;
    std::erase_if(g_intents,[slot](const Intent& item){return item.slot==slot;});std::erase_if(g_captures,[slot](const Capture& row){return row.slot==slot;});
    for(auto& [key,row]:g_rows)row.pending&=~(1u<<slot);for(auto& p:g_pulses)p.pending&=~(1u<<slot);}}
void OnDisconnect(){++g_epoch;g_session=nullptr;g_world=g_sequence=g_request=0;g_poll=g_now=0;g_serial=0;g_running=g_linked=false;g_rows.clear();g_intents.clear();
    g_pulses.clear();g_captures.clear();g_lastPulse=g_episode=0;g_sampleRevision.clear();
    for(int i=0;i<net::kMaxPeers;++i){g_seen[i]=0;g_rateAt[i]=0;g_rateCount[i]=0;g_people[i]=0;}}
}
