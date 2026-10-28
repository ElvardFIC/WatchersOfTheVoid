#include "coop/interactables/laptop_ui_sync.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/items/save_record_wire.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/devices/laptop_ui.h"
#include "ue_wrap/engine/world_identity.h"
#include <array>
#include <chrono>

namespace coop::laptop_ui_sync {
namespace {
namespace UI = ue_wrap::laptop_ui;
namespace W = save_record_wire;
net::Session* g_session = nullptr;
blob_chunks::Assembler g_assembler;
UI::State g_state, g_sent;
bool g_have = false, g_apply = false, g_haveSent = false, g_busy = false;
int g_operatorFailures = 0;
constexpr int kOperatorRepaints = 3;
uint8_t g_owner = 0xff;
uint32_t g_lease = 0, g_revision = 0, g_command = 0, g_blob = 0, g_world = 0;
uint64_t g_epoch = 0, g_poll = 0;
uint64_t g_claimRevision = 0;
std::array<uint32_t, players::kMaxPeers> g_seen{};
std::array<bool, players::kMaxPeers> g_dirty{};
std::array<uint64_t, players::kMaxPeers> g_peerEpoch{};
uint64_t Now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool Newer(uint32_t a, uint32_t b) { return a && (!b || int32_t(a - b) > 0); }
uint32_t Next(uint32_t& value) { if (!++value) ++value; return value; }
bool Active() { return g_session && g_session->running() && g_session->connected(); }
bool Host() { return Active() && g_session->role() == net::Role::Host; }
void Clear() {
    g_assembler.Clear(); g_state = {}; g_sent = {};
    g_have = g_apply = g_haveSent = false; g_operatorFailures = 0; g_owner = 0xff;
    g_lease = g_revision = g_command = g_blob = 0; g_seen.fill(0); g_dirty.fill(false);
    g_poll = 0; g_claimRevision = 0; ++g_epoch; g_world = ue_wrap::world_identity::Generation();
}
void World() { if (g_world != ue_wrap::world_identity::Generation()) Clear(); }
void Dirty() {
    Next(g_revision);
    for (int slot = 1; slot < players::kMaxPeers; ++slot)
        if (g_session->HasPeerConn(slot) && g_session->IsSlotWorldReady(slot)) g_dirty[slot] = true;
}
void Lease() {
    if (!Host()) return;
    const auto owner = device_occupancy::HolderOf(L"laptop");
    const auto revision = device_occupancy::LaptopClaimRevision();
    if (g_lease && owner == g_owner && revision == g_claimRevision) return;
    g_claimRevision = revision;
    g_owner = owner; Next(g_lease); g_seen.fill(0); g_haveSent = false;
    // A new operator starts with the preceding operator's cart and page.
    g_apply = g_have; g_operatorFailures = 0; Dirty();
}
std::vector<uint8_t> Encode(bool canonical, uint32_t serial, const UI::State& state) {
    std::vector<uint8_t> b;
    W::AppU8(b, canonical ? 1 : 0); W::AppU32(b, g_lease);
    W::AppU32(b, serial); W::AppU8(b, g_owner);
    for (auto tab : state.tabs) W::AppI32(b, tab);
    W::AppU32(b, uint32_t(state.cart.size()));
    for (const auto& row : state.cart) W::AppWStr(b, row);
    return b;
}
bool Decode(const std::vector<uint8_t>& b, bool& canonical, uint32_t& lease,
            uint32_t& serial, uint8_t& owner, UI::State& s) {
    if (b.size() > 16 * 1024) return false;
    size_t o = 0; uint8_t op = 0; uint32_t count = 0;
    if (!W::RdU8(b, o, op) || op > 1 || !W::RdU32(b, o, lease) || !lease ||
        !W::RdU32(b, o, serial) || !serial || !W::RdU8(b, o, owner) ||
        (owner != 0xff && owner >= players::kMaxPeers)) return false;
    canonical = op == 1;
    for (auto& tab : s.tabs) if (!W::RdI32(b, o, tab)) return false;
    if (!W::RdU32(b, o, count) || count > 50 || !W::Feasible(count * 4, b, o)) return false;
    for (uint32_t i = 0; i < count; ++i) {
        std::wstring row;
        if (!W::RdWStr(b, o, row) || row.empty() || row.size() > 128) return false;
        s.cart.push_back(std::move(row));
    }
    return o == b.size();
}
void Receive(const std::vector<uint8_t>& b, int slot) {
    UI::State state; bool canonical = false; uint32_t lease = 0, serial = 0; uint8_t owner = 0xff;
    if (!Decode(b, canonical, lease, serial, owner, state)) return;
    const auto epoch = g_epoch;
    if (Host()) {
        Lease();
        if (canonical || slot <= 0 || owner != slot || g_owner != slot || lease != g_lease ||
            !Newer(serial, g_seen[slot]) || !UI::Valid(state) || epoch != g_epoch) return;
        Lease();
        if (lease != g_lease || g_owner != slot || !Newer(serial, g_seen[slot])) return;
        g_seen[slot] = serial;
        g_state = std::move(state); g_have = g_apply = true; g_operatorFailures = 0; Dirty();
    } else {
        if (!canonical || slot != 0 || !Newer(serial, g_revision)) return;
        // Catalog may still be loading on a joining client. Keep the bounded
        // snapshot and retry Apply rather than consuming and losing the seed.
        const bool newLease = lease != g_lease;
        g_lease = lease; g_revision = serial; g_owner = owner;
        g_state = std::move(state); g_have = true; g_operatorFailures = 0;
        const bool local = owner == players::Registry::Get().LocalPeerId() && device_occupancy::LocalHolds(L"laptop");
        if (newLease || !local) { g_apply = true; g_operatorFailures = 0; }
        if (newLease) g_haveSent = false;
    }
}
}
void Install(net::Session* session) {
    if (g_session != session) { g_session = session; Clear(); }
}
void Tick() {
    if (!ue_wrap::game_thread::IsGameThread() || !Active() || g_busy) return;
    g_busy = true; struct Busy { ~Busy() { g_busy = false; } } busy;
    World(); Lease();
    const auto epoch = g_epoch, now = Now();
    if (now < g_poll) return;
    g_poll = now + 150;
    g_assembler.Sweep(std::chrono::steady_clock::now(), std::chrono::seconds(5));
    if (g_apply) {
        const auto state = g_state;
        const auto revision = g_revision;
        const auto lease = g_lease;
        const bool applied = UI::Apply(state);
        if (epoch != g_epoch || !Active()) return;
        Lease();
        if (revision != g_revision || lease != g_lease) return;
        if (applied) g_apply = false;
        else if (g_lease && g_owner == players::Registry::Get().LocalPeerId() &&
                 device_occupancy::LocalHolds(L"laptop")) {
            // A readable operator must regain control after a persistently
            // refused inherited repaint. Loading/missing widgets keep the seed.
            UI::State readable;
            const bool ready = UI::Read(readable);
            if (epoch != g_epoch || !Active()) return;
            Lease();
            if (revision != g_revision || lease != g_lease) return;
            if (g_owner != players::Registry::Get().LocalPeerId() || !device_occupancy::LocalHolds(L"laptop")) {
                g_operatorFailures = 0; return;
            }
            if (!ready) g_operatorFailures = 0;
            else if (++g_operatorFailures >= kOperatorRepaints) {
                UE_LOGW("laptop_ui_sync: inherited repaint refused %d times on a readable UI; operator resumes",
                        g_operatorFailures);
                g_apply = false;
            }
        } else g_operatorFailures = 0;
    }
    if (!g_apply) g_operatorFailures = 0;
    const auto local = players::Registry::Get().LocalPeerId();
    const auto readingLease = g_lease;
    const bool author = !g_apply && g_lease && g_owner == local && device_occupancy::LocalHolds(L"laptop");
    UI::State current;
    if ((author || (Host() && !g_have)) && UI::Read(current)) {
        if (epoch != g_epoch || !Active()) return;
        Lease();
        if (readingLease != g_lease) return;
        if (Host()) {
            if (!g_have || current != g_state) { g_state = current; g_have = true; Dirty(); }
        } else if (!g_haveSent || current != g_sent) {
            const auto bytes = Encode(false, Next(g_command), current);
            const auto lease = g_lease;
            const bool sent = blob_chunks::SendBlobToSlot(g_session, 0, net::ReliableKind::LaptopUI, Next(g_blob), bytes);
            if (epoch != g_epoch || !Active()) return;
            if (lease == g_lease && sent) { g_sent = std::move(current); g_haveSent = true; }
        }
    }
    if (!Host() || !g_have) return;
    const auto revision = g_revision;
    const auto bytes = Encode(true, revision, g_state);
    for (int slot = 1; slot < players::kMaxPeers; ++slot) {
        if (!g_dirty[slot] || !g_session->HasPeerConn(slot) || !g_session->IsSlotWorldReady(slot)) continue;
        const auto peerEpoch = g_peerEpoch[slot];
        const bool sent = blob_chunks::SendBlobToSlot(g_session, slot, net::ReliableKind::LaptopUI, Next(g_blob), bytes);
        if (epoch != g_epoch || !Active()) return;
        if (revision == g_revision && peerEpoch == g_peerEpoch[slot] && sent) g_dirty[slot] = false;
    }
}
void OnChunk(const net::BlobChunkPayload& chunk, int slot) {
    if (!ue_wrap::game_thread::IsGameThread() || !Active() || slot < 0 || slot >= players::kMaxPeers) return;
    World();
    if (Host()) {
        if (slot == 0 || !g_session->HasPeerConn(slot) || !g_session->IsSlotWorldReady(slot)) return;
        Lease();
        if (g_owner != slot) { g_assembler.ClearSlot(uint8_t(slot)); return; }
    } else if (slot != 0) return;
    if (chunk.chunks > (16 * 1024 + 217) / 218) return;
    std::vector<uint8_t> bytes;
    if (g_assembler.OnChunk(chunk, uint8_t(slot), bytes)) Receive(bytes, slot);
}
bool BeforeLocalRelease() {
    if (!Active() || !g_lease || g_owner != players::Registry::Get().LocalPeerId() || g_apply) return true;
    // This runs before the occupancy lane erases the claim. Both records use
    // Normal, so the host receives the final edit before the following release.
    g_poll = 0; Tick();
    if (!Active() || Host()) return true;
    UI::State current;
    if (!UI::Read(current)) return true; // destroyed widget/world: nothing left to capture
    return g_haveSent && current == g_sent;
}
void QueueConnectBroadcastForSlot(int slot) {
    if (Host() && slot > 0 && slot < players::kMaxPeers) g_dirty[slot] = true;
}
void OnPeerLeft(int slot) {
    if (slot <= 0 || slot >= players::kMaxPeers) return;
    g_assembler.ClearSlot(uint8_t(slot)); g_seen[slot] = 0; g_dirty[slot] = false; ++g_peerEpoch[slot];
    if (Host() && g_owner == slot) { g_owner = 0xff; Next(g_lease); g_apply = g_have; Dirty(); }
    g_dirty[slot] = false;
}
void OnDisconnect() { g_session = nullptr; Clear(); }
}
