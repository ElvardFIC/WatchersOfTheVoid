// coop/dev/atv_health_drill.h -- [dev] the ATV lane's table, judged on the world's own ATV (atv_health_drill=sit|grab|
// occupied|synced|silent|gate|owner|trap|spawned|carry_still|synced_peer|join|stream_pace on BOTH peers; only the client
// in slot 1 acts, another slot idles; the three-peer arms give slot 2 the observer role).
//   sit, grab -- CLIENT sits (leaves) or grabs (lets go) on the host's word; HOST names the seat and the syncer, then
//               the ATV handed back at rest. occupied, synced -- HOST sits or grabs; CLIENT's one raw request is refused.
//   silent   -- CLIENT sits, then stops streaming; HOST takes the seat and the ATV back by its silence rule.
//   gate, owner, trap -- the damage arms (_damage). spawned -- HOST spawns an ATV, CLIENT times its table, walks, sits.
//   carry_still, synced_peer, join, stream_pace -- the peers arms (_peers): a still carry is not handed back; a second
//               client's grab of a carried ATV is refused; a joiner takes the seat from the snapshot; a burst is paced.
// The peers sequence on lobby-chat lines. "[ATV-HEALTH-DRILL] FAIL" is the lane failing (--fail-marker); a pass prints
// a DONE PASS line on each peer. The red arm atv_health_red turns one check off, so the arm that tests it must FAIL:
// back (sit, grab), occupied, takeover (synced), silence (silent), nonowner (gate), trapbit (trap), not_syncer (hostile),
// carry_back (carry_still), peer_carry (synced_peer).

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::atv_health_drill {

// Advances this peer's phases; a latched read when off. Game thread.
void Tick(coop::net::Session* s);

// The session ended: the drill starts over.
void OnDisconnect();

}  // namespace coop::dev::atv_health_drill
