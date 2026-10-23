// coop/interactables/atv_index_internal.h -- what the ATV index (atv_index.cpp) shares with atv_sync.cpp.
// atv_index.cpp owns the runtime-spawned identity (synthetic wire keys and the set of save-placed
// ones) and the hub's scan callbacks that fill atv_sync.cpp's map; atv_sync.cpp reads the identity at
// the connect snapshot, the mirror adoption and the disconnect, and asks for the registration. Nothing
// outside coop/interactables/ includes this. Game thread only.

#pragma once

#include "coop/interactables/atv_sync_internal.h"  // AtvEntry

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace coop::atv_sync::detail {

// atv_sync.cpp's state, defined there and filled by the index: the lane's session and the key -> record map.
extern std::atomic<coop::net::Session*>        g_session;
extern std::unordered_map<std::wstring, AtvEntry> g_atvs;

// Host: real keys seen before any client connected, and the actors present then.
extern std::unordered_set<std::wstring>        g_savePlacedKeys;
extern std::unordered_set<void*>               g_savePlacedActors;
// Actor -> synthetic wire key (host runtime-spawned + client mirror).
extern std::unordered_map<void*, std::wstring> g_synthForActor;
// Host: the monotonic synthetic-key id.
extern uint32_t                                g_synthCounter;
// World gen of the last completed hub pass (a stale-gen index is EMPTY).
extern uint32_t                                g_indexGen;

bool IsSynthKey(const std::wstring& k);

// Host: announces a runtime ATV so clients fresh-spawn a native mirror; slot < 0 broadcasts, slot >= 0
// seeds one joiner.
void SendAtvSpawn(const std::wstring& synthKey, void* actor, int slot);

// Registers the index's scan callbacks with the hub; latched, safe to call every tick.
void RegisterWithScanHub();

}  // namespace coop::atv_sync::detail
