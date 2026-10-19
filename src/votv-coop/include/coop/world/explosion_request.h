// coop/world/explosion_request.h -- a client's shots and explosion requests, and the host that receives them. A
// client's own game dissolves the explosions it would spawn (coop/world/explosion_sync.h); what its own player caused
// is asked of the host in two kinds, ProjectileFired (its launcher fired) and ExplosionRequest (the blast that
// followed), which ride one ordered lane and are never relayed.
//   - CLIENT (explosion_request_client.cpp): the launcher fire bodies are watched, so a rocket or sticky bomb born
//     inside one is the player's shot, sent when the body ends; the dissolved birth of an explosion whose source the
//     player owns (a shot's projectile, a held grenade or pipe bomb, the pilled body) is sent as its request.
//   - HOST: each is checked strictly -- length, the class pairs the game's Blueprints produce, finite floats, a
//     field the kind does not use zero -- and queued on its sender's slot, then judged one per token of its queue's
//     own bucket (explosion_request_host.cpp) and spawned natively. Every refusal is counted by reason and said
//     through a per-lane tally. The slot's state clears with its occupant.
//   - The explosion source classes (ue_wrap/world/explosion_source.h) resolve by name each tick until held, on a
//     client, the only role that reads them.
// Game thread throughout.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::explosion_request {

// Store the session and take the client half's watches; idempotent, called every load tick.
void Install(coop::net::Session* s);

// On a client resolve the source classes; flush the refusal tally and, on the host, serve each slot's three queues.
void Tick();

// A new world: every slot's queues and judged shots, and the client half's pending births and sent shots, are
// forgotten (they name projectiles of the world that ended). The counters and one-shot logs stay.
void OnWorldChange();

// Log the accepted, sent and refusal counts, then clear every per-slot state, the client half's state and the
// counters.
void OnDisconnect();

// Host receive: raw bytes, as the dispatch hands them; the format check and its refusal live here.
void OnFiredReceived(int senderSlot, const uint8_t* bytes, uint16_t len);
void OnRequestReceived(int senderSlot, const uint8_t* bytes, uint16_t len);

// Client, every birth (before the class filter): records a rocket or sticky born inside one of this client's
// launcher fire bodies. A class compare, then the script gate's body test only for those two classes.
void OnClientProjectileBirth(void* actor);

// Client: the dissolved birth of an explosion whose spawning Blueprint is sourceObject; sends at most one request.
void OnClientBirth(void* sourceObject, void* explosionActor);

}  // namespace coop::explosion_request
