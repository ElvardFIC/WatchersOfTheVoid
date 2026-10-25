// coop/session/net_pump_world_ready_internal.h -- INTERNAL, src-tree only: shared by net_pump.cpp and
// net_pump_world_ready.cpp; nothing else includes it. The public API stays in
// include/coop/session/net_pump.h.

#pragma once

namespace coop::net { class Session; }

namespace coop::net_pump {

// Forget the world-ready announce state: the next connection announces again and re-stamps its world.
// (defined in net_pump_world_ready.cpp)
void ForgetWorldReadyAnnounce();

// The client's world-ready announce for this tick: waits on the gates, sends ClientWorldReady once the
// world is coherent, and arms the re-announce after a world change. Returns the reason to leave the
// session, or null. A no-op on the host. Game thread.
// (defined in net_pump_world_ready.cpp)
const char* TickWorldReadyAnnounce(coop::net::Session& session, void* localNow, bool worldUp,
                                   bool isConnected, bool isHost);

}  // namespace coop::net_pump
