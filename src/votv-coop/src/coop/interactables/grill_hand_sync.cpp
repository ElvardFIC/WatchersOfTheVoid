#include "coop/interactables/grill_hand_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/devices/grill.h"
#include <array>
#include <cmath>
namespace coop::grill_hand_sync {
namespace {
namespace G=ue_wrap::grill;
using Packet=net::GrillHandPayload;
using ue_wrap::CachedObjRef;
struct Row {
    Packet packet{};
    CachedObjRef painted;
    uint64_t at=0,next=0;
    uint32_t applied=0;
    uint32_t pending=0;
    bool admitted=false,expired=false;
};
std::array<Row,net::kMaxPeers> g_rows{};
uint16_t g_people[net::kMaxPeers]{};
uint8_t g_rates[net::kMaxPeers]{};
uint64_t g_rateAt[net::kMaxPeers]{},g_poll=0,g_epoch=0;
uint32_t g_world=0,g_sequence=0;
bool g_ticking=false;
net::Session* g_session=nullptr;
uint8_t Local(){return players::Registry::Get().LocalPeerId();}
bool Current(uint64_t epoch){return g_session && g_session->connected() && epoch==g_epoch &&
    g_world==ue_wrap::world_identity::Generation();}
uint32_t Mask(int except){uint32_t mask=0;for(int slot=1;slot<net::kMaxPeers;++slot)
    if(slot!=except && g_session->IsSlotWorldReady(slot))mask|=uint32_t(1u<<slot);return mask;}
G::State Decode(const Packet& p){return {bool(p.flags&1),bool(p.flags&2),bool(p.flags&4),p.coals,p.burn,p.volume};}
void Encode(const G::State& s,Packet& p){p.flags=uint8_t(s.active)|(uint8_t(s.lid)<<1)|(uint8_t(s.cooking)<<2);
    p.coals=s.coals;p.burn=s.burn;p.volume=s.volume;}
void* Actor(const Packet& p){
    if(p.slot>=net::kMaxPeers || !p.playerNo || roster_ledger::Get(p.slot).playerNo!=p.playerNo)return nullptr;
    return p.slot==Local() ? (hand_item::Generation(p.slot)==p.generation?hand_item::LocalHandActor():nullptr) :
        (hand_item::MirrorGeneration(p.slot)==p.generation?hand_item::MirrorActorForSlot(p.slot):nullptr);
}
void Stop(CachedObjRef actor,uint32_t world){
    // Remote hands are disposable display actors; their roster row may already be gone.
    const auto detached=[&]{
        if(world!=ue_wrap::world_identity::Generation() || !actor.Get() || actor.Get()==hand_item::LocalHandActor())return false;
        for(const auto& row:g_rows)if(row.painted.Is(actor.Raw()))return false;
        return true;
    };
    if(detached())G::StopSound(actor.Get(),detached);
}
void World(){
    const auto world=ue_wrap::world_identity::Generation();if(g_world==world)return;
    auto* session=g_session;const auto epoch=g_epoch;OnDisconnect();
    if(g_epoch!=epoch+1 || g_session)return;g_session=session;g_world=world;
}
bool Reconcile(){
    const auto epoch=g_epoch;
    for(int slot=0;slot<net::kMaxPeers;++slot){
        const auto person=roster_ledger::Get(slot).playerNo;if(person==g_people[slot])continue;
        if(g_people[slot])OnPeerLeft(slot);
        if(!Current(epoch) || roster_ledger::Get(slot).playerNo!=person)return false;
        g_people[slot]=person;
    }
    return true;
}
bool Same(const Packet& a,const Packet& b){return a.generation==b.generation && a.playerNo==b.playerNo &&
    a.flags==b.flags && a.coals==b.coals && std::fabs(a.burn-b.burn)<.01f && std::fabs(a.volume-b.volume)<.002f;}
void Sample(uint64_t now){
    const auto slot=Local();if(slot>=net::kMaxPeers)return;
    void* actor=hand_item::LocalHandActor();const auto generation=hand_item::Generation(slot);
    const auto person=roster_ledger::Get(slot).playerNo;
    if(!generation || !person || !G::IsGrill(actor)){g_rows[slot]={};return;}
    CachedObjRef live;live.Set(actor);const auto epoch=g_epoch;G::State value{};
    if(!G::Read(actor,value) || !Current(epoch) || !live.Get() || hand_item::LocalHandActor()!=actor ||
        hand_item::Generation(slot)!=generation || roster_ledger::Get(slot).playerNo!=person)return;
    Packet p{};p.slot=slot;p.playerNo=person;p.generation=generation;Encode(value,p);
    auto& row=g_rows[slot];if(row.packet.sequence && Same(row.packet,p) && now-row.at<1000)return;
    if(!++g_sequence)++g_sequence;p.sequence=g_sequence;
    row.packet=p;row.at=now;row.admitted=true;row.pending=g_session->role()==net::Role::Host?Mask(slot):1;
}
void Apply(uint64_t now){
    const auto epoch=g_epoch;const bool host=g_session->role()==net::Role::Host;
    for(uint8_t slot=0;slot<net::kMaxPeers;++slot){
        if(slot==Local())continue;
        const auto snapshot=g_rows[slot];if(!snapshot.packet.sequence && !snapshot.painted.Raw())continue;
        void* actor=Actor(snapshot.packet);
        const auto current=[&]{return Current(epoch) && g_rows[slot].packet.sequence==snapshot.packet.sequence &&
            g_rows[slot].packet.playerNo==snapshot.packet.playerNo && g_rows[slot].packet.generation==snapshot.packet.generation && Actor(snapshot.packet)==actor;};
        const bool expired=now-snapshot.at>3000;
        if(snapshot.painted.Raw() && (snapshot.painted.Get()!=actor || (!snapshot.expired && expired))){
            g_rows[slot].painted={};g_rows[slot].applied=0;Stop(snapshot.painted,g_world);
            if(!Current(epoch))return;if(!current())continue;
        }
        if(!G::IsGrill(actor) || (expired && !snapshot.admitted) || now<snapshot.next)continue;
        if(snapshot.painted.Is(actor) && snapshot.applied==snapshot.packet.sequence && snapshot.expired==expired)continue;
        CachedObjRef live;live.Set(actor);
        const auto valid=[&]{return current() && live.Get();};
        if(host && !snapshot.admitted){
            const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,slot,600.f).Authorize(actor));
            if(!Current(epoch))return;if(!allowed || !valid())continue;
        }
        auto state=Decode(snapshot.packet);if(expired)state.cooking=false;
        g_rows[slot].next=now+100;g_rows[slot].painted=live;
        const bool applied=G::Apply(actor,state,valid);
        if(!Current(epoch))return;if(!valid())continue;
        if(applied){auto& row=g_rows[slot];row.applied=snapshot.packet.sequence;row.expired=expired;
            if(!row.admitted && host)row.pending=Mask(slot);row.admitted=true;}
    }
}
void Flush(uint64_t now){
    const auto epoch=g_epoch;
    for(uint8_t owner=0;owner<net::kMaxPeers;++owner){
        const auto row=g_rows[owner];if(!row.packet.sequence || !row.admitted || !row.pending)continue;
        auto packet=row.packet;if(now-row.at>3000)packet.flags&=~4u;
        const auto mask=g_session->role()==net::Role::Client?uint8_t(1):row.pending;
        for(int slot=0;slot<net::kMaxPeers;++slot)if(mask&(1u<<slot)){
            if(slot && !g_session->IsSlotWorldReady(slot))continue;
            const auto person=roster_ledger::Get(slot).playerNo;
            const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::GrillHand,&packet,sizeof(packet));
            if(!Current(epoch))return;
            auto& latest=g_rows[owner];if(sent && latest.packet.sequence==packet.sequence &&
                latest.packet.playerNo==packet.playerNo && latest.packet.generation==packet.generation &&
                roster_ledger::Get(slot).playerNo==person)latest.pending&=uint32_t(~(1u<<slot));
        }
    }
}
}
void Install(net::Session* session){
    if(g_session!=session){const auto epoch=g_epoch;OnDisconnect();if(g_epoch!=epoch+1 || g_session)return;}
    g_session=session;World();
}
void Tick(){
    if(!g_session || !g_session->connected() || g_ticking)return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    World();const auto epoch=g_epoch;if(!Reconcile())return;const auto now=active_drive::NowMs();
    if(now>=g_poll){g_poll=now+100;Sample(now);}if(!Current(epoch))return;
    Apply(now);if(Current(epoch))Flush(now);
}
void Receive(const Packet& p,uint8_t sender){
    if(!g_session || !g_session->connected() || p.slot>=net::kMaxPeers || !p.playerNo || !p.generation || !p.sequence ||
        (p.flags&~7u) || !G::Valid(Decode(p)))return;
    World();const auto epoch=g_epoch;if(!Reconcile() || !Current(epoch) || roster_ledger::Get(p.slot).playerNo!=p.playerNo)return;
    const bool host=g_session->role()==net::Role::Host;
    if(host ? (!sender || sender!=p.slot || !g_session->IsSlotWorldReady(sender)) : (sender || p.slot==Local()))return;
    auto& row=g_rows[p.slot];if(row.packet.sequence && int32_t(p.sequence-row.packet.sequence)<=0)return;
    const auto now=active_drive::NowMs();
    if(host){if(now-g_rateAt[p.slot]>=1000){g_rateAt[p.slot]=now;g_rates[p.slot]=0;}
        if(g_rates[p.slot]>=20)return;++g_rates[p.slot];}
    row.packet=p;row.at=now;row.next=0;row.pending=0;row.admitted=!host;
}
void QueueConnect(int slot){
    if(!g_session || !g_session->connected() || g_session->role()!=net::Role::Host || slot<1 || slot>=net::kMaxPeers)return;
    World();if(!Reconcile())return;
    for(auto& row:g_rows)if(row.packet.sequence && row.admitted && row.packet.slot!=slot)row.pending|=uint32_t(1u<<slot);
    g_poll=0;
}
void OnPeerLeft(int slot){
    if(slot<0 || slot>=net::kMaxPeers)return;const auto previous=g_rows[slot].painted;
    g_rows[slot]={};g_people[slot]=0;g_rates[slot]=0;g_rateAt[slot]=0;
    for(auto& row:g_rows)row.pending&=uint32_t(~(1u<<slot));Stop(previous,g_world);
}
void OnDisconnect(){
    const auto previous=g_rows;const auto world=g_world;++g_epoch;g_session=nullptr;g_rows={};g_world=g_sequence=0;g_poll=0;
    for(int slot=0;slot<net::kMaxPeers;++slot){g_people[slot]=0;g_rates[slot]=0;g_rateAt[slot]=0;}
    for(const auto& row:previous)Stop(row.painted,world);
}
}
