// coop/interactables/atv_index_internal.h -- what the ATV index (atv_index.cpp) shares with the rest of
// the lane. atv_index.cpp owns the hub's scan callbacks that fill atv_sync.cpp's map, the host's announce
// of that index (AtvSpawn / AtvDestroy) and the test of a key that may name an ATV on the wire;
// atv_sync.cpp reads them at the connect snapshot and the disconnect and asks for the registration,
// atv_runtime_mirror.cpp asks which keys it may take. Nothing outside coop/interactables/ includes this.
// Game thread only.

#pragma once

#include "coop/interactables/atv_sync_internal.h"  // AtvEntry

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace coop::net { struct WireKey; }

namespace coop::atv_sync::detail {

// atv_sync.cpp's state, defined there and filled by the index: the lane's session and the key -> record map.
extern std::atomic<coop::net::Session*>        g_session;
extern std::unordered_map<std::wstring, AtvEntry> g_atvs;
// World gen of the last completed hub pass (a stale-gen index is EMPTY).
extern uint32_t                                g_indexGen;

// Whether `key` can name an ATV on the wire: not empty, not "None", at most 31 chars (what a WireKey
// carries). A refusal is logged once per key; `side` ("host" / "client") says whose it is.
bool KeyNamesAnAtv(const std::wstring& key, const char* side);
// The same test on a received key, which also refuses a declared length past what the field holds.
bool WireKeyNamesAnAtv(const coop::net::WireKey& key, const char* side);

// Announces the ATV filed under `key` to every connected client (slot < 0) or seeds one joiner
// (slot >= 0). True when the packet was queued.
bool SendAtvSpawn(const std::wstring& key, void* actor, int slot);

// A session ended: the broadcast counter restarts and the refused keys may be said again.
void ResetAnnounceState();

// Registers the index's scan callbacks with the hub; latched, safe to call every tick.
void RegisterWithScanHub();

}  // namespace coop::atv_sync::detail
