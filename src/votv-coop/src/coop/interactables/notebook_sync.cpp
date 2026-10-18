// coop/interactables/notebook_sync.cpp -- see coop/interactables/notebook_sync.h.

#include "coop/interactables/notebook_sync.h"

#include "coop/interactables/device_occupancy.h"
#include "coop/net/session.h"
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
#include <vector>

namespace coop::notebook_sync {
namespace {

namespace sg = ue_wrap::script_gate;
namespace N  = ue_wrap::notebook;
namespace DS = ue_wrap::device_screen;

UE_FACT_FN(kNotebookGraph, "notebook_sync", L"ui_notebook_C", L"ExecuteUbergraph_ui_notebook", Declared);
UE_FACT_FN(kNotebookQuit, "notebook_sync", L"ui_notebook_C", L"quit", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_graphWatched = false;
bool g_quitWatched = false;

// The graph's POST fires on every event of the widget, a keystroke included, so the work behind it is
// gated by time first: the page text is read and compared at most once per kPublishEvery.
constexpr auto kPublishEvery = std::chrono::seconds(2);
std::chrono::steady_clock::time_point g_nextExamine{};
// What the editing peer last published, or read when it began: the text a later examine compares to.
struct Baseline {
    std::wstring key;
    std::vector<std::wstring> pages;
};
Baseline g_base;
// The notebook the editor was open on when quit began; quit clears the widget's `clipboard` in its own
// body, so only a watch before the body can name it.
ue_wrap::CachedObjRef g_closing;

coop::net::Session* Session() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->running() ? s : nullptr;
}

// The notebook's occupancy claim, held by this peer.
bool HoldsClaim(void* notebook) {
    const std::wstring claim = DS::ClassifyDeviceActorClaimKey(notebook);
    return !claim.empty() && coop::device_occupancy::LocalHolds(claim.c_str());
}

// Publish the notebook's record through the prop record lane: the canonical on the host, an intent
// on a client. The pages are remembered as the last published whatever the lane did with them: a
// refusal is the lane's to log and retry, and only a changed text publishes again.
void PublishRecord(coop::net::Session* s, void* notebook, const std::wstring& key,
                   const std::vector<std::wstring>& pages) {
    coop::prop_save_data::Publish(s, notebook, key);
    g_base.key = key;
    g_base.pages = pages;
}

void OnGraphPost(const sg::Call& c) {
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextExamine) return;
    g_nextExamine = now + kPublishEvery;
    auto* s = Session();
    if (!s) return;
    void* notebook = N::NotebookOf(c.object);
    if (!notebook || N::IsReadOnly(c.object) || !HoldsClaim(notebook)) return;
    const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
    if (key.empty() || key == L"None") return;
    if (g_base.key != key) {
        // The first look at this notebook: the text it opened with is what the edits are measured from.
        g_base.key = key;
        N::ReadPages(notebook, g_base.pages);
        return;
    }
    std::vector<std::wstring> pages;
    if (!N::CommitEditorPage(c.object, pages) || pages == g_base.pages) return;
    PublishRecord(s, notebook, key, pages);
}

sg::Verdict OnQuitPre(const sg::Call& c) {
    g_closing.Set(N::NotebookOf(c.object));
    return sg::Verdict::Run;
}

void OnQuitPost(const sg::Call& c) {
    void* notebook = g_closing.Get();
    g_closing.Reset();
    g_nextExamine = {};
    g_base = Baseline{};
    auto* s = Session();
    if (!notebook || !s || N::IsReadOnly(c.object) || !HoldsClaim(notebook)) return;
    const std::wstring key = ue_wrap::prop::GetKeyString(notebook);
    std::vector<std::wstring> pages;
    if (key.empty() || key == L"None" || !N::ReadPages(notebook, pages)) return;
    // The body has written the box into the open page; this is the text the editor closed with, sent
    // before the pump tick that sees the interface leave and releases the claim.
    PublishRecord(s, notebook, key, pages);
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (!g_graphWatched) g_graphWatched = sg::WatchName(kNotebookGraph, 0, nullptr, &OnGraphPost);
    if (!g_quitWatched) g_quitWatched = sg::WatchName(kNotebookQuit, 0, &OnQuitPre, &OnQuitPost);
}

bool AcceptsClientRecord(void* actor, uint8_t senderSlot) {
    if (!N::IsNotebook(actor)) return true;
    const std::wstring claim = DS::ClassifyDeviceActorClaimKey(actor);
    const uint8_t holder = claim.empty() ? uint8_t{0xFF} : coop::device_occupancy::HolderOf(claim.c_str());
    const uint8_t last = claim.empty() ? uint8_t{0xFF} : coop::device_occupancy::LastHolderOf(claim.c_str());
    if (holder == senderSlot || (holder == 0xFF && last == senderSlot)) return true;
    UE_LOGW("notebook_sync: record for notebook '%ls' from slot %u refused -- the claim is held by "
            "slot %u, last held by slot %u", ue_wrap::prop::GetKeyString(actor).c_str(),
            static_cast<unsigned>(senderSlot), static_cast<unsigned>(holder), static_cast<unsigned>(last));
    return false;
}

bool LocalEditorHolds(void* actor) {
    return N::IsNotebook(actor) && HoldsClaim(actor);
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_nextExamine = {};
    g_base = Baseline{};
    g_closing.Reset();
}

}  // namespace coop::notebook_sync
