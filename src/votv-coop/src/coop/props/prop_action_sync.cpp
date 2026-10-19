#include "coop/props/prop_action_sync.h"

#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_save_data.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/safe.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/actors/wall_build.h"
#include "ue_wrap/world/keyed_objects.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/hit_result.h"
#include "ue_wrap/world/weapon_catalog.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <vector>

namespace coop::prop_action_sync {
namespace {
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;

constexpr uint8_t kDamage = 1, kSlice = 2, kOpenMre = 3, kUnlockSafe = 4, kWallDamage = 5;
constexpr float kReachUU = 450.f;
constexpr float kBurst = 8.f, kRate = 8.f;
constexpr int kDamageTag = 0x5041444D; // PADM
constexpr int kSliceTag = 0x5041534C;  // PASL
constexpr int kMreTag = 0x50414D52;  // PAMR
constexpr wchar_t kDamageFn[] = L"addDamage";
constexpr wchar_t kSliceFn[] = L"slice";

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_watchesInstalled = false;
struct QueuedIntent {
    coop::net::PropActionIntentPayload payload{};
    uint64_t queuedAt = 0;
    ue_wrap::CachedObjRef target;
};
std::deque<QueuedIntent> g_pending;
struct Bucket { float tokens = kBurst; uint64_t stamp = 0; };
Bucket g_buckets[coop::net::kMaxPeers];
uint32_t g_world = 0;
ue_wrap::CachedObjRef g_lastMreRequest;
uint64_t g_lastMreRequestAt = 0;
struct SafePublish {ue_wrap::CachedObjRef actor;uint64_t retryAt=0,serial=0;};
std::unordered_map<std::wstring,SafePublish> g_safePublish;
uint64_t g_safeSerial=0;

void EnsureWorld() {
    const uint32_t world = ue_wrap::world_identity::Generation();
    if (world == g_world) return;
    g_world = world;
    g_pending.clear();
    g_safePublish.clear();++g_safeSerial;
    g_lastMreRequest.Reset();
    g_lastMreRequestAt = 0;
    for (auto& bucket : g_buckets) bucket = {};
}

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool TakeToken(uint8_t slot) {
    Bucket& b = g_buckets[slot]; const uint64_t now = NowMs();
    if (b.stamp && now > b.stamp) b.tokens = (std::min)(kBurst, b.tokens + kRate * (now - b.stamp) / 1000.f);
    b.stamp = now; if (b.tokens < 1.f) return false; b.tokens -= 1.f; return true;
}

bool Sawable(void* actor) {
    if (!ue_wrap::prop::IsDescendantOfProp(actor)) return false;
    auto* component = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(actor) + ue_wrap::profile::off::Aprop_physicsImpact);
    if (!component || !R::IsLive(component)) return false;
    void* cls = R::ClassOf(component);
    const int32_t off = cls ? R::FindPropertyOffset(cls, L"isNotSawable") : -1;
    if (off < 0) return false;
    bool blocked = true;
    std::memcpy(&blocked, reinterpret_cast<uint8_t*>(component) + off, sizeof(blocked));
    return !blocked;
}

bool IsSwingValid(const std::wstring& held, float damage) {
    ue_wrap::weapon_catalog::Swing swing{};
    return std::isfinite(damage) && damage > 0.f && ue_wrap::weapon_catalog::Lookup(held, swing) &&
           swing.canSwing && damage <= swing.maxDamage + 0.01f;
}

template<class T> bool ReadArg(const SG::Call& c, const wchar_t* name, T& value) {
    if (!c.function || !c.locals) return false;
    const int32_t off = R::FindParamOffset(c.function, name);
    const int32_t size = R::FunctionFrameSize(c.function);
    if (off < 0 || off > size || sizeof(T) > size - off) return false;
    std::memcpy(&value, c.locals + off, sizeof(T));
    return true;
}

bool SendIntent(void* target, uint8_t action, float damage, const ue_wrap::FVector& impact, bool clean) {
    EnsureWorld();
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Client || !target) return false;
    const std::wstring key = action == kWallDamage ? ue_wrap::wall_build::Key(target) : ue_wrap::prop::GetKeyString(target);
    if (key.empty() || key == L"None" || key.size() > sizeof(coop::net::WireKey{}.data)) return false;
    coop::net::PropActionIntentPayload p{};
    coop::net::WireKeyFromString(key, p.key);
    if (coop::net::StringFromWireKey(p.key) != key) return false;
    p.action = action; p.damage = damage;
    p.impactX = impact.X; p.impactY = impact.Y; p.impactZ = impact.Z;
    p.clean = clean ? 1 : 0;
    // A new hit/open must not overtake an older intent waiting for queue space.
    if (g_pending.empty() && s->SendReliableToSlot(0, coop::net::ReliableKind::PropActionIntent, &p, sizeof(p))) return true;
    if (g_pending.size() >= 256) return false;
    QueuedIntent queued{p, NowMs()};
    queued.target.Set(target);
    g_pending.push_back(queued);
    return true;
}

SG::Verdict OnSafeOpened(const SG::Call& c) {
    auto* s=g_session.load();
    if(!s||!s->connected()||s->role()!=coop::net::Role::Client||
       !ue_wrap::safe::IsSafe(c.object)||!coop::net_pump::IsInAnnouncedWorld(c.object))return SG::Verdict::Run;
    // Both dial checkPass and damage's hacked() reach opened AFTER changing the
    // local lock. Keep the native dial/UI feedback; only the host creates loot.
    if(!c.fromOurCode&&!coop::hand_item::IsHandAxisActor(c.object)) {
        EnsureWorld();
        bool pending=false;
        for(const auto& item:g_pending)if(item.payload.action==kUnlockSafe&&item.target.Is(c.object)){pending=true;break;}
        if(!pending)SendIntent(c.object,kUnlockSafe,0.f,{},false);
    }
    return SG::Verdict::Cancel;
}
void AfterSafeOpened(const SG::Call& c) {
    auto* s=g_session.load();
    if(!s||!s->connected()||s->role()!=coop::net::Role::Host||!ue_wrap::safe::IsSafe(c.object))return;
    EnsureWorld();
    // loadData restores the lock/password without calling opened again. The
    // existing record lane handles send retry, birth ordering and late join.
    const auto key=ue_wrap::prop::GetKeyString(c.object);
    if(key.empty()||key==L"None"||key.size()>31||(g_safePublish.size()>=64&&!g_safePublish.count(key)))return;
    auto& row=g_safePublish[key];row.actor.Set(c.object);row.retryAt=0;row.serial=++g_safeSerial;
}

void PlayMreRequestCue(void* target) {
    // Cancelling the client body also suppresses its native ripping sound. Keep
    // requester feedback, using the same cooked cue and small attenuation.
    static ue_wrap::CachedObjRef cue, attenuation;
    if (!cue.Get()) cue.Set(R::FindObject(L"mreopen_Cue", L"SoundCue"));
    if (!attenuation.Get()) attenuation.Set(R::FindObject(L"att_small", L"SoundAttenuation"));
    ue_wrap::FVector loc{};
    if (cue.Get() && attenuation.Get() && ue_wrap::engine::TryGetActorLocation(target, loc))
        ue_wrap::engine::PlaySoundAtLocation(target, cue.Get(), loc, attenuation.Get(), 1.f, 1.f);
}

SG::Verdict OnMreOpen(const SG::Call& c) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (c.fromOurCode || !s || !s->connected() || s->role() != coop::net::Role::Client ||
        !c.object || !c.locals || !c.function || !ue_wrap::prop::IsDescendantOfProp(c.object))
        return SG::Verdict::Run;
    const int32_t action = R::FindParamOffset(c.function, L"action");
    const int32_t player = R::FindParamOffset(c.function, L"player");
    const int32_t frame = R::FunctionFrameSize(c.function);
    if (action < 0 || action >= frame || c.locals[action] != 4 || player < 0 ||
        player > frame || sizeof(void*) > frame - player) return SG::Verdict::Run;
    void* source = nullptr;
    std::memcpy(&source, c.locals + player, sizeof(source));
    if (!source || source != coop::players::Registry::Get().Local()) return SG::Verdict::Run;
    // Hand items have a separate generation/identity lane. Do not cancel their
    // native action: prop_spawn_authoring admits this hand MRE's own contents.
    if (coop::hand_item::IsHandAxisActor(c.object)) return SG::Verdict::Run;
    EnsureWorld();
    const uint64_t now = NowMs();
    // Press/release or another click before the host destroys the package must
    // not repeat the request cue. A denied/lost request can be tried again soon.
    if (g_lastMreRequest.Is(c.object) && now >= g_lastMreRequestAt && now - g_lastMreRequestAt < 750)
        return SG::Verdict::Cancel;
    const uint8_t slot = coop::players::Registry::Get().LocalPeerId();
    if (slot > 0 && slot < coop::net::kMaxPeers && TakeToken(slot) &&
        SendIntent(c.object, kOpenMre, 0.f, {}, false)) {
        g_lastMreRequest.Set(c.object);
        g_lastMreRequestAt = now;
        PlayMreRequestCue(c.object);
    }
    return SG::Verdict::Cancel;
}

SG::Verdict OnDamage(const SG::Call& c) {
    auto* s = g_session.load(std::memory_order_acquire);
    const bool wall = c.object && ue_wrap::placed_save::Type(c.object) == ue_wrap::placed_save::Kind::Wall;
    if (!s || !s->connected() || s->role() != coop::net::Role::Client || !c.object ||
        (!wall && !ue_wrap::prop::IsDescendantOfProp(c.object)))
        return SG::Verdict::Run;
    void* source = nullptr;
    if (!ReadArg(c, L"actor", source)) return SG::Verdict::Run;
    void* localPlayer = coop::players::Registry::Get().Local();
    if (!localPlayer || source != localPlayer) return SG::Verdict::Run;
    // Once a client-local player authored this hit, the prop must not execute
    // its native mutation on that client. That includes malformed parameters,
    // cooldown rejection, and a temporarily full network queue; running locally
    // in those cases creates precisely the one-peer break/destruction state
    // this authority lane is meant to prevent.
    float damage = 0.f;
    ue_wrap::FVector impact{};
    if (!ReadArg(c, L"damage", damage) || !ReadArg(c, L"impact", impact)) return SG::Verdict::Cancel;
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    // A local-player hit in a live client session must never fall back to native
    // client-side mutation just because the peer slot is not ready yet.
    if (localSlot == 0 || localSlot >= coop::net::kMaxPeers) return SG::Verdict::Cancel;
    const std::wstring held = coop::hand_item::HeldItem(localSlot);
    if (!TakeToken(localSlot) || !IsSwingValid(held, damage) || !std::isfinite(impact.X) ||
        !std::isfinite(impact.Y) || !std::isfinite(impact.Z) ||
        impact.X * impact.X + impact.Y * impact.Y + impact.Z * impact.Z > 10000000000.f)
        return SG::Verdict::Cancel;
    if (wall && (c.fromOurCode || !coop::net_pump::IsInAnnouncedWorld(c.object))) return SG::Verdict::Cancel;
    SendIntent(c.object, wall ? kWallDamage : kDamage, damage, impact, false);
    return SG::Verdict::Cancel;
}

SG::Verdict OnSlice(const SG::Call& c) {
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Client || !c.object ||
        !ue_wrap::prop::IsDescendantOfProp(c.object))
        return SG::Verdict::Run;
    if (localSlot == 0 || localSlot >= coop::net::kMaxPeers) return SG::Verdict::Cancel;
    if (!coop::hand_item::LastHeldClassIs(localSlot, L"prop_hacksaw_C")) return SG::Verdict::Run;
    uint8_t cleanValue = 0;
    if (!ReadArg(c, L"clean", cleanValue) || cleanValue > 1) return SG::Verdict::Cancel;
    const bool clean = cleanValue != 0;
    const ue_wrap::FVector zero{};
    if (!Sawable(c.object) || !TakeToken(localSlot)) return SG::Verdict::Cancel;
    SendIntent(c.object, kSlice, 0.f, zero, clean);
    return SG::Verdict::Cancel;
}

void RunDamage(void* target, void* player, float damage, const ue_wrap::FVector& impact) {
    // Most prop subclasses inherit this event. Exact-owner lookup silently
    // drops their hits after the client has already cancelled native damage.
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(target), L"addDamage");
    if (!fn) { UE_LOGW("prop_action_sync: addDamage missing on '%ls'", R::ClassNameOf(target).c_str()); return; }
    ue_wrap::ParamFrame frame(fn);
    ue_wrap::FVector loc{};
    if (!frame.valid() || !ue_wrap::engine::TryGetActorLocation(target, loc) ||
        !frame.Set<void*>(L"actor", player) || !frame.Set<float>(L"damage", damage) ||
        !frame.Set<ue_wrap::FVector>(L"impact", impact) || !frame.Set<bool>(L"skipSetting", false) ||
        !ue_wrap::hit_result::Write(frame, L"hit", target, ue_wrap::prop::GetStaticMesh(target), loc)) {
        UE_LOGW("prop_action_sync: cannot build addDamage frame on '%ls'", R::ClassNameOf(target).c_str());
        return;
    }
    ue_wrap::Call(target, frame);
}

void RunSlice(void* target, bool clean) {
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(target), L"slice");
    if (!fn) return;
    ue_wrap::ParamFrame frame(fn);
    if (frame.valid() && frame.Set<bool>(L"clean", clean)) ue_wrap::Call(target, frame);
}

void RunMreOpen(void* target, void* player) {
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(target), L"actionOptionIndex");
    void* owner = fn ? R::OuterOf(fn) : nullptr;
    // Only the verified MRE opening body (including inherited use). Arbitrary
    // prop action 4 can eat, toggle or summon things and must not run here.
    if (!owner || !R::NameEquals(R::NameOf(owner), L"prop_food_mre_C")) return;
    ue_wrap::ParamFrame frame(fn);
    ue_wrap::FVector loc{};
    if (!frame.valid() || !ue_wrap::engine::TryGetActorLocation(target, loc) ||
        !frame.Set<void*>(L"player", player) || !frame.Set<uint8_t>(L"action", 4) ||
        !frame.Set<void*>(L"lookAtComponent", ue_wrap::prop::GetStaticMesh(target)) ||
        !ue_wrap::hit_result::Write(frame, L"hit", target, ue_wrap::prop::GetStaticMesh(target), loc))
        return;
    // Native spawn() creates the random contents once, then destroys the packet.
    // The host birth watcher publishes every resulting prop with its final data.
    ue_wrap::Call(target, frame);
}
} // namespace

void Install(coop::net::Session* session) {
    if (g_session.load(std::memory_order_acquire) != session) OnDisconnect();
    g_session.store(session, std::memory_order_release);
    if (g_watchesInstalled) return;
    // These Blueprint events are overridden on several prop subclasses. A
    // class-scoped watch on prop_C only sees the base UFunction body, so a
    // client could still run (and locally destroy) a subclass without ever
    // sending its intent. Match the function name globally and keep the strict
    // prop/source/tool checks in the callbacks below.
    const bool a = SG::WatchName(kDamageFn, kDamageTag, &OnDamage, nullptr);
    const bool b = SG::WatchName(kSliceFn, kSliceTag, &OnSlice, nullptr);
    const bool c = SG::WatchClassName(L"prop_food_mre_C", L"actionOptionIndex", kMreTag, &OnMreOpen, nullptr);
    const bool d = SG::WatchClassName(L"prop_safe_C", L"opened", 0x53414645, &OnSafeOpened, &AfterSafeOpened);
    g_watchesInstalled = a && b && c && d;
    if (g_watchesInstalled)
        UE_LOGI("prop_action_sync: client damage/slice, world MRE opening and safe completion are relayed to host");
    else UE_LOGW("prop_action_sync: prop action watches failed (damage=%d slice=%d mre=%d safe=%d)", a ? 1 : 0, b ? 1 : 0, c ? 1 : 0, d ? 1 : 0);
}

void Tick() {
    EnsureWorld();
    auto* s = g_session.load(std::memory_order_acquire);
    if(s&&s->connected()&&s->role()==coop::net::Role::Host) {
        std::vector<std::wstring> keys;for(const auto& row:g_safePublish)keys.push_back(row.first);
        const auto world=g_world;const auto now=NowMs();
        for(const auto& key:keys) {
            auto it=g_safePublish.find(key);if(it==g_safePublish.end())continue;
            void* actor=it->second.actor.Get();
            if(!actor||ue_wrap::prop::GetKeyString(actor)!=key){g_safePublish.erase(it);continue;}
            if(now<it->second.retryAt)continue;
            const auto serial=it->second.serial;it->second.retryAt=now+500;
            const bool sent=coop::prop_save_data::Publish(s,actor,key);
            if(g_session.load()!=s||world!=ue_wrap::world_identity::Generation())return;
            it=g_safePublish.find(key);
            if(sent&&it!=g_safePublish.end()&&it->second.serial==serial)g_safePublish.erase(it);
        }
    }
    if (!s || !s->connected() || s->role() != coop::net::Role::Client) {
        g_pending.clear();
        return;
    }
    const uint64_t now = NowMs();
    while (!g_pending.empty()) {
        auto& item = g_pending.front();
        void* target = item.target.Get();
        if (!target || now < item.queuedAt || (item.payload.action!=kUnlockSafe && now - item.queuedAt > 5000) ||
            (item.payload.action == kWallDamage ? ue_wrap::wall_build::Key(target) : ue_wrap::prop::GetKeyString(target)) != coop::net::StringFromWireKey(item.payload.key) ||
            (item.payload.action == kOpenMre && coop::hand_item::IsHandAxisActor(target))) {
            g_pending.pop_front();
            continue;
        }
        if (!s->SendReliableToSlot(0, coop::net::ReliableKind::PropActionIntent,
                                   &item.payload, sizeof(item.payload))) break;
        g_pending.pop_front();
    }
}

void OnReliable(const coop::net::PropActionIntentPayload& p, uint8_t senderSlot) {
    EnsureWorld();
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host || senderSlot == 0 ||
        senderSlot >= coop::net::kMaxPeers || p.key.len>sizeof(p.key.data) ||
        (p.action != kDamage && p.action != kSlice && p.action != kOpenMre && p.action != kUnlockSafe && p.action != kWallDamage) ||
        p.clean > 1 || p._pad[0] || p._pad[1] || !std::isfinite(p.damage) ||
        !std::isfinite(p.impactX) || !std::isfinite(p.impactY) || !std::isfinite(p.impactZ)) return;
    const std::wstring key = coop::net::StringFromWireKey(p.key);
    const auto world = ue_wrap::world_identity::Generation();
    void* target = key.empty() ? nullptr : (p.action == kWallDamage ? ue_wrap::keyed_objects::Resolve(key.c_str()) :
        coop::prop_element_tracker::ResolveLiveActorByKey(key, nullptr));
    if (g_session.load() != s || world != ue_wrap::world_identity::Generation() || !target) return;
    if (p.action == kWallDamage ? ue_wrap::placed_save::Type(target) != ue_wrap::placed_save::Kind::Wall :
        !ue_wrap::prop::IsDescendantOfProp(target)) return;
    ue_wrap::CachedObjRef live; live.Set(target);
    const float impactLengthSq = p.impactX * p.impactX + p.impactY * p.impactY + p.impactZ * p.impactZ;
    if (!std::isfinite(impactLengthSq) || impactLengthSq > 10000000000.f) return;
    const auto auth = coop::element::IntentTarget::ForClientIntent(*s, senderSlot, kReachUU).Authorize(target);
    if (!auth) {
        // The requester already saw native dial success. A rejected completion
        // must restore the authority's lock instead of leaving a local unlock.
        if(p.action==kUnlockSafe&&ue_wrap::safe::IsSafe(target))
            coop::prop_save_data::PublishToSlot(s,senderSlot,target,key);
        return;
    }
    auto* remote = coop::players::Registry::Get().Puppet(senderSlot);
    void* player = remote && remote->valid() ? remote->GetActor() : nullptr;
    if (!player || !TakeToken(senderSlot)) return;
    if (p.action == kDamage || p.action == kWallDamage) {
        const std::wstring held = coop::hand_item::HeldItem(senderSlot);
        if (p.clean || !IsSwingValid(held, p.damage)) return;
        if (g_session.load() != s || world != ue_wrap::world_identity::Generation() || !live.Get()) return;
        if (p.action == kWallDamage) ue_wrap::wall_build::Damage(target, p.damage);
        else RunDamage(target, player, p.damage, {p.impactX, p.impactY, p.impactZ});
    } else if (p.action == kSlice) {
        if (p.damage != 0.f || !coop::hand_item::LastHeldClassIs(senderSlot, L"prop_hacksaw_C") ||
            !Sawable(target)) return;
        RunSlice(target, p.clean != 0);
    } else if (p.action == kOpenMre) {
        if (p.damage != 0.f || p.clean || p.impactX != 0.f || p.impactY != 0.f || p.impactZ != 0.f ||
            coop::hand_item::IsHandAxisActor(target)) return;
        RunMreOpen(target, player);
    } else if (p.action == kUnlockSafe) {
        if(p.damage!=0.f||p.clean||p.impactX!=0.f||p.impactY!=0.f||p.impactZ!=0.f||
           coop::hand_item::IsHandAxisActor(target)||!ue_wrap::safe::IsSafe(target))return;
        ue_wrap::CachedObjRef live;live.Set(target);
        const auto world=g_world;
        if(ue_wrap::safe::Unlock(target)&&live.Get()&&g_session.load()==s&&world==ue_wrap::world_identity::Generation()) {
            SG::Call opened{};opened.object=target;AfterSafeOpened(opened);
        }
    }
}
void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_pending.clear();
    g_safePublish.clear();++g_safeSerial;
    g_lastMreRequest.Reset();
    g_lastMreRequestAt = 0;
    g_world = 0;
    for (auto& bucket : g_buckets) bucket = {};
}
} // namespace coop::prop_action_sync
