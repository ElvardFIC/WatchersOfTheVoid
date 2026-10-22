#include "coop/world/desk_short_circuit_sync.h"
#include "coop/net/session.h"
#include "coop/element/intent_authority.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/engine/world_identity.h"
#include <array>
#include <atomic>
#include <chrono>

namespace coop::desk_short_circuit_sync {
namespace {
namespace D = ue_wrap::console_desk;
namespace SG = ue_wrap::script_gate;
using Packet = net::DeskShortCircuitPayload;
constexpr uint64_t kWaitMs = 8000;
constexpr float kWaterReach = 2500.f; // water projectiles travel beyond the player's arm
std::atomic<net::Session*> g_session{nullptr};
uint32_t g_world = 0, g_sequence = 0, g_seenEvent = 0;
uint64_t g_nextTick = 0, g_nextRequest = 0;
bool g_applying = false, g_hostWasActive = false;
ue_wrap::CachedObjRef g_hostDesk;
struct Pending { Packet p{}; uint64_t since = 0, sent = 0; };
Pending g_request, g_effect;
struct Peer { Pending request; uint32_t ack = 0; bool oweAck = false; };
std::array<Peer, net::kMaxPeers> g_peers{};
uint32_t g_oweEffect = 0;

uint64_t Now() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
bool After(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) > 0; }
net::Session* Session() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->connected() ? s : nullptr;
}
void Clear() {
    g_sequence = g_seenEvent = 0;
    g_nextTick = g_nextRequest = 0;
    g_applying = g_hostWasActive = false;
    g_hostDesk = {};
    g_request = {}; g_effect = {}; g_peers = {}; g_oweEffect = 0;
}
void ResetWorld() {
    const auto world = ue_wrap::world_identity::Generation();
    if (world == g_world) return;
    g_world = world;
    Clear();
}
uint32_t Next() { if (!++g_sequence) ++g_sequence; return g_sequence; }

SG::Verdict OnShortCircuit(const SG::Call& c) {
    auto* s = Session();
    if (!s || s->role() != net::Role::Client || !net_pump::IsInAnnouncedWorld(c.object) ||
        c.object != D::Instance()) return SG::Verdict::Run;
    ResetWorld();
    if (g_applying) return SG::Verdict::Run;
    bool active = false;
    const auto now = Now();
    if (D::ReadShortCircuit(c.object, active) && !active && !g_request.p.sequence && now >= g_nextRequest) {
        g_request = {{Next(), 0, {}}, now, 0};
        g_nextRequest = now + 1000;
    }
    // Includes native projectile hits reached through our own reflected calls.
    // Only the host's effect replay may enter the client's native spark loop.
    return SG::Verdict::Cancel;
}
SG::Verdict OnDeskBlackout(const SG::Call& c) {
    auto* s = Session();
    // A replay ends later, through a latent ubergraph resume, outside g_applying.
    // Suppress just THIS caller's solar(), even when fromOurCode is set. Other
    // power-grid replays keep their existing rules and the host authors breakers.
    return s && s->role() == net::Role::Client && c.callerObject &&
           c.callerObject == D::Instance() && net_pump::IsInAnnouncedWorld(c.callerObject)
        ? SG::Verdict::Cancel : SG::Verdict::Run;
}

bool Start(void* desk) {
    const auto world = g_world;
    ue_wrap::CachedObjRef ref; ref.Set(desk);
    g_applying = true;
    const bool called = ue_wrap::component_calls::CallParamlessNamed(desk, L"shortCircuit");
    g_applying = false;
    bool active = false;
    return called && world == ue_wrap::world_identity::Generation() && ref.Get() == desk &&
           D::ReadShortCircuit(desk, active) && active;
}

void HostTick(net::Session& s, void* desk, uint64_t now) {
    bool active = false;
    if (!D::ReadShortCircuit(desk, active)) return;
    if (!active) g_hostWasActive = false; // a new request can restart it during this very tick
    const auto world = g_world;
    for (int slot = 1; slot < net::kMaxPeers; ++slot) {
        auto& peer = g_peers[slot];
        if (!s.IsSlotReady(slot)) { peer = {}; continue; }
        const auto request = peer.request;
        if (request.p.sequence) {
            auto intent = element::IntentTarget::ForClientIntent(s, uint8_t(slot), kWaterReach);
            if (now - request.since < kWaitMs && !intent.HasBody()) continue;
            ue_wrap::CachedObjRef ref; ref.Set(desk);
            const bool allowed = now - request.since < kWaitMs && bool(intent.Authorize(desk));
            if (world != ue_wrap::world_identity::Generation() || ref.Get() != desk) return;
            if (allowed && !active) {
                if (!Start(desk)) return; // transient reflection failure: keep request for bounded retry
                if (world != ue_wrap::world_identity::Generation() || ref.Get() != desk) return;
                active = true;
            }
            peer.ack = request.p.sequence;
            peer.oweAck = true;
            peer.request = {};
        }
        if (peer.oweAck) {
            const Packet ack{peer.ack, 2, {}};
            if (s.SendReliableToSlot(slot, net::ReliableKind::DeskShortCircuit, &ack, sizeof(ack))) peer.oweAck = false;
        }
    }
    // Observe the actual guard, covering host water/bucket hits and accepted client hits.
    // Fifty native delayed pulses keep it true for 2.5–7.5 seconds, well beyond this poll.
    if (active && (!g_hostWasActive || g_hostDesk.Get() != desk)) {
        g_effect = {{Next(), 1, {}}, now, 0};
        g_oweEffect = 0;
        for (int slot = 1; slot < net::kMaxPeers; ++slot)
            if (s.IsSlotReady(slot)) g_oweEffect |= uint32_t(1u << slot);
    }
    g_hostWasActive = active;
    g_hostDesk.Set(desk);
    if (now - g_effect.since >= kWaitMs) g_oweEffect = 0;
    for (int slot = 1; slot < net::kMaxPeers; ++slot) {
        const auto bit = uint32_t(1u << slot);
        if ((g_oweEffect & bit) && (!s.IsSlotReady(slot) ||
            s.SendReliableToSlot(slot, net::ReliableKind::DeskShortCircuit, &g_effect.p, sizeof(Packet))))
            g_oweEffect &= uint32_t(~bit);
    }
}
} // namespace

void Install(net::Session* session) {
    if (g_session.load(std::memory_order_acquire) != session) OnDisconnect();
    g_session = session;
    static bool installed = false;
    if (installed) return;
    installed = true;
    const bool circuit = SG::WatchClassName(L"analogDScreenTest_C", L"shortCircuit", 0x44534301, &OnShortCircuit, nullptr);
    const bool blackout = SG::WatchClassName(L"powerControl_C", L"solar", 0x44534302, &OnDeskBlackout, nullptr);
    // A client's cosmetic replay must not alter the global ambient-event latch,
    // particularly if another host event is still active when this loop finishes.
    const bool ambient = SG::WatchClassName(L"lib_C", L"setEvent", 0x44534303, &OnDeskBlackout, nullptr);
    if (!circuit || !blackout || !ambient)
        UE_LOGE("desk-short-circuit: failed to register native gates");
}
void Tick() {
    auto* s = Session(); if (!s) return;
    ResetWorld();
    const auto now = Now(); if (now < g_nextTick) return; g_nextTick = now + 100;
    if (s->role() == net::Role::Client && g_request.p.sequence) {
        if (now - g_request.since >= kWaitMs) g_request = {};
        else if (!g_request.sent || now - g_request.sent >= 500) {
            if (s->SendReliableToSlot(0, net::ReliableKind::DeskShortCircuit, &g_request.p, sizeof(Packet)))
                g_request.sent = now;
        }
    }
    void* desk = D::Instance();
    if (!desk || !net_pump::IsInAnnouncedWorld(desk)) return;
    if (s->role() == net::Role::Host) { HostTick(*s, desk, now); return; }
    if (!g_effect.p.sequence) return;
    if (now - g_effect.since >= kWaitMs) { g_effect = {}; return; }
    bool active = false;
    if (!D::ReadShortCircuit(desk, active)) return;
    // The previous native loop may finish a little later than the host's.
    // Wait for it instead of losing the start of the next water-induced circuit.
    if (!active && Start(desk)) g_effect = {};
}
void Receive(const Packet& p, uint8_t sender) {
    auto* s = Session();
    if (!s || !p.sequence || sender >= net::kMaxPeers || p.op > 2 ||
        p.reserved[0] || p.reserved[1] || p.reserved[2]) return;
    ResetWorld();
    if (s->role() == net::Role::Host) {
        if (sender == 0 || p.op != 0 || !s->IsSlotReady(sender)) return;
        auto& peer = g_peers[sender];
        if (!After(p.sequence, peer.ack)) {
            if (p.sequence == peer.ack) peer.oweAck = true;
            return;
        }
        if (!peer.request.p.sequence) peer.request = {p, Now(), 0};
    } else if (sender == 0) {
        if (p.op == 2 && p.sequence == g_request.p.sequence) g_request = {};
        if (p.op == 1 && After(p.sequence, g_seenEvent)) {
            g_seenEvent = p.sequence;
            g_effect = {p, Now(), 0};
        }
    }
}
void OnPeerLeft(int slot) {
    if (slot > 0 && slot < net::kMaxPeers) {
        g_peers[slot] = {};
        g_oweEffect &= uint32_t(~(1u << slot));
    }
}
void OnDisconnect() { g_session = nullptr; g_world = 0; Clear(); }
}
