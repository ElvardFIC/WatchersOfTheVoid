#include "coop/player/blood_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "ue_wrap/actors/player_blood.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <deque>

namespace coop::blood_sync {
namespace {
namespace FX = ue_wrap::player_blood;
namespace SG = ue_wrap::script_gate;
using Packet = net::PlayerBloodPayload;
UE_FACT_FN(kDrop, "blood_sync", L"bloodDecalMaster_C", L"addParticle", Declared);
constexpr int kTag = 0x424c4f44;
struct Pending { Packet packet{}; uint64_t expires = 0, ownerEpoch = 0; };
struct Peer {
    std::deque<Pending> queue;
    uint32_t generation = 0, seen = 0;
    uint16_t person = 0;
    uint64_t rateAt = 0;
    unsigned rate = 0;
};
net::Session* g_session = nullptr;
std::array<Peer, net::kMaxPeers> g_peers;
std::array<uint64_t, net::kMaxPeers> g_peerEpoch{};
std::deque<Pending> g_in;
uint32_t g_world = 0, g_serial = 0, g_sequence = 0;
uint64_t g_epoch = 0;
bool g_ticking = false;
uint64_t Now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool Current(uint64_t epoch) {
    return g_session && g_session->running() && g_session->connected() && epoch == g_epoch &&
        g_world == ue_wrap::world_identity::Generation() && g_serial == net::session_serial::Current();
}
bool Host() { return g_session->role() == net::Role::Host; }
uint8_t Local() { return players::Registry::Get().LocalPeerId(); }
void Clear() {
    ++g_epoch; g_in.clear(); g_peers = {};
    g_world = ue_wrap::world_identity::Generation(); g_serial = net::session_serial::Current();
    // Sequence numbers survive local reconnects; transport generations isolate
    // occupancy. Reusing an old sequence after a world change could hide a drop.
}
void Refresh() {
    if (g_world != ue_wrap::world_identity::Generation() || g_serial != net::session_serial::Current()) Clear();
    if (!Current(g_epoch)) return;
    for (int slot = 0; slot < net::kMaxPeers; ++slot) {
        const auto person = roster_ledger::Get(slot).playerNo;
        const auto generation = g_session->peerGenerationForSlot(slot);
        auto& peer = g_peers[slot];
        if (peer.person != person || peer.generation != generation) {
            OnPeerLeft(slot);
            peer = {}; peer.person = person; peer.generation = generation;
        }
    }
}
FX::Drop Decode(const Packet& p) { return {{p.x,p.y,p.z},{p.vx,p.vy,p.vz},p.size}; }
bool Valid(const Packet& p) {
    return p.slot < net::kMaxPeers && p.playerNo && p.sequence && !p.reserved && FX::Valid(Decode(p));
}
bool Owner(const Packet& p) { return roster_ledger::Get(p.slot).playerNo == p.playerNo; }
void Queue(int slot, const Packet& p) {
    if (g_peers[slot].queue.size() < 32) g_peers[slot].queue.push_back({p,Now()+2000,g_peerEpoch[p.slot]});
}
void Relay(const Packet& p) {
    for (int slot = 1; slot < net::kMaxPeers; ++slot)
        if (slot != p.slot && g_session->HasPeerConn(slot) && g_session->IsSlotWorldReady(slot)) Queue(slot,p);
}
void Observe(const SG::Call& call) {
    if (!ue_wrap::game_thread::IsGameThread()) return;
    Refresh(); const auto epoch = g_epoch;
    if (!Current(epoch) || Local() >= net::kMaxPeers) return;
    FX::Drop drop{};
    if (!FX::ReadDrop(call,drop) || !Current(epoch)) return;
    Packet p{}; p.slot=Local(); p.playerNo=roster_ledger::Get(p.slot).playerNo;
    if (!p.playerNo) return;
    if (!++g_sequence) ++g_sequence;
    p.sequence=g_sequence; p.x=drop.location.X; p.y=drop.location.Y; p.z=drop.location.Z;
    p.vx=drop.velocity.X; p.vy=drop.velocity.Y; p.vz=drop.velocity.Z; p.size=drop.size;
    // The injured owner already ran the original native call. Never echo it.
    if (Host()) Relay(p); else Queue(0,p);
}
}
void Install(net::Session* session) {
    if (g_session != session) { g_session=session; Clear(); }
    SG::WatchName(kDrop,kTag,nullptr,&Observe);
}
void Receive(const Packet& p, uint8_t sender) {
    if (!ue_wrap::game_thread::IsGameThread()) return;
    Refresh(); const auto epoch=g_epoch;
    if (!Current(epoch) || !Valid(p) || !Owner(p) || p.slot==Local()) return;
    if (Host()) {
        if (!sender || sender>=net::kMaxPeers || sender!=p.slot || !g_session->HasPeerConn(sender) ||
            !g_session->IsSlotWorldReady(sender)) return;
    } else if (sender!=0) return;
    auto& peer=g_peers[p.slot];
    if (peer.seen && int32_t(p.sequence-peer.seen)<=0) return;
    if (Host()) {
        const auto generation=peer.generation;
        const auto ownerEpoch=g_peerEpoch[p.slot];
        // Native drop origins are the midpoint of the owner's 200cm downward
        // camera trace. Never admit a remote request to paint another location.
        const ue_wrap::FVector point{p.x,p.y,p.z};
        const bool allowed=bool(element::IntentTarget::ForClientIntent(*g_session,sender,400.f).AuthorizeSegment(point,point));
        if (!Current(epoch) || !Owner(p) || g_session->peerGenerationForSlot(sender)!=generation ||
            g_peerEpoch[p.slot]!=ownerEpoch || !allowed || (peer.seen && int32_t(p.sequence-peer.seen)<=0)) return;
        const auto now=Now();
        if (now-peer.rateAt>=1000) { peer.rateAt=now; peer.rate=0; }
        if (peer.rate>=8) return;
        ++peer.rate;
    }
    peer.seen=p.sequence;
    if (g_in.size()<128) g_in.push_back({p,Now()+2000,g_peerEpoch[p.slot]});
    if (Host()) Relay(p);
}
void Tick() {
    if (g_ticking) return;
    g_ticking=true; struct Guard { ~Guard(){g_ticking=false;} } guard;
    Refresh(); const auto epoch=g_epoch;
    if (!Current(epoch)) return;
    // Work owns a copy across native/send callbacks, which may disconnect or
    // replace the world/session. No queue reference survives such a callback.
    auto incoming=std::move(g_in); g_in.clear();
    for (const auto& row:incoming) {
        if (!Current(epoch)) return;
        const auto current=[&]{return Current(epoch) && Owner(row.packet) && g_peerEpoch[row.packet.slot]==row.ownerEpoch;};
        if (Now()>=row.expires || !current()) continue;
        if (!FX::Replay(Decode(row.packet),current) && current()) g_in.push_back(row);
    }
    for (int slot=0; slot<net::kMaxPeers; ++slot) {
        if (!Current(epoch)) return;
        if ((!Host() && slot!=0) || slot==Local()) continue;
        auto& peer=g_peers[slot]; const auto generation=peer.generation; const auto person=peer.person;
        const auto recipientEpoch=g_peerEpoch[slot];
        for (int budget=0; !peer.queue.empty() && budget<8; ++budget) {
            const auto row=peer.queue.front();
            if (Now()>=row.expires || !Owner(row.packet) || g_peerEpoch[row.packet.slot]!=row.ownerEpoch || !g_session->HasPeerConn(slot) ||
                !g_session->IsSlotWorldReady(slot)) { peer.queue.pop_front(); continue; }
            const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::PlayerBlood,&row.packet,sizeof(row.packet));
            if (!Current(epoch)) return;
            if (g_peerEpoch[slot]!=recipientEpoch || g_session->peerGenerationForSlot(slot)!=generation || roster_ledger::Get(slot).playerNo!=person) {
                peer={}; break;
            }
            if (!sent) break;
            if (!peer.queue.empty() && peer.queue.front().packet.slot==row.packet.slot &&
                peer.queue.front().packet.sequence==row.packet.sequence) peer.queue.pop_front();
        }
    }
}
void OnPeerLeft(int slot) {
    if (slot<0 || slot>=net::kMaxPeers) return;
    ++g_peerEpoch[slot];
    g_peers[slot]={};
    std::erase_if(g_in,[=](const Pending& row){return row.packet.slot==slot;});
    for (auto& peer:g_peers) std::erase_if(peer.queue,[=](const Pending& row){return row.packet.slot==slot;});
}
void OnDisconnect() { g_session=nullptr; Clear(); }
}
