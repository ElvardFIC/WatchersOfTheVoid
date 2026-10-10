// coop/world/power_decay.cpp -- see coop/world/power_decay.h.

#include "coop/world/power_decay.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/net/session.h"
#include "coop/session/net_pump.h"  // IsInAnnouncedWorld: a client's own world load runs natively

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace coop::power_decay {
namespace {

namespace sg = ue_wrap::script_gate;

UE_FACT_FN(kRowTimerTransformers, "power_decay", L"generatorFuckuper_C", L"timer_transformers", Declared);
constexpr int kTag = 0x50474454;  // 'PGDT'

std::atomic<coop::net::Session*> g_session{nullptr};
uint64_t g_refused = 0, g_ran = 0;
bool g_registered = false;
bool g_settled = false;

// [dev] power_decay_drill: `watch` says every tick's verdict and ends on the client's second tick; `red` lets a
// client's tick run, the negative control.
enum class Drill : uint8_t { Off, Watch, Red };
Drill DrillMode() {
    static const Drill d = [] {
        const std::string v = coop::config::ResolveString(::coop::config_registry::rows::power_decay_drill);
        return v == "watch" ? Drill::Watch : v == "red" ? Drill::Red : Drill::Off;
    }();
    return d;
}

sg::Verdict OnTickPre(const sg::Call& call) {
    if (call.fromOurCode) return sg::Verdict::Run;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return sg::Verdict::Run;
    const bool client = s->role() == coop::net::Role::Client;
    const Drill drill = DrillMode();
    const bool refuse = client && drill != Drill::Red && coop::net_pump::IsInAnnouncedWorld(call.object);
    const uint64_t n = refuse ? ++g_refused : ++g_ran;
    if (refuse && n == 1)
        UE_LOGI("power_decay: this client refuses its own decay tick -- only the host's dice wear the generators");
    if (drill != Drill::Off) {
        UE_LOGI("[POWER-DECAY] %s tick %s (%llu)", client ? "client" : "host", refuse ? "refused" : "ran",
                static_cast<unsigned long long>(n));
        if (client && g_refused + g_ran == 2)
            UE_LOGI("[POWER-DECAY] DONE client refused=%llu ran=%llu", static_cast<unsigned long long>(g_refused),
                    static_cast<unsigned long long>(g_ran));
    }
    return refuse ? sg::Verdict::Cancel : sg::Verdict::Run;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_registered) return;
    g_registered = true;
    if (!sg::WatchName(kRowTimerTransformers, kTag, &OnTickPre, nullptr))
        UE_LOGE("power_decay: the script-body gate refused the watch on %ls::%ls -- each peer rolls its own wear",
                kRowTimerTransformers.Class(), kRowTimerTransformers.Name());
}

void Tick() {
    if (g_settled || !g_registered) return;
    sg::ResolvePendingNames();
    if (sg::NameWatchLive(kRowTimerTransformers, kTag)) {
        g_settled = true;
        UE_LOGI("power_decay: the decay tick's gate is live");
    } else if (sg::NameWatchSettled(kRowTimerTransformers, kTag)) {
        g_settled = true;
        UE_LOGE("power_decay: the decay tick's gate is dead -- each peer rolls its own wear");
    }
}

void OnDisconnect() {
    if (g_refused || g_ran)
        UE_LOGI("power_decay: session end -- decay ticks refused %llu, run %llu",
                static_cast<unsigned long long>(g_refused), static_cast<unsigned long long>(g_ran));
    g_refused = g_ran = 0;
}

}  // namespace coop::power_decay
