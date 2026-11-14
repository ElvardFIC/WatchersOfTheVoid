#pragma once
#include "coop/net/peer_limits.h"
#ifdef KERFUS_COMMAND_FIXTURE
#include <functional>
#endif
#ifdef SERVED_PLAYER_FIXTURE
namespace coop { class RemotePlayer; }
#endif
namespace coop::players {
inline constexpr uint8_t kMaxPeers=coop::net::kPeerCapacity;
inline constexpr uint8_t kPeerIdHost=0;
#ifdef KERFUS_COMMAND_FIXTURE
inline std::function<void()> localHook;
inline void* testLocal=nullptr;
#endif
#ifdef SERVED_PLAYER_FIXTURE
inline coop::RemotePlayer* testPuppets[coop::net::kPeerCapacity]{};
inline std::function<void()> puppetHook;
#endif
class Registry {
public:
    static Registry& Get(){static Registry registry;return registry;}
#ifdef SERVED_PLAYER_FIXTURE
    coop::RemotePlayer* Puppet(uint8_t slot){auto callback=puppetHook;if(callback)callback();return slot<kMaxPeers?testPuppets[slot]:nullptr;}
#endif
    void* Local(){
#ifdef KERFUS_COMMAND_FIXTURE
        auto callback=localHook;if(callback)callback();return testLocal;
#else
        return this;
#endif
    }
};
}
