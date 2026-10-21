// coop/world/balance_sync.cpp -- see coop/world/balance_sync.h.

#include "coop/world/balance_sync.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/world/economy.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"

#include <atomic>
#include <array>
#include <cstdint>

namespace coop::balance_sync {
namespace {

namespace GT = ue_wrap::game_thread;
namespace E  = ue_wrap::economy;

std::atomic<coop::net::Session*> g_session{nullptr};

struct Delivery {
    uint32_t occupant = 0;
    int32_t total = 0;
    bool sent = false;
    uint64_t requested = 0, delivered = 0;
};
std::array<Delivery, net::kMaxPeers> g_delivery{};
// The nonzero world generation and signed value travel together. A native HUD
// callback can publish a newer value while Tick is applying the previous one.
std::atomic<uint64_t> g_pending{0};
std::atomic<uint64_t> g_epoch{0};
std::atomic<uint32_t> g_serial{0};
uint32_t g_world = 0;
bool g_ticking = false;

bool Current(net::Session* s, uint64_t epoch, uint32_t world) {
    return g_epoch.load() == epoch && g_session.load() == s && s && s->connected() &&
        g_serial.load() == net::session_serial::Current() &&
        world == ue_wrap::world_identity::Generation();
}

void Deliver(net::Session* s, int slot, int32_t points, uint64_t epoch, uint32_t world) {
    auto& row = g_delivery[slot];
    const auto occupant = s->peerGenerationForSlot(slot);
    if (row.occupant != occupant) { row = {}; row.occupant = occupant; }
    if (!s->IsSlotReady(slot)) { row.sent = false; return; }
    if (row.sent && row.total == points && row.requested == row.delivered) return;
    const auto request = row.requested;
    const net::BalancePayload p{points};
    const bool sent = s->SendReliableToSlot(slot, net::ReliableKind::BalanceSync, &p, sizeof(p));
    if (!Current(s, epoch, world) || s->peerGenerationForSlot(slot) != occupant) return;
    if (!sent) return; // Every recipient retains its own delivery debt.
    row.sent = true; row.total = points; row.delivered = request;
    UE_LOGI("balance_sync: sent Points=%d to slot %d", points, slot);
}

}  // namespace

void SetSession(coop::net::Session* session) {
    ++g_epoch;
    g_delivery = {};
    g_pending.store(0);
    g_serial.store(net::session_serial::Current());
    g_world = ue_wrap::world_identity::Generation();
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || g_ticking || g_serial.load() != net::session_serial::Current()) return;
    struct Guard { Guard(){g_ticking=true;} ~Guard(){g_ticking=false;} } guard;
    const auto epoch = g_epoch.load();
    const auto world = ue_wrap::world_identity::Generation();
    if (g_world != world) { g_delivery = {}; g_world = world; }
    if (s->role() == coop::net::Role::Host) {
        int32_t points = 0;
        if (!E::ReadPoints(&points) || !Current(s, epoch, world)) return;
        for (int slot = 1; slot < net::kMaxPeers; ++slot) {
            Deliver(s, slot, points, epoch, world);
            if (!Current(s, epoch, world)) return;
        }
    } else {
        auto pending = g_pending.load(std::memory_order_acquire);
        if (!pending) return;
        if (uint32_t(pending >> 32) != world) {
            g_pending.compare_exchange_strong(pending, 0, std::memory_order_acq_rel);
            return;
        }
        const int32_t total = static_cast<int32_t>(static_cast<uint32_t>(pending));
        if (!E::WritePoints(total) || !Current(s, epoch, world)) return;
        if (!E::RefreshPointsHud() || !Current(s, epoch, world)) return;
        // Do not acknowledge a newer balance published during the native repaint.
        g_pending.compare_exchange_strong(pending, 0, std::memory_order_acq_rel);
    }
}

void SendCurrentToSlot(int slot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host || slot <= 0 || slot >= net::kMaxPeers ||
        g_serial.load() != net::session_serial::Current()) return;
    auto& row = g_delivery[slot];
    const auto occupant = s->peerGenerationForSlot(slot);
    if (row.occupant != occupant) { row = {}; row.occupant = occupant; }
    ++row.requested;
    if (g_ticking || !s->connected()) return;
    struct Guard { Guard(){g_ticking=true;} ~Guard(){g_ticking=false;} } guard;
    const auto epoch = g_epoch.load();
    const auto world = ue_wrap::world_identity::Generation();
    int32_t points = 0;
    if (!E::ReadPoints(&points) || !Current(s, epoch, world)) return;
    Deliver(s, slot, points, epoch, world);
}

void ApplyFromHost(int32_t total) {
    // From the host alone: the host's receive edge refuses a client's copy (client_authorship.h).
    // Stash the latest; Tick (game thread) writes it + RETRIES until the saveSlot
    // resolves (a connect-edge BalanceSync can arrive before the gamemode is ready).
    auto* s = g_session.load();
    if (!s || !s->connected() || s->role() != net::Role::Client ||
        g_serial.load() != net::session_serial::Current()) return;
    const auto world = ue_wrap::world_identity::Generation();
    if (world) g_pending.store((uint64_t(world) << 32) | static_cast<uint32_t>(total), std::memory_order_release);
}

void CreditLocal(int32_t amount) {
    auto* s = g_session.load(std::memory_order_acquire);
    const bool host      = s && s->role() == coop::net::Role::Host;
    const bool connected = s && s->connected();
    if (host || !connected) {
        // Host OR solo (no live session): apply locally. If hosting, the next TickHost
        // poll sees the new Points and broadcasts it to all clients.
        const auto epoch = g_epoch.load();
        const auto serial = net::session_serial::Current();
        GT::Post([amount, s, epoch, serial] {
            if (g_epoch.load() != epoch || g_session.load() != s ||
                net::session_serial::Current() != serial) return;
            E::AddPoints(amount);
        });
        UE_LOGI("balance_sync: local credit %+d (%s)", amount,
                host ? "host -> will broadcast" : "solo");
        return;
    }
    // A connected CLIENT cannot AUTHOR the shared balance, and that half is deliberate: the lane
    // that let it -- a delta the host applied through AddPoints with no value bound -- was retired
    // whole rather than clamped, because the balance is host-authoritative everywhere. Client-side
    // earning still reaches it, but only where an intent lane forwards the action and the host
    // re-runs the game's own verb on the authoritative instance, as coingun_collect does for a coin
    // pickup. A direct credit from here is not that, so it is refused: defence in depth, since
    // coop::dev_gate already refuses the only caller on a client.
    UE_LOGW("balance_sync: local credit %+d REFUSED -- a client cannot author the shared "
            "balance; earning reaches it through an intent lane the host re-runs", amount);
}

void OnDisconnect() {
    ++g_epoch;
    g_session.store(nullptr);
    g_serial.store(0);
    g_delivery = {};
    g_pending.store(0);
}

}  // namespace coop::balance_sync
