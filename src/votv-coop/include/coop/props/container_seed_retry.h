#pragma once

#include <cstdint>
#include <cstddef>
#include <map>
#include <utility>

namespace coop::props {

// A seed is owed to one occupancy of one seat. An unrelated recipient accepting
// a broadcast cannot retire it. Callbacks may queue newer work or clear the ledger.
class ContainerSeedRetries {
    using Key = std::pair<uint32_t, int>;
    struct Target { uint32_t generation; uint64_t revision; };
    std::map<Key, Target> rows_;
    Key cursor_{};
    uint64_t revision_ = 0, epoch_ = 0;
    bool draining_ = false;
public:
    void Queue(uint32_t eid, int slot, uint32_t generation) {
        rows_[{eid, slot}] = {generation, ++revision_};
    }
    void Clear() { rows_.clear(); cursor_ = {}; ++epoch_; }
    std::size_t Size() const { return rows_.size(); }

    template<class Publish>
    void Drain(Publish publish) {
        if (draining_) return;
        draining_ = true;
        struct Guard { bool& value; ~Guard() { value = false; } } guard{draining_};
        const auto epoch = epoch_;
        const auto count = rows_.size() < 16 ? rows_.size() : std::size_t{16};
        for (std::size_t i = 0; i < count && !rows_.empty(); ++i) {
            auto it = rows_.upper_bound(cursor_);
            if (it == rows_.end()) it = rows_.begin();
            const auto key = it->first;
            const auto target = it->second;
            cursor_ = key;
            const bool done = publish(key.first, key.second, target.generation);
            if (epoch_ != epoch) return;
            it = rows_.find(key);
            if (done && it != rows_.end() && it->second.revision == target.revision)
                rows_.erase(it);
        }
    }
};

}  // namespace coop::props
