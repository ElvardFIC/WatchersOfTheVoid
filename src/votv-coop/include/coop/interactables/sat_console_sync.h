// coop/interactables/sat_console_sync.h -- the SAT console's shared commands run on the host.
//
// A machine's SAT panels share one terminal. A client's shared-world command is
// sent with its dish/ROOT, name and panel context to a private host terminal kept
// by typist identity. Its world effects run once and cross through their own lanes. Each
// line that terminal prints is ordered by the host and sent to every display, including the origin.
// Local commands keep their local effects and submit their output to that same order. Only the host
// produces unsolicited terminal alerts; clr and recent join history use the same display stream.
// The busy level belongs to its typist. A typist that leaves mid-command leaves the command
// running, and back, is bound to its terminal again as its world is ready; the last client's leave ends
// the session and discards its terminals. MTA sends commands to the server as the player
// and echoes to that player alone (reference/mtasa-blue/Client/mods/deathmatch/ClientCommands.cpp:91,
// reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:2406); MTA also runs the client's own
// handlers first (:60), which here would run the one game handler twice, so a line runs on one machine.
#pragma once

// Queued logs/busy levels belong to one session/world and recipient occupancy.
// History seeds own a revision and include current busy state; refused sends
// retry without replaying native commands. Capacity covers all remote seats.
// Native input is consumed before submission. Invocation-owned command frames
// and per-row playback scopes keep nested/canceled calls from sharing ownership.

#include <cstddef>
#include <cstdint>

namespace coop::net {
class Session;
struct BlobChunkPayload;
}

namespace coop::sat_console_sync {

void Install(coop::net::Session* s);
void Tick();
void OnChunk(const coop::net::BlobChunkPayload& p, uint8_t senderSlot);
// A peer's world is ready: a typist back at a terminal still running is bound to it again. A peer's slot
// is gone: its chunks and owed blobs are forgotten; on the host its terminal is kept.
void OnPeerWorldReady(int slot);
void OnPeerGone(uint8_t slot);
void OnDisconnect();

// [dev] What the lane did, for the drill: the lines this client sent to run on the host, and on the host
// the lines it ran for clients, the lines those runs printed back, the typists bound again to a terminal
// they had left, and those whose terminal was busy then; and the terminals the host keeps.
struct Counts {
    uint64_t linesSent = 0;
    uint64_t linesRun = 0;
    uint64_t linesReturned = 0;
    uint64_t rebinds = 0;
    uint64_t busyRebinds = 0;
};
Counts LaneCounts();
size_t KeptTerminals(void** out, size_t cap);

}  // namespace coop::sat_console_sync
