// coop/world/salt_rocket_flight.cpp -- see coop/world/salt_rocket_flight.h.

#include "coop/world/salt_rocket_flight.h"

#include "coop/net/session.h"
#include "coop/props/prop_drive_host.h"
#include "coop/world/spawn_authority.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/prop_fire.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace coop::salt_rocket_flight {
namespace {

namespace sg = ue_wrap::script_gate;

// The rockets fusing at once on the host are its own player's and its clients' lights (each sender at most four a
// second) over a fuse and a thrust of a few seconds; a world fire lighting more is counted, not grown.
constexpr int kMaxFiring = 32;
constexpr int kTagFire = 0x53524652;  // 'SRFR'

UE_FACT_FN(kRowFire, "salt_rocket", L"prop_saltrocket_C", L"fire", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};

ue_wrap::CachedObjRef g_firing[kMaxFiring];
int g_count = 0;
Counters g_counters{};
bool g_fullWarned = false;
bool g_registered = false;
bool g_settled = false;

coop::net::Session* Connected() {
    auto* s = g_session.load(std::memory_order_acquire);
    return (s && s->connected()) ? s : nullptr;
}

bool IsHost(coop::net::Session* s) { return s && s->role() == coop::net::Role::Host; }

void Join(void* rocket) {
    for (int i = 0; i < g_count; ++i)
        if (g_firing[i].Is(rocket)) return;
    if (g_count >= kMaxFiring) {
        ++g_counters.full;
        if (!g_fullWarned) {
            g_fullWarned = true;
            UE_LOGW("[SALT] %d rockets are fusing at once -- the next one's flight is not driven", kMaxFiring);
        }
        return;
    }
    g_firing[g_count++].Set(rocket);
    ++g_counters.joined;
    UE_LOGI("[SALT] host fire key='%ls' -- its flight is driven",
            ue_wrap::prop::GetKeyString(rocket).c_str());
}

// The fire body starts the fuse when the prop has no flame yet (BP @875/@919): a call on a burning rocket is a
// repeat and the fuse runs on.
sg::Verdict OnFirePre(const sg::Call& c) {
    if (coop::spawn_authority::IsActiveClientSession()) {
        ++g_counters.clientFires;
        return sg::Verdict::Run;
    }
    if (IsHost(Connected()) && c.object && !ue_wrap::prop::PropFlameBaseValid(c.object)) Join(c.object);
    return sg::Verdict::Run;
}

}  // namespace

void Install(coop::net::Session* s) {
    g_session.store(s, std::memory_order_release);
    if (g_registered) return;
    g_registered = true;
    if (!sg::WatchName(kRowFire, kTagFire, &OnFirePre, nullptr))
        UE_LOGE("salt_rocket_flight: the script-body gate refused the watch on %ls::%ls -- a host rocket's flight is "
                "not driven", kRowFire.Class(), kRowFire.Name());
}

void Tick() {
    if (g_registered && !g_settled) {
        sg::ResolvePendingNames();
        if (sg::NameWatchLive(kRowFire, kTagFire)) {
            g_settled = true;
            UE_LOGI("salt_rocket_flight: the fire gate is live");
        } else if (sg::NameWatchSettled(kRowFire, kTagFire)) {
            g_settled = true;
            UE_LOGE("salt_rocket_flight: the fire gate is dead -- a host rocket's flight is not driven");
        }
    }
    if (!IsHost(Connected())) return;
    int kept = 0;
    for (int i = 0; i < g_count; ++i) {
        void* rocket = g_firing[i].Get();
        if (!rocket) continue;
        coop::prop_drive_host::Claim(rocket, "salt rocket fire");
        if (kept != i) g_firing[kept] = g_firing[i];
        ++kept;
    }
    for (int i = kept; i < g_count; ++i) g_firing[i].Reset();
    g_count = kept;
}

void OnWorldChange() {
    for (int i = 0; i < g_count; ++i) g_firing[i].Reset();
    g_count = 0;
}

void OnDisconnect() {
    if (g_counters.joined || g_counters.full)
        UE_LOGI("[SALT] flight lane at disconnect: joined=%u full=%u clientFires=%u", g_counters.joined,
                g_counters.full, g_counters.clientFires);
    OnWorldChange();
    g_counters = Counters{};
    g_fullWarned = false;
}

Counters GetCounters() { return g_counters; }

}  // namespace coop::salt_rocket_flight
