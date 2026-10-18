// coop/interactables/notebook_sync.h -- a notebook's page text across peers.
//
// The page text is the notebook's own save record (`text`, read by getData, written by loadData), which
// already crosses on the prop record lane (coop/props/prop_save_data). This module adds the editor:
//   - one editor at a time: ui_notebook_C open on a notebook holds the notebook's occupancy claim;
//   - what the editor wrote reaches the record: the editing peer publishes it at the editor's close and,
//     while it holds the claim, once per kPublishEvery when the page text changed (the open page is
//     folded into `text` first, which the widget writes only on commit and quit);
//   - the host takes a client's record from the slot that holds the claim, or from any slot while nobody
//     does (a free notebook is anyone's to change);
//   - a client lets the claim go only after the host has answered its last publish, so the close reaches
//     the host while its sender still holds; a bound makes it let go anyway;
//   - a peer holding the claim applies no record for that notebook: its edit is the newer.
// The two rules are registered with coop/props/prop_record_gates; the hold on the release with
// coop/interactables/device_occupancy. Game thread throughout.

#pragma once

#include <cstdint>

namespace coop::net { class Session; }

namespace coop::notebook_sync {

// Store the session, watch ui_notebook_C's graph and its quit, and register the rules (idempotent;
// re-entered by the subsystem pump until the watches take). Both roles.
void Install(coop::net::Session* session);

// HOST: may the record a client sent for the notebook `actor` land? From the slot holding its claim, or
// from any slot when none holds it; a record from another slot is refused, logged.
bool AcceptsClientRecord(void* actor, uint8_t senderSlot);

// Session teardown: the session, the last-published text, the close in flight, the unanswered publishes.
void OnDisconnect();

}  // namespace coop::notebook_sync
