// Ragdoll limbs are a partitioned stream: newest-wins on the whole packet loses
// limbs whenever transport/game ticks coalesce several different bone batches.
#pragma once
#include "coop/net/ragdoll_pose_validation.h"
#include "coop/net/stream_slot.h"
#include <algorithm>
#include <array>

namespace coop::net {

inline bool SameRagdollRig(const RagdollPoseSnapshot& a, const RagdollPoseSnapshot& b) {
    return a.rigTag == b.rigTag && a.rigBones == b.rigBones && a.rigSignature == b.rigSignature;
}

// Owned by the session's local lock. Advance only when the transport takes a
// send round, never by wall time or game-frame count. Each limb gets a turn.
class RagdollPoseOutbox {
public:
    void Publish(const RagdollPoseSnapshot& pose, const RagdollBonePose* bones, size_t count) {
        if (!SameRagdollRig(pose_, pose) || count_ != count) cursor_ = cycle_ = 0;
        pose_ = pose;
        count_ = bones && count <= bones_.size() && count == pose.rigBones ? count : 0;
        if (count_) std::copy_n(bones, count_, bones_.begin());
    }
    RagdollPoseSnapshot Next() {
        auto out = pose_;
        if (count_) {
            const size_t n = (std::min)(size_t{7}, count_);
            out.boneCount = static_cast<uint8_t>(n);
            for (size_t i = 0; i < n; ++i) out.bones[i] = bones_[(cursor_ + i) % count_];
            cursor_ = (cursor_ + n) % count_;
            // Shift an exactly repeated cycle too: periodic packet loss must
            // not always remove the same group on rigs divisible by seven.
            if (++cycle_ == (count_ + 6) / 7) {
                cycle_ = 0;
                if (count_ > n) cursor_ = (cursor_ + 1) % count_;
            }
        }
        return out;
    }
private:
    RagdollPoseSnapshot pose_{};
    std::array<RagdollBonePose, 64> bones_{};
    size_t count_ = 0, cursor_ = 0, cycle_ = 0;
};

// Owned by the remote lock. Keep the newest received value PER bone until the
// game thread drains it. Bounded to one rig/64 bodies, no unbounded packet queue.
class RagdollPoseInbox {
public:
    bool Offer(uint32_t seq, const RagdollPoseSnapshot& pose) {
        if (!ValidateRagdollBones(pose) || !sequence_.Advance(seq)) return false;
        if (!SameRagdollRig(latest_, pose)) { count_ = 0; dirty_.fill(false); }
        latest_ = pose; pending_ = true;
        for (uint8_t i = 0; i < pose.boneCount; ++i) {
            size_t at = 0;
            while (at < count_ && bones_[at].index != pose.bones[i].index) ++at;
            if (at == count_) {
                if (count_ >= pose.rigBones || count_ >= bones_.size()) continue;
                ++count_;
            }
            bones_[at] = pose.bones[i]; dirty_[at] = true;
        }
        return true;
    }
    bool Read(RagdollPoseSnapshot& out, bool* isNew) {
        if (!sequence_.Latched()) return false;
        out = latest_; out.boneCount = 0;
        const bool fresh = pending_;
        pending_ = false;
        for (size_t i = 0; i < count_; ++i) {
            if (!dirty_[i]) continue;
            if (out.boneCount == 7) { pending_ = true; continue; }
            out.bones[out.boneCount++] = bones_[i]; dirty_[i] = false;
        }
        if (isNew) *isNew = fresh;
        return true;
    }
private:
    SeqLatch sequence_;
    RagdollPoseSnapshot latest_{};
    std::array<RagdollBonePose, 64> bones_{};
    std::array<bool, 64> dirty_{};
    size_t count_ = 0;
    bool pending_ = false;
};
}
