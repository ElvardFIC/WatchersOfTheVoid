#include "event_fire_delivery.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/world_identity.h"
#include <array>
#include <deque>

namespace coop::event_fire_sync::delivery {
namespace {
struct Item { net::EventFirePayload payload{}; uint64_t id = 0; };
struct Recipient { std::deque<Item> queue; uint32_t generation = 0; };
std::array<Recipient, net::kMaxPeers> g_peers;
net::Session* g_session = nullptr;
uint32_t g_world = 0, g_serial = 0;
uint64_t g_epoch = 0, g_id = 0;
bool g_flushing = false;

bool Host() {
    return g_session && g_session->running() && g_session->connected() && g_session->role() == net::Role::Host;
}
void Reset() {
    for (auto& peer : g_peers) peer = {};
    ++g_epoch;
    g_world = ue_wrap::world_identity::Generation();
    g_serial = net::session_serial::Current();
}
void Refresh() {
    if (!Host() || g_world != ue_wrap::world_identity::Generation() || g_serial != net::session_serial::Current()) Reset();
    for (int slot = 1; slot < net::kMaxPeers; ++slot) {
        auto& peer = g_peers[slot];
        const uint32_t generation = Host() && g_session->HasPeerConn(slot) ? g_session->peerGenerationForSlot(slot) : 0;
        if (peer.generation != generation) { peer = {}; peer.generation = generation; }
    }
}
void Flush() {
    if (g_flushing) return;
    g_flushing = true;
    struct Scope { ~Scope() { g_flushing = false; } } scope;
    Refresh();
    const auto epoch = g_epoch;
    for (int slot = 1; Host() && slot < net::kMaxPeers; ++slot) {
        auto& peer = g_peers[slot];
        const auto generation = peer.generation;
        for (int budget = 0; !peer.queue.empty() && budget < 8; ++budget) {
            const Item item = peer.queue.front();
            // EventFire is admitted during world transfer too: loading clients
            // retain the native replay in their own bounded FIFO.
            const bool sent = g_session->SendReliableToSlot(slot, net::ReliableKind::EventFire,
                &item.payload, sizeof(item.payload));
            if (epoch != g_epoch || !Host()) return;
            if (g_world != ue_wrap::world_identity::Generation() || g_serial != net::session_serial::Current()) {
                Reset(); return;
            }
            if (generation != g_session->peerGenerationForSlot(slot) || !g_session->HasPeerConn(slot)) {
                peer = {}; break;
            }
            if (!sent) break;
            if (!peer.queue.empty() && peer.queue.front().id == item.id) peer.queue.pop_front();
        }
    }
}
}
void Install(net::Session* session) {
    if (g_session != session) { g_session = session; Reset(); }
    Flush();
}
void Publish(const net::EventFirePayload& payload) {
    Refresh();
    if (!Host()) return;
    const Item item{payload, ++g_id};
    for (int slot = 1; slot < net::kMaxPeers; ++slot) {
        auto& peer = g_peers[slot];
        if (!g_session->HasPeerConn(slot)) continue;
        if (peer.queue.size() >= 512) {
            UE_LOGE("event_fire: peer %d pending fire queue exhausted; '%.*s' was not queued", slot,
                int(sizeof(payload.name)), payload.name);
            continue;
        }
        peer.queue.push_back(item);
    }
    Flush();
}
void Clear() { g_session = nullptr; Reset(); }
}
