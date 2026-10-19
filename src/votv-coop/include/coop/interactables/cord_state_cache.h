#pragma once
#include "coop/interactables/cord_wire.h"
#include <algorithm>
#include <vector>

namespace coop::cord_wire {
// The host refreshes live links every two seconds. Expiry removes destroyed/unloaded
// devices without making a temporary actor-resolution miss discard a live connection.
class StateCache {
public:
    static constexpr uint64_t kLifetimeMs = 60000;
    static constexpr size_t kCapacity = 256;
    struct Row { net::CordStatePayload p; uint64_t at; };
    bool Observe(const net::CordStatePayload& p, uint64_t now) {
        if (!Valid(p)) return false;
        Prune(now);
        auto row = std::find_if(rows_.begin(), rows_.end(),
            [&](const Row& r) { return Same(r.p.cord, p.cord); });
        if (row == rows_.end()) {
            if (rows_.size() >= kCapacity) return false;
            rows_.push_back({p, now});
        } else *row = {p, now};
        // A new canonical occupant invalidates an older row for that socket, even
        // when the old cord's explicit unplug has not arrived/resolved yet.
        if (p.socket.kind) for (auto& r : rows_)
            if (!Same(r.p.cord, p.cord) && Same(r.p.socket, p.socket)) {
                r.p.socket = {}; r.p.powered = 0;
            }
        return true;
    }
    void Prune(uint64_t now) {
        rows_.erase(std::remove_if(rows_.begin(), rows_.end(),
            [&](const Row& r) { return now >= r.at && now-r.at >= kLifetimeMs; }), rows_.end());
    }
    const std::vector<Row>& Rows() const { return rows_; }
    void Clear() { rows_.clear(); }
private:
    std::vector<Row> rows_;
};
}
