// coop/interactables/notebook_sync.h -- a notebook's page text across peers.
//
// The page text is the notebook's own save record (`text`, read by getData, written by loadData), which
// already crosses on the prop record lane (coop/props/prop_save_data). This module adds the editor:
//   - one editor at a time: ui_notebook_C open on a notebook holds the notebook's occupancy claim;
//   - what the editor wrote reaches the record: the editing peer publishes it at the editor's close and,
//     while it holds the claim, once per kPublishEvery when the page text changed (the open page is
//     folded into `text` first, which the widget writes only on commit and quit);
//   - the host takes a client's record only from the slot holding the claim, or holding it last when
//     nobody does now: the record rides the Bulk lane, the release the Normal one;
//   - a peer holding the claim applies no canonical record for that notebook: its edit is the newer.
// The torn sheet's keyed birth is the ordinary prop birth route (coop/props/prop_drop_intent).
// Game thread throughout.

#pragma once

#include <cstdint>

namespace coop::net { class Session; }

namespace coop::notebook_sync {

// Store the session and watch ui_notebook_C's graph and its quit (idempotent; re-entered by the
// subsystem pump until the watches take). Both roles.
void Install(coop::net::Session* session);

// HOST: may the record a client sent for `actor` land? True for an actor that is not a notebook (this
// lane has no say there). A notebook's record is taken from the slot that holds its claim, else from
// the slot that held it last; any other sender is refused, logged.
bool AcceptsClientRecord(void* actor, uint8_t senderSlot);

// Does THIS peer hold the claim of `actor`, a notebook? A canonical record for it is not applied then.
bool LocalEditorHolds(void* actor);

// Session teardown: the session, the last-published text, the close in flight.
void OnDisconnect();

}  // namespace coop::notebook_sync
