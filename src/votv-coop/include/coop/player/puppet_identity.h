// coop/player/puppet_identity.h -- a remote player's puppet is never "this machine's player".
//
// A puppet is an unpossessed mainPlayer_C, so a Blueprint that casts "the actor that touched me" to the player
// passes it, and then does what only this machine's own player earns: saves, quits, travels, rewinds the clock,
// leaves a dream, turns the player's camera. Each such path is refused at its own seam when the toucher is a
// puppet body (players::Registry::IsPuppetBody), and runs unchanged for this machine's player and in single
// player. A stub that tests no toucher is cancelled whole; a chain that does other work the host needs is cut at
// the one global callee (a scope mark read by a later seam); a scan whose result the Blueprint reads has the
// puppet removed from the result. Every row runs on every machine: the host refuses a client's puppet, a client
// the host's or another client's puppet touching its copy. Game thread.

#pragma once

#include <cstdint>

namespace coop::puppet_identity {

// Register the stub watches and the puppet-count listener once, and install each native seam as soon as its
// class resolves. Retried every pump tick; idempotent.
void Install();

// Settle the watches' registration and say once how many are live, then re-resolve the per-world pointers when a
// class or the class set moved. Game thread, per frame.
void Tick();

// The session's caches. Session end.
void OnDisconnect();

// How many of the module's name watches are live at the gate.
int WatchesLive();

// A row's name and how many times it refused a puppet since the process started. The drills' read.
struct RowCounter {
    const char* row;
    uint32_t    refused;
};
const RowCounter* Counters(int& n);

}  // namespace coop::puppet_identity
