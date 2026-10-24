// coop/dev/atv_health_drill.h -- [dev] the ATV lane's table, judged on the world's own ATV (atv_health_drill=sit|grab|
// occupied|synced|silent|gate|owner|trap|spawned on BOTH peers; only the client in slot 1 acts, another slot idles).
//   sit      -- CLIENT sits in the ATV through the game's own verb and leaves on the host's word; HOST names the seat
//               and the syncer, the freed seat, then the ATV handed back at rest.
//   grab     -- CLIENT grabs the ATV with the grav hand and lets go; HOST names the syncer, then the hand-back.
//   occupied -- HOST sits in the ATV; CLIENT sends one raw sit request, which the host must refuse as occupied.
//   synced   -- HOST grabs the ATV; CLIENT sends one raw takeover, which the host must refuse as synced.
//   silent   -- CLIENT sits, then stops streaming; HOST must take the seat and the ATV back by its silence rule and the
//               client's own copy must give up the seat on the host's stamp.
//   gate, owner, trap -- the damage arms (atv_health_drill_damage.cpp): the host's blast on the sitting client's copy,
//               the client's kill of its copy, the host's story-event trap fired on the client's copy.
//   spawned  -- HOST spawns its own ATV and says its key; CLIENT times its table showing it, walks and sits (_spawned).
// The peers sequence on lobby-chat lines. "[ATV-HEALTH-DRILL] FAIL" is the lane failing (--fail-marker); a pass prints
// a DONE PASS line on each peer. The red arm atv_health_red turns one check off, so the arm that tests it must FAIL:
// back (sit, grab), occupied, takeover (synced), silence (silent), nonowner (gate), trapbit (trap), not_syncer (hostile).

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::atv_health_drill {

// Advances this peer's phases; a latched read when off. Game thread.
void Tick(coop::net::Session* s);

// The session ended: the drill starts over.
void OnDisconnect();

}  // namespace coop::dev::atv_health_drill
