// coop/dev/badsun_drill.cpp -- see coop/dev/badsun_drill.h.

#include "coop/dev/badsun_drill.h"

#include "coop/config/config.h"
#include "coop/dev/set_clock.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_snapshot.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/world/bad_sun.h"
#include "ue_wrap/world/daynightcycle.h"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

namespace coop::dev::badsun_drill {
namespace {

namespace BS  = ue_wrap::bad_sun;
namespace DNC = ue_wrap::daynightcycle;
using Steady = std::chrono::steady_clock;

enum class Arm { Off, ClockArm };

// WaitSet: the clock shows the host's set. Sample: the count and the copy's tests, every second.
enum class Step { WaitJoin, HostSetClock, WaitSet, Sample, Done };

constexpr float kSetFraction = (6 * 60 + 58) / 1440.f;  // 06:58, two game minutes before the copy's hour test
constexpr int   kLandedHour  = 6;
constexpr int   kLandedMin   = 57;   // the set reads 06:58, and a float that rounds down reads 06:57
constexpr int   kActiveHour  = 7;
constexpr int   kMaxDrainFails = 10;
constexpr auto  kJoinBound   = std::chrono::seconds(300);
constexpr auto  kClockBound  = std::chrono::seconds(120);  // from the arm's start to the clock reading 07:00
constexpr auto  kActiveBound = std::chrono::seconds(15);   // from the clock reading 07:00 to the copy active
constexpr auto  kSample      = std::chrono::seconds(1);

std::atomic<coop::net::Session*> g_session{nullptr};
Step              g_step = Step::WaitJoin;
bool              g_saidArm = false;
int               g_slot = -1;
Steady::time_point g_deadline{};
Steady::time_point g_nextSample{};
Steady::time_point g_t07{};
bool              g_t07Seen = false;
bool              g_counted = false;
int               g_drainFails = 0;
int               g_lastHour = -1, g_lastMin = -1;
bool              g_lastWidget = false, g_lastSender = false;

Arm ArmOf() {
    static const Arm a = [] {
        const std::string v = coop::config::ResolveEnum(::coop::config_registry::rows::badsun_drill);
        return v == "clock" ? Arm::ClockArm : Arm::Off;
    }();
    return a;
}

bool IsHost(coop::net::Session* s) { return s->role() == coop::net::Role::Host; }
const char* RoleName(bool host) { return host ? "host" : "client"; }

void Say(bool host, bool pass, const char* fmt, ...) {
    char buf[384];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (pass) UE_LOGI("[BADSUN-DRILL] clock %s PASS %s", RoleName(host), buf);
    else UE_LOGW("[BADSUN-DRILL] clock %s FAIL %s", RoleName(host), buf);
}

void Pass(bool host, const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Say(host, true, "%s", buf);
    g_step = Step::Done;
}

void Fail(bool host, const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Say(host, false, "%s", buf);
    g_step = Step::Done;
}

// What the bound ending says, by where the peer stood.
void FailTimeout(bool host) {
    switch (g_step) {
    case Step::WaitJoin:
        return Fail(host, "timed out waiting for the join");
    case Step::HostSetClock:
    case Step::WaitSet:
        return Fail(host, "the clock never showed the set to 06:58 within %d s of the arm's start (last read %02d:%02d)",
                    static_cast<int>(kClockBound.count()), g_lastHour, g_lastMin);
    default:
        break;
    }
    if (!g_t07Seen)
        return Fail(host, "the clock never reached 07:00 within %d s of the arm's start (last read %02d:%02d)",
                    static_cast<int>(kClockBound.count()), g_lastHour, g_lastMin);
    Fail(host, "the copy was not active by both tests %d s after the clock read 07:00 (widget %d, sender %d, last read %02d:%02d)",
         static_cast<int>(kActiveBound.count()), g_lastWidget ? 1 : 0, g_lastSender ? 1 : 0, g_lastHour, g_lastMin);
}

// Both peers: once the join is over the arm's 120 s start, and the host sets the clock.
void BeginArm(bool host) {
    g_deadline = Steady::now() + kClockBound;
    g_step = host ? Step::HostSetClock : Step::WaitSet;
}

void HostSetClock() {
    int32_t h = -1, m = -1, d = -1;
    DNC::ReadTimeZ(h, m, d);
    UE_LOGI("badsun_drill: [host] slot %d's join is over; the clock reads %02d:%02d, setting it to 06:58", g_slot, h, m);
    coop::dev::set_clock::SetTimeFraction(kSetFraction);
    g_step = Step::WaitSet;
}

// The named clock, every tick: this peer's own reading of the set, and of the hour the copy tests.
bool ReadClock() {
    int32_t h = 0, m = 0, d = 0;
    if (!DNC::ReadTimeZ(h, m, d)) return false;
    g_lastHour = h;
    g_lastMin = m;
    return true;
}

void WaitSet(bool host) {
    if (!ReadClock()) return;
    if (g_lastHour != kLandedHour || g_lastMin < kLandedMin) return;
    UE_LOGI("badsun_drill: [%s] the clock shows the set (%02d:%02d); sampling", RoleName(host), g_lastHour, g_lastMin);
    g_nextSample = Steady::now();
    g_step = Step::Sample;
}

void Sample(bool host) {
    if (!ReadClock()) return;
    const auto now = Steady::now();
    if (g_lastHour >= kActiveHour && !g_t07Seen) {
        g_t07Seen = true;
        g_t07 = now;
        g_deadline = now + kActiveBound;
        UE_LOGI("badsun_drill: [%s] the clock reads %02d:%02d", RoleName(host), g_lastHour, g_lastMin);
    }
    if (now < g_nextSample) return;
    g_nextSample = now + kSample;

    void* first = nullptr;
    const int n = BS::CountLive(&first);
    if (n < 0) {
        if (++g_drainFails >= kMaxDrainFails) return Fail(host, "the index could not drain (%d tries in a row)", g_drainFails);
        return;
    }
    g_drainFails = 0;
    if (!g_counted) {
        g_counted = true;
        if (n != 1) return Fail(host, "%d live Bad Sun copies at the first sample, expected 1 (clock %02d:%02d)", n,
                                g_lastHour, g_lastMin);
    }
    if (!first) return Fail(host, "the Bad Sun copy is gone (%d live, clock %02d:%02d)", n, g_lastHour, g_lastMin);

    g_lastWidget = BS::IsActive(first);
    g_lastSender = BS::IsActiveEventSender(first);
    UE_LOGI("badsun_drill: [%s] clock %02d:%02d, %d live, widget %d, sender %d", RoleName(host), g_lastHour, g_lastMin,
            n, g_lastWidget ? 1 : 0, g_lastSender ? 1 : 0);
    if (!g_t07Seen) {
        if (g_lastWidget || g_lastSender)
            Fail(host, "the copy went active at %02d:%02d, before 07:00 (widget %d, sender %d): it did not read this clock",
                 g_lastHour, g_lastMin, g_lastWidget ? 1 : 0, g_lastSender ? 1 : 0);
        return;
    }
    if (g_lastWidget && g_lastSender) {
        const float secs = std::chrono::duration<float>(now - g_t07).count();
        Pass(host, "the copy is active by both tests at %02d:%02d, %.1f s after the clock read 07:00", g_lastHour, g_lastMin,
             secs);
    }
}

bool JoinOver(coop::net::Session* s, bool host) {
    if (host) {
        for (int slot = 1; slot < static_cast<int>(coop::players::kMaxPeers) && g_slot < 0; ++slot)
            if (s->IsSlotWorldReady(slot) && coop::prop_snapshot::IsBracketClosed(slot)) g_slot = slot;
        return g_slot >= 0;
    }
    if (!coop::net_pump::HasAnnouncedWorldReady() ||
        coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle)
        return false;
    UE_LOGI("badsun_drill: [client] joined; waiting for the clock to show the host's set");
    return true;
}

}  // namespace

bool IsEnabled() { return ArmOf() != Arm::Off; }

void Install(coop::net::Session* session) {
    if (!IsEnabled()) return;
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    if (!IsEnabled()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running() || g_step == Step::Done) return;
    const bool host = IsHost(s);
    if (!g_saidArm) {
        g_saidArm = true;
        g_deadline = Steady::now() + kJoinBound;
        UE_LOGI("badsun_drill: [%s] armed -- %s", RoleName(host),
                host ? "once a client's join is over, the clock to 06:58, then this peer's own copy"
                     : "once joined, this peer's own copy against its own clock");
    }
    if (Steady::now() > g_deadline) return FailTimeout(host);
    switch (g_step) {
    case Step::WaitJoin:
        if (JoinOver(s, host)) BeginArm(host);
        return;
    case Step::HostSetClock: HostSetClock(); return;
    case Step::WaitSet:      WaitSet(host); return;
    case Step::Sample:       Sample(host); return;
    default:                 return;
    }
}

void OnDisconnect() {
    g_step = Step::WaitJoin;
    g_saidArm = false;
    g_slot = -1;
    g_t07Seen = false;
    g_counted = false;
    g_drainFails = 0;
    g_lastHour = g_lastMin = -1;
    g_lastWidget = g_lastSender = false;
}

}  // namespace coop::dev::badsun_drill
