#include "coop/props/dingus_music_sync.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/element/intent_authority.h"
#include "coop/player/hand_item.h"
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
namespace coop::dingus_music_sync {
namespace {
namespace M=ue_wrap::dingus_music;
namespace SG=ue_wrap::script_gate;
namespace R=ue_wrap::reflection;
using Packet=net::DingusMusicPayload;
struct Row {Packet state{};ue_wrap::CachedObjRef actor;uint32_t pending=0;uint64_t at=0;};
struct Intent {Packet state{};uint64_t expires=0;};
struct Pulse {Packet state{};uint64_t expires=0;uint32_t pending=0;bool play=false;};
std::unordered_map<std::wstring,Row> g_rows;
std::deque<Intent> g_intents;
std::deque<Pulse> g_pulses;
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0,g_request=0,g_seen[net::kMaxPeers]{};
uint32_t g_lastPulse=0;
uint64_t g_epoch=0,g_poll=0,g_rateAt[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};
bool g_ticking=false;
uint32_t Next(uint32_t& n){if(!++n)++n;return n;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
bool Current(){return g_session && g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
bool Eligible(void* a){return M::IsDingus(a) && !hand_item::IsHandAxisActor(a) &&
    ue_wrap::world_identity::CurrentWorld() && ue_wrap::world_identity::WorldOf(a)==ue_wrap::world_identity::CurrentWorld();}
bool SoundEligible(void* a){return M::IsSqueakyToy(a) && !hand_item::IsHandAxisActor(a) &&
    ue_wrap::world_identity::CurrentWorld() && ue_wrap::world_identity::WorldOf(a)==ue_wrap::world_identity::CurrentWorld();}
constexpr int kPlushUse=0x44494E48;
uint32_t Mask(){uint32_t m=0;for(int s=1;s<net::kMaxPeers;++s)if(g_session->IsSlotWorldReady(s))m|=1u<<s;return m;}
std::wstring Key(void* actor){auto key=ue_wrap::prop::GetInteractableKeyString(actor);
    return key.empty() || key==L"None" || key.size()>sizeof(Packet{}.key.data)?std::wstring{}:key;}
void Sample(void* actor,uint64_t now) {
    if(!Eligible(actor))return;
    const auto epoch=g_epoch;ue_wrap::CachedObjRef live;live.Set(actor);
    const auto key=Key(actor);M::State state{};
    if(key.empty() || !live.Get() || !Current() || epoch!=g_epoch || !M::Read(actor,state) || !Current() || epoch!=g_epoch)return;
    auto found=g_rows.find(key);if(found==g_rows.end() && g_rows.size()>=256)return;
    auto& row=g_rows[key];const bool changed=!row.actor.Is(actor) || bool(row.state.playing)!=state.playing ||
        bool(row.state.used)!=state.used || row.state.volume!=state.volume;
    row.actor=live;
    if(changed || !row.state.sequence || now-row.at>=1000) {
        row.state={};net::WireKeyFromString(key,row.state.key);
        row.state.sequence=Next(g_sequence);row.state.playing=uint8_t(state.playing);
        row.state.used=uint8_t(state.used);row.state.volume=state.volume;
        row.pending=Mask();row.at=now;
    }
}
void Request(void* actor,uint8_t op) {
    if(!Eligible(actor) || g_intents.size()>=32)return;
    const auto epoch=g_epoch;ue_wrap::CachedObjRef live;live.Set(actor);
    const auto key=Key(actor);if(key.empty() || epoch!=g_epoch || !Current() || !live.Get())return;
    Packet p{};net::WireKeyFromString(key,p.key);p.sequence=Next(g_request);p.op=op;
    g_intents.push_back({p,active_drive::NowMs()+3000});
}
SG::Verdict BeforeAction(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !Eligible(c.object) || g_session->role()!=net::Role::Client)return SG::Verdict::Run;
    if(!c.function || !c.locals)return SG::Verdict::Run;
    const int off=R::FindParamOffset(c.function,L"action"),size=R::FunctionFrameSize(c.function);
    if(off<0 || off>=size || c.locals[off]!=5)return SG::Verdict::Run;
    Request(c.object,1);return SG::Verdict::Cancel;
}
SG::Verdict BeforeTouch(const SG::Call& c) {
    if(Current() && !c.fromOurCode && g_session->role()==net::Role::Client && Eligible(c.object)) {
        M::State state{};const auto epoch=g_epoch;
        if(M::Read(c.object,state) && !state.used && Current() && epoch==g_epoch)Request(c.object,2);
    }
    return SG::Verdict::Run;
}
void QueuePulse(Packet p,int origin) {
    if(g_pulses.size()>=32)return;
    p.op=4;p.sequence=Next(g_sequence);
    g_pulses.push_back({p,active_drive::NowMs()+1500,Mask()&~(1u<<origin),origin!=0});
}
void AfterAction(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !SoundEligible(c.object) || !c.function || !c.locals)return;
    if(c.tag==kPlushUse) {if(!M::IsAltUse(c.function,c.locals))return;}
    else {
        const int off=R::FindParamOffset(c.function,L"action"),size=R::FunctionFrameSize(c.function);
        if(off<0 || off>=size || c.locals[off]!=6)return;
    }
    const auto epoch=g_epoch;ue_wrap::CachedObjRef live;live.Set(c.object);
    const auto key=Key(c.object);float pitch=0;
    if(key.empty() || epoch!=g_epoch || !Current() || !live.Get() || !M::ReadSqueakPitch(c.object,pitch))return;
    Packet p{};net::WireKeyFromString(key,p.key);p.volume=pitch;
    if(g_session->role()==net::Role::Host)QueuePulse(p,0);
    else if(g_intents.size()<32) {
        p.op=3;p.sequence=Next(g_request);
        g_intents.push_back({p,active_drive::NowMs()+1500});
    }
}
void DrainPulses(uint64_t now) {
    const auto epoch=g_epoch;const size_t budget=(std::min)(size_t(4),g_pulses.size());
    uint32_t blocked=0;std::deque<Pulse> retained;
    for(size_t i=0;i<budget;++i) {
        auto pulse=g_pulses.front();g_pulses.pop_front();
        if(now>pulse.expires)continue;
        if(pulse.play) {
            void* actor=prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(pulse.state.key));
            if(SoundEligible(actor) && M::Squeak(actor,pulse.state.volume))pulse.play=false;
            if(epoch!=g_epoch || !Current())return;
        }
        for(int slot=1;slot<net::kMaxPeers;++slot)if((pulse.pending&~blocked)&(1u<<slot))
            if(!g_session->IsSlotWorldReady(slot) || g_session->SendReliableToSlot(slot,net::ReliableKind::DingusMusic,&pulse.state,sizeof(pulse.state)))pulse.pending&=~(1u<<slot);
        blocked|=pulse.pending;
        if(pulse.play || pulse.pending)retained.push_back(pulse);
    }
    // Keep retries before the untouched tail; rotating the first four to the
    // back lets a later sequence overtake them and the receiver rejects them.
    g_pulses.insert(g_pulses.begin(),retained.begin(),retained.end());
}
}
void Install(net::Session* s) {
    const auto world=ue_wrap::world_identity::Generation();
    if(g_session!=s || world!=g_world){OnDisconnect();g_session=s;g_world=world;}
    SG::WatchClassName(L"prop_dingus_C",L"actionOptionIndex",0x44494E47,&BeforeAction,&AfterAction);
    SG::WatchClassName(L"prop_dingus_C",L"touch",0x44494E47,&BeforeTouch,nullptr);
    SG::WatchClassName(L"prop_abplush_C",L"player_use",kPlushUse,nullptr,&AfterAction);
    SG::ResolvePendingNames();
}
void Tick() {
    if(!Current() || g_ticking)return;
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    const auto epoch=g_epoch,now=active_drive::NowMs();
    if(g_session->role()==net::Role::Host && !g_session->AnyWorldReadyPeer())return;
    DrainPulses(now);if(epoch!=g_epoch || !Current())return;
    if(g_session->role()==net::Role::Client) {
        while(!g_intents.empty()) {
            const auto item=g_intents.front();
            if(now>item.expires){g_intents.pop_front();continue;}
            if(!g_session->SendReliableToSlot(0,net::ReliableKind::DingusMusic,&item.state,sizeof(item.state)))break;
            g_intents.pop_front();
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
                void* actor=row.actor.Get();if(!actor)actor=prop_element_tracker::FindLiveActorByKey(key);
                if(!Eligible(actor))continue;
                ue_wrap::CachedObjRef live;live.Set(actor);
                M::Apply(actor,{row.state.playing!=0,row.state.used!=0,row.state.volume});
                if(epoch!=g_epoch || !Current())return;
                const auto it=g_rows.find(key);
                if(it!=g_rows.end() && it->second.state.sequence==row.state.sequence)it->second.actor=live;
            }
        }
    }
    for(auto it=g_rows.begin();it!=g_rows.end();) {
        auto& row=it->second;
        if((g_session->role()==net::Role::Host && !Eligible(row.actor.Get())) || now-row.at>10000){it=g_rows.erase(it);continue;}
        if(g_session->role()==net::Role::Host)for(int slot=1;slot<net::kMaxPeers;++slot)if(row.pending&(1u<<slot))
            if(!g_session->IsSlotWorldReady(slot) || g_session->SendReliableToSlot(slot,net::ReliableKind::DingusMusic,&row.state,sizeof(row.state)))row.pending&=~(1u<<slot);
        ++it;
    }
}
void Receive(const Packet& p,uint8_t sender) {
    if(!Current() || !p.sequence || !p.key.len || p.key.len>sizeof(p.key.data) || p.op>4 ||
       p.playing>1 || p.used>1 || p.reserved || !std::isfinite(p.volume) || p.volume<0 || p.volume>10)return;
    if(p.op>=3 && (p.volume<=0 || p.playing || p.used))return;
    const auto key=net::StringFromWireKey(p.key);if(key.empty() || key==L"None" || key.find(L'\0')!=std::wstring::npos)return;
    const auto now=active_drive::NowMs(),epoch=g_epoch;
    if(g_session->role()==net::Role::Host) {
        if(sender==0 || sender>=net::kMaxPeers || !g_session->IsSlotWorldReady(sender) || !p.op || p.op>3 ||
           p.playing || p.used || (p.op<3 && p.volume) || (g_seen[sender] && !After(p.sequence,g_seen[sender])))return;
        if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rateCount[sender]=0;}
        if(g_rateCount[sender]>=8)return;
        ++g_rateCount[sender];g_seen[sender]=p.sequence;
        void* actor=prop_element_tracker::FindLiveActorByKey(key);if(!(p.op==3?SoundEligible(actor):Eligible(actor)))return;
        ue_wrap::CachedObjRef live;live.Set(actor);
        if(!element::IntentTarget::ForClientIntent(*g_session,sender,400.f).Authorize(actor) ||
           !Current() || epoch!=g_epoch || !live.Get())return;
        if(p.op==3){QueuePulse(p,sender);return;}
        if(M::Interact(actor,p.op==1) && Current() && epoch==g_epoch && live.Get())Sample(actor,now);
        return;
    }
    if(sender!=0)return;
    if(p.op==4) {
        if(g_lastPulse && !After(p.sequence,g_lastPulse))return;
        g_lastPulse=p.sequence;
        if(g_pulses.size()<32)g_pulses.push_back({p,now+1500,0,true});
        return;
    }
    if(p.op)return;
    auto it=g_rows.find(key);if(it==g_rows.end() && g_rows.size()>=256)return;
    if(it!=g_rows.end() && !After(p.sequence,it->second.state.sequence))return;
    auto& row=g_rows[key];row.state=p;row.at=now;g_poll=0;
}
void QueueConnect(int slot){if(Current() && g_session->role()==net::Role::Host && slot>0 && slot<net::kMaxPeers){for(auto& [key,row]:g_rows)row.pending|=1u<<slot;g_poll=0;}}
void OnPeerLeft(int slot){if(slot>0 && slot<net::kMaxPeers){g_seen[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;for(auto& [key,row]:g_rows)row.pending&=~(1u<<slot);for(auto& p:g_pulses)p.pending&=~(1u<<slot);}}
void OnDisconnect(){++g_epoch;g_session=nullptr;g_world=g_sequence=g_request=0;g_poll=0;g_rows.clear();g_intents.clear();
    g_pulses.clear();g_lastPulse=0;
    for(int i=0;i<net::kMaxPeers;++i){g_seen[i]=0;g_rateAt[i]=0;g_rateCount[i]=0;}}
}
