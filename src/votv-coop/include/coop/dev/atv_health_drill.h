// coop/dev/atv_health_drill.h -- [dev] the ATV lane's table, judged on the world's own ATV (atv_health_drill=sit|grab on
// BOTH peers; only the client in slot 1 acts, another slot idles).
//   sit  -- CLIENT walks to the ATV with the director, takes its seat through the game's own verb and leaves it on the
//           host's word. HOST names the seat and the syncer, then the freed seat, then the ATV handed back at rest.
//   grab -- CLIENT grabs the ATV with the grav hand and lets it go on the host's word. HOST names the syncer with the
//           seat free, then the ATV handed back at rest.
// The peers sequence on lobby-chat lines. "[ATV-HEALTH-DRILL] FAIL" is the lane failing (--fail-marker); a pass prints
// a DONE PASS line on each peer (the done marker names the role), the host's with the times it measured. The red arm
// atv_health_red=back (host) skips the at-rest hand-back, so both arms must FAIL at `back`.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::atv_health_drill {

// Advances this peer's phases; a latched read when off. Game thread.
void Tick(coop::net::Session* s);

// The session ended: the drill starts over.
void OnDisconnect();

}  // namespace coop::dev::atv_health_drill
