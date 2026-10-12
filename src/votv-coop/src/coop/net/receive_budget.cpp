// coop/net/receive_budget.cpp -- see coop/net/receive_budget.h.

#include "coop/net/receive_budget.h"

namespace coop::net {

void ReceiveBudget::Configure(long burst, long perSecond) {
    budget_.burst = static_cast<float>(burst);
    budget_.perSecond = static_cast<float>(perSecond);
    for (Slot& s : slots_) s = Slot{};
}

ReceiveBudget::Slot& ReceiveBudget::For(int slot, uint32_t hConn) {
    Slot& s = slots_[slot];
    if (s.hConn != hConn) {
        s.hConn = hConn;
        s.spent = false;
        s.reliable.Reset();
    }
    return s;
}

ReceiveBudget::Take ReceiveBudget::TakeReliable(int slot, uint32_t hConn, uint64_t nowMs) {
    if (!On() || slot < 0 || slot >= static_cast<int>(coop::players::kMaxPeers)) return Take::Ok;
    Slot& s = For(slot, hConn);
    if (s.spent) return Take::Spent;
    if (s.reliable.Take(budget_, nowMs)) return Take::Ok;
    s.spent = true;
    return Take::Emptied;
}

}  // namespace coop::net
