// coop/world/explosion_request_internal.h -- what the explosion request lane's files share: the host's per-slot
// queues, the refusal reasons with their counters, and the session. Declarations only; the definitions sit in
// explosion_request.cpp. Impl-private: nothing outside coop/world includes this.

#pragma once

#include "coop/net/intent_queue.h"
#include "coop/net/protocol.h"

#include <cstddef>
#include <cstdint>

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::explosion_request::detail {

// The queue caps, initial values: a sender at its cap is refused as QueueFull.
inline constexpr size_t kFiredCap     = 64;
inline constexpr size_t kShotBlastCap = 64;
inline constexpr size_t kBodyBlastCap = 4;

// Why a message was refused, one counter each.
enum class Refusal : uint8_t { Malformed, DuplicateShot, QueueFull, Count };

// Snake case: "malformed", "duplicate_shot", "queue_full".
const char* RefusalName(Refusal r);

// One refusal from `slot`: the tally's note (steady clock) and the counter.
void Refuse(uint8_t slot, Refusal r);

// The session Install stored; null before it.
coop::net::Session* SessionPtr();

struct FiredItem {
    coop::net::ProjectileFiredPayload p;
    uint64_t                          recvMs;
};
struct RequestItem {
    coop::net::ExplosionRequestPayload p;
    uint64_t                           recvMs;
};

// One sender: its shots, and its blasts split by what caused them. Cleared with its occupant.
struct Slot {
    coop::net::IntentQueue<FiredItem>   fired;
    coop::net::IntentQueue<RequestItem> shotBlast;
    coop::net::IntentQueue<RequestItem> bodyBlast;
};

// The slot's state; the one PerSlotState<Slot>.
Slot& SlotOf(uint8_t slot);

struct Counters {
    uint64_t refused[static_cast<int>(Refusal::Count)];
};
Counters& MutCounters();

}  // namespace coop::explosion_request::detail
