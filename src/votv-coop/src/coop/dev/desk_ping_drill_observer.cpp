// coop/dev/desk_ping_drill_observer.cpp -- the observer half of the desk-ping drill; see coop/dev/desk_ping_drill.h.
// The surface the drill's other TU uses is declared in desk_ping_drill_observer.h.

#include "desk_ping_drill_observer.h"

#include "coop/interactables/desk_ping_sync.h"    // CountsNow: the scan endings this peer took
#include "coop/interactables/device_occupancy.h"  // HolderOf: who holds the desk
#include "coop/player/players_registry.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/core/log.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/desk_ping.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace coop::dev::desk_ping_drill::observer {
namespace {

namespace CD = ue_wrap::console_desk;
namespace DG = ue_wrap::desk_ping;
namespace PS = coop::desk_ping_sync;

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


// The scan's ending was taken since `endedBefore` and the desk reads idle.
bool ObserverEnded(uint32_t endedBefore) {
    const DG::ObserverView& v = g_obsView;
    return PS::CountsNow().mirrorEnded > endedBefore && g_obsViewValid && !v.isPing && v.stage == 0 &&
           v.outer == 0.f && v.inner == 0.f && v.ping == 0.f;
}

}  // namespace

void Reset() {
    g_obsMax = ObserverMax{};
    g_obsLastValid = false;
    g_obsLogMs = 0;
    g_obsWaitMs = 0;
}

void Sample(uint64_t now, const char* who) {
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

bool Passed(uint32_t endedBefore) {
    return g_obsMax.edgesSeen && g_obsMax.innerSeen && ObserverEnded(endedBefore);
}

// The host's verdict rolled: its line waits for the scan's ending, to a bound.
bool Settled(uint64_t now, uint32_t endedBefore) {
    if (!g_obsWaitMs) g_obsWaitMs = now;
    return ObserverEnded(endedBefore) || now - g_obsWaitMs > kObserverEndBoundMs;
}

std::string MaxText(uint32_t endedBefore) {
    char buf[240];
    std::snprintf(buf, sizeof(buf),
                  "observer max: outer %.2f inner %.2f ping %.2f alphaEdges %.2f alphaInner %.2f isPing-ever %d; "
                  "observer %s (edges at stage 0 %d, inner at stage 1 %d, ended %d)",
                  g_obsMax.outer, g_obsMax.inner, g_obsMax.ping, g_obsMax.alphaEdges, g_obsMax.alphaInner,
                  g_obsMax.isPingEver ? 1 : 0, Passed(endedBefore) ? "PASS" : "FAIL",
                  g_obsMax.edgesSeen ? 1 : 0, g_obsMax.innerSeen ? 1 : 0, ObserverEnded(endedBefore) ? 1 : 0);
    return buf;
}

bool SettleStarted() { return g_obsWaitMs != 0; }

namespace {

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
                g_ocOpenStage, *why ? ", " : "", why, MaxText(g_ocEnded).c_str());
    } else {
        const bool ok = Passed(g_ocEnded);
        g_ocPass = g_ocPass && ok;
        UE_LOGI("[DESK-PING-DRILL] observer %s: scan %d%s%s; %s", ok ? "PASS" : "FAIL", g_ocScans + 1,
                *why ? ", " : "", why, MaxText(g_ocEnded).c_str());
        ++g_ocScans;
    }
    g_ocOpen = false;
    g_ocEnded = PS::CountsNow().mirrorEnded;
    g_ocIdleMs = g_ocWaitMs = now;
    Reset();
}

void ObserverClientFail(const char* why, unsigned holder) {
    UE_LOGI("[DESK-PING-DRILL] observer FAIL: %s (holder %u)", why, holder);
    g_ocPass = false;
    ObserverClientDone();
}


}  // namespace

// True for a client that is not the first: the observer, which this tick served.
bool ClientTick() {
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
    Sample(now, "client");
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

void OnDisconnect() {
    g_ocScans = 0;
    g_ocSaid = false;
    g_ocOpen = false;
    g_ocDone = false;
    g_ocPass = true;
    g_ocIdleMs = 0;
    g_ocWaitMs = 0;
    g_ocLate = false;
    g_ocSawHolder = false;
    Reset();
}

}  // namespace coop::dev::desk_ping_drill::observer
