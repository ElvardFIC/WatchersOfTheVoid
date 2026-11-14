// A deferred child release owns its capture and each continuation through callbacks.
#include "coop/props/prop_child_release.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_park.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/engine/engine.h"
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>

namespace coop::prop_child_release {
namespace {
namespace E = ue_wrap::engine;
namespace R = ue_wrap::reflection;
struct Life {
    ue_wrap::CachedObjRef object, cls;
    void Set(void* p) {
        if (!p || !R::IsLive(p)) return;
        object.Set(p);
        if (object.Get()) cls.Set(R::ClassOf(p));
    }
    bool Current() const { return object.Get() && cls.Get() && R::ClassOf(object.Raw()) == cls.Raw(); }
};
struct Wait {
    Life door, parent;
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    uint64_t sinceMs = 0, epoch = 0;
    uint32_t world = 0;
    bool hasPose = false, capturing = true, ready = false, restorePose = true;
};
using Pending = std::shared_ptr<Wait>;
std::unordered_map<void*, Pending> g_waits;
uint64_t g_epoch = 1;
bool g_pumping = false, g_draining = false;
struct Busy {
    bool& flag;
    explicit Busy(bool& f) : flag(f) { flag = true; }
    ~Busy() { flag = false; }
    Busy(const Busy&) = delete;
};
struct Capture {
    Pending wait;
    ~Capture() { wait->capturing = false; }
};
bool Installed(const Pending& w) {
    auto it = g_waits.find(w->door.object.Raw());
    return it != g_waits.end() && it->second == w;
}
bool Current(const Pending& w) {
    return !g_draining && w->epoch == g_epoch && w->world == ue_wrap::world_identity::Generation() &&
           Installed(w) && w->door.Current();
}
void Erase(const Pending& w) {
    auto it = g_waits.find(w->door.object.Raw());
    if (it != g_waits.end() && it->second == w) g_waits.erase(it);
}
std::vector<Pending> Snapshot() {
    std::vector<Pending> result; result.reserve(g_waits.size());
    for (const auto& [door, wait] : g_waits) result.push_back(wait);
    return result;
}
bool Unheld(const Pending& w) {
    if (!Current(w)) return false;
    const bool held = coop::prop_drive_host::InAHand(w->door.object.Raw());
    if (!Current(w)) return false;
    if (held) { Erase(w); return false; }
    return true;
}
bool PoseCurrent(const Pending& w) {
    if (!Unheld(w)) return false;
    if (w->restorePose && !w->parent.Current()) { w->restorePose = false; return false; }
    return true;
}
void Release(const Pending& w) {
    if (!PoseCurrent(w)) return;
    void* door = w->door.object.Raw();
    if (w->hasPose && w->restorePose) {
        const bool located = E::SetActorLocation(door, w->loc);
        if (!PoseCurrent(w) || !located) return;
        const bool rotated = E::SetActorRotation(door, w->rot);
        if (!PoseCurrent(w) || !rotated) return;
    }
    void* mesh = ue_wrap::prop::GetStaticMesh(door);
    if (!PoseCurrent(w)) return;
    Life component; component.Set(mesh);
    if (mesh && !component.Current()) return;
    const bool bound = ue_wrap::prop::GetStaticMesh(door) == mesh;
    if (!PoseCurrent(w) || !bound || (mesh && !component.Current())) return;
    Erase(w); // consume before native unpark; an uncertain dispatched release is never retried
    coop::prop_park::Unpark(door, mesh, true);
}
void Pump() {
    if (g_pumping || g_draining) return;
    Busy pumping(g_pumping);
    const auto rows = Snapshot();
    for (const auto& w : rows) {
        if (!Current(w)) { Erase(w); continue; }
        if (w->capturing || !Unheld(w)) continue;
        if (!w->parent.Current()) { w->ready = true; w->restorePose = false; }
        if (w->ready) Release(w);
    }
}
bool Finite(const ue_wrap::FVector& v) {
    return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z);
}
bool Finite(const ue_wrap::FRotator& v) {
    return std::isfinite(v.Pitch) && std::isfinite(v.Yaw) && std::isfinite(v.Roll);
}
} // namespace

void Defer(void* door, void* parent) {
    UE_ASSERT_GAME_THREAD("prop_child_release::Defer");
    if (g_draining || !door || !parent || door == parent) return;
    auto w = std::make_shared<Wait>();
    w->epoch = g_epoch; w->world = ue_wrap::world_identity::Generation();
    w->door.Set(door); w->parent.Set(parent);
    if (!w->door.Current() || !w->parent.Current() || w->epoch != g_epoch ||
        w->world != ue_wrap::world_identity::Generation()) return;
    w->sinceMs = coop::active_drive::NowMs();
    g_waits[door] = w;
    Capture capture{w};
    ue_wrap::FVector loc{}; ue_wrap::FRotator rot{};
    const bool located = E::TryGetActorLocation(door, loc);
    if (!Current(w) || !w->parent.Current()) return;
    if (!located || !Finite(loc)) return;
    const bool rotated = E::TryGetActorRotation(door, rot);
    if (!Current(w) || !w->parent.Current() || !rotated || !Finite(rot)) return;
    w->loc = loc; w->rot = rot; w->hasPose = true;
}
void Forget(void* door) {
    UE_ASSERT_GAME_THREAD("prop_child_release::Forget");
    g_waits.erase(door);
}
void OnParentReleased(void* parent) {
    UE_ASSERT_GAME_THREAD("prop_child_release::OnParentReleased");
    if (g_draining || !parent) return;
    const auto rows = Snapshot();
    for (const auto& w : rows)
        if (Current(w) && w->parent.Current() && w->parent.object.Is(parent)) w->ready = true;
    Pump();
}
void Tick(uint64_t nowMs) {
    UE_ASSERT_GAME_THREAD("prop_child_release::Tick");
    if (g_draining) return;
    const auto rows = Snapshot();
    for (const auto& w : rows) {
        if (Current(w) && nowMs >= w->sinceMs && nowMs - w->sinceMs >= kChildWaitMs) w->ready = true;
    }
    Pump();
}
void OnDisconnect() {
    UE_ASSERT_GAME_THREAD("prop_child_release::OnDisconnect");
    if (g_draining) return;
    Busy draining(g_draining);
    ++g_epoch;
    decltype(g_waits) retired;
    retired.swap(g_waits);
    for (const auto& [door, w] : retired) {
        auto current = [&] { return w->world == ue_wrap::world_identity::Generation() && w->door.Current(); };
        if (!current() || coop::prop_drive_host::InAHand(door) || !current()) continue;
        void* mesh = ue_wrap::prop::GetStaticMesh(door);
        if (!current()) continue;
        Life component; component.Set(mesh);
        if (mesh && !component.Current()) continue;
        if (coop::prop_drive_host::InAHand(door) || !current()) continue;
        const bool bound = ue_wrap::prop::GetStaticMesh(door) == mesh;
        if (current() && bound && (!mesh || component.Current()) &&
            !coop::prop_drive_host::InAHand(door) && current()) coop::prop_park::Unpark(door, mesh, true);
    }
}
} // namespace coop::prop_child_release
