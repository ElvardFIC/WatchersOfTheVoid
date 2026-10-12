// coop/net/receive_budget.h -- the host's bound on what each seated client sends it, counted at the receive edge on
// every packet. A stream type has a bucket per slot at twice the send rate, and a datagram past it is dropped: a lost
// pose is one the next replaces, a lost voice frame a gap the jitter buffer covers, and the bound caps what the host
// relays to every other client. Every other packet counts against one message bucket per slot; a client that empties
// it is past anything a client of this build sends, so the host ends its link (Session::TakeBudget). The bound is the
// transport's; a kind's own cost is bounded in its lane. Net thread only, no lock.
//
// MTA counts each event a client triggers on an element, refused or not, toward a per-player threshold and raises an
// event for the server's scripts past it (CGame.cpp:2663-2711,4927-4976). Ours counts every packet, sync included,
// with numbers to match; a token bucket holds a burst and a rate in one, where MTA's fixed window, opened by the first
// event, passes up to twice its count across a boundary; the host answers by ending the link; and a rate of 0 turns it
// off, which MTA's row cannot (mtaserver.conf:316-319). MTA relays every near player's puresync (CPlayer.cpp:981-1028,
// zone 0 in CBandwidthSettings.h:37,52); the stream bound is Source's shape, a client's commands budgeted per server
// tick and the rest dropped (player.cpp:3100-3101,3243,3392-3414).

#pragma once

#include "coop/net/intent_bucket.h"
#include "coop/net/protocol.h"             // MsgType
#include "coop/player/players_registry.h"  // kMaxPeers

#include <cstdint>

namespace coop::net {

class ReceiveBudget {
public:
    // The host's rows, applied at its session start: `burst` messages at once, refilled at `perSecond`. A rate of 0
    // turns the message bound off.
    void Configure(long burst, long perSecond);
    // The streams' bound: `perSecond` datagrams a second per slot and type, as many at once. 0 turns it off.
    void ConfigureStreams(int perSecond);

    // One packet of `type` from `slot`'s connection `hConn` at `nowMs`. A new connection on the slot starts full. A
    // stream type is counted against its own bucket and only dropped past it (StreamOver); anything else is a message:
    // Emptied on the packet that empties the message bucket, Spent on every later one from that connection.
    enum class Take { Ok, StreamOver, Emptied, Spent };
    Take TakePacket(int slot, uint32_t hConn, MsgType type, uint64_t nowMs);

    const IntentBudget& Budget() const { return budget_; }

    // The arithmetic: a burst, its refill, the latch, a new connection's full bucket, the streams apart, a rate of 0.
    static bool RunSelftest();

private:
    // The stream types a client sends: the nine scalar channels and voice.
    static constexpr int kStreamTypes = 10;
    static int StreamIndex(MsgType type);  // -1 for a type counted as a message

    struct Slot {
        uint32_t     hConn = 0;
        bool         spent = false;  // emptied once: latched until the slot's connection changes
        IntentBucket messages;
        IntentBucket streams[kStreamTypes];
    };
    Slot& For(int slot, uint32_t hConn);

    IntentBudget budget_{0.0f, 0.0f};  // off until Configure: the numbers live in the host's rows
    IntentBudget streamBudget_{0.0f, 0.0f};
    Slot slots_[coop::players::kMaxPeers];
};

}  // namespace coop::net
