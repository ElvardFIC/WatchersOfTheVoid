#pragma once
#ifndef VOTV_COOP_PROP_END_DELIVERY_H
#define VOTV_COOP_PROP_END_DELIVERY_H
// Shared game-thread End delivery for the publisher and the syncer judge. Native release is owned by the caller.
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/element/registry.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_element_tracker.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace coop::prop_end_delivery {
inline coop::net::Session* g_session = nullptr;
inline uint64_t g_epoch = 1, g_stops = 0, g_identity = 0;
inline uint32_t g_world = 0, g_serial = 0;
inline bool g_draining = false;
inline size_t g_turn = 0;
inline std::array<uint64_t, coop::net::kMaxPeers> g_departures{};
inline std::unordered_map<uint32_t, uint64_t> g_latest;
struct Recipient {
    uint32_t generation = 0;
    uint64_t departure = 0, lastAttempt = 0;
    bool active = false, attempted = false;
};
struct Batch {
    coop::net::PropDriveEndPayload payload{};
    ue_wrap::CachedObjRef actor, cls;
    uint64_t epoch = 0, identity = 0;
    bool cancelled = false;
    std::array<Recipient, coop::net::kMaxPeers> recipients;
    int cursor = 1;
};
using Receipt = std::shared_ptr<Batch>;
inline std::unordered_map<uint32_t, Receipt> g_pending;
inline constexpr size_t kBudget = 16;
inline constexpr uint64_t kRetryMs = 250;
using Guard = std::function<bool()>;

inline void Clear() {
    ++g_epoch; g_session = nullptr; g_world = g_serial = 0; g_stops = 0;
    g_pending.clear(); g_latest.clear(); g_departures.fill(0); g_turn = 0;
}
inline bool ContextCurrent() {
    return g_session && g_world == ue_wrap::world_identity::Generation() &&
        g_serial == coop::net::session_serial::Current() && g_stops == g_session->StopCount() &&
        g_session->connected() && g_session->role() == coop::net::Role::Host;
}
inline bool Bind(coop::net::Session& s) {
    if (g_session && (g_session != &s || !ContextCurrent())) Clear();
    if (!s.connected() || s.role() != coop::net::Role::Host) return false;
    g_session = &s; g_world = ue_wrap::world_identity::Generation();
    g_serial = coop::net::session_serial::Current(); g_stops = s.StopCount();
    return ContextCurrent();
}
inline bool Basic(const Receipt& batch) {
    const auto latest = g_latest.find(batch->payload.eid);
    return !batch->cancelled && batch->epoch == g_epoch && ContextCurrent() && latest != g_latest.end() &&
        latest->second == batch->identity && batch->actor.Get() && batch->cls.Get();
}
inline bool Current(const Receipt& batch, const Guard& source = {}) {
    const auto valid = [&] { return (!source || source()) && Basic(batch); };
    if (!valid()) return false;
    void* cls = ue_wrap::reflection::ClassOf(batch->actor.Raw());
    if (!valid() || cls != batch->cls.Raw()) return false;
    const auto eid = coop::prop_element_tracker::GetPropElementIdForActor(batch->actor.Raw());
    if (!valid() || static_cast<uint32_t>(eid) != batch->payload.eid) return false;
    cls = ue_wrap::reflection::ClassOf(batch->actor.Raw());
    return valid() && cls == batch->cls.Raw();
}
inline void Erase(const Receipt& batch) {
    const auto found = g_pending.find(batch->payload.eid);
    if (found != g_pending.end() && found->second == batch) g_pending.erase(found);
}
inline void Cancel(const Receipt& batch) {
    if (!batch) return;
    batch->cancelled = true;
    Erase(batch);
}
inline bool RecipientCurrent(const Receipt& batch, int slot) {
    const auto& recipient = batch->recipients[slot];
    return Basic(batch) && recipient.active && recipient.generation &&
        recipient.generation == g_session->peerGenerationForSlot(slot) &&
        recipient.departure == g_departures[slot] && g_session->IsSlotConnected(slot);
}
inline bool HasDebt(const Receipt& batch) {
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) if (batch->recipients[slot].active) return true;
    return false;
}
inline bool Attempt(const Receipt& batch, int slot, uint64_t now, const Guard& source = {}) {
    auto& recipient = batch->recipients[slot];
    if (!RecipientCurrent(batch, slot)) { recipient.active = false; return false; }
    if (!g_session->IsSlotWorldReady(slot)) return false;
    const auto elapsed = now >= recipient.lastAttempt ? now - recipient.lastAttempt : 0;
    if (recipient.attempted && elapsed < kRetryMs) return false;
    recipient.attempted = true; recipient.lastAttempt = now;
    if (!Current(batch, source)) { Cancel(batch); return false; }
    if (!RecipientCurrent(batch, slot)) { recipient.active = false; return false; }
    if (!g_session->IsSlotWorldReady(slot)) return false;
    const auto payload = batch->payload;
    const bool sent = g_session->SendReliableToSlot(slot, coop::net::ReliableKind::PropDriveEnd, &payload, sizeof(payload));
    if (!Current(batch, source)) { Cancel(batch); return true; }
    if (!RecipientCurrent(batch, slot) || sent) recipient.active = false;
    return true;
}
struct Drain {
    bool previous = g_draining;
    Drain() { g_draining = true; }
    ~Drain() { g_draining = previous; }
};
inline void Opened(uint32_t eid) {
    g_latest[eid] = ++g_identity;
    g_pending.erase(eid);
}
inline void Departed(uint8_t slot) {
    if (slot > 0 && slot < coop::net::kMaxPeers) ++g_departures[slot];
}
inline bool Valid(const coop::net::PropDriveEndPayload& p) {
    if (p._pad[0] || p._pad[1] || (p.physFlags & 0x80)) return false;
    if (!coop::element::Registry::IsAllowedHostAllocatedEid(p.eid) || !p.gen || !coop::net::PropDriveEndInBounds(p)) return false;
    for (float value : {p.x,p.y,p.z,p.pitch,p.yaw,p.roll,p.linVelX,p.linVelY,p.linVelZ,p.angVelX,p.angVelY,p.angVelZ})
        if (!std::isfinite(value)) return false;
    return true;
}
inline bool Queue(coop::net::Session& s, const coop::net::PropDriveEndPayload& input,
                  ue_wrap::CachedObjRef actor, ue_wrap::CachedObjRef cls, int skip = -1, const Guard& source = {},
                  Receipt* accepted = nullptr) {
    auto batch = std::make_shared<Batch>();
    batch->payload = input; batch->actor = actor; batch->cls = cls;
    if (!Valid(batch->payload) || !Bind(s)) return false;
    batch->epoch = g_epoch; batch->identity = ++g_identity;
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        auto& recipient = batch->recipients[slot];
        recipient.generation = s.peerGenerationForSlot(slot); recipient.departure = g_departures[slot];
        recipient.active = slot != skip && recipient.generation && s.IsSlotConnected(slot);
    }
    g_latest[batch->payload.eid] = batch->identity;
    g_pending[batch->payload.eid] = batch; // A callback can supersede this request before its first send.
    if (accepted) *accepted = batch;
    const bool nested = g_draining;
    Drain drain;
    if (!Current(batch, source)) { Cancel(batch); return false; }
    if (nested) return true;
    size_t budget = kBudget;
    const auto now = coop::active_drive::NowMs();
    for (int visited = 0; visited < coop::net::kMaxPeers - 1 && budget; ++visited) {
        const int slot = batch->cursor;
        batch->cursor = slot + 1 < coop::net::kMaxPeers ? slot + 1 : 1;
        if (Attempt(batch, slot, now, source)) --budget;
        if (!Current(batch, source)) { Cancel(batch); return false; }
    }
    if (!HasDebt(batch)) Erase(batch);
    return true; // Every original recipient either accepted, has retained debt, or lost its incarnation.
}
inline void Tick(coop::net::Session& s) {
    if (g_draining || !Bind(s) || g_pending.empty()) return;
    Drain drain;
    const auto epoch = g_epoch;
    const auto now = coop::active_drive::NowMs();
    std::vector<Receipt> batches;
    for (const auto& [eid, batch] : g_pending) batches.push_back(batch);
    size_t budget = kBudget;
    for (size_t visited = 0; visited < batches.size() && visited < kBudget && budget; ++visited) {
        auto batch = batches[g_turn++ % batches.size()];
        if (!Current(batch)) { Cancel(batch); continue; }
        for (int slots = 0; slots < coop::net::kMaxPeers - 1 && budget; ++slots) {
            const int slot = batch->cursor;
            batch->cursor = slot + 1 < coop::net::kMaxPeers ? slot + 1 : 1;
            const bool attempted = Attempt(batch, slot, now);
            if (attempted) --budget;
            if (epoch != g_epoch || !Basic(batch)) { Cancel(batch); break; }
            if (attempted) break;
        }
        if (epoch != g_epoch || !ContextCurrent()) return;
        if (!HasDebt(batch)) Erase(batch);
    }
}
} // namespace coop::prop_end_delivery
#endif
