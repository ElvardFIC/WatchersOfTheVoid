// coop/dev/hostile_drill_chat.h -- the chat arms of the receive-boundary drill. chat_ill_formed: a joined client sends
// an ill-formed UTF-8 chat line, then a well-formed one behind it; the host must refuse the first and commit the
// second. The HOST judges by its own lobby record. Defensive testing of OUR OWN mod, per coop/dev/hostile_drill.h;
// nothing here ships enabled.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::hostile_drill::chat {

// The sub-arm; None when the active drill arm is not one of ours.
enum class Arm { None, ChatIllFormed };

// The arm's name, for the verdict markers.
const char* ArmName(Arm a);

// Client (sender), game thread: once this peer's own join is over, the arm's lines.
void TickClient(coop::net::Session* s, Arm a);

// Host (judge), game thread: arm on the first world-ready slot, then judge and print the PASS / FAIL marker.
void TickHostJudge(coop::net::Session* s, Arm a);

// Back to the un-armed state (disconnect / re-arm).
void OnDisconnect();

}  // namespace coop::dev::hostile_drill::chat
