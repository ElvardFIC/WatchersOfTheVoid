#include "coop/creatures/grayboar_sync.h"
#include "coop/creatures/grayboar_actions.h"
#include "coop/element/registry.h"
#include "coop/net/session.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_drive_stream.h"
#include "coop/props/prop_echo_suppress.h"
#include "coop/player/player_damage.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>

namespace coop::grayboar_sync {
namespace {
namespace SG = ue_wrap::script_gate;
std::atomic<coop::net::Session*> g_session{nullptr};
std::atomic<bool> g_watched{false}, g_damageWatched{false};
constexpr int kBodyTickTag = 0x474254;
constexpr int kDamageTag = 0x474244;
constexpr int kNavigationTag = 0x47424E;
bool g_navigationWatched=false;
constexpr int kAuthorityTag = 0x47420000;
constexpr const wchar_t* kBoarClass = L"grayboar_C";
constexpr const wchar_t* kChargedClass = L"grayboar_overcharged_C";
struct AuthorityEntry { const wchar_t* owner; const wchar_t* name; };
// A scoped script watch matches the class DECLARING the function body, not the
// actor's ancestry. Inherited bodies use the base owner; overrides need their own.
constexpr AuthorityEntry kAuthorityEntries[] = {
    {kBoarClass, L"BndEvt__grayboar_biteZone_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature"},
    {kBoarClass, L"OnNotifyBegin_FEF3E3834EA127D483B3218D367A2A58"},
    {kBoarClass, L"sensePawn"},
    {kBoarClass, L"detected"},
    {kBoarClass, L"damaged"},
    {kBoarClass, L"OnNotifyBegin_F2D44BFA4340ED84016CA19643B74736"},
    {kBoarClass, L"OnCompleted_F2D44BFA4340ED84016CA19643B74736"},
    {kBoarClass, L"move"},
    {kBoarClass, L"updMov"},
    {kBoarClass, L"checkIfOnPath"},
    {kBoarClass, L"nomoveMove"},
    {kBoarClass, L"OnRequestFailed_EAF218CB4B42F6CCCF73E9B452B5AF9D"},
    {kBoarClass, L"OnMoveFinished_EAF218CB4B42F6CCCF73E9B452B5AF9D"},
    {kBoarClass, L"senseUpdate"},
    {kBoarClass, L"saw"},
    {kBoarClass, L"setWalking"},
    {kBoarClass, L"BndEvt__grayboar_StaticMesh_K2Node_ComponentBoundEvent_1_ComponentHitSignature__DelegateSignature"},
    {kBoarClass, L"reachedByLightning"},
    {kBoarClass, L"explosion"},
    {kChargedClass, L"explosion"},
    {kChargedClass, L"killed"},
    {kChargedClass, L"deathplosion"},
    {kChargedClass, L"objectDamaged"},
    {kChargedClass, L"zapSfx"},
};
constexpr int kAuthorityEntryCount = sizeof(kAuthorityEntries) / sizeof(kAuthorityEntries[0]);
bool g_authorityWatched[kAuthorityEntryCount]{};
bool g_velocityHook = false;
uint64_t g_velocityRetryAt = 0;
uint64_t g_epoch = 0;
bool g_stoppingNavigation = false;

template<class T> bool ReadArg(const SG::Call& call, const wchar_t* name, T& value) {
    if (!call.function || !call.locals) return false;
    const int off = ue_wrap::reflection::FindParamOffset(call.function, name);
    const int size = ue_wrap::reflection::FunctionFrameSize(call.function);
    if (off < 0 || off > size || sizeof(T) > static_cast<size_t>(size - off)) return false;
    std::memcpy(&value, call.locals + off, sizeof(value));
    return true;
}

bool IsBoar(void* actor) {
    namespace R = ue_wrap::reflection;
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = R::ClassOf(actor);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls)) {
        if (R::NameEquals(R::NameOf(cls), L"grayboar_C")) return true;
    }
    return false;
}

bool IsHostReplica(void* actor) {
    auto& registry = coop::element::Registry::Get();
    const auto eid = registry.EidForActor(actor);
    if (eid == 0 || eid >= coop::element::kHostRangeSize) return false;
    auto* element = registry.Get(eid);
    return element && element->IsMirror() && element->LiveActor() == actor;
}

bool IsAuthoritativeReplica(void* actor) {
    return coop::prop_echo_suppress::IsMirrorSpawn(actor) ||
           coop::prop_drive_stream::IsParked(actor) || IsHostReplica(actor);
}

bool HasClass(void* object, const wchar_t* name) {
    namespace R = ue_wrap::reflection;
    if (!object || !R::IsLive(object)) return false;
    void* cls = R::ClassOf(object);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls))
        if (R::NameEquals(R::NameOf(cls), name)) return true;
    return false;
}

void* ObjectProperty(void* object, const wchar_t* name) {
    namespace R = ue_wrap::reflection;
    const int offset = R::FindPropertyOffset(R::ClassOf(object), name);
    void* value = nullptr;
    if (offset >= 0) std::memcpy(&value, static_cast<uint8_t*>(object) + offset, sizeof(value));
    return value && R::IsLive(value) ? value : nullptr;
}

void StopReplicaNavigation(void* actor) {
    namespace R = ue_wrap::reflection;
    auto* session = g_session.load(std::memory_order_acquire);
    if (g_stoppingNavigation || !session || !session->running() || !session->connected() ||
        session->role() != coop::net::Role::Client || !IsBoar(actor) || !IsAuthoritativeReplica(actor)) return;
    void* pawn = ObjectProperty(actor, L"pawn");
    // Native construction spawns grayboarPawn with self as Owner. Require both
    // directions of possession before touching an AI controller (never a player).
    if (!HasClass(pawn, L"grayboarPawn_C") || ObjectProperty(pawn, L"Owner") != actor) return;
    void* controller = ObjectProperty(pawn, L"Controller");
    if (!HasClass(controller, L"AIController") || ObjectProperty(controller, L"Pawn") != pawn) return;
    ue_wrap::CachedObjRef bodyRef, pawnRef, controllerRef;
    bodyRef.Set(actor); pawnRef.Set(pawn); controllerRef.Set(controller);
    const auto epoch = g_epoch;
    const auto world = ue_wrap::world_identity::Generation();
    auto current = [&] {
        return epoch == g_epoch && world == ue_wrap::world_identity::Generation() &&
            g_session.load(std::memory_order_acquire) == session && session->running() && session->connected() &&
            session->role() == coop::net::Role::Client && bodyRef.Get() == actor &&
            pawnRef.Get() == pawn && controllerRef.Get() == controller && IsAuthoritativeReplica(actor) &&
            ObjectProperty(actor, L"pawn") == pawn && ObjectProperty(pawn, L"Owner") == actor &&
            ObjectProperty(pawn, L"Controller") == controller && ObjectProperty(controller, L"Pawn") == pawn;
    };
    struct Guard { Guard(){g_stoppingNavigation=true;} ~Guard(){g_stoppingNavigation=false;} } guard;
    ue_wrap::ParamFrame status(R::FindDispatchFunctionCached(R::ClassOf(controller), L"GetMoveStatus"));
    uint8_t state = 0;
    if (!status.valid() || !ue_wrap::Call(controller, status) || !current() ||
        !status.GetRaw(L"ReturnValue", &state, sizeof(state)) || state == 0 || state > 3) return;
    // EPathFollowingStatus: Idle=0, Waiting/Paused/Moving=1..3. StopMovement
    // aborts an existing path, including one started before save adoption. Its
    // task completion can reenter Blueprint; authority gates refuse that retry.
    ue_wrap::ParamFrame stop(R::FindDispatchFunctionCached(R::ClassOf(controller), L"StopMovement"));
    if (stop.valid() && current()) ue_wrap::Call(controller, stop);
    // No cached "stopped" bit: late possession, failed metadata resolution or a
    // later task gets checked again. No persistent controller settings changed.
}

SG::Verdict OnNativeAuthority(const SG::Call& call) {
    if(call.tag==kAuthorityTag) coop::grayboar_actions::Before(call,1);
    if(call.tag==kAuthorityTag+3) coop::grayboar_actions::Before(call,2);
    if(call.tag==kAuthorityTag+4) coop::grayboar_actions::Before(call,3);
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() ||
        session->role() != coop::net::Role::Client || !IsBoar(call.object))
        return SG::Verdict::Run;
    // The overlap starts a montage AND applies a physics impulse. Its bite
    // notify damages arbitrary int_objects, not just players. Refuse both seams:
    // a montage started before save adoption may still deliver its notify later.
    // sensePawn assigns a target and calls saw/move/detected. detected itself
    // starts a roar montage and binds target destruction.
    // Neither belongs to the independently running client copy's AI.
    // damaged authors health/death; the death notify creates loot, and its
    // completion destroys the actor. Those remain native host decisions.
    // Cosmetic replication must play assets directly, without these delegates.
    // A body hit has its own 10-damage/impulse path; lightning replaces the boar.
    // Charged overrides also author explosions, death timers, punches and spins.
    return IsAuthoritativeReplica(call.object) ? SG::Verdict::Cancel : SG::Verdict::Run;
}

void AfterNativeAuthority(const SG::Call& call) {
    if(call.tag==kAuthorityTag) coop::grayboar_actions::After(call,1);
    if(call.tag==kAuthorityTag+3) coop::grayboar_actions::After(call,2);
    if(call.tag==kAuthorityTag+4) coop::grayboar_actions::After(call,3);
}

SG::Verdict OnNavigationResume(const SG::Call& call) {
    auto* session=g_session.load(std::memory_order_acquire);
    if(!session || !session->running() || !session->connected() ||
       session->role()!=coop::net::Role::Client || !IsBoar(call.object) ||
       !IsAuthoritativeReplica(call.object))return SG::Verdict::Run;
    int32_t entry=-1;
    // Pre-adoption delays resume directly in the ubergraph, bypassing their
    // gated scheduling bodies. Keep target/delegate cleanup, construction and
    // cosmetic bookkeeping native; cancel only verified AI continuations.
    if (!ReadArg(call,L"EntryPoint",entry)) return SG::Verdict::Run;
    switch (entry) {
    case 15:   // move -> AIMoveTo and movement-update delegate binding
    case 543:  // successful request -> move
    case 558:  // failed request -> move
    case 966:  // delayed detected roar
    case 1016: // stuck check -> stop helper and move
    case 1381: // delayed path check -> stop helper and move
        return SG::Verdict::Cancel;
    default:
        return SG::Verdict::Run;
    }
}

void OnAnimationVelocity(void* actor, void* source, void*) {
    namespace R = ue_wrap::reflection;
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() ||
        session->role() != coop::net::Role::Client || !source) return;
    void* cls = R::ClassOf(source);
    if (!cls || !R::NameEquals(R::NameOf(cls), L"grayboar_Skeleton_AnimBlueprint_C") || !IsBoar(actor)) return;
    ue_wrap::FVector velocity{};
    if (!coop::prop_drive_stream::TryGetPresentationVelocity(actor, velocity)) return;
    if (void* result = ue_wrap::ufunction_hook::CurrentResult())
        std::memcpy(result, &velocity, sizeof(velocity));
}

void TryVelocityHook(void* actor) {
    if (g_velocityHook) return;
    const auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    if (now < g_velocityRetryAt) return;
    g_velocityRetryAt = now + 1000;
    void* fn = ue_wrap::reflection::FindDispatchFunctionCached(ue_wrap::reflection::ClassOf(actor), L"GetVelocity");
    if (fn) g_velocityHook = ue_wrap::ufunction_hook::InstallPostHook(fn, &OnAnimationVelocity);
}

SG::Verdict OnPlayerDamage(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() || !call.object)
        return SG::Verdict::Run;
    void* source = nullptr;
    if (!ReadArg(call, L"source", source) || !IsBoar(source)) return SG::Verdict::Run;
    auto& players = coop::players::Registry::Get();
    if (session->role() == coop::net::Role::Client) {
        if (call.object != players.Local()) return SG::Verdict::Cancel;
        // Wire damage is applied with no source actor and does not enter this
        // refusal. Save-adopted replicas are not fresh mirror spawns, and a
        // handoff to a hand removes their prop-drive row. Their host identity
        // remains authoritative across both transitions.
        return IsAuthoritativeReplica(source)
                   ? SG::Verdict::Cancel : SG::Verdict::Run;
    }
    if (call.object == players.Local()) return SG::Verdict::Run;
    const auto slot = players.PeerIdOfActor(call.object);
    float damage = 0.f;
    if (slot > 0 && slot < coop::players::kMaxPeers &&
        ReadArg(call, L"damage", damage) && std::isfinite(damage) && damage > 0.f && damage <= 1000.f)
        coop::player_damage::SendPlayerDamage(slot, damage);
    // A foreign body (including a preview/unregistered puppet) must never
    // write this machine's saveSlot.health, even if no relay can be addressed.
    return SG::Verdict::Cancel;
}

SG::Verdict OnBodyTick(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !call.object) return SG::Verdict::Run;
    if (session->role() == coop::net::Role::Client) {
        TryVelocityHook(call.object);
        StopReplicaNavigation(call.object);
        return SG::Verdict::Run;
    }
    // Init/record authoring may finish after the first tick. Retry naturally at
    // the next tick, without assigning a second identity or claiming a mirror.
    const auto eid = coop::prop_element_tracker::GetPropElementIdForActor(call.object);
    if (eid == 0 || eid >= coop::element::kHostRangeSize) return SG::Verdict::Run;
    // ReceiveTick applies force/torque to the prop's StaticMesh. Streaming the
    // helper pawn would not reproduce that movement. Claim is idempotent and
    // already handles held-item arbitration, join repair and dead-body cleanup.
    coop::prop_drive_host::Claim(call.object, "grayboar body");
    return SG::Verdict::Run;
}

} // namespace

void Install(coop::net::Session* session) {
    ++g_epoch;
    g_session.store(session, std::memory_order_release);
    coop::grayboar_actions::Install(session);
    if(!g_navigationWatched)g_navigationWatched=SG::WatchClassName(L"grayboar_C",
        L"ExecuteUbergraph_grayboar",kNavigationTag,&OnNavigationResume,nullptr);
    for (int i = 0; i < kAuthorityEntryCount; ++i) {
        if (!g_authorityWatched[i]) {
            g_authorityWatched[i] = SG::WatchClassName(kAuthorityEntries[i].owner, kAuthorityEntries[i].name,
                                                kAuthorityTag + i, &OnNativeAuthority, &AfterNativeAuthority);
            if (!g_authorityWatched[i]) UE_LOGW("grayboar: native authority watch %d unavailable", i);
        }
    }
    // Class-scoped body watch also sees subclasses inheriting this function,
    // including the overcharged variant. Other props' ticks never enter here.
    if (!g_watched.load(std::memory_order_acquire) &&
        SG::WatchClassName(L"grayboar_C", L"ReceiveTick", kBodyTickTag, &OnBodyTick, nullptr))
        g_watched.store(true, std::memory_order_release);
    else if (!g_watched.load(std::memory_order_acquire))
        UE_LOGW("grayboar: body movement watch unavailable");
    if (!g_damageWatched.load(std::memory_order_acquire) &&
        SG::WatchClassName(L"mainPlayer_C", L"Add Player Damage", kDamageTag, &OnPlayerDamage, nullptr))
        g_damageWatched.store(true, std::memory_order_release);
    else if (!g_damageWatched.load(std::memory_order_acquire))
        UE_LOGW("grayboar: player damage watch unavailable");
}

void OnDisconnect() {
    ++g_epoch;
    g_session.store(nullptr, std::memory_order_release);
    coop::grayboar_actions::OnDisconnect();
    // prop_drive_host owns and clears the claims at the session disconnect edge.
}

} // namespace coop::grayboar_sync
