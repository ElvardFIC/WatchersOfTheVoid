// coop/dev/desk_ping_drill.h -- drill: a client's ping is rolled on the host (ini desk_ping_drill=run / env
// VOTVCOOP_DESK_PING_DRILL; BOTH peers).
//
// The client walks to a point about the main desk's button_coords that a NavMesh route reaches, enters the coordinates
// screen with a press on it, commits a triangle and, once the panel's vertex markers stand on it, presses the key the
// player bound to coord_ping on the game window. Three legs: miss, the triangle away from every sky signal, where the
// host's verdict finds nothing; catch, a triangle over the sky signal the host puts at its centre with a circle the
// ping's inner circle matches, so the verdict gathers it; busy, a second verdict the client sends right behind the
// catch's, which finds the host's desk holding that one. Each peer counts the renderer's gatherSignal bodies entered and
// run. The client says its own verdict was entered and never ran, that the host's failure line reached its log once,
// that the catch is on its own profile and its desk, and that the busy verdict came back refused; the host that it
// rolled the first two, caught the second without counting the find as its own, relayed that catch as the client's,
// and refused the third. The host also reads what its own desk shows while the client pings (the progress fields and
// the line materials' alpha) and appends the per-ping maxima to the miss and catch lines; that reading judges nothing.
// Lines tagged [DESK-PING-DRILL]; 'client DONE PASS|FAIL' ends it; ABANDONED is a step that could not run.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::desk_ping_drill {

// Advance this peer's legs. Every pump tick in a world; a latched read when off. Game thread.
void Tick(coop::net::Session* session);

// The session ended: the legs start over.
void OnDisconnect();

}  // namespace coop::dev::desk_ping_drill
