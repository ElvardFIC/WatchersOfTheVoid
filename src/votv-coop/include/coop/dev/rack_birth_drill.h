// coop/dev/rack_birth_drill.h -- [dev] a drive rack born holding rows reaches the client through the rack lane.
//   HOST   -- once a client's world is ready, spawns a drive rack and fills rows 0-3 in that same tick, then
//             judges the rack lane's first sight of it: the lane must have broadcast a canonical for it.
//   CLIENT -- must end holding the host's four rows, and send the host no op (the rig's --absent lines
//             "take idx=" and "set idx=" judge that half).
// Lines: "[RACK-BIRTH] host HOLD" (the rack as the host filled it, with its digest), "[RACK-BIRTH] host ANNOUNCED"
// (the rack lane broadcast its canonical), "[RACK-BIRTH] client DONE" (the run's end, with the client's digest)
// and "[RACK-BIRTH] FAIL" (a verdict against). The two digests must be equal (the rig's --compare).
// Run, on both peers: --config rack_birth_drill=1 --done-marker '[RACK-BIRTH] client DONE'
//                     --fail-marker '[RACK-BIRTH] FAIL'

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::rack_birth_drill {

// Once per session (the pump's install).
void Install(coop::net::Session* session);

// Game thread, once per pump tick; a single latched read when off.
void Tick();

// Back to the start: a rejoin's peers run the drill again.
void OnDisconnect();

}  // namespace coop::dev::rack_birth_drill
