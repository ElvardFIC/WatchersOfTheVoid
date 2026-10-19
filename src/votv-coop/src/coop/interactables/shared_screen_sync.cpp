#include "coop/interactables/shared_screen_sync.h"
#include "coop/interactables/shared_screen_wire.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include <chrono>

namespace coop::shared_screen_sync {
namespace {
net::Session* g_session = nullptr;
blob_chunks::Assembler g_assembler;
std::vector<uint8_t> g_canonical, g_sent;
std::vector<ue_wrap::shared_screen::Row> g_target;
bool g_owed[net::kMaxPeers]{};
uint32_t g_sequence = 1;
uint8_t g_holder = 0xFF;
std::chrono::steady_clock::time_point g_next{};
std::chrono::steady_clock::time_point g_lastSend{};
bool g_apply = false;
void Publish(const std::vector<uint8_t>& bytes, int except = -1) {
    g_canonical = bytes;
    g_apply = shared_screen_wire::Decode(bytes, g_target);
    for (int slot = 1; slot < net::kMaxPeers; ++slot)
        if (slot != except && g_session->IsSlotWorldReady(slot)) g_owed[slot] = true;
}
}
void Install(net::Session* session) { g_session = session; }
void Tick() {
    if (!g_session || !g_session->connected()) return;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_next) return;
    g_next = now + std::chrono::milliseconds(200);
    g_assembler.Sweep(now, std::chrono::seconds(2));
    const uint8_t holder = device_occupancy::HolderOf(L"laptop");
    if (holder != g_holder) { g_holder = holder; g_sent.clear(); g_assembler.Clear(); }
    if (device_occupancy::LocalHolds(L"laptop")) {
        std::vector<ue_wrap::shared_screen::Row> rows;
        if (ue_wrap::shared_screen::Read(rows)) {
            auto bytes = shared_screen_wire::Encode(rows);
            if (!bytes.empty() && (bytes != g_sent || now - g_lastSend > std::chrono::seconds(2))) {
                g_target = rows; g_apply = true;
                if (g_session->role() == net::Role::Host) { Publish(bytes); g_sent = std::move(bytes); }
                else if (blob_chunks::SendBlobToSlot(g_session, 0, net::ReliableKind::SharedScreen,
                                                      g_sequence++, bytes)) g_sent = std::move(bytes);
                g_lastSend = now;
            }
        }
    } else if (g_apply) {
        // Reapply while observing: native widget bindings may repaint, and late
        // children can appear after a screen snapshot arrives during loading.
        ue_wrap::shared_screen::Apply(g_target);
    }
    if (g_session->role() == net::Role::Host && !g_canonical.empty()) {
        for (int slot = 1; slot < net::kMaxPeers; ++slot)
            if (g_owed[slot] && g_session->IsSlotWorldReady(slot) &&
                blob_chunks::SendBlobToSlot(g_session, slot, net::ReliableKind::SharedScreen,
                                           g_sequence++, g_canonical)) g_owed[slot] = false;
    }
}
void OnChunk(const net::BlobChunkPayload& chunk, uint8_t sender) {
    if (!g_session || sender >= net::kMaxPeers) return;
    const bool host = g_session->role() == net::Role::Host;
    if (host ? (sender == 0 || device_occupancy::HolderOf(L"laptop") != sender) : sender != 0) return;
    // Bound partial assemblies before allocating; this is a UI snapshot lane.
    if (chunk.chunks > (shared_screen_wire::kMaxBytes + sizeof(chunk.data) - 1) / sizeof(chunk.data)) return;
    std::vector<uint8_t> bytes;
    if (!g_assembler.OnChunk(chunk, sender, bytes)) return;
    std::vector<ue_wrap::shared_screen::Row> rows;
    if (!shared_screen_wire::Decode(bytes, rows)) return;
    if (host) Publish(bytes, sender);
    if (!device_occupancy::LocalHolds(L"laptop")) { g_target = std::move(rows); g_apply = true; }
}
void QueueConnectBroadcastForSlot(int slot) {
    if (slot > 0 && slot < net::kMaxPeers) g_owed[slot] = true;
}
void OnDisconnectForSlot(int slot) {
    if (slot >= 0 && slot < net::kMaxPeers) { g_owed[slot] = false; g_assembler.ClearSlot(static_cast<uint8_t>(slot)); }
}
void OnDisconnect() {
    g_assembler.Clear(); g_canonical.clear(); g_sent.clear(); g_target.clear();
    for (bool& owed : g_owed) owed = false;
    g_holder = 0xFF; g_apply = false; g_next = {};
}
}
