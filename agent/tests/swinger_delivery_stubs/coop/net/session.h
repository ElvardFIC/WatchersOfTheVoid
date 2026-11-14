#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <vector>
namespace coop::net {
inline constexpr uint8_t kMaxPeers = kPeerCapacity;
enum class Role : uint8_t { Host, Client };
class Session {
public:
    Role mode = Role::Host;
    bool linked = true, sendOK = false;
    uint64_t stops = 0;
    std::array<uint32_t,kMaxPeers> generations = []{std::array<uint32_t,kMaxPeers> a{};a.fill(1);return a;}();
    std::array<bool,kMaxPeers> ready = []{std::array<bool,kMaxPeers> a{};a.fill(true);return a;}();
    std::array<bool,kMaxPeers> online = []{std::array<bool,kMaxPeers> a{};a.fill(true);return a;}();
    struct Sent { int slot; SwingerRestPosePayload payload; bool accepted; };
    std::vector<Sent> sent;
    std::function<void(const void*,size_t)> onSend;
    int depth = 0, maxDepth = 0;
    Role role() const { return mode; }
    bool connected() const { return linked; }
    uint64_t StopCount() const { return stops; }
    bool IsSlotConnected(int s) const { return linked && online.at(s); }
    bool IsSlotWorldReady(int s) const { return ready.at(s); }
    uint32_t peerGenerationForSlot(int s) const { return generations.at(s); }
    bool SendReliableToSlot(int slot, ReliableKind kind, const void* data, size_t size) {
        if (kind != ReliableKind::SwingerRestPose || size != sizeof(SwingerRestPosePayload)) std::abort();
        ++depth; if(depth > maxDepth) maxDepth = depth;
        SwingerRestPosePayload before{}; std::memcpy(&before,data,size);
        auto fn=std::move(onSend);onSend={};if(fn)fn(data,size);
        // Read after the callback to catch references into replaced queue storage.
        SwingerRestPosePayload after{}; std::memcpy(&after,data,size);
        if(std::memcmp(&before,&after,size)) std::abort();
        sent.push_back({slot,after,sendOK}); --depth;
        return sendOK;
    }
};
}
