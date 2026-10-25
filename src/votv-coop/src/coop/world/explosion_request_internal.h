// coop/world/explosion_request_internal.h -- what the explosion request lane's files share: the host's per-slot
// queues and shot table, the budgets, the refusal note and the writable counters (their types are public), the
// judges, the client half's install and clear, and the session.
// Declarations only; the queue and counter definitions sit in explosion_request.cpp, the judges in
// explosion_request_host.cpp (the ATV's in explosion_request_atv.cpp) and the client half in
// explosion_request_client.cpp. Impl-private: nothing outside
// coop/world includes this.

#pragma once

#include "coop/net/intent_bucket.h"
#include "coop/net/intent_queue.h"
#include "coop/net/protocol.h"
#include "coop/world/explosion_recipe.h"
#include "coop/world/explosion_request.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::explosion_request::detail {

// The queue caps, initial values: a sender at its cap is refused as QueueFull.
inline constexpr size_t kFiredCap     = 64;
inline constexpr size_t kShotBlastCap = 64;
inline constexpr size_t kBodyBlastCap = 4;
inline constexpr size_t kAtvBlastCap  = 8;

// Each queue's own pacing, the only cadence of its judge: a person's fastest launcher input (the witch car fires
// per press), a detonate-all of every live sticky in one frame, one fuse or one burn, and one ATV's death.
inline constexpr coop::net::IntentBudget kFired     = {16.0f, 10.0f};
inline constexpr coop::net::IntentBudget kShotBlast = {64.0f, 10.0f};
inline constexpr coop::net::IntentBudget kBodyBlast = {2.0f, 1.0f / 3.0f};
inline constexpr coop::net::IntentBudget kAtvBlast  = {4.0f, 1.0f};

// The judged shots kept per slot: a 65th evicts the oldest sticky entry, else the oldest.
inline constexpr size_t kShotsMax = 64;

// The slot of a request kind in the counters' per-kind arrays.
constexpr size_t KindSlot(coop::explosion_recipe::RequestKind k) { return static_cast<size_t>(k) - 1; }

// One refusal from `slot`: the tally's note (steady clock) and the counter.
void Refuse(uint8_t slot, Refusal r);

// The session Install stored; null before it.
coop::net::Session* SessionPtr();

// The edge of every host receive: the sender's slot is a client's (1..kMaxPeers-1), and its world is ready. A sender
// still in its load window has no puppet, hand state or ATV to judge against; MTA drops a packet from a player that
// has not finished joining (reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:2835, :2957).
bool ValidSenderSlot(int senderSlot);
bool SenderWorldReady(uint8_t slot);

struct FiredItem {
    coop::net::ProjectileFiredPayload p;
    uint64_t                          recvMs;
};
struct RequestItem {
    coop::net::ExplosionRequestPayload p;
    uint64_t                           recvMs;
};
// An ATV's request, by its key: the ATV is found again when the item is served.
struct AtvItem {
    std::wstring key;
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
    coop::net::IntentQueue<AtvItem>     atvBlast;
    Shot                                shots[kShotsMax];
    size_t                              count = 0;
};

// The slot's state; the one PerSlotState<Slot>.
Slot& SlotOf(uint8_t slot);

// The counters (coop/world/explosion_request.h), writable by the lane's own files.
Counters& MutCounters();

// The client half, in explosion_request_client.cpp. InstallClient takes the fire watches (retried on a throttle
// until each is live or given up, both roles) and the object index's death listener; ClearClient forgets the
// session's pending births, sent shots, outbox, shot counter and one-shot logs; ClearClientWorld forgets only the
// pending births, the sent shots and the outbox, for a world change. TickClient sends the outbox head first as the
// player's hand announce allows, each tick of an active client session.
void InstallClient();
void TickClient();
void ClearClient();
void ClearClientWorld();

// The ATV family's once-per-session log latch, in explosion_request_atv.cpp; cleared with the session.
void ClearAtvClient();

// The four judges, one per queue, the first three in explosion_request_host.cpp and the ATV's in
// explosion_request_atv.cpp. Each returns Done (accepted or refused) except at its Waits, each bounded for an honest
// sender: the hand lane's refill, or the slot's own Fired queue draining. Each measures its windows from its item's
// arrival, never from the tick that serves it. The ATV's never waits.
coop::net::Served JudgeShot(uint8_t slot, Slot& s, const FiredItem& it);
coop::net::Served JudgeShotBlast(uint8_t slot, Slot& s, const RequestItem& it);
coop::net::Served JudgeBody(uint8_t slot, Slot& s, const RequestItem& it);
coop::net::Served JudgeAtvBlast(uint8_t slot, const AtvItem& it);

// The one spawn of a request's blast, for every family, in explosion_request_host.cpp: the host's own parameters for
// `src` at `level`, spawned natively inside the lane's scope so the birth seam leaves it alone. Refuses and counts
// only Unresolved and SpawnFailed, and returns false for either; the caller counts and says its own accept.
bool HostSpawn(uint8_t slot, ue_wrap::explosion_source::Kind src, const float loc[3], float level);

}  // namespace coop::explosion_request::detail
