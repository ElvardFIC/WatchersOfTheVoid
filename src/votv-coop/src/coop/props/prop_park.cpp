// coop/props/prop_park.cpp -- see coop/props/prop_park.h.
#include "coop/props/prop_park.h"
#include "coop/net/peer_limits.h"
#include "coop/props/rider_hold.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_attach.h"
#include "ue_wrap/engine/engine_physics.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace coop::prop_park {
namespace {
namespace E = ue_wrap::engine;
namespace R = ue_wrap::reflection;
constexpr int kMaxRiders = coop::net::kPeerCapacity;
struct Rider {
    ue_wrap::CachedObjRef updated, movement;
    rider_hold::Token hold = 0;
};
struct Record {
    ue_wrap::CachedObjRef actor, mesh;
    uint32_t world = 0;
    std::vector<Rider> riders;
    bool busy = false;
};
using ParkRecord = std::shared_ptr<Record>;
std::unordered_map<void*, ParkRecord> g_parked;
bool g_draining = false;

void GiveBack(Record& rec) {
    for (auto& rider : rec.riders) {
        const auto token = rider.hold;
        rider.hold = 0; // callbacks must not return this lease twice
        if (token) rider_hold::Give(rider.movement.Raw(), token);
    }
}
bool Current(void* actor, const ParkRecord& row) {
    const auto it = g_parked.find(actor);
    return !g_draining && it != g_parked.end() && it->second == row &&
           row->world == ue_wrap::world_identity::Generation() && row->actor.Get() == actor;
}
bool RootSimulate(void* actor, void* mesh, bool on) {
    return mesh ? E::SetComponentSimulatePhysics(mesh, on) : E::SetActorSimulatePhysics(actor, on);
}
struct Busy {
    ParkRecord row;
    explicit Busy(ParkRecord r) : row(std::move(r)) { row->busy = true; }
    ~Busy() { row->busy = false; }
};
} // namespace

void Park(void* actor, void* mesh) {
    Park(actor, mesh, {});
}
bool Park(void* actor, void* mesh, const std::function<bool()>& source) {
    UE_ASSERT_GAME_THREAD("prop_park::Park");
    const auto allowed = [&] {return !source || source();};
    if (!allowed() || g_draining || !actor || !R::IsLive(actor)) return false;
    ue_wrap::CachedObjRef actorRef, meshRef;
    actorRef.Set(actor);
    if (mesh) {
        if (!R::IsLive(mesh)) return false;
        meshRef.Set(mesh);
    }
    const auto world = ue_wrap::world_identity::Generation();
    auto it = g_parked.find(actor);
    if (it != g_parked.end() && (it->second->actor.Get() != actor || it->second->world != world)) {
        auto stale = it->second;
        g_parked.erase(it);
        GiveBack(*stale); // native callbacks can insert a successor; do not overwrite it
        if (!allowed() || world != ue_wrap::world_identity::Generation() || actorRef.Get() != actor ||
            (mesh && meshRef.Get() != mesh) || g_parked.contains(actor)) return false;
        it = g_parked.end();
    }
    const bool first = it == g_parked.end();
    auto row = first ? std::make_shared<Record>() : it->second;
    if (row->busy) return false;
    // A cached pointer whose old component slot was recycled is not permission
    // to change the new occupant. A genuinely different component can be supplied.
    if (!first && mesh && row->mesh.Raw() == mesh && row->mesh.Get() != mesh) return false;
    row->actor = actorRef;
    row->mesh = meshRef;
    row->world = world;
    if (first) g_parked.emplace(actor, row);
    Busy busy(row);
    const auto current = [&] {
        return allowed() && Current(actor, row) && (!mesh || meshRef.Get() == mesh);
    };
    if (first) {
        E::AttachedCharacter found[kMaxRiders]{};
        const int count = E::AttachedCharactersOf(actor, found, kMaxRiders);
        if (!current()) return false;
        const int n = std::clamp(count, 0, kMaxRiders);
        for (int i = 0; i < n; ++i) {
            if (!found[i].updated || !found[i].movement || !R::IsLive(found[i].updated) ||
                !R::IsLive(found[i].movement)) continue;
            Rider rider;
            rider.updated.Set(found[i].updated);
            rider.movement.Set(found[i].movement);
            row->riders.push_back(rider);
        }
    }
    bool still[kMaxRiders]{};
    for (size_t i = 0; i < row->riders.size(); ++i) {
        const auto& rider = row->riders[i];
        void* updated = rider.updated.Get();
        if (!rider.hold && updated) still[i] = E::IsComponentSimulatingPhysics(updated);
        if (!current()) return false;
    }
    const bool parked = RootSimulate(actor, mesh, false);
    if (!current() || !parked) return false;
    for (size_t i = 0; i < row->riders.size(); ++i) {
        auto& rider = row->riders[i];
        void* updated = rider.updated.Get();
        if (!still[i] || !updated) continue;
        const bool simulates = E::IsComponentSimulatingPhysics(updated);
        if (!current()) return false;
        if (simulates || rider.updated.Get() != updated) continue;
        void* movement = rider.movement.Get();
        const auto token = rider_hold::Take(movement);
        if (!current() || rider.updated.Get() != updated || rider.movement.Get() != movement) {
            if (token) rider_hold::Give(movement, token);
            return false;
        }
        rider.hold = token;
    }
    return current();
}

void Unpark(void* actor, void* mesh, bool rootSimulates) {
    Unpark(actor, mesh, rootSimulates, {});
}
bool Unpark(void* actor, void* mesh, bool rootSimulates, const std::function<bool()>& source) {
    UE_ASSERT_GAME_THREAD("prop_park::Unpark");
    const auto allowed = [&] {return !source || source();};
    if (!allowed() || g_draining || !actor) return false;
    if (auto it = g_parked.find(actor); it != g_parked.end()) {
        auto row = it->second;
        g_parked.erase(it); // detach before physics and tick setters can reenter
        const bool live = row->actor.Get() == actor &&
                          row->world == ue_wrap::world_identity::Generation();
        const bool meshLive = !mesh || row->mesh.Get() == mesh;
        bool restored = true;
        if (live && meshLive && rootSimulates) restored = allowed() && RootSimulate(actor, mesh, true);
        // Returning an owned movement lease is cleanup, including after source cancellation.
        GiveBack(*row);
        return allowed() && live && meshLive && restored;
    }
    // An explicit release can arrive before a first pose/park.
    if (rootSimulates && R::IsLive(actor) && (!mesh || R::IsLive(mesh)))
        return allowed() && RootSimulate(actor, mesh, true) && allowed();
    return allowed();
}

void OnDisconnect() {
    UE_ASSERT_GAME_THREAD("prop_park::OnDisconnect");
    if (g_draining) return;
    g_draining = true;
    decltype(g_parked) retired;
    retired.swap(g_parked);
    for (auto& [actor, row] : retired) GiveBack(*row);
    g_draining = false;
}
} // namespace coop::prop_park
