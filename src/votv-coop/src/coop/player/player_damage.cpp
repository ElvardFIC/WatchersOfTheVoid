// coop/player/player_damage.cpp -- see coop/player/player_damage.h.
//
// The damage relay's wire half: the host SENDS PlayerDamage to the hit peer, and the owner APPLIES
// it to its own possessed player. The vitals tail -- the pose stream's health fraction, and the
// puppet hurt flash its drop arms -- is already wired, so this module's job ends at making the
// owner's local saveSlot.health drop.

#include "coop/player/player_damage.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/element/element.h"
#include "coop/element/player.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/pvp.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <cmath>
#include <cstdint>

namespace coop::player_damage {
namespace {

namespace R = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace E = ue_wrap::engine;

std::atomic<coop::net::Session*> g_session{nullptr};  // read in the impact interceptor (any thread)

// Defensive per-hit bound: VOTV max health is ~100, so 1000 is generous headroom for
// any legitimate big hit while rejecting an absurd/exploit value before it reaches the
// damage BP. NaN/Inf/<=0 are rejected too.
constexpr float kMaxDamagePerHit = 1000.f;

bool ValidDamage(float d) { return std::isfinite(d) && d > 0.f && d <= kMaxDamagePerHit; }

// The impact-entry cancel. mainPlayer_C's three native->BP impact events, one shared PRE callback
// registered once for each. The class is loaded at the main menu and lives for the process
// (coop/dev/class_lifetime_probe measures it), so the registrations hold across a world change.
UE_FACT_FN(kRowImpactDamage, "player_damage", L"mainPlayer_C", L"impactDamage", Declared);
UE_FACT_FN(kRowImpactDamageCpp, "player_damage", L"mainPlayer_C", L"impactDamageCPP", Declared);
UE_FACT_FN(kRowImpactSquishCpp, "player_damage", L"mainPlayer_C", L"impactSquishCPP", Declared);
UE_FACT_FN(kRowAddDamageInterceptor, "player_damage", L"mainPlayer_C", L"addDamage", Declared);
UE_FACT_FN(kRowIgnite, "player_damage", L"mainPlayer_C", L"ignite", Declared);
const ue_wrap::facts::Function* const kImpactEntryRows[3] = {
    &kRowImpactDamage, &kRowImpactDamageCpp, &kRowImpactSquishCpp,
};
const wchar_t* const kImpactEntryNames[3] = {L"impactDamage", L"impactDamageCPP", L"impactSquishCPP"};
void*    g_impactFns[3] = {};
void*    g_addDamageFn = nullptr;
bool     g_impactInterceptorsDone = false;
uint32_t g_canceled = 0;   // rate-latched log counter
std::atomic<uint32_t> g_verbCanceled{0};
std::atomic<uint32_t> g_igniteCanceled{0};

// The LOCAL possessed player, published by Tick (game thread) for the interceptor's any-thread
// compare. The interceptor may not reach the registry: Registry::Local() mutates game-thread-only
// cache state, and a ProcessEvent interceptor can fire on a parallel-anim worker (game_thread.h's
// own contract). A published atomic keeps the CANCEL effective on every dispatching thread with one
// acquire load and no registry access.
std::atomic<void*> g_localPawn{nullptr};

// PRE cancel: a physical impact dispatched on a mainPlayer BODY that is NOT the local possessed
// player (a peer's puppet, or the F1 skin-preview mannequin) must not run the BP body -- it writes
// the PER-MACHINE saveSlot.health singleton, so lethal contact resolved against a puppet in the
// host's world drained the HOST, and killing a player took the server down with them. The victim's
// own machine computes the same contact on its own possessed player (self==local -> pass-through),
// so nothing is lost: MTA's victim-authoritative shape, no relay. Coop-session gated, because solo
// play has only the local body anyway and an unset registry must never eat solo damage.
bool OnImpactEntryPre(void* self, void* params, int which) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return false;
    if (!self) return false;
    void* localPawn = g_localPawn.load(std::memory_order_acquire);
    if (!localPawn) return false;           // pre-resolve boot window: native behavior
    void* fn = (which >= 0 && which < 3) ? g_impactFns[which] : nullptr;
    if (self == localPawn) return coop::pvp::CancelLocalImpact(params, fn);
    coop::pvp::OnPuppetImpact(self, params, fn);
    const uint32_t n = ++g_canceled;
    if (n <= 5 || (n % 50) == 0) {
        UE_LOGI("player_damage: canceled impact entry on NON-LOCAL body %p (#%u) -- "
                "the hit body's owner computes its own damage", self, n);
    }
    return true;  // cancel: the singleton saveSlot write must not run for a foreign body
}

bool OnImpact0(void* self, void* params) { return OnImpactEntryPre(self, params, 0); }
bool OnImpact1(void* self, void* params) { return OnImpactEntryPre(self, params, 1); }
bool OnImpact2(void* self, void* params) { return OnImpactEntryPre(self, params, 2); }

// A shovel or crowbar calls addDamage on whoever the trace hit. On a puppet that write does not
// reach the victim's health, and the impact-entry cancel never sees it. Treat it as the swing or
// the throw, and do not let it touch this machine's health.
bool OnAddDamage(void* self, void* params) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || !self) return false;
    void* localPawn = g_localPawn.load(std::memory_order_acquire);
    if (!localPawn) return false;
    if (self == localPawn) return coop::pvp::CancelLocalImpact(params, g_addDamageFn);
    coop::pvp::OnPuppetImpact(self, params, g_addDamageFn);
    return true;
}

// These BP bodies also run through internal Blueprint calls, bypassing the
// ProcessEvent impact interceptors above. An orphan's ignite starts latent loops
// that read player 0's controller and shared health; ragdollMode/fallen hide its
// meshes and enter the single-player death chain. Remote presentation belongs to
// RagdollDisplay instead. The script gate is GT-only, so resolve the current local
// pawn here rather than trusting yesterday's atomic pointer during world travel.
ue_wrap::script_gate::Verdict OnNativePlayerState(const ue_wrap::script_gate::Call& c) {
    using ue_wrap::script_gate::Verdict;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || !c.object) return Verdict::Run;
    void* local = coop::players::Registry::Get().Local();
    if (!local || c.object == local) return Verdict::Run;
    static uint32_t canceled = 0; // script-gate callbacks are game-thread-only
    const uint32_t n = ++canceled;
    g_verbCanceled.fetch_add(1, std::memory_order_relaxed);
    if (c.tag == 0x50530002) g_igniteCanceled.fetch_add(1, std::memory_order_relaxed);
    if (n <= 8 || n % 100 == 0)
        UE_LOGI("player_damage: blocked foreign native state tag=0x%x body=%p local=%p (#%u)",
                c.tag, c.object, local, n);
    return Verdict::Cancel;
}

void InstallNativeStateGuards() {
    static const wchar_t* const names[] = {L"ragdollMode", L"fallen", L"ignite", L"startBurning"};
    static bool registered[4]{};
    for (int i = 0; i < 4; ++i) {
        if (!registered[i]) registered[i] = i == 2
            ? ue_wrap::script_gate::WatchName(kRowIgnite, 0x50530000 + i, &OnNativePlayerState, nullptr)
            : ue_wrap::script_gate::WatchClassName(
                L"mainPlayer_C", names[i], 0x50530000 + i, &OnNativePlayerState, nullptr);
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

uint32_t RefusedDamage() { return g_verbCanceled.load(std::memory_order_relaxed); }
uint32_t RefusedIgnites() { return g_igniteCanceled.load(std::memory_order_relaxed); }

void Tick() {
    InstallNativeStateGuards();
    // Publish the local pawn for the interceptor's any-thread compare (GT here;
    // Registry::Local() is GT-only and cached).
    void* local = coop::players::Registry::Get().Local();
    g_localPawn.store(local, std::memory_order_release);
    coop::pvp::NoteBodies(local, coop::hand_item::LocalHandActor());
    coop::pvp::Tick();
    if (g_impactInterceptorsDone) return;
    void* cls = ue_wrap::object_index::ClassByName(L"mainPlayer_C");
    if (!cls) return;
    // A Blueprint class loads with its functions, so the first attempt with the class present is the
    // last: a name that does not resolve is a recooked Blueprint, and a full table is a capacity fault
    // to fix, not a slot to wait for.
    g_impactInterceptorsDone = true;
    bool all = true;
    GT::UFunctionInterceptor cbs[3] = { &OnImpact0, &OnImpact1, &OnImpact2 };
    for (int i = 0; i < 3; ++i) {
        const auto resolved = ue_wrap::facts::ResolveOn(*kImpactEntryRows[i], cls);
        g_impactFns[i] = resolved.Fn();
        if (resolved && GT::RegisterInterceptor(resolved, cbs[i])) continue;
        all = false;
        UE_LOGE("player_damage: impact entry %ls %s -- a non-local body runs its damage body", kImpactEntryRows[i]->Name(),
                g_impactFns[i] ? "did not register (the interceptor table is full)"
                               : "is not declared on mainPlayer_C (a recooked Blueprint)");
    }
    const auto addDamage = ue_wrap::facts::ResolveOn(kRowAddDamageInterceptor, cls);
    g_addDamageFn = addDamage.Fn();
    if (!addDamage || !GT::RegisterInterceptor(addDamage, &OnAddDamage)) {
        UE_LOGE("player_damage: addDamage %s -- a shovel swing on a puppet never reaches that player",
                g_addDamageFn ? "did not register (the interceptor table is full)"
                              : "is not declared on mainPlayer_C");
    }
    if (all) {
        UE_LOGI("player_damage: impact-entry PRE cancels installed (impactDamage=%p "
                "impactDamageCPP=%p impactSquishCPP=%p addDamage=%p) -- non-local bodies never run "
                "the saveSlot damage body",
                g_impactFns[0], g_impactFns[1], g_impactFns[2], g_addDamageFn);
    }
}

void OnWireDamage(const coop::net::PlayerDamagePayload& p) {
    if (!ValidDamage(p.damage)) {
        UE_LOGW("player_damage: dropping wire damage with invalid amount=%.3f", p.damage);
        return;
    }
    const uint32_t targetEid = p.targetElementId;
    const float damage = p.damage;
    GT::Post([targetEid, damage] {
        auto& reg = coop::players::Registry::Get();
        const coop::element::ElementId localEid = reg.LocalPlayerElementId();
        // Defense-in-depth: PlayerDamage is sent point-to-point to our slot, so it IS
        // for us; the element check guards a future fan-out/relay-path bug. Apply on a
        // match OR when our id isn't allocated yet (boot window); drop ONLY on a
        // definite mismatch against a valid local id.
        if (localEid != coop::element::kInvalidId && targetEid != localEid) {
            UE_LOGW("player_damage: wire damage targetEid=%u != our localEid=%u -- dropping "
                    "(not addressed to us)", targetEid, localEid);
            return;
        }
        void* mp = reg.Local();
        if (!mp || !R::IsLive(mp)) {
            UE_LOGW("player_damage: wire damage but no live local player to apply to -- dropping");
            return;
        }
        const bool possessed = (E::GetController(mp) != nullptr);
        const bool isPup = reg.IsPuppet(mp);
        float before = -1.f, after = -1.f;
        ue_wrap::vitals::Read(ue_wrap::vitals::Field::Health, &before);
        const bool ok = E::InvokeAddPlayerDamage(mp, damage);
        ue_wrap::vitals::Read(ue_wrap::vitals::Field::Health, &after);
        UE_LOGI("player_damage: applied %.1f to own player (targetEid=%u) -> ok=%d, "
                "saveSlot.health %.2f -> %.2f [mp possessed=%d isPuppet=%d]",
                damage, targetEid, ok ? 1 : 0, before, after, possessed ? 1 : 0, isPup ? 1 : 0);
    });
}

void SendPlayerDamage(int ownerSlot, float damage) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) {
        UE_LOGW("player_damage: SendPlayerDamage but session is unset");
        return;
    }
    if (ownerSlot <= 0 || ownerSlot >= coop::players::kMaxPeers) {
        UE_LOGW("player_damage: SendPlayerDamage invalid ownerSlot=%d", ownerSlot);
        return;
    }
    if (!ValidDamage(damage)) {
        UE_LOGW("player_damage: SendPlayerDamage invalid amount=%.3f", damage);
        return;
    }
    auto& reg = coop::players::Registry::Get();
    coop::element::Player* el = reg.GetPlayerElement(static_cast<uint8_t>(ownerSlot));
    if (!el) {
        UE_LOGW("player_damage: SendPlayerDamage slot=%d has no Player Element yet -- skipping "
                "(peer not fully identified)", ownerSlot);
        return;
    }
    coop::net::PlayerDamagePayload p{};
    p.targetElementId = el->GetId();
    p.damage = damage;
    const bool sent = s->SendReliableToSlot(
        ownerSlot, coop::net::ReliableKind::PlayerDamage, &p, sizeof(p), /*senderSlot=*/0);
    UE_LOGI("player_damage: sent PlayerDamage(%.1f) to slot=%d (targetEid=%u) -> sent=%d",
            damage, ownerSlot, p.targetElementId, sent ? 1 : 0);
}

bool DebugForceHitPuppet(int ownerSlot, float damage) {
    if (ownerSlot <= 0 || ownerSlot >= coop::players::kMaxPeers) {
        UE_LOGW("player_damage: DebugForceHitPuppet invalid ownerSlot=%d", ownerSlot);
        return false;
    }
    auto& reg = coop::players::Registry::Get();
    if (!reg.Puppet(static_cast<uint8_t>(ownerSlot))) {
        UE_LOGW("player_damage: DebugForceHitPuppet slot=%d has no puppet (peer not connected) "
                "-- cannot simulate a hit", ownerSlot);
        return false;
    }
    SendPlayerDamage(ownerSlot, damage);
    return true;
}

}  // namespace coop::player_damage
