// coop/net/receive_budget.h -- the host's bound on how many reliable messages each seated client may send it: one
// token bucket per slot at the receive edge, counting every arrival whatever its kind. A client that empties it is
// past what any client of this build sends, so the host ends its link (Session's edge); the bound is the transport's,
// and a kind's own cost is bounded in its lane. Net thread only, no lock.

#pragma once

#include "coop/net/intent_bucket.h"
#include "coop/player/players_registry.h"  // kMaxPeers

#include <cstdint>

namespace coop::net {

class ReceiveBudget {
public:
    // The host's rows, applied at its session start: `burst` messages at once, refilled at `perSecond`. A rate of 0
    // turns the bound off.
    void Configure(long burst, long perSecond);

    // One reliable from `slot`'s connection `hConn` at `nowMs`. A new connection on the slot starts with a full bucket.
    // Emptied once, the call that empties it; Spent on every later call for that connection.
    enum class Take { Ok, Emptied, Spent };
    Take TakeReliable(int slot, uint32_t hConn, uint64_t nowMs);

    const IntentBudget& Budget() const { return budget_; }
    bool On() const { return budget_.perSecond > 0.0f; }

private:
    struct Slot {
        uint32_t     hConn = 0;
        bool         spent = false;  // emptied once: latched until the slot's connection changes
        IntentBucket reliable;
    };
    Slot& For(int slot, uint32_t hConn);

    IntentBudget budget_{0.0f, 0.0f};  // off until Configure: the numbers live in the host's rows
    Slot slots_[coop::players::kMaxPeers];
};

}  // namespace coop::net
