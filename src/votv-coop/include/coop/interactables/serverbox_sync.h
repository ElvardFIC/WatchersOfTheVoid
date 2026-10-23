// coop/interactables/serverbox_sync.h -- the signal servers' break state, the host's: its verbs run there alone.
//
// A box's state is its broken flag, its `damaged` flag and the repair minigame type its break rolled, and the farm's
// three totals on the gamemode. Three verbs write it -- breakServer (eight callers, each a world event or a load),
// break_type (the desk's virus event) and fix (the player's repair minigame, the Kerfur, p_kerfus). The script gate
// watches the three BY NAME: a client refuses every one through its whole connected session, and its player's own
// repair -- fix called by the gamemode's repair widget -- goes to the host as a repair intent, which the host runs on
// its box when broken and within reach and answers, refused, with the state row to the presser alone. The host polls
// the state at about 1 Hz, tracking the last accepted row PER recipient, at once after a repair, and sends a joiner
// the row at its world-ready. A refused send keeps its debt through unchanged polls; a replacement occupant or world
// requires a fresh row. A client retains its latest row through wrapper/list readiness, raw-writes each box's flags
// and type, re-skins through the notify-free check() and mirrors the totals. Pending work is bound to its world,
// gamemode and session lifetime. Identity is the save-stable servers[] index. MTA shape: the server validates a client's request and answers
// a refusal with its own value (reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:2779-2793).

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct ServerStatePayload;
struct ServerRepairPayload;
}  // namespace coop::net

namespace coop::serverbox_sync {

// Cache the session and register the three watches (each once; a refusal by the gate is said once).
void Install(coop::net::Session* session);

// Per net-pump tick, game thread: the watches driven to settled, then, throttled to about 1 Hz, the HOST's poll and
// its per-recipient send on a change or delivery debt; client pending applies retry until resolved.
void Tick();

// HOST, game thread, at a joiner's ClientWorldReady edge: request the current row; retry a refused/unresolved send.
void QueueConnectBroadcastForSlot(int slot);

// Game thread (event_feed drain). CLIENT: retain/apply the latest host row; the host refuses a client's copy at
// its receive edge (client_authorship.h).
void OnReliable(const coop::net::ServerStatePayload& payload);

// Game thread (event_feed drain). HOST: a client's finished repair of a box; run, or answered with the state row.
void OnRepair(const coop::net::ServerRepairPayload& payload, int senderPeerSlot);

// Teardown: the poll baseline and the session.
void OnDisconnect();

// The lane's counters, for its drill.
struct Counts {
    uint64_t refusedBreaks = 0;   // CLIENT: breakServer and break_type refused
    uint64_t refusedFixes = 0;    // CLIENT: a fix from any caller but the repair widget
    uint64_t repairsSent = 0;     // CLIENT: its player's repairs sent to the host
    uint64_t repairsRun = 0;      // HOST: a client's repair run
    uint64_t repairsRefused = 0;  // HOST: a client's repair answered with the state row
};
Counts LaneCounts();

}  // namespace coop::serverbox_sync
