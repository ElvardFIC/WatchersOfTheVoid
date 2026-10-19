// coop/interactables/notebook_sync.cpp -- see coop/interactables/notebook_sync.h.

#include "coop/interactables/notebook_sync.h"

#include "coop/comms/peer_action_feed.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_key_index.h"
#include "coop/props/prop_record_gates.h"
#include "coop/props/prop_sound.h"
#include "coop/props/prop_save_data.h"
#include "ue_wrap/actors/notebook.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/device_screen.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace coop::notebook_sync {
namespace {

namespace sg = ue_wrap::script_gate;
namespace N  = ue_wrap::notebook;
namespace DS = ue_wrap::device_screen;
using Clock = std::chrono::steady_clock;

UE_FACT_FN(kNotebookCommit, "notebook_sync", L"ui_notebook_C", L"BndEvt__MultiLineEditableText_86_K2Node_ComponentBoundEvent_5_OnMultiLineEditableTextCommittedEvent__DelegateSignature", Declared);
UE_FACT_FN(kNotebookQuit, "notebook_sync", L"ui_notebook_C", L"quit", Declared);
UE_FACT_FN(kNotebookUsedOn, "notebook_sync", L"prop_notebook_C", L"playerUsedOn", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_commitWatched = false;
bool g_quitWatched = false;
bool g_usedWatched = false;
bool g_registered = false;

// A live publish is at most one per kPublishEvery; the close is never held by it.
constexpr auto kPublishEvery = std::chrono::seconds(2);
// A client lets go of the notebook's claim only after the host has answered its last publish; if the answer
// does not come in this long it lets go anyway.
constexpr auto kAckBound = std::chrono::seconds(10);
Clock::time_point g_nextPublish{};
// What the editing peer last published or applied, or read when it began: the text a commit compares to.
struct Baseline {
    std::wstring key;
    std::wstring claim;
    std::vector<std::wstring> pages;
};
Baseline g_base;
// The notebook the editor was open on when quit began; quit clears the widget's `clipboard` in its own
// body, so only a watch before the body can name it.
ue_wrap::CachedObjRef g_closing;

// CLIENT: the publishes the host has not answered yet. The lane republishes every intent it takes to
// every peer, the author included, so a canonical record for the notebook's Key comes back after each
// publish; one that arrives after a publish answers the oldest still unanswered.
struct Flight {
    std::wstring key;
    std::wstring claim;
    int unacked = 0;
    bool closing = false;       // the editor has closed: the release waits for unacked to reach 0
    Clock::time_point deadline{};
};
Flight g_flight;
// The claim this peer has published a notebook under since it took it; cleared when the claim ends.
std::wstring g_pubClaim;
dev::Stats g_stats;

coop::net::Session* Session() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->running() ? s : nullptr;
}

// The notebook's occupancy claim key. Empty, said once, for a notebook with no Key: it has no identity to
// claim or to address a record by, so its edits stay on this peer.
std::wstring ClaimOf(void* notebook) {
    std::wstring claim = DS::ClassifyDeviceActorClaimKey(notebook);
    static bool said = false;
    if (claim.empty() && !said) {
        said = true;
        UE_LOGW("notebook_sync: a notebook with no Key has no claim and no record; its edits stay on this peer");
    }
    return claim;
}

// Publish the notebook's record through the prop record lane: the canonical on the host, an intent on a
// client, which then waits for the host's answer before it lets the claim go.
void PublishRecord(coop::net::Session* s, void* notebook, const std::wstring& key,
                   const std::wstring& claim) {
    if (!coop::prop_save_data::Publish(s, notebook, key)) return;
    ++g_stats.published;
    g_pubClaim = claim;
    if (s->role() == coop::net::Role::Host) return;
    g_flight.key = key;
    g_flight.claim = claim;
    ++g_flight.unacked;
}

// The editor's own commit (the bound handler of the box's OnTextCommitted: focus leaving the box, or the
// game's commit) writes the box into the open page of `text` and reloads the widget. It is the one event
// in which the live text changes, so the lane reads the notebook's `text` array (strings only) around it and
// no box is read. The PRE takes the first look at a notebook: the text it held before this commit is what
// edits are measured from. The POST publishes when `text` now differs from the last text published or
// applied, at most once per kPublishEvery; a change the gate held back goes with the next commit or the close.
sg::Verdict OnCommitPre(const sg::Call& c) {
    if (!Session()) return sg::Verdict::Run;
    void* notebook = N::NotebookOf(c.object);
    if (!notebook || N::IsReadOnly(c.object)) return sg::Verdict::Run;
    const std::wstring claim = ClaimOf(notebook);
    if (claim.empty() || !coop::device_occupancy::LocalHolds(claim.c_str())) return sg::Verdict::Run;
    const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
    if (key.empty() || key == L"None" || g_base.key == key) return sg::Verdict::Run;
    g_base.key = key;
    g_base.claim = claim;
    N::ReadPages(notebook, g_base.pages);
    g_flight = Flight{};
    g_pubClaim.clear();
    return sg::Verdict::Run;
}

void OnCommitPost(const sg::Call& c) {
    const auto now = Clock::now();
    auto* s = Session();
    if (!s || now < g_nextPublish) return;
    void* notebook = N::NotebookOf(c.object);
    if (!notebook || N::IsReadOnly(c.object)) return;
    const std::wstring claim = ClaimOf(notebook);
    const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
    if (claim.empty() || g_base.key != key || !coop::device_occupancy::LocalHolds(claim.c_str())) return;
    std::vector<std::wstring> pages;
    if (!N::ReadPages(notebook, pages) || pages == g_base.pages) return;
    PublishRecord(s, notebook, key, claim);
    g_base.pages = std::move(pages);
    g_nextPublish = now + kPublishEvery;
}

sg::Verdict OnQuitPre(const sg::Call& c) {
    g_closing.Set(N::NotebookOf(c.object));
    return sg::Verdict::Run;
}

void OnQuitPost(const sg::Call& c) {
    void* notebook = g_closing.Get();
    g_closing.Reset();
    auto* s = Session();
    if (notebook && s && !N::IsReadOnly(c.object)) {
        const std::wstring claim = ClaimOf(notebook);
        const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
        if (!claim.empty() && coop::device_occupancy::LocalHolds(claim.c_str()) && !key.empty() && key != L"None") {
            // The body has written the box into the open page; this is the text the editor closed with. It
            // goes while the claim is still held, and the claim waits for the host's answer (ReleaseHold).
            PublishRecord(s, notebook, key, claim);
            if (g_flight.unacked > 0 && g_flight.claim == claim) {
                g_flight.closing = true;
                g_flight.deadline = Clock::now() + kAckBound;
            }
        }
    }
    // The next open of any notebook takes its first look afresh.
    g_nextPublish = {};
    g_base = Baseline{};
}

// A page added to a notebook outside the editor: playerUsedOn with a torn sheet in the hand appends the
// sheet's text to the notebook's and destroys the sheet. The text changes with no editor open, so the
// verb's POST publishes the record when the page count grew; the host's rule takes it from whoever ran the
// verb while nobody holds the notebook. While another peer holds it the verb is cancelled at its PRE on the
// peer that ran it: the open editor's close would overwrite the page on every peer, so it is never applied.
ue_wrap::CachedObjRef g_used;
size_t g_usedPagesBefore = 0;

sg::Verdict OnUsedPre(const sg::Call& c) {
    g_used.Reset();
    std::vector<std::wstring> pages;
    if (!Session() || !N::ReadPages(c.object, pages)) return sg::Verdict::Run;
    const std::wstring claim = DS::ClassifyDeviceActorClaimKey(c.object);
    const uint8_t holder = claim.empty() ? uint8_t{0xFF} : coop::device_occupancy::HolderOf(claim.c_str());
    if (holder != 0xFF && holder != coop::players::Registry::Get().LocalPeerId()) {
        // The player keeps the sheet; the notice is the busy line the devices use, and the deny click.
        static Clock::time_point nextNotice{};
        if (Clock::now() >= nextNotice) {
            nextNotice = Clock::now() + std::chrono::seconds(3);
            coop::peer_action_feed::AnnounceDirect(holder, coop::peer_action_feed::Action::UsingDevice, L"the notebook");
            if (void* local = coop::players::Registry::Get().Local()) coop::prop_sound::PlayDenyClick(local);
            UE_LOGI("notebook_sync: a page was not added to notebook '%ls': slot %u has it open",
                    ue_wrap::prop::GetKeyString(c.object).c_str(), static_cast<unsigned>(holder));
        }
        return sg::Verdict::Cancel;
    }
    g_used.Set(c.object);
    g_usedPagesBefore = pages.size();
    return sg::Verdict::Run;
}

void OnUsedPost(const sg::Call&) {
    void* notebook = g_used.Get();
    g_used.Reset();
    auto* s = Session();
    std::vector<std::wstring> pages;
    if (!notebook || !s || !N::ReadPages(notebook, pages) || pages.size() <= g_usedPagesBefore) return;
    const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
    if (key.empty() || key == L"None") return;
    coop::prop_save_data::Publish(s, notebook, key);
}

// device_occupancy asks this each tick the local claim's release is due. The state of the claim is not
// reset here: ClaimEnded is told when the claim is over, by whatever route.
bool ReleaseHold(const wchar_t* key) {
    if (!g_flight.closing || g_flight.claim != key) return false;
    if (g_flight.unacked > 0 && Clock::now() < g_flight.deadline) {
        ++g_stats.releaseWaits;
        return true;
    }
    if (g_flight.unacked > 0) {
        ++g_stats.releasedByBound;
        UE_LOGW("notebook_sync: the host did not answer the close of '%ls' in %lld s -- releasing its claim anyway",
                key, static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(kAckBound).count()));
    }
    return false;
}

// device_occupancy tells this when the local claim `key` is over: released, lost, the world gone, the
// session ended. Everything kept for that claim goes, so the next claim of the notebook starts fresh.
void ClaimEnded(const wchar_t* key) {
    if (g_pubClaim == key) g_pubClaim.clear();
    if (g_flight.claim == key) g_flight = Flight{};
    if (g_base.claim == key) {
        g_base = Baseline{};
        g_nextPublish = {};
    }
}

// The editor shows a page loaded when it opened. A record applied to the notebook under it leaves the box
// stale, and the box would write that page back at the next commit or quit, so the widget's own page load
// (init) runs again once the apply is done, and the lane measures later edits from what was applied. It runs
// only before this peer's first publish of the claim, so nothing this peer typed has left it yet; what the
// box holds uncommitted at that instant is dropped, and the window is the round trip after the grant.
void ReloadAfterApply(const std::wstring& key) {
    ue_wrap::game_thread::Post([key] {
        void* notebook = coop::prop_key_index::Find(key);
        void* local = coop::players::Registry::Get().Local();
        void* widget = local ? DS::ReadActiveInterface(local) : nullptr;
        if (!notebook || N::NotebookOf(widget) != notebook) return;
        const std::wstring claim = DS::ClassifyDeviceActorClaimKey(notebook);
        if (claim.empty() || g_pubClaim == claim) return;   // its own publish stands, not the applied page
        if (!N::ReloadEditor(widget)) {
            // The record is on `text` and the box still shows the old page: the next commit would write it
            // back. Said once so a run shows it.
            static bool said = false;
            if (!said) {
                said = true;
                UE_LOGW("notebook_sync: the editor's box could not be reloaded after a record for '%ls' applied "
                        "(said once)", key.c_str());
            }
            return;
        }
        if (g_base.key == key) N::ReadPages(notebook, g_base.pages);
    });
}

// The apply gate: a peer that holds the notebook's claim and has published it in this claim applies no
// record for it. Its own edit is the newer state; a record that comes back is the host's answer to one
// of its publishes, or older. Before its first publish it has nothing newer to protect, and a record
// the host sent before the grant (the previous editor's close, overtaken by the grant) applies, with
// the editor's box reloaded from it. Asked more than once for a record that fails to apply, it is safe: the
// count moves only on the branch that drops the record, and a repeated reload is a repeated page load.
bool ApplyGate(void* actor) {
    const std::wstring claim = DS::ClassifyDeviceActorClaimKey(actor);
    if (claim.empty() || !coop::device_occupancy::LocalHolds(claim.c_str())) return true;
    if (g_pubClaim != claim) {
        ReloadAfterApply(ue_wrap::prop::GetKeyString(actor));
        return true;
    }
    if (g_flight.unacked > 0 && g_flight.key == ue_wrap::prop::GetKeyString(actor)) --g_flight.unacked;
    return false;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (!g_commitWatched) g_commitWatched = sg::WatchName(kNotebookCommit, 0, &OnCommitPre, &OnCommitPost);
    if (!g_quitWatched) g_quitWatched = sg::WatchName(kNotebookQuit, 0, &OnQuitPre, &OnQuitPost);
    if (!g_usedWatched) g_usedWatched = sg::WatchName(kNotebookUsedOn, 1, &OnUsedPre, &OnUsedPost);
    if (!g_registered) {
        g_registered = true;
        coop::prop_record_gates::RegisterIntentGate(L"prop_notebook_C", &AcceptsClientRecord);
        coop::prop_record_gates::RegisterApplyGate(L"prop_notebook_C", &ApplyGate);
        coop::device_occupancy::RegisterReleaseHold(&ReleaseHold);
        coop::device_occupancy::RegisterClaimEnded(&ClaimEnded);
    }
}

bool AcceptsClientRecord(void* actor, uint8_t senderSlot) {
    const std::wstring claim = DS::ClassifyDeviceActorClaimKey(actor);
    if (claim.empty()) return true;   // no identity to hold: nothing for the claim to say
    const uint8_t holder = coop::device_occupancy::HolderOf(claim.c_str());
    if (holder == 0xFF) { ++g_stats.acceptedFree; return true; }   // a free notebook is anyone's to change
    if (holder == senderSlot) { ++g_stats.acceptedHeld; return true; }
    ++g_stats.refused;
    UE_LOGW("notebook_sync: record for notebook '%ls' from slot %u refused -- slot %u holds its claim",
            ue_wrap::prop::GetKeyString(actor).c_str(), static_cast<unsigned>(senderSlot),
            static_cast<unsigned>(holder));
    return false;
}


namespace dev {

bool BaselineTaken(void* notebook) {
    const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
    return !key.empty() && g_base.key == key;
}

Stats StatsNow() { return g_stats; }

}  // namespace dev

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_nextPublish = {};
    g_base = Baseline{};
    g_flight = Flight{};
    g_pubClaim.clear();
    g_closing.Reset();
    g_used.Reset();
    g_stats = dev::Stats{};
}

}  // namespace coop::notebook_sync
