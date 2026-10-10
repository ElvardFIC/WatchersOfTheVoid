// coop/dev/desk_crossing_drill.cpp -- see coop/dev/desk_crossing_drill.h.

#include "coop/dev/desk_crossing_drill.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/interactables/desk_sim_sync.h"      // HostCrossings, ClientPostSteps
#include "coop/interactables/signal_catch_sync.h"  // LocalCatchesRelayed: the caught signal is on the wire
#include "coop/net/session.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/desk_detector.h"
#include "ue_wrap/desk/saved_signals.h"
#include "ue_wrap/desk/signal_dynamic.h"
#include "ue_wrap/facts/function_row.h"

#include <windows.h>

#include <cstdint>
#include <string>

namespace coop::dev::desk_crossing_drill {
namespace {

namespace CD = ue_wrap::console_desk;
namespace DD = ue_wrap::desk_detector;
namespace SD = ue_wrap::signal_dynamic;
namespace SS = ue_wrap::saved_signals;
namespace sg = ue_wrap::script_gate;

enum class Mode : uint8_t { Off, Run, Join };
Mode ModeOf() {
    static const Mode m = [] {
        const std::string v = coop::config::ResolveString(::coop::config_registry::rows::desk_crossing_drill);
        return v == "run" ? Mode::Run : v == "join" ? Mode::Join : Mode::Off;
    }();
    return m;
}

// The drill's caught signals sit at coordinates no sky signal reaches, apart from the save-family drill's, so a
// row the looker_behind leg saves is known by them.
constexpr float kBandX = 914000.f;
constexpr float kBandY = 917000.f;
// 0: planeteater's, the one list_objects row whose object renderer spawns a signal object actor, so its
// fullyProcessed can be read; 1: looker_behind's, whose crossing saves.
constexpr int   kLegs = 2;
// The needle held below 1 by less than the client's smallest step (0.001 times its multiplier, 0.9 or 1.0).
constexpr float kHoldGap = 0.0005f;
constexpr uint32_t kHoldCycles = 3;  // the host's own play cycles to pass while it holds
// The host's own play cycles to pass after a crossing before the next leg's catch: the crossing's snapshots reach the
// client, at the stream's 10 Hz, before a new caught signal does. A play cycle runs whether or not it steps.
constexpr uint32_t kSettleCycles = 2;
// Failure bounds only: each step ends on the state it waits for.
constexpr uint64_t kStartBoundMs = 60000;
constexpr uint64_t kArmBoundMs = 30000;
constexpr uint64_t kHoldBoundMs = 90000;
constexpr uint64_t kCrossBoundMs = 90000;
constexpr uint64_t kSaveBoundMs = 30000;

// ---- the census: the loop resumes whose step crossed, the game's step cycles, autoSave entered and run ----

UE_FACT_FN(kRowExecuteUbergraphAnalogDScreenTest, "dev.desk_crossing_drill", L"analogDScreenTest_C",
           L"ExecuteUbergraph_analogDScreenTest", Declared);
UE_FACT_FN(kRowSetFullyProcessedSignalObject, "dev.desk_crossing_drill", L"analogDScreenTest_C",
           L"setFullyProcessedSignalObject", Declared);
UE_FACT_FN(kRowAutoSave, "dev.desk_crossing_drill", L"analogDScreenTest_C", L"autoSave", Declared);
UE_FACT_FN(kRowDownloadPlaySignall, "dev.desk_crossing_drill", L"analogDScreenTest_C", L"download_playSignall",
           Declared);
constexpr int kTagUber = 0x44434430;   // 'DCD0'
constexpr int kTagCycle = 0x44434431;  // 'DCD1'
constexpr int kTagSave = 0x44434432;   // 'DCD2'
constexpr int kTagPlay = 0x44434433;   // 'DCD3'
struct Census {
    uint32_t crossed = 0;      // loop resumes (entry 3541) whose step took the needle from below 1 to 1 or above
    uint32_t cycles = 0;       // setFullyProcessedSignalObject run by the game's own loop: a cycle that stepped
    uint32_t plays = 0;        // download_playSignall run by the game's own loop: every play cycle, stepping or not
    uint32_t autoSaveIn = 0;   // autoSave entered
    uint32_t autoSaveRan = 0;  // autoSave bodies run: a refused one enters and does not run
};
Census g_census;
bool g_watched[4] = {};
struct Before {
    float needle = 0.f;
    bool  loop = false;
};
Before g_depth[32];  // a loop resume's needle, carried from pre to post by the gate's depth

sg::Verdict OnUberPre(const sg::Call& c) {
    if (c.depth <= 0 || c.depth >= 32) return sg::Verdict::Run;
    Before& b = g_depth[c.depth];
    int32_t entry = -1;
    b.loop = DD::ReadEntry(c.function, c.locals, entry) && entry == DD::kLoopEntry && DD::ReadNeedle(c.object, b.needle);
    return sg::Verdict::Run;
}

void OnUberPost(const sg::Call& c) {
    if (c.depth <= 0 || c.depth >= 32 || !g_depth[c.depth].loop) return;
    g_depth[c.depth].loop = false;
    float after = 0.f;
    if (g_depth[c.depth].needle < 1.f && DD::ReadNeedle(c.object, after) && after >= 1.f) ++g_census.crossed;
}

sg::Verdict OnCyclePre(const sg::Call& c) {
    if (!c.fromOurCode) ++g_census.cycles;
    return sg::Verdict::Run;
}

sg::Verdict OnPlayPre(const sg::Call& c) {
    if (!c.fromOurCode) ++g_census.plays;
    return sg::Verdict::Run;
}

sg::Verdict OnSavePre(const sg::Call&) {
    ++g_census.autoSaveIn;
    return sg::Verdict::Run;
}

void OnSavePost(const sg::Call&) { ++g_census.autoSaveRan; }

// Every census watch live: null; else the first function that is not.
const wchar_t* CensusNotLive() {
    const ue_wrap::facts::Function* const rows[4] = {&kRowExecuteUbergraphAnalogDScreenTest,
                                                     &kRowSetFullyProcessedSignalObject, &kRowAutoSave,
                                                     &kRowDownloadPlaySignall};
    if (!g_watched[0]) g_watched[0] = sg::WatchName(*rows[0], kTagUber, &OnUberPre, &OnUberPost);
    if (!g_watched[1]) g_watched[1] = sg::WatchName(*rows[1], kTagCycle, &OnCyclePre, nullptr);
    if (!g_watched[2]) g_watched[2] = sg::WatchName(*rows[2], kTagSave, &OnSavePre, &OnSavePost);
    if (!g_watched[3]) g_watched[3] = sg::WatchName(*rows[3], kTagPlay, &OnPlayPre, nullptr);
    sg::ResolvePendingNames();
    const int tags[4] = {kTagUber, kTagCycle, kTagSave, kTagPlay};
    for (int i = 0; i < 4; ++i)
        if (!g_watched[i] || !sg::NameWatchLive(*rows[i], tags[i])) return rows[i]->Name();
    return nullptr;
}

// ---- what a peer's desk holds, and the host's fixtures --------------------------------------------------------

bool InBand(const SD::Row& r) { return r.locY == kBandY && r.locX >= kBandX && r.locX < kBandX + 16.f; }

int BandRows() {
    int n = 0;
    const int32_t count = SS::Count();
    for (int32_t i = 0; i < count; ++i) {
        SD::Row r;
        if (SS::ReadRow(i, r) && InBand(r)) ++n;
    }
    return n;
}

int PurgeBand() {
    int purged = 0;
    for (bool found = true; found;) {
        found = false;
        const int32_t count = SS::Count();
        for (int32_t i = count - 1; i >= 0; --i) {
            SD::Row r;
            if (!SS::ReadRow(i, r) || !InBand(r)) continue;
            if (!SS::DeleteSignal(i)) return purged;
            ++purged;
            found = true;
            break;
        }
    }
    return purged;
}

bool WriteCaught(int leg) {
    CD::CoordSignal sig;
    sig.x = kBandX + static_cast<float>(leg);
    sig.y = kBandY;
    sig.strength = 1.f;
    sig.frequency = 1.f + static_cast<float>(leg);
    sig.frequencySpread = 0.5f;
    sig.polaritySpread = 0.5f;
    sig.objectName = leg == 1 ? L"looker_behind" : L"planeteater";
    return CD::WriteCoordSignal(sig);
}

struct PeerState {
    bool canDL = false, canRead = false, fully = false, fullyRead = false;
};
PeerState ReadState(void* desk) {
    PeerState p;
    p.canRead = DD::ReadCanDL(desk, p.canDL);
    p.fullyRead = DD::ReadFullyProcessed(desk, p.fully);
    return p;
}
const char* Yes(bool read, bool v) { return !read ? "unread" : v ? "yes" : "NO"; }

// ---- the host ---------------------------------------------------------------------------------------------------

enum class HStep : uint8_t { WaitClient, Census, Arm, Relayed, Hold, WaitCross, WaitSave, Settle, Done };
HStep    g_host = HStep::WaitClient;
int      g_hostLeg = 0;
uint64_t g_hostMs = 0;
bool     g_hostPass = true;
uint64_t g_catches = 0;
float    g_heldMultiplier = 0.f;
bool     g_holding = false;
Census   g_hostBefore;
uint32_t g_crossingsBefore = 0;
PeerState g_hostAtCross;  // read as the crossing lands, before looker_behind's save resets the download

void HostGo(HStep s) {
    g_host = s;
    g_hostMs = ::GetTickCount64();
}

void HostAbandon(const char* why) {
    UE_LOGW("[DESK-CROSSING-DRILL] ABANDONED on the host: %s", why);
    g_host = HStep::Done;
}

void LetGo(void* desk) {
    if (!g_holding) return;
    g_holding = false;
    if (desk) DD::WriteMultiplier(desk, g_heldMultiplier);
}

void HostJudge() {
    const uint32_t crossed = g_census.crossed - g_hostBefore.crossed;
    const uint32_t counted = coop::desk_sim_sync::HostCrossings() - g_crossingsBefore;
    const uint32_t saved = g_census.autoSaveRan - g_hostBefore.autoSaveRan;
    const PeerState& p = g_hostAtCross;
    const bool ok = crossed == 1 && counted == 1 && (g_hostLeg == 0 || saved == 1) && p.canDL &&
                    (g_hostLeg == 1 || (p.fullyRead && p.fully));
    g_hostPass = g_hostPass && ok;
    UE_LOGI("[DESK-CROSSING-DRILL] host leg %d %s: its step crossed %u, the lane counted %u, autoSave ran %u; canDL "
            "%s, fullyProcessed %s", g_hostLeg, ok ? "PASS" : "FAIL", crossed, counted, saved, Yes(p.canRead, p.canDL),
            Yes(p.fullyRead, p.fully));
}

void HostLegDone() {
    HostJudge();
    if (ModeOf() == Mode::Join) {
        UE_LOGI("[DESK-CROSSING-DRILL] host DONE %s -- crossed before the client joins", g_hostPass ? "PASS" : "FAIL");
        g_host = HStep::Done;
        return;
    }
    if (++g_hostLeg < kLegs) {
        g_hostBefore = g_census;
        HostGo(HStep::Settle);
        return;
    }
    UE_LOGI("[DESK-CROSSING-DRILL] host DONE %s", g_hostPass ? "PASS" : "FAIL");
    g_host = HStep::Done;
}

void HostTick(coop::net::Session* s, void* desk) {
    const uint64_t now = ::GetTickCount64();
    switch (g_host) {
    case HStep::WaitClient:
        // join crosses before any client's world is ready; run waits for one.
        if (ModeOf() == Mode::Join ? s->AnyWorldReadyPeer() : !s->AnyWorldReadyPeer()) {
            if (ModeOf() == Mode::Join) HostAbandon("a client's world was ready before the host could cross");
            return;
        }
        HostGo(HStep::Census);
        return;
    case HStep::Census: {
        if (const wchar_t* fn = CensusNotLive()) {
            if (now - g_hostMs > kStartBoundMs) {
                UE_LOGW("[DESK-CROSSING-DRILL] ABANDONED on the host: the census watch on '%ls' never went live", fn);
                g_host = HStep::Done;
            }
            return;
        }
        const int purged = PurgeBand();
        UE_LOGI("[DESK-CROSSING-DRILL] host: the census is live; %d earlier band rows purged", purged);
        g_hostLeg = 0;
        HostGo(HStep::Arm);
        return;
    }
    case HStep::Settle:
        if (g_census.plays - g_hostBefore.plays < kSettleCycles) {
            if (now - g_hostMs > kHoldBoundMs) HostAbandon("the host's own loop stopped playing after a crossing");
            return;
        }
        HostGo(HStep::Arm);
        return;
    case HStep::Arm:
        g_catches = coop::signal_catch_sync::LocalCatchesRelayed();
        if (!WriteCaught(g_hostLeg)) { HostAbandon("the caught signal did not write"); return; }
        HostGo(HStep::Relayed);
        return;
    case HStep::Relayed:
        if (coop::signal_catch_sync::LocalCatchesRelayed() == g_catches) {
            if (now - g_hostMs > kArmBoundMs) HostAbandon("the caught signal was never relayed");
            return;
        }
        if (!CD::ArmDownloadFromSignal(1.0e6f, -1)) { HostAbandon("the download did not form"); return; }
        if (!DD::ReadMultiplier(desk, g_heldMultiplier) || !DD::WriteMultiplier(desk, 0.f) ||
            !DD::WriteNeedle(desk, 1.f - kHoldGap)) {
            HostAbandon("the needle could not be held");
            return;
        }
        g_holding = true;
        g_hostBefore = g_census;
        UE_LOGI("[DESK-CROSSING-DRILL] host holds leg %d's needle at %.4f (its multiplier %.3f held at 0)", g_hostLeg,
                1.f - kHoldGap, g_heldMultiplier);
        HostGo(HStep::Hold);
        return;
    case HStep::Hold:
        if (g_census.plays - g_hostBefore.plays < kHoldCycles) {
            // The fixture holds the needle where it put it: a download's own start may reset it after the form.
            float needle = 0.f;
            if (DD::ReadNeedle(desk, needle) && needle != 1.f - kHoldGap) DD::WriteNeedle(desk, 1.f - kHoldGap);
            if (now - g_hostMs > kHoldBoundMs) HostAbandon("the host's own loop never stepped");
            return;
        }
        g_crossingsBefore = coop::desk_sim_sync::HostCrossings();
        g_hostBefore = g_census;
        LetGo(desk);
        UE_LOGI("[DESK-CROSSING-DRILL] host lets leg %d's step run after %u of its step cycles", g_hostLeg, kHoldCycles);
        HostGo(HStep::WaitCross);
        return;
    case HStep::WaitCross:
        if (coop::desk_sim_sync::HostCrossings() == g_crossingsBefore) {
            if (now - g_hostMs > kCrossBoundMs) HostAbandon("the host's step never crossed");
            return;
        }
        g_hostAtCross = ReadState(desk);
        if (g_hostLeg == 1) {
            HostGo(HStep::WaitSave);
            return;
        }
        HostLegDone();
        return;
    case HStep::WaitSave:
        if (g_census.autoSaveRan == g_hostBefore.autoSaveRan) {
            if (now - g_hostMs > kSaveBoundMs) HostAbandon("the host's looker_behind crossing never saved");
            return;
        }
        HostLegDone();
        return;
    default:
        return;
    }
}

// ---- the client ---------------------------------------------------------------------------------------------------

enum class CStep : uint8_t { Ready, JoinWait, WaitHold, Watch, WaitSave, Done };
CStep    g_client = CStep::Ready;
int      g_leg = 0;
uint64_t g_stepMs = 0;
uint64_t g_readySinceMs = 0;
bool     g_clientPass = true;
Census   g_before;
uint32_t g_postStepsBefore = 0;
uint64_t g_lastKey = 0;
PeerState g_atCross;  // read once the path after the step ran, before looker_behind's save resets the download

void Go(CStep s) {
    g_client = s;
    g_stepMs = ::GetTickCount64();
}

// A leg's counts run from its start, the hold included: a client's own crossing comes during the hold.
void StartLeg() {
    g_before = g_census;
    g_postStepsBefore = coop::desk_sim_sync::ClientPostSteps();
    Go(CStep::WaitHold);
}

void Abandon(const char* why) {
    UE_LOGW("[DESK-CROSSING-DRILL] ABANDONED on the client: %s", why);
    g_client = CStep::Done;
}

void ClientJudge() {
    const uint32_t own = g_census.crossed - g_before.crossed;
    const uint32_t paths = coop::desk_sim_sync::ClientPostSteps() - g_postStepsBefore;
    const uint32_t entered = g_census.autoSaveIn - g_before.autoSaveIn;
    const uint32_t ran = g_census.autoSaveRan - g_before.autoSaveRan;
    const int rows = BandRows();
    const PeerState& p = g_atCross;
    bool ok = own == 0 && paths == 1 && p.canDL && (g_leg == 1 || (p.fullyRead && p.fully));
    if (g_leg == 1) ok = ok && ran == 0 && rows == 1;
    g_clientPass = g_clientPass && ok;
    UE_LOGI("[DESK-CROSSING-DRILL] client leg %d %s: its own step crossed %u, the path after the step ran %u, autoSave "
            "entered %u and ran %u, %d band rows; canDL %s, fullyProcessed %s", g_leg, ok ? "PASS" : "FAIL", own, paths,
            entered, ran, rows, Yes(p.canRead, p.canDL), Yes(p.fullyRead, p.fully));
}

void ClientTick(void* desk) {
    const uint64_t now = ::GetTickCount64();
    switch (g_client) {
    case CStep::Ready: {
        if (!coop::net_pump::HasAnnouncedWorldReady()) return;
        if (!g_readySinceMs) g_readySinceMs = now;
        const bool settled = coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle;
        const wchar_t* notLive = settled ? CensusNotLive() : nullptr;
        if (!settled || notLive) {
            if (now - g_readySinceMs > kStartBoundMs) Abandon(settled ? "a census watch never went live" : "the join never settled");
            return;
        }
        if (ModeOf() == Mode::Join) {
            Go(CStep::JoinWait);
            return;
        }
        UE_LOGI("[DESK-CROSSING-DRILL] client: the census is live");
        g_leg = 0;
        StartLeg();
        return;
    }
    case CStep::JoinWait: {
        // The first snapshot seeds canDL, and fullyProcessed comes from it or from the download this client forms for
        // the crossed signal; the crossing it carried is past. The world load restored the host's caught signal from
        // the save, which is no catch of this client's: it relays none, and the host's download holds through the
        // join, its needle at 1 in the stream.
        const PeerState p = ReadState(desk);
        if (!(p.canDL && p.fully) && now - g_stepMs <= kStartBoundMs) return;
        float needle = 0.f;
        const bool needleRead = DD::ReadNeedle(desk, needle);
        const uint64_t relayed = coop::signal_catch_sync::LocalCatchesRelayed();
        const bool ok = g_census.crossed == 0 && relayed == 0 && needleRead && needle >= 1.f && p.canDL &&
                        p.fullyRead && p.fully;
        UE_LOGI("[DESK-CROSSING-DRILL] client joined: its own step crossed %u, it relayed %llu catches, its needle "
                "reads %.4f; canDL %s, fullyProcessed %s", g_census.crossed, static_cast<unsigned long long>(relayed),
                needle, Yes(p.canRead, p.canDL), Yes(p.fullyRead, p.fully));
        UE_LOGI("[DESK-CROSSING-DRILL] client DONE %s", ok ? "PASS" : "FAIL");
        g_client = CStep::Done;
        return;
    }
    case CStep::WaitHold: {
        // The leg's download, new on this desk, with the host's needle held just below 1.
        uint64_t key = 0;
        float needle = 0.f;
        const bool held = CD::ReadDLSignalKey(key) && key != 0 && key != g_lastKey && DD::ReadNeedle(desk, needle) &&
                          needle >= 1.f - 2.f * kHoldGap && needle < 1.f;
        if (!held) {
            if (now - g_stepMs > kHoldBoundMs) Abandon("the leg's held download never reached this desk");
            return;
        }
        g_lastKey = key;
        UE_LOGI("[DESK-CROSSING-DRILL] client sees leg %d's needle held at %.4f", g_leg, needle);
        Go(CStep::Watch);
        return;
    }
    case CStep::Watch:
        if (coop::desk_sim_sync::ClientPostSteps() == g_postStepsBefore) {
            if (now - g_stepMs > kCrossBoundMs) Abandon("the host's crossing never reached this desk");
            return;
        }
        g_atCross = ReadState(desk);
        if (g_leg == 1) {
            Go(CStep::WaitSave);
            return;
        }
        ClientJudge();
        g_leg = 1;
        StartLeg();
        return;
    case CStep::WaitSave: {
        // looker_behind: the host's save reaches the band, and this desk's own save path has ended: its Delay(5)
        // reached autoSave, or the host's save reset this desk's download, after which the Delay's check fails.
        uint64_t key = 0;
        const bool pathEnded = g_census.autoSaveIn != g_before.autoSaveIn || !CD::ReadDLSignalKey(key) ||
                               key != g_lastKey;
        if (BandRows() == 0 || !pathEnded) {
            if (now - g_stepMs > kSaveBoundMs) {
                ClientJudge();
                UE_LOGI("[DESK-CROSSING-DRILL] client DONE FAIL -- the looker_behind save did not settle");
                g_client = CStep::Done;
            }
            return;
        }
        ClientJudge();
        UE_LOGI("[DESK-CROSSING-DRILL] client DONE %s", g_clientPass ? "PASS" : "FAIL");
        g_client = CStep::Done;
        return;
    }
    default:
        return;
    }
}

}  // namespace

void Tick(coop::net::Session* session) {
    if (ModeOf() == Mode::Off || !session || !session->connected()) return;
    const bool host = session->role() == coop::net::Role::Host;
    if (host ? g_host == HStep::Done : g_client == CStep::Done) return;
    if (!SS::EnsureResolved() || !CD::EnsureResolved()) return;
    void* desk = CD::Instance();
    if (!desk) return;
    if (host) HostTick(session, desk);
    else ClientTick(desk);
}

void OnDisconnect() {
    LetGo(CD::Instance());
    g_host = HStep::WaitClient;
    g_hostLeg = 0;
    g_hostMs = 0;
    g_hostPass = true;
    g_client = CStep::Ready;
    g_leg = 0;
    g_stepMs = 0;
    g_readySinceMs = 0;
    g_clientPass = true;
    g_lastKey = 0;
}

}  // namespace coop::dev::desk_crossing_drill
