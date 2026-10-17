// coop/net/send_rate_control.h -- what a peer's link actually does, measured per slot.
//
// The transport offers no bandwidth estimate: it writes a connection's send rate once at connect
// from the ping and thereafter only clamps it, so a link past a few milliseconds of ping runs at
// the configured floor for its whole life. This module measures what replaces it -- the goodput
// the peer acknowledges, and a round trip timed on the lane the pose stream rides -- and DECIDES
// the rate from it. An idle link is probed slowly and silently, because a baseline taken only
// under load is the queue it is meant to reveal. This is the only place a rate is DECIDED: the
// two writes are `connection_tuning`'s opening one and the session's per-decision one, and both
// carry a value from here. `net.ratecontrol=0` leaves the transport at its stock rate and
// `net.sendrate_kbs` pins a fixed one; neither is a fallback, because the behaviour they restore
// was the defect. The law, the two refuted before it and their measurements: docs/send-path.md.
//
// Threading: three byte counters are atomics written at the send and receive choke points from
// whichever thread reached them, and a fourth ARMS a slot's teardown. The rest is net-thread-only.

#pragma once

#include "coop/net/protocol.h"             // LinkProbePayload
#include "coop/player/players_registry.h"  // kMaxPeers

#include <atomic>
#include <cstdint>

namespace coop::net {

class SendRateControl {
public:
    // Below this much work in flight, in either direction, a link counts as idle: it reports
    // nothing, and a rate is a ceiling, so with nothing queued it costs nothing to leave alone.
    static constexpr int64_t kIdleDemandBytes = 8 * 1024;

    // Probe cadence, busy and idle. The idle one is what makes `rttMin` a BASELINE rather than a
    // reading of the bottleneck queue. 1 Hz of a 32-byte datagram is 32 B/s per peer, so an idle
    // link is measured for free.
    static constexpr uint64_t kProbeIntervalMs     = 100;
    static constexpr uint64_t kIdleProbeIntervalMs = 1'000;
    // How many probes may be outstanding at once. A ninth would ask nothing the eight unanswered
    // ones have not, so the mint is skipped and counted; retirement is kProbeLostMs, not this.
    static constexpr int      kMaxOutstanding  = 8;
    // When an unanswered probe stops being a slow reading and becomes a lost one. Our policy, not
    // the transport's, whose connected-timeout is twice it -- so the link is still live when we
    // retire a probe, and an echo that arrives later lands in `stray`.
    static constexpr uint64_t kProbeLostMs = 5'000;

    // The window rttMin is taken over, in one-second buckets: long enough that a queue filling for
    // a few seconds cannot drag the baseline up behind it.
    static constexpr int kRttMinBuckets = 10;

    // The BASE delay: the smallest round trip a probe measured while the wire held at most one
    // packet of ours, kept as the minimum of the last kBaseHistMinutes one-minute minima (RFC 6817
    // section 2.5 keeps ten; libutp's base-delay history keeps thirteen). A minute is the unit so a
    // route change ages out in ten minutes, and an hour-long queue cannot become the baseline.
    static constexpr int kBaseHistMinutes = 10;
    static constexpr uint64_t kBaseMinuteMs = 60'000;
    // A probe is ADMITTED to the base when the reliable bytes on the wire and unacknowledged at the
    // moment it went out were no more than this: one packet, so the probe did not wait behind a
    // queue of our own making. It is the reading `queue = inflight - base` is measured from.
    static constexpr int kBaseAdmitUnackedBytes = 1200;

    // ---- The control law's constants ----

    // The envelope. The floor is an honest minimum -- a join on a link that thin is slow, not
    // broken. The ceiling is the save pump's own output, the biggest producer in the tree:
    // kChunksPerTick (coop/save/save_transfer.cpp) x kSaveChunkBytes (coop/net/protocol.h) at
    // 60 Hz. A rate above it paces nothing that exists.
    static constexpr int64_t kFloorBps   = 32 * 1024;
    static constexpr int64_t kCeilingBps = 4LL * 56 * 1024 * 60;

    // Where a link opens, for the connect-time write: the window between the transport's
    // ping-derived guess and this controller's first decision belongs to nobody otherwise. Opening
    // wide is the overdrive this controller exists to end; opening at the floor makes every join
    // on a good link crawl. 4.5x the floor is about 1% of the ceiling.
    static constexpr int64_t kStartRateBps = 145 * 1024;

    // THE ANCHOR, and the one guard that would have stopped both refuted laws on its own: a rate
    // may not stand above this multiple of the delivery it is measured against, while there is
    // reliable work to measure. The margin above 1.0 is paid for in retransmission, and a reliable
    // retry outranks the lane-0 pose datagram, so it is the probe margin BBR uses and no more.
    static constexpr int kOverdriveNum = 5, kOverdriveDen = 4;

    // THE BRAKE and its dead band, in milliseconds of delivery standing on the wire. The climb
    // stops where health stopped and the brake fires an order of magnitude under the collapse;
    // between them the rate is left alone, because a controller with no dead band hunts.
    static constexpr int64_t kQueueClimbMs = 150;
    static constexpr int64_t kQueueBrakeMs = 400;

    // The climb, per decision while the brake is silent. The additive term is what carries a rate
    // off the floor, where a purely multiplicative step crawls.
    static constexpr int     kClimbNum = 5, kClimbDen = 4;
    static constexpr int64_t kClimbStepBps = 8 * 1024;

    // The delivery estimate's smoothing over the 10 Hz samples, as a shift: new = old - old/4 +
    // sample/4, a ~400 ms time constant. One 100 ms sample is too noisy to brake on, and a full
    // second is too slow for a join to climb inside.
    static constexpr int kServedEwmaShift = 2;
    // How many samples the ANCHOR takes its maximum over, the same span as the EWMA's time
    // constant. The anchor may NOT read the EWMA: while a link climbs, delivery equals the rate and
    // the EWMA lags it, so an anchor fed by it clamps below a rate that is already succeeding and
    // fights its own climb. A maximum has no lag on the way up and still expires in 400 ms.
    static constexpr int kServedPeakSamples = 1 << kServedEwmaShift;
    // And how many samples that estimate is worth dividing by. An EWMA is not a measurement before
    // its own time constant has passed, and the first sample of a transfer is the worst case:
    // the buffer is already deep while the estimate is one reading old, and `inflight` computed
    // from that pair reads as a stalled link. Until it is warm the link holds at its opening rate.
    static constexpr int kServedWarmupSamples = 1 << kServedEwmaShift;

    // The rate a connection opens at, for the connect-time write.
    static int64_t StartRateBps();

    // What this slot's link actually DELIVERS per second, smoothed: the same `served` the anchor
    // bounds against and the per-second line prints. This is what a queue on this link drains at,
    // and so the only honest divisor for a bound expressed in seconds of queue -- the transport's
    // own paced rate is not that number. 0 until the estimate is warm. Net thread.
    int64_t ServedBps(int slot);

    // Session::Start: a new session measures from zero. Called before the net thread exists.
    // `controlEnabled` false leaves every link where the transport was configured and still
    // measures, which is the before-picture a drill compares against. `pinnedKbs` is the drill's
    // fixed-rate override in KB/s, 0 for none; it is resolved ONCE here rather than per connection,
    // because `ResolveInt` re-reads the ini and a mid-session edit would pin a steered link.
    void Reset(bool controlEnabled, long pinnedKbs);
    // Whether this session's links are paced by the law. The session's own resolved answer, so a
    // connection being tuned asks THIS rather than re-reading an ini that resolves live.
    bool Enabled() const { return controlEnabled_; }
    // The drill's fixed-rate override as resolved once at Start; 0 when none.
    long PinnedRateKbs() const { return pinnedKbs_; }
    // Slot teardown. The transport's pending counters restart with the next connection, so ours
    // must too, or the slot's next occupant inherits a byte debt that reads as delivery. Any
    // thread -- a host-side kick runs on the game thread -- so it ARMS only, and the net thread
    // performs the reset at its next touch: the reader's arm test and its counter read are two
    // operations, so no fence could keep the block below single-owner instead.
    void FreeSlot(int slot);

    // Reliable wire bytes the transport ACCEPTED for this slot: the whole packet, which is the
    // number the peer counts on arrival. Any thread, from the send choke points.
    void NoteReliableQueued(int slot, int bytes);
    // Unreliable wire bytes it accepted. Counted because the RATE paces the whole connection while
    // acknowledged delivery covers only the reliable half: anchoring a connection-wide rate to a
    // reliable-only measurement would clamp ordinary play, which is pose and voice, to the floor.
    // Nothing acknowledges these, so the offer stands in -- exact for knowing what the rate is FOR.
    void NoteUnreliableQueued(int slot, int bytes);
    // Reliable wire bytes this slot's link delivered to us, counting the same kinds the sender
    // counts. Net thread. It is the receiving end of the same quantity, which is what makes a
    // sender's goodput estimate checkable against a second, independent measurement.
    void NoteReliableReceived(int slot, int bytes);
    // The responder could not put an echo on the wire (the send buffer was at the brim). Counted
    // on the answering side, because to the prober a refused echo and a lost probe look identical.
    void NoteEchoRefused(int slot);

    // What the net thread read off one connection: the transport's send accounting, plus the depth
    // of our own queue in front of it. Passed in rather than read here, so this file stays free of
    // the transport API and the session keeps one status read per pass.
    struct LinkSample {
        int    pendingReliable     = 0;  // m_cbPendingReliable: handed over, not yet on the wire
        int    sentUnackedReliable = 0;  // m_cbSentUnackedReliable: on the wire, not yet acked
        int    pendingUnreliable   = 0;  // m_cbPendingUnreliable
        int    gnsRateBps          = 0;  // m_nSendRateBytesPerSecond: what it is pacing at now
        int    gnsPingMs           = -1; // m_nPing: an RTT floor (min-filtered), the cross-check
        size_t backlogBytes        = 0;  // SendBacklog::DepthBytes: queued before the transport
    };

    // Fold one sample for a slot (net thread, ~10 Hz) and emit that slot's line at 1 Hz.
    void Sample(int slot, const LinkSample& in, uint64_t nowMs);

    // The probe this slot owes, if one is due; false when the link is idle or a probe went out
    // within the cadence. Net thread. The caller sends `out` and, if the send fails, says so.
    bool NextProbe(int slot, uint64_t nowMs, LinkProbePayload& out);
    // The send of a minted probe failed. Expected, not exceptional: with the send buffer at the
    // brim -- the state a bulk transfer creates -- every new message is refused, probes included.
    void NoteProbeRefused(int slot, uint32_t token);
    // An echo came back. The round trip is measured against OUR record of when that token went out;
    // the payload is the token alone and carries no time, so an echo has none to invent one from.
    void NoteProbeReply(int slot, const LinkProbePayload& reply, uint64_t nowMs);

    // The rate this slot's link should now be paced at, or -1 when there is nothing to write --
    // control disabled, or the law left the rate where the connection already has it. Net thread,
    // and a PEEK: the decision is not spent until `NoteRateWritten` says it reached the transport.
    // Latching here would retire a decision a failed write never delivered, and the dead band can
    // leave the rate still for a long time, so one missed write would strand the link.
    int64_t PendingRateWrite(int slot);
    // The write landed. `us` is what it cost, folded into the per-second cost line beside the
    // status read's, because a config write per decision is a claim of its own. Net thread.
    void NoteRateWritten(int slot, int64_t bps, uint64_t us);

    // What the status reads themselves cost, per net-thread pass; reported once a second while any
    // link is busy. `links` is how many connections that pass read, which is why the line reports
    // passes and link samples as two numbers -- on a three-client host they differ by 3x.
    void NoteSampleCost(uint64_t us, int links, uint64_t nowMs);

private:
    static constexpr int kSlots = static_cast<int>(coop::players::kMaxPeers);

    struct Outstanding {
        uint32_t token  = 0;   // 0 = the entry is free
        uint64_t sentMs = 0;
        bool     admitted = false;  // minted with <= kBaseAdmitUnackedBytes on the wire: base material
    };

    // What one run of the law decided; the per-slot change of it is printed as a transition.
    enum class LawKind : uint8_t { Hold = 0, Brake, Climb, Dead };

    // Everything the net thread alone owns, in one struct so an armed teardown is one assignment
    // (the two counters beside it are atomics, written from any thread, and cannot be copied over).
    struct Measured {
        int64_t  demand = 0;          // in-flight work on OUR send side
        bool     busy = false;        // in flight in EITHER direction: the probe's cadence choice
        LinkSample last{};

        // The differentiated delivery: the previous sample, and the 1 Hz window the line reports.
        int64_t  prevDelivered = 0;
        uint64_t prevSampleMs = 0;
        bool     havePrev = false;
        int64_t  peakBps = 0;         // the fastest 100 ms interval inside the reported second

        uint64_t windowMs = 0;        // when the reported second began (0 = not started)
        int64_t  windowDelivered = 0;
        uint64_t windowRecv = 0;
        bool     sawTraffic = false;  // busy in either direction at any sample in this window
        bool     wasBusy = false;     // did the previous window report, so the quiet edge says so

        // The probe.
        uint32_t    probeCounter = 0;
        uint64_t    lastProbeMs = 0;
        Outstanding out[kMaxOutstanding];
        int         rttLastMs = -1;
        int         rttMinWindow = -1, rttMaxWindow = -1;
        int64_t     rttSumWindow = 0;
        int         rttCountWindow = 0;
        int         probesSent = 0, probesReplied = 0, probesRefused = 0, probesLost = 0;
        int         probesUnknown = 0;  // an echo for no token of ours
        int         probesSkipped = 0;  // the mint the full outstanding ring refused
        int         echoesRefused = 0;  // OUR answers the send buffer would not take
        int         rttBucketMin[kRttMinBuckets]{};
        uint64_t    rttBucketSec[kRttMinBuckets]{};
        // The base delay's history: the finished minutes' minima (a ring, oldest overwritten), the
        // minute being filled, and when it began. A minute with no admitted reply pushes nothing.
        int         baseHist[kBaseHistMinutes]{};
        int         baseCount = 0, baseNext = 0;
        int         baseCurMin = -1;
        uint64_t    baseMinuteStartMs = 0;

        // ---- The law's state: two smoothed byte rates and what they decided ----
        // Acknowledged delivery, smoothed. The term `inflight` is divided by, so RELIABLE-ONLY:
        // `m_cbSentUnackedReliable` is too, and dividing one class by another measures neither.
        int64_t  relBps    = 0;
        // The same plus unreliable bytes handed over: what the RATE is for, and the term the anchor
        // bounds against. Each comparison is given the traffic class it belongs to.
        int64_t  servedBps = 0;
        int      servedSamples = 0;    // until the EWMA is warm, neither guard may bind
        // The anchor's own view of the same quantity: the last kServedPeakSamples raw readings, of
        // which it takes the maximum. See kServedPeakSamples for why this may not be the EWMA.
        int64_t  servedRing[kServedPeakSamples]{};
        int      servedRingIdx = 0;
        int64_t  prevUnrel  = 0;       // the unreliable counter at the previous sample
        // The last decision's inputs, kept for the reported line: a field log that cannot show why
        // the rate moved is the state this whole arc started in.
        int64_t  inflightMs = 0;
        // inflight less the base delay (the whole inflight while no base exists), computed beside it
        // and printed; the law does not read it yet.
        int64_t  queueMs = 0;
        // The kind of the previous decision, so a change of it is one log line.
        LawKind  lastKind = LawKind::Hold;
        // What the law wants, and what the connection was last told, so a write costs one per move.
        // Both open at the rate `TuneConnection` already wrote, NOT at zero: a link whose demand
        // never reaches kIdleDemandBytes decides nothing, and zeroes here would report a rate of 0
        // while the transport paced the opening one. Set by Reset and by Take_'s armed teardown.
        int64_t  rateBps     = 0;
        int64_t  rateWritten = 0;
        // Which branch the law took, per reported second. `anchor` counts the decisions the
        // overdrive bound pulled back, which is the number that says whether it is load-bearing.
        int      climbCount = 0, brakeCount = 0, anchorCount = 0, deadCount = 0, holdCount = 0;
        int      rateWrites = 0;
    };

    struct Slot {
        // The three cumulative counters, written from any thread at the choke points.
        std::atomic<uint64_t> queuedBytes{0};
        std::atomic<uint64_t> unrelBytes{0};
        std::atomic<uint64_t> recvBytes{0};
        // Set by FreeSlot from whichever thread tore the slot down; consumed by the net thread.
        std::atomic<bool> resetArmed{false};
        Measured m;
    };

    // The slot's measured block, with an armed teardown performed first. Net thread only -- this is
    // the one door to `Measured`, which is what makes its single ownership checkable.
    Measured& Take_(int slot);

    // The smallest round trip in the rttMin window, or -1 with nothing measured yet.
    static int RttMinFor_(const Measured& m, uint64_t nowMs);
    // The base delay: the minimum over the minute history and the minute being filled, or -1 with
    // no admitted probe measured yet.
    static int BaseMs_(const Measured& m);
    // Close the base minute that has run its 60 s: push its minimum if it has one, start the next.
    static void RollBaseMinute_(Measured& m, uint64_t nowMs);
    static const char* LawName_(LawKind kind);
    // "<n> ms", or "-" while there is no base: the text both log lines print after `base=`.
    static void FormatBase_(const Measured& m, char* buf, size_t cap);
    // Retire outstanding probes older than kProbeLostMs, counting them lost.
    static void ExpireOutstanding_(Measured& m, uint64_t nowMs);
    // The anchor's bound: the largest raw `served` reading in the window. Separate from the EWMA
    // because they answer different questions AND reach zero at different times -- the guard in
    // `Steer_` tests this one, since this is what the anchor divides by.
    static int64_t ServedPeak_(const Measured& m);
    // Start a fresh 1 Hz reporting window at `nowMs`.
    static void OpenWindow_(Measured& m, uint64_t nowMs, int64_t delivered, uint64_t recv);

    // The law, run once per 10 Hz sample against the two smoothed rates -- on the SAMPLE cadence,
    // not the reported second, because a join has to climb inside its own download. Returns what it
    // decided; the caller prints the change.
    static LawKind Steer_(Measured& m);

    Slot slots_[kSlots];
    // Whether the law's decisions are handed to the transport at all.
    bool controlEnabled_ = false;
    long pinnedKbs_ = 0;

    // The sample-cost accounting, session-wide (the reads are one pass over every live slot).
    uint64_t costUs_ = 0;
    int      costPasses_ = 0;   // passes over the slot array
    int      costLinks_ = 0;    // connections actually read, summed over those passes
    uint64_t costWindowMs_ = 0;
    bool     anyBusy_ = false;
    // The config WRITE's own cost, which the read's measurement never covered.
    uint64_t rateWriteUs_ = 0;
    int      rateWriteCount_ = 0;
};

}  // namespace coop::net
