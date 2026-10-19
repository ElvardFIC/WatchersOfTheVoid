#pragma once

#include "coop/props/prop_drop_intent.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <unordered_map>
#include <unordered_set>

namespace coop::join_membership_sweep {
// A callback may begin a new bracket or reset the session. Old work must stop
// without clearing the new bracket, and recursive ticks cannot start another pass.
class ReconcilePass {
    const uint64_t& epoch_;
    bool& running_;
    uint64_t started_;
    bool owns_;
public:
    ReconcilePass(const uint64_t& epoch, bool& running)
        : epoch_(epoch), running_(running), started_(epoch), owns_(!running) {
        if (owns_) running_ = true;
    }
    ~ReconcilePass() { if (owns_) running_ = false; }
    ReconcilePass(const ReconcilePass&) = delete;
    ReconcilePass& operator=(const ReconcilePass&) = delete;
    bool Current() const { return owns_ && epoch_ == started_; }
};
using LocalBirths = std::unordered_map<void*, ue_wrap::CachedObjRef>;

// A local birth is not save-load divergence while it is being admitted by the host.
// Never grant a recycled address the previous actor's authorship exemption.
inline bool ProtectedLocalBirth(void* actor, const LocalBirths& authored) {
    if (!actor) return false;
    const auto it = authored.find(actor);
    if (it != authored.end() && it->second.Get() == actor) return true;
    return coop::prop_drop_intent::IsAwaitingHost(actor) ||
           coop::prop_drop_intent::IsPendingHostPlacement(actor);
}

// Capture identity when selecting a candidate, not after intervening native calls.
struct SweepCandidate {
    ue_wrap::CachedObjRef ref;
    explicit SweepCandidate(void* actor) { ref.Set(actor); }
    void* Resolve(const std::unordered_set<void*>& claimed, const LocalBirths& authored) const {
        void* actor = ref.Get();
        return actor && !claimed.count(actor) && !ProtectedLocalBirth(actor, authored) ? actor : nullptr;
    }
};
}  // namespace coop::join_membership_sweep
