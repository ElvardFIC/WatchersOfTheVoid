// coop/dev/hostile_drill_atv_detail.h -- what the atv_ family's files share: the phase table of each arm, the verdict
// print and the lobby-chat lines the two peers say to each other (coop/dev/drill_chat.h). Private to coop/dev:
// hostile_drill_atv*.cpp.

#pragma once

#include "coop/dev/hostile_drill_atv.h"

#include "coop/net/protocol.h"
#include "coop/world/explosion_request.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace coop::dev::hostile_drill::atv::detail {

using Clock = std::chrono::steady_clock;
constexpr auto kFailBound = std::chrono::seconds(180);   // a failure bound, never a wait
constexpr uint8_t kSlot = 1;                             // the one slot the family acts on
constexpr int kMaxAtvs = 4;                              // the ATVs one arm spawns at most

constexpr size_t kExplReasons = static_cast<size_t>(coop::explosion_request::Refusal::Count);

// What one phase expects of the host's counters since the phase's baseline. Every counter not named stays at 0.
struct Expect {
    uint32_t refused[16] = {};                 // atv_authority, by AtvRefusal
    uint32_t admitted = 0, notSyncer = 0;
    uint32_t explRefused[kExplReasons] = {};   // explosion_request, by Refusal
    uint32_t explAccepted = 0;                 // explosion_request's requestsAccepted for an ATV
    uint32_t blownSlack = 0;                   // `blown` refusals allowed beyond the table: honest ones the drill did not send
    Expect& R(coop::net::AtvRefusal r, uint32_t n = 1) { refused[static_cast<unsigned>(r) & 15u] += n; return *this; }
    Expect& X(coop::explosion_request::Refusal r, uint32_t n = 1) { explRefused[static_cast<size_t>(r)] += n; return *this; }
};

// One ATV the host spawns for a phase: its slot in the arm's list, 2000 cm out when `far` else 600 cm, from the fan
// direction `dir` on (the first that projects to the NavMesh).
struct Spawn {
    int  idx;
    bool far;
    int  dir;
};

// What the host does before it says go: the ATVs to spawn (all must be in its table with a key), whether its own
// player then sits in ATV 0, and a settle (ms since the previous phase was judged) the go waits out.
struct Prep {
    Spawn    spawns[3];
    int      nSpawns;
    bool     hostSits;
    uint32_t settleMs = 0;
};

// `judgeAtGo`: the phase is judged from the go itself, not from the client's sent line (its forgeries are sent right
// after the go, and a later honest message must not read as one of them).
struct Phase {
    Prep   prep;
    Expect expect;
    bool   judgeAtGo = false;
};

// The arm's phases, numbered from 1; the count, and a phase by number.
int          PhaseCount(Arm a);
const Phase& PhaseOf(Arm a, int n);

// The verdict line: PASS to the info log, FAIL and INCONCLUSIVE as warnings.
void Verdict(Arm a, const char* what, const std::string& why);

// The lobby chat lines the peers say (coop/dev/drill_chat.h): the client's once its streams run, the host's go line for a
// phase (a prefix; the ATV keys follow, space apart) and the client's once a phase is sent.
std::string ClientReadyLine(Arm a);
std::string GoPrefix(Arm a, int n);
std::string SentLine(Arm a, int n);

// The name of an AtvRefusal value, for the accounting lines.
const char* AtvReasonName(unsigned r);

void ClientTick(coop::net::Session* s, Arm a);
void ClientReset();
void HostTick(coop::net::Session* s, Arm a);
void HostReset();

}  // namespace coop::dev::hostile_drill::atv::detail
