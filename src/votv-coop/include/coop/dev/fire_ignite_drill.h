// coop/dev/fire_ignite_drill.h -- [dev] a salt rocket a client lights is lit, flown and blown by the host, and every
// peer sees the flight. Set fire_ignite_drill on BOTH peers; the arm names who lights.
//   client -- the HOST spawns a rocket 150 cm before the client's puppet and waits; the CLIENT finds its mirror,
//             lets it settle, takes a lighter into its hand, turns its trace onto the rocket with the aim fan and
//             presses the lighter's LMB. The client's own copy must not fuse (its light is queued and sent), and the
//             host must perform it: a fuse, a flight, a blast.
//   host   -- the HOST spawns a rocket before its own player, lets it settle and lights it by the rocket's own
//             attemptIgnite; the CLIENT only watches its mirror, which must move and then blow with the host's.
// Each peer prints one `[FIRE-IGNITE] host DONE` or `[FIRE-IGNITE] client DONE` on a pass. `[FIRE-IGNITE] FAIL` is
// the lane failing (--fail-marker), `[FIRE-IGNITE] ABANDONED` the drill unable to do its part (--dead-marker). The
// bounds in the .cpp are failure bounds of the instrument, never a wait for a milestone. Run on a fresh host world.
// The CLIENT records the rockets it already holds and says `fire-ignite client-ready` in the lobby chat; the HOST
// spawns its rocket only after it sees that line, so the rocket is never in the start set.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::fire_ignite_drill {

// Store the session; idempotent, called every load tick. A latched read when off.
void Install(coop::net::Session* s);

// Advances this peer's steps; a latched read when off. Game thread.
void Tick();

// The session ended: the drill starts over.
void OnDisconnect();

}  // namespace coop::dev::fire_ignite_drill
