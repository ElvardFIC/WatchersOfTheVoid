// coop/dev/burn_revive_drill.h -- [dev] a client who dies burning is revived with no fire.
//   CLIENT -- once the revive can answer a death (world settled, the arm ready, the spawn protection over): sets the
//             local player on fire with the game's own verb, delivers a lethal hit, and judges the revive's end:
//             `isBurning` clear, the burn timer at 0, and the revive reporting success. Nothing on the host.
// Lines: "[BURN-REVIVE] client lit", "[BURN-REVIVE] client killed", "[BURN-REVIVE] client DONE ... -- PASS" (the run's
// end) and "[BURN-REVIVE] FAIL client: ..." (a verdict against, or a step that did not run).
// Run: --config burn_revive_drill=1 --done-marker '[BURN-REVIVE] client DONE' --fail-marker '[BURN-REVIVE] FAIL'

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::burn_revive_drill {

// Once per session (the pump's install).
void Install(coop::net::Session* session);

// Game thread, once per pump tick; a single latched read when off.
void Tick();

// Back to the start: a revive that failed flees, and the session's end is the verdict of a drill still waiting. A
// rejoin's peer runs the drill again.
void OnDisconnect();

}  // namespace coop::dev::burn_revive_drill
