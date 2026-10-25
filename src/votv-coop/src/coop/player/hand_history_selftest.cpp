// coop/player/hand_history_selftest.cpp -- the un-gated selftest of the hand history ring, run once per
// process (shape: coop/build_trust/build_trust_selftest.cpp). Pinned times on the last valid slot, no
// engine and no wire: a ring that forgets a class early, or keeps one forever, lets the host judge an
// act against the wrong hand.
//
// Red arm: with the dev row selftest_break_hand_history the first Note is skipped, so every check that
// reads that hold fails and the run must fail.

#include "coop/player/hand_history.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/core/log.h"

#include <mutex>
#include <string>

namespace coop::hand_history {
namespace {

constexpr uint8_t kSlot = coop::players::kMaxPeers - 1;

struct Tally {
    int pass = 0;
    int total = 0;
    void operator()(bool ok, const char* what) {
        ++total;
        if (ok) { ++pass; return; }
        UE_LOGE("hand_history selftest FAIL: %s", what);
    }
};

// Note on the last slot, except that the red arm drops the very first one.
struct Noter {
    bool skipFirst;
    void operator()(bool has, const wchar_t* cls, uint64_t nowMs) {
        if (skipFirst) { skipFirst = false; return; }
        Note(kSlot, has, cls, nowMs);
    }
};

void RunHeldNow(Tally& check, bool breakIt) {
    Clear(kSlot);
    Noter note{breakIt};
    note(true, L"prop_a_C", 1000);
    check(HeldWithin(kSlot, L"prop_a_C", 0, 1000), "held now: the class held at this moment is not found");
    check(HeldWithin(kSlot, L"prop_a_C", 0, 9000), "held now: an open entry is not found at a later time");
    check(!HeldWithin(kSlot, L"prop_b_C", 5000, 1000), "held now: a class never held is found");
    Clear(kSlot);
}

// A hold that began after the reference time still counts: an act judged at its arrival, before the take applied.
void RunOpenedAfter(Tally& check) {
    Clear(kSlot);
    Note(kSlot, true, L"prop_a_C", 1250);
    check(HeldWithin(kSlot, L"prop_a_C", 500, 1000), "opened after: an entry opened after the time is not found");
    Note(kSlot, false, L"", 1400);
    check(HeldWithin(kSlot, L"prop_a_C", 500, 1000), "opened after: an entry opened and closed after the time is not found");
    Clear(kSlot);
}

void RunDropWindow(Tally& check) {
    Clear(kSlot);
    Note(kSlot, true, L"prop_a_C", 1000);
    Note(kSlot, false, L"", 1500);
    check(HeldWithin(kSlot, L"prop_a_C", 500, 2000), "dropped inside the window: not found");
    check(!HeldWithin(kSlot, L"prop_a_C", 499, 2000), "dropped: the window's edge is not exact");
    check(!HeldWithin(kSlot, L"prop_a_C", 100, 3000), "dropped past the window: still found");
    Clear(kSlot);
}

void RunSwitch(Tally& check) {
    Clear(kSlot);
    Note(kSlot, true, L"prop_a_C", 1000);
    Note(kSlot, true, L"prop_b_C", 1200);
    check(HeldWithin(kSlot, L"prop_a_C", 300, 1400), "switch A to B: A is lost inside the window");
    check(HeldWithin(kSlot, L"prop_b_C", 0, 1400), "switch A to B: B is not held");
    check(!HeldWithin(kSlot, L"prop_a_C", 100, 1400), "switch A to B: A outlives its window");
    Clear(kSlot);
}

// The held class noted again must not open an entry: four more notes of B would otherwise push A out.
void RunReNote(Tally& check) {
    Clear(kSlot);
    Note(kSlot, true, L"prop_a_C", 1000);
    Note(kSlot, true, L"prop_b_C", 1100);
    for (uint64_t t = 1200; t <= 1500; t += 100) Note(kSlot, true, L"prop_b_C", t);
    check(HeldWithin(kSlot, L"prop_a_C", 100000, 2000), "re-note: a repeated class pushed an older entry out");
    check(HeldWithin(kSlot, L"prop_b_C", 0, 2000), "re-note: the held class is not held");
    Clear(kSlot);
}

void RunOverwrite(Tally& check) {
    Clear(kSlot);
    const wchar_t* const names[5] = {L"prop_a_C", L"prop_b_C", L"prop_c_C", L"prop_d_C", L"prop_e_C"};
    for (int i = 0; i < 5; ++i) Note(kSlot, true, names[i], 1000 + 100 * static_cast<uint64_t>(i));
    check(!HeldWithin(kSlot, L"prop_a_C", 100000, 2000), "five switches: the oldest was not overwritten");
    check(HeldWithin(kSlot, L"prop_b_C", 100000, 2000), "five switches: the second oldest was lost");
    check(HeldWithin(kSlot, L"prop_e_C", 0, 2000), "five switches: the newest is not held");
    Clear(kSlot);
}

void RunClear(Tally& check) {
    Clear(kSlot);
    Note(kSlot, true, L"prop_a_C", 1000);
    Clear(kSlot);
    check(!HeldWithin(kSlot, L"prop_a_C", 100000, 1000), "clear: the slot still remembers its hold");
    Note(kSlot, true, L"prop_b_C", 2000);
    check(HeldWithin(kSlot, L"prop_b_C", 0, 2000), "clear: the slot does not record after a clear");
    Clear(kSlot);
}

void RunCase(Tally& check) {
    Clear(kSlot);
    Note(kSlot, true, L"Prop_A_c", 1000);
    check(HeldWithin(kSlot, L"PROP_a_C", 0, 1000), "case: a differently cased name does not match");
    Clear(kSlot);
}

void RunBadSlot(Tally& check) {
    Clear(kSlot);
    for (const uint8_t bad : {static_cast<uint8_t>(coop::players::kMaxPeers), static_cast<uint8_t>(255)}) {
        Note(bad, true, L"prop_a_C", 1000);
        check(!HeldWithin(bad, L"prop_a_C", 100000, 1000), "bad slot: an out-of-range slot answers true");
        check(!HeldWithin(kSlot, L"prop_a_C", 100000, 1000), "bad slot: an out-of-range Note reached the last slot");
        check(!HeldWithin(0, L"prop_a_C", 100000, 1000), "bad slot: an out-of-range Note reached slot 0");
    }
    Clear(kSlot);
}

bool RunSelftestBody() {
    Tally check;
    const bool breakIt = coop::config::ResolveFlag(coop::config_registry::rows::selftest_break_hand_history);
    RunHeldNow(check, breakIt);
    RunOpenedAfter(check);
    RunDropWindow(check);
    RunSwitch(check);
    RunReNote(check);
    RunOverwrite(check);
    RunClear(check);
    RunCase(check);
    RunBadSlot(check);
    if (check.pass == check.total) {
        UE_LOGI("hand_history selftest: ALL PASS (%d checks)", check.total);
        return true;
    }
    UE_LOGE("hand_history selftest: %d/%d checks passed", check.pass, check.total);
    return false;
}

}  // namespace

// Once per process: the session-runtime call is once per session start, and a later call returns
// the first run's verdict without output.
bool RunSelftest() {
    static std::once_flag ran;
    static bool verdict = false;
    std::call_once(ran, [] { verdict = RunSelftestBody(); });
    return verdict;
}

}  // namespace coop::hand_history
