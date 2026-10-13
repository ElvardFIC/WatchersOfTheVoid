// coop/net/latest_wins.h -- a client's changes of one single-valued state on the host, the newest kept: the lane
// applies a changed value at most at its budget, and past it holds the newest value as owed, applied once a token
// refills. An honest player's last choice always lands; a fast or hostile sender loses only the values in between,
// and costs the host no more applications than the budget. A refusal would leave the host on an older value, so
// nothing is refused. One instance per slot and kind, beside the lane's pending value in one PerSlotState so both
// clear with the occupant. A keyed state (one value per device) or a claim that owes its sender an answer is not
// this helper's: it is a verb, paced in its lane. One thread per instance, as IntentBucket.
//
// MTA relays a player's sync to the others at a zone's interval and skips what falls inside it, the next update
// carrying the state then current (Server/mods/deathmatch/logic/CPlayer.cpp:981-1028; CBandwidthSettings.h:98, set at
// :40,55,70). Its sync is a stream, so a skipped update is always followed by a newer one; the states this paces are
// sent once per change, so the newest skipped value is kept and paid when the budget allows -- the divergence.

#pragma once

#include "coop/net/intent_bucket.h"

#include <cstdint>

namespace coop::net {

class LatestWins {
public:
    // A changed value at `nowMs`, a steady clock's milliseconds, which the lane has stored as its pending one: true
    // when a token pays for it now (apply it; nothing stays owed), else the value is owed.
    bool Offer(const IntentBudget& budget, uint64_t nowMs) {
        owed_ = !bucket_.Take(budget, nowMs);
        return !owed_;
    }

    // The payment, asked each tick: true when a value is owed and a token pays for it now (apply the pending value).
    bool Due(const IntentBudget& budget, uint64_t nowMs) {
        if (!owed_ || !bucket_.Take(budget, nowMs)) return false;
        owed_ = false;
        return true;
    }

    // The newest value equals the applied one: nothing is owed.
    void Withdraw() { owed_ = false; }

    bool Owed() const { return owed_; }

    // The arithmetic: a first value at once, a debt past the burst, paid only at a refill and only when owed, a
    // withdrawal, and a value-initialised instance owing nothing. Logs ALL PASS or each failure.
    static bool RunSelftest();

private:
    IntentBucket bucket_;
    bool         owed_ = false;
};

}  // namespace coop::net
