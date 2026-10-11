// harness/autotest/autotest_seeddrill.cpp -- the ready-edge seed drill
// (VOTVCOOP_RUN_SEED_DRILL=1, host-only). It authors two distinctively-topic'd emails at the
// two instants a ready-edge seed must handle:
//   1. "[seed-drill] solo" -- BEFORE any client connects. It lands in the joiner's
//      transferred save, the seed's delta is 0 for it, and the vacuous adopt keeps the old
//      retry from re-broadcasting it. The client must hold exactly ONE copy: zero wire
//      applies of this topic is the pass, one is the duplicate reproduced.
//   2. "[seed-drill] in-window" -- while a slot is CONNECTED but NOT world-ready, in the
//      joiner's load window, after its save snapshot was taken. It is not in the save, so
//      only the ready-edge seed can deliver it; the client logging "email_sync: applied
//      email from slot 0" for this topic is the pass.
//
// VOTVCOOP_SEED_DISABLE=1 skips CaptureJoinSnapshot in email_sync and signal_sync, so the in-window
// email never arrives -- which is what shows the seed, not a leftover retry, is the delivery
// mechanism. Grep key: "[SEED-DRILL] authored", once per topic.

#include "harness/autotest.h"

#include "coop/net/session.h"
#include "coop/world/email_sync.h"
#include "harness/session_runtime.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/world/email.h"

#include <atomic>

namespace harness::autotest {
namespace {

namespace GT = ue_wrap::game_thread;

// The email lane's own predicates live on the game thread; this thread asks them there.
bool EmailSettled() {
    return GT::RunAndWait([](std::atomic<int>& a) { a.store(coop::email_sync::Settled() ? 1 : 2); }) == 1;
}
bool SnapshotTaken(int slot) {
    return GT::RunAndWait([slot](std::atomic<int>& a) {
               a.store(coop::email_sync::JoinSnapshotCaptured(slot) ? 1 : 2);
           }) == 1;
}

void AuthorDrillEmail(const wchar_t* topic) {
    GT::Post([topic] {
        ue_wrap::email::Row r;
        r.username = 0;
        r.topic = topic;
        r.text = L"seeds-arc drill row (autotest_seeddrill)";
        const ue_wrap::RowApply a = ue_wrap::email::AddEmail(r);
        if (a.result == ue_wrap::RowApplyResult::Applied)
            UE_LOGI("[SEED-DRILL] authored %ls", topic);
        else
            UE_LOGW("[SEED-DRILL] AddEmail FAILED for %ls -- %s", topic, a.why);
    });
}

}  // namespace

DWORD WINAPI SeedDrillThread(LPVOID /*arg*/) {
    if (IsClientRole()) {
        UE_LOGI("[SEED-DRILL] client role -- host-only drill, exiting");
        return 0;
    }
    auto& s = harness::session_runtime::Session();

    // Phase 1: the SOLO email -- must land before any client connects. Authored the moment the
    // session runs and the email lane has settled against its array, the state a row added to this
    // world needs; the loop bounds below are failure bounds, never a wait. If a peer already
    // connected, skip: the solo case needs the pre-connect save state.
    for (int i = 0; i < 600 && !s.running(); ++i) ::Sleep(100);
    bool settled = false;
    for (int i = 0; i < 1200 && s.running() && !(settled = EmailSettled()); ++i) ::Sleep(100);
    if (!settled) {
        UE_LOGW("[SEED-DRILL] the email lane never settled -- drill not run");
        return 0;
    }
    bool anyPeer = false;
    for (int i = 1; i < coop::net::kMaxPeers; ++i)
        if (s.IsSlotWorldReady(i)) anyPeer = true;
    if (!anyPeer) {
        AuthorDrillEmail(L"[seed-drill] solo");
    } else {
        UE_LOGW("[SEED-DRILL] a peer is already world-ready -- solo phase skipped");
    }

    // Phase 2: the IN-WINDOW email -- authored for a slot that is CONNECTED, not yet world-ready (the
    // joiner's load window), and whose save snapshot has been captured: a row authored before the
    // capture would ride the SAVE and leave the loss case unexercised, so the capture itself is the
    // state waited on. Author once. The loop bound is a failure bound.
    UE_LOGI("[SEED-DRILL] waiting for a connected-but-not-ready slot past its snapshot...");
    for (int waited = 0; waited < 3000; ++waited) {  // up to 5 min
        for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) {
            if (!s.HasPeerConn(slot) || s.IsSlotWorldReady(slot) || !SnapshotTaken(slot)) continue;
            AuthorDrillEmail(L"[seed-drill] in-window");
            return 0;
        }
        ::Sleep(100);
    }
    UE_LOGW("[SEED-DRILL] no load window observed -- in-window phase not run");
    return 0;
}

}  // namespace harness::autotest
