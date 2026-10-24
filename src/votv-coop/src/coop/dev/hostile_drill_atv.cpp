// coop/dev/hostile_drill_atv.cpp -- see coop/dev/hostile_drill_atv.h. The arm table, the phases of each arm, the verdict
// print, the lobby-chat lines and the family's entry points; the client's sender is hostile_drill_atv_client.cpp and the
// host's setup and judge hostile_drill_atv_host.cpp.

#include "coop/dev/hostile_drill_atv.h"

#include "hostile_drill_atv_detail.h"  // co-located private header (src tree, not include/)

#include "ue_wrap/core/log.h"

#include <cstring>

namespace coop::dev::hostile_drill::atv {
namespace detail {
namespace {

using coop::net::AtvRefusal;
using Refusal = coop::explosion_request::Refusal;

struct ArmRow {
    const char* name;
    Arm         arm;
};
const ArmRow kArms[] = {
    {"atv_state_forge",    Arm::StateForge},
    {"atv_seat_forge",     Arm::SeatForge},
    {"atv_takeover_forge", Arm::TakeoverForge},
    {"atv_explode_forge",  Arm::ExplodeForge},
};

// The wait for the seat bucket to hand back the token the out-of-reach sit took (one per second, with a margin).
constexpr uint32_t kSeatRefillMs = 1100;

// A prep with no spawn and no sit.
constexpr Prep kNothing{{}, 0, false};

Expect NotSyncerBurst() {
    Expect e;
    e.notSyncer = 20;
    return e;
}
Expect Admitted(uint32_t n) {
    Expect e;
    e.admitted = n;
    return e;
}

// atv_state_forge: 20 raw blocks for an ATV the host syncs, every one counted `not_syncer`.
const std::vector<Phase>& StatePhases() {
    static const std::vector<Phase> p = {
        {Prep{{Spawn{0, false, 0}}, 1, false}, NotSyncerBurst()},
    };
    return p;
}

// atv_seat_forge: the form's four refusals; a sit from an ATV 20 m off; five sits in one tick in reach; the leave. The
// five sits want the host's seat bucket (4 tokens, one back per second) full, and the sit from far took one: phase 3's go
// waits kSeatRefillMs after phase 2 was judged. The bucket is the host's own state and is not readable here.
const std::vector<Phase>& SeatPhases() {
    static const std::vector<Phase> p = {
        {Prep{{Spawn{0, true, 0}}, 1, false},
         Expect().R(AtvRefusal::Malformed).R(AtvRefusal::NoSeat).R(AtvRefusal::UnknownAtv).R(AtvRefusal::NotOccupant)},
        {kNothing, Expect().R(AtvRefusal::OutOfReach)},
        {Prep{{Spawn{1, false, 2}}, 1, false, kSeatRefillMs},
         Admitted(1).R(AtvRefusal::AlreadySeated, 3).R(AtvRefusal::SeatPaced)},
        {kNothing, Admitted(1)},
    };
    return p;
}

// atv_takeover_forge: six takeovers of an ATV the host sits in; then three parked ATVs in reach, two granted and the
// third past the sender's pace.
const std::vector<Phase>& TakeoverPhases() {
    static const std::vector<Phase> p = {
        {Prep{{Spawn{0, false, 0}}, 1, true}, Expect().R(AtvRefusal::Seated, 6)},
        {Prep{{Spawn{1, false, 2}, Spawn{2, false, 4}, Spawn{3, false, 6}}, 3, false},
         Admitted(2).R(AtvRefusal::TakeoverPaced)},
    };
    return p;
}

// atv_explode_forge: a request for an ATV the host syncs; a request with a pad byte; the client's own seat (an admitted
// sit); then two requests in one tick, the first a blast and the second `blown`. Phase 4 is judged from its go: the
// host's own blast may kill the client's seated copy, whose honest request is a real `blown` too. One such honest
// `blown` from the source blast is expected and not counted (blownSlack); the drill's two are the accepted one and the
// first `blown`.
const std::vector<Phase>& ExplodePhases() {
    static const std::vector<Phase> p = {
        {Prep{{Spawn{0, false, 0}}, 1, false}, Expect().X(Refusal::NotSyncer)},
        {kNothing, Expect().X(Refusal::Malformed)},
        {kNothing, Admitted(1)},
        {kNothing, [] { Expect e; e.explAccepted = 1; e.X(Refusal::Blown); e.blownSlack = 1; return e; }(), true},
    };
    return p;
}

const std::vector<Phase>& PhasesOf(Arm a) {
    switch (a) {
    case Arm::StateForge:    return StatePhases();
    case Arm::SeatForge:     return SeatPhases();
    case Arm::TakeoverForge: return TakeoverPhases();
    default:                 return ExplodePhases();
    }
}

}  // namespace

int PhaseCount(Arm a) { return static_cast<int>(PhasesOf(a).size()); }

const Phase& PhaseOf(Arm a, int n) { return PhasesOf(a)[static_cast<size_t>(n - 1)]; }

void Verdict(Arm a, const char* what, const std::string& why) {
    if (std::strcmp(what, "PASS") == 0) UE_LOGI("[HOSTILE-DRILL] %s PASS -- %s", ArmName(a), why.c_str());
    // A FAIL or INCONCLUSIVE reads `<what>: <why>`, the hostile family's verdict form (hostile_drill.cpp): the rig
    // matches that exact string, so this family's red ends on its FAIL line like every other's.
    else UE_LOGW("[HOSTILE-DRILL] %s %s: %s", ArmName(a), what, why.c_str());
}

// Lines no save can hold, as a prop class or a place could.
std::string ClientReadyLine(Arm a) { return std::string("hostile-drill ") + ArmName(a) + " client-ready"; }
std::string GoPrefix(Arm a, int n) { return std::string("hostile-drill ") + ArmName(a) + " go " + std::to_string(n) + " "; }
std::string SentLine(Arm a, int n) { return std::string("hostile-drill ") + ArmName(a) + " sent " + std::to_string(n); }

const char* AtvReasonName(unsigned r) {
    switch (static_cast<AtvRefusal>(r)) {
    case AtvRefusal::Admitted:      return "admitted";
    case AtvRefusal::NotWorldReady: return "not_world_ready";
    case AtvRefusal::Malformed:     return "malformed";
    case AtvRefusal::UnknownAtv:    return "unknown_atv";
    case AtvRefusal::NoSeat:        return "no_seat";
    case AtvRefusal::NotOccupant:   return "not_occupant";
    case AtvRefusal::SenderDead:    return "sender_dead";
    case AtvRefusal::SeatPaced:     return "seat_paced";
    case AtvRefusal::AlreadySeated: return "already_seated";
    case AtvRefusal::Occupied:      return "occupied";
    case AtvRefusal::NoBody:        return "no_body";
    case AtvRefusal::OutOfReach:    return "out_of_reach";
    case AtvRefusal::NoTarget:      return "no_target";
    case AtvRefusal::Seated:        return "seated";
    case AtvRefusal::Synced:        return "synced";
    case AtvRefusal::TakeoverPaced: return "takeover_paced";
    }
    return "unknown";
}

}  // namespace detail

Arm ArmOf(const std::string& value) {
    for (const detail::ArmRow& r : detail::kArms)
        if (value == r.name) return r.arm;
    return Arm::None;
}

const char* ArmName(Arm a) {
    for (const detail::ArmRow& r : detail::kArms)
        if (r.arm == a) return r.name;
    return "off";
}

void TickHostJudge(coop::net::Session* s, Arm a) { detail::HostTick(s, a); }

void TickClient(coop::net::Session* s, Arm a) { detail::ClientTick(s, a); }

void OnDisconnect() {
    detail::ClientReset();
    detail::HostReset();
}

}  // namespace coop::dev::hostile_drill::atv
