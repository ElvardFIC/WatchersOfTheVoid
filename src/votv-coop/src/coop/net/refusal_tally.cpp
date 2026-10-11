// coop/net/refusal_tally.cpp -- see coop/net/refusal_tally.h.

#include "coop/net/refusal_tally.h"

#include "ue_wrap/core/log.h"

namespace coop::net {

namespace {

const char* LeftMark(bool left) { return left ? " (the sender has left)" : ""; }

}  // namespace

void RefusalTally::Note(std::uint8_t slot, Charge charge, const char* why, Clock::time_point now) {
    if (charge == Charge::Sender && slot < bySlot_.size()) ++bySlot_[slot];
    const bool left = charge == Charge::Leaver;
    if (open_ && now - openedAt_ < kWindow) {
        ++more_;
        lastSlot_ = slot;
        lastLeft_ = left;
        lastWhy_ = why;
        return;
    }
    Flush(now);
    UE_LOGW("%s: refused a row from slot %u%s -- %s", lane_, static_cast<unsigned>(slot), LeftMark(left),
            why);
    open_ = true;
    openedAt_ = now;
    more_ = 0;
}

void RefusalTally::Flush(Clock::time_point now) {
    if (!open_ || now - openedAt_ < kWindow) return;
    if (more_ > 0)
        UE_LOGW("%s: refused %u more row(s) in the %lld s after it -- the last from slot %u%s: %s", lane_,
                more_, static_cast<long long>(kWindow.count()), static_cast<unsigned>(lastSlot_),
                LeftMark(lastLeft_), lastWhy_);
    open_ = false;
    more_ = 0;
}

void RefusalTally::Clear() {
    // The session ends: an open window's sum is said now, since no later tick will close it.
    if (open_ && more_ > 0)
        UE_LOGW("%s: refused %u more row(s) before the session ended -- the last from slot %u%s: %s", lane_,
                more_, static_cast<unsigned>(lastSlot_), LeftMark(lastLeft_), lastWhy_);
    bySlot_.fill(0);
    open_ = false;
    more_ = 0;
}

}  // namespace coop::net
