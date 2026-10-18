// coop/dev/notebook_drill.h -- [dev] a notebook's page is written at one peer's editor and read on the others.
//   HOST     -- drops a prop_notebook_C beside the client and seeds its first page. While the client holds the
//               claim it presses the open on its own copy (the claim sends its editor out), then reads the page
//               the client typed and left open (the 2 s publish), asks its rule about a slot that is not the
//               holder (refused), reads the page the client closed with (taken from the slot still holding),
//               and, once the claim is released, asks the rule again (any slot's record is taken).
//   CLIENT   -- walks to the notebook, opens it, types a page through the editor's commit handler and waits for
//               the host's word, then writes a last page and closes with the editor's own quit. A client that
//               arrives to find that page written (a rejoin's second life) only reads it.
//   OBSERVER -- a further client reads the last page as it crosses, or from the snapshot.
// Lines tagged [NOTE-DRILL]: a PASS line per leg and judging peer; "[NOTE-DRILL] FAIL" is the lane failing a step
// (--fail-marker), "[NOTE-DRILL] ABANDONED" the drill unable to do its part (--dead-marker); "host DONE", "client
// DONE" and "observer DONE" end each peer; "client LIFE DONE" ends a first life a rejoin follows (--rejoin-marker).
// Ini: notebook_drill=run on every peer; rejoin when the client comes back; red closes the editor with the game's
// discard (skipEdit), so the last page is not carried and the host's read must fail.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::notebook_drill {

// Game thread, once per pump tick; a latched string compare when off.
void Tick(coop::net::Session* session);

// The per-session half: a rejoin's peers run their legs again.
void OnDisconnect();

}  // namespace coop::dev::notebook_drill
