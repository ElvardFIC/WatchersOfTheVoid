#include "coop/props/sponge_effects_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
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
namespace R=ue_wrap::reflection;
using Packet=net::SpongeEffectPayload;
using ue_wrap::CachedObjRef;
struct Frame {
    void* object=nullptr;CachedObjRef actor,cls,function,user,userClass;FX::Use use{};
    uint64_t epoch=0,id=0,invocation=0,peerEpoch=0;void* stack=nullptr;int depth=0;
    uint8_t slot=0;uint16_t person=0;uint32_t generation=0,transport=0;
    std::wstring key;bool hand=false,ready=false;
};
struct Event {
    Packet packet{};CachedObjRef source,cls;
    uint64_t created=0,epoch=0,peerEpoch=0,id=0;uint32_t transport=0,pending=0;
    std::array<uint16_t,net::kMaxPeers> recipients{};
    std::array<uint32_t,net::kMaxPeers> connections{};
    std::array<uint64_t,net::kMaxPeers> recipientEpochs{};
    bool own=false,hand=false,authorized=false,play=false,surface=false,burst=false,bound=false;
};
net::Session* g_session=nullptr;
uint32_t g_world=0,g_serial=0,g_request=0,g_sequence=0,g_lastHost=0,g_seen[net::kMaxPeers]{};
net::Role g_role=net::Role::Host;bool g_running=false,g_connected=false;
uint64_t g_epoch=0,g_capture=0,g_lastNow=0,g_rateAt[net::kMaxPeers]{},g_peerEpoch[net::kMaxPeers]{};
uint64_t g_eventId=0,g_playCursor=0;
uint32_t g_connections[net::kMaxPeers]{};
uint16_t g_people[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};
std::vector<Frame> g_frames;
std::deque<Event> g_events;
bool g_ticking=false,g_watch=false;
uint64_t g_watchRetry=0;
constexpr int kTag=0x53504F4E;
constexpr unsigned kAuthorizations=8,kPresentations=8,kSends=64;
bool Current(){return g_session && g_session->running() && g_session->connected() &&
    g_world==ue_wrap::world_identity::Generation() && g_serial==net::session_serial::Current() && g_role==g_session->role();}
bool Current(uint64_t epoch){return Current() && g_epoch==epoch;}
void SyncSource(){
    auto* session=g_session;if(!session)return;
    if(g_world==ue_wrap::world_identity::Generation() && g_serial==net::session_serial::Current() &&
       g_role==session->role() && g_running==session->running() && g_connected==session->connected())return;
    OnDisconnect();g_session=session;g_world=ue_wrap::world_identity::Generation();
    g_serial=net::session_serial::Current();g_role=session->role();g_running=session->running();g_connected=session->connected();
}
uint64_t Now(){
    const auto now=active_drive::NowMs();
    if(g_lastNow && now<g_lastNow){
        ++g_epoch;g_frames.clear();g_events.clear();g_watchRetry=0;g_playCursor=0;
        for(int slot=0;slot<net::kMaxPeers;++slot){g_rateAt[slot]=0;g_rateCount[slot]=0;}
    }
    g_lastNow=now;return now;
}
uint32_t Next(uint32_t& value){if(!++value)++value;return value;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
uint8_t Local(){return players::Registry::Get().LocalPeerId();}
uint32_t Transport(uint8_t origin){return g_session->peerGenerationForSlot(g_role==net::Role::Host?origin:0);}
bool ClassCurrent(void* actor,const CachedObjRef& live,const CachedObjRef& cls){
    return actor && live.Get()==actor && cls.Get() && R::ClassOf(actor)==cls.Raw();
}
bool KeyValid(const net::WireKey& key){
    if(!key.len || key.len>sizeof(key.data))return false;
    for(unsigned i=0;i<key.len;++i)if(uint8_t(key.data[i])<33 || uint8_t(key.data[i])>126)return false;
    return net::StringFromWireKey(key)!=L"None";
}
bool WorldSource(void* actor){
    return Current() && FX::IsSponge(actor) && !hand_item::IsHandAxisActor(actor) && ue_wrap::world_identity::CurrentWorld() &&
        ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld() &&
        (g_role!=net::Role::Client || net_pump::IsInAnnouncedWorld(actor));
}
void Reconcile(){
    for(int slot=0;slot<net::kMaxPeers;++slot){
        const auto person=roster_ledger::Get(slot).playerNo;
        const auto connection=g_session->peerGenerationForSlot(slot);
        if(person==g_people[slot] && connection==g_connections[slot])continue;
        if(g_people[slot] || g_connections[slot])OnPeerLeft(slot);
        g_people[slot]=person;g_connections[slot]=connection;
    }
}
uint32_t Mask(int except){
    uint32_t mask=0;for(int slot=1;slot<net::kMaxPeers;++slot)
        if(slot!=except && g_session->IsSlotWorldReady(slot) && roster_ledger::Get(slot).playerNo &&
           g_session->peerGenerationForSlot(slot))mask|=1u<<slot;
    return mask;
}
void SetPending(Event& event,uint32_t mask){
    event.pending=mask;
    for(int slot=0;slot<net::kMaxPeers;++slot){
        event.recipients[slot]=roster_ledger::Get(slot).playerNo;
        event.connections[slot]=g_session->peerGenerationForSlot(slot);
        event.recipientEpochs[slot]=g_peerEpoch[slot];
    }
}
bool RecipientCurrent(const Event& event,int slot){
    return Current(event.epoch) && event.recipients[slot]==roster_ledger::Get(slot).playerNo &&
        event.connections[slot]==g_session->peerGenerationForSlot(slot) &&
        event.recipientEpochs[slot]==g_peerEpoch[slot] && g_session->IsSlotWorldReady(slot);
}
bool SourceCurrent(const Event& event){
    const auto& p=event.packet;
    const auto now=active_drive::NowMs();
    if(now<event.created || now-event.created>=1500)return false;
    if(!Current(event.epoch) || p.slot>=net::kMaxPeers || !p.playerNo ||
       roster_ledger::Get(p.slot).playerNo!=p.playerNo || g_peerEpoch[p.slot]!=event.peerEpoch ||
       Transport(p.slot)!=event.transport)return false;
    if(event.bound && !ClassCurrent(event.source.Raw(),event.source,event.cls))return false;
    if(event.hand){
        const auto generation=hand_item::Generation(p.slot);
        if(event.own && generation!=p.generation)return false;
        if(generation && generation!=p.generation && !After(p.generation,generation))return false;
    }
    return true;
}
void* Source(const Event& event){
    const auto& p=event.packet;
    if(!event.hand)return prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key));
    if(p.slot==Local())return hand_item::Generation(p.slot)==p.generation?hand_item::LocalHandActor():nullptr;
    return hand_item::MirrorGeneration(p.slot)==p.generation?hand_item::MirrorActorForSlot(p.slot):nullptr;
}
bool TargetCurrent(const Event& event){
    if(!SourceCurrent(event))return false;
    if(!event.bound)return true;
    void* actor=event.source.Raw();
    if(Source(event)!=actor || !SourceCurrent(event))return false;
    if(event.hand)return true;
    const auto key=ue_wrap::prop::GetInteractableKeyString(actor);
    return SourceCurrent(event) && key==net::StringFromWireKey(event.packet.key);
}
bool FrameCurrent(const Frame& frame){
    return Current(frame.epoch) && frame.slot<net::kMaxPeers && frame.person && Local()==frame.slot &&
        roster_ledger::Get(frame.slot).playerNo==frame.person && g_peerEpoch[frame.slot]==frame.peerEpoch &&
        Transport(frame.slot)==frame.transport && ClassCurrent(frame.object,frame.actor,frame.cls) &&
        frame.function.Get() && ClassCurrent(frame.user.Raw(),frame.user,frame.userClass) &&
        players::Registry::Get().Local()==frame.user.Raw() && SG::IsInvocationActive(frame.invocation) && (!frame.hand ||
        (frame.generation && hand_item::LocalHandActor()==frame.object && hand_item::Generation(frame.slot)==frame.generation));
}
bool FrameTargetCurrent(const Frame& frame){
    if(!FrameCurrent(frame))return false;
    if(frame.hand)return true;
    if(!WorldSource(frame.object) || !FrameCurrent(frame))return false;
    const auto key=ue_wrap::prop::GetInteractableKeyString(frame.object);
    return FrameCurrent(frame) && key==frame.key;
}
void PruneFrames(){std::erase_if(g_frames,[](const Frame& frame){return !FrameCurrent(frame);});}
SG::Verdict Before(const SG::Call& call){
    SyncSource();if(!Current() || call.fromOurCode || !call.object || !R::IsLive(call.object))return SG::Verdict::Run;
    Frame frame;frame.actor.Set(call.object);frame.cls.Set(R::ClassOf(call.object));frame.epoch=g_epoch;
    frame.object=call.object;
    if(!FX::IsSponge(call.object) || !Current(frame.epoch) || !ClassCurrent(call.object,frame.actor,frame.cls))return SG::Verdict::Run;
    if(hand_item::IsMirrorActor(call.object))return SG::Verdict::Cancel;
    Now();Reconcile();PruneFrames();
    if(!Current(frame.epoch) || !call.invocation || !call.function || !R::IsLive(call.function) ||
       g_frames.size()>=16 || g_events.size()>=128)return SG::Verdict::Run;
    frame.function.Set(call.function);frame.invocation=call.invocation;frame.stack=call.stack;frame.depth=call.depth;
    frame.id=++g_capture;frame.slot=Local();if(frame.slot>=net::kMaxPeers)return SG::Verdict::Run;
    void* user=players::Registry::Get().Local();
    if(!Current(frame.epoch) || !user || !R::IsLive(user))return SG::Verdict::Run;
    frame.user.Set(user);frame.userClass.Set(R::ClassOf(user));
    frame.person=roster_ledger::Get(frame.slot).playerNo;frame.peerEpoch=g_peerEpoch[frame.slot];frame.transport=Transport(frame.slot);
    frame.hand=call.object==hand_item::LocalHandActor();if(frame.hand)frame.generation=hand_item::Generation(frame.slot);
    if(!FrameCurrent(frame))return SG::Verdict::Run;
    // Reserve ownership before callback-capable key/trace/stock reads. Nested
    // observations cannot exceed capacity or overwrite this row's completion.
    g_frames.push_back(frame);
    const auto retire=[&]{std::erase_if(g_frames,[&](const Frame& row){return row.id==frame.id;});};
    if(!frame.hand){
        if(!WorldSource(frame.object) || !FrameCurrent(frame)){retire();return SG::Verdict::Run;}
        frame.key=ue_wrap::prop::GetInteractableKeyString(frame.object);
        net::WireKey key{};net::WireKeyFromString(frame.key,key);
        if(!FrameCurrent(frame) || frame.key.size()>sizeof(key.data) || !KeyValid(key)){retire();return SG::Verdict::Run;}
    }
    if(!FX::ReadUse(frame.object,frame.user.Raw(),frame.use) || !FrameTargetCurrent(frame)){
        retire();return SG::Verdict::Run;
    }
    const auto it=std::find_if(g_frames.begin(),g_frames.end(),[&](const Frame& row){return row.id==frame.id;});
    if(it!=g_frames.end()){frame.ready=true;*it=frame;}
    return SG::Verdict::Run;
}
void AfterUse(const SG::Call& call){
    SyncSource();
    const auto it=std::find_if(g_frames.rbegin(),g_frames.rend(),[&](const Frame& frame){
        return frame.invocation==call.invocation && frame.function.Raw()==call.function &&
            frame.stack==call.stack && frame.depth==call.depth && frame.object==call.object;
    });
    if(it==g_frames.rend())return;
    const auto frame=*it;g_frames.erase(std::next(it).base());
    if(!frame.ready || call.fromOurCode || !FrameTargetCurrent(frame) || g_events.size()>=128)return;
    float soap=0;
    if(!FX::ReadSoap(call.object,soap) || !FrameTargetCurrent(frame) || !std::isfinite(soap) || soap>frame.use.soap-4.99f)return;
    Event event;event.epoch=frame.epoch;event.peerEpoch=frame.peerEpoch;event.transport=frame.transport;
    event.own=true;event.hand=frame.hand;event.source=frame.actor;event.cls=frame.cls;event.bound=event.authorized=true;
    auto& p=event.packet;p.slot=frame.slot;p.playerNo=frame.person;
    p.x=frame.use.location.X;p.y=frame.use.location.Y;p.z=frame.use.location.Z;
    p.nx=frame.use.normal.X;p.ny=frame.use.normal.Y;p.nz=frame.use.normal.Z;
    if(frame.hand)p.generation=frame.generation;else net::WireKeyFromString(frame.key,p.key);
    event.created=Now();if(!FrameTargetCurrent(frame))return;
    if(g_role==net::Role::Host){p.op=1;SetPending(event,Mask(p.slot));}
    else {p.sequence=Next(g_request);SetPending(event,1u);}
    if(event.pending){event.id=++g_eventId;g_events.push_back(event);}
}
void Watch(){
    if(g_watch || active_drive::NowMs()<g_watchRetry)return;
    g_watchRetry=active_drive::NowMs()+1000;
    g_watch=SG::WatchClassName(L"prop_sponge_C",L"cleanBubbles",kTag,&Before,&AfterUse);
    SG::ResolvePendingNames();
}
bool Rate(uint8_t slot,uint64_t now){
    if(now<g_rateAt[slot] || now-g_rateAt[slot]>=1000){g_rateAt[slot]=now;g_rateCount[slot]=0;}
    if(g_rateCount[slot]>=16)return false;++g_rateCount[slot];return true;
}
}
void Install(net::Session* session){
    if(g_session!=session){OnDisconnect();g_session=session;}
    SyncSource();Watch();
}
void Tick(){
    SyncSource();if(!Current() || g_ticking)return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    const auto now=Now(),epoch=g_epoch;
    Watch();if(!Current(epoch))return;Reconcile();PruneFrames();
    const bool host=g_role==net::Role::Host;
    if(host)std::stable_partition(g_events.begin(),g_events.end(),[](const Event& event){
        return event.authorized && event.packet.op==1 && event.packet.sequence;
    });
    std::deque<Event> retained;uint32_t blocked=0;
    unsigned auth=kAuthorizations,sends=kSends;
    // Rotate native presentation independently of ordered wire delivery. An
    // unresolved early mirror must not consume every later ready row's budget.
    std::vector<uint64_t> eligible;
    for(const auto& event:g_events)if(event.play && SourceCurrent(event))eligible.push_back(event.id);
    std::sort(eligible.begin(),eligible.end());
    std::array<uint64_t,kPresentations> selected{};size_t selectedCount=0;
    if(!eligible.empty()){
        auto it=std::upper_bound(eligible.begin(),eligible.end(),g_playCursor);
        if(it==eligible.end())it=eligible.begin();
        while(selectedCount<selected.size() && selectedCount<eligible.size()){
            selected[selectedCount++]=*it;if(++it==eligible.end())it=eligible.begin();
        }
        g_playCursor=selected[selectedCount-1];
    }
    const auto count=g_events.size();
    for(size_t i=0;i<count && !g_events.empty();++i){
        if(!Current(epoch))return;
        auto event=g_events.front();g_events.pop_front();auto& p=event.packet;
        if(now<event.created || now-event.created>=1500 || !SourceCurrent(event))continue;
        if((event.own && p.slot!=Local()) || (event.bound && !TargetCurrent(event)))continue;
        if(!Current(epoch))return;
        if(host && !event.authorized){
            void* actor=Source(event);if(!SourceCurrent(event))continue;
            if(!actor || !R::IsLive(actor)){retained.push_back(event);continue;}
            event.source.Set(actor);event.cls.Set(R::ClassOf(actor));event.bound=true;
            const auto valid=[&]{return TargetCurrent(event);};
            const bool sponge=event.hand?FX::IsSponge(actor):WorldSource(actor);
            if(!Current(epoch))return;
            if(!valid())continue;
            if(!sponge){retained.push_back(event);continue;}
            if(!auth){retained.push_back(event);continue;}--auth;
            const auto target=element::IntentTarget::ForClientIntent(*g_session,p.slot,600.f);
            if(!valid())continue;
            const bool allowed=bool(target.Authorize(actor));if(!Current(epoch))return;
            if(!allowed || !valid())continue;
            const ue_wrap::FVector point{p.x,p.y,p.z};
            const bool reached=bool(target.AuthorizeSegment(point,point));if(!Current(epoch))return;
            if(!reached || !valid())continue;
            event.authorized=true;p.op=1;p.sequence=0;SetPending(event,Mask(p.slot));
        }
        if(host && !p.sequence)p.sequence=Next(g_sequence);
        if(event.play && std::find(selected.begin(),selected.begin()+selectedCount,event.id)!=selected.begin()+selectedCount){
            const auto valid=[&]{return TargetCurrent(event);};
            if(!event.surface && FX::Surface({p.x,p.y,p.z},{p.nx,p.ny,p.nz},valid))event.surface=true;
            if(!Current(epoch))return;if(!valid())continue;
            if(!event.burst){
                void* actor=Source(event);if(!SourceCurrent(event))continue;
                if(actor && R::IsLive(actor)){
                    if(!event.bound){event.source.Set(actor);event.cls.Set(R::ClassOf(actor));event.bound=true;}
                    const auto sourceValid=[&]{
                        if(!TargetCurrent(event) || event.source.Raw()!=actor)return false;
                        const bool sponge=event.hand?FX::IsSponge(actor):WorldSource(actor);
                        return sponge && TargetCurrent(event);
                    };
                    if(sourceValid() && FX::Burst(actor,sourceValid))event.burst=true;
                }
                if(!Current(epoch))return;
            }
            event.play=!event.surface || !event.burst;
        }
        if(!SourceCurrent(event))continue;
        for(int slot=0;slot<net::kMaxPeers;++slot)if((event.pending&~blocked)&(1u<<slot)){
            if(!RecipientCurrent(event,slot)){event.pending&=~(1u<<slot);continue;}
            if(!sends)break;--sends;
            if(!TargetCurrent(event))break;if(!Current(epoch))return;
            const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::SpongeEffect,&p,sizeof(p));
            if(!Current(epoch))return;if(!TargetCurrent(event))break;
            if(sent || !RecipientCurrent(event,slot))event.pending&=~(1u<<slot);
        }
        if(!TargetCurrent(event))continue;
        blocked|=event.pending;if(event.pending || event.play)retained.push_back(event);
    }
    if(!Current(epoch))return;
    // A departure may have occurred while a detached earlier row was retained.
    std::erase_if(retained,[&](Event& event){
        if(!TargetCurrent(event))return true;
        for(int slot=0;slot<net::kMaxPeers;++slot)if(!RecipientCurrent(event,slot))event.pending&=~(1u<<slot);
        return !event.pending && !event.play;
    });
    if(Current(epoch))g_events.insert(g_events.begin(),retained.begin(),retained.end());
}
void Receive(const Packet& incoming,uint8_t sender){
    const Packet p=incoming;SyncSource();if(!Current())return;
    const auto now=Now();Reconcile();
    if(!p.sequence || p.op>1 || p.slot>=net::kMaxPeers || !p.playerNo ||
       roster_ledger::Get(p.slot).playerNo!=p.playerNo || !FX::ValidSurface({p.x,p.y,p.z},{p.nx,p.ny,p.nz}) ||
       (p.key.len?(!KeyValid(p.key) || p.generation):!p.generation))return;
    Event event;event.packet=p;event.hand=!p.key.len;event.created=now;event.play=true;
    event.epoch=g_epoch;event.peerEpoch=g_peerEpoch[p.slot];event.transport=Transport(p.slot);
    if(g_role==net::Role::Host){
        if(!sender || sender>=net::kMaxPeers || sender!=p.slot || p.op || !g_session->IsSlotWorldReady(sender) ||
           !event.transport || (g_seen[sender] && !After(p.sequence,g_seen[sender])) || g_events.size()>=128 || !Rate(sender,now))return;
        event.id=++g_eventId;g_events.push_back(event);g_seen[sender]=p.sequence;return;
    }
    if(sender || p.op!=1 || p.slot==Local() || (g_lastHost && !After(p.sequence,g_lastHost)) || g_events.size()>=128)return;
    event.authorized=true;event.id=++g_eventId;g_events.push_back(event);g_lastHost=p.sequence;
}
void OnPeerLeft(int slot){
    if(slot<0 || slot>=net::kMaxPeers)return;
    ++g_peerEpoch[slot];g_people[slot]=0;g_connections[slot]=0;g_seen[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;
    std::erase_if(g_events,[slot](Event& event){event.pending&=~(1u<<slot);return event.packet.slot==slot;});
    std::erase_if(g_frames,[slot](const Frame& frame){return frame.slot==slot;});
}
void OnDisconnect(){
    ++g_epoch;g_session=nullptr;g_world=g_serial=g_request=g_sequence=g_lastHost=0;
    g_running=g_connected=false;g_role=net::Role::Host;g_lastNow=g_playCursor=0;g_frames.clear();g_events.clear();
    for(int slot=0;slot<net::kMaxPeers;++slot){
        g_seen[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;g_people[slot]=0;g_connections[slot]=0;g_peerEpoch[slot]=0;
    }
}
}
