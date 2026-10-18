// coop/dev/desk_ping_drill.cpp -- see coop/dev/desk_ping_drill.h.

#include "coop/dev/desk_ping_drill.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/director/director.h"
#include "coop/dev/director/routes.h"
#include "coop/dev/game_window.h"
#include "coop/interactables/desk_ping_sync.h"     // CountsNow: what the lane did on this peer
#include "coop/interactables/device_occupancy.h"   // LocalHolds: the client entered the coordinates screen
#include "coop/interactables/signal_catch_sync.h"  // LocalCatchesRelayed: a catch the host counted as its own
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/coords_panel.h"
#include "ue_wrap/desk/desk_ping.h"
#include "ue_wrap/desk/desk_press.h"
#include "ue_wrap/desk/device_screen.h"
#include "ue_wrap/desk/space_renderer.h"
#include "ue_wrap/engine/engine_component.h"  // GetComponentLocation
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/profile.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace coop::dev::desk_ping_drill {
namespace {

namespace CD = ue_wrap::console_desk;
namespace CP = ue_wrap::coords_panel;
namespace DG = ue_wrap::desk_ping;
namespace DPR = ue_wrap::desk_press;
namespace DS = ue_wrap::device_screen;
namespace E = ue_wrap::engine;
namespace GW = coop::dev::game_window;
namespace PS = coop::desk_ping_sync;
namespace R = ue_wrap::reflection;
namespace SR = ue_wrap::space_renderer;
namespace sg = ue_wrap::script_gate;

UE_FACT_FN(kRowGatherSignal, "dev.desk_ping_drill", L"spaceRenderer_C", L"gatherSignal", Declared);
UE_FACT_FN(kRowWriteToCoordLog2, "dev.desk_ping_drill", L"analogDScreenTest_C", L"writeToCoordLog_2", Declared);

bool On() {
    static const bool on = coop::config::ResolveString(::coop::config_registry::rows::desk_ping_drill) == "run";
    return on;
}

// An equilateral triangle of circumradius kCircumradius: sides of 346, under the atlas's 740, and an inradius of 100,
// which paces a ping at about 18 s (its stages divide by the inradius over 100, plus 1).
constexpr float kCircumradius = 200.f;
// The least gap between the ping's inner circle and any other sky signal's circle, so the miss finds nothing and the
// catch finds the host's fixture alone.
constexpr float kClearance = 250.f;
// No route ends at the button, on the desk: the client walks to a point of a ring about it that one reaches.
constexpr int   kStandpoints = 6;
constexpr float kStandRingCm = 110.f;
constexpr float kStandReachCm = 60.f;
constexpr int   kWalkDeadlineS = 240;
// Failure bounds only: each step ends on the state it waits for.
constexpr uint64_t kStartBoundMs = 60000;
constexpr uint64_t kEnterBoundMs = 15000;
constexpr uint64_t kPlaceBoundMs = 45000;
constexpr uint64_t kPingStartBoundMs = 5000;
constexpr uint64_t kPingEndBoundMs = 60000;
constexpr uint64_t kVerdictBoundMs = 90000;
constexpr uint64_t kRelayBoundMs = 5000;  // the host's catch detector polls once a second
// A step that retries its reads does so this often, not every tick.
constexpr uint64_t kRetryMs = 500;
constexpr const wchar_t* kFixtureName = L"planeteater";

// ---- the census: this peer's gatherSignal bodies entered and run, and its log lines of a failed ping ----------------

struct Census {
    uint32_t entered = 0;  // gatherSignal bodies entered (a refused one included)
    uint32_t ran = 0;      // gatherSignal bodies run
    uint32_t failed = 0;   // a verdict's "Error [n] Ping failed" lines written into this desk's log
    uint32_t busy = 0;     // the host's refusals of a verdict while its desk was busy, as their line shows them
};
Census g_census;
constexpr int kTagVerdict = 0x44505230;  // 'DPR0'
constexpr int kTagLog = 0x44505231;      // 'DPR1'
bool g_watched[2] = {};
bool g_live = false;
int32_t g_lineOff = -1;  // writeToCoordLog_2's B

sg::Verdict OnVerdictPre(const sg::Call&) {
    ++g_census.entered;
    return sg::Verdict::Run;
}
void OnVerdictPost(const sg::Call&) { ++g_census.ran; }

void OnLogPost(const sg::Call& c) {
    if (g_lineOff < 0) g_lineOff = R::FindParamOffset(c.function, L"B");
    if (g_lineOff < 0 || !c.locals) return;
    const std::wstring line = ue_wrap::field_io::ReadFStringAt(c.locals, g_lineOff);
    if (line.find(L"Ping failed") == std::wstring::npos) return;
    if (line.find(L"Error [") != std::wstring::npos) ++g_census.failed;
    if (line.find(L"already pinging") != std::wstring::npos) ++g_census.busy;
}

bool g_censusDead = false;  // a watch the gate refused or that settled dead: the drill cannot count

bool CensusLive() {
    if (g_live || g_censusDead) return g_live;
    if (!g_watched[0])
        g_watched[0] = sg::WatchName(kRowGatherSignal, kTagVerdict, &OnVerdictPre, &OnVerdictPost);
    if (!g_watched[1]) g_watched[1] = sg::WatchName(kRowWriteToCoordLog2, kTagLog, nullptr, &OnLogPost);
    if (!g_watched[0] || !g_watched[1]) {
        g_censusDead = true;
        return false;
    }
    sg::ResolvePendingNames();
    const bool verdictLive = sg::NameWatchLive(kRowGatherSignal, kTagVerdict);
    const bool logLive = sg::NameWatchLive(kRowWriteToCoordLog2, kTagLog);
    g_live = verdictLive && logLive;
    g_censusDead = (!verdictLive && sg::NameWatchSettled(kRowGatherSignal, kTagVerdict)) ||
                   (!logLive && sg::NameWatchSettled(kRowWriteToCoordLog2, kTagLog));
    return g_live;
}

// ---- the triangle -------------------------------------------------------------------------------------------------

struct Triangle {
    float v[6] = {};  // three vertices, x then y
    float cx = 0.f, cy = 0.f;
};

Triangle TriangleAt(float cx, float cy) {
    Triangle t;
    t.cx = cx;
    t.cy = cy;
    for (int i = 0; i < 3; ++i) {
        const float a = 1.5707963f + 2.0943951f * static_cast<float>(i);  // 90, 210 and 330 degrees
        t.v[2 * i] = cx + kCircumradius * std::cos(a);
        t.v[2 * i + 1] = cy + kCircumradius * std::sin(a);
    }
    return t;
}

// The centre, over a grid of the area, whose inner circle clears every sky signal's circle by the most, apart from a
// centre used before; false when none clears kClearance.
bool PickCentre(bool haveAway, float awayX, float awayY, float& ox, float& oy) {
    float w = 0.f, h = 0.f;
    std::vector<SR::SignalRow> sky;
    if (!CP::ReadAreaSize(w, h) || !SR::ReadSignals(sky)) return false;
    float best = -1e30f;
    for (int i = 1; i < 16; ++i) {
        for (int j = 1; j < 16; ++j) {
            const float x = w * static_cast<float>(i) / 16.f, y = h * static_cast<float>(j) / 16.f;
            if (x < kCircumradius || y < kCircumradius || x > w - kCircumradius || y > h - kCircumradius) continue;
            if (haveAway && std::hypot(x - awayX, y - awayY) < 4.f * kCircumradius) continue;
            float clear = 1e30f;
            for (const SR::SignalRow& s : sky)
                clear = std::fmin(clear, std::hypot(s.x - x, s.y - y) - s.strength * 50.f - kCircumradius / 2.f);
            if (clear > best) {
                best = clear;
                ox = x;
                oy = y;
            }
        }
    }
    return best >= kClearance;
}

bool Commit(const Triangle& t) {
    CP::DishAim aim;
    if (!CP::ReadDishAim(aim)) return false;
    aim.viewX = t.cx;
    aim.viewY = t.cy;
    aim.c0X = t.v[0];
    aim.c0Y = t.v[1];
    aim.c1X = t.v[2];
    aim.c1Y = t.v[3];
    aim.c2X = t.v[4];
    aim.c2Y = t.v[5];
    return CP::WriteCursorOnly(t.cx, t.cy) && CP::WriteDishCommitted(aim);
}

bool SkyHasAt(float x, float y) {
    std::vector<SR::SignalRow> sky;
    if (!SR::ReadSignals(sky)) return false;
    for (const SR::SignalRow& s : sky)
        if (std::fabs(s.x - x) < 1.f && std::fabs(s.y - y) < 1.f) return true;
    return false;
}

// ---- the host -----------------------------------------------------------------------------------------------------

enum class HStep : uint8_t { WaitClient, Miss, Fixture, Catch, Done };
HStep    g_host = HStep::WaitClient;
uint64_t g_hostMs = 0;
uint64_t g_rolledMs = 0;  // the catch's verdict rolled; the relay follows within a detector poll
bool     g_hostPass = true;
Census   g_hostBefore;
PS::Counts g_hostCounts;
uint64_t g_hostCatches = 0;
uint64_t g_hostAttributed = 0;
int32_t  g_hostFound = 0;
CP::DishAim g_missAim;

// ---- the observer: what a peer that does not run the ping sees of it -------------------------------------------------
// The host reads its own desk while a client pings, and so does a second client. The scan is seen when the first
// edge line's alpha passes kAlphaSeen while the stage is 0, and the first inner line's while the stage is 1, with
// coord_isPing clear (the mirror's values, not a machine's own: the host's primed verdict sets the flag). It ends when
// the scan's ending was taken from the pinger and all four values read 0.
constexpr float kAlphaSeen = 0.5f;
constexpr uint64_t kObserverLogMs = 500;
constexpr uint64_t kObserverEndBoundMs = 15000;   // the ending after the host's verdict
constexpr uint64_t kObserverIdleBoundMs = 120000;  // a held desk, and no scan reaching the second client
constexpr uint64_t kObserverNoPingerBoundMs = kStartBoundMs + kWalkDeadlineS * 1000ull + kEnterBoundMs;  // no peer held the desk

struct ObserverMax {
    float outer = 0.f, inner = 0.f, ping = 0.f, alphaEdges = 0.f, alphaInner = 0.f;
    bool isPingEver = false;
    bool edgesSeen = false;  // alphaEdges >= kAlphaSeen at stage 0, isPing clear
    bool innerSeen = false;  // alphaInner >= kAlphaSeen at stage 1, isPing clear
};
ObserverMax g_obsMax;
DG::ObserverView g_obsLast;   // the last view that was logged
DG::ObserverView g_obsView;   // the latest view read
bool     g_obsViewValid = false;
bool     g_obsLastValid = false;
bool     g_obsReadFailLogged = false;
uint64_t g_obsLogMs = 0;
uint64_t g_obsWaitMs = 0;  // the host waits for the scan's ending since this

void ObserverReset() {
    g_obsMax = ObserverMax{};
    g_obsLastValid = false;
    g_obsLogMs = 0;
    g_obsWaitMs = 0;
}

void ObserverSample(uint64_t now, const char* who) {
    DG::ObserverView v;
    g_obsViewValid = DG::ReadObserverView(CD::Instance(), v);
    if (!g_obsViewValid) {
        if (!g_obsReadFailLogged) UE_LOGW("[DESK-PING-DRILL] %s observer view does not read", who);
        g_obsReadFailLogged = true;
        return;
    }
    g_obsView = v;
    g_obsMax.outer = std::fmax(g_obsMax.outer, v.outer);
    g_obsMax.inner = std::fmax(g_obsMax.inner, v.inner);
    g_obsMax.ping = std::fmax(g_obsMax.ping, v.ping);
    g_obsMax.alphaEdges = std::fmax(g_obsMax.alphaEdges, v.alphaEdges);
    g_obsMax.alphaInner = std::fmax(g_obsMax.alphaInner, v.alphaInner);
    g_obsMax.isPingEver = g_obsMax.isPingEver || v.isPing;
    if (!v.isPing && v.stage == 0 && v.alphaEdges >= kAlphaSeen) g_obsMax.edgesSeen = true;
    if (!v.isPing && v.stage == 1 && v.alphaInner >= kAlphaSeen) g_obsMax.innerSeen = true;
    const bool changed = !g_obsLastValid || v.isPing != g_obsLast.isPing || v.stage != g_obsLast.stage ||
                         v.outer != g_obsLast.outer || v.inner != g_obsLast.inner || v.ping != g_obsLast.ping ||
                         v.alphaEdges != g_obsLast.alphaEdges || v.alphaInner != g_obsLast.alphaInner;
    if (!changed || (g_obsLastValid && now - g_obsLogMs < kObserverLogMs)) return;
    UE_LOGI("[DESK-PING-DRILL] %s observer sample: isPing %d stage %d outer %.2f inner %.2f ping %.2f alphaEdges "
            "%.2f alphaInner %.2f",
            who, v.isPing ? 1 : 0, v.stage, v.outer, v.inner, v.ping, v.alphaEdges, v.alphaInner);
    g_obsLast = v;
    g_obsLastValid = true;
    g_obsLogMs = now;
}

// The scan's ending was taken since `endedBefore` and the desk reads idle.
bool ObserverEnded(uint32_t endedBefore) {
    const DG::ObserverView& v = g_obsView;
    return PS::CountsNow().mirrorEnded > endedBefore && g_obsViewValid && !v.isPing && v.stage == 0 &&
           v.outer == 0.f && v.inner == 0.f && v.ping == 0.f;
}

bool ObserverPassed(uint32_t endedBefore) {
    return g_obsMax.edgesSeen && g_obsMax.innerSeen && ObserverEnded(endedBefore);
}

// The host's verdict rolled: its line waits for the scan's ending, to a bound.
bool ObserverSettled(uint64_t now, uint32_t endedBefore) {
    if (!g_obsWaitMs) g_obsWaitMs = now;
    return ObserverEnded(endedBefore) || now - g_obsWaitMs > kObserverEndBoundMs;
}

std::string ObserverMaxText(uint32_t endedBefore) {
    char buf[240];
    std::snprintf(buf, sizeof(buf),
                  "observer max: outer %.2f inner %.2f ping %.2f alphaEdges %.2f alphaInner %.2f isPing-ever %d; "
                  "observer %s (edges at stage 0 %d, inner at stage 1 %d, ended %d)",
                  g_obsMax.outer, g_obsMax.inner, g_obsMax.ping, g_obsMax.alphaEdges, g_obsMax.alphaInner,
                  g_obsMax.isPingEver ? 1 : 0, ObserverPassed(endedBefore) ? "PASS" : "FAIL",
                  g_obsMax.edgesSeen ? 1 : 0, g_obsMax.innerSeen ? 1 : 0, ObserverEnded(endedBefore) ? 1 : 0);
    return buf;
}

void HostGo(HStep s) {
    g_host = s;
    g_hostMs = ::GetTickCount64();
    g_hostBefore = g_census;
    g_hostCounts = PS::CountsNow();
    if (s == HStep::Miss || s == HStep::Catch) ObserverReset();
}

void HostAbandon(const char* why) {
    UE_LOGW("[DESK-PING-DRILL] host ABANDONED: %s", why);
    g_host = HStep::Done;
}

void HostTick(coop::net::Session* s) {
    const uint64_t now = ::GetTickCount64();
    const PS::Counts c = PS::CountsNow();
    if (g_host == HStep::Miss || g_host == HStep::Catch) ObserverSample(now, "host");
    switch (g_host) {
    case HStep::WaitClient:
        if (!s->AnyWorldReadyPeer() || !CensusLive()) {
            if (g_censusDead) { HostAbandon("the census watches did not go live"); return; }
            if (!g_hostMs) g_hostMs = now;
            if (now - g_hostMs > kStartBoundMs * 4) HostAbandon("no client's world became ready");
            return;
        }
        UE_LOGI("[DESK-PING-DRILL] host: the census is live");
        HostGo(HStep::Miss);
        return;
    case HStep::Miss: {
        if (c.rolled == g_hostCounts.rolled) return;  // the client's legs carry their own bounds
        // The aim stays the miss's until the client's next commit, which follows this ping's ending.
        if (!g_obsWaitMs) CP::ReadDishAim(g_missAim);
        if (!ObserverSettled(now, g_hostCounts.mirrorEnded)) return;
        const bool ok = c.primed == g_hostCounts.primed + 1 && c.caught == g_hostCounts.caught &&
                        g_census.ran == g_hostBefore.ran + 1 && ObserverPassed(g_hostCounts.mirrorEnded);
        g_hostPass = g_hostPass && ok;
        UE_LOGI("[DESK-PING-DRILL] host miss %s: primed %u, rolled %u, caught %u; its gatherSignal ran %u; %s",
                ok ? "PASS" : "FAIL", c.primed - g_hostCounts.primed, c.rolled - g_hostCounts.rolled,
                c.caught - g_hostCounts.caught, g_census.ran - g_hostBefore.ran,
                ObserverMaxText(g_hostCounts.mirrorEnded).c_str());
        HostGo(HStep::Fixture);
        return;
    }
    case HStep::Fixture: {
        // The catch's triangle, once the client's aim has moved from the miss's: a sky signal at its centre whose
        // circle is the ping's inner circle, so the verdict's overlap is whole.
        CP::DishAim aim;
        if (!CP::ReadDishAim(aim) || (aim.c0X == g_missAim.c0X && aim.c0Y == g_missAim.c0Y)) return;
        const float cx = (aim.c0X + aim.c1X + aim.c2X) / 3.f, cy = (aim.c0Y + aim.c1Y + aim.c2Y) / 3.f;
        std::vector<SR::SignalRow> sky;
        if (!SR::ReadSignals(sky)) { HostAbandon("the host's sky does not read"); return; }
        SR::SignalRow fix;
        fix.x = cx;
        fix.y = cy;
        fix.strength = (kCircumradius / 2.f) / 50.f;
        fix.frequency = 7.25f;
        fix.frequencySpread = 0.5f;
        fix.polaritySpread = 0.5f;
        fix.lifeTime = fix.maxLifetime = 200.f;
        fix.objectName = kFixtureName;
        sky.push_back(fix);
        SR::ApplyStats st;
        if (!SR::ApplySignalSet(sky, st) || st.added < 1) { HostAbandon("the fixture signal was not added"); return; }
        UE_LOGI("[DESK-PING-DRILL] host put '%ls' at (%.0f,%.0f) under the catch's triangle", kFixtureName, cx, cy);
        g_hostCatches = coop::signal_catch_sync::LocalCatchesRelayed();
        g_hostAttributed = coop::signal_catch_sync::AttributedCatchesRelayed();
        ue_wrap::profile::ReadSignalsFound(g_hostFound);
        g_rolledMs = 0;
        HostGo(HStep::Catch);
        return;
    }
    case HStep::Catch: {
        // The catch's verdict, the busy intent the client sends behind it, and the catch's relay as the client's,
        // which the detector makes at its next poll.
        if (c.rolled == g_hostCounts.rolled) return;
        if (!g_rolledMs) g_rolledMs = now;
        const uint64_t attributed = coop::signal_catch_sync::AttributedCatchesRelayed() - g_hostAttributed;
        if ((c.busy == g_hostCounts.busy || attributed == 0) && now - g_rolledMs <= kRelayBoundMs) return;
        if (!ObserverSettled(now, g_hostCounts.mirrorEnded)) return;
        int32_t found = 0;
        const bool foundRead = ue_wrap::profile::ReadSignalsFound(found);
        const uint64_t own = coop::signal_catch_sync::LocalCatchesRelayed() - g_hostCatches;
        const bool ok = c.primed == g_hostCounts.primed + 1 && c.caught == g_hostCounts.caught + 1 &&
                        g_census.ran == g_hostBefore.ran + 1 && foundRead && found == g_hostFound &&
                        attributed == 1 && own == 0 && c.busy == g_hostCounts.busy + 1 &&
                        ObserverPassed(g_hostCounts.mirrorEnded);
        g_hostPass = g_hostPass && ok;
        UE_LOGI("[DESK-PING-DRILL] host catch %s: primed %u, rolled %u, caught %u; its gatherSignal ran %u; its own "
                "finds %d -> %d; catches relayed as the client's %llu, as its own %llu; busy refusals %u; %s",
                ok ? "PASS" : "FAIL", c.primed - g_hostCounts.primed, c.rolled - g_hostCounts.rolled,
                c.caught - g_hostCounts.caught, g_census.ran - g_hostBefore.ran, g_hostFound, found,
                static_cast<unsigned long long>(attributed), static_cast<unsigned long long>(own),
                c.busy - g_hostCounts.busy, ObserverMaxText(g_hostCounts.mirrorEnded).c_str());
        UE_LOGI("[DESK-PING-DRILL] host DONE %s", g_hostPass ? "PASS" : "FAIL");
        g_host = HStep::Done;
        return;
    }
    case HStep::Done:
        return;
    }
}

// ---- the client ---------------------------------------------------------------------------------------------------

enum class CStep : uint8_t { Ready, Walk, Enter, Place, Settle, Pinging, Verdict, Leave, Done };
CStep    g_client = CStep::Ready;
int      g_leg = 0;  // 0 the miss, 1 the catch
uint64_t g_stepMs = 0;
uint64_t g_readyMs = 0;
uint64_t g_tryMs = 0;  // the last try of a step that retries its reads
bool     g_pass = true;
bool     g_started = false;
unsigned g_vk = 0;
std::vector<ue_wrap::FVector> g_ring;  // the standpoints about button_coords a route reaches, shortest first
size_t   g_stand = 0;
bool     g_ringBuilt = false;
Triangle g_tri;
Triangle g_missTri;
Census   g_before;
PS::Counts g_counts;
int32_t  g_found = 0;
std::shared_ptr<coop::director::BackgroundWalk> g_walk;
bool     g_left = false;  // the Leave step has made its exit

void Go(CStep s) {
    g_client = s;
    g_stepMs = ::GetTickCount64();
}

void Abandon(const char* why) {
    UE_LOGW("[DESK-PING-DRILL] ABANDONED: %s", why);
    g_client = CStep::Done;
}

const char* LegName() { return g_leg == 0 ? "miss" : "catch"; }

void ClientTick(coop::net::Session* s) {
    const uint64_t now = ::GetTickCount64();
    void* desk = CD::EnsureResolved() ? CD::Instance() : nullptr;
    void* player = coop::players::Registry::Get().Local();
    switch (g_client) {
    case CStep::Ready: {
        if (!coop::net_pump::HasAnnouncedWorldReady()) return;
        if (!g_readyMs) g_readyMs = now;
        const bool settled = coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle;
        if (!settled || !CensusLive() || !desk || !player) {
            if (g_censusDead)
                Abandon("the census watches did not go live");
            else if (now - g_readyMs > kStartBoundMs)
                Abandon("the join, the census, the desk or the player never came");
            return;
        }
        UE_LOGI("[DESK-PING-DRILL] client: the census is live");
        Go(CStep::Walk);
        return;
    }
    case CStep::Walk: {
        void* button = desk ? DPR::Member(desk, L"button_coords") : nullptr;
        if (!button) { Abandon("the desk's button_coords does not read"); return; }
        if (!g_walk) {
            if (!g_ringBuilt) {
                g_ring = coop::director::ReachableStandpoints(player, E::GetComponentLocation(button), kStandRingCm,
                                                              kStandpoints, kStandReachCm);
                g_ringBuilt = true;
                UE_LOGI("[DESK-PING-DRILL] client: a route reaches %zu of the %d standpoints about button_coords",
                        g_ring.size(), kStandpoints);
            }
            if (g_stand >= g_ring.size()) { Abandon("no standpoint about button_coords could be walked to"); return; }
            g_walk = coop::director::StartBackgroundWalk(g_ring[g_stand], kStandReachCm, kWalkDeadlineS);
            return;
        }
        const int w = g_walk->state.load();
        if (w == 0) return;
        g_walk.reset();
        if (w == 2) {
            UE_LOGI("[DESK-PING-DRILL] client: standpoint %zu of %zu could not be walked to", g_stand + 1,
                    g_ring.size());
            ++g_stand;
            return;
        }
        if (!DPR::Press(desk, player, button)) { Abandon("the press on button_coords did not dispatch"); return; }
        Go(CStep::Enter);
        return;
    }
    case CStep::Enter: {
        if (!coop::device_occupancy::LocalHolds(L"desk")) {
            if (now - g_stepMs > kEnterBoundMs) Abandon("the press never put this player on the coordinates screen");
            return;
        }
        std::wstring key;
        if (!ue_wrap::profile::KeybindDisplayName(L"coord_ping", key)) {
            Abandon("coord_ping's key does not read");
            return;
        }
        g_vk = GW::VirtualKeyOf(key);
        if (!g_vk) {
            UE_LOGW("[DESK-PING-DRILL] client: coord_ping is bound to '%ls', which this drill cannot press",
                    key.c_str());
            Abandon("coord_ping's key has no virtual key here");
            return;
        }
        UE_LOGI("[DESK-PING-DRILL] client is on the coordinates screen; coord_ping is '%ls'", key.c_str());
        Go(CStep::Place);
        return;
    }
    case CStep::Place: {
        if (now - g_tryMs < kRetryMs) return;
        g_tryMs = now;
        float cx = 0.f, cy = 0.f;
        if (!PickCentre(g_leg == 1, g_missTri.cx, g_missTri.cy, cx, cy)) {
            if (now - g_stepMs > kPlaceBoundMs) Abandon("no centre clears every sky signal");
            return;
        }
        g_tri = TriangleAt(cx, cy);
        if (g_leg == 0) g_missTri = g_tri;
        if (!Commit(g_tri)) { Abandon("the triangle did not commit"); return; }
        UE_LOGI("[DESK-PING-DRILL] client committed the %s's triangle about (%.0f,%.0f)", LegName(), cx, cy);
        Go(CStep::Settle);
        return;
    }
    case CStep::Settle: {
        if (now - g_tryMs < kRetryMs) return;
        g_tryMs = now;
        bool canPing = false;
        const bool ready = CP::ReadCanPing(canPing) && canPing && (g_leg == 0 || SkyHasAt(g_tri.cx, g_tri.cy));
        if (!ready) {
            if (now - g_stepMs > kPlaceBoundMs)
                Abandon(g_leg == 0 ? "the panel's markers never reached the triangle"
                                   : "the host's fixture never reached this sky");
            return;
        }
        g_before = g_census;
        g_counts = PS::CountsNow();
        ue_wrap::profile::ReadSignalsFound(g_found);
        if (!GW::PostKeyPress(g_vk)) { Abandon("the key could not be posted to the game window"); return; }
        g_started = false;
        Go(CStep::Pinging);
        return;
    }
    case CStep::Pinging: {
        bool pinging = false;
        DG::ReadPinging(desk, pinging);
        if (!g_started) {
            if (pinging) { g_started = true; return; }
            if (now - g_stepMs > kPingStartBoundMs) Abandon("the key did not start a ping");
            return;
        }
        if (g_leg == 1) {
            // The busy leg goes out behind this ping's own verdict, so it finds the host's desk holding that verdict,
            // queued or rolling, whatever the host's timing.
            if (PS::CountsNow().refused == g_counts.refused) {
                if (now - g_stepMs > kPingEndBoundMs) Abandon("the ping never reached its verdict");
                return;
            }
            CP::DishAim aim;
            CP::ReadDishAim(aim);
            coop::net::DeskPingVerdictPayload p{};
            p.op = coop::net::desk_ping::kOpIntent;
            p.seq = 0xB0B0u;
            p.viewX = aim.viewX;
            p.viewY = aim.viewY;
            p.aim.c0X = aim.c0X;
            p.aim.c0Y = aim.c0Y;
            p.aim.c1X = aim.c1X;
            p.aim.c1Y = aim.c1Y;
            p.aim.c2X = aim.c2X;
            p.aim.c2Y = aim.c2Y;
            s->SendReliableToSlot(0, coop::net::ReliableKind::DeskPingVerdict, &p, static_cast<int>(sizeof(p)));
            UE_LOGI("[DESK-PING-DRILL] client sent a second verdict behind its catch's");
            Go(CStep::Verdict);
            return;
        }
        // The miss waits for this run's end: the catch's key starts a ping only on an idle machine.
        if (pinging) {
            if (now - g_stepMs > kPingEndBoundMs) Abandon("the ping never ended");
            return;
        }
        Go(CStep::Verdict);
        return;
    }
    case CStep::Verdict: {
        const PS::Counts c = PS::CountsNow();
        if (g_leg == 0) {
            if (g_census.failed == g_before.failed && now - g_stepMs <= kVerdictBoundMs) return;
            const bool ok = g_census.entered == g_before.entered + 1 && g_census.ran == g_before.ran &&
                            c.refused == g_counts.refused + 1 && c.outputsHeld > g_counts.outputsHeld &&
                            g_census.failed == g_before.failed + 1;
            UE_LOGI("[DESK-PING-DRILL] client miss %s: its gatherSignal entered %u and ran %u, refused %u, held %u; "
                    "the verdict's failure lines in its log %u", ok ? "PASS" : "FAIL",
                    g_census.entered - g_before.entered, g_census.ran - g_before.ran, c.refused - g_counts.refused,
                    c.outputsHeld - g_counts.outputsHeld, g_census.failed - g_before.failed);
            g_pass = g_pass && ok;
            g_leg = 1;
            Go(CStep::Place);
            return;
        }
        // The find, the busy refusal and the catch itself arrive on their own lanes, each in its time; the desk is read
        // once the two counts are in.
        const bool counted = c.finds > g_counts.finds && c.answered > g_counts.answered;
        CD::CoordSignal sig;
        const bool holds = counted && CD::ReadCoordSignal(sig) && sig.objectName == kFixtureName;
        if (!holds && now - g_stepMs <= kVerdictBoundMs) return;
        if (!counted) CD::ReadCoordSignal(sig);
        int32_t found = 0;
        const bool ok = g_census.entered == g_before.entered + 1 && g_census.ran == g_before.ran &&
                        c.refused == g_counts.refused + 1 && c.finds == g_counts.finds + 1 &&
                        ue_wrap::profile::ReadSignalsFound(found) && found == g_found + 1 && holds;
        UE_LOGI("[DESK-PING-DRILL] client catch %s: its gatherSignal entered %u and ran %u, refused %u; finds %u, its "
                "own count %d -> %d; its desk holds '%ls'", ok ? "PASS" : "FAIL", g_census.entered - g_before.entered,
                g_census.ran - g_before.ran, c.refused - g_counts.refused, c.finds - g_counts.finds, g_found, found,
                sig.objectName.empty() ? L"nothing" : sig.objectName.c_str());
        const bool busyOk = c.answered == g_counts.answered + 1 && g_census.busy == g_before.busy + 1;
        UE_LOGI("[DESK-PING-DRILL] client busy %s: refusals shown %u, busy lines %u", busyOk ? "PASS" : "FAIL",
                c.answered - g_counts.answered, g_census.busy - g_before.busy);
        g_pass = g_pass && ok && busyOk;
        UE_LOGI("[DESK-PING-DRILL] client DONE %s", g_pass ? "PASS" : "FAIL");
        g_left = false;
        Go(CStep::Leave);
        return;
    }
    case CStep::Leave: {
        // The pinger gives the desk back by the game's own forced exit from an interface, as the other drills leave a
        // screen, so a second client can tell the scans are over.
        if (!g_left) {
            g_left = true;
            if (!DS::ForceExitInterface(player)) {
                UE_LOGW("[DESK-PING-DRILL] client could not leave the coordinates screen");
                g_client = CStep::Done;
            }
            return;
        }
        if (coop::device_occupancy::LocalHolds(L"desk") || coop::device_occupancy::HolderOf(L"desk") != 0xFF) {
            if (now - g_stepMs > kEnterBoundMs) {
                UE_LOGW("[DESK-PING-DRILL] client still holds the desk after leaving the coordinates screen");
                g_client = CStep::Done;
            }
            return;
        }
        UE_LOGI("[DESK-PING-DRILL] client left the desk");
        g_client = CStep::Done;
        return;
    }
    case CStep::Done:
        return;
    }
}

// ---- the second client ----------------------------------------------------------------------------------------------
// It pings nothing. It reads the first client's scans on its own desk, which reach it only through the host's relay
// (the pinger named in each packet), and says whether each one it saw from the start was seen and ended. It is done
// when the pinger gives the desk back.

int      g_ocScans = 0;
bool     g_ocSaid = false;
bool     g_ocOpen = false;
bool     g_ocDone = false;
bool     g_ocPass = true;
uint64_t g_ocOpenMs = 0;
uint64_t g_ocIdleMs = 0;
uint32_t g_ocEnded = 0;  // scan endings taken when the last scan closed
uint64_t g_ocWaitMs = 0;  // since this peer was ready, or the last scan closed
bool     g_ocLate = false;  // the open scan was first seen past its start
bool     g_ocSawHolder = false;  // another peer held the desk after this one was ready
int32_t  g_ocOpenStage = 0;

void ObserverClientDone() {
    UE_LOGI("[DESK-PING-DRILL] observer DONE %s", g_ocPass ? "PASS" : "FAIL");
    g_ocDone = true;
}

// A scan this client first saw past its start (it joined mid-scan, principle 8's own row) is said and not judged: its
// first stages were never on this desk, so it counts only from a stage-0 start.
void ObserverClientClose(uint64_t now, const char* why) {
    if (g_ocLate) {
        UE_LOGI("[DESK-PING-DRILL] observer late join: a scan first seen at stage %d is not judged%s%s; %s",
                g_ocOpenStage, *why ? ", " : "", why, ObserverMaxText(g_ocEnded).c_str());
    } else {
        const bool ok = ObserverPassed(g_ocEnded);
        g_ocPass = g_ocPass && ok;
        UE_LOGI("[DESK-PING-DRILL] observer %s: scan %d%s%s; %s", ok ? "PASS" : "FAIL", g_ocScans + 1,
                *why ? ", " : "", why, ObserverMaxText(g_ocEnded).c_str());
        ++g_ocScans;
    }
    g_ocOpen = false;
    g_ocEnded = PS::CountsNow().mirrorEnded;
    g_ocIdleMs = g_ocWaitMs = now;
    ObserverReset();
}

void ObserverClientFail(const char* why, unsigned holder) {
    UE_LOGI("[DESK-PING-DRILL] observer FAIL: %s (holder %u)", why, holder);
    g_ocPass = false;
    ObserverClientDone();
}

// True for a client that is not the first: the observer, which this tick served.
bool ObserverClientTick() {
    const uint8_t mine = coop::players::Registry::Get().LocalPeerId();
    if (mine <= 1) return false;
    if (g_ocDone || !coop::net_pump::HasAnnouncedWorldReady()) return true;
    const uint64_t now = ::GetTickCount64();
    if (!g_ocSaid) {
        g_ocSaid = true;
        g_ocEnded = PS::CountsNow().mirrorEnded;
        g_ocIdleMs = g_ocWaitMs = now;
        UE_LOGI("[DESK-PING-DRILL] client in slot %u: an observer, it pings nothing and reads the first client's scans",
                mine);
    }
    ObserverSample(now, "client");
    if (!g_ocOpen) {
        const DG::ObserverView& v = g_obsView;
        if (g_obsViewValid && !v.isPing && (v.stage > 0 || v.outer > 0.f)) {
            g_ocOpen = true;
            g_ocOpenMs = now;
            g_ocOpenStage = v.stage;
            g_ocLate = v.stage > 0;
            return true;
        }
        // The run ends when the pinger gives the desk back. Another peer holding it with no scan reaching this one, and
        // no peer ever holding it, each have a bound.
        const uint8_t holder = coop::device_occupancy::HolderOf(L"desk");
        if (holder != 0xFF && holder != mine) {
            g_ocSawHolder = true;
            if (now - g_ocIdleMs > kObserverIdleBoundMs)
                ObserverClientFail("a peer holds the desk and no scan reached this client", holder);
            return true;
        }
        g_ocIdleMs = now;
        if (g_ocSawHolder && holder == 0xFF) {
            if (g_ocScans == 0)
                ObserverClientFail("the pinger left the desk and no scan was judged from a stage-0 start", holder);
            else if (!g_ocPass)
                ObserverClientFail("the pinger left the desk after a judged scan failed", holder);
            else
                ObserverClientDone();
        } else if (!g_ocSawHolder && now - g_ocWaitMs > kObserverNoPingerBoundMs) {
            ObserverClientFail("no peer held the desk", holder);
        }
        return true;
    }
    if (ObserverEnded(g_ocEnded))
        ObserverClientClose(now, "");
    else if (now - g_ocOpenMs > kVerdictBoundMs)
        ObserverClientClose(now, "its ending never came");
    return true;
}

}  // namespace

void Tick(coop::net::Session* session) {
    if (!On() || !session || !session->running()) return;
    if (session->role() == coop::net::Role::Host) {
        HostTick(session);
        return;
    }
    if (session->connected() && !ObserverClientTick()) ClientTick(session);
}

void OnDisconnect() {
    g_host = HStep::WaitClient;
    g_hostMs = 0;
    g_rolledMs = 0;
    g_hostPass = true;
    g_client = CStep::Ready;
    g_leg = 0;
    g_readyMs = 0;
    g_tryMs = 0;
    g_pass = true;
    g_walk.reset();
    g_ring.clear();
    g_stand = 0;
    g_ringBuilt = false;
    g_ocScans = 0;
    g_ocSaid = false;
    g_ocOpen = false;
    g_ocDone = false;
    g_ocPass = true;
    g_ocIdleMs = 0;
    g_ocWaitMs = 0;
    g_ocLate = false;
    g_ocSawHolder = false;
    g_left = false;
    ObserverReset();
}

}  // namespace coop::dev::desk_ping_drill
