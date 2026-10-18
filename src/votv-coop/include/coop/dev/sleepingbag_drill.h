// coop/dev/sleepingbag_drill.h -- [dev] a sleeping bag unrolls and folds on both peers, one for one.
//   HOST   -- drops a packed prop_sleepingbag_C on an empty floor a route reaches (no bag, deployed bag or bunk
//             mattress within 450 cm), says its element id in the chat, and judges its own copy through the client's
//             unroll and fold (one new form with an element id and a prop key, the old form gone) and then its own.
//   CLIENT -- walks to the bag, presses the unroll and the fold as a player's press runs them (actionOptionIndex with
//             its local player), and requires its own copy to keep the bag through each press: the lane cancels the
//             local body. It judges the same legs, requires the deployed bag to still stand 5 s of world time after its
//             birth, and says what it saw in the chat before the host's own unroll and fold.
//   BUNK   -- last, the host drops a bed_bunkMattress_C on another empty floor, the client presses its take-off
//             (action 7), and both peers must show one packed mattress and no bunk mattress.
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
