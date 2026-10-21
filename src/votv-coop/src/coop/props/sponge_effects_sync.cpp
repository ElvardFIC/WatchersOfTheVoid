#include "coop/props/sponge_effects_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/sponge_effects.h"
#include "ue_wrap/core/script_gate.h"
#include <algorithm>
#include <array>
#include <deque>
#include <vector>
namespace coop::sponge_effects_sync {
namespace {
namespace FX=ue_wrap::sponge_effects;
namespace SG=ue_wrap::script_gate;
using Packet=net::SpongeEffectPayload;
using ue_wrap::CachedObjRef;
struct Frame {
    void* object=nullptr;CachedObjRef actor;FX::Use use;uint64_t epoch;void* stack;int depth;bool hand;
    uint8_t slot=0;uint16_t person=0;uint32_t generation=0;
};
struct Event {
    Packet packet{};CachedObjRef source;
    uint64_t expires=0;uint32_t pending=0;
    std::array<uint16_t,net::kMaxPeers> recipients{};
    bool own=false,hand=false,authorized=false,play=false,surface=false,burst=false;
};
net::Session* g_session=nullptr;
uint32_t g_world=0,g_request=0,g_sequence=0,g_lastHost=0,g_seen[net::kMaxPeers]{};
uint64_t g_epoch=0,g_rateAt[net::kMaxPeers]{};
uint16_t g_people[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};
std::vector<Frame> g_frames;
std::deque<Event> g_events;
bool g_ticking=false,g_watch=false;
uint64_t g_watchRetry=0;
constexpr int kTag=0x53504F4E;
bool Current(){return g_session && g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
bool Current(uint64_t epoch){return Current() && g_epoch==epoch;}
uint32_t Next(uint32_t& value){if(!++value)++value;return value;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
uint8_t Local(){return players::Registry::Get().LocalPeerId();}
uint32_t Mask(int except) {
    uint32_t mask=0;for(int slot=1;slot<net::kMaxPeers;++slot)
        if(slot!=except && g_session->IsSlotWorldReady(slot))mask|=1u<<slot;
    return mask;
}
void SetPending(Event& event,uint32_t mask) {
    event.pending=mask;
    for(int slot=0;slot<net::kMaxPeers;++slot)event.recipients[slot]=roster_ledger::Get(slot).playerNo;
}
bool SourceCurrent(const Event& event) {
    const auto& p=event.packet;
    if(roster_ledger::Get(p.slot).playerNo!=p.playerNo)return false;
    if(event.hand) {
        const auto generation=hand_item::Generation(p.slot);
        if(generation && generation!=p.generation && !After(p.generation,generation))return false;
    }
    return true;
}
bool KeyValid(const net::WireKey& key) {
    if(!key.len || key.len>sizeof(key.data))return false;
    for(unsigned i=0;i<key.len;++i)if(uint8_t(key.data[i])<33 || uint8_t(key.data[i])>126)return false;
    return net::StringFromWireKey(key)!=L"None";
}
bool WorldSource(void* actor) {
    return FX::IsSponge(actor) && !hand_item::IsHandAxisActor(actor) && ue_wrap::world_identity::CurrentWorld() &&
        ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld() &&
        (g_session->role()!=net::Role::Client || net_pump::IsInAnnouncedWorld(actor));
}
void Reconcile() {
    for(int slot=0;slot<net::kMaxPeers;++slot) {
        const auto person=roster_ledger::Get(slot).playerNo;
        if(person==g_people[slot])continue;
        if(g_people[slot])OnPeerLeft(slot);
        g_people[slot]=person;
    }
}
SG::Verdict Before(const SG::Call& call) {
    if(!Current() || call.fromOurCode || !FX::IsSponge(call.object))return SG::Verdict::Run;
    if(hand_item::IsMirrorActor(call.object))return SG::Verdict::Cancel;
    if(g_frames.size()>=16 || g_events.size()>=128)return SG::Verdict::Run;
    Frame frame{};frame.object=call.object;frame.actor.Set(call.object);frame.epoch=g_epoch;frame.stack=call.stack;frame.depth=call.depth;
    frame.hand=call.object==hand_item::LocalHandActor();
    frame.slot=Local();if(frame.slot>=net::kMaxPeers)return SG::Verdict::Run;
    frame.person=roster_ledger::Get(frame.slot).playerNo;
    if(frame.hand)frame.generation=hand_item::Generation(frame.slot);
    if(!frame.hand && !WorldSource(call.object))return SG::Verdict::Run;
    if(FX::ReadUse(call.object,players::Registry::Get().Local(),frame.use) && Current(frame.epoch) && frame.actor.Get() &&
       Local()==frame.slot && roster_ledger::Get(frame.slot).playerNo==frame.person &&
       (!frame.hand || (hand_item::LocalHandActor()==call.object && hand_item::Generation(frame.slot)==frame.generation)))
        g_frames.push_back(frame);
    return SG::Verdict::Run;
}
void AfterUse(const SG::Call& call) {
    if(call.fromOurCode)return;
    auto it=std::find_if(g_frames.rbegin(),g_frames.rend(),[&](const Frame& frame){
        return frame.stack==call.stack && frame.depth==call.depth && frame.object==call.object;});
    if(it==g_frames.rend())return;
    const auto frame=*it;g_frames.erase(std::next(it).base());
    const auto valid=[&]{return Current(frame.epoch) && frame.actor.Get() && Local()==frame.slot &&
        roster_ledger::Get(frame.slot).playerNo==frame.person && (!frame.hand ||
        (hand_item::LocalHandActor()==frame.object && hand_item::Generation(frame.slot)==frame.generation));};
    if(!valid() || g_events.size()>=128)return;
    float soap=0;
    if(!FX::ReadSoap(call.object,soap) || soap>frame.use.soap-4.99f || !valid())return;
    const auto owner=frame.slot;
    Event event{};auto& p=event.packet;
    p.slot=owner;p.playerNo=frame.person;
    p.x=frame.use.location.X;p.y=frame.use.location.Y;p.z=frame.use.location.Z;
    p.nx=frame.use.normal.X;p.ny=frame.use.normal.Y;p.nz=frame.use.normal.Z;
    if(frame.hand)p.generation=frame.generation;
    else {
        const auto key=ue_wrap::prop::GetInteractableKeyString(call.object);
        if(!valid())return;
        if(key.size()>sizeof(p.key.data))return;
        net::WireKeyFromString(key,p.key);if(!KeyValid(p.key))return;
    }
    event.own=true;event.hand=frame.hand;event.source=frame.actor;event.authorized=true;
    event.expires=active_drive::NowMs()+1500;
    if(g_session->role()==net::Role::Host){p.op=1;SetPending(event,Mask(owner));}
    else {p.sequence=Next(g_request);SetPending(event,1u);}
    if(event.pending)g_events.push_back(event);
}
void Watch() {
    if(g_watch || active_drive::NowMs()<g_watchRetry)return;
    g_watchRetry=active_drive::NowMs()+1000;
    g_watch=SG::WatchClassName(L"prop_sponge_C",L"cleanBubbles",kTag,&Before,&AfterUse);
    SG::ResolvePendingNames();
}
void* Source(const Event& event) {
    const auto& p=event.packet;
    if(!event.hand)return prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key));
    if(p.slot==Local())return hand_item::Generation(p.slot)==p.generation?hand_item::LocalHandActor():nullptr;
    return hand_item::MirrorGeneration(p.slot)==p.generation?hand_item::MirrorActorForSlot(p.slot):nullptr;
}
bool Rate(uint8_t slot,uint64_t now) {
    if(now-g_rateAt[slot]>=1000){g_rateAt[slot]=now;g_rateCount[slot]=0;}
    if(g_rateCount[slot]>=16)return false;
    ++g_rateCount[slot];return true;
}
}
void Install(net::Session* session) {
    const auto world=ue_wrap::world_identity::Generation();
    if(g_session!=session || g_world!=world){OnDisconnect();g_session=session;g_world=world;}
    Watch();
}
void Tick() {
    if(!Current() || g_ticking)return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    Watch();Reconcile();const auto epoch=g_epoch,now=active_drive::NowMs();
    const bool host=g_session->role()==net::Role::Host;
    if(host)std::stable_partition(g_events.begin(),g_events.end(),[](const Event& event){
        return event.authorized && event.packet.op==1 && event.packet.sequence;});
    std::deque<Event> retained;
    uint32_t blocked=0;const auto count=g_events.size();
    for(size_t i=0;i<count && !g_events.empty();++i) {
        auto event=g_events.front();g_events.pop_front();auto& p=event.packet;
        if(now>event.expires)continue;
        if(event.own) {
            void* actor=event.source.Get();
            if(!actor || p.slot!=Local() || (event.hand?actor!=hand_item::LocalHandActor():!WorldSource(actor)))continue;
            const auto person=roster_ledger::Get(p.slot).playerNo;
            if(p.playerNo && p.playerNo!=person)continue;
            if(!p.playerNo)p.playerNo=person;
            if(event.hand) {
                const auto generation=hand_item::Generation(p.slot);
                if(p.generation && generation && p.generation!=generation)continue;
                if(!p.generation)p.generation=generation;
            }
            if(!p.playerNo || (event.hand && !p.generation)) {
                retained.push_back(event);g_events.insert(g_events.begin(),retained.begin(),retained.end());return;
            }
        }
        if(!SourceCurrent(event))continue;
        if(event.hand) {
            const auto current=hand_item::Generation(p.slot);
            if(current && current!=p.generation && !After(p.generation,current))continue;
        }
        if(host && !event.authorized) {
            void* actor=Source(event);
            if(!(event.hand?FX::IsSponge(actor):WorldSource(actor))){retained.push_back(event);continue;}
            CachedObjRef live;live.Set(actor);
            const auto target=element::IntentTarget::ForClientIntent(*g_session,p.slot,600.f);
            const ue_wrap::FVector point{p.x,p.y,p.z};
            bool allowed=bool(target.Authorize(actor));
            if(!Current(epoch))return;
            if(!allowed || !live.Get() || Source(event)!=actor || !SourceCurrent(event))continue;
            allowed=bool(target.AuthorizeSegment(point,point));
            if(!Current(epoch))return;
            if(!allowed || !live.Get() || Source(event)!=actor || !SourceCurrent(event))continue;
            event.authorized=true;p.op=1;p.sequence=0;SetPending(event,Mask(p.slot));
        }
        if(host && !p.sequence)p.sequence=Next(g_sequence);
        if(event.play) {
            const auto valid=[&]{return Current(epoch) && SourceCurrent(event);};
            if(!event.surface && FX::Surface({p.x,p.y,p.z},{p.nx,p.ny,p.nz},valid))event.surface=true;
            if(!Current(epoch))return;
            if(!SourceCurrent(event))continue;
            if(!event.burst) {
                void* actor=Source(event);
                const auto sourceValid=[&]{return valid() && Source(event)==actor &&
                    (event.hand?FX::IsSponge(actor):WorldSource(actor));};
                if(sourceValid() && FX::Burst(actor,sourceValid))event.burst=true;
                if(!Current(epoch))return;
            }
            event.play=!event.surface || !event.burst;
        }
        if(!SourceCurrent(event))continue;
        for(int slot=0;slot<net::kMaxPeers;++slot)if((event.pending&~blocked)&(1u<<slot)) {
            if(!g_session->IsSlotWorldReady(slot) || event.recipients[slot]!=roster_ledger::Get(slot).playerNo) {
                event.pending&=~(1u<<slot);continue;
            }
            const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::SpongeEffect,&p,sizeof(p));
            if(!Current(epoch))return;
            if(!SourceCurrent(event))break;
            if(sent || event.recipients[slot]!=roster_ledger::Get(slot).playerNo)
                event.pending&=~(1u<<slot);
        }
        if(!SourceCurrent(event))continue;
        blocked|=event.pending;
        if(event.pending || event.play)retained.push_back(event);
    }
    g_events.insert(g_events.begin(),retained.begin(),retained.end());
}
void Receive(const Packet& p,uint8_t sender) {
    if(!Current() || !p.sequence || p.op>1 || p.slot>=net::kMaxPeers || !p.playerNo ||
       roster_ledger::Get(p.slot).playerNo!=p.playerNo || !FX::ValidSurface({p.x,p.y,p.z},{p.nx,p.ny,p.nz}) ||
       (p.key.len?(!KeyValid(p.key) || p.generation):!p.generation))return;
    Reconcile();const auto now=active_drive::NowMs();
    Event event{};event.packet=p;event.hand=!p.key.len;event.expires=now+1500;event.play=true;
    if(g_session->role()==net::Role::Host) {
        if(sender==0 || sender>=net::kMaxPeers || sender!=p.slot || p.op || !g_session->IsSlotWorldReady(sender) ||
           (g_seen[sender] && !After(p.sequence,g_seen[sender])) || !Rate(sender,now) || g_events.size()>=128)return;
        g_seen[sender]=p.sequence;g_events.push_back(event);return;
    }
    if(sender!=0 || p.op!=1 || p.slot==Local() || (g_lastHost && !After(p.sequence,g_lastHost)))return;
    g_lastHost=p.sequence;event.authorized=true;
    if(g_events.size()<128)g_events.push_back(event);
}
void OnPeerLeft(int slot) {
    if(slot<0 || slot>=net::kMaxPeers)return;
    g_people[slot]=0;g_seen[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;
    std::erase_if(g_events,[slot](Event& event){event.pending&=~(1u<<slot);return event.packet.slot==slot;});
}
void OnDisconnect() {
    ++g_epoch;g_session=nullptr;g_world=g_request=g_sequence=g_lastHost=0;g_frames.clear();g_events.clear();
    for(int slot=0;slot<net::kMaxPeers;++slot){g_seen[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;g_people[slot]=0;}
}
}
