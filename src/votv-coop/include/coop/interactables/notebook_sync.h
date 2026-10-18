// coop/interactables/notebook_sync.h -- a notebook's page text across peers.
//
// The page text is the notebook's own save record (`text`, read by getData, written by loadData), which
// already crosses on the prop record lane (coop/props/prop_save_data). This module adds the editor:
//   - one editor at a time: ui_notebook_C open on a notebook holds the notebook's occupancy claim;
//   - what the editor wrote reaches the record: the editing peer publishes at the editor's close and, while it
//     holds the claim, once per kPublishEvery when the page changed (the open page is folded into `text`);
//   - the host takes a client's record from the slot holding the claim, or from any slot while nobody
//     does (a free notebook is anyone's to change);
//   - a client lets the claim go only after the host has answered its last publish, so the close reaches
//     the host while its sender still holds; a bound makes it let go anyway;
//   - a peer holding the claim applies no record for that notebook once it has published in this claim:
//     its edit is then the newer.
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

// What the host's rule has judged since the session began: records taken from the slot holding the claim,
// from any slot while nobody held it, and refused. For the notebook drill, which reads what the lane did.
struct Stats {
    uint32_t acceptedHeld = 0;
    uint32_t acceptedFree = 0;
    uint32_t refused = 0;
};
Stats StatsNow();

// Has the 2 s examine taken its first look at this notebook, the text a later edit is measured from?
// An edit made before that is not a change to publish. For the notebook drill, which waits on it.
bool BaselineTaken(void* notebook);

// Session teardown: the session, the last-published text, the close in flight, the unanswered publishes.
void OnDisconnect();

}  // namespace coop::notebook_sync
