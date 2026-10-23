#pragma once
#include "../../../../../src/votv-coop/include/coop/net/protocol.h"
namespace coop::net {
inline constexpr int kMaxPeers = kPeerCapacity;
enum class Role { Host, Client };
}
