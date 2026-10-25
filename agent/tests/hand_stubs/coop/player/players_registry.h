#pragma once
#include <cstdint>
namespace coop {class RemotePlayer;}
namespace coop::players {
inline constexpr uint8_t kMaxPeers=4;
class Registry {
public:
 static Registry& Get();
 void* Local(){return nullptr;}
 uint8_t LocalPeerId()const;
 coop::RemotePlayer* Puppet(uint8_t);
};
}
