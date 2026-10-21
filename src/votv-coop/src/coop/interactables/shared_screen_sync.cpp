#include "coop/interactables/shared_screen_sync.h"
#include "coop/interactables/shared_screen_wire.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/engine/world_identity.h"
#include <array>
#include <chrono>

namespace coop::shared_screen_sync {
namespace {
namespace UI = ue_wrap::shared_screen;
namespace W = shared_screen_wire;
net::Session* g_session = nullptr;
blob_chunks::Assembler g_assembler;
std::vector<uint8_t> g_canonical, g_sent;
std::vector<UI::Row> g_target;
std::array<bool, net::kMaxPeers> g_owed{};
std::array<uint32_t, net::kMaxPeers> g_seen{};
std::array<uint64_t, net::kMaxPeers> g_peerEpoch{};
uint32_t g_sequence = 0, g_lease = 0, g_revision = 0, g_command = 0, g_world = 0;
uint32_t g_serial = 0;
uint64_t g_epoch = 0, g_claimRevision = 0;
uint8_t g_holder = 0xff;
std::chrono::steady_clock::time_point g_next{};
bool g_apply = false, g_busy = false;
bool Active() { return g_session && g_session->running() && g_session->connected(); }
bool Host() { return Active() && g_session->role() == net::Role::Host; }
uint32_t Next(uint32_t& value) { if (!++value) ++value; return value; }
bool Newer(uint32_t a, uint32_t b) { return a && (!b || int32_t(a - b) > 0); }
void Clear() {
    const auto serial = net::session_serial::Current();
    if (g_serial != serial) {
        g_sequence = g_lease = g_revision = g_command = 0;
        g_serial = serial;
    }
    g_assembler.Clear(); g_canonical.clear(); g_sent.clear(); g_target.clear();
    g_owed.fill(false); g_seen.fill(0); g_peerEpoch.fill(0);
    g_holder = 0xff; g_apply = false; g_next = {}; g_claimRevision = 0;
    // Keep lease/revision/blob counters monotonic through travel so old-world
    // packets cannot acquire the next world's lease by reusing its number.
    ++g_epoch; g_world = ue_wrap::world_identity::Generation();
    if (Host()) { Next(g_lease); Next(g_revision); }
}
void World() {
    if (g_world != ue_wrap::world_identity::Generation() || g_serial != net::session_serial::Current()) Clear();
}
bool Current(uint64_t epoch) {
    return Active() && epoch == g_epoch && g_world == ue_wrap::world_identity::Generation() &&
        g_serial == net::session_serial::Current();
}
void Dirty() {
    Next(g_revision);
    for (int slot = 1; slot < net::kMaxPeers; ++slot)
        if (g_session->HasPeerConn(slot) && g_session->IsSlotWorldReady(slot)) g_owed[slot] = true;
}
void Lease() {
    if (!Host()) return;
    const auto holder = device_occupancy::HolderOf(L"laptop");
    const auto claim = device_occupancy::LaptopClaimRevision();
    if (g_lease && holder == g_holder && claim == g_claimRevision) return;
    g_holder = holder; g_claimRevision = claim; Next(g_lease);
    g_sent.clear(); g_seen.fill(0); g_assembler.Clear();
    g_apply = !g_canonical.empty(); Dirty();
}
bool Author() {
    return g_lease && g_holder == players::Registry::Get().LocalPeerId() &&
        device_occupancy::LocalHolds(L"laptop");
}
void Publish(std::vector<uint8_t> bytes, std::vector<UI::Row> rows) {
    g_canonical = std::move(bytes); g_target = std::move(rows); Dirty();
}
// Capture while the claim still exists, also used by the occupancy falling edge.
// A rejected or partial send keeps that claim until the complete final edit fits.
bool Capture(bool final = false) {
    const auto epoch = g_epoch;
    const auto lease = g_lease;
    std::vector<UI::Row> rows;
    if (!UI::Read(rows)) return final; // widget/world gone: no final presentation remains
    if (!Current(epoch)) return false;
    Lease();
    if (lease != g_lease) return false;
    auto bytes = W::Encode(rows);
    if (bytes.empty()) return true; // unrepresentable presentation must not strand the device
    if (Host()) {
        if (bytes != g_canonical) Publish(std::move(bytes), std::move(rows));
        return true;
    }
    if (bytes == g_sent) return true;
    const auto command = Next(g_command);
    const auto encoded = W::EncodeSnapshot(false, g_lease, command, g_holder, bytes);
    const bool sent = blob_chunks::SendBlobToSlotWhile(g_session, 0, net::ReliableKind::SharedScreen,
        Next(g_sequence), encoded, [=] { return Current(epoch) && g_lease == lease && Author(); });
    if (!Current(epoch) || g_lease != lease) return false;
    if (sent) { g_sent = std::move(bytes); g_target = std::move(rows); }
    return sent;
}
}
void Install(net::Session* session) {
    if (g_session != session) { g_session = session; Clear(); }
}
void Tick() {
    if (!ue_wrap::game_thread::IsGameThread() || !Active() || g_busy) return;
    g_busy = true; struct Busy { ~Busy() { g_busy = false; } } busy;
    World(); Lease();
    const auto now = std::chrono::steady_clock::now();
    if (now < g_next) return;
    g_next = now + std::chrono::milliseconds(200);
    g_assembler.Sweep(now, std::chrono::seconds(5));
    const auto epoch = g_epoch;
    if (!g_target.empty() && (g_apply || !Author())) {
        // Native bindings can repaint observers. A new operator must first
        // inherit the preceding screen, including scroll/text/visibility.
        const auto target = g_target;
        const auto revision = g_revision;
        const bool applied = UI::Apply(target);
        if (!Current(epoch) || revision != g_revision) return;
        if (applied) g_apply = false;
    }
    if ((!g_apply && Author()) || (Host() && g_canonical.empty())) {
        Capture();
        if (!Current(epoch)) return;
    }
    if (!Host() || g_canonical.empty()) return;
    const auto revision = g_revision;
    const auto encoded = W::EncodeSnapshot(true, g_lease, revision, g_holder, g_canonical);
    for (int slot = 1; slot < net::kMaxPeers; ++slot) {
        if (!g_owed[slot] || !g_session->HasPeerConn(slot) || !g_session->IsSlotWorldReady(slot)) continue;
        const auto peerEpoch = g_peerEpoch[slot];
        const bool sent = blob_chunks::SendBlobToSlotWhile(g_session, slot, net::ReliableKind::SharedScreen,
            Next(g_sequence), encoded, [=] {
                return Current(epoch) && revision == g_revision && peerEpoch == g_peerEpoch[slot];
            });
        if (!Current(epoch)) return;
        if (revision == g_revision && peerEpoch == g_peerEpoch[slot] && sent) g_owed[slot] = false;
    }
}
void OnChunk(const net::BlobChunkPayload& chunk, uint8_t sender) {
    if (!ue_wrap::game_thread::IsGameThread() || !Active() || sender >= net::kMaxPeers) return;
    World(); Lease();
    if (Host()) {
        if (sender == 0 || sender != g_holder || !g_session->HasPeerConn(sender) ||
            !g_session->IsSlotWorldReady(sender)) return;
    } else if (sender != 0) return;
    constexpr auto maxBytes = W::kMaxBytes + W::kEnvelopeBytes;
    if (chunk.chunks > (maxBytes + sizeof(chunk.data) - 1) / sizeof(chunk.data)) return;
    std::vector<uint8_t> bytes;
    if (!g_assembler.OnChunk(chunk, sender, bytes)) return;
    W::Snapshot snapshot;
    if (!W::DecodeSnapshot(bytes, snapshot) ||
        (snapshot.owner != 0xff && snapshot.owner >= net::kMaxPeers)) return;
    if (Host()) {
        if (snapshot.canonical || snapshot.owner != sender || snapshot.lease != g_lease ||
            !Newer(snapshot.revision, g_seen[sender])) return;
        g_seen[sender] = snapshot.revision;
        const auto rows = W::Encode(snapshot.rows);
        Publish(rows, std::move(snapshot.rows)); g_apply = true;
    } else {
        if (!snapshot.canonical || !Newer(snapshot.revision, g_revision)) return;
        const bool newLease = snapshot.lease != g_lease;
        g_lease = snapshot.lease; g_revision = snapshot.revision; g_holder = snapshot.owner;
        // Same-lease acknowledgements must not roll back newer local edits.
        if (newLease || !Author()) { g_target = std::move(snapshot.rows); g_apply = true; }
        if (newLease) g_sent.clear();
    }
}
bool BeforeLocalRelease() {
    if (!Active()) return true;
    World(); Lease();
    if (!Author() || g_apply) return true;
    return Capture(true);
}
void QueueConnectBroadcastForSlot(int slot) {
    if (slot > 0 && slot < net::kMaxPeers) g_owed[slot] = true;
}
void OnDisconnectForSlot(int slot) {
    if (slot < 0 || slot >= net::kMaxPeers) return;
    g_owed[slot] = false; g_seen[slot] = 0; ++g_peerEpoch[slot];
    g_assembler.ClearSlot(static_cast<uint8_t>(slot));
    if (Host()) Lease();
    g_owed[slot] = false;
}
void OnDisconnect() {
    g_session = nullptr; Clear();
    g_sequence = g_lease = g_revision = g_command = 0;
}
}
