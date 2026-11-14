#pragma once
#include <cstdint>
namespace coop::players {
inline constexpr int kMaxPeers=32;
class Registry {
public:
 void* body=nullptr;uint8_t peer=1;
 static Registry& Get(){static Registry r;return r;}
 void* Local(){return body;}
 uint8_t LocalPeerId()const{return peer;}
};
}
