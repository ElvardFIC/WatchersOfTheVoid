// coop/dev/atv_health_drill.cpp -- see coop/dev/atv_health_drill.h. The arm table, the phase clock, the verdict and chat
// lines and the entry points; the arms' phases are atv_health_drill_arms.cpp.

#include "coop/dev/atv_health_drill.h"

#include "atv_health_drill_detail.h"  // co-located private header (src tree, not include/)

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/net/session.h"

#include "ue_wrap/core/log.h"

#include <string>

namespace coop::dev::atv_health_drill {
namespace detail {
namespace {

constexpr Spec kSpecs[] = {
    {Arm::Off,  "off",  "", "", ""},
    {Arm::Sit,  "sit",  "seated", "sit",  "dismount"},
    {Arm::Grab, "grab", "synced", "grab", "release"},
};

}  // namespace

const Spec& SpecOf(Arm a) {
    for (const Spec& s : kSpecs)
        if (s.arm == a) return s;
    return kSpecs[0];
}

Arm ArmOf() {
    static const Arm a = [] {
        const std::string v = coop::config::ResolveEnum(coop::config_registry::rows::atv_health_drill);
        if (v == "sit") return Arm::Sit;
        if (v == "grab") return Arm::Grab;
        return Arm::Off;
    }();
    return a;
}

void Enter(Run& r, int phase, const char* name) {
    r.phase = phase;
    r.name = name;
    r.at = Clock::now();
}

bool Past(const Run& r, std::chrono::seconds bound) { return Clock::now() - r.at > bound; }

long long Ms(Clock::time_point from, Clock::time_point to) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count();
}

void Fail(const char* role, Arm a, Run& r, const std::string& what, const std::string& got, const std::string& want) {
    UE_LOGW("[ATV-HEALTH-DRILL] FAIL %s arm=%s phase=%s: %s, got %s, want %s", role, SpecOf(a).name, r.name,
            what.c_str(), got.c_str(), want.c_str());
    r.done = true;
}

void Pass(const char* role, Arm a, Run& r, const std::string& numbers) {
    UE_LOGI("[ATV-HEALTH-DRILL] %s DONE PASS arm=%s%s%s", role, SpecOf(a).name, numbers.empty() ? "" : " ",
            numbers.c_str());
    r.done = true;
}

// Lines no save can hold, as a prop class or a place could.
std::string ReadyLine(Arm a) { return std::string("ATVHD client ready ") + SpecOf(a).name; }
std::string PhaseLine(const char* name) { return std::string("ATVHD phase ") + name; }

}  // namespace detail

void Tick(coop::net::Session* s) {
    const detail::Arm a = detail::ArmOf();
    if (a == detail::Arm::Off || !s) return;
    if (s->role() == coop::net::Role::Host) detail::HostTick(s, a);
    else if (s->connected()) detail::ClientTick(a);
}

void OnDisconnect() {
    if (detail::ArmOf() == detail::Arm::Off) return;
    detail::HostReset();
    detail::ClientReset();
}

}  // namespace coop::dev::atv_health_drill
