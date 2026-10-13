#pragma once
#include <cstdint>
namespace coop::players {
class Registry {
public:
 uint8_t slot=1;
 static Registry& Get(){static Registry r;return r;}
 uint8_t LocalPeerId() const{return slot;}
};
}
