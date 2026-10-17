// coop/dev/badsun_drill.cpp -- see coop/dev/badsun_drill.h.

#include "coop/dev/badsun_drill.h"

#include "coop/config/config.h"
#include "coop/dev/set_clock.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_snapshot.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"
#include "coop/world/time_sync.h"

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

// Begin: the first tick picks the peer's road. HostPark: the host reads the save's clock and copy, then parks the
// clock before any join. WaitJoin: the join is over. WaitStart: the first clock reading of hour 6 or later (a
// client also waits for the first host sample to land). Sample: the count and the copy's tests, every second.
enum class Step { Begin, HostPark, WaitJoin, WaitStart, Sample, Done };

constexpr float kParkFraction = (5 * 60) / 1440.f;       // 05:00, two game hours before the copy's hour test
constexpr float kSetFraction  = (6 * 60 + 30) / 1440.f;  // 06:30, about 94 s of real time before the copy's hour test
constexpr int   kStartHour   = 6;
constexpr int   kActiveHour  = 7;
constexpr int   kMaxDrainFails = 10;
constexpr auto  kJoinBound   = std::chrono::seconds(300);  // from the first tick (the host's park, the client's join)
constexpr auto  kClockBound  = std::chrono::seconds(120);  // from the join being over to the clock reading 07:00
constexpr auto  kActiveBound = std::chrono::seconds(15);   // from the clock reading 07:00 to the copy active
constexpr auto  kSample      = std::chrono::seconds(1);

std::atomic<coop::net::Session*> g_session{nullptr};
Step              g_step = Step::Begin;
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

// The one verdict line; the drill is over after it.
void End(bool host, const char* word, const char* fmt, va_list ap) {
    char buf[320];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    if (word[0] == 'P') UE_LOGI("[BADSUN-DRILL] clock %s %s %s", RoleName(host), word, buf);
    else UE_LOGW("[BADSUN-DRILL] clock %s %s %s", RoleName(host), word, buf);
    g_step = Step::Done;
}

void Pass(bool host, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    End(host, "PASS", fmt, ap);
    va_end(ap);
}

void Fail(bool host, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    End(host, "FAIL", fmt, ap);
    va_end(ap);
}

void Inconclusive(bool host, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    End(host, "INCONCLUSIVE", fmt, ap);
    va_end(ap);
}

// What the bound ending says, by where the peer stood.
void FailTimeout(bool host) {
    switch (g_step) {
    case Step::Begin:
    case Step::HostPark:
        return Fail(host, "the host's clock or copy was not readable within %d s of the first tick",
                    static_cast<int>(kJoinBound.count()));
    case Step::WaitJoin:
        return Fail(host, "timed out waiting for the join");
    case Step::WaitStart:
        return Fail(host, "the clock never read hour %d or later within %d s of the join being over (last read %02d:%02d)",
                    kStartHour, static_cast<int>(kClockBound.count()), g_lastHour, g_lastMin);
    default:
        break;
    }
    if (!g_t07Seen)
        return Fail(host, "the clock never reached 07:00 within %d s of the join being over (last read %02d:%02d)",
                    static_cast<int>(kClockBound.count()), g_lastHour, g_lastMin);
    Fail(host, "the copy was not active by both tests %d s after the clock read 07:00 (widget %d, sender %d, last read %02d:%02d)",
         static_cast<int>(kActiveBound.count()), g_lastWidget ? 1 : 0, g_lastSender ? 1 : 0, g_lastHour, g_lastMin);
}

// The named clock, every tick: this peer's own reading.
bool ReadClock() {
    int32_t h = 0, m = 0, d = 0;
    if (!DNC::ReadTimeZ(h, m, d)) return false;
    g_lastHour = h;
    g_lastMin = m;
    return true;
}

// Host, before any client: read the save's clock and copy, then park the clock at 05:00 so the join runs in the
// hours before the copy's own test. A copy already active at hosting measures nothing.
void HostPark() {
    if (!ReadClock()) return;
    void* first = nullptr;
    const int n = BS::CountLive(&first);
    if (n < 0) {
        if (++g_drainFails >= kMaxDrainFails) return Fail(true, "the index could not drain (%d tries in a row)", g_drainFails);
        return;
    }
    g_drainFails = 0;
    bool active = false;
    if (first) {
        // A copy exists, so badSun_C has loaded: a widget read that fails is the reader's latched resolve failure.
        // The sender read also fails while the gamemode has not loaded yet; that waits for the next tick, and the
        // join bound ends a latch of it.
        bool widget = false, sender = false;
        if (!BS::ReadActive(first, widget))
            return Fail(true, "the Bad Sun reader did not resolve (clock %02d:%02d)", g_lastHour, g_lastMin);
        if (!BS::ReadIsSender(first, sender)) return;
        active = widget || sender;
    }
    UE_LOGI("badsun_drill: [host] the save's clock reads %02d:%02d, %d copies, active %d", g_lastHour, g_lastMin, n,
            active ? 1 : 0);
    if (active)
        return Inconclusive(true, "the save's copy was active at hosting (%02d:%02d); the arm needs a save before 07:00",
                            g_lastHour, g_lastMin);
    // The named clock (timeZ) is rebuilt on the cycle's next tick: a copy that polls in this frame reads the old
    // hour. Sample's first-sample rule catches the peer that began after 07:00 on that read.
    if (!coop::dev::set_clock::ApplyTimeFraction(kParkFraction))
        return Fail(true, "the clock set to 05:00 was refused");
    g_deadline = Steady::now() + kJoinBound;
    g_step = Step::WaitJoin;
}

// Both peers, once the join is over: the clock window opens; the host also sets the clock to 06:30.
void BeginArm(bool host) {
    if (host) {
        UE_LOGI("badsun_drill: [host] slot %d's join is over; setting the clock to 06:30", g_slot);
        if (!coop::dev::set_clock::ApplyTimeFraction(kSetFraction))
            return Fail(true, "the clock set to 06:30 was refused");
    }
    g_deadline = Steady::now() + kClockBound;
    g_step = Step::WaitStart;
}

void WaitStart(bool host) {
    // A client's clock holds its own world's value until the first host sample lands: not the host's clock.
    if (!host && coop::time_sync::LastHostDayZ() < 0) return;
    if (!ReadClock() || g_lastHour < kStartHour) return;
    UE_LOGI("badsun_drill: [%s] the clock reads %02d:%02d; sampling", RoleName(host), g_lastHour, g_lastMin);
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
    const bool firstSample = !g_counted;
    g_counted = true;
    if (firstSample && n != 1)
        return Fail(host, "%d live Bad Sun copies at the first sample, expected 1 (clock %02d:%02d)", n, g_lastHour,
                    g_lastMin);
    if (!first) return Fail(host, "the Bad Sun copy is gone (%d live, clock %02d:%02d)", n, g_lastHour, g_lastMin);

    if (!BS::ReadActive(first, g_lastWidget) || !BS::ReadIsSender(first, g_lastSender))
        return Fail(host, "the Bad Sun reader did not resolve (clock %02d:%02d)", g_lastHour, g_lastMin);
    UE_LOGI("badsun_drill: [%s] clock %02d:%02d, %d live, widget %d, sender %d", RoleName(host), g_lastHour, g_lastMin,
            n, g_lastWidget ? 1 : 0, g_lastSender ? 1 : 0);
    const bool active = g_lastWidget || g_lastSender;
    if (firstSample && g_lastHour >= kActiveHour)
        return Inconclusive(host, "this peer did not see its clock cross 07:00 (first sample %02d:%02d, widget %d, sender %d)",
                            g_lastHour, g_lastMin, g_lastWidget ? 1 : 0, g_lastSender ? 1 : 0);
    if (!g_t07Seen) {
        if (active)
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
    UE_LOGI("badsun_drill: [client] joined; waiting for the host's clock, then hour %d", kStartHour);
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
    if (g_step == Step::Begin) {
        g_deadline = Steady::now() + kJoinBound;
        g_step = host ? Step::HostPark : Step::WaitJoin;
        UE_LOGI("badsun_drill: [%s] armed -- %s", RoleName(host),
                host ? "the clock to 05:00 at hosting, to 06:30 once a client's join is over, then this peer's own copy"
                     : "once joined, this peer's own copy against its own clock");
    }
    if (Steady::now() > g_deadline) return FailTimeout(host);
    switch (g_step) {
    case Step::HostPark:  HostPark(); return;
    case Step::WaitJoin:
        if (JoinOver(s, host)) BeginArm(host);
        return;
    case Step::WaitStart: WaitStart(host); return;
    case Step::Sample:    Sample(host); return;
    default:              return;
    }
}

void OnDisconnect() {
    g_step = Step::Begin;
    g_slot = -1;
    g_t07Seen = false;
    g_counted = false;
    g_drainFails = 0;
    g_lastHour = g_lastMin = -1;
    g_lastWidget = g_lastSender = false;
}

}  // namespace coop::dev::badsun_drill
