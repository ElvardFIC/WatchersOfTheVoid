// coop/dev/sleepingbag_drill.h -- [dev] a sleeping bag unrolls and folds on both peers, one for one.
//   HOST   -- once a client's puppet is in its world, drops one packed prop_sleepingbag_C on a floor a NavMesh route
//             reaches and says its element id in the lobby chat. It then judges its own copy through four legs: the
//             client's unroll (the packed bag gone, exactly one deployed bag with an element id and a prop key), the
//             client's fold (one packed bag back, the deployed one gone), and, on the client's word that it saw
//             both, its own unroll and fold by the bag's actionOptionIndex.
//   CLIENT -- reads the id, finds the mirror, walks to it with the director and presses the unroll as a player's
//             press runs it (actionOptionIndex with its local player). The press must leave its own copy standing:
//             the lane cancels the local body and only the host's state may change it. It judges the same two legs on
//             its copy, requires the deployed bag to still stand 5 s of world time after its birth (a mirror whose own
//             BeginPlay folds it back would fail there), presses the fold, says what it saw in the chat, and then
//             judges the host's unroll and fold the same way.
// Lines tagged [BAG-DRILL]: a PASS line per leg and judging peer; "[BAG-DRILL] FAIL" is the lane failing a step
// (--fail-marker), "[BAG-DRILL] ABANDONED" the drill unable to do its part (--dead-marker), and
// "[BAG-DRILL] client DONE PASS" the end (--done-marker). Ini: sleepingbag_drill=run on BOTH peers.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::sleepingbag_drill {

// Game thread, once per pump tick; a latched string compare when off.
void Tick(coop::net::Session* session);

// The per-session half: a rejoin's peers run their legs again.
void OnDisconnect();

}  // namespace coop::dev::sleepingbag_drill
