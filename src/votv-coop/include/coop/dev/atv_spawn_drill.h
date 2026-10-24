// coop/dev/atv_spawn_drill.h -- [dev] a client holds one actor per ATV the host holds, and none the host does not.
//   HOST   -- spawns an ATV_C in front of its own player and prints how many ATVs it holds: run, once slot 1's world
//             is ready; join, while slot 1 is seated and still loading, so the save the host takes carries the ATV
//             and the client adopts it; park, as run; divergence, as join, but the host destroys the ATV as the
//             world stream ends, so the client's save holds an ATV the host's world does not and the host's complete
//             set removes it.
//   CLIENT -- once its world is ready, judges the live ATVs by key: two actors for a key is a failure at once; a key
//             the host never announced is a ghost; the arm's own evidence (a mirror spawned, a copy adopted, the
//             park having held a spawn, a destroy applied) must hold on five reads in a row after the host's set was
//             applied. In park mode atv_park_first_spawn=1 parks the first arrival that would spawn a mirror.
// "[ATV-SPAWN-DRILL] FAIL" is the lane failing (--fail-marker), "[ATV-SPAWN-DRILL] ABANDONED" the drill unable to do
// its part (--dead-marker). Run with atv_spawn_drill=run|join|park|divergence and --done-marker
// "[ATV-SPAWN-DRILL] client DONE"; the two "count atvs=" lines are compared, host against client. The red arms:
// atv_adopt_break=1 on the client (join), atv_set_break=1 on the client (divergence).

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::atv_spawn_drill {

// Advances this peer's steps; a latched read when off. Game thread.
void Tick(coop::net::Session* s);

// HOST: slot `slot`'s world stream has completed. In the divergence arm the drill's ATV is destroyed here, after the
// world the joiner receives was taken; a no-op in every other arm. Game thread.
void OnSlotStreamComplete(int slot);

// The session ended: the drill starts over.
void OnDisconnect();

}  // namespace coop::dev::atv_spawn_drill
