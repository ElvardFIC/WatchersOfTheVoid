#include "coop/props/watoy_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/watoy.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <unordered_map>
#include <vector>
namespace coop::watoy_sync {
namespace {
namespace W=ue_wrap::watoy;
namespace SG=ue_wrap::script_gate;
namespace R=ue_wrap::reflection;
using Packet=net::WatoyPayload;
using ue_wrap::CachedObjRef;
enum Op : uint8_t {WorldState,ActivateWorld,WorldCue,HandState,HandCue};
struct Row {Packet state{};CachedObjRef actor;uint64_t at=0;uint32_t pending=0;bool authorized=false;};
struct Outbound {Packet state{};uint64_t expires=0;uint32_t pending=0;bool play=false;uint8_t origin=0;bool authorize=false;CachedObjRef ownHand;};
struct Activation {CachedObjRef actor;uint64_t epoch;bool active=false;};
std::unordered_map<std::wstring,Row> g_worldRows;
std::unordered_map<std::wstring,W::View> g_worldViews;
std::array<Row,net::kMaxPeers> g_hands{};
std::array<W::View,net::kMaxPeers> g_handViews{};
std::deque<Outbound> g_out;
std::vector<Activation> g_activation;
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0,g_request=0,g_seen[net::kMaxPeers]{},g_lastCue=0;
uint32_t g_seenState[net::kMaxPeers]{};
uint16_t g_occupants[net::kMaxPeers]{};
uint64_t g_epoch=0,g_poll=0,g_rateAt[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};
bool g_ticking=false,g_forceHook=false,g_velocityHook=false,g_gravityHook=false;
bool g_watches=false,g_watchSaid=false;
uint64_t g_hookRetry=0,g_watchRetry=0;
constexpr int kActivate=0x57415430,kPark=0x57415431;
UE_FACT_FN(kRowForce,"watoy_sync",L"PrimitiveComponent",L"AddForce",Declared);
UE_FACT_FN(kRowVelocity,"watoy_sync",L"PrimitiveComponent",L"SetPhysicsLinearVelocity",Declared);
UE_FACT_FN(kRowGravity,"watoy_sync",L"PrimitiveComponent",L"SetEnableGravity",Declared);
uint32_t Next(uint32_t& value){if(!++value)++value;return value;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
bool Current(){return g_session && g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
bool Current(uint64_t epoch){return epoch==g_epoch && Current();}
uint8_t Local(){return players::Registry::Get().LocalPeerId();}
uint32_t Mask(int except=-1) {
    uint32_t result=0;for(int slot=1;slot<net::kMaxPeers;++slot)
        if(slot!=except && g_session->IsSlotWorldReady(slot))result|=1u<<slot;
    return result;
}
bool Eligible(void* actor) {
    return W::IsToy(actor) && !hand_item::IsHandAxisActor(actor) &&
        ue_wrap::world_identity::CurrentWorld() &&
        ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld() &&
        (g_session->role()!=net::Role::Client || net_pump::IsInAnnouncedWorld(actor));
}
bool KeyValid(const net::WireKey& key) {
    if(!key.len || key.len>sizeof(key.data))return false;
    for(unsigned i=0;i<key.len;++i)if(uint8_t(key.data[i])<33 || uint8_t(key.data[i])>126)return false;
    return net::StringFromWireKey(key)!=L"None";
}
std::wstring Key(void* actor) {
    const auto key=ue_wrap::prop::GetInteractableKeyString(actor);
    net::WireKey wire{};
    if(key.size()>sizeof(wire.data))return {};
    net::WireKeyFromString(key,wire);return KeyValid(wire)?key:std::wstring{};
}
W::State Decode(const Packet& p){return {p.active!=0,p.volume,p.pitch,p.effect!=0};}
void Encode(const W::State& s,Packet& p){p.active=uint8_t(s.active);p.effect=uint8_t(s.effect);p.volume=s.volume;p.pitch=s.pitch;}
bool Changed(const Row& row,const W::State& s) {
    return !row.state.sequence || bool(row.state.active)!=s.active || bool(row.state.effect)!=s.effect ||
        std::fabs(row.state.volume-s.volume)>=.01f || std::fabs(row.state.pitch-s.pitch)>=.02f;
}
void* HandActor(const Packet& p) {
    if(p.slot>=net::kMaxPeers || !p.playerNo || roster_ledger::Get(p.slot).playerNo!=p.playerNo)return nullptr;
    if(p.slot==Local())return hand_item::Generation(p.slot)==p.generation?hand_item::LocalHandActor():nullptr;
    return hand_item::MirrorGeneration(p.slot)==p.generation?hand_item::MirrorActorForSlot(p.slot):nullptr;
}
void ReconcileOccupants() {
    const auto epoch=g_epoch;
    for(int slot=0;slot<net::kMaxPeers;++slot) {
        const auto person=roster_ledger::Get(slot).playerNo;
        if(person==g_occupants[slot])continue;
        if(g_occupants[slot])OnPeerLeft(slot);
        if(!Current(epoch))return;
        g_occupants[slot]=person;
    }
}
void SampleWorld(void* actor,uint64_t now) {
    if(!Eligible(actor))return;
    const auto epoch=g_epoch;CachedObjRef live;live.Set(actor);
    const auto key=Key(actor);W::State value{};
    if(key.empty() || !Current(epoch) || !live.Get() || !W::Read(actor,value) || !Current(epoch) || !live.Get())return;
    if(value.active)prop_drive_host::Claim(actor,"watoy");
    if(!Current(epoch) || !live.Get())return;
    auto it=g_worldRows.find(key);if(it==g_worldRows.end() && g_worldRows.size()>=256)return;
    const auto previous=it==g_worldRows.end()?Row{}:it->second;
    if(!value.active && previous.state.active && previous.actor.Is(actor))prop_drive_host::Release(actor);
    if(!Current(epoch) || !live.Get())return;
    auto& row=g_worldRows[key];
    if(!row.actor.Is(actor) || Changed(row,value) || now-row.at>=1000) {
        Packet p{};net::WireKeyFromString(key,p.key);p.sequence=Next(g_sequence);Encode(value,p);
        row={p,live,now,Mask(),true};
    }
}
void QueueCue(void* actor) {
    if(g_out.size()>=64)return;
    Packet p{};p.sequence=Next(g_request);
    if(actor==hand_item::LocalHandActor()) {
        p.slot=Local();if(p.slot>=net::kMaxPeers)return;
        p.playerNo=roster_ledger::Get(p.slot).playerNo;p.generation=hand_item::Generation(p.slot);p.op=HandCue;
        if(!p.playerNo)return;
    } else {
        if(!Eligible(actor))return;
        const auto key=Key(actor);if(key.empty())return;
        net::WireKeyFromString(key,p.key);p.op=WorldCue;
    }
    const bool host=g_session->role()==net::Role::Host;
    if(host)p.sequence=0; // assign in delivery order, after a pending hand is announced
    g_out.push_back({p,active_drive::NowMs()+1500,host?Mask(p.op==HandCue?p.slot:-1):1u,false});
    if(p.op==HandCue)g_out.back().ownHand.Set(actor);
}
SG::Verdict BeforeActivate(const SG::Call& c) {
    if(!Current() || c.fromOurCode || !W::IsToy(c.object))return SG::Verdict::Run;
    if(hand_item::IsMirrorActor(c.object))return SG::Verdict::Cancel;
    const auto epoch=g_epoch;CachedObjRef live;live.Set(c.object);
    if(c.object!=hand_item::LocalHandActor() && g_session->role()==net::Role::Client) {
        if(Eligible(c.object) && g_out.size()<64) {
            const auto key=Key(c.object);
            if(!key.empty() && Current(epoch) && live.Get()) {
                Packet p{};net::WireKeyFromString(key,p.key);p.op=ActivateWorld;p.sequence=Next(g_request);
                g_out.push_back({p,active_drive::NowMs()+3000,1u,false});
            }
        }
        return SG::Verdict::Cancel;
    }
    W::State before{};
    if(W::Read(c.object,before) && Current(epoch) && live.Get())g_activation.push_back({live,epoch,before.active});
    return SG::Verdict::Run;
}
void AfterActivate(const SG::Call& c) {
    if(c.fromOurCode || g_activation.empty() || !g_activation.back().actor.Is(c.object))return;
    const auto before=g_activation.back();g_activation.pop_back();
    if(!Current(before.epoch) || before.active)return;
    W::State state{};
    if(!W::Read(c.object,state) || !Current(before.epoch) || !before.actor.Get() || !state.active)return;
    QueueCue(c.object);g_poll=0;
}
bool Park(void* actor) {
    return Current() && W::IsToy(actor) && ue_wrap::world_identity::CurrentWorld() &&
        ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld() && (hand_item::IsMirrorActor(actor) ||
        (g_session->role()==net::Role::Client && actor!=hand_item::LocalHandActor()));
}
SG::Verdict BeforeTick(const SG::Call& c) {
    return !c.fromOurCode && Park(c.object)?SG::Verdict::Cancel:SG::Verdict::Run;
}
SG::Verdict BeforePhysics(void* component,void* source,const uint8_t*) {
    if(!Current() || !source)return SG::Verdict::Run;
    void* owner=W::ZeroGravityOwner(source);
    return owner && Park(owner) && ue_wrap::prop::GetStaticMesh(owner)==component?SG::Verdict::Cancel:SG::Verdict::Run;
}
void EnsurePhysicsHooks() {
    if((g_forceHook && g_velocityHook && g_gravityHook) || active_drive::NowMs()<g_hookRetry)return;
    g_hookRetry=active_drive::NowMs()+1000;
    void* cls=ue_wrap::object_index::ClassByName(L"PrimitiveComponent");if(!cls)return;
    namespace H=ue_wrap::ufunction_hook;namespace F=ue_wrap::facts;
    if(!g_forceHook)g_forceHook=H::InstallPreHook(F::ResolveOn(kRowForce,cls),&BeforePhysics,true);
    if(!g_velocityHook)g_velocityHook=H::InstallPreHook(F::ResolveOn(kRowVelocity,cls),&BeforePhysics,true);
    if(!g_gravityHook)g_gravityHook=H::InstallPreHook(F::ResolveOn(kRowGravity,cls),&BeforePhysics,true);
}
void EnsureWatches() {
    if(g_watches || active_drive::NowMs()<g_watchRetry)return;
    g_watchRetry=active_drive::NowMs()+1000;
    const bool activate=SG::WatchClassName(L"prop_watoy_C",L"activate",kActivate,&BeforeActivate,&AfterActivate);
    const bool tick=SG::WatchClassName(L"prop_watoy_C",L"ReceiveTick",kPark,&BeforeTick,nullptr);
    const bool timeline=SG::WatchClassName(L"prop_watoy_C",L"Timeline_0__UpdateFunc",kPark,&BeforeTick,nullptr);
    g_watches=activate && tick && timeline;
    if(!g_watches && !g_watchSaid){g_watchSaid=true;UE_LOGW("watoy: producer watch registration incomplete; retrying");}
    SG::ResolvePendingNames();
}
void SampleLocal(uint64_t now) {
    const auto slot=Local();if(slot>=net::kMaxPeers)return;
    void* actor=hand_item::LocalHandActor();const auto generation=hand_item::Generation(slot);
    const auto person=roster_ledger::Get(slot).playerNo;
    if(!W::IsToy(actor) || !generation || !person){g_hands[slot]={};return;}
    const auto epoch=g_epoch;CachedObjRef live;live.Set(actor);W::State value{};
    if(!W::Read(actor,value) || !Current(epoch) || !live.Get() || actor!=hand_item::LocalHandActor() ||
       generation!=hand_item::Generation(slot) || person!=roster_ledger::Get(slot).playerNo)return;
    auto& row=g_hands[slot];
    if(row.state.generation==generation && !Changed(row,value) && now-row.at<1000)return;
    Packet p{};p.sequence=Next(g_session->role()==net::Role::Host?g_sequence:g_request);
    p.op=HandState;p.slot=slot;p.generation=generation;p.playerNo=person;Encode(value,p);
    row={p,live,now,g_session->role()==net::Role::Host?Mask(slot):1u,true};
}
void ApplyHands() {
    const auto epoch=g_epoch;
    for(uint8_t slot=0;slot<net::kMaxPeers;++slot) {
        if(slot==Local())continue;
        const auto row=g_hands[slot];
        void* actor=row.state.sequence?HandActor(row.state):nullptr;
        if(!W::IsToy(actor)) {
            auto previous=g_handViews[slot];g_handViews[slot]={};W::Clear(previous);
            if(!Current(epoch))return;
            continue;
        }
        CachedObjRef live;live.Set(actor);
        const bool host=g_session->role()==net::Role::Host;
        if(host && !row.authorized) {
            const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,slot,600.f).Authorize(actor));
            if(!Current(epoch))return;
            if(!allowed || !live.Get() || HandActor(row.state)!=actor)continue;
        }
        auto view=g_handViews[slot];
        const auto valid=[&]{return Current(epoch) && HandActor(row.state)==actor &&
            g_hands[slot].state.sequence==row.state.sequence;};
        const bool ok=W::Apply(view,actor,Decode(row.state),valid);
        if(!valid()){W::Clear(view);if(!Current(epoch))return;continue;}
        g_handViews[slot]=view;
        if(!ok || !live.Get() || HandActor(row.state)!=actor || g_hands[slot].state.sequence!=row.state.sequence)continue;
        g_hands[slot].actor=live;
        if(host && !row.authorized){g_hands[slot].pending=Mask(slot);g_hands[slot].state.sequence=Next(g_sequence);}
        g_hands[slot].authorized=true;
    }
}
bool ActivateRequest(const Packet& p,uint8_t sender,uint64_t now) {
    const auto epoch=g_epoch;
    const auto person=roster_ledger::Get(sender).playerNo;
    if(!person || !g_session->IsSlotWorldReady(sender))return true;
    const auto current=[&]{return Current(epoch) && roster_ledger::Get(sender).playerNo==person;};
    void* actor=prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key));
    if(!actor)return false;
    if(!Eligible(actor))return true;
    CachedObjRef live;live.Set(actor);
    const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,sender,600.f).Authorize(actor));
    if(!current() || !allowed || !live.Get())return true;
    W::State before{},after{};
    const bool readable=W::Read(actor,before);
    if(!current() || !live.Get())return true;
    if(!readable)return false;
    if(!before.active) {
        const bool activated=W::Activate(actor);
        if(!current() || !live.Get())return true;
        if(!activated)return false;
        if(W::Read(actor,after) && current() && live.Get() && after.active)QueueCue(actor);
    }
    if(current() && live.Get())SampleWorld(actor,now);
    return true;
}
void FlushRows() {
    for(auto& [key,row]:g_worldRows)for(int slot=1;slot<net::kMaxPeers;++slot)if(row.pending&(1u<<slot)) {
        if(!g_session->IsSlotWorldReady(slot) || g_session->SendReliableToSlot(slot,net::ReliableKind::Watoy,&row.state,sizeof(row.state)))
            row.pending&=~(1u<<slot);
    }
    for(auto& row:g_hands)if(row.authorized)for(int slot=0;slot<net::kMaxPeers;++slot)if(row.pending&(1u<<slot)) {
        if(!g_session->IsSlotWorldReady(slot) || g_session->SendReliableToSlot(slot,net::ReliableKind::Watoy,&row.state,sizeof(row.state)))
            row.pending&=~(1u<<slot);
    }
}
void Drain(uint64_t now) {
    const auto epoch=g_epoch;uint32_t blocked=0;
    if(g_session->role()==net::Role::Host)std::stable_partition(g_out.begin(),g_out.end(),[](const Outbound& item){
        return !item.authorize && item.state.sequence && (item.state.op==WorldCue || item.state.op==HandCue);});
    std::deque<Outbound> retained;
    const size_t count=g_out.size();
    for(size_t i=0;i<count;++i) {
        auto item=g_out.front();g_out.pop_front();auto& p=item.state;
        if(now>item.expires)continue;
        if(p.op==HandCue && (!p.playerNo || roster_ledger::Get(p.slot).playerNo!=p.playerNo))continue;
        if(item.ownHand.Idx()>=0) {
            if(!item.ownHand.Get() || item.ownHand.Get()!=hand_item::LocalHandActor() || p.slot!=Local())continue;
            const auto generation=hand_item::Generation(p.slot);
            if(p.generation && generation && p.generation!=generation)continue;
            if(!p.generation)p.generation=generation;
            if(!p.generation){retained.push_back(item);g_out.insert(g_out.begin(),retained.begin(),retained.end());return;}
        }
        if(item.authorize) {
            if(p.op==ActivateWorld) {
                if(now<g_poll || !ActivateRequest(p,item.origin,now))retained.push_back(item);
                if(!Current(epoch))return;
                continue;
            }
            void* actor=HandActor(p);
            if(!W::IsToy(actor)){retained.push_back(item);continue;}
            CachedObjRef live;live.Set(actor);
            const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,item.origin,600.f).Authorize(actor));
            if(!Current(epoch))return;
            if(!allowed || !live.Get() || HandActor(p)!=actor)continue;
            item.authorize=false;item.pending=Mask(item.origin);p.sequence=Next(g_sequence);
        }
        if(g_session->role()==net::Role::Host && !p.sequence)p.sequence=Next(g_sequence);
        if(item.play) {
            void* actor=p.op==HandCue?HandActor(p):prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key));
            if((p.op==HandCue?W::IsToy(actor):Eligible(actor)) && W::PlayStartCue())item.play=false;
            if(!Current(epoch))return;
        }
        for(int slot=0;slot<net::kMaxPeers;++slot)if((item.pending&~blocked)&(1u<<slot)) {
            if(!g_session->IsSlotWorldReady(slot) || g_session->SendReliableToSlot(slot,net::ReliableKind::Watoy,&p,sizeof(p)))
                item.pending&=~(1u<<slot);
        }
        blocked|=item.pending;
        if(item.pending || item.play)retained.push_back(item);
    }
    g_out.insert(g_out.begin(),retained.begin(),retained.end());
}
void PruneRows(uint64_t now) {
    const auto epoch=g_epoch;std::vector<std::wstring> remove;
    const bool host=g_session->role()==net::Role::Host;
    for(const auto& [key,row]:g_worldRows)
        if(now-row.at>10000 || (host && !Eligible(row.actor.Get())))remove.push_back(key);
    for(const auto& key:remove) {
        g_worldRows.erase(key);const auto it=g_worldViews.find(key);
        if(it==g_worldViews.end())continue;
        auto view=it->second;g_worldViews.erase(it);W::Clear(view);
        if(!Current(epoch))return;
    }
}
bool Rate(uint8_t slot,uint64_t now) {
    if(now-g_rateAt[slot]>=1000){g_rateAt[slot]=now;g_rateCount[slot]=0;}
    if(g_rateCount[slot]>=32)return false;
    ++g_rateCount[slot];return true;
}
}
void Install(net::Session* session) {
    const auto world=ue_wrap::world_identity::Generation();
    if(g_session!=session || g_world!=world){OnDisconnect();g_session=session;g_world=world;}
    EnsureWatches();
    // Keep component initialization, timers and retirement native. Refuse only
    // its physics writes on presentation copies, including a pre-session component.
    EnsurePhysicsHooks();
}
void Tick() {
    if(!Current() || g_ticking)return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    const auto epoch=g_epoch,now=active_drive::NowMs();EnsurePhysicsHooks();EnsureWatches();ReconcileOccupants();if(!Current(epoch))return;
    Drain(now);if(!Current(epoch))return;
    if(now>=g_poll) {
        g_poll=now+100;SampleLocal(now);if(!Current(epoch))return;
        ApplyHands();if(!Current(epoch))return;
        if(g_session->role()==net::Role::Host) {
            std::array<CachedObjRef,256> actors{};const auto count=W::Collect(actors.data(),actors.size());
            for(size_t i=0;i<count;++i){SampleWorld(actors[i].Get(),now);if(!Current(epoch))return;}
        } else {
            const auto rows=g_worldRows;
            for(const auto& [key,row]:rows) {
                void* actor=prop_element_tracker::FindLiveActorByKey(key);if(!Eligible(actor))continue;
                CachedObjRef live;live.Set(actor);auto view=g_worldViews[key];
                const auto valid=[&]{const auto it=g_worldRows.find(key);return Current(epoch) && live.Get() &&
                    it!=g_worldRows.end() && it->second.state.sequence==row.state.sequence && Eligible(actor);};
                const bool ok=W::Apply(view,actor,Decode(row.state),valid);
                if(!valid()){W::Clear(view);if(!Current(epoch))return;continue;}
                g_worldViews[key]=view;
                const auto it=g_worldRows.find(key);
                if(ok && live.Get() && it!=g_worldRows.end() && it->second.state.sequence==row.state.sequence)
                    it->second.actor=live;
            }
        }
    }
    PruneRows(now);if(!Current(epoch))return;
    FlushRows();
}
void Receive(const Packet& p,uint8_t sender) {
    if(!Current() || !p.sequence || p.op>HandCue || p.effect>1 || p.active>1 || p.effect>p.active ||
       !std::isfinite(p.volume) || p.volume<0 || p.volume>1 || !std::isfinite(p.pitch) || p.pitch<0 || p.pitch>100)return;
    const bool hand=p.op==HandState || p.op==HandCue,cue=p.op==WorldCue || p.op==HandCue;
    if(hand) {
        if(p.key.len || p.slot>=net::kMaxPeers || !p.generation || !p.playerNo ||
           roster_ledger::Get(p.slot).playerNo!=p.playerNo)return;
    } else if(!KeyValid(p.key) || p.slot || p.generation || p.playerNo)return;
    if((cue || p.op==ActivateWorld) && (p.active || p.effect || p.volume || p.pitch))return;
    if(!cue && p.op!=ActivateWorld && p.pitch<=0)return;
    const auto epoch=g_epoch,now=active_drive::NowMs();ReconcileOccupants();if(!Current(epoch))return;
    if(g_session->role()==net::Role::Host) {
        if(sender>=net::kMaxPeers)return;
        auto& seen=p.op==HandState?g_seenState[sender]:g_seen[sender];
        if(sender==0 || sender>=net::kMaxPeers || !g_session->IsSlotWorldReady(sender) ||
           (p.op!=ActivateWorld && p.op!=HandState && p.op!=HandCue) || (hand && p.slot!=sender) ||
           (seen && !After(p.sequence,seen)) || !Rate(sender,now))return;
        seen=p.sequence;
        if(p.op==HandState) {
            const auto generation=hand_item::Generation(sender);
            if(generation && generation!=p.generation && !After(p.generation,generation))return;
            g_hands[sender]={p,{},now,0,false};g_poll=0;return;
        }
        if(p.op==HandCue) {
            const auto generation=hand_item::Generation(sender);
            if(generation && generation!=p.generation && !After(p.generation,generation))return;
            if(g_out.size()<64)g_out.push_back({p,now+1500,0,true,sender,true});
            return;
        }
        if(!ActivateRequest(p,sender,now) && Current(epoch) && g_out.size()<64)
            g_out.push_back({p,now+3000,0,false,sender,true});
        return;
    }
    if(sender!=0 || p.op==ActivateWorld)return;
    if(cue) {
        if(g_lastCue && !After(p.sequence,g_lastCue))return;
        g_lastCue=p.sequence;
        if(g_out.size()<64)g_out.push_back({p,now+1500,0,true});return;
    }
    if(hand) {
        if(p.slot==Local())return;
        const auto& row=g_hands[p.slot];
        if(row.state.sequence && !After(p.sequence,row.state.sequence))return;
        const auto generation=hand_item::Generation(p.slot);
        if(generation && generation!=p.generation && !After(p.generation,generation))return;
        g_hands[p.slot]={p,{},now,0,true};g_poll=0;return;
    }
    const auto key=net::StringFromWireKey(p.key);auto found=g_worldRows.find(key);
    if(found==g_worldRows.end() && g_worldRows.size()>=256)return;
    if(found!=g_worldRows.end() && !After(p.sequence,found->second.state.sequence))return;
    g_worldRows[key]={p,{},now,0,true};g_poll=0;
}
void QueueConnect(int slot) {
    if(!Current() || g_session->role()!=net::Role::Host || slot<1 || slot>=net::kMaxPeers)return;
    for(auto& [key,row]:g_worldRows)row.pending|=1u<<slot;
    for(auto& row:g_hands)if(row.authorized && row.state.sequence && row.state.slot!=slot)row.pending|=1u<<slot;
    g_poll=0;
}
void OnPeerLeft(int slot) {
    if(slot<0 || slot>=net::kMaxPeers)return;
    auto previous=g_handViews[slot];g_handViews[slot]={};
    g_hands[slot]={};g_seen[slot]=g_seenState[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;g_occupants[slot]=0;
    for(auto& [key,row]:g_worldRows)row.pending&=~(1u<<slot);
    for(auto& row:g_hands)row.pending&=~(1u<<slot);
    std::erase_if(g_out,[slot](Outbound& item){item.pending&=~(1u<<slot);return
        (item.authorize && item.origin==slot) || (item.state.op==HandCue && item.state.slot==slot);});
    W::Clear(previous);
}
void OnDisconnect() {
    std::vector<W::View> stop;
    for(const auto& [key,view]:g_worldViews)stop.push_back(view);
    for(const auto& view:g_handViews)stop.push_back(view);
    ++g_epoch;g_session=nullptr;g_worldRows.clear();g_hands={};g_out.clear();g_activation.clear();
    g_worldViews.clear();g_handViews={};
    g_world=g_sequence=g_request=g_lastCue=0;g_poll=0;
    for(int slot=0;slot<net::kMaxPeers;++slot){g_seen[slot]=g_seenState[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;g_occupants[slot]=0;}
    for(auto& view:stop)W::Clear(view);
}
}
