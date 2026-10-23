// coop/dev/hostile_drill_ignite.h -- the ignite_* drill arms, proving the host refuses what a client's salt rocket
// light request may not do, performs what it may, and counts each outcome under its own reason. Defensive testing of
// OUR OWN mod, per coop/dev/hostile_drill.h; nothing here ships enabled.
//
// A joined client forges PropIgniteIntent messages through the session's own send, bypassing its own ignite lane, and
// the HOST judges by accounting: every message is exactly one performed light or one refusal, so the sum of its
// counter deltas since its baseline equals the messages sent, and each reason's delta matches the arm's table.
//   ignite_malformed  ignite_no_tool  ignite_torch_attempt  ignite_torch_ignite  ignite_reach  ignite_not_rocket
//   ignite_not_ready  ignite_flood
// The host spawns the targets before the client's puppet (a near rocket, a far rocket, a lighter on the floor) and
// says it is ready in the lobby chat once the client's hand holds what the arm needs. The family acts on slot 1 only;
// another slot idles.

#pragma once

#include <string>

namespace coop::net { class Session; }

namespace coop::dev::hostile_drill::ignite {

// The sub-arm; None when the active drill arm is not ours.
enum class Arm { None, Malformed, NoTool, TorchAttempt, TorchIgnite, Reach, NotRocket, NotReady, Flood };

// The arm a config value names; None for a value that is not one of ours.
Arm ArmOf(const std::string& value);

// The arm's name, for the verdict markers.
const char* ArmName(Arm a);

// Client: the not_ready arm holds this client's world-ready announce, once per session.
void Install(coop::net::Session* s, Arm a);

// Host (judge), game thread: spawn the targets, say ready, then judge by accounting and print PASS / FAIL /
// INCONCLUSIVE.
void TickHostJudge(coop::net::Session* s, Arm a);

// Client (sender), game thread: take the tool, wait for the host's ready line, then send the arm's forged messages.
void TickClient(coop::net::Session* s, Arm a);

// Back to the un-armed state, putting back what the arm held (disconnect / re-arm).
void OnDisconnect();

}  // namespace coop::dev::hostile_drill::ignite
