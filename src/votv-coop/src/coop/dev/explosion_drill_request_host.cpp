// coop/dev/explosion_drill_request_host.cpp -- see coop/dev/explosion_drill_internal.h.
//
// The request arms' host half. The host does not walk: it holds the gamemode's fungun rule from its session start
// (a client's launcher in hand runs on the host's world rule) and judges its own request-lane and capture counters,
// as deltas against the baseline it took before the client could send. Past a reading's want is a failure at once;
// short of it is only not yet, until the failure bound. Every refusal reason, the shots the cap evicted and the
// ones that aged out unrequested stay at 0 but where the arm names them. The client's half is
// explosion_drill_request.cpp.

#include "coop/dev/explosion_drill_internal.h"

#include "explosion_drill_source.h"  // Guard, g_src (co-located private header)

#include <chrono>
#include <cstdint>

namespace coop::dev::explosion_drill::detail {

namespace ES = coop::native_explosion_sync;
namespace ER = coop::explosion_request;

namespace {

constexpr const char* kAcceptedNames[3] = {"requestsAccepted[projectile]", "requestsAccepted[hand]",
                                           "requestsAccepted[self]"};

struct Host {
    bool              began = false, guarded = false;
    ES::Counters      sync0{};
    ER::Counters      req0{};
    Clock::time_point at{};
};
Host g_h;

unsigned long long U(uint64_t v) { return static_cast<unsigned long long>(v); }

}  // namespace

bool RequestHostBegin() {
    Host& h = g_h;
    if (!h.began) {
        h.began = true;
        h.sync0 = ES::GetCounters();
        h.req0 = ER::GetCounters();
        SnapshotPre();  // the projectiles the arm's PASS waits out are the ones born after this
    }
    if (h.guarded) return true;
    g_src.host = true;
    h.guarded = Guard(true) == nullptr;  // a member that did not resolve is said once
    return h.guarded;
}

bool RequestHostTick(Arm arm) {
    Host& h = g_h;
    const RequestWant& w = RequestWantOf(arm);
    const ES::Counters s = ES::GetCounters();
    const ER::Counters c = ER::GetCounters();
    if (h.at == Clock::time_point{}) h.at = Clock::now();
    uint64_t accepted = 0;
    Miss     m;
    Compare(m, "shotsAccepted", c.shotsAccepted - h.req0.shotsAccepted, w.shots);
    for (int k = 0; k < 3; ++k) {
        const uint64_t in = c.requestsAccepted[k] - h.req0.requestsAccepted[k];
        accepted += in;
        Compare(m, kAcceptedNames[k], in, w.kind[k]);
    }
    const uint64_t captured = s.captured - h.sync0.captured;
    Compare(m, "captured", captured, w.blasts);
    Compare(m, "expired", c.expired - h.req0.expired, w.expired);
    Compare(m, "evicted", c.evicted - h.req0.evicted, 0);
    for (int r = 0; r < static_cast<int>(ER::Refusal::Count); ++r)
        Compare(m, ER::RefusalName(static_cast<ER::Refusal>(r)), c.refused[r] - h.req0.refused[r], 0);
    // Equal counters are not the end: the arm's rockets and sticky bombs must be gone too, so a second blast after the
    // first is still read.
    if (!m.name) {
        const int standing = Count(kRocketCls, true) + Count(kBombCls, true);
        if (standing > 0) m = Miss{"projectiles standing", static_cast<uint64_t>(standing), 0, false};
    }
    const char* name = RequestArmName(arm);
    if (m.over || (m.name && Past(h.at, kVerdictBound))) {
        Warn("FAIL host arm=%s %s got=%llu want=%llu", name, m.name, U(m.got), U(m.want));
        Unguard();
        return true;
    }
    if (m.name) return false;
    const uint64_t waited = c.waited - h.req0.waited;
    if (waited < w.waitedMin) {
        // The hand lane kept up with the client's swaps, so the shot never had to wait: the arm showed nothing.
        Warn("ABANDONED INCONCLUSIVE arm=%s waited=%llu want>=%u (the hand lane kept up)", name, U(waited),
             static_cast<unsigned>(w.waitedMin));
        Unguard();
        return true;
    }
    Info("host DONE PASS arm=%s accepted=%llu captured=%llu", name, U(accepted), U(captured));
    Unguard();
    return true;
}

void RequestHostReset() {
    Unguard();
    g_h = Host{};
}

}  // namespace coop::dev::explosion_drill::detail
