// coop/dev/hostile_drill_hand.h -- the drill arm proving a hand-item message cannot make a peer build an
// actor of a class that is not a prop. bad_hand_class: a joined client describes its own hand as holding
// the engine's base actor class, then sends one chat line behind it on the same lane. The HOST judges at
// that line: the class stored for the slot, then on its next call the slot's display mirror, which one
// mirror pass has had the chance to build by then. Defensive testing of OUR OWN mod, per
// coop/dev/hostile_drill.h; nothing here ships enabled.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::hostile_drill::hand {

// The arm's name, for the verdict markers.
const char* ArmName();

// Client (sender), game thread: once this peer's own join is over, send the forged hand state and its
// sentinel line, once.
void TickClient(coop::net::Session* s);

// Host (judge), game thread: arm on the first world-ready slot, then judge at the sentinel and print the
// PASS / FAIL / INCONCLUSIVE marker.
void TickHostJudge(coop::net::Session* s);

// Back to the un-armed state (disconnect / re-arm).
void OnDisconnect();

}  // namespace coop::dev::hostile_drill::hand
