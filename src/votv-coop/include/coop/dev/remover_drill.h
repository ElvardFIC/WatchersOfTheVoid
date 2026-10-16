// coop/dev/remover_drill.h -- [dev] the tool gun's remover pressed by a client, each peer judging its own copies
// (ini remover_drill=granted|withheld, env VOTVCOOP_REMOVER_DRILL; BOTH peers, the same value).
//
//   HOST   -- once slot 1's world is ready and its puppet stands, spawns sacks about the puppet (a prop the remover
//             may remove) and, in `granted`, a kerfurOmega beside them (a creature it may not). It judges its own
//             copies when the client's closing chat line reaches it.
//   CLIENT -- finds the host's props about itself, spawns a toolgun and, per shot, a tool_remover_C; writes the
//             toolgun's outHit at the target and runs the tool's init(toolgun), the body a press runs. It waits on
//             what its own copies and screen show, never a clock.
//             granted:  one prop gone; six presses at once on another; twenty props in its sight one after another,
//                       each gone and none refused for sight; then the creature, which stays and is answered.
//             withheld: one prop, refused by the host's permissions: it stays and the refusal shows.
// A FAIL is the lane failing, an ABANDONED the drill unable to do its part; each leg prints "client leg". The drill
// names no symbol of the remover's request lane: it reads the copies and the screen's notices by their text.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::remover_drill {

// Advances this peer's steps; a latched read when off. Game thread.
void Tick(coop::net::Session* s);

// The session ended: the drill starts over.
void OnDisconnect();

}  // namespace coop::dev::remover_drill
