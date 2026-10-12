// coop/net/receive_budget.cpp -- see coop/net/receive_budget.h.

#include "coop/net/receive_budget.h"

namespace coop::net {

void ReceiveBudget::Configure(long burst, long perSecond) {
    budget_.burst = static_cast<float>(burst);
    budget_.perSecond = static_cast<float>(perSecond);
    for (Slot& s : slots_) s = Slot{};
}

void ReceiveBudget::ConfigureStreams(int perSecond) {
    streamBudget_.burst = static_cast<float>(perSecond);
    streamBudget_.perSecond = static_cast<float>(perSecond);
    for (Slot& s : slots_) s = Slot{};
}

int ReceiveBudget::StreamIndex(MsgType type) {
    switch (type) {
    case MsgType::PoseSnapshot:   return 0;
    case MsgType::PropPose:       return 1;
    case MsgType::RagdollPose:    return 2;
    case MsgType::HandPose:       return 3;
    case MsgType::DeskCursorPose: return 4;
    case MsgType::ClockPose:      return 5;
    case MsgType::DeskSimPose:    return 6;
    case MsgType::DishPose:       return 7;
    case MsgType::ReelPose:       return 8;
    case MsgType::VoiceFrame:     return 9;
    default:                      return -1;
    }
}

ReceiveBudget::Slot& ReceiveBudget::For(int slot, uint32_t hConn) {
    Slot& s = slots_[slot];
    if (s.hConn != hConn) {
        s.hConn = hConn;
        s.spent = false;
        s.reliable.Reset();
        for (IntentBucket& b : s.streams) b.Reset();
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

bool ReceiveBudget::TakeStream(int slot, uint32_t hConn, MsgType type, uint64_t nowMs) {
    if (streamBudget_.perSecond <= 0.0f || slot < 0 || slot >= static_cast<int>(coop::players::kMaxPeers)) return true;
    const int i = StreamIndex(type);
    if (i < 0) return true;
    return For(slot, hConn).streams[i].Take(streamBudget_, nowMs);
}

}  // namespace coop::net
