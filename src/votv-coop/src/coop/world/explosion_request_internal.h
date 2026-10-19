// coop/world/explosion_request_internal.h -- what the explosion request lane's files share: the host's per-slot
// queues and shot table, the budgets, the refusal reasons with their counters, the judges, and the session.
// Declarations only; the queue and counter definitions sit in explosion_request.cpp and the judges in
// explosion_request_host.cpp. Impl-private: nothing outside coop/world includes this.

#pragma once

#include "coop/net/intent_bucket.h"
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

// Each queue's own pacing, the only cadence of its judge: a person's fastest launcher input (the witch car fires
// per press), a detonate-all of every live sticky in one frame, and one fuse or one burn.
inline constexpr coop::net::IntentBudget kFired     = {16.0f, 10.0f};
inline constexpr coop::net::IntentBudget kShotBlast = {64.0f, 10.0f};
inline constexpr coop::net::IntentBudget kBodyBlast = {2.0f, 1.0f / 3.0f};

// The judged shots kept per slot: a 65th evicts the oldest sticky entry, else the oldest.
inline constexpr size_t kShotsMax = 64;

// Why a message was refused, one counter each.
enum class Refusal : uint8_t {
    Malformed, DuplicateShot, QueueFull,
    NoLauncher, FungunRule, NotSeated, OriginBound, UnknownShot, Reach, NoExplosive, HandBound, NoPuppet, PuppetDead,
    Unresolved, SpawnFailed,
    Count
};

// Snake case: "malformed", "duplicate_shot", "queue_full", "no_launcher", "fungun_rule", "not_seated",
// "origin_bound", "unknown_shot", "reach", "no_explosive", "hand_bound", "no_puppet", "puppet_dead", "unresolved",
// "spawn_failed".
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

// A judged shot: what the request that follows is bounded against. `recvMs` is the host's receive time of the
// shot, so a paced queue never eats a rocket's reach.
struct Shot {
    uint32_t shotId;
    uint8_t  projClass;
    uint8_t  launcher;
    float    origin[3];
    uint64_t recvMs;
};

// One sender: its shots, its blasts split by what caused them, and the table of its judged shots. Cleared with its
// occupant.
struct Slot {
    coop::net::IntentQueue<FiredItem>   fired;
    coop::net::IntentQueue<RequestItem> shotBlast;
    coop::net::IntentQueue<RequestItem> bodyBlast;
    Shot                                shots[kShotsMax];
    size_t                              count = 0;
};

// The slot's state; the one PerSlotState<Slot>.
Slot& SlotOf(uint8_t slot);

struct Counters {
    uint64_t refused[static_cast<int>(Refusal::Count)];
    uint64_t shotsAccepted, requestsAccepted, evicted, expired;
};
Counters& MutCounters();

// The three judges, one per queue, in explosion_request_host.cpp. Each returns Done (accepted or refused) except
// at its Waits, each bounded for an honest sender: the hand lane's refill, or the slot's own Fired queue draining.
// `now` is the net clock's milliseconds.
coop::net::Served JudgeShot(uint8_t slot, Slot& s, const FiredItem& it, uint64_t now);
coop::net::Served JudgeShotBlast(uint8_t slot, Slot& s, const RequestItem& it, uint64_t now);
coop::net::Served JudgeBody(uint8_t slot, Slot& s, const RequestItem& it, uint64_t now);

}  // namespace coop::explosion_request::detail
