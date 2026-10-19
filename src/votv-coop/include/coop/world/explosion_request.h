// coop/world/explosion_request.h -- a client's shots and explosion requests, as the host receives them. A client's
// own game dissolves the explosions it would spawn (coop/world/explosion_sync.h); what its own player caused is
// asked of the host in two kinds, ProjectileFired (this client's launcher fired) and ExplosionRequest (the blast
// that followed), which ride one ordered lane and are never relayed.
//   - HOST: each is checked strictly -- its length, the class pairs the game's Blueprints produce, every float
//     finite, a field the kind does not use zero -- and queued on its sender's slot, then judged one per token of its
//     queue's own bucket (coop/world/explosion_request_host.cpp): a shot against the launcher held or the car seated
//     in and the shooter's reach, a blast against its shot, the explosive held or the living player, and then spawned
//     natively. Every refusal is counted by reason and said through a per-lane tally. The slot's state clears with
//     its occupant.
//   - The explosion source classes (ue_wrap/world/explosion_source.h) resolve by name each tick until held.
// Game thread throughout.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::explosion_request {

// Store the session; idempotent, called every load tick.
void Install(coop::net::Session* s);

// Resolve the source classes, flush the refusal tally and, on the host, serve each slot's three queues.
void Tick();

// Log the accepted and refusal counts, then clear every per-slot state and the counters.
void OnDisconnect();

// Host receive: raw bytes, as the dispatch hands them; the format check and its refusal live here.
void OnFiredReceived(int senderSlot, const uint8_t* bytes, uint16_t len);
void OnRequestReceived(int senderSlot, const uint8_t* bytes, uint16_t len);

}  // namespace coop::explosion_request
