#include "coop/player/hand_fire_sync.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/actors/prop_fire.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>

namespace coop::hand_fire_sync {
namespace {
namespace F = ue_wrap::prop_fire;
namespace sg = ue_wrap::script_gate;
using net::HandFirePayload;
using ue_wrap::CachedObjRef;
struct Row {
    HandFirePayload state{};
    CachedObjRef actor;
    uint64_t received = 0;
    uint64_t retryAt = 0;
    uint32_t pending = 0;
    bool applied = false;
};
std::array<Row, net::kMaxPeers> g_rows;
std::atomic<net::Session*> g_session{nullptr};
uint64_t g_epoch = 0, g_poll = 0, g_lastSample = 0;
uint32_t g_world = 0, g_sequence = 0;
bool g_ticking = false;
CachedObjRef g_applying;
uint64_t Now() { using namespace std::chrono; return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count(); }
net::Session* Session() { auto* s = g_session.load(); return s && s->connected() ? s : nullptr; }
uint8_t Self() { return players::Registry::Get().LocalPeerId(); }
bool After(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) > 0; }
F::State Decode(const HandFirePayload& p) { return {p.burning != 0, p.fuel, p.fuelSpeed}; }
F::State Aged(const Row& r, uint64_t now) {
    auto value = Decode(r.state);
    value.fuel = std::max(0.f, value.fuel - value.fuelSpeed * float(now >= r.received ? now - r.received : 0) / 1000.f);
    if (value.fuel <= 0) value.burning = false;
    return value;
}
void Clear() {
    ++g_epoch; g_rows = {}; g_poll = g_lastSample = 0; g_sequence = 0;
    // Do not reset stack-owned applying/ticking guards during a native callback.
}
void World() {
    const auto w = ue_wrap::world_identity::Generation();
    if (w != g_world) { Clear(); g_world = w; }
}
bool Current(uint64_t epoch) { return epoch == g_epoch && Session() && g_world == ue_wrap::world_identity::Generation(); }
uint32_t Destinations(net::Session& s, uint8_t owner) {
    if (s.role() == net::Role::Client) return 1;
    uint32_t mask = 0;
    for (int slot = 1; slot < net::kMaxPeers; ++slot)
        if (slot != owner && s.IsSlotReady(slot)) mask |= uint32_t(1u << slot);
    return mask;
}
sg::Verdict GuardMirror(const sg::Call& c) {
    if (!Session() || !hand_item::IsMirrorActor(c.object)) return sg::Verdict::Run;
    // Owner runs the actual item's damage/subclass behavior. The display mirror
    // only runs base fire effects and its native contact trace against local actors.
    if (c.tag == 0x48460001 && g_applying.Is(c.object)) return sg::Verdict::Run;
    return sg::Verdict::Cancel;
}
bool HooksLive() {
    return sg::NameWatchLive(L"ignite", 0x48460001) &&
        sg::NameWatchLive(L"extinguishFire", 0x48460002) &&
        sg::NameWatchLive(L"fireDamage", 0x48460003) &&
        sg::NameWatchLive(L"ignited", 0x48460004);
}
}
void Install(net::Session* s) {
    if (g_session.load() != s) Clear();
    g_session.store(s); World();
    sg::WatchName(L"ignite", 0x48460001, &GuardMirror, nullptr);
    sg::WatchName(L"extinguishFire", 0x48460002, &GuardMirror, nullptr);
    sg::WatchName(L"fireDamage", 0x48460003, &GuardMirror, nullptr);
    sg::WatchName(L"ignited", 0x48460004, &GuardMirror, nullptr);
}
void Receive(const HandFirePayload& p, uint8_t sender) {
    auto* s = Session();
    if (!s || p.slot >= net::kMaxPeers || !p.generation || !p.sequence || p.burning > 1 ||
        p.reserved || !F::Valid(Decode(p)) || p.slot == Self()) return;
    if (s->role() == net::Role::Host) {
        if (!sender || sender >= net::kMaxPeers || sender != p.slot) return;
    } else if (sender != 0) return;
    World(); auto& r = g_rows[p.slot];
    if (r.state.sequence && !After(p.sequence, r.state.sequence)) return;
    // HandItem and HandFire may cross at spawn/join; keep the newest fire sample,
    // but only apply it once that exact hand generation exists.
    r.state = p; r.received = Now(); r.applied = false; r.retryAt = 0;
    r.pending = s->role() == net::Role::Host ? Destinations(*s, p.slot) : 0;
}
void QueueConnect(int slot) {
    if (slot <= 0 || slot >= net::kMaxPeers) return;
    World();
    for (auto& r : g_rows) if (r.state.sequence && r.state.slot != slot) r.pending |= uint32_t(1u << slot);
    g_poll = 0;
}
void OnPeerLeft(int slot) {
    if (slot <= 0 || slot >= net::kMaxPeers) return;
    ++g_epoch; g_rows[slot] = {};
    for (auto& r : g_rows) r.pending &= uint32_t(~(1u << slot));
}
void OnDisconnect() { g_session.store(nullptr); Clear(); g_world = 0; }
void Tick() {
    auto* s = Session(); if (!s || g_ticking) return;
    struct Guard { Guard() { g_ticking = true; } ~Guard() { g_ticking = false; } } guard;
    World(); const auto epoch = g_epoch, now = Now();
    sg::ResolvePendingNames(); if (!Current(epoch) || !HooksLive()) return;
    const uint8_t self = Self(); if (self >= net::kMaxPeers) return;
    if (now >= g_poll) {
        g_poll = now + 250;
        auto& own = g_rows[self];
        void* a = hand_item::LocalHandActor();
        const auto generation = hand_item::Generation(self);
        F::State value{};
        if (a && generation && F::Read(a, value)) {
            if (!Current(epoch) || hand_item::Generation(self) != generation || hand_item::LocalHandActor() != a) return;
            const auto predicted = Aged(own, now);
            if (own.state.generation != generation || value.burning != (own.state.burning != 0) ||
                std::fabs(value.fuelSpeed - own.state.fuelSpeed) > .01f ||
                std::fabs(value.fuel - predicted.fuel) > 1.f ||
                (value.burning && now - g_lastSample >= 2000)) {
                if (!++g_sequence) ++g_sequence;
                own.state = {generation, g_sequence, value.fuel, value.fuelSpeed, self, uint8_t(value.burning), 0};
                own.received = now; own.pending = Destinations(*s, self); g_lastSample = now;
            }
        } else if (!generation) own = {};
        for (auto& r : g_rows) if (r.state.sequence && r.pending) {
            auto p = r.state; const auto aged = Aged(r, now);
            p.fuel = aged.fuel; p.burning = uint8_t(aged.burning);
            for (int slot = 0; slot < net::kMaxPeers; ++slot) if (r.pending & (1u << slot)) {
                if (s->role() == net::Role::Host && !s->IsSlotReady(slot)) continue;
                if (s->SendReliableToSlot(slot, net::ReliableKind::HandFire, &p, sizeof(p))) r.pending &= uint32_t(~(1u << slot));
            }
        }
    }
    for (uint8_t slot = 0; slot < net::kMaxPeers; ++slot) {
        if (slot == self) continue;
        auto& r = g_rows[slot]; const auto p = r.state;
        void* a = hand_item::MirrorActorForSlot(slot);
        if (!a || !p.sequence || hand_item::MirrorGeneration(slot) != p.generation) continue;
        if (!r.actor.Is(a)) { r.actor.Set(a); r.applied = false; r.retryAt = 0; }
        if (r.applied || now < r.retryAt) continue;
        r.retryAt = now + 250; // An underwater/native rejection must not spawn a flame every frame.
        const auto identity = r.actor;
        const auto valid = [&] { return Current(epoch) && identity.Is(a) && r.state.sequence == p.sequence &&
            hand_item::MirrorActorForSlot(slot) == a && hand_item::MirrorGeneration(slot) == p.generation; };
        struct ApplyGuard {
            CachedObjRef previous = g_applying;
            explicit ApplyGuard(void* actor) { g_applying.Set(actor); }
            ~ApplyGuard() { g_applying = previous; }
        } apply(a);
        const bool ok = F::Apply(a, Aged(r, now), valid);
        if (!Current(epoch)) return;
        if (ok && valid()) r.applied = true;
    }
}
}
