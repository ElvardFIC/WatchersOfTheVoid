// coop/player/revive_protection.cpp -- see coop/player/revive_protection.h.

#include "coop/player/revive_protection.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/facts/function_row.h"

#include <windows.h>

#include <atomic>
#include <cstdint>

namespace coop::revive_protection {
namespace {

namespace E  = ue_wrap::engine;
namespace sg = ue_wrap::script_gate;

constexpr uint64_t kWindowMs = 5000;  // the game's own spawn window: mainPlayer BeginPlay's Delay(5)
constexpr int kTag = 0x52505230;      // 'RPR0'

UE_FACT_FN(kRowAttack, "revive_protection", L"mainPlayer_C", L"attack", Declared);

enum class Reg : uint8_t { Pending, Registered, Refused };

// Game-thread state.
Reg g_reg = Reg::Pending;
bool g_settled = false;
bool g_watchLive = false;
bool g_warnedNoWatch = false;
ue_wrap::CachedObjRef g_pawn;
uint64_t g_beganMs = 0;
uint64_t g_deadlineMs = 0;

std::atomic<bool> g_active{false};
std::atomic<uint32_t> g_ended{0};
std::atomic<uint8_t> g_lastWhy{static_cast<uint8_t>(EndWhy::None)};
std::atomic<uint64_t> g_lastWindowMs{0};

const char* WhyName(EndWhy why) {
    switch (why) {
        case EndWhy::Timeout: return "timeout";
        case EndWhy::Attack:  return "attack";
        case EndWhy::Session: return "session";
        case EndWhy::Revive:  return "revive";
        default:              return "none";
    }
}

sg::Verdict OnAttackPre(const sg::Call& c) {
    if (!g_active.load(std::memory_order_relaxed)) return sg::Verdict::Run;
    if (c.object && c.object == g_pawn.Get()) End(EndWhy::Attack);
    return sg::Verdict::Run;
}

}  // namespace

void Install() {
    if (g_settled) return;
    if (g_reg == Reg::Pending)
        g_reg = sg::WatchName(kRowAttack, kTag, &OnAttackPre, nullptr) ? Reg::Registered : Reg::Refused;
    if (g_reg == Reg::Registered) sg::ResolvePendingNames();
    if (g_reg == Reg::Registered && sg::NameWatchLive(kRowAttack, kTag)) {
        g_settled = true;
        g_watchLive = true;
        UE_LOGI("revive_protection: the attack watch on mainPlayer_C::attack is LIVE");
    } else if (g_reg == Reg::Refused || sg::NameWatchSettled(kRowAttack, kTag)) {
        g_settled = true;
        UE_LOGE("revive_protection: the attack watch is dead -- a swing will not end a window");
    }
}

void Begin(void* player) {
    if (g_active.load(std::memory_order_relaxed)) End(EndWhy::Revive);
    if (!E::WriteMainPlayerStartInvinc(player, true)) {
        UE_LOGW("revive_protection: the flag could not be written -- no window");
        return;
    }
    if (!g_watchLive && !g_warnedNoWatch) {  // not live yet, or settled dead
        g_warnedNoWatch = true;
        UE_LOGW("revive_protection: a window began with no live attack watch -- a swing will not end it");
    }
    g_pawn.Set(player);
    g_beganMs = ::GetTickCount64();
    g_deadlineMs = g_beganMs + kWindowMs;
    g_active.store(true, std::memory_order_release);
    UE_LOGI("revive_protection: began (%llu ms)", static_cast<unsigned long long>(kWindowMs));
}

void Tick() {
    if (g_active.load(std::memory_order_relaxed) && ::GetTickCount64() >= g_deadlineMs) End(EndWhy::Timeout);
}

void End(EndWhy why) {
    if (!g_active.load(std::memory_order_relaxed)) return;
    if (void* pawn = g_pawn.Get()) E::WriteMainPlayerStartInvinc(pawn, false);
    g_pawn.Reset();
    const uint64_t length = ::GetTickCount64() - g_beganMs;
    g_lastWindowMs.store(length, std::memory_order_release);
    g_lastWhy.store(static_cast<uint8_t>(why), std::memory_order_release);
    g_active.store(false, std::memory_order_release);
    g_ended.fetch_add(1, std::memory_order_release);
    UE_LOGI("revive_protection: ended (%s) after %llu ms", WhyName(why), static_cast<unsigned long long>(length));
}

bool Active() { return g_active.load(std::memory_order_acquire); }
uint32_t EndedCount() { return g_ended.load(std::memory_order_acquire); }
EndWhy LastEndWhy() { return static_cast<EndWhy>(g_lastWhy.load(std::memory_order_acquire)); }
uint64_t LastWindowMs() { return g_lastWindowMs.load(std::memory_order_acquire); }

}  // namespace coop::revive_protection
