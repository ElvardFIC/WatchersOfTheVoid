#pragma once
#include <cstdint>
#include <vector>
namespace coop::players {
inline constexpr uint8_t kPeerIdUnknown = 0xff;
class Registry {
public:
    struct Body { void* actor; int serial; uint8_t slot; };
    using PuppetCountFn = void (*)(int);
    static Registry& Get() { static Registry value; return value; }
    std::vector<Body> bodies;
    bool IsPuppetBody(void* actor);
    uint8_t PeerIdOfBody(void* actor);
    PuppetCountFn listener = nullptr;
    PuppetCountFn SetPuppetCountListener(PuppetCountFn fn) {
        const auto before = listener; listener = fn;
        if (fn) fn(static_cast<int>(bodies.size()));
        return before;
    }
};
}
