// coop/world/salt_rocket_flight.h -- the host's salt rocket in flight, seen by every peer. A rocket's fire() starts
// its fuse and its thrust on the host's copy, and the thrust moves the body by the physics alone: no hand holds it
// and no hook ties it, so no channel streamed its pose and a client's copy stood still until the blast. The fire
// body is watched on the host: a call that starts the fuse (the prop has no flame yet) puts the rocket in a small
// set, and each tick the set's rockets are claimed on the driven-prop channel (coop/props/prop_drive_host.h), which
// streams the pose through the flight. A claimed row stays open, probed at 4 Hz while the body rests, until the
// rocket is destroyed or a hand takes it (coop/props/prop_drive_host.cpp, the rest rule in Tick). On a client the
// same watch only counts a fire() that ran on its own copy, which never fuses: its light is the host's to perform
// (coop/world/salt_rocket_ignite.h); the count is the drill's proof.
// MTA's unoccupied-vehicle syncer is the precedent (reference/mtasa-blue/Server/mods/deathmatch/logic/
// CUnoccupiedVehicleSync.cpp). Here the host is always the syncer (MTA picks a client by a 500 ms scan, :36-42,
// :171-192) and the set is fed by the fire() PRE, never by a scan; the set is capped at 32 (MTA iterates every
// vehicle); stale packets are dropped by the drive channel, not by this lane (MTA by time context, :285).
// Game thread throughout.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::salt_rocket_flight {

// Store the session and register the fire watch, once for the process. Idempotent; called every load tick.
void Install(coop::net::Session* s);

// Resolve and settle the watch, then on the host claim every rocket in the firing set for the drive channel and drop
// the dead. Every role, every gameplay tick.
void Tick();

// A new world: the rockets of the world that ended leave the set.
void OnWorldChange();

// One line of the counters when any moved, then the set emptied and the counters zeroed.
void OnDisconnect();

// joined: rockets that entered the host's firing set; full: fire() calls the full set left out (counted, the set
// is not grown); clientFires: fire() bodies that ran on a client's copy in a running session.
struct Counters {
    uint32_t joined, full, clientFires;
};
Counters GetCounters();

}  // namespace coop::salt_rocket_flight
