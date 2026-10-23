// coop/player/sleep_sync.cpp -- see coop/player/sleep_sync.h.

#include "coop/player/sleep_sync.h"

#include "coop/comms/chat_feed.h"
#include "coop/net/latest_wins.h"
#include "coop/net/net_clock.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/roster_ledger.h"  // PerSlotState, DisplayName
#include "coop/text/utf8_codec.h"
#include "l10n/l10n.h"

#include "ue_wrap/actors/sleep.h"
#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/world_singleton.h"

#include <atomic>
#include <array>
#include <cmath>
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

// ---- what is known of every peer ------------------------------------------------------------------
// The host: each slot's state as it applied it (slot 0 the host's own, published). A client: as the host
// relayed it.
SleepPeerState g_state[coop::net::kMaxPeers] = {};
WireKey        g_bed[coop::net::kMaxPeers] = {};
bool           g_hostSleeping = false;    // the host: its isSleep as last published. A client: the host's last flag.
// A client: the state last sent to the host.
SleepPeerState g_sentState = SleepPeerState::Awake;
WireKey        g_sentBed{};
bool g_haveSent=false;
uint64_t g_clientSendSerial=0;
uint64_t g_epoch=0,g_revision=0,g_serial=0;
uint32_t g_world=0;
coop::net::Role g_role=coop::net::Role::Client;
std::array<uint32_t,coop::net::kMaxPeers> g_generation{};
std::array<bool,coop::net::kMaxPeers> g_ready{};
std::array<uint64_t,coop::net::kMaxPeers> g_hostDebt{};
std::array<uint64_t,coop::net::kMaxPeers> g_nextTry{};
std::array<std::array<uint64_t,coop::net::kMaxPeers>,coop::net::kMaxPeers> g_restDebt{};
bool g_flushing=false;

bool IsHost(coop::net::Session* s) { return s->role() == coop::net::Role::Host; }

bool Current(coop::net::Session* s,uint64_t epoch) {
    return s && g_session.load()==s && epoch==g_epoch && s->running() &&
        g_world==ue_wrap::world_identity::Generation() && g_serial==coop::net::session_serial::Current() && s->role()==g_role;
}

void RefreshContext(coop::net::Session* s) {
    if(s && (g_world!=ue_wrap::world_identity::Generation() ||
       g_serial!=coop::net::session_serial::Current() || s->role()!=g_role))Install(s);
}

// Two beds are the same name: the length and the bytes inside it, whatever sits past them.
bool SameBed(const WireKey& a, const WireKey& b) {
    return a.len == b.len && a.len <= sizeof(a.data) && std::memcmp(a.data, b.data, a.len) == 0;
}

// One warning per refused field, at most one every five seconds each: a hostile sender cannot fill the log, and
// a flood on one field does not hide another's refusal. The fields are the literals this file passes; a table
// that ran out would share its last row, which no caller here can reach.
void Refuse(const char* field, int senderSlot) {
    struct Said { const char* field; bool said; uint64_t lastMs; };
    static Said table[12] = {};
    const uint64_t now = coop::net::NowMs();
    Said* row = nullptr;
    for (Said& e : table) {
        if (e.field && std::strcmp(e.field, field) == 0) { row = &e; break; }
        if (!e.field) { row = &e; e.field = field; break; }
    }
    if (!row) row = &table[11];
    if (row->said && now - row->lastMs < 5000) return;
    row->said = true;
    row->lastMs = now;
    UE_LOGW("sleep_sync: SleepState refused -- %s (sender slot %d)", field, senderSlot);
}

// The bed refusals, counted per sender and reason for the hostile drill (coop/dev/hostile_drill_world).
struct BedRefusals { uint32_t n[4] = {}; };
coop::roster_ledger::PerSlotState<BedRefusals> g_bedRefused;

void RefuseBed(BedRefusal why, int senderSlot) {
    if (senderSlot > 0 && senderSlot < static_cast<int>(coop::net::kMaxPeers))
        ++g_bedRefused[senderSlot].n[static_cast<int>(why)];
    Refuse("bed", senderSlot);
}

bool BedFaulty(const WireKey& bed, BedRefusal* why) {
    if (bed.len > sizeof(bed.data)) { if (why) *why = BedRefusal::Length; return true; }
    for (size_t i = 0; i < bed.len; ++i) {
        const unsigned char c = static_cast<unsigned char>(bed.data[i]);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            if (why) *why = BedRefusal::Charset;
            return true;
        }
    }
    for (size_t i = bed.len; i < sizeof(bed.data); ++i) {
        if (bed.data[i] != 0) { if (why) *why = BedRefusal::Tail; return true; }
    }
    return false;
}

// The host's name for the chat line; the ledger owns the fallback when the host's row has no nick.
void PushHostAsleepLine() {
    const std::string name = coop::text::ToUtf8(coop::roster_ledger::DisplayName(0));
    char buf[256];
    if (l10n::Fmt(buf, sizeof(buf), l10n::T("%1$s is asleep"), name.c_str()) < 0)
        return;
    // A status readout of a state that ends: reading it back hours later would be noise, not history.
    coop::chat_feed::Push(std::string(buf), coop::chat_feed::Keep::Transient);
}

// Delivery stores current-state debt, never an unbounded history of sleep edges.
// Reentrant changes supersede the observed revision; old sends cannot clear them.
void FlushHost(coop::net::Session* s) {
    if(g_flushing || !s || !IsHost(s) || !s->connected())return;
    g_flushing=true;
    struct Guard{~Guard(){g_flushing=false;}} guard;
    const auto epoch=g_epoch;
    const auto now=coop::net::NowMs();
    for(int to=1;to<coop::net::kMaxPeers;++to) {
        const auto generation=g_generation[to];
        const auto recipient=[&]{return Current(s,epoch) && s->connected() && s->IsSlotWorldReady(to) &&
            s->IsSlotConnected(to) && generation==s->peerGenerationForSlot(to);};
        if(!Current(s,epoch))return;
        if(!recipient() || now<g_nextTry[to])continue;
        const auto hostRevision=g_hostDebt[to];
        if(hostRevision) {
            SleepStatePayload p{};p.op=uint8_t(SleepOp::HostAsleep);p.state=g_hostSleeping?1:0;
            const bool sent=s->SendReliableToSlot(to,coop::net::ReliableKind::SleepState,&p,sizeof(p));
            if(!recipient())continue;
            if(sent && g_hostDebt[to]==hostRevision)g_hostDebt[to]=0;
            if(!sent){g_nextTry[to]=now+100;continue;}
        }
        for(int slot=0;slot<coop::net::kMaxPeers;++slot) {
            if(!recipient())break;
            const auto revision=g_restDebt[to][slot];if(!revision)continue;
            SleepStatePayload p{};p.op=uint8_t(SleepOp::PeerRest);p.state=uint8_t(g_state[slot]);
            p.slot=uint8_t(slot);p.bed=g_bed[slot];
            const bool sent=s->SendReliableToSlot(to,coop::net::ReliableKind::SleepState,&p,sizeof(p));
            if(!recipient())break;
            if(sent && g_restDebt[to][slot]==revision)g_restDebt[to][slot]=0;
            if(!sent){g_nextTry[to]=now+100;break;}
        }
    }
}

// To one slot, or (toSlot < 0) to every client but `exceptSlot` (-1: none).
void SendPeerRest(coop::net::Session* s, int toSlot, int slot, int exceptSlot = -1) {
    const auto revision=++g_revision;
    for (int to = 1; to < static_cast<int>(coop::net::kMaxPeers); ++to)
        if ((toSlot<0 || to==toSlot) && to!=exceptSlot && s->IsSlotWorldReady(to))g_restDebt[to][slot]=revision;
    FlushHost(s);
}

void SendHostAsleep(coop::net::Session* s, int toSlot) {
    const auto revision=++g_revision;
    for(int to=1;to<coop::net::kMaxPeers;++to)
        if((toSlot<0 || to==toSlot) && s->IsSlotWorldReady(to))g_hostDebt[to]=revision;
    FlushHost(s);
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

void SeedSlot(int to) {
    g_hostDebt[to]=++g_revision;
    for(int slot=0;slot<coop::net::kMaxPeers;++slot)g_restDebt[to][slot]=++g_revision;
}

// Heal a missed leave edge and readiness transitions before accepting reports.
void ConformPeers(coop::net::Session* s) {
    for(int slot=1;slot<coop::net::kMaxPeers;++slot) {
        const bool connected=s->IsSlotConnected(slot),ready=connected && s->IsSlotWorldReady(slot);
        const auto generation=connected?s->peerGenerationForSlot(slot):0;
        if(generation!=g_generation[slot] || !connected || (g_ready[slot] && !ready)) {
            const bool changed=g_state[slot]!=SleepPeerState::Awake || g_bed[slot].len;
            g_generation[slot]=generation;g_ready[slot]=false;
            g_state[slot]=SleepPeerState::Awake;g_bed[slot]={};g_pending[slot]={};
            g_hostDebt[slot]=0;g_restDebt[slot]={};g_nextTry[slot]=0;
            if(changed)for(int to=1;to<coop::net::kMaxPeers;++to)
                if(to!=slot && s->IsSlotWorldReady(to))g_restDebt[to][slot]=++g_revision;
        }
        if(!ready)g_pending[slot]={};
        if(ready && !g_ready[slot])SeedSlot(slot);
        g_ready[slot]=ready;
    }
}

bool HoldsRest(int slot, SleepPeerState state, const WireKey& bed) {
    return g_state[slot] == state && SameBed(g_bed[slot], bed);
}

// HOST: a client's changed rest state, stored and relayed to every client but the reporter, as MTA's element-data
// relay skips its source (CGame.cpp:2761, the skip at :2768). MTA relays to joined players only; ours is the same
// through SendReliableToSlot's pre-world gate (coop/net/session.cpp:99).
void ApplyRest(coop::net::Session* s, int slot, SleepPeerState state, const WireKey& bed) {
    g_state[slot] = state;
    g_bed[slot] = bed;
    ++g_reportsApplied[slot];
    SendPeerRest(s, -1, slot, slot);
    UE_LOGI("sleep_sync: slot %d is now %d (bed '%.*s')", slot, static_cast<int>(state),
            static_cast<int>(bed.len), bed.data);
}

// ---- this peer's own state ------------------------------------------------------------------------
// The state of the gamemode now: dreaming over resting over awake. False while the flags do not resolve.
bool ReadLocal(SleepPeerState* state, bool* sleeping) {
    float asleep = 0.f, dreaming = 0.f;
    namespace V = ue_wrap::vitals;
    if (!V::Read(V::Field::Sleeping, &asleep) || !V::Read(V::Field::Dreaming, &dreaming)) return false;
    if(!std::isfinite(asleep) || !std::isfinite(dreaming))return false;
    *sleeping = asleep != 0.f;
    *state = dreaming != 0.f ? SleepPeerState::Dreaming
                             : (*sleeping ? SleepPeerState::Resting : SleepPeerState::Awake);
    return true;
}

// A CLIENT: this peer's polled state to the host. Recorded as sent only when the transport took it.
void SendClientRest(coop::net::Session* s) {
    SleepStatePayload p{};
    p.op = static_cast<uint8_t>(SleepOp::ClientRest);
    p.state = static_cast<uint8_t>(g_localState);
    p.bed = g_localBed;
    const auto epoch=g_epoch;
    const auto serial=++g_clientSendSerial;
    if (s->SendReliableToSlot(0, coop::net::ReliableKind::SleepState, &p, sizeof(p)) && Current(s,epoch) &&
        s->connected() && !IsHost(s) && serial==g_clientSendSerial) {
        g_sentState = static_cast<SleepPeerState>(p.state);
        g_sentBed = p.bed;g_haveSent=true;
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    if(g_session.load()!=session || (session && (g_world!=ue_wrap::world_identity::Generation() ||
       g_serial!=coop::net::session_serial::Current() || g_role!=session->role())))OnDisconnect();
    g_world=ue_wrap::world_identity::Generation();g_serial=coop::net::session_serial::Current();
    if(session)g_role=session->role();
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running()) return;
    RefreshContext(s);
    if(IsHost(s)){ConformPeers(s);FlushHost(s);}
    const auto epoch=g_epoch;
    if (!SLP::EnsureResolved()) return;

    SleepPeerState state = SleepPeerState::Awake;
    bool sleeping = false;
    if (!ReadLocal(&state, &sleeping)) return;
    if(!Current(s,epoch))return;
    if (state != g_localState || sleeping != g_localSleeping) {
        // The bed is read at the edge only: the gamemode sets it before isSleep rises, and a read per tick
        // would allocate. Into a dream the bed is kept as published: the non-ariral wakeup() clears
        // gamemode.bed before the dream, so a read there finds none.
        WireKey bed{};
        if (state == SleepPeerState::Dreaming && g_localState != SleepPeerState::Awake) bed = g_localBed;
        else if (state != SleepPeerState::Awake) coop::net::WireKeyFromString(SLP::BedClassName(), bed);
        if(!Current(s,epoch))return;
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
            if(!Current(s,epoch))return;
        }
        if (!HoldsRest(0, g_localState, g_localBed)) {
            g_state[0] = g_localState;
            g_bed[0] = g_localBed;
            if (s->connected()) SendPeerRest(s, -1, 0);
        }
        return;
    }

    if (!s->connected()) return;

    if (!g_haveSent || g_localState != g_sentState || !SameBed(g_localBed, g_sentBed)) SendClientRest(s);
}

void OnReliable(const uint8_t* payload, size_t len, uint8_t senderSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running() || !s->connected()) return;
    RefreshContext(s);
    if (len != sizeof(SleepStatePayload) || !payload) return Refuse("length", senderSlot);
    SleepStatePayload p{};
    std::memcpy(&p, payload, sizeof(p));
    if(p._pad)return Refuse("padding",senderSlot);
    if (p.op > static_cast<uint8_t>(SleepOp::PeerRest)) return Refuse("op", senderSlot);
    if (p.state > static_cast<uint8_t>(p.op == static_cast<uint8_t>(SleepOp::HostAsleep)
                                           ? SleepPeerState::Resting
                                           : SleepPeerState::Dreaming))
        return Refuse("state", senderSlot);
    BedRefusal fault{};
    if (BedFaulty(p.bed, &fault)) return RefuseBed(fault, senderSlot);
    if(p.op!=uint8_t(SleepOp::HostAsleep) && p.state==uint8_t(SleepPeerState::Awake) && p.bed.len)
        return RefuseBed(BedRefusal::AwakeWithBed,senderSlot);
    if (p.op == static_cast<uint8_t>(SleepOp::HostAsleep)) {
        if (p.slot != 0) return Refuse("slot", senderSlot);
        if (p.bed.len != 0) return Refuse("bed", senderSlot);
    }

    if (IsHost(s)) {
        // The host takes a client's own state only, from the transport's sender slot.
        if (p.op != static_cast<uint8_t>(SleepOp::ClientRest)) return Refuse("op for the host", senderSlot);
        if (senderSlot == 0 || senderSlot >= coop::net::kMaxPeers) return Refuse("sender", senderSlot);
        if (p.slot != 0) return Refuse("slot", senderSlot);
        // MTA acts on a joined player's reports only (CGame.cpp:2376; coop/interactables/dish_calib_sync.cpp).
        if (!s->IsSlotWorldReady(senderSlot)) return Refuse("slot", senderSlot);
        ConformPeers(s);
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
        // The host refuses a rest from a slot it does not yet hold world-ready, so a rest sent early is sent
        // again here (the connect broadcast reaches this slot only once it is); a held one is dropped there.
        if (g_localState != SleepPeerState::Awake) SendClientRest(s);
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

void OnDreamScene(const coop::net::DreamScenePayload& /*p*/, uint8_t /*senderSlot*/) {
    // Reserved legacy packet. mainGamemode's native in-sleep roll checks isSleep,
    // nightmaresEnabled and bedSleepProb(), then calls wakeup() and createDream()
    // only on the peer whose roll won. createDream selects/processes the scene,
    // records total_dreams, moves that local pawn and sets the local dream phase;
    // native leaveDream returns the pawn and calls the selected dream's awoken().
    // Replaying another peer's scene would bypass its own conditions and duplicate
    // local progression/reward callbacks. SleepState carries presentation state;
    // every peer keeps its own native dream and Insomniac path.
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running() || !s->connected() || !IsHost(s)) return;
    if (peerSlot <= 0 || peerSlot >= static_cast<int>(coop::net::kMaxPeers)) return;
    RefreshContext(s);ConformPeers(s);
    if(!s->IsSlotWorldReady(peerSlot))return;
    SeedSlot(peerSlot);FlushHost(s);
}

void OnDisconnectForSlot(int slot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !IsHost(s)) return;
    if (slot <= 0 || slot >= static_cast<int>(coop::net::kMaxPeers)) return;
    g_pending[slot]={};g_generation[slot]=0;g_ready[slot]=false;
    g_hostDebt[slot]=0;g_restDebt[slot]={};g_nextTry[slot]=0;
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

bool AllPlayersResting() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running() || !IsHost(s))return false;
    RefreshContext(s);
    SleepPeerState local{};bool sleeping=false;
    if(!ReadLocal(&local,&sleeping) || local!=SleepPeerState::Resting || !sleeping)return false;
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers); ++slot) {
        if (!s->IsSlotConnected(slot)) continue;
        if (!s->IsSlotWorldReady(slot) || g_generation[slot]!=s->peerGenerationForSlot(slot) ||
            g_state[slot] != SleepPeerState::Resting ||
            (g_pending[slot].pace.Owed() && g_pending[slot].state!=SleepPeerState::Resting)) return false;
    }
    return true;
}

void PayOwed(coop::net::Session& session) {
    if(g_session.load()!=&session || !session.running() || !session.connected() || !IsHost(&session))return;
    RefreshContext(&session);ConformPeers(&session);
    const auto epoch=g_epoch;
    uint64_t now = 0;  // read once, and only when a report is owed
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers); ++slot) {
        if(!Current(&session,epoch))return;
        PendingRest& pend = g_pending[slot];
        if (!pend.pace.Owed()) continue;
        if (HoldsRest(slot, pend.state, pend.bed)) {  // the slot's own arrival or leaving got there first
            pend.pace.Withdraw();
            continue;
        }
        if (now == 0) now = coop::net::NowMs();
        if (pend.pace.Due(kRestBudget, now)) {
            const auto state=pend.state;const auto bed=pend.bed;
            ApplyRest(&session, slot, state, bed);
        }
    }
    FlushHost(&session);
}

uint32_t ReportsApplied(int slot) { return g_reportsApplied[slot]; }

uint32_t BedRefusalsFrom(int slot, BedRefusal why) {
    return g_bedRefused[slot].n[static_cast<int>(why)];
}

bool LocalInBed() { return g_localState != SleepPeerState::Awake; }

void OnDisconnect() {
    ++g_epoch;g_session.store(nullptr,std::memory_order_release);
    g_haveSent=false;g_generation={};g_ready={};g_hostDebt={};g_restDebt={};g_nextTry={};
    for(int slot=0;slot<coop::net::kMaxPeers;++slot){g_pending[slot]={};g_reportsApplied[slot]=0;g_bedRefused[slot]={};}
    // Nightmare probability and sleep entry stay native on every peer. The
    // reports below carry presentation state, never another player's dream roll.
    g_localState = SleepPeerState::Awake;
    std::memset(&g_localBed, 0, sizeof(g_localBed));
    g_localSleeping = false;
    for (auto& st : g_state) st = SleepPeerState::Awake;
    std::memset(g_bed, 0, sizeof(g_bed));
    g_hostSleeping = false;
    g_sentState = SleepPeerState::Awake;
    std::memset(&g_sentBed, 0, sizeof(g_sentBed));
}

}  // namespace coop::sleep_sync
