// coop/dev/burn_revive_drill.cpp -- see coop/dev/burn_revive_drill.h.

#include "coop/dev/burn_revive_drill.h"

#include "coop/config/config.h"
#include "coop/net/session.h"
#include "coop/player/death_revive.h"
#include "coop/player/players_registry.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/engine/engine_player_verbs.h"

#include <atomic>
#include <cstdint>

namespace coop::dev::burn_revive_drill {
namespace {

namespace E = ue_wrap::engine;
namespace V = ue_wrap::vitals;

// The drill's steps, one per edge: light the player, kill the player, judge the revive.
enum class Step : uint8_t { Light, Kill, Judge, Over };

// ignite keeps the larger of its fuel and the current timer; any value that outlasts the death is enough.
constexpr float kFuel = 60.f;

std::atomic<coop::net::Session*> g_session{nullptr};

Step     g_step = Step::Light;
uint32_t g_cleanupsAtKill = 0;  // death_revive::ScreenCleanupsEnded() when the lethal hit was delivered

bool Enabled() {
    static const bool s = coop::config::ResolveFlag(::coop::config_registry::rows::burn_revive_drill);
    return s;
}

void Fail(const char* why) {
    UE_LOGW("[BURN-REVIVE] FAIL client: %s", why);
    g_step = Step::Over;
}

// (a) The revive can answer a death and the spawn protection is over: only then does the lethal hit land and the
// revive run, so the drill tests the steady-state death and not the race before it.
bool Armed(void* local) {
    if (!coop::net_pump::IsWorldSettled() || coop::death_revive::ArmReadyAfterPawnMs() < 0) return false;
    bool invinc = true;
    return E::ReadMainPlayerStartInvinc(local, invinc) && !invinc;  // unreadable: not yet
}

void LightTick(void* local) {
    if (!Armed(local)) return;
    if (!E::InvokeMainPlayerIgnite(local, kFuel)) { Fail("ignite did not run"); return; }
    float burning = -1.f;  // the verb sets isBurning synchronously
    if (!V::Read(V::Field::Burning, &burning) || burning != 1.f) { Fail("ignited and not burning"); return; }
    UE_LOGI("[BURN-REVIVE] client lit");
    g_step = Step::Kill;
}

void KillTick(void* local) {
    float maxHealth = 0.f;
    if (!V::Read(V::Field::MaxHealth, &maxHealth) || maxHealth <= 0.f) { Fail("max health unread"); return; }
    g_cleanupsAtKill = coop::death_revive::ScreenCleanupsEnded();
    if (!E::InvokeAddPlayerDamage(local, 2.f * maxHealth, /*blood=*/true)) { Fail("the lethal hit did not run"); return; }
    bool ragdoll = false, dead = false;
    if (!E::ReadMainPlayerRagdollState(local, ragdoll, dead) || !dead) {
        Fail("the lethal hit killed nobody");  // an immortal or dreaming player refuses it
        return;
    }
    UE_LOGI("[BURN-REVIVE] client killed");
    g_step = Step::Judge;
}

void JudgeTick() {
    if (coop::death_revive::ScreenCleanupsEnded() <= g_cleanupsAtKill) return;
    float burnTime = -1.f, burning = -1.f;
    const bool haveBurnTime = V::Read(V::Field::BurningTime, &burnTime);
    const bool haveBurning = V::Read(V::Field::Burning, &burning);
    const bool revived = coop::death_revive::LastReviveSucceeded();
    if (haveBurnTime && haveBurning && burning == 0.f && burnTime <= 0.f && revived) {
        UE_LOGI("[BURN-REVIVE] client DONE burning=0 burn=%.1f -- PASS", burnTime);
    } else {
        UE_LOGW("[BURN-REVIVE] FAIL client: revived burning=%d burn=%.1f revive=%d",
                haveBurning ? static_cast<int>(burning) : -1,
                haveBurnTime ? burnTime : -1.f, revived ? 1 : 0);
    }
    g_step = Step::Over;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    if (!Enabled()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Client || g_step == Step::Over) return;
    if (g_step == Step::Judge) { JudgeTick(); return; }
    void* local = coop::players::Registry::Get().Local();
    if (!local) return;
    if (g_step == Step::Light) LightTick(local);
    else KillTick(local);
}

void OnDisconnect() {
    if (g_step == Step::Judge) Fail("the session ended before the revive was judged");  // a failed revive flees
    g_session.store(nullptr, std::memory_order_release);
    g_step = Step::Light;
    g_cleanupsAtKill = 0;
}

}  // namespace coop::dev::burn_revive_drill
