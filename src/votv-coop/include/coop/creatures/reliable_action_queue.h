#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace coop::creature_actions {

// Reliable action streams use a non-zero, wrapping sequence. Keeping this in
// one place prevents each creature adapter from subtly disagreeing at wrap.
inline uint32_t NextSequence(uint32_t& sequence) {
    if (!++sequence) ++sequence;
    return sequence;
}

inline bool IsNewerSequence(uint32_t candidate, uint32_t lastSeen) {
    return candidate && (!lastSeen || static_cast<int32_t>(candidate - lastSeen) > 0);
}

enum class SendResult { Sent, Retry, DropPeer, Abort };

// Cue is deliberately structural: adapters keep their own packet, actor
// identity and playback state, while sharing bounded per-peer admission.
// Required fields are uint32_t peers and uint64_t expires.
template <size_t MaxPeers, class Queue, class Ready>
uint32_t QueueablePeers(Queue& queue, uint64_t now, size_t perPeerLimit, Ready&& ready) {
    static_assert(MaxPeers > 1 && MaxPeers <= 32);
    queue.erase(std::remove_if(queue.begin(), queue.end(),
        [=](const auto& cue) { return !cue.peers || now > cue.expires; }), queue.end());

    std::array<size_t, MaxPeers> pending{};
    for (const auto& cue : queue)
        for (size_t slot = 1; slot < MaxPeers; ++slot)
            if (cue.peers & (uint32_t{1} << slot)) ++pending[slot];

    uint32_t peers = 0;
    for (size_t slot = 1; slot < MaxPeers; ++slot)
        if (pending[slot] < perPeerLimit && ready(static_cast<int>(slot)))
            peers |= uint32_t{1} << slot;
    return peers;
}

template <class Queue>
void DropPeer(Queue& queue, int slot) {
    if (slot <= 0 || slot >= 32) return;
    const uint32_t bit = uint32_t{1} << slot;
    for (auto& cue : queue) cue.peers &= ~bit;
    queue.erase(std::remove_if(queue.begin(), queue.end(),
        [](const auto& cue) { return !cue.peers; }), queue.end());
}

// Flushes each peer in FIFO order while allowing successful peers to advance
// past another peer's temporary send failure. Returning false means the
// adapter detected teardown/reentrancy and the caller must stop this tick.
template <size_t MaxPeers, class Queue, class Valid, class Ready, class Send>
bool FlushPerPeerFifo(Queue& queue, uint64_t now, Valid&& valid, Ready&& ready, Send&& send) {
    static_assert(MaxPeers > 1 && MaxPeers <= 32);
    uint32_t blocked = 0;
    size_t remaining = queue.size();
    for (size_t i = 0; remaining && i < queue.size(); --remaining) {
        const auto cue = queue[i];
        if (now > cue.expires || !valid(cue)) {
            queue.erase(queue.begin() + i);
            continue;
        }
        for (size_t slot = 1; slot < MaxPeers; ++slot) {
            const uint32_t bit = uint32_t{1} << slot;
            if (!(queue[i].peers & bit)) continue;
            if (!ready(static_cast<int>(slot))) {
                queue[i].peers &= ~bit;
                continue;
            }
            if (blocked & bit) continue;
            switch (send(cue, static_cast<int>(slot))) {
            case SendResult::Sent:
            case SendResult::DropPeer:
                queue[i].peers &= ~bit;
                break;
            case SendResult::Retry:
                blocked |= bit;
                break;
            case SendResult::Abort:
                return false;
            }
        }
        if (!queue[i].peers) queue.erase(queue.begin() + i);
        else ++i;
    }
    return true;
}

}  // namespace coop::creature_actions
