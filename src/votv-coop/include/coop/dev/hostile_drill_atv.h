// coop/dev/hostile_drill_atv.h -- the atv_* drill arms, proving the host refuses what a client's ATV messages may not
// do, performs what they may, and counts each outcome under its own reason. Defensive testing of OUR OWN mod, per
// coop/dev/hostile_drill.h; nothing here ships enabled.
//
// A joined client forges AtvState, AtvSeat, AtvTakeover and AtvExplodeRequest messages through the session's own send,
// bypassing its ATV lane, and the HOST judges by accounting: each phase of an arm is a burst of forgeries and a table
// of counter deltas, and a counter the table does not name must stay at 0.
//   atv_state_forge  atv_seat_forge  atv_takeover_forge  atv_explode_forge
// The host spawns the ATVs the arm needs beside the client's puppet, says each phase's keys in the lobby chat, and
// judges the phase from the client's "sent" line. The family acts on slot 1 only; another slot idles.

#pragma once

#include <string>

namespace coop::net { class Session; }

namespace coop::dev::hostile_drill::atv {

// The sub-arm; None when the active drill arm is not ours.
enum class Arm { None, StateForge, SeatForge, TakeoverForge, ExplodeForge };

// The arm a config value names; None for a value that is not one of ours.
Arm ArmOf(const std::string& value);

// The arm's name, for the verdict markers.
const char* ArmName(Arm a);

// Host (judge), game thread: spawn the phase's ATVs, say go, then judge by accounting and print PASS / FAIL /
// INCONCLUSIVE.
void TickHostJudge(coop::net::Session* s, Arm a);

// Client (sender), game thread: say ready, then for each phase wait for the host's go line and send the phase's forged
// messages through the session's own send.
void TickClient(coop::net::Session* s, Arm a);

// Back to the un-armed state, putting back what the arm held (disconnect / re-arm).
void OnDisconnect();

}  // namespace coop::dev::hostile_drill::atv
