// coop/dev/desk_verb_drill_refiner.cpp -- the desk-verb drill's refiner legs and the refiner half of its join; see
// coop/dev/desk_verb_drill_internal.h. The step machines are coop/dev/desk_verb_drill.cpp's.

#include "coop/dev/desk_verb_drill_internal.h"

#include "coop/interactables/comp_sync.h"    // MirrorActive: the host's decode as this client mirrors it
#include "coop/interactables/signal_sync.h"  // JoinSnapshotCaptured: the joiner's save is taken
#include "coop/net/session.h"                // kMaxPeers

#include "ue_wrap/core/log.h"
#include "ue_wrap/desk/comp_pane.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/desk_press.h"

#include <cstdio>
#include <cwchar>
#include <string>

namespace coop::dev::desk_verb_drill::detail {
namespace {

namespace CD = ue_wrap::console_desk;
namespace DP = ue_wrap::desk_press;

// HOST: the leg's readings at its arm, and whether its completion was set.
int32_t g_hostLevel = -1;
int32_t g_hostProcessed = -1;
bool    g_poked = false;
// CLIENT: its readings at the press.
int32_t g_levelBefore = -1;
int32_t g_processedBefore = -1;
// join, CLIENT: the census as it went live ahead of the load, so the join counts its own restore.
Census g_joinBase;

// The last level's completion runs a world trigger named by the row's signal (CXXHeaderDump/analogDScreenTest.cpp :6006-6018).
bool FiresATrigger(const std::wstring& signal) {
    return _wcsicmp(signal.c_str(), L"evil") == 0 || _wcsicmp(signal.c_str(), L"lifecrystal") == 0 ||
           _wcsicmp(signal.c_str(), L"deer") == 0;
}

}  // namespace

// ---- the host ------------------------------------------------------------------------------------------------------

const char* RefinerArm(int leg, void* player) {
    if (!HoldProcessLevel(3)) return "the process upgrade did not read or write";
    if (leg == kProcessed) {
        void* stop = Button(kStop);
        if (CompLatched() && (!stop || !DP::Press(CD::Instance(), player, stop)))
            return "the host's own stop press did not run";
        if (FiresATrigger(CompSignal())) return "the band row's signal fires a world trigger at its last level";
        if (!SetCompLevel(2)) return "the refiner's row did not take level 2";
    }
    g_hostLevel = CompLevel();
    g_hostProcessed = Processed();
    g_poked = false;
    return nullptr;
}

const char* RefinerStep(int leg) {
    if ((leg != kComplete && leg != kProcessed) || g_poked || !CompLatched()) return nullptr;
    if (!SetCompProgress(100.f)) return "the refiner's progress did not write";
    g_poked = true;
    return nullptr;
}

bool RefinerHostDone(int leg) {
    if (leg == kStart) return CompLatched();
    if (leg == kStop) return !CompLatched();
    return CompLevel() > g_hostLevel;  // complete, processed
}

bool RefinerHostJudge(int leg, const Census& d, uint64_t glossesSent, std::string& tail) {
    bool ok;
    if (leg == kStart)  // the replayed press latched this machine's refiner
        ok = d.game[kCompStart] == 1 && d.ran[kCompStart] == 1 && CompLatched();
    else if (leg == kStop)
        ok = d.game[kCompStop] == 1 && d.ran[kCompStop] == 1 && !CompLatched();
    else  // completed here: its gloss entered and was refused, sent to the client, and its processed point put back
        ok = d.game[kCompStart] >= 1 && d.game[kSetSignalID] == 1 && d.game[kAddGloss] == 1 &&
             d.ran[kAddGloss] == 0 && glossesSent == 1 && (leg != kProcessed || Processed() == g_hostProcessed);
    char buf[96];
    std::snprintf(buf, sizeof(buf), "; refiner level %d (was %d), latched %d, processed %d (was %d)", CompLevel(),
                  g_hostLevel, CompLatched() ? 1 : 0, Processed(), g_hostProcessed);
    tail = buf;
    return ok;
}

bool JoinSnapshotTaken() {
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot)
        if (coop::signal_sync::JoinSnapshotCaptured(slot)) return true;
    return false;
}

// From the tick the host starts hosting, with no drive to spawn and seat: the rig's client connects within seconds of
// it, and its save snapshot is taken at the connect. The start is comp_start itself, from 50%, once the save's
// upgrades and the refiner read, which a world just up may not yet.
int HostJoinDecode(const char*& why) {
    if (JoinSnapshotTaken()) {
        why = "the joiner's save snapshot was taken before the host's refiner decoded";
        return -1;
    }
    bool succ = false;
    if (!HoldProcessLevel(1) || !ArmCompRow() || !ue_wrap::comp_pane::CallStart(50.f, succ)) return 0;
    if (!succ || !CompLatched()) {
        why = "the host's comp_start refused its row";
        return -1;
    }
    UE_LOGI("[DESK-VERB-DRILL] host's refiner decodes from 50%% before any client joins");
    return 1;
}

// ---- the client ----------------------------------------------------------------------------------------------------

bool RefinerArmed(int leg) {
    // The uploaded row in and idle; the host's decode running; stopped; the host's row at level 2.
    if (leg == kStart) return CompRow() != 0 && !coop::comp_sync::MirrorActive();
    if (leg == kStop) return coop::comp_sync::MirrorActive();
    if (leg == kComplete) return !coop::comp_sync::MirrorActive() && !CompLatched();
    return CompLevel() == 2 && !coop::comp_sync::MirrorActive();  // processed
}

void RefinerBeforePress() {
    g_levelBefore = CompLevel();
    g_processedBefore = Processed();
}

bool RefinerClientDone(int leg) {
    if (leg == kStart) return coop::comp_sync::MirrorActive() || CompLatched();
    if (leg == kStop) return !coop::comp_sync::MirrorActive() && !CompLatched();
    return CompLevel() > g_levelBefore;  // complete, processed
}

bool RefinerClientJudge(int leg, const Census& d, uint64_t glossesMade, std::string& tail) {
    // This refiner never latches: the host's decode is mirrored here. A completion's gloss was made here once, and the
    // last level's processed point is on this profile.
    bool ok = !CompLatched();
    if (leg == kComplete || leg == kProcessed)
        ok = ok && d.other[kAddGloss] == 1 && glossesMade == 1 &&
             (leg != kProcessed || Processed() == g_processedBefore + 1);
    char buf[112];
    std::snprintf(buf, sizeof(buf), "; refiner level %d (was %d), latched %d, mirror %d, processed %d (was %d)",
                  CompLevel(), g_levelBefore, CompLatched() ? 1 : 0, coop::comp_sync::MirrorActive() ? 1 : 0,
                  Processed(), g_processedBefore);
    tail = buf;
    return ok;
}

void MarkJoinCensus() { g_joinBase = CensusNow(); }

bool RefinerJoinSeeded() { return coop::comp_sync::MirrorActive(); }

bool RefinerJoinCheck(std::string& tail) {
    const Census d = Since(g_joinBase);
    const bool refused = d.game[kCompStart] >= 1 && d.ran[kCompStart] == 0;
    const bool mirrored = !CompLatched() && coop::comp_sync::MirrorActive();
    char buf[128];
    std::snprintf(buf, sizeof(buf), "; the restore's comp_start entered %u, ran %u; refiner latched %d, mirror %d",
                  d.game[kCompStart], d.ran[kCompStart], CompLatched() ? 1 : 0,
                  coop::comp_sync::MirrorActive() ? 1 : 0);
    tail = buf;
    return refused && mirrored;
}

void RefinerReset() {
    g_hostLevel = g_hostProcessed = -1;
    g_poked = false;
    g_levelBefore = g_processedBefore = -1;
    g_joinBase = Census{};
}

}  // namespace coop::dev::desk_verb_drill::detail
