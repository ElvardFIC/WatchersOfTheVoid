// coop/dev/drone_call_drill.h -- [dev] the drone console drill: a client's press on the garage
// console reaches the host, which presses its own console for it, and the drone flies on both copies.
//   CLIENT -- once its world is ready and its join is over: walks (the director, a NavMesh route) to the
//             console, opens its lid through the latch if it is shut, presses the leave-timer face
//             (button_changeLeave), waits for its click to sound and the host's stream to flip the mirror's
//             leaveAfter5min, then presses it again for the flip back; then turns its camera through a fan
//             of headings until the trace strikes the keyboard (button_call) and presses it. The lane
//             refuses the client's own body and sends the press; the host's "[DRONE-CALL] PRESSED
//             console" line is the lane's verdict. A sack that comes to the client's copy after the
//             first keyboard press is followed by a second. The client's "DONE" line says whether the
//             drone moved on its copy (PASS) or not (INCONCLUSIVE): the drone's say, reported.
//   HOST   -- logs its drone's changes (a sack in the world, hasSack, active, a flight), each once.
// A leg that cannot go on says "[DRONE-CALL-DRILL] ABANDONED" (a run's --dead-marker), and a keyboard
// press that sent nothing, or a timer flip or click that never came, says "[DRONE-CALL-DRILL] FAIL"
// (--fail-marker). Run with drone_call_drill=1.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::drone_call_drill {

bool IsEnabled();

// Game thread, once per pump tick; a single bool read when off.
void Tick(coop::net::Session* session);

// The per-session half: a rejoin's client runs its legs again, its DONE line naming the session it is
// this process's (a --rehost run's life 2 says "in session 2"), and the host's watch starts over.
void OnDisconnect();

}  // namespace coop::dev::drone_call_drill
