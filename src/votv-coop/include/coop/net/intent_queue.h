// coop/net/intent_queue.h -- one client's intents of one kind, queued on the host in arrival order and served one
// head per tick at the lane's budget: a client past its budget costs the host no work beyond a queue of `cap`
// entries, and what the lane cannot yet do (a target not ready, a line still being said) waits at the head without
// spending the budget. Past `cap` the push is refused, and the lane counts it and answers as it does today. One
// instance per slot and kind, inside the lane's PerSlotState, so the queue and its bucket clear with the occupant;
// there is no Clear. One thread per instance, as IntentBucket.

#pragma once

#include "coop/net/intent_bucket.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>

namespace coop::net {

// What a served head did: Done spends its token and leaves the queue, Wait gives the token back and stays.
enum class Served : uint8_t { Done, Wait };

template <class T>
class IntentQueue {
public:
    // Queues `v` behind the others: false when `cap` entries are already queued (nothing is queued then).
    bool Push(T&& v, size_t cap) {
        if (q_.size() >= cap) return false;
        q_.push_back(std::move(v));
        return true;
    }

    // At most one head at `nowMs`, a steady clock's milliseconds: nothing when empty; else one token is taken, and
    // with it `fn(head)` answers Done (the head leaves) or Wait (the token returns, the head stays for the next call).
    template <class Fn>
    void Serve(const IntentBudget& b, uint64_t nowMs, Fn&& fn) {
        if (q_.empty()) return;
        if (!bucket_.Take(b, nowMs)) return;
        if (fn(q_.front()) == Served::Done) {
            q_.pop_front();
        } else {
            bucket_.Refund(b);
        }
    }

    size_t size() const { return q_.size(); }
    bool   empty() const { return q_.empty(); }

private:
    std::deque<T> q_;
    IntentBucket  bucket_;
};

// The queue's arithmetic: the cap's refusal, no token serving nothing, Done and Wait, and a value-initialised
// instance. Logs ALL PASS or each failure.
bool RunIntentQueueSelftest();

}  // namespace coop::net
