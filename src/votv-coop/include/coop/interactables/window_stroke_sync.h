// coop/interactables/window_stroke_sync.h -- the base's big bay window (Ad_window_C), whose dirt is a
// render target wiped one sponge dab at a time, replicated as dabs on ReliableKind::WindowStroke.
// Gameplay/network layer (principle 7); the engine is reached only through ue_wrap::window_canvas.
//
// SYMMETRIC, presser-authored. A stroke runs only on the peer whose camera launched it, so that
// peer observes its own dab (a Func post-hook on UCanvas::K2_DrawMaterial filtered to a window frame
// with an open stroke session), reads the held sponge's brush and sends the dab. Every other peer
// replays it through the window's own Canvas session, and the host relays a client's dab to the
// other clients, so the host's world -- and its save -- carries every peer's wipes. A joiner gets the
// window as it was in the host's transferred save.
// Known limits: a thrown sponge's dab is not sent (no hand to read its brush from); dabs that
// arrive while a signal plays wait for playback to end; the periodic dirt splotch still rolls on
// each peer independently.
// Queues are scoped to one running session/world/role and bounded to 256 dabs.
// Outgoing payloads are sealed at the first drain and retain their brush on refused
// sends; FIFO retries expire after 20 seconds. Each tick sends/replays at most eight.
// Unresolved native preparation retains its FIFO row; an admitted native draw is
// attempted once, including an uncertain failure.

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
