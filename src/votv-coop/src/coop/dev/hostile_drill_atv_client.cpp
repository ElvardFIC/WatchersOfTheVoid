// coop/dev/hostile_drill_atv_client.cpp -- the atv_ family's sender: see coop/dev/hostile_drill_atv.h. The client says it
// is ready, mutes its own honest ATV stream (so the host's counters hold only what the drill sent), and for each phase
// waits for the host's go line with the ATV keys, waits for its table to show them, then sends the phase's raw
// AtvState, AtvSeat, AtvTakeover or AtvExplodeRequest payloads through the session's own send, bypassing the ATV lane.

#include "hostile_drill_atv_detail.h"  // co-located private header (src tree, not include/)

#include "atv_health_drill_detail.h"   // StreamsRun (co-located private header)
#include "explosion_drill_source.h"    // Guard (co-located private header)

#include "coop/dev/drill_chat.h"
#include "coop/interactables/atv_sync.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/devices/atv.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace coop::dev::hostile_drill::atv::detail {
namespace {

namespace A  = ue_wrap::atv;
namespace DC = coop::dev::drill_chat;
namespace XD = coop::dev::explosion_drill::detail;
using coop::atv_sync::TableView;
using Kind = coop::net::ReliableKind;

constexpr uint32_t kBlocks = 20;            // the state arm's forged blocks, sent in one tick
constexpr float    kForgedOffsetCm = 600.f; // how far from the host's copy a forged block claims the ATV is
constexpr uint8_t  kSeqFrom = 200;          // the raw requests' numbers: an honest request never meets them here
constexpr const wchar_t* kNoSuchAtv = L"hostile_no_such_atv";

enum class CStep : uint8_t { Run, Done };
CStep             g_step = CStep::Run;
bool              g_idleSaid = false, g_muted = false, g_guarded = false, g_readySaid = false, g_sitSent = false;
int               g_phase = 1;
Clock::time_point g_since{};
const char*       g_waiting = nullptr;
std::vector<std::wstring> g_keys;           // the current phase's, from the host's go line
uint8_t           g_seq = kSeqFrom;

void Inconclusive(Arm a, const std::string& why) {
    Verdict(a, "INCONCLUSIVE", why);
    g_step = CStep::Done;
}

// True once the wait named `what` has run past the failure bound; a different wait restarts the clock.
bool Late(const char* what) {
    if (!g_waiting || std::strcmp(g_waiting, what) != 0) {
        g_waiting = what;
        g_since = Clock::now();
    }
    return Clock::now() - g_since > kFailBound;
}

// One forged run, counting what it sent; a send that fails stops the run.
struct Out {
    coop::net::Session* s;
    bool                ok = true;
    void Seat(const std::wstring& key, uint8_t seat, uint8_t action) {
        coop::net::AtvSeatPayload p{};
        coop::net::WireKeyFromString(key, p.key);
        p.seat = seat;
        p.action = action;
        p.seq = ++g_seq;
        ok = ok && s->SendReliable(Kind::AtvSeat, &p, sizeof(p));
    }
    void Takeover(const std::wstring& key) {
        coop::net::AtvTakeoverPayload p{};
        coop::net::WireKeyFromString(key, p.key);
        p.seq = ++g_seq;
        ok = ok && s->SendReliable(Kind::AtvTakeover, &p, sizeof(p));
    }
    void Explode(const std::wstring& key, uint8_t pad0) {
        coop::net::AtvExplodeRequestPayload p{};
        coop::net::WireKeyFromString(key, p.key);
        p.pad[0] = pad0;
        ok = ok && s->SendReliable(Kind::AtvExplodeRequest, &p, sizeof(p));
    }
    // A block that claims the seat and the syncing for this client and a pose off the ATV's own, every other field
    // plain: the host overwrites the stamp and, past its sender test, would apply the rest.
    void State(const std::wstring& key, const ue_wrap::FVector& at, const ue_wrap::FRotator& rot) {
        coop::net::AtvStatePayload p{};
        coop::net::WireKeyFromString(key, p.key);
        p.x = at.X + kForgedOffsetCm;
        p.y = at.Y;
        p.z = at.Z;
        p.pitch = rot.Pitch;
        p.yaw = rot.Yaw;
        p.roll = rot.Roll;
        p.occupantSlot = kSlot;
        p.syncerSlot = kSlot;
        p.fuel = 100.f;
        p.health = 100.f;
        ok = ok && s->SendReliable(Kind::AtvState, &p, sizeof(p));
    }
};

// The keys of the go line: the text after the prefix, split on spaces.
void ParseKeys(const std::string& rest) {
    g_keys.clear();
    size_t at = 0;
    while (at < rest.size()) {
        const size_t sp = rest.find(' ', at);
        const size_t end = sp == std::string::npos ? rest.size() : sp;
        if (end > at) g_keys.emplace_back(rest.begin() + static_cast<std::ptrdiff_t>(at),
                                          rest.begin() + static_cast<std::ptrdiff_t>(end));
        at = end + 1;
    }
}

// How many keys the phase addresses (the go line carries every ATV spawned so far).
size_t KeysNeeded(Arm a, int n) {
    switch (a) {
    case Arm::SeatForge:     return n >= 3 ? 2 : 1;
    case Arm::TakeoverForge: return n == 2 ? 4 : 1;
    default:                 return 1;
    }
}

enum class Sent : uint8_t { Wait, Done, Failed };

// The explode arm's third phase: the client's own admitted sit in ATV E, then the wait for the host's stamp to name this
// client seat and syncer in its table. The two requests follow in phase 4, once the sit is the host's table.
Sent SendExplodeSit(Out& o, const std::wstring& key, uint8_t self) {
    if (!g_sitSent) {
        o.Seat(key, 0, 1);
        g_sitSent = true;
        return o.ok ? Sent::Wait : Sent::Failed;
    }
    TableView t;
    if (!coop::atv_sync::ReadTable(key, t) || t.occupantSlot != self || t.syncerSlot != self) {
        if (Late("the host's stamp naming this client the seat and the syncer")) return Sent::Failed;
        return Sent::Wait;
    }
    return Sent::Done;
}

// The phase's forged messages. Wait while a step the phase owes is not in yet.
Sent SendPhase(Out& o, Arm a, int n, uint8_t self) {
    const std::wstring& k0 = g_keys[0];
    switch (a) {
    case Arm::StateForge: {
        TableView t;
        ue_wrap::FVector at{};
        ue_wrap::FRotator rot{};
        if (!coop::atv_sync::ReadTable(k0, t) || !A::GetRootTransform(t.actor, at, rot)) return Sent::Failed;
        for (uint32_t i = 0; i < kBlocks; ++i) o.State(k0, at, rot);
        break;
    }
    case Arm::SeatForge:
        if (n == 1) {
            o.Seat(k0, 0, 2);            // an action the wire does not have
            o.Seat(k0, 1, 1);            // the passenger seat, which does not exist
            o.Seat(kNoSuchAtv, 0, 1);    // an ATV nobody spawned
            o.Seat(k0, 0, 0);            // a leave while not seated
        } else if (n == 2) {
            o.Seat(k0, 0, 1);            // a sit in the ATV 20 m off
        } else if (n == 3) {
            for (int i = 0; i < 5; ++i) o.Seat(g_keys[1], 0, 1);   // five sits in reach in one tick
        } else {
            o.Seat(g_keys[1], 0, 0);     // the leave
        }
        break;
    case Arm::TakeoverForge:
        if (n == 1) {
            for (int i = 0; i < 6; ++i) o.Takeover(k0);   // the ATV the host sits in
        } else {
            for (int i = 1; i < 4; ++i) o.Takeover(g_keys[static_cast<size_t>(i)]);   // three parked ATVs in reach
        }
        break;
    case Arm::ExplodeForge:
        if (n == 1) o.Explode(k0, 0);            // an ATV this client does not sync
        else if (n == 2) o.Explode(k0, 1);       // a pad byte set
        else if (n == 3) return SendExplodeSit(o, k0, self);   // the client's own seat, the syncer it makes
        else { o.Explode(k0, 0); o.Explode(k0, 0); }           // two in one tick: a blast, then `blown`
        break;
    default:
        return Sent::Failed;
    }
    return o.ok ? Sent::Done : Sent::Failed;
}

// The phase's keys are in this client's table: its copy of each ATV exists before a message names it.
bool TablesHold() {
    for (const std::wstring& k : g_keys) {
        TableView t;
        if (!coop::atv_sync::ReadTable(k, t)) return false;
    }
    return true;
}

void NextPhase() {
    g_keys.clear();
    g_sitSent = false;
    g_waiting = nullptr;
    ++g_phase;
}

}  // namespace

void ClientTick(coop::net::Session* s, Arm a) {
    if (g_step == CStep::Done) return;
    if (!coop::dev::atv_health_drill::detail::StreamsRun()) return;
    const uint8_t self = coop::players::Registry::Get().LocalPeerId();
    if (!coop::players::Registry::Get().Local() || self == 0 || self >= coop::players::kMaxPeers) return;
    if (self != kSlot) {
        if (!g_idleSaid) {
            g_idleSaid = true;
            UE_LOGI("[HOSTILE-DRILL] %s idles: this client is slot %u, the family acts on slot %u", ArmName(a),
                    static_cast<unsigned>(self), static_cast<unsigned>(kSlot));
        }
        g_step = CStep::Done;
        return;
    }
    if (!g_muted) {   // the honest stream would feed the host's counters the drill judges
        coop::atv_sync::SetDrillMute(true);
        g_muted = true;
    }
    if (a == Arm::ExplodeForge && !g_guarded) {
        XD::g_src.host = false;
        if (XD::Guard(false)) {
            if (Late("the guard (startInvinc, unlockFungunsInStory) to resolve"))
                Inconclusive(a, "the guard did not resolve");
            return;
        }
        g_guarded = true;
    }
    if (!g_readySaid) {
        g_readySaid = true;
        DC::Say(ClientReadyLine(a));
        return;
    }
    if (g_keys.empty()) {
        std::string rest;
        if (DC::PollDue() && DC::LiveRestOf(GoPrefix(a, g_phase), &rest)) ParseKeys(rest);
        if (g_keys.empty()) {
            if (Late("the host's go line")) Inconclusive(a, "the host never said go for phase " + std::to_string(g_phase));
            return;
        }
        g_waiting = nullptr;
    }
    if (g_keys.size() < KeysNeeded(a, g_phase)) return Inconclusive(a, "the host's go line carries too few keys");
    if (!TablesHold()) {
        if (Late("this client's table to show the host's ATVs")) Inconclusive(a, "the host's ATVs never entered this table");
        return;
    }
    Out o{s};
    switch (SendPhase(o, a, g_phase, self)) {
    case Sent::Wait:
        return;
    case Sent::Failed:
        g_step = CStep::Done;
        return Verdict(a, "FAIL", "client: a forged message was not sent or the step it waited on never came");
    case Sent::Done:
        break;
    }
    DC::Say(SentLine(a, g_phase));
    UE_LOGI("[HOSTILE-DRILL] %s sent phase %d of %d", ArmName(a), g_phase, PhaseCount(a));
    if (g_phase >= PhaseCount(a)) {
        g_step = CStep::Done;
        return;
    }
    NextPhase();
}

void ClientReset() {
    g_step = CStep::Run;
    g_idleSaid = g_readySaid = g_sitSent = false;
    g_phase = 1;
    g_since = {};
    g_waiting = nullptr;
    g_keys.clear();
    g_seq = kSeqFrom;
    if (g_muted) {
        coop::atv_sync::SetDrillMute(false);
        g_muted = false;
    }
    if (g_guarded) {
        XD::Unguard();
        g_guarded = false;
    }
}

}  // namespace coop::dev::hostile_drill::atv::detail
