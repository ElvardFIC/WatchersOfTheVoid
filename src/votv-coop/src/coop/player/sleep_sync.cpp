// coop/player/sleep_sync.cpp -- see coop/player/sleep_sync.h.

#include "coop/player/sleep_sync.h"

#include "coop/comms/chat_feed.h"
#include "coop/net/latest_wins.h"
#include "coop/net/net_clock.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/roster_ledger.h"  // PerSlotState, DisplayName
#include "coop/text/utf8_codec.h"
#include "l10n/l10n.h"

#include "ue_wrap/actors/sleep.h"
#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/world/world_singleton.h"

#include <atomic>
#include <cstring>
#include <string>

namespace coop::sleep_sync {
namespace {

namespace SLP = ue_wrap::sleep;
using coop::net::SleepOp;
using coop::net::SleepPeerState;
using coop::net::SleepStatePayload;
using coop::net::WireKey;

std::atomic<coop::net::Session*> g_session{nullptr};

// ---- this peer's own state, as polled -------------------------------------------------------------
SleepPeerState g_localState = SleepPeerState::Awake;
WireKey        g_localBed{};              // the bed's class name while not awake, else empty
bool           g_localSleeping = false;   // isSleep
bool           g_dreamProbSuppressed = false;  // a client holds gamemode.dreamProbability at 0
void*          g_gmInst = nullptr;        // the gamemode the polled state belongs to

// ---- what is known of every peer ------------------------------------------------------------------
// The host: each slot's state as it applied it (slot 0 the host's own, published). A client: as the host
// relayed it.
SleepPeerState g_state[coop::net::kMaxPeers] = {};
WireKey        g_bed[coop::net::kMaxPeers] = {};
bool           g_hostSleeping = false;    // the host: its isSleep as last published. A client: the host's last flag.
// A client: the state last sent to the host.
SleepPeerState g_sentState = SleepPeerState::Awake;
WireKey        g_sentBed{};

bool IsHost(coop::net::Session* s) { return s->role() == coop::net::Role::Host; }

bool SameBed(const WireKey& a, const WireKey& b) { return std::memcmp(&a, &b, sizeof(WireKey)) == 0; }

// One warning per refusal, at most one every five seconds, whatever the field: a hostile sender cannot
// fill the log.
void Refuse(const char* field, int senderSlot) {
    static bool said = false;
    static uint64_t lastMs = 0;
    const uint64_t now = coop::net::NowMs();
    if (said && now - lastMs < 5000) return;
    said = true;
    lastMs = now;
    UE_LOGW("sleep_sync: SleepState refused -- %s (sender slot %d)", field, senderSlot);
}

// The host's name for the chat line; the ledger owns the fallback when the host's row has no nick.
void PushHostAsleepLine() {
    const std::string name = coop::text::ToUtf8(coop::roster_ledger::DisplayName(0));
    char buf[256];
    if (l10n::Fmt(buf, sizeof(buf), l10n::T("%1$s is asleep -- the world runs faster"), name.c_str()) < 0)
        return;
    // A status readout of a state that ends: reading it back hours later would be noise, not history.
    coop::chat_feed::Push(std::string(buf), coop::chat_feed::Keep::Transient);
}

void SendPeerRest(coop::net::Session* s, int toSlot, int slot) {
    SleepStatePayload p{};
    p.op = static_cast<uint8_t>(SleepOp::PeerRest);
    p.state = static_cast<uint8_t>(g_state[slot]);
    p.slot = static_cast<uint8_t>(slot);
    p.bed = g_bed[slot];
    if (toSlot < 0) s->SendReliable(coop::net::ReliableKind::SleepState, &p, sizeof(p));
    else s->SendReliableToSlot(toSlot, coop::net::ReliableKind::SleepState, &p, sizeof(p));
}

void SendHostAsleep(coop::net::Session* s, int toSlot) {
    SleepStatePayload p{};
    p.op = static_cast<uint8_t>(SleepOp::HostAsleep);
    p.state = g_hostSleeping ? 1 : 0;
    if (toSlot < 0) s->SendReliable(coop::net::ReliableKind::SleepState, &p, sizeof(p));
    else s->SendReliableToSlot(toSlot, coop::net::ReliableKind::SleepState, &p, sizeof(p));
}

// ---- the host's side of a client's reports --------------------------------------------------------
// The host's applications of a client's rest report, per sender (an instrument: the world_latest drill counts
// them).
coop::roster_ledger::PerSlotState<uint32_t> g_reportsApplied;

// A client's rest reports, the newest kept at a pace (coop/net/latest_wins.h). The state on the wire is a level,
// sent on its edge; a player's entries and exits peak at 1 a second and 3 in ten in the archived client logs, and
// a changed state is relayed to every client, so a sender past its burst costs every peer a message. A state still
// owed when the player leaves is dropped with the slot; one still owed otherwise is paid like any other: only a
// sender past its burst is ever owed, and it could send the same state again. MTA applies a client's element data
// at once (CGame.cpp:2757); paced here for the relay's cost.
constexpr coop::net::IntentBudget kRestBudget{4.0f, 1.0f};
struct PendingRest {
    SleepPeerState state = SleepPeerState::Awake;
    WireKey bed{};
    coop::net::LatestWins pace;
};
coop::roster_ledger::PerSlotState<PendingRest> g_pending;

bool HoldsRest(int slot, SleepPeerState state, const WireKey& bed) {
    return g_state[slot] == state && SameBed(g_bed[slot], bed);
}

// HOST: a client's changed rest state, stored and relayed to every client.
void ApplyRest(coop::net::Session* s, int slot, SleepPeerState state, const WireKey& bed) {
    g_state[slot] = state;
    g_bed[slot] = bed;
    ++g_reportsApplied[slot];
    SendPeerRest(s, -1, slot);
    UE_LOGI("sleep_sync: slot %d is now %d (bed '%.*s')", slot, static_cast<int>(state),
            static_cast<int>(bed.len), bed.data);
}

// ---- this peer's own state ------------------------------------------------------------------------
// The state of the gamemode now: dreaming over resting over awake. False while the flags do not resolve.
bool ReadLocal(SleepPeerState* state, bool* sleeping) {
    float asleep = 0.f, dreaming = 0.f;
    namespace V = ue_wrap::vitals;
    if (!V::Read(V::Field::Sleeping, &asleep) || !V::Read(V::Field::Dreaming, &dreaming)) return false;
    *sleeping = asleep != 0.f;
    *state = dreaming != 0.f ? SleepPeerState::Dreaming
                             : (*sleeping ? SleepPeerState::Resting : SleepPeerState::Awake);
    return true;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running()) return;
    if (!SLP::EnsureResolved()) return;
    void* gm = ue_wrap::world_singleton::Gamemode();
    if (!gm) return;

    // A fresh gamemode (boot, level reload) starts awake with its own dreamProbability; what was
    // published stays until the poll below compares it with the new one.
    if (gm != g_gmInst) {
        g_gmInst = gm;
        g_localState = SleepPeerState::Awake;
        std::memset(&g_localBed, 0, sizeof(g_localBed));
        g_localSleeping = false;
        g_dreamProbSuppressed = false;
    }

    SleepPeerState state = SleepPeerState::Awake;
    bool sleeping = false;
    if (!ReadLocal(&state, &sleeping)) return;
    if (state != g_localState || sleeping != g_localSleeping) {
        // The bed is read at the edge only: the gamemode sets it before isSleep rises and holds it for the
        // whole rest, and a read per tick would allocate.
        WireKey bed{};
        if (state != SleepPeerState::Awake) coop::net::WireKeyFromString(SLP::BedClassName(), bed);
        g_localState = state;
        g_localSleeping = sleeping;
        g_localBed = bed;
        UE_LOGI("sleep_sync: local state %d (sleeping %d, bed '%.*s')", static_cast<int>(state),
                sleeping ? 1 : 0, static_cast<int>(bed.len), bed.data);
    }

    if (IsHost(s)) {
        // The host's own state is published to every client on a change; while nobody is connected the
        // tables still follow it, and a joiner is sent them (QueueConnectBroadcastForSlot).
        if (sleeping != g_hostSleeping) {
            g_hostSleeping = sleeping;
            if (s->connected()) SendHostAsleep(s, -1);
        }
        if (!HoldsRest(0, g_localState, g_localBed)) {
            g_state[0] = g_localState;
            g_bed[0] = g_localBed;
            if (s->connected()) SendPeerRest(s, -1, 0);
        }
        return;
    }

    if (!s->connected()) return;

    // A client holds its nightmare rolls off for the session: a nightmare is the host's machine's roll alone.
    if (!g_dreamProbSuppressed && SLP::SetDreamProbability(0.f)) g_dreamProbSuppressed = true;

    if (g_localState != g_sentState || !SameBed(g_localBed, g_sentBed)) {
        SleepStatePayload p{};
        p.op = static_cast<uint8_t>(SleepOp::ClientRest);
        p.state = static_cast<uint8_t>(g_localState);
        p.bed = g_localBed;
        if (s->SendReliableToSlot(0, coop::net::ReliableKind::SleepState, &p, sizeof(p))) {
            g_sentState = g_localState;
            g_sentBed = g_localBed;
        }
    }
}

void OnReliable(const uint8_t* payload, size_t len, uint8_t senderSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) return;
    if (len != sizeof(SleepStatePayload)) return Refuse("length", senderSlot);
    SleepStatePayload p{};
    std::memcpy(&p, payload, sizeof(p));
    if (p.op > static_cast<uint8_t>(SleepOp::PeerRest)) return Refuse("op", senderSlot);
    if (p.state > static_cast<uint8_t>(p.op == static_cast<uint8_t>(SleepOp::HostAsleep)
                                           ? SleepPeerState::Resting
                                           : SleepPeerState::Dreaming))
        return Refuse("state", senderSlot);
    if (p.bed.len > sizeof(p.bed.data)) return Refuse("bed", senderSlot);

    if (IsHost(s)) {
        // The host takes a client's own state only, from the transport's sender slot.
        if (p.op != static_cast<uint8_t>(SleepOp::ClientRest)) return Refuse("op for the host", senderSlot);
        if (senderSlot == 0 || senderSlot >= coop::net::kMaxPeers) return Refuse("sender", senderSlot);
        if (p.slot != 0) return Refuse("slot", senderSlot);
        const SleepPeerState state = static_cast<SleepPeerState>(p.state);
        PendingRest& pend = g_pending[senderSlot];
        if (HoldsRest(senderSlot, state, p.bed)) {
            pend.pace.Withdraw();
            return;
        }
        pend.state = state;
        pend.bed = p.bed;
        if (pend.pace.Offer(kRestBudget, coop::net::NowMs())) ApplyRest(s, senderSlot, state, p.bed);
        return;
    }

    // A client takes the host's word only (the transport's slot 0).
    if (senderSlot != 0) return Refuse("sender", senderSlot);
    switch (static_cast<SleepOp>(p.op)) {
    case SleepOp::HostAsleep: {
        const bool now = p.state != 0;
        if (now && !g_hostSleeping) PushHostAsleepLine();
        if (now != g_hostSleeping) UE_LOGI("sleep_sync: the host is %s", now ? "asleep" : "awake");
        g_hostSleeping = now;
        break;
    }
    case SleepOp::PeerRest: {
        if (p.slot >= coop::net::kMaxPeers) return Refuse("slot", senderSlot);
        const SleepPeerState state = static_cast<SleepPeerState>(p.state);
        if (!HoldsRest(p.slot, state, p.bed)) {
            g_state[p.slot] = state;
            g_bed[p.slot] = p.bed;
            UE_LOGI("sleep_sync: peer %u is now %d (bed '%.*s')", static_cast<unsigned>(p.slot),
                    static_cast<int>(state), static_cast<int>(p.bed.len), p.bed.data);
        }
        break;
    }
    default:
        Refuse("op for a client", senderSlot);
        break;
    }
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !IsHost(s)) return;
    if (peerSlot <= 0 || peerSlot >= static_cast<int>(coop::net::kMaxPeers)) return;
    SendHostAsleep(s, peerSlot);
    for (int slot = 0; slot < static_cast<int>(coop::net::kMaxPeers); ++slot)
        if (g_state[slot] != SleepPeerState::Awake) SendPeerRest(s, peerSlot, slot);
}

void OnDisconnectForSlot(int slot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !IsHost(s)) return;
    if (slot <= 0 || slot >= static_cast<int>(coop::net::kMaxPeers)) return;
    g_pending[slot].pace.Withdraw();
    if (g_state[slot] == SleepPeerState::Awake) return;  // every client already holds it awake
    g_state[slot] = SleepPeerState::Awake;
    std::memset(&g_bed[slot], 0, sizeof(WireKey));
    SendPeerRest(s, -1, slot);
}

SleepPeerState PeerState(int slot) {
    return (slot >= 0 && slot < static_cast<int>(coop::net::kMaxPeers)) ? g_state[slot]
                                                                         : SleepPeerState::Awake;
}

bool HostAsleepNow() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (s && !IsHost(s)) return g_hostSleeping;
    return SLP::IsSleeping();
}

void PayOwed(coop::net::Session& session) {
    if (session.role() != coop::net::Role::Host) return;
    uint64_t now = 0;  // read once, and only when a report is owed
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers); ++slot) {
        PendingRest& pend = g_pending[slot];
        if (!pend.pace.Owed()) continue;
        if (HoldsRest(slot, pend.state, pend.bed)) {  // the slot's own arrival or leaving got there first
            pend.pace.Withdraw();
            continue;
        }
        if (now == 0) now = coop::net::NowMs();
        if (pend.pace.Due(kRestBudget, now)) ApplyRest(&session, slot, pend.state, pend.bed);
    }
}

uint32_t ReportsApplied(int slot) { return g_reportsApplied[slot]; }

bool LocalInBed() { return g_localState != SleepPeerState::Awake; }

void OnDisconnect() {
    // Restore the SP nightmare sentinel. A peer still asleep keeps sleeping: with the session gone its
    // world is single-player again and the native timelapse is exactly the stock behaviour.
    if (g_dreamProbSuppressed) {
        SLP::SetDreamProbability(-1.f);
        g_dreamProbSuppressed = false;
    }
    g_localState = SleepPeerState::Awake;
    std::memset(&g_localBed, 0, sizeof(g_localBed));
    g_localSleeping = false;
    g_gmInst = nullptr;
    for (auto& st : g_state) st = SleepPeerState::Awake;
    std::memset(g_bed, 0, sizeof(g_bed));
    g_hostSleeping = false;
    g_sentState = SleepPeerState::Awake;
    std::memset(&g_sentBed, 0, sizeof(g_sentBed));
}

}  // namespace coop::sleep_sync
