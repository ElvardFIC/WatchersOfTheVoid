// coop/props/rider_hold.cpp -- see coop/props/rider_hold.h.
#include "coop/props/rider_hold.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/engine/engine.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace coop::rider_hold {
namespace {
namespace E = ue_wrap::engine;
struct Held {
    ue_wrap::CachedObjRef movement;
    uint32_t world = 0;
    std::vector<Token> tokens;
    bool acquiring = true;
    bool stopAttempted = false;
};
std::unordered_map<void*, std::shared_ptr<Held>> g_held;
uint64_t g_epoch = 1;
Token g_nextToken = 0;
bool g_draining = false;

Token NextToken() {
    if (++g_nextToken == 0) ++g_nextToken;
    return g_nextToken;
}
bool SameRow(void* movement, const std::shared_ptr<Held>& row) {
    const auto it = g_held.find(movement);
    return it != g_held.end() && it->second == row;
}
bool Current(void* movement, const std::shared_ptr<Held>& row, uint64_t epoch) {
    return epoch == g_epoch && row->world == ue_wrap::world_identity::Generation() &&
           row->movement.Get() == movement && SameRow(movement, row);
}
void Forget(void* movement, const std::shared_ptr<Held>& row) {
    if (SameRow(movement, row)) g_held.erase(movement);
}
} // namespace

Token Take(void* movement) {
    UE_ASSERT_GAME_THREAD("rider_hold::Take");
    if (g_draining || !movement) return 0;
    if (auto it = g_held.find(movement); it != g_held.end()) {
        const auto row = it->second;
        if (row->movement.Get() == movement && row->world == ue_wrap::world_identity::Generation()) {
            if (row->acquiring) return 0; // native query/set can reenter before the stop is established
            const auto token = NextToken();
            row->tokens.push_back(token);
            return token;
        }
        g_held.erase(it);
    }
    if (!ue_wrap::reflection::IsLive(movement)) return 0;
    auto row = std::make_shared<Held>();
    row->movement.Set(movement);
    row->world = ue_wrap::world_identity::Generation();
    const auto epoch = g_epoch;
    const auto token = NextToken();
    row->tokens.push_back(token);
    g_held.emplace(movement, row); // reserve before either engine call
    const bool enabled = E::IsComponentTickEnabled(movement);
    if (!Current(movement, row, epoch) || !enabled) {
        Forget(movement, row);
        return 0;
    }
    row->stopAttempted = true;
    const bool stopped = E::SetComponentTickEnabled(movement, false);
    if (!Current(movement, row, epoch)) {
        Forget(movement, row);
        return 0;
    }
    if (!stopped) {
        // Restore the pre-call state, with the failed reservation detached first.
        // Reentry from this write can create a new hold without our erasing it.
        Forget(movement, row);
        E::SetComponentTickEnabled(movement, true);
        return 0;
    }
    row->acquiring = false;
    return token;
}

void Give(void* movement, Token token) {
    UE_ASSERT_GAME_THREAD("rider_hold::Give");
    if (!movement || !token) return;
    const auto it = g_held.find(movement);
    if (it == g_held.end()) return;
    const auto row = it->second;
    if (row->movement.Get() != movement || row->world != ue_wrap::world_identity::Generation()) {
        g_held.erase(it);
        return;
    }
    const auto lease = std::find(row->tokens.begin(), row->tokens.end(), token);
    if (lease == row->tokens.end()) return;
    row->tokens.erase(lease);
    if (!row->tokens.empty()) return;
    g_held.erase(it); // retire before native reentry; never erase a newly acquired row afterwards
    if (row->stopAttempted) E::SetComponentTickEnabled(movement, true);
}

void OnDisconnect() {
    UE_ASSERT_GAME_THREAD("rider_hold::OnDisconnect");
    if (g_draining) return;
    g_draining = true;
    ++g_epoch;
    decltype(g_held) retired;
    retired.swap(g_held);
    for (const auto& [movement, row] : retired) {
        if (row->stopAttempted && row->world == ue_wrap::world_identity::Generation() &&
            row->movement.Get() == movement)
            E::SetComponentTickEnabled(movement, true);
    }
    g_draining = false;
}
} // namespace coop::rider_hold
