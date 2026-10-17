// coop/dev/pi2_selftest.h -- [dev] proves the three pieces the puppet-identity module rests on, on the
// live game: the locals a bytecode call writes (script_calls::CallHasLocal), the registry's puppet-count
// listener, the caller-locals boot check (which now covers RemoveActors' remaining), and the registry's
// puppet-body query.
//
// Each peer, once its world is up (the host with its own Player Element, a client once it announced
// world-ready, both with the Blueprint library class loaded and a local player), the other peer's puppet
// spawned and the three classes birch_C, npc_angryErieFlesh_C and prop_shower_C resolved (a class not yet
// loaded is loaded by its /Game path, once), runs every check in ONE tick so each result line is printed
// before a rig can end the run. Each check (call-locals, listener, caller-locals, body) logs a PASS line
// tagged PI-2 or the FAIL lines of what it found, and a DONE line naming the role ends the run. The
// listener check registers and clears a test puppet on the highest peer slot and leaves the registry, and
// the previous listener, as it found them. Armed per run by pi2_selftest=1.
#pragma once

namespace coop::net { class Session; }

namespace coop::dev::pi2_selftest {

// The pump tick's entry: one latched flag read when off. Runs once per process, on the first tick on which
// this peer is ready. Game thread.
void Tick(coop::net::Session* session);

}  // namespace coop::dev::pi2_selftest
