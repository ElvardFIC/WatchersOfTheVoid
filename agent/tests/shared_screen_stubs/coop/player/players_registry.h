#pragma once
#include <cstdint>
namespace coop::players {
class Registry {
public:
    uint8_t local = 0;
    static Registry& Get() { static Registry registry; return registry; }
    uint8_t LocalPeerId() const { return local; }
};
}
