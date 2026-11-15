// coop/interactables/window_stroke_sync.h -- syncs sponge dabs on the bay window.
// The presser captures its own dab; the host relays client dabs, and joiners
// receive the window through the transferred save.
// Thrown sponges are omitted, signal playback delays arrivals, and dirt marks
// still roll independently. Queues hold 256 dabs; refused sends retry for 20 s.
// Each tick handles at most eight dabs. Unresolved draws retain their FIFO row;
// each admitted native draw is attempted once.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct WindowStrokePayload;
}  // namespace coop::net

namespace coop::window_stroke_sync {

// Store the session and register the window with the shared scan. Idempotent. Game thread.
void Install(coop::net::Session* session);

// Validate the active source, sender and draw ranges, then queue the dab for replay.
void OnReliable(const coop::net::WindowStrokePayload& payload, uint8_t senderPeerSlot);

// Per net-pump tick: install the stroke observer once, send observed dabs, replay received ones.
void Tick();

// Session teardown: drop queued dabs and release the brush material.
void OnDisconnect();

}  // namespace coop::window_stroke_sync
