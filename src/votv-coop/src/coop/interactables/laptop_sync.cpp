// coop/interactables/laptop_sync.cpp -- see coop/interactables/laptop_sync.h.

#include "coop/interactables/laptop_sync.h"

#include "coop/interactables/portable_pc_lid.h"  // op 6 goes to the portable PC's lid lane
#include "coop/net/session.h"

#include "ue_wrap/devices/laptop.h"
#include "ue_wrap/core/log.h"

#include <windows.h>

#include <atomic>
#include <array>
#include <cstdint>

namespace coop::laptop_sync {
namespace {

namespace L = ue_wrap::laptop;

std::atomic<coop::net::Session*> g_session{nullptr};

constexpr uint64_t kPollMs = 250;  // the 4 Hz edge poll

uint64_t NowMs() {
    return static_cast<uint64_t>(::GetTickCount64());
}

// The poll baseline, primed on every wire apply (the apply-and-prime shape).
bool     g_havePrev = false;
bool     g_prevOpened = false;
uint64_t g_nextPoll = 0;

// Wire targets survive actor loading and animation. All intermediate edges are suppressed
// until convergence; only the latest target matters.
bool g_wantValid = false;
bool g_wantOpened = false;

bool g_announced = false;

struct PendingSend {
    bool needsState = false;  // join seed waiting for a settled native state
    bool valid = false;
    coop::net::LaptopStatePayload payload{};
    uint8_t origin = 0;
};
std::array<PendingSend, coop::net::kMaxPeers> g_pending{};

void FlushOutgoing(coop::net::Session* s) {
    if (!s->connected()) return;
    for (int slot = 0; slot < static_cast<int>(g_pending.size()); ++slot) {
        auto& row = g_pending[slot];
        if (!row.valid) continue;
        if (s->role() == coop::net::Role::Host) {
            if (slot == 0 || !s->IsSlotReady(slot)) continue;
        } else if (slot != 0) continue;
        if (s->SendReliableToSlot(slot, coop::net::ReliableKind::LaptopState,
                                  &row.payload, sizeof(row.payload), row.origin))
            row.valid = false;
    }
}

// A client to the host; the host to ready clients or slots already owed a join/retry.
void SendOut(coop::net::Session* s, const coop::net::LaptopStatePayload& p, int exceptSlot) {
    if (s->role() == coop::net::Role::Client) {
        g_pending[0] = {false, true, p, 0};
    } else {
        for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers); ++slot) {
            if (slot == exceptSlot) continue;
            if (!s->IsSlotReady(slot) && !g_pending[slot].valid && !g_pending[slot].needsState) continue;
            g_pending[slot] = {false, true, p,
                              exceptSlot > 0 ? static_cast<uint8_t>(exceptSlot) : uint8_t{0}};
        }
    }
    FlushOutgoing(s);
}

void SampleJoinStates(coop::net::Session* s) {
    if (s->role() != coop::net::Role::Host || g_wantValid) return;
    bool needed = false;
    for (const auto& row : g_pending) needed |= row.needsState;
    if (!needed || !L::EnsureResolved() || !L::Instance()) return;
    L::PowerState ps;
    if (!L::ReadPower(ps) || ps.anim) return;
    for (auto& row : g_pending) {
        if (!row.needsState) continue;
        row = {};
        row.valid = true;
        row.payload.op = 3;
        row.payload.isOpened = ps.isOpened ? 1 : 0;
    }
}

void PrimeBaselines() {
    L::PowerState ps;
    if (L::ReadPower(ps)) {
        g_prevOpened = ps.isOpened;
        g_havePrev = true;
    }
}

void ApplyPowerTarget() {
    if (!g_wantValid) return;
    L::PowerState ps;
    if (!L::ReadPower(ps)) return;
    if (ps.anim) return;               // boot/shutdown latent running -- retry next poll
    if (ps.isOpened == g_wantOpened) {
        PrimeBaselines();
        g_wantValid = false;
        return;
    }  // converged
    if (!ps.powered && g_wantOpened) return;  // wall power lags the panel's canonical -- retry
    if (L::CallPowerToggle())
        UE_LOGI("laptop_sync: power replay dispatched (target isOpened=%u)",
                static_cast<unsigned>(g_wantOpened));
    // The opened flag settles after the native latent chain; the poll's want-target predicate
    // consumes that edge as wire-transient.
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running()) return;
    const uint64_t now = NowMs();
    if (now < g_nextPoll) return;
    g_nextPoll = now + kPollMs;

    if (!L::EnsureResolved() || !L::Instance()) {
        FlushOutgoing(s);
        return;
    }
    if (!g_announced) {
        g_announced = true;
        UE_LOGI("laptop_sync: installed (laptop resolved; power axis)");
    }
    if (!g_havePrev) PrimeBaselines();

    ApplyPowerTarget();

    L::PowerState ps;
    if (!L::ReadPower(ps)) { FlushOutgoing(s); return; }
    if (s->connected() && g_havePrev && ps.isOpened != g_prevOpened && !g_wantValid) {
        coop::net::LaptopStatePayload p{};
        p.op = 0;
        p.isOpened = ps.isOpened ? 1 : 0;
        SendOut(s, p, -1);
        UE_LOGI("laptop_sync: local POWER edge (isOpened=%u) -- broadcast", static_cast<unsigned>(p.isOpened));
    }
    g_prevOpened = ps.isOpened;
    g_havePrev = true;
    SampleJoinStates(s);
    FlushOutgoing(s);
}

void OnLaptopState(const coop::net::LaptopStatePayload& p, uint8_t senderSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) return;
    // The lid op addresses a portable-PC prop by eid, not the laptop: its own lane.
    if (p.op == 6) {
        coop::portable_pc_lid::OnLid(p, senderSlot);
        return;
    }
    if (p.op != 0 && p.op != 3) return;
    if (!s->running() || !s->connected() || p.isOpened > 1) return;
    if (senderSlot >= coop::net::kMaxPeers) return;
    if (s->role() == coop::net::Role::Host && (p.op != 0 || senderSlot == 0)) return;
    // An older unsent local/relay value must not undo this newer received target.
    g_pending[s->role() == coop::net::Role::Client ? 0 : senderSlot] = {};
    // A power edge (0) or a joiner's state (3): the target converges through the native toggle.
    g_wantValid = true;
    g_wantOpened = p.isOpened != 0;
    if (L::EnsureResolved() && L::Instance()) {
        ApplyPowerTarget();
        PrimeBaselines();
    }
    // The host re-fans a client's power edge to the other clients (the origin excluded).
    if (s->role() == coop::net::Role::Host && p.op == 0) SendOut(s, p, /*exceptSlot*/ senderSlot);
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;
    if (peerSlot <= 0 || peerSlot >= static_cast<int>(g_pending.size())) return;
    g_pending[peerSlot] = {};
    g_pending[peerSlot].needsState = true;
    SampleJoinStates(s);
    FlushOutgoing(s);
}

void OnPeerLeft(int peerSlot) {
    if (peerSlot >= 0 && peerSlot < static_cast<int>(g_pending.size()))
        g_pending[peerSlot] = {};
}

void OnDisconnect() {
    g_havePrev = false;
    g_prevOpened = false;
    g_nextPoll = 0;
    g_wantValid = false;
    g_pending = {};
    g_announced = false;
}

}  // namespace coop::laptop_sync
