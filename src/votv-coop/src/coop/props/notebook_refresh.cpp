// Notebook UI writes the prop directly; it never calls the prop's empty upd().
// Publish the existing save record after native commit/quit/page operations.
#include "coop/props/notebook_refresh.h"
#include "coop/net/session.h"
#include "coop/props/prop_save_data.h"
#include "coop/props/join_membership_sweep.h"
#include "coop/props/active_drive.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include <array>
#include <atomic>
#include <cstring>
#include <string>

namespace coop::props::notebook_refresh {
namespace {
namespace R = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
std::atomic<net::Session*> g_session{nullptr};
struct Pending {
    ue_wrap::CachedObjRef actor;
    std::wstring key;
    uint64_t due = 0, version = 0;
    unsigned tries = 0;
};
std::array<Pending, 32> g_pending{};
uint64_t g_epoch = 0, g_version = 0;
uint32_t g_world = 0;
bool g_ticking = false;
void World() {
    const auto now = ue_wrap::world_identity::Generation();
    if (now == g_world) return;
    ++g_epoch; g_world = now; g_pending = {};
}
bool Eligible(void* actor) {
    auto* s = g_session.load();
    if (!s || !s->connected() || !actor || !R::IsLive(actor)) return false;
    if (s->role() == net::Role::Client &&
        (!net_pump::IsInAnnouncedWorld(actor) || !join_membership_sweep::HasLoadTailQuiesced())) return false;
    // FBoolProperty is resolved by name: uneditable story pages and generated pages
    // with skipTextSave must not publish a record which cannot carry their text.
    for (const auto* name : {L"uneditable", L"skipTextSave"}) {
        int32_t off = -1; uint8_t mask = 0;
        if (!R::FindBoolProperty(R::ClassOf(actor), name, off, mask) ||
            (static_cast<const uint8_t*>(actor)[off] & mask)) return false;
    }
    return prop_save_data::Covers(actor);
}
void Mark(void* actor) {
    World();
    if (!Eligible(actor)) return;
    const auto key = ue_wrap::prop::GetInteractableKeyString(actor);
    if (key.empty() || key == L"None" || key.size() > 31) return;
    Pending* free = nullptr;
    for (auto& p : g_pending) {
        if (p.actor.Is(actor)) { p.key = key; p.version = ++g_version; p.due = 0; p.tries = 0; return; }
        if (!free && !p.actor.Get()) free = &p;
    }
    if (!free) { UE_LOGW("notebook: pending record queue full"); return; }
    *free = {}; free->actor.Set(actor); free->key = key; free->version = ++g_version;
}
void* Clipboard(void* ui) {
    if (!ui || !R::IsLive(ui)) return nullptr;
    const int off = R::FindPropertyOffset(R::ClassOf(ui), L"clipboard");
    if (off < 0) return nullptr;
    void* actor = nullptr; std::memcpy(&actor, static_cast<const uint8_t*>(ui) + off, sizeof(actor));
    return actor;
}
sg::Verdict BeforeQuit(const sg::Call& c) {
    // quit() clears clipboard before its post-hook. Keep only a slot/serial/world
    // reference now, capture the finished record from Tick after this stack unwinds.
    if (!c.fromOurCode) Mark(Clipboard(c.object));
    return sg::Verdict::Run;
}
void AfterUI(const sg::Call& c) { if (!c.fromOurCode) Mark(Clipboard(c.object)); }
void AfterAppend(const sg::Call& c) { if (!c.fromOurCode) Mark(c.object); }
}
void Install(net::Session* session) {
    if (g_session.load() != session) { ++g_epoch; g_pending = {}; }
    g_session.store(session); World();
    sg::WatchClassName(L"ui_notebook_C", L"quit", 0x4E4F0001, &BeforeQuit, nullptr);
    // No publication per keypress: TextChanged only changes scroll position.
    // Page tear/crumple end through quit, and append has its own prop hook below.
    sg::WatchClassName(L"ui_notebook_C",
        L"BndEvt__MultiLineEditableText_86_K2Node_ComponentBoundEvent_5_OnMultiLineEditableTextCommittedEvent__DelegateSignature",
        0x4E4F0002, nullptr, &AfterUI);
    sg::WatchClassName(L"prop_notebook_C", L"playerUsedOn", 0x4E4F0003, nullptr, &AfterAppend);
}
void Tick() {
    auto* s = g_session.load();
    if (!s || !s->connected() || g_ticking) return;
    World();
    struct Guard { Guard(){g_ticking=true;} ~Guard(){g_ticking=false;} } guard;
    const auto epoch = g_epoch, now = active_drive::NowMs();
    unsigned budget = 2;
    for (auto& row : g_pending) {
        if (row.key.empty()) continue;
        void* actor = row.actor.Get();
        if (!actor || ue_wrap::prop::GetInteractableKeyString(actor) != row.key) { row = {}; continue; }
        if (row.due > now || !budget) continue;
        if (!Eligible(actor)) continue;
        if (!s->AnyWorldReadyPeer()) continue;
        --budget;
        const auto copy = row;
        ++row.tries; row.due = now + (300u << (row.tries > 4 ? 4 : row.tries));
        const bool ok = prop_save_data::Publish(s, actor, copy.key);
        if (epoch != g_epoch || g_world != ue_wrap::world_identity::Generation()) return;
        // Native getData can reenter or clear this queue. A new edit must survive.
        if (row.version != copy.version) continue;
        if (ok) row = {};
        else if (row.tries >= 8) { UE_LOGW("notebook: record retry exhausted for '%ls'", copy.key.c_str()); row = {}; }
    }
}
void OnDisconnect() { ++g_epoch; g_session.store(nullptr); g_pending = {}; }
}
