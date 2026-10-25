// coop/dev/hostile_drill_explosion_detail.h -- what the explosion family's files share: the arm table, the verdict
// print and the lobby-chat lines the two peers say to each other. Private to coop/dev: hostile_drill_explosion*.cpp,
// and the explosion drill's witch car arm, which reads the chat helpers.

#pragma once

#include "coop/dev/hostile_drill_explosion.h"

#include "coop/world/explosion_request.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace coop::dev::hostile_drill::explosion::detail {

using Clock = std::chrono::steady_clock;
constexpr auto kFailBound = std::chrono::seconds(180);   // a failure bound, never a wait
constexpr uint8_t kSlot = 1;                             // the one slot the family acts on

// What the client holds in its hand for the arm.
enum class Hand : uint8_t { Empty, Rpg, Sticky, Grenade };
// Plain: one run of messages. The others carry a mechanism of their own.
enum class Mode : uint8_t { Plain, Story, NotReady, Flood, Rejoin };

// One arm: the messages the client sends (`sent`, the accounting total), and the host's expected deltas. Every counter
// not named here must stay at 0. Flood expects its own shape (accepted + queue_full == sent, queue_full >= 1).
struct Spec {
    const char*                       name;
    Arm                               arm;
    Hand                              hand;
    Mode                              mode;
    uint32_t                          sent;
    uint32_t                          shots;        // shots accepted
    uint32_t                          requests[3];  // requests accepted: projectile, hand, self
    coop::explosion_request::Refusal  refusal;      // the one reason expected, Count for none
    uint32_t                          refused;
    uint32_t                          evicted;
    bool                              hostPlayer;   // the host's own player is guarded too (a blast lands at its puppet)
};
const Spec* SpecOf(Arm a);

// The verdict line: PASS to the info log, FAIL and INCONCLUSIVE as warnings.
void Verdict(Arm a, const char* what, const std::string& why);

// The lobby chat lines: the host's ready line for a life of the client (1, and 2 for the rejoin arm's returning
// occupant), and the rejoin arm's shot-taken line.
std::string ReadyLine(Arm a, int life);
std::string TakenLine(Arm a);
void Say(const std::string& line);
// True when a LIVE chat row holds `needle`. A row seeded at the join sits in the retained tier and never counts, so a
// peer acts on what the other said after it joined, never on the record of an earlier life.
bool ChatHasLive(const std::string& needle);
// A feed walk is a few times a second: true on every fifteenth call.
bool PollDue();

// The class name of what `h` puts in a hand (the wchar form the hand lane compares).
const wchar_t* HandClass(Hand h);

void ClientTick(coop::net::Session* s, const Spec& sp);
void ClientReset();
void HostTick(coop::net::Session* s, const Spec& sp);
void HostReset();

}  // namespace coop::dev::hostile_drill::explosion::detail
