// coop/interactables/laptop_sync.cpp -- see coop/interactables/laptop_sync.h.

#include "coop/interactables/laptop_sync.h"

#include "coop/interactables/portable_pc_lid.h"  // op 6 goes to the portable PC's lid lane
#include "coop/net/session.h"
#include "coop/player/roster_ledger.h"  // PerSlotState

#include "ue_wrap/devices/laptop.h"
#include "ue_wrap/core/log.h"

#include <windows.h>

#include <atomic>
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

// The wire's target, converged on through the laptop's own toggle. The toggle runs a latent chain: laptop_C's
// actionOptionIndex sets `anim` inside the call and returns at once while `anim` is set, and the chain's end flips
// `isOpened` and clears `anim` in one block. So a target is reached only once no chain runs, and the edge that ends a
// chain we dispatched is ours: the replay's flag holds the value that chain leaves, and that edge is consumed, not
// broadcast as this peer's own.
bool g_wantValid = false;
bool g_wantOpened = false;
bool g_replayInFlight = false;  // a toggle we dispatched is running
bool g_replayTo = false;        // ...and the isOpened its end leaves

bool g_announced = false;

// The send helper: a client to the host, the host to every ready client but the origin.
void SendOut(coop::net::Session* s, const coop::net::LaptopStatePayload& p, int exceptSlot) {
    if (s->role() == coop::net::Role::Client) {
        s->SendReliableToSlot(0, coop::net::ReliableKind::LaptopState, &p, sizeof(p));
        return;
    }
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers); ++slot) {
        if (slot == exceptSlot || !s->IsSlotReady(slot)) continue;
        s->SendReliableToSlot(slot, coop::net::ReliableKind::LaptopState, &p, sizeof(p),
                              exceptSlot > 0 ? static_cast<uint8_t>(exceptSlot) : 0);
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
    if (ps.anim) return;               // a chain runs: isOpened is not final until its end -- retry next poll
    if (ps.isOpened == g_wantOpened) { g_wantValid = false; return; }  // reached
    // Our last chain's edge is not judged yet (it ended after the last poll): a new chain now would replace the value
    // that edge is consumed by. The next poll judges it and ends the flag.
    if (g_replayInFlight) return;
    if (!ps.powered && g_wantOpened) return;  // wall power lags the panel's canonical -- retry
    if (!L::CallPowerToggle()) return;
    // A call the blueprint's own early returns met starts no chain, so only a chain seen running is ours.
    L::PowerState after;
    if (L::ReadPower(after) && after.anim) {
        g_replayInFlight = true;
        g_replayTo = !ps.isOpened;
    }
    UE_LOGI("laptop_sync: power replay dispatched (target isOpened=%u)", static_cast<unsigned>(g_wantOpened));
}

// Instruments (the world_latest drill): the host's applications of a client's line, per sender, and the edges this
// peer broadcast as its own.
coop::roster_ledger::PerSlotState<uint32_t> g_clientEdges;
uint32_t g_organicSent = 0;

// A wire target applied: converge on it through the native toggle, whose end the replay's flag consumes. The host
// re-fans a client's power edge (op 0) to the other clients, the origin excluded.
void ApplyTarget(coop::net::Session* s, const coop::net::LaptopStatePayload& p, uint8_t senderSlot) {
    g_wantValid = true;
    g_wantOpened = p.isOpened != 0;
    ApplyPowerTarget();
    if (s->role() != coop::net::Role::Host) return;
    if (p.op == 0) SendOut(s, p, /*exceptSlot*/ senderSlot);
    ++g_clientEdges[senderSlot];
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

    if (!L::EnsureResolved() || !L::Instance()) return;
    if (!g_announced) {
        g_announced = true;
        UE_LOGI("laptop_sync: installed (laptop resolved; power axis)");
    }
    if (!g_havePrev) { PrimeBaselines(); return; }

    // The edge first, judged against the chain that made it; a chain dispatched below is judged on a later poll.
    L::PowerState ps;
    if (!L::ReadPower(ps)) return;
    if (s->connected() && ps.isOpened != g_prevOpened) {
        if (g_replayInFlight && ps.isOpened == g_replayTo) {
            g_replayInFlight = false;  // our replay's end: consumed. A press in the gap before this poll went the
                                       // other way, so its own edge is still to come, and goes out.
        } else {
            coop::net::LaptopStatePayload p{};
            p.op = 0;
            p.isOpened = ps.isOpened ? 1 : 0;
            SendOut(s, p, -1);
            ++g_organicSent;
            UE_LOGI("laptop_sync: local POWER edge (isOpened=%u) -- broadcast", static_cast<unsigned>(p.isOpened));
        }
    }
    // A chain that ended without its edge (the wall power changed under it) ends the flag too.
    if (g_replayInFlight && !ps.anim) g_replayInFlight = false;
    g_prevOpened = ps.isOpened;

    ApplyPowerTarget();
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
    if (!L::EnsureResolved() || !L::Instance()) {
        UE_LOGW("laptop_sync: wire op=%u declined (laptop unresolved)", p.op);
        return;
    }
    // A power edge (0) or a joiner's state (3): the target converges through the native toggle.
    ApplyTarget(s, p, senderSlot);
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;
    if (!L::EnsureResolved() || !L::Instance()) return;
    // The target, not the laptop's state: while a replay runs, isOpened still holds the value it leaves.
    bool target = false;
    if (!Target(target)) return;
    coop::net::LaptopStatePayload p{};
    p.op = 3;
    p.isOpened = target ? 1 : 0;
    s->SendReliableToSlot(peerSlot, coop::net::ReliableKind::LaptopState, &p, sizeof(p));
    UE_LOGI("laptop_sync: connect state -> slot %d (isOpened=%u)", peerSlot, static_cast<unsigned>(p.isOpened));
}

uint32_t ClientEdgesApplied(int slot) { return g_clientEdges[slot]; }

uint32_t OrganicEdgesSent() { return g_organicSent; }

bool ReadLocalOpened(bool& out) {
    L::PowerState ps;
    if (!L::EnsureResolved() || !L::ReadPower(ps)) return false;
    out = ps.isOpened;
    return true;
}

bool Target(bool& out) {
    if (g_wantValid) {
        out = g_wantOpened;
        return true;
    }
    return ReadLocalOpened(out);
}

bool Settled(bool& opened) {
    L::PowerState ps;
    if (!L::EnsureResolved() || !L::ReadPower(ps)) return false;
    opened = ps.isOpened;
    return !ps.anim && !g_wantValid && !g_replayInFlight && g_havePrev && g_prevOpened == ps.isOpened;
}

void OnDisconnect() {
    g_havePrev = false;
    g_prevOpened = false;
    g_nextPoll = 0;
    g_wantValid = false;
    g_replayInFlight = false;
    g_announced = false;
}

}  // namespace coop::laptop_sync
