// coop/interactables/notebook_sync.cpp -- see coop/interactables/notebook_sync.h.

#include "coop/interactables/notebook_sync.h"

#include "coop/interactables/device_occupancy.h"
#include "coop/net/session.h"
#include "coop/props/prop_record_gates.h"
#include "coop/props/prop_save_data.h"
#include "ue_wrap/actors/notebook.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
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

UE_FACT_FN(kNotebookGraph, "notebook_sync", L"ui_notebook_C", L"ExecuteUbergraph_ui_notebook", Declared);
UE_FACT_FN(kNotebookQuit, "notebook_sync", L"ui_notebook_C", L"quit", Declared);
UE_FACT_FN(kNotebookUsedOn, "notebook_sync", L"prop_notebook_C", L"playerUsedOn", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_graphWatched = false;
bool g_quitWatched = false;
bool g_usedWatched = false;
bool g_registered = false;

// The graph's POST fires on every event of the widget, a keystroke included, so the work behind it is
// gated by time first: the page text is read and compared at most once per kPublishEvery.
constexpr auto kPublishEvery = std::chrono::seconds(2);
// A client lets go of the notebook's claim only after the host has answered its last publish; if the answer
// does not come in this long it lets go anyway.
constexpr auto kAckBound = std::chrono::seconds(10);
Clock::time_point g_nextExamine{};
// What the editing peer last published, or read when it began: the text a later examine compares to.
struct Baseline {
    std::wstring key;
    std::vector<std::wstring> pages;
};
Baseline g_base;
// The notebook the editor was open on when quit began; quit clears the widget's `clipboard` in its own
// body, so only a watch before the body can name it.
ue_wrap::CachedObjRef g_closing;

// CLIENT: the publishes the host has not answered yet. The lane republishes every intent it takes to
// every peer, the author included, so one canonical record for the notebook comes back per publish.
struct Flight {
    std::wstring key;
    std::wstring claim;
    int unacked = 0;
    bool closing = false;       // the editor has closed: the release waits for unacked to reach 0
    Clock::time_point deadline{};
};
Flight g_flight;
Stats g_stats;

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

bool HoldsClaim(void* notebook) {
    const std::wstring claim = ClaimOf(notebook);
    return !claim.empty() && coop::device_occupancy::LocalHolds(claim.c_str());
}

// Publish the notebook's record through the prop record lane: the canonical on the host, an intent on a
// client, which then waits for the host's answer before it lets the claim go.
void PublishRecord(coop::net::Session* s, void* notebook, const std::wstring& key,
                   const std::wstring& claim) {
    if (!coop::prop_save_data::Publish(s, notebook, key) || s->role() == coop::net::Role::Host) return;
    g_flight.key = key;
    g_flight.claim = claim;
    ++g_flight.unacked;
}

void OnGraphPost(const sg::Call& c) {
    const auto now = Clock::now();
    if (now < g_nextExamine) return;
    g_nextExamine = now + kPublishEvery;
    auto* s = Session();
    if (!s) return;
    void* notebook = N::NotebookOf(c.object);
    if (!notebook || N::IsReadOnly(c.object)) return;
    const std::wstring claim = ClaimOf(notebook);
    if (claim.empty() || !coop::device_occupancy::LocalHolds(claim.c_str())) return;
    const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
    if (key.empty() || key == L"None") return;
    if (g_base.key != key) {
        // The first look at this notebook: the text it opened with is what the edits are measured from.
        g_base.key = key;
        N::ReadPages(notebook, g_base.pages);
        g_flight = Flight{};
        return;
    }
    std::vector<std::wstring> pages;
    if (!N::CommitEditorPage(c.object, pages) || pages == g_base.pages) return;
    PublishRecord(s, notebook, key, claim);
    g_base.pages = std::move(pages);
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
    g_nextExamine = {};
    g_base = Baseline{};
}

// A page added to a notebook outside the editor: playerUsedOn with a torn sheet in the hand appends the
// sheet's text to the notebook's and destroys the sheet. The text changed with no editor open and no
// claim held, so the verb's POST publishes the record when the page count grew; the host's rule takes
// it from whoever ran the verb while nobody holds the notebook.
ue_wrap::CachedObjRef g_used;
size_t g_usedPagesBefore = 0;

sg::Verdict OnUsedPre(const sg::Call& c) {
    std::vector<std::wstring> pages;
    if (N::ReadPages(c.object, pages)) {
        g_used.Set(c.object);
        g_usedPagesBefore = pages.size();
    } else {
        g_used.Reset();
    }
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

// device_occupancy asks this each tick the local claim's release is due.
bool ReleaseHold(const wchar_t* key) {
    if (!g_flight.closing || g_flight.claim != key) return false;
    if (g_flight.unacked > 0 && Clock::now() < g_flight.deadline) return true;
    if (g_flight.unacked > 0)
        UE_LOGW("notebook_sync: the host did not answer the close of '%ls' in %lld s -- releasing its claim anyway",
                key, static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(kAckBound).count()));
    g_flight = Flight{};
    return false;
}

// The apply gate: a peer that holds the notebook's claim applies no record for it. Its own edit is the
// newer state; a record that comes back is the host's answer to one of its publishes, or older.
bool ApplyGate(void* actor) {
    if (!HoldsClaim(actor)) return true;
    if (g_flight.unacked > 0 && g_flight.key == ue_wrap::prop::GetKeyString(actor)) --g_flight.unacked;
    return false;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (!g_graphWatched) g_graphWatched = sg::WatchName(kNotebookGraph, 0, nullptr, &OnGraphPost);
    if (!g_quitWatched) g_quitWatched = sg::WatchName(kNotebookQuit, 0, &OnQuitPre, &OnQuitPost);
    if (!g_usedWatched) g_usedWatched = sg::WatchName(kNotebookUsedOn, 1, &OnUsedPre, &OnUsedPost);
    if (!g_registered) {
        g_registered = true;
        coop::prop_record_gates::RegisterIntentGate(L"prop_notebook_C", &AcceptsClientRecord);
        coop::prop_record_gates::RegisterApplyGate(L"prop_notebook_C", &ApplyGate);
        coop::device_occupancy::RegisterReleaseHold(&ReleaseHold);
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


bool BaselineTaken(void* notebook) {
    const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
    return !key.empty() && g_base.key == key;
}

Stats StatsNow() { return g_stats; }

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_nextExamine = {};
    g_base = Baseline{};
    g_flight = Flight{};
    g_closing.Reset();
    g_used.Reset();
    g_stats = Stats{};
}

}  // namespace coop::notebook_sync
