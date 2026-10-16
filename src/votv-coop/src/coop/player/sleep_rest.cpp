// coop/player/sleep_rest.cpp -- see coop/player/sleep_rest.h.

#include "coop/player/sleep_rest.h"

#include "coop/net/session.h"
#include "coop/player/players_registry.h"

#include "ue_wrap/actors/effects.h"
#include "ue_wrap/actors/sleep.h"
#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/world_singleton.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace coop::sleep_rest {
namespace {

namespace V  = ue_wrap::vitals;
namespace SLP = ue_wrap::sleep;
namespace FX = ue_wrap::effects;
namespace EN = ue_wrap::engine;
namespace sg = ue_wrap::script_gate;

// The factor the game's own sleep sets (mainGamemode.cpp:5428, SetGlobalTimeDilation(20)). The row below is
// the census's entry for the `sleep` body that holds that literal (include/ue_wrap/facts/
// fingerprints_0.9.0n.inc), so a game update that changes the body is named at boot; the watch compares
// the factor it finds in the same call.
constexpr float kNativeSleepDilation = 20.0f;
constexpr int kTagSleepRest = 0x534C5253;  // 'SLRS'
UE_FACT_FN(kRowSleep, "sleep_rest", L"mainGamemode_C", L"sleep", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};

bool g_watchRegistered = false;
bool g_watchSettled = false;
bool g_saidFactor = false;  // a factor other than the pinned one is said once, whatever the cause

// The rest in progress. The gamemode and the body are held by identity: a world change or a new body ends it.
bool g_resting = false;
ue_wrap::CachedObjRef g_gm;
ue_wrap::CachedObjRef g_body;
// Food and coffee as the last tick read them (after our own write). Unset on the first tick of a rest, which
// takes the baselines and corrects nothing.
bool  g_haveFood = false;
bool  g_haveCoffee = false;
float g_foodBase = 0.f;
float g_coffeeBase = 0.f;

bool ConnectedClient() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->connected() && s->role() == coop::net::Role::Client;
}

// The local player's body: the registry's (warm while the controller holds the sleeping pawn), else the
// pawn `sleep` took from the controller.
void* CurrentBody() {
    if (void* b = coop::players::Registry::Get().Local()) return b;
    return SLP::OrigPawn();
}

void SetEffectFactor(void* effect, void* ctx) {
    EN::SetCustomTimeDilation(effect, *static_cast<const float*>(ctx));
}

void BeginRest() {
    void* const gm = ue_wrap::world_singleton::Gamemode();
    void* const body = CurrentBody();
    if (!gm || !body) {
        UE_LOGW("sleep_rest: a rest began with no local body (%p) or gamemode (%p) -- it restores at the world's rate",
                body, gm);
        return;
    }
    if (!EN::SetCustomTimeDilation(body, kNativeSleepDilation)) return;
    g_gm.Set(gm);
    g_body.Set(body);
    g_resting = true;
    g_haveFood = g_haveCoffee = false;
    UE_LOGI("sleep_rest: rest begun -- the world stays at 1, the body and the world's effects run at %.0f",
            kNativeSleepDilation);
}

void EndRest(const char* why) {
    if (void* b = g_body.Get()) EN::SetCustomTimeDilation(b, 1.f);
    float one = 1.f;
    FX::ForEachWorldEffect(&SetEffectFactor, &one);
    g_resting = false;
    g_gm.Reset();
    g_body.Reset();
    g_haveFood = g_haveCoffee = false;
    UE_LOGI("sleep_rest: rest ended (%s) -- the body and the effects are back at 1", why);
}

// The call has run: the game set the world's dilation and the sleeping flag inside it. A refusal leaves the
// flag down and nothing to undo. Our own reflected calls come through here as the game's do.
void OnSleepPost(const sg::Call&) {
    if (!ConnectedClient()) return;
    float asleep = 0.f;
    if (!V::Read(V::Field::Sleeping, &asleep) || asleep == 0.f) return;
    if (!SLP::EnsureResolved()) {
        UE_LOGE("sleep_rest: a sleep began and the dilation calls do not resolve -- the world stays at the game's factor");
        return;
    }
    const float found = SLP::GetGlobalTimeDilation();
    if (std::fabs(found - kNativeSleepDilation) > 0.01f && !g_saidFactor) {
        g_saidFactor = true;
        UE_LOGW("sleep_rest: the game's sleep set a world dilation of %.3f, not the pinned %.1f -- a game update "
                "changed the speed-up; the body and the effects are given the pinned factor all the same",
                found, kNativeSleepDilation);
    }
    if (!SLP::SetGlobalTimeDilation(1.f)) {
        UE_LOGE("sleep_rest: the world's dilation could not be set back to 1 after the game's sleep");
        return;
    }
    BeginRest();
}

// Register the watch, then ask the gate to resolve its name until it is live or settled dead.
void EnsureWatch() {
    if (g_watchSettled) return;
    if (!g_watchRegistered) g_watchRegistered = sg::WatchName(kRowSleep, kTagSleepRest, nullptr, &OnSleepPost);
    if (!g_watchRegistered) return;
    sg::ResolvePendingNames();
    if (sg::NameWatchLive(kRowSleep, kTagSleepRest)) {
        g_watchSettled = true;
        UE_LOGI("sleep_rest: a client's sleep is seen at the call (a script-gate watch on mainGamemode_C::sleep)");
    } else if (sg::NameWatchSettled(kRowSleep, kTagSleepRest)) {
        g_watchSettled = true;
        UE_LOGE("sleep_rest: the sleep watch is dead -- a client's sleep runs the world at the game's factor");
    }
}

// A fall since the last read: food gets (1 - 1/F) of it back, coffee loses (F - 1) times it more. A rise
// is not touched, and the baseline is read again after the write.
void SettleNeeds() {
    constexpr float kF = kNativeSleepDilation;
    float food = 0.f, coffee = 0.f;
    if (V::Read(V::Field::Food, &food)) {
        const float fall = g_haveFood ? g_foodBase - food : 0.f;
        if (fall > 0.f && V::Write(V::Field::Food, food + fall * (1.f - 1.f / kF))) V::Read(V::Field::Food, &food);
        g_foodBase = food;
        g_haveFood = true;
    }
    if (V::Read(V::Field::CoffeePower, &coffee)) {
        const float fall = g_haveCoffee ? g_coffeeBase - coffee : 0.f;
        if (fall > 0.f && V::Write(V::Field::CoffeePower, std::max(coffee - fall * (kF - 1.f), 0.f)))
            V::Read(V::Field::CoffeePower, &coffee);
        g_coffeeBase = coffee;
        g_haveCoffee = true;
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running()) return;
    if (s->role() == coop::net::Role::Client) EnsureWatch();
    if (!g_resting) return;
    if (!ConnectedClient()) return EndRest("the session left");
    void* const gm = ue_wrap::world_singleton::Gamemode();
    if (!gm || gm != g_gm.Raw() || !g_gm.Alive()) return EndRest("the gamemode changed");
    void* const body = CurrentBody();
    if (!body || body != g_body.Raw() || !g_body.Alive()) return EndRest("the body changed");
    float asleep = 0.f;
    if (!V::Read(V::Field::Sleeping, &asleep) || asleep == 0.f) return EndRest("the sleeping flag fell");
    float factor = kNativeSleepDilation;
    FX::ForEachWorldEffect(&SetEffectFactor, &factor);
    SettleNeeds();
}

void OnDisconnect() {
    if (g_resting) EndRest("the session ended");
}

}  // namespace coop::sleep_rest
