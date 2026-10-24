// coop/dev/atv_eject_drill.h -- [dev] the ATV wheel's presence rule, judged on the world's own ATV
// (atv_eject_drill=host|client on BOTH peers, on a fresh host world; only the client in slot 1 acts, another slot idles).
//   host   -- HOST knocks a rear wheel off its own idle ATV through the game's damageWheel; CLIENT's live copy must lose
//             that wheel with exactly one updTires call (the wheel's bit clear) and no presence skipped.
//   client -- CLIENT walks to the ATV with the director, sits and knocks the wheel off its copy; HOST must keep its wheel,
//             count the client's presence as skipped and call no updTires (the known divergence, held contained).
// The peers sequence on lobby-chat lines. "[ATV-EJECT-DRILL] FAIL" is the lane failing (--fail-marker). Each peer prints
// a DONE PASS line, but an arm ends on the judging peer's: arm host on the client's DONE PASS line, arm client on
// the host's; the other peer's DONE line says only that its own act ran. The red arm atv_eject_red
// inverts the receiving copy's presence rule, so both arms must FAIL.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::atv_eject_drill {

// Advances this peer's phases; a latched read when off. Game thread.
void Tick(coop::net::Session* s);

// The session ended: the drill starts over.
void OnDisconnect();

}  // namespace coop::dev::atv_eject_drill
