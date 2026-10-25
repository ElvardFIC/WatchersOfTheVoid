// coop/interactables/atv_damage_gate.cpp -- see coop/interactables/atv_damage_gate.h.

#include "coop/interactables/atv_damage_gate.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/interactables/atv_sync.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <chrono>

namespace coop::atv_damage_gate {
namespace {

namespace SG = ue_wrap::script_gate;

// ATV_C's own bodies. The class owns all three; car1_witch_C inherits them, so a Declared row matches it too.
UE_FACT_FN(kRowAddDamage, "atv_damage_gate", L"ATV_C", L"addDamage", Declared);
UE_FACT_FN(kRowExploded, "atv_damage_gate", L"ATV_C", L"exploded", Declared);
UE_FACT_FN(kRowExplode, "atv_damage_gate", L"ATV_C", L"explode", Declared);

constexpr int kTagAddDamage = 0x41544430;  // 'ATD0'
constexpr int kTagExploded = 0x41544431;   // 'ATD1'
constexpr int kTagExplode = 0x41544432;    // 'ATD2'

enum Row { kAddDamage, kExploded, kExplode };

Counters g_counters{};

SG::Verdict Judge(Row row, const SG::Call& c) {
    // The developer red arm of the ATV health drill: no copy is a non-owner here, so the host's copy runs the damage
    // the drill's `gate` arm needs it to cancel. Resolved once.
    static const bool s_redNonowner =
        coop::config::ResolveEnum(coop::config_registry::rows::atv_health_red) == "nonowner";
    if (!s_redNonowner && coop::atv_sync::NonOwnerCopy(c.object)) {
        ++g_counters.cancelled[row];
        return SG::Verdict::Cancel;
    }
    ++g_counters.ran[row];
    return SG::Verdict::Run;
}

SG::Verdict OnAddDamagePre(const SG::Call& c) { return Judge(kAddDamage, c); }
SG::Verdict OnExplodedPre(const SG::Call& c) { return Judge(kExploded, c); }
SG::Verdict OnExplodePre(const SG::Call& c) { return Judge(kExplode, c); }

struct Hook {
    const ue_wrap::facts::Function* row;
    int                             tag;
    SG::PreFn                       pre;
    const wchar_t*                  name;
};
const Hook kTable[kRows] = {
    {&kRowAddDamage, kTagAddDamage, &OnAddDamagePre, L"addDamage"},
    {&kRowExploded, kTagExploded, &OnExplodedPre, L"exploded"},
    {&kRowExplode, kTagExplode, &OnExplodePre, L"explode"},
};

constexpr auto kRetry = std::chrono::seconds(1);
constexpr int  kMaxTries = 30;
using Clock = std::chrono::steady_clock;

bool              g_done[kRows] = {};
bool              g_dead[kRows] = {};
int               g_tries[kRows] = {};
Clock::time_point g_nextTry{};
bool              g_latched = false;

void SayDead(const Hook& h) {
    UE_LOGE("atv_damage_gate: the ATV_C::%ls watch is dead -- that route of an ATV's damage runs on every copy",
            h.name);
}

}  // namespace

// The shape of explosion_parking's Install (coop/world/explosion_parking.cpp): a refused registration is retried
// each second, 30 times; a registered watch the gate later settles not live is dead too. A Declared watch reads live
// before its class is judged, so a later class judgement that kills it is reported by the gate's own ERROR only.
void Install() {
    if (g_latched) return;
    if (Clock::now() >= g_nextTry) {
        g_nextTry = Clock::now() + kRetry;
        for (int i = 0; i < kRows; ++i) {
            if (g_done[i] || g_dead[i]) continue;
            if (SG::WatchName(*kTable[i].row, kTable[i].tag, kTable[i].pre, nullptr)) {
                g_done[i] = true;
            } else if (++g_tries[i] >= kMaxTries) {
                g_dead[i] = true;
                SayDead(kTable[i]);
            }
        }
    }
    bool pending = false, dead = false;
    for (int i = 0; i < kRows; ++i) {
        if (g_dead[i]) dead = true;
        else if (!g_done[i]) pending = true;
    }
    if (pending) return;
    if (dead) {   // each dead row said so when it died; the rest are left to the gate's own report
        g_latched = true;
        return;
    }
    SG::ResolvePendingNames();
    bool live = true;
    for (int i = 0; i < kRows; ++i)
        if (!SG::NameWatchLive(*kTable[i].row, kTable[i].tag)) live = false;
    if (live) {
        g_latched = true;
        UE_LOGI("atv_damage_gate: the ATV_C addDamage, exploded and explode watches are live");
        return;
    }
    for (int i = 0; i < kRows; ++i) {
        if (!SG::NameWatchSettled(*kTable[i].row, kTable[i].tag) || SG::NameWatchLive(*kTable[i].row, kTable[i].tag))
            continue;
        g_latched = true;
        SayDead(kTable[i]);
    }
}

Counters ReadCounters() { return g_counters; }

void Reset() { g_counters = Counters{}; }

}  // namespace coop::atv_damage_gate
