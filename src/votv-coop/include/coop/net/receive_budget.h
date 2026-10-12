// coop/net/receive_budget.h -- the host's bound on what each seated client sends it, at the receive edge. Reliable
// messages: one token bucket per slot, counting every arrival whatever its kind; a client that empties it is past what
// any client of this build sends, so the host ends its link (Session's edge). Stream datagrams: one bucket per slot and
// stream type at twice the send rate; a datagram past it is dropped before it is stored or relayed, since a stream is
// newest-wins. The bound is the transport's; a kind's own cost is bounded in its lane. Net thread only, no lock.

#pragma once

#include "coop/net/intent_bucket.h"
#include "coop/net/protocol.h"             // MsgType
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

    // The streams' bound: `perSecond` datagrams a second per slot and type, as many at once. 0 turns it off.
    void ConfigureStreams(int perSecond);
    // One stream datagram of `type` from `slot`'s connection `hConn` at `nowMs`: false past its type's rate. A type
    // the bound does not cover (the reliable lane, a batch only the host sends) passes.
    bool TakeStream(int slot, uint32_t hConn, MsgType type, uint64_t nowMs);

    const IntentBudget& Budget() const { return budget_; }
    bool On() const { return budget_.perSecond > 0.0f; }

private:
    // The stream types a client sends: the nine scalar channels and voice.
    static constexpr int kStreamTypes = 10;
    static int StreamIndex(MsgType type);  // -1 for a type not bounded here

    struct Slot {
        uint32_t     hConn = 0;
        bool         spent = false;  // emptied once: latched until the slot's connection changes
        IntentBucket reliable;
        IntentBucket streams[kStreamTypes];
    };
    Slot& For(int slot, uint32_t hConn);

    IntentBudget budget_{0.0f, 0.0f};  // off until Configure: the numbers live in the host's rows
    IntentBudget streamBudget_{0.0f, 0.0f};
    Slot slots_[coop::players::kMaxPeers];
};

}  // namespace coop::net
