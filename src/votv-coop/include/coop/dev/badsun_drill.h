// coop/dev/badsun_drill.h -- drill: the Bad Sun on a joined pair (ini badsun_drill=off|clock / env
// VOTVCOOP_BADSUN_DRILL; BOTH peers, with the rig's save, which holds one saved Bad Sun, a client joined). Every
// phase waits on a state its peer can read, never on a clock; a phase that does not come ends FAIL after a bound.
// Each verdict is one line: the tag, the arm, the role, PASS, FAIL or INCONCLUSIVE, then the reason; every other
// line starts with the drill key and a colon, so a run's done marker meets the verdict alone.
//   clock: that a client's own loaded copy goes active at 07:00 by its own clock, with no wire built. The host
//     reads the save's clock and copy at its first tick and sets the clock to 05:00, so the join runs before the
//     copy's own test; once a join is over it sets 06:58. Each peer samples from its first clock reading of hour 6
//     or later after its own join: the count of live copies (1 at the first sample), the copy's two tests (widget
//     set; in activeEvents_senders) and the clock. Inactive until 07:00, active by both within 15 s after it, else
//     FAIL, as for a reader that does not resolve. INCONCLUSIVE: the save's copy was active at hosting (the save
//     must read before 07:00), or a peer's first sample came after 07:00 with its copy already active.
// Run: the rig's smoke with badsun_drill=clock on both peers, the client's verdict line as its done marker, on the
// rig's save and not a fresh host world: the save's record is the copy measured.
#pragma once

namespace coop::net { class Session; }

namespace coop::dev::badsun_drill {

// True unless the row is `off`.
bool IsEnabled();

// Cache the session. Called from the dev install fanout; no-op when off.
void Install(coop::net::Session* session);

// Advance this peer's phase. Every pump tick in a world. Game thread.
void Tick();

// The session ended: back to the first phase.
void OnDisconnect();

}  // namespace coop::dev::badsun_drill
