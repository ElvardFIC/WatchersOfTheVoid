// coop/dev/notebook_drill.h -- [dev] a notebook's page is written at one peer's editor and read on the others.
//   HOST     -- drops a prop_notebook_C on a floor a route reaches beside the client, writes a seed page into it and
//               publishes it. When the client holds the notebook's claim it presses the open on its own copy and requires
//               the claim to refuse it (the editor leaves again, the client still holds), then says so in the chat.
//               It then requires its own copy to read the client's page and says that.
//   CLIENT   -- walks to the notebook, opens it with the notebook's own action, waits for the host's word on the claim,
//               writes a known page into the open editor and closes it with the editor's own quit, then waits for the
//               host's word that it read the page. A client that arrives to find the page already written (a rejoin's
//               second life) only reads it.
//   OBSERVER -- a further client reads the page as it crosses, or from the snapshot.
// Lines tagged [NOTE-DRILL]: a PASS line per leg and judging peer; "[NOTE-DRILL] FAIL" is the lane failing a step
// (--fail-marker), "[NOTE-DRILL] ABANDONED" the drill unable to do its part (--dead-marker); "host DONE", "client DONE"
// and "observer DONE" end each peer; "client LIFE DONE" ends a first life that a rejoin follows (--rejoin-marker).
// Ini: notebook_drill=run on every peer; rejoin for a run whose client comes back; red closes the editor with the
// game's discard (skipEdit), so no page is carried and the host's read must fail.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::notebook_drill {

// Game thread, once per pump tick; a latched string compare when off.
void Tick(coop::net::Session* session);

// The per-session half: a rejoin's peers run their legs again.
void OnDisconnect();

}  // namespace coop::dev::notebook_drill
