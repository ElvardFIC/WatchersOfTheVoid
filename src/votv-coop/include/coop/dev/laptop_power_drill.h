// coop/dev/laptop_power_drill.h -- [dev] the stationary laptop's power, pressed on each peer, its replay judged.
// Each peer presses its own laptop with the blueprint's own press (actionOptionIndex with action 8, as the game's
// auto-press and the lane's replay call it), and each step waits on its laptop settling on the value it expects.
//   CLIENT -- once its world is ready, its join over and its laptop settled with power: presses; once settled the
//             other way, its own edges sent are one more than at its start. Then it waits for the host's press to
//             arrive as a replay: settled back, its own edges unchanged (a replay sent back as this peer's own edge
//             is the defect). Then it presses again: settled the other way, two own edges (a replay that never
//             released the lane would swallow this one).
//   HOST   -- once a client's world is ready and its own laptop settled with power: waits for the client's edge
//             applied and its laptop settled the other way with no edge of its own sent; presses; settled back with
//             one own edge; then the client's second edge applied and settled the other way, still one own edge:
//             DONE.
// "[LAPTOP-POWER-DRILL] FAIL" is a step failing (--fail-marker), "[LAPTOP-POWER-DRILL] ABANDONED" the drill unable to
// do its part (--dead-marker), "[LAPTOP-POWER-DRILL] host DONE PASS" the run's end (--done-marker). Each step's bound
// is a failure bound, never a wait. Run with laptop_power_drill=1 on both peers.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::laptop_power_drill {

// Game thread, once per pump tick; a single bool read when off.
void Tick(coop::net::Session* session);

// Back to the start: a rejoin's peers run their steps again.
void OnDisconnect();

}  // namespace coop::dev::laptop_power_drill
