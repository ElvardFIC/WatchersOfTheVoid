#include "coop/player/emote_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/player/hand_item.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/actors/emote_wheel.h"
#include <algorithm>
#include <array>

namespace coop::emote_sync {
namespace {
namespace E=ue_wrap::emote_wheel;
using Packet=net::EmoteStatePayload;
struct Identity {uint16_t person=0;uint32_t generation=0;uint8_t origin=0;};
struct Delivery {uint32_t sequence=0;uint64_t at=0;Identity recipient{};bool ready=false;};
struct Row {
    Packet packet{};uint64_t at=0;float transit=0;uint64_t revision=0;
    Identity owner{};E::Playback playback{};
    std::array<Delivery,net::kMaxPeers> deliveries{};
};
std::array<Row,net::kMaxPeers> g_rows{};
std::array<Identity,net::kMaxPeers> g_owners{};
std::array<uint32_t,net::kMaxPeers> g_seen{};
std::array<uint64_t,net::kMaxPeers> g_rateAt{};
std::array<unsigned,net::kMaxPeers> g_rateCount{};
net::Session* g_session=nullptr;
uint32_t g_world=0,g_serial=0,g_sequence=0,g_canonical=0;
uint64_t g_stops=0,g_epoch=0,g_revision=0,g_poll=0,g_localAt=0;
net::Role g_role=net::Role::Host;
bool g_running=false,g_connected=false,g_ticking=false;
size_t g_cursor=0;
Packet g_local{};bool g_localPending=false;

bool Same(const Identity& a,const Identity& b){return a.person==b.person && a.generation==b.generation && a.origin==b.origin;}
Identity Address(int slot){const auto& person=roster_ledger::Get(slot);return {person.playerNo,g_session->peerGenerationForSlot(slot),person.originContext};}
uint32_t Next(uint32_t& value){if(!++value)++value;return value;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
bool Current(){return g_session && g_world==ue_wrap::world_identity::Generation() && g_serial==net::session_serial::Current() &&
    g_stops==g_session->StopCount() && g_role==g_session->role() && g_running==g_session->running() &&
    (g_role==net::Role::Host || g_connected==g_session->connected()) && g_running && (g_role==net::Role::Host || g_connected);}
E::State StateOf(const Packet& p){return {p.episode,p.musicEpisode,p.position,p.musicPosition,p.part,p.music};}
E::Hand HandOf(const Packet& p){E::Hand h{};h.generation=p.handGeneration;h.bone=p.handBone;std::copy_n(p.handPose,7,h.pose);return h;}
void PutHand(Packet& p,const E::Hand& h){p.handGeneration=h.generation;p.handBone=h.bone;std::copy_n(h.pose,7,p.handPose);}
void PutState(Packet& p,const E::State& s){p.episode=s.episode;p.musicEpisode=s.musicEpisode;p.position=s.position;p.musicPosition=s.musicPosition;p.part=s.part;p.music=s.music;}
bool Active(const Packet& p){return p.part || p.music;}
bool SameState(const Packet& a,const Packet& b){
    if(a.part!=b.part || a.music!=b.music || a.episode!=b.episode || a.musicEpisode!=b.musicEpisode || a.playerNo!=b.playerNo || a.slot!=b.slot ||
       a.handGeneration!=b.handGeneration || a.handBone!=b.handBone)return false;
    for(int i=0;i<7;++i)if(std::abs(a.handPose[i]-b.handPose[i])>(i<3?.05f:.005f))return false;
    return true;
}
void Retire(int slot){auto playback=g_rows[slot].playback;g_rows[slot]={};g_seen[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;E::Stop(playback);}
void Reset(bool observation=true){
    ++g_epoch;const auto old=g_rows;g_rows={};g_owners={};g_seen={};g_rateAt={};g_rateCount={};
    g_local={};g_localPending=false;g_poll=g_localAt=0;g_cursor=0;
    if(observation)E::ResetObservation();for(auto row:old)E::Stop(row.playback);
}
void Scope(net::Session* session){
    if(session==g_session && g_world==ue_wrap::world_identity::Generation() && g_serial==net::session_serial::Current() &&
        (!session || (g_stops==session->StopCount() && g_role==session->role() && g_running==session->running() &&
                      (g_role==net::Role::Host || g_connected==session->connected()))))return;
    // Observation belongs to the live local pawn; joining a solo host must
    // retain the montage that started before its first client connected.
    const auto epoch=g_epoch;Reset(false);if(g_epoch!=epoch+1)return;
    g_session=session;g_world=ue_wrap::world_identity::Generation();g_serial=net::session_serial::Current();
    if(session){g_stops=session->StopCount();g_role=session->role();g_running=session->running();g_connected=session->connected();}
}
void Reconcile(){
    if(!Current())return;const auto epoch=g_epoch;
    for(int slot=0;slot<net::kMaxPeers;++slot){
        const auto now=Address(slot);if(Same(now,g_owners[slot]))continue;
        g_owners[slot]=now;Retire(slot);if(epoch!=g_epoch || !Current())return;
    }
}
bool Owned(const Row& row){return Current() && row.packet.slot<net::kMaxPeers && row.owner.person && Same(row.owner,Address(row.packet.slot));}
bool Still(int slot,const Row& row,uint64_t epoch){return epoch==g_epoch && Owned(row) && g_rows[slot].revision==row.revision;}
Packet Advance(const Row& row,uint64_t now){
    auto p=row.packet;const float age=float(now>=row.at?now-row.at:0)/1000.f+row.transit;
    if(p.part){const auto* part=E::PartAt(p.part);p.position+=age;
        if(part->loop)p.position=std::fmod(p.position,part->duration);
        else if(p.position>=part->duration){p.part=0;p.episode=0;p.position=0;PutHand(p,{});}
    }
    if(p.music)p.musicPosition+=age;
    return p;
}
void Store(Packet p,uint8_t slot,uint64_t now,float transit){
    auto& row=g_rows[slot];row.packet=p;row.at=now;row.transit=transit;row.owner=Address(slot);row.revision=++g_revision;
}
void Canonical(Packet p,uint64_t now,float transit=0){p.sequence=Next(g_canonical);Store(p,p.slot,now,transit);}
void* Body(int slot){
    auto& registry=players::Registry::Get();if(slot==registry.LocalPeerId() || registry.LifeOf(uint8_t(slot))!=players::PuppetLife::Alive)return nullptr;
    auto* puppet=registry.Puppet(uint8_t(slot));
    return puppet && puppet->CanPlayEmote() ? puppet->actor() : nullptr;
}
float Transit(uint8_t sender){const int rtt=g_session->rttMsForSlot(sender);return rtt>=0 && rtt<=10000?float(rtt)/2000.f:0;}
}

void Install(net::Session* session){Scope(session);}
void Receive(const Packet& input,uint8_t sender){
    const Packet p=input;Scope(g_session);if(!Current() || p.slot>=net::kMaxPeers || !p.playerNo || !p.sequence || p.reserved ||
        p.handReserved[0] || p.handReserved[1] || p.handReserved[2] || !E::Valid(StateOf(p)) || !E::Valid(HandOf(p)) || (!p.part && p.handGeneration))return;
    Reconcile();if(!Current() || p.playerNo!=roster_ledger::Get(p.slot).playerNo)return;
    const auto now=active_drive::NowMs();
    if(g_role==net::Role::Host){
        if(!sender || sender>=net::kMaxPeers || sender!=p.slot || !g_session->IsSlotConnected(sender) || !g_session->IsSlotWorldReady(sender) ||
            (g_seen[sender] && !After(p.sequence,g_seen[sender])))return;
        if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rateCount[sender]=0;}
        if(g_rateCount[sender]>=24)return;++g_rateCount[sender];g_seen[sender]=p.sequence;
        Canonical(p,now,Transit(sender));return;
    }
    if(sender!=0 || p.slot==players::Registry::Get().LocalPeerId() ||
        (g_seen[p.slot] && !After(p.sequence,g_seen[p.slot])))return;
    g_seen[p.slot]=p.sequence;Store(p,p.slot,now,Transit(0));
}
void Tick(){
    if(g_ticking)return;g_ticking=true;struct Exit {~Exit(){g_ticking=false;}} exit;
    Scope(g_session);if(!Current())return;Reconcile();if(!Current())return;
    const auto now=active_drive::NowMs(),epoch=g_epoch;
    if(now<g_poll)return;g_poll=now+50;
    const uint8_t me=players::Registry::Get().LocalPeerId();
    if(me<net::kMaxPeers && roster_ledger::Get(me).playerNo){
        E::Observe(players::Registry::Get().Local());if(epoch!=g_epoch || !Current())return;
        const auto state=E::Sample();if(epoch!=g_epoch || !Current())return;
        Packet p{};PutState(p,state);p.slot=me;p.playerNo=roster_ledger::Get(me).playerNo;
        if(state.part){
            PutHand(p,E::CaptureHand(players::Registry::Get().Local(),hand_item::LocalHandActor(),hand_item::Generation(me)));
            if(epoch!=g_epoch || !Current())return;
        }
        if(!SameState(p,g_local) || (Active(p) && now-g_localAt>=1000)){
            p.sequence=Next(g_sequence);g_local=p;g_localAt=now;g_localPending=true;
        }
        if(g_localPending){
            const auto outgoing=g_local;const auto owner=Address(me);
            if(g_role==net::Role::Host){Canonical(outgoing,now);g_localPending=false;}
            else {
                const bool sent=g_session->SendReliableToSlot(0,net::ReliableKind::EmoteState,&outgoing,sizeof(outgoing));
                if(epoch!=g_epoch || !Current())return;
                if(sent && Same(owner,Address(me)) && g_local.sequence==outgoing.sequence)g_localPending=false;
            }
        }
    }
    // A silent/disconnected producer cannot leave a looping montage or song forever.
    for(int slot=0;slot<net::kMaxPeers;++slot){
        auto row=g_rows[slot];if(!row.packet.sequence || !Owned(row))continue;
        if(Active(row.packet) && now-row.at>3500){
            auto stop=row.packet;PutState(stop,{});PutHand(stop,{});
            if(g_role==net::Role::Host)Canonical(stop,now);else {row.packet=stop;Store(stop,uint8_t(slot),now,0);}
            row=g_rows[slot];
        }
        if(slot!=me){
            auto playback=row.playback;void* body=Body(slot);
            const auto advanced=Advance(row,now);
            if(body && Active(advanced)){
                const auto desired=StateOf(advanced);
                E::Apply(body,desired,playback,[&,slot]{return Still(slot,row,epoch) && Body(slot)==body;});
            }else E::Stop(playback);
            if(!Still(slot,row,epoch)){E::Stop(playback);if(epoch!=g_epoch || !Current())return;continue;}
            g_rows[slot].playback=playback;
        }
    }
    if(g_role!=net::Role::Host)return;
    // Rotate the source and recipient together: backpressure cannot starve slot 31.
    unsigned budget=64;
    for(size_t walked=0;walked<net::kMaxPeers*net::kMaxPeers && budget;++walked){
        const auto index=g_cursor++%(net::kMaxPeers*net::kMaxPeers);
        const int source=int(index/net::kMaxPeers),target=int(index%net::kMaxPeers);
        if(!target || source==target)continue;
        const auto row=g_rows[source];if(!row.packet.sequence || !Owned(row))continue;
        const bool ready=g_session->IsSlotConnected(target) && g_session->IsSlotWorldReady(target);
        if(!ready){g_rows[source].deliveries[target].ready=false;continue;}
        const auto recipient=Address(target);if(!recipient.person)continue;
        const auto delivery=row.deliveries[target];
        if(delivery.ready && Same(delivery.recipient,recipient) && delivery.sequence==row.packet.sequence && now-delivery.at<1000)continue;
        const auto outgoing=Advance(row,now);--budget;
        const bool sent=g_session->SendReliableToSlot(target,net::ReliableKind::EmoteState,&outgoing,sizeof(outgoing));
        if(epoch!=g_epoch || !Current())return;
        if(sent && Still(source,row,epoch) && Same(recipient,Address(target)))g_rows[source].deliveries[target]={outgoing.sequence,now,recipient,true};
    }
}
void OnPeerLeft(int slot){if(slot<0 || slot>=net::kMaxPeers)return;g_owners[slot]={};Retire(slot);}
void OnDisconnect(){
    const bool soloHost=g_session && g_session->running() && g_session->role()==net::Role::Host;
    g_session=nullptr;Reset(!soloHost);
}
HandDrive DriveHand(void* pawn,void* mirror){
    const auto slot=players::Registry::Get().PeerIdOfActor(pawn);
    if(slot>=net::kMaxPeers || !Current() || slot==players::Registry::Get().LocalPeerId())return HandDrive::Fallback;
    const auto row=g_rows[slot];const auto epoch=g_epoch;const auto p=Advance(row,active_drive::NowMs());
    if(!p.part || !p.handGeneration || !Owned(row) || !row.playback.actor.Is(pawn) ||
       row.playback.state.part!=p.part || row.playback.state.episode!=p.episode ||
       p.handGeneration!=hand_item::MirrorGeneration(slot) || mirror!=hand_item::MirrorActorForSlot(slot))return HandDrive::Fallback;
    const auto current=[&]{return Still(slot,row,epoch) && Body(slot)==pawn && row.playback.actor.Get() &&
        mirror==hand_item::MirrorActorForSlot(slot) && p.handGeneration==hand_item::MirrorGeneration(slot);};
    const bool applied=E::PlaceHand(pawn,mirror,HandOf(p),current);
    if(!current())return HandDrive::Retired;
    return applied?HandDrive::Applied:HandDrive::Fallback;
}
}
