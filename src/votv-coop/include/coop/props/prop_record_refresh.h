// Republish save records when stationary drive boxes, reel cases or canned food
// change. Birth snapshots alone cannot carry later lid and content mutations.
// Script watches observe the classes' own refresh verbs, including internal BP
// calls. Cans publish after open/eat; native loadData/init rebuilds their mesh.
// Held cans remain on the hand lifecycle. See docs/props.md.

#pragma once

namespace coop::net { class Session; }

namespace coop::props::prop_record_refresh {

// Watch each class's refresh verb and cache the session. Idempotent; a class not loaded yet is
// retried on the next call. Game thread.
void Install(coop::net::Session* session);

// Session teardown: the per-actor coalescing state is dropped.
void OnDisconnect();

}  // namespace coop::props::prop_record_refresh
