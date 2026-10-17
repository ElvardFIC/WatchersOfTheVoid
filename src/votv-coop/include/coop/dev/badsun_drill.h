// coop/dev/badsun_drill.h -- drill: the Bad Sun on a joined pair (ini badsun_drill=off|clock / env
// VOTVCOOP_BADSUN_DRILL; BOTH peers, with the rig's save, which holds one saved Bad Sun, a client joined). Every
// phase waits on a state its peer can read, never on a clock; a phase that does not come ends FAIL with the reason
// after a bound. Each verdict is one line: the tag, the arm, the role, PASS or FAIL, then the reason; every other
// line the drill prints starts with the drill key and a colon, so a run's done marker meets the verdict alone.
//   clock: what the join design's first claim rests on, that a client's own loaded copy goes active at 07:00 by its
//     own clock, with no wire built. Once a client's join is over the host sets the within-day clock to 06:58 and
//     says the clock it read before; each peer then reads, every second, the count of live copies, the first copy's
//     two tests (its widget is set; it is in the gamemode's activeEvents_senders) and the named clock, from the
//     moment its own clock shows the set (06:57 or later within hour 6). The count at the first sample must be 1;
//     the copy must be inactive until the clock reads 07:00, and active by both tests within 15 s after it. A clock
//     that never reaches 07:00 within 120 s of the arm's start, or a copy active before it, is a FAIL.
// Run: the rig's smoke with badsun_drill=clock set on both peers and the client's verdict line as its done marker,
// on the rig's save and not a fresh host world: the save's record is the copy measured.
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
