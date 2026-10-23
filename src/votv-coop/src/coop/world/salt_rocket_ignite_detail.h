// coop/world/salt_rocket_ignite_detail.h -- what the salt rocket ignite lane's files share: the host's per-sender
// queues, the budget and caps, the refusal note and the writable counters (their type is public), and the host half's
// entry points. Impl-private: nothing outside coop/world includes this. The host half is in
// salt_rocket_ignite_host.cpp, the client half and the lifecycle in salt_rocket_ignite.cpp.

#pragma once

#include "coop/net/intent_bucket.h"
#include "coop/net/intent_queue.h"
#include "coop/net/protocol.h"
#include "coop/world/salt_rocket_ignite.h"

#include <cstddef>
#include <cstdint>

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::salt_rocket_ignite::detail {

// A person lights a rocket a few times a second at most, and one click is at most two intents (the lighter's attempt
// and its ignite): a sender is served four a second after a burst of four, with eight pending (the door verbs' bound).
inline constexpr coop::net::IntentBudget kBudget{4.0f, 4.0f};
inline constexpr size_t kMaxPending = 8;

// The reach a sender's light is judged against: twice the default armLength, the wide lanes' reach, which
// coop/element/intent_authority pads with the rocket's bounds and the puppet's lag.
inline constexpr float kReachUU = 400.0f;

// The window the sender's hand must have held the tool in, measured against the intent's receive time: EX-2's flat
// window, which covers the hand lane's pacing.
inline constexpr uint64_t kToolWindowMs = 2000;

// A refusal after the first is said at most this often per sender.
inline constexpr uint64_t kRefusalSayMs = 10000;

struct Item {
    coop::net::PropIgniteIntentPayload p;
    uint64_t                           recvMs;
};

// One sender: its queued intents with their bucket, and the latch that clears with its occupant.
struct Slot {
    coop::net::IntentQueue<Item> queue;
    uint64_t                     nextSayMs = 0;
};

// The session Install stored; null before it.
coop::net::Session* SessionPtr();

// The counters (coop/world/salt_rocket_ignite.h), writable by the lane's own files.
Counters& MutCounters();

// The sender's state; the one PerSlotState<Slot>.
Slot& SlotOf(uint8_t slot);

// One refusal from `slot`: the counter, and a line the first time and then at most every kRefusalSayMs.
void Refuse(uint8_t slot, Refusal r);

// Host: serve each slot's head; forget every slot's state.
void HostServe(coop::net::Session& s);
void HostReset();

}  // namespace coop::salt_rocket_ignite::detail
