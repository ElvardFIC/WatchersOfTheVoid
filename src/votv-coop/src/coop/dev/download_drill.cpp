// coop/dev/download_drill.cpp -- see coop/dev/download_drill.h.

#include "coop/dev/download_drill.h"

#include "coop/config/config.h"
#include "coop/interactables/download_arm_sync.h"
#include "coop/interactables/signal_catch_sync.h"
#include "coop/net/session.h"
#include "coop/save/save_transfer.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/dish.h"
#include "ue_wrap/desk/signal_dynamic.h"
#include "ue_wrap/desk/space_renderer.h"
#include "ue_wrap/facts/function_row.h"

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace coop::dev::download_drill {
namespace {

namespace AS = coop::download_arm_sync;
namespace CD = ue_wrap::console_desk;
namespace D  = ue_wrap::dish;
namespace SD = ue_wrap::signal_dynamic;
namespace sg = ue_wrap::script_gate;

constexpr int kCycles = 5;
constexpr float kDrillX = 917000.f;  // a band of caught signals no sky roll makes
constexpr uint64_t kSourceBoundMs = 60000;  // HOST: the sky's first signals, once its session state is the mode's
constexpr uint64_t kStepBoundMs = 30000;    // HOST: a step waiting on its own machine or its catch's relay
constexpr uint64_t kLeaveBoundMs = 90000;   // HOST (join): the first life's leave, which the rig's rejoin makes
constexpr uint64_t kClientBoundMs = 90000;  // CLIENT: world-ready to every replay
constexpr uint64_t kCheckEveryMs = 250;
constexpr int kTagFormCount = 0x44444646;   // 'DDFF'
constexpr int kTagLogCount  = 0x4444464C;   // 'DDFL'
UE_FACT_FN(kRowFormDownload, "dev.download_drill", L"analogDScreenTest_C", L"formDownload", Declared);
UE_FACT_FN(kRowWriteToCoordLog2, "dev.download_drill", L"analogDScreenTest_C", L"writeToCoordLog_2", Declared);
const wchar_t* const kLogLine = L"Signal data deleted";

enum class HStep : uint8_t { Wait, Catch, CatchWait, Arm, Reset, AwaitLeave, JoinWindow, JoinCatchWait, JoinArm, Done };
enum class CStep : uint8_t { Ready, Replays, Done };
HStep    g_host = HStep::Wait;
CStep    g_client = CStep::Ready;
uint64_t g_stepMs = 0;
uint64_t g_nextCheckMs = 0;
int      g_session = 1;
int      g_cycle = 0;
int      g_joiner = -1;
int      g_firstLife = -1;  // HOST (join): the slot the client's first life held when the first arm was made
bool     g_saidLife1 = false;  // CLIENT (join): the first life said its arm replayed
uint64_t g_mark = 0;
bool     g_formWatch = false;
bool     g_logWatch = false;
int      g_formBodies = 0;  // formDownload bodies this peer ran (a refused call runs none)
int      g_resetLines = 0;  // the game's reset line written here, by any writer, counted at the write
ue_wrap::space_renderer::SignalRow g_source;

const std::string& Mode() {
    static const std::string s = coop::config::ResolveString(::coop::config_registry::rows::download_drill);
    return s;
}
bool Join() { return Mode() == "join"; }
bool Enabled() { return Mode() == "run" || Join(); }
bool Expired(uint64_t bound) { return ::GetTickCount64() - g_stepMs > bound; }

void Fail(const char* what) {
    UE_LOGW("[DOWNLOAD-DRILL] FAIL in session %d: %s", g_session, what);
    g_host = HStep::Done;
    g_client = CStep::Done;
}
void Abandon(const char* why) {
    UE_LOGW("[DOWNLOAD-DRILL] ABANDONED in session %d: %s", g_session, why);
    g_host = HStep::Done;
    g_client = CStep::Done;
}

void OnFormPost(const sg::Call&) { ++g_formBodies; }

sg::Verdict OnLogPre(const sg::Call& call) {
    const std::wstring line = CD::ReadCoordLogWrite(call.function, call.locals);
    if (line.compare(0, ::wcslen(kLogLine), kLogLine) == 0) ++g_resetLines;
    return sg::Verdict::Run;
}

bool FindSource() {
    std::vector<ue_wrap::space_renderer::SignalRow> sky;
    if (!ue_wrap::space_renderer::ReadSignals(sky)) return false;
    for (const auto& s : sky) {
        if (s.objectName.empty() || s.objectName == L"None") continue;
        g_source = s;
        return true;
    }
    return false;
}

bool WriteCaught(int cycle) {
    CD::CoordSignal sig;
    sig.x = kDrillX + static_cast<float>(cycle);
    sig.y = 0.f;
    sig.z = 0.f;
    sig.type = g_source.type;
    sig.strength = 1.f;
    sig.frequency = 1.f + static_cast<float>(cycle);
    sig.frequencySpread = 0.5f;
    sig.polarity = 0.f;
    sig.polaritySpread = 0.5f;
    sig.objectName = g_source.objectName;
    return CD::WriteCoordSignal(sig);
}

void HGo(HStep s) { g_host = s; g_stepMs = ::GetTickCount64(); }

bool Reset(const char* where) {
    if (CD::CallDeleteActiveSignal() && !CD::DownloadMeshValid()) return true;
    char why[128];
    std::snprintf(why, sizeof(why), "the host's deleteActiveSignal did not reset its download (%s)", where);
    Abandon(why);
    return false;
}

void HostTick(coop::net::Session* s) {
    switch (g_host) {
    case HStep::Wait:
        if (!s->running() || !CD::EnsureResolved() || !CD::Instance() || !D::EnsureResolved()) {
            g_stepMs = ::GetTickCount64();
            return;
        }
        // Once a client is in its world: in join its first life, whose leave and return the rig's rejoin makes.
        if (!s->AnyWorldReadyPeer()) {
            g_stepMs = ::GetTickCount64();
            return;
        }
        if (!FindSource()) {  // the sky rolls its first signals a moment after the world comes up; bounded from here
            if (Expired(kSourceBoundMs)) Abandon("the sky rolled no signal whose object a caught signal could name");
            return;
        }
        HGo(HStep::Catch);
        return;
    case HStep::Catch:
        g_mark = coop::signal_catch_sync::LocalCatchesRelayed();
        if (!WriteCaught(g_cycle)) {
            Abandon("the caught signal could not be written");
            return;
        }
        HGo(HStep::CatchWait);
        return;
    case HStep::CatchWait:
        if (coop::signal_catch_sync::LocalCatchesRelayed() <= g_mark) {
            if (Expired(kStepBoundMs)) Abandon("the caught signal was never relayed");
            return;
        }
        HGo(HStep::Arm);
        return;
    case HStep::Arm:
        if (!D::CallCheckFordDishes() || !CD::DownloadMeshValid()) {
            Abandon("the host's desk formed no download from the caught signal");
            return;
        }
        if (Join()) {
            for (int i = 1; i < coop::net::kMaxPeers && g_firstLife < 0; ++i)
                if (s->IsSlotWorldReady(i)) g_firstLife = i;
            UE_LOGI("[DOWNLOAD-DRILL] host (join): armed while slot %d's first life is in; its rejoin comes next",
                    g_firstLife);
            HGo(HStep::AwaitLeave);
            return;
        }
        HGo(HStep::Reset);
        return;
    case HStep::Reset:
        if (!Reset("a run cycle")) return;
        if (++g_cycle < kCycles) {
            HGo(HStep::Catch);
            return;
        }
        UE_LOGI("[DOWNLOAD-DRILL] host (run): %d arms and %d resets made", kCycles, kCycles);
        g_host = HStep::Done;
        return;
    case HStep::AwaitLeave:
        // The rejoiner's world is captured from this machine, armed; its window only opens once the first life left.
        // With no other client that leave ends the host's session, and OnDisconnect moves on; with one, it is seen here.
        if (g_firstLife > 0 && s->IsSlotWorldReady(g_firstLife)) {
            if (Expired(kLeaveBoundMs)) Abandon("the client's first life never left for its rejoin");
            return;
        }
        HGo(HStep::JoinWindow);
        return;
    case HStep::JoinWindow: {
        // A joiner's window: its world taken, armed, and not yet ready, so what the machine does now reaches it only as
        // its connect rows -- a reset, then the catch's seed, then an arm, in the game's order.
        g_joiner = -1;
        for (int i = 1; i < coop::net::kMaxPeers && g_joiner < 0; ++i)
            if (coop::save_transfer::WorldTakenFor(i) && !s->IsSlotWorldReady(i)) g_joiner = i;
        if (g_joiner < 0) {
            if (s->AnyWorldReadyPeer()) Abandon("a joiner's world was ready before the host saw its window");
            return;
        }
        if (!Reset("the joiner's window")) return;
        g_cycle = 1;
        g_mark = coop::signal_catch_sync::LocalCatchesRelayed();
        if (!WriteCaught(g_cycle)) {
            Abandon("the caught signal could not be written");
            return;
        }
        HGo(HStep::JoinCatchWait);
        return;
    }
    case HStep::JoinCatchWait:
        if (coop::signal_catch_sync::LocalCatchesRelayed() <= g_mark) {
            if (Expired(kStepBoundMs)) Abandon("the joiner's-window catch was never relayed");
            return;
        }
        HGo(HStep::JoinArm);
        return;
    case HStep::JoinArm:
        if (!D::CallCheckFordDishes() || !CD::DownloadMeshValid()) {
            Abandon("the host's desk formed no download from the joiner's-window catch");
            return;
        }
        UE_LOGI("[DOWNLOAD-DRILL] host (join): reset, caught and armed again while slot %d loaded (%s)", g_joiner,
                s->IsSlotWorldReady(g_joiner) ? "its world ready by the arm, which reached it live"
                                              : "all three before its world was ready");
        g_host = HStep::Done;
        return;
    default:
        return;
    }
}

void ClientReady() {
    // The baseline at this client's own world-ready announce, before any connect row applies.
    if (!coop::net_pump::HasAnnouncedWorldReady() || !CD::EnsureResolved() || !CD::Instance()) {
        g_stepMs = ::GetTickCount64();
        return;
    }
    g_formBodies = 0;
    g_resetLines = 0;
    g_client = CStep::Replays;
}

void ClientJoin(const AS::Counts& c) {
    CD::CoordSignal sig;
    const bool read = CD::ReadCoordSignal(sig);
    // The first life: the host's first arm replayed live. Its word is the rig's cue to kill and relaunch this client.
    if (!g_saidLife1 && read && sig.x == kDrillX && c.replayedArm >= 1 && c.replayedReset == 0 &&
        CD::DownloadMeshValid()) {
        g_saidLife1 = true;
        UE_LOGI("[DOWNLOAD-DRILL] client LIFE 1 DONE in session %d: the host's first arm replayed here; the rejoin "
                "follows", g_session);
        return;
    }
    if (g_saidLife1) return;  // waiting for the rig to end this life
    const bool caughtB = read && sig.x == kDrillX + 1.f;
    if (c.replayedReset < 1 || c.replayedArm < 1 || !CD::DownloadMeshValid() || !caughtB) {
        if (Expired(kClientBoundMs))
            Fail("the joiner's machine did not end on the host's reset, catch and arm (the connect rows' order)");
        return;
    }
    UE_LOGI("[DOWNLOAD-DRILL] client DONE in session %d (join): the connect rows reset this joiner's machine, then "
            "its catch and arm came as the host's -- PASS (%llu reset(s), %llu arm(s) replayed)", g_session,
            static_cast<unsigned long long>(c.replayedReset), static_cast<unsigned long long>(c.replayedArm));
    g_client = CStep::Done;
}

void ClientRun(const AS::Counts& c) {
    if (c.replayedArm < kCycles || c.replayedReset < kCycles || CD::DownloadMeshValid()) {
        if (Expired(kClientBoundMs)) Fail("the host's arms and resets did not all replay here");
        return;
    }
    CD::CoordSignal sig;
    SD::Row row;
    if (!CD::ReadCoordSignal(sig) || !(sig.objectName.empty() || sig.objectName == L"None")) {
        Fail("the caught signal was not cleared by the reset");
        return;
    }
    if (!CD::ReadDownloadRow(row) || row.frequency != 0 || row.quality != 0 || row.objectType != 0) {
        Fail("the reset download kept the last signal's frequency, quality or object type");
        return;
    }
    // Each replay's own measure: one formDownload body an arm, one reset line a reset, whatever connect row came too.
    if (static_cast<uint64_t>(g_formBodies) != c.replayedArm || static_cast<uint64_t>(g_resetLines) != c.replayedReset) {
        char why[192];
        std::snprintf(why, sizeof(why), "formDownload ran %d time(s) and the reset line was written %d time(s), for %llu "
                      "arms and %llu resets replayed", g_formBodies, g_resetLines,
                      static_cast<unsigned long long>(c.replayedArm), static_cast<unsigned long long>(c.replayedReset));
        Fail(why);
        return;
    }
    // This client's own arm and reset: the machine is the host's, so both are refused and neither body runs.
    const uint64_t refusedBefore = c.refused;
    const int bodiesBefore = g_formBodies;
    const int linesBefore = g_resetLines;
    CD::ArmDownloadFromSignal(0.f, -1);
    CD::CallDeleteActiveSignal();
    if (AS::LaneCounts().refused != refusedBefore + 2 || g_formBodies != bodiesBefore || g_resetLines != linesBefore ||
        CD::DownloadMeshValid()) {
        Fail("this client's own formDownload or deleteActiveSignal was not refused");
        return;
    }
    UE_LOGI("[DOWNLOAD-DRILL] client DONE in session %d (run): %llu arms and %llu resets replayed with the host's values, "
            "one formDownload body an arm, one reset line a reset, the reset clean, its own arm and reset refused -- PASS",
            g_session, static_cast<unsigned long long>(c.replayedArm), static_cast<unsigned long long>(c.replayedReset));
    g_client = CStep::Done;
}

void ClientTick() {
    const AS::Counts c = AS::LaneCounts();
    if (c.replayOff > 0 || c.dropped > 0) {
        Fail(c.replayOff > 0 ? "a replay's outcome was not the host's (the lane's own line says which)"
                             : "a row found nothing to replay on (the lane's own line says which)");
        return;
    }
    switch (g_client) {
    case CStep::Ready:
        ClientReady();
        return;
    case CStep::Replays:
        if (Join()) ClientJoin(c);
        else ClientRun(c);
        return;
    default:
        return;
    }
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (!Enabled() || !s) return;
    if (!g_formWatch) g_formWatch = sg::WatchName(kRowFormDownload, kTagFormCount, nullptr, &OnFormPost);
    if (!g_logWatch) g_logWatch = sg::WatchName(kRowWriteToCoordLog2, kTagLogCount, &OnLogPre, nullptr);
    const uint64_t now = ::GetTickCount64();
    if (now < g_nextCheckMs) return;
    g_nextCheckMs = now + kCheckEveryMs;
    if (s->role() == coop::net::Role::Host) {
        if (g_host != HStep::Done) HostTick(s);
    } else if (s->connected() && g_client != CStep::Done) {
        ClientTick();
    }
}

void OnDisconnect() {
    if (!Enabled()) return;
    ++g_session;
    // HOST, join: a host its last client left ends its session, so the first life's leave lands here; its machine stays
    // armed on the first signal, and the rejoin's window is the next session's step.
    if (Join() && g_host == HStep::AwaitLeave) {
        HGo(HStep::JoinWindow);
        return;
    }
    g_host = HStep::Wait;
    g_client = CStep::Ready;
    g_stepMs = g_nextCheckMs = 0;
    g_cycle = 0;
    g_joiner = -1;
    g_firstLife = -1;
    g_saidLife1 = false;
    g_mark = 0;
    g_formBodies = 0;
    g_resetLines = 0;
}

}  // namespace coop::dev::download_drill
