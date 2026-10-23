// Native extraction: wait for the saved key, then retain birth/state delivery.
#include "coop/props/prop_lifecycle.h"
#include "coop/items/container_extract_recovery.h"
#include "prop_lifecycle_detail.h"
#include "coop/element/element.h"
#include "coop/net/net_clock.h"
#include "coop/net/session_serial.h"
#include "coop/props/join_membership_sweep.h"
#include "coop/props/prop_echo_suppress.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_save_data.h"
#include "coop/props/prop_synth_key.h"
#include "coop/props/prop_wire_parity.h"
#include "ue_wrap/actors/food.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/facts/function_row.h"
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <vector>

namespace coop::prop_lifecycle {
namespace {
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
namespace GT = ue_wrap::game_thread;
namespace E = ue_wrap::engine;
namespace PT = coop::prop_element_tracker;
UE_FACT_FN(kRowTakeObj, "prop_container_extract", L"propInventory_C", L"takeObj", Declared);
constexpr int kTakeTag = 0x54414b45;
constexpr size_t kPendingCap = 128;
constexpr uint64_t kRetryMs = 250;
bool g_watchRegistered = false, g_draining = false;
ue_wrap::CachedObjRef g_takeFunction;
struct ResultLayout {
    ue_wrap::CachedObjRef function;
    int32_t actor = -1, success = -1;
} g_result;
struct Pending {
    ue_wrap::CachedObjRef actor;
    uint64_t revision = 0, next = 0;
    bool spawnQueued = false;
    std::wstring key;
};
std::map<int32_t, Pending> g_pending;
int32_t g_cursor = -1;
uint64_t g_epoch = 0, g_revision = 0;
uint32_t g_world = 0, g_serial = 0;
net::Session* g_context = nullptr;
net::Role g_role = net::Role::Host;

bool Active(net::Session* s) {
    return GT::IsGameThread() && s && s == LoadSession() && s->connected() &&
        s == g_context && s->role() == g_role && g_world == ue_wrap::world_identity::Generation() &&
        g_serial == net::session_serial::Current();
}
void Context(net::Session* s) {
    const auto world = ue_wrap::world_identity::Generation(), serial = net::session_serial::Current();
    if (s != g_context || world != g_world || serial != g_serial || (s && s->role() != g_role)) {
        ClearContainerExtractions();
        g_context = s; g_world = world; g_serial = serial;
        if (s) g_role = s->role();
    }
}
SG::Verdict BeforeTake(const SG::Call& call) {
    if (GT::IsGameThread() && call.function) g_takeFunction.Set(call.function);
    return SG::Verdict::Run;
}
// Cache offsets by UFunction incarnation and validate both fields before reading.
// Out parameters may live in the caller rather than the gate's local frame.
void* CompletedActor(const SG::Call& call) {
    if (!call.function || !call.locals) return nullptr;
    if (!g_result.function.Is(call.function)) {
        g_result = {};
        g_result.function.Set(call.function);
        const auto frame = R::FunctionFrameSize(call.function);
        const auto fits = [frame](int32_t off, int32_t bytes) {
            return off >= 0 && off <= frame && bytes <= frame - off;
        };
        const auto actor = R::FindParamOffset(call.function, L"object");
        const auto success = R::FindParamOffset(call.function, L"return");
        if (fits(actor, sizeof(void*))) g_result.actor = actor;
        if (fits(success, 1)) g_result.success = success;
    }
    if (!g_result.function.Is(call.function) || g_result.actor < 0 || g_result.success < 0) return nullptr;
    const auto* success = SG::OutParamPtr(call, g_result.success);
    if (!success) success = call.locals + g_result.success;
    if (!*success) return nullptr;
    const auto* output = SG::OutParamPtr(call, g_result.actor);
    if (!output) output = call.locals + g_result.actor;
    void* actor = nullptr; std::memcpy(&actor, output, sizeof(actor));
    return actor;
}
void AfterTake(const SG::Call& call) {
    auto* s = LoadSession();
    if (!Active(s) || !call.object) return;
    void* actor = CompletedActor(call);
    if (!actor || !R::IsLive(actor) || coop::prop_echo_suppress::IsMirrorSpawn(actor)) return;
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    auto it = g_pending.find(ref.Idx());
    if (it != g_pending.end() && it->second.actor.Is(actor)) return;
    if (g_pending.size() >= kPendingCap) {
        UE_LOGW("container birth: pending cap %zu reached; actor %p not queued", kPendingCap, actor);
        return;
    }
    Pending row; row.actor = ref; row.revision = ++g_revision;
    g_pending.insert_or_assign(ref.Idx(), std::move(row));
}
bool BuildSpawn(void* actor, net::PropSpawnPayload& p, const std::function<bool()>& current) {
    if (!current() || !ue_wrap::prop::IsKeyedInteractable(actor) || !current()) return false;
    const auto cls = R::ClassNameOf(actor);
    if (cls.empty() || cls.size() > sizeof(p.className.data)) return false;
    auto key = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!current()) return false;
    key = coop::prop_synth_key::EnsureKeyForBroadcast(actor, key);
    if (!current()) return false;
    ue_wrap::FVector loc{}; ue_wrap::FRotator rot{};
    if (!E::TryGetActorLocation(actor, loc) || !current() ||
        !E::TryGetActorRotation(actor, rot) || !current()) return false;
    const auto scale = E::GetActorScale3D(actor);
    if (!current()) return false;
    for (float value : {loc.X, loc.Y, loc.Z, rot.Pitch, rot.Yaw, rot.Roll, scale.X, scale.Y, scale.Z})
        if (!std::isfinite(value)) return false;
    p.className.len = static_cast<uint8_t>(cls.size());
    for (size_t i = 0; i < cls.size(); ++i) p.className.data[i] = static_cast<char>(cls[i]);
    p.locX = loc.X; p.locY = loc.Y; p.locZ = loc.Z;
    p.rotPitch = ue_wrap::NormalizeAxis(rot.Pitch); p.rotYaw = ue_wrap::NormalizeAxis(rot.Yaw); p.rotRoll = ue_wrap::NormalizeAxis(rot.Roll);
    p.scaleX = scale.X; p.scaleY = scale.Y; p.scaleZ = scale.Z;
    p.physFlags = coop::prop_wire_parity::PhysFlagsOf(actor) | net::propspawn_flags::kSimulatePhysics;
    if (!current()) return false;
    if (ue_wrap::prop::IsDescendantOfProp(actor)) {
        const auto name = ue_wrap::prop::GetPropNameString(actor);
        if (!current() || name.size() > sizeof(p.propName.data)) return false;
        p.propName.len = static_cast<uint8_t>(name.size());
        for (size_t i = 0; i < name.size(); ++i) p.propName.data[i] = static_cast<char>(name[i]);
    }
    key = PT::MarkPropElement(actor, key, cls, PT::EnrollSource::kExpressSeam);
    if (!current() || key.empty() || key == L"None" || key.size() > sizeof(p.key.data)) return false;
    p.key.len = static_cast<uint8_t>(key.size());
    for (size_t i = 0; i < key.size(); ++i) p.key.data[i] = static_cast<char>(key[i]);
    p.elementId = PT::GetPropElementIdForActor(actor);
    if (!current() || !p.elementId || p.elementId == coop::element::kInvalidId) return false;
    return ue_wrap::prop::GetInteractableKeyString(actor) == key && current();
}
void Drain() {
    if (g_draining) return;
    auto* s = LoadSession();
    if (!Active(s) || g_pending.empty()) return;
    g_draining = true;
    struct Scope { ~Scope(){g_draining=false;} } scope;
    const auto epoch = g_epoch, now = net::NowMs();
    // A rotating cursor keeps later identities moving even at low frame rates.
    // No iterator or reference to the map crosses a native call.
    std::vector<std::pair<int32_t,Pending>> work;
    for (size_t visited=0; visited<g_pending.size() && work.size()<8; ++visited) {
        auto it=g_pending.upper_bound(g_cursor);
        if(it==g_pending.end())it=g_pending.begin();
        g_cursor=it->first;
        if(it->second.next<=now)work.push_back(*it);
    }
    for (const auto& [index, snapshot] : work) {
        const auto current = [&] {
            const auto it = g_pending.find(index);
            return epoch == g_epoch && Active(s) && snapshot.actor.Get() &&
                it != g_pending.end() && it->second.revision == snapshot.revision;
        };
        if (epoch != g_epoch || !Active(s)) return;
        auto it = g_pending.find(index);
        if (it == g_pending.end() || it->second.revision != snapshot.revision) continue;
        void* actor = snapshot.actor.Get();
        if (!actor || coop::prop_echo_suppress::IsMirrorSpawn(actor)) {g_pending.erase(it); continue;}
        it->second.next = now + kRetryMs;
        if (!snapshot.spawnQueued) {
            net::PropSpawnPayload p{};
            if (!BuildSpawn(actor, p, current) || !current()) continue;
            coop::join_membership_sweep::RecordClaimIfTracking(actor);
            if (!current() || !s->SendPropSpawn(p) || !current()) continue;
            it = g_pending.find(index);
            it->second.spawnQueued = true;
            it->second.key.assign(p.key.data, p.key.data + p.key.len);
        }
        if (!current()) continue;
        it = g_pending.find(index);
        const auto key = it->second.key;
        if (ue_wrap::prop::GetInteractableKeyString(actor) != key || !current()) {
            if (current()) g_pending.erase(index);
            continue;
        }
        const bool carries = coop::prop_save_data::Covers(actor) || ue_wrap::food::IsFood(actor);
        if (!current()) continue;
        if (carries) {
            net::WireKey wire{}; wire.len = static_cast<uint8_t>(key.size());
            for (size_t i=0;i<key.size();++i) wire.data[i] = static_cast<char>(key[i]);
            if (!coop::prop_save_data::PublishWithSpawn(s, actor, wire) || !current()) continue;
        }
        if (current()) g_pending.erase(index);
    }
}
} // namespace

bool ContainerExtractionInFlight() {
    // The gate's per-thread scope unwinds on cancellation/faults. An inner
    // extraction cannot clear the scope of a still-running outer extraction.
    return coop::container_extract_recovery::InFlight() ||
        (GT::IsGameThread() && g_takeFunction.Get() && SG::IsBodyActive(g_takeFunction.Get()));
}
bool ContainerBirthPending(void* actor) {
    if (!GT::IsGameThread() || !actor) return false;
    if(coop::container_extract_recovery::Pending(actor))return true;
    if (ContainerExtractionInFlight()) return true;
    const auto it = g_pending.find(R::InternalIndexOf(actor));
    return it != g_pending.end() && it->second.actor.Is(actor);
}
void ClearContainerExtractions() {
    ++g_epoch; g_pending.clear(); g_cursor=-1; g_takeFunction.Reset(); g_result = {};
}
void InstallInventory(net::Session* session) {
    g_session_ptr.store(session, std::memory_order_release);
    PT::SetSession(session);
    if (!GT::IsGameThread()) return;
    Context(session);
    if (!g_watchRegistered) g_watchRegistered = SG::WatchName(kRowTakeObj, kTakeTag, &BeforeTake, &AfterTake);
    Drain();
}
} // namespace coop::prop_lifecycle
