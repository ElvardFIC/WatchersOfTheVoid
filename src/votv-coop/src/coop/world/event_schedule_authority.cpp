#include "coop/world/event_schedule_authority.h"

#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"

#include <atomic>
#include <algorithm>
#include <vector>

namespace coop::event_schedule_authority {
namespace {
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
std::atomic<coop::net::Session*> g_session{nullptr};
constexpr int kTag = 0x45565343; // EVSC
bool g_watched = false;
ue_wrap::CachedObjRef g_class;
int32_t g_offset = -1;
uint32_t g_serial=0,g_world=0;
uint64_t g_epoch=0;
struct NameArray { R::FName* data; int32_t num; int32_t max; };
struct HeldSchedule {
    ue_wrap::CachedObjRef slot,cls;
    NameArray saved{};
    int32_t offset = -1;
    uint32_t world=0;
};
std::vector<HeldSchedule> g_held;

SG::Verdict OnSetTimePre(const SG::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running() || s->role() != coop::net::Role::Client || !call.object)
        return SG::Verdict::Run;
    if(g_serial!=coop::net::session_serial::Current() || g_world!=ue_wrap::world_identity::Generation())return SG::Verdict::Run;
    const auto epoch=g_epoch;
    ue_wrap::CachedObjRef slotRef,classRef;slotRef.Set(call.object);
    // Do not bypass fromOurCode: a host-time sample can call a native clock body,
    // which then invokes settime inside that reflected call's scope.
    void* cls = R::ClassOf(call.object);
    if (!cls) return SG::Verdict::Run;
    classRef.Set(cls);
    const auto current=[&]{return epoch==g_epoch && s==g_session.load() &&
        g_serial==coop::net::session_serial::Current() && s->running() && s->role()==coop::net::Role::Client &&
        g_world==ue_wrap::world_identity::Generation() &&
        slotRef.Get() && classRef.Get() && R::ClassOf(call.object)==classRef.Get();};
    if (!g_class.Is(cls)) {
        const auto offset=R::FindPropertyOffset(cls, L"allEvents");
        if(!current())return SG::Verdict::Run;
        g_class=classRef;
        g_offset = offset;
        if (g_offset < 0)
            UE_LOGW("event_schedule: saveSlot.allEvents missing; client schedule cannot be held");
    }
    if (g_offset < 0 || !current()) return SG::Verdict::Run;
    auto* all = reinterpret_cast<NameArray*>(static_cast<uint8_t*>(call.object) + g_offset);
    if (all->num == 0) return SG::Verdict::Run;
    if (!all->data || all->num < 0 || all->num > 100000 || all->max < all->num) {
        UE_LOGW("event_schedule: invalid allEvents header; leaving it untouched");
        return SG::Verdict::Run;
    }
    // Usually there is one live slot. Keep each identity separately so a temporary
    // slot's settime or a world transition cannot strand another held schedule.
    g_held.erase(std::remove_if(g_held.begin(), g_held.end(), [](const auto& h) {
        return !h.slot.Alive();
    }), g_held.end());
    auto it = std::find_if(g_held.begin(), g_held.end(), [&](const auto& h) {
        return h.slot.Is(call.object);
    });
    if (it == g_held.end()) { g_held.emplace_back(); it = g_held.end() - 1; }
    it->slot.Set(call.object);
    it->cls=classRef;
    it->saved = *all;
    it->offset = g_offset;
    it->world=g_world;
    // Suppress the walk, not just runEvent: settime otherwise appends a cancelled
    // row to passEvents, and the client's replay dedupe would eat the host's fire.
    // Native boot rebuilds allEvents from its table on each world load.
    all->num = 0;
    UE_LOGI("event_schedule: client allEvents held (%d -> 0) before settime", it->saved.num);
    return SG::Verdict::Run;
}
}

void Install(coop::net::Session* session) {
    if(g_session.load()!=session || g_serial!=coop::net::session_serial::Current() ||
       g_world!=ue_wrap::world_identity::Generation())OnDisconnect();
    g_serial=coop::net::session_serial::Current();
    g_world=ue_wrap::world_identity::Generation();
    g_session.store(session, std::memory_order_release);
    if (!g_watched)
        g_watched = SG::WatchClassName(L"saveSlot_C", L"settime", kTag, &OnSetTimePre, nullptr);
}

void OnDisconnect() {
    const auto retirement=++g_epoch;
    g_session.store(nullptr, std::memory_order_release);
    const auto held=std::move(g_held);g_held.clear();
    g_class.Reset();g_offset=-1;
    // A recycled UObject or replaced/reallocated array must not inherit stale rows.
    for (const auto& h : held) {
        if(retirement!=g_epoch)return;
        if (void* slot = h.slot.Get(); slot && h.saved.num > 0 && h.offset >= 0 &&
            h.world==ue_wrap::world_identity::Generation() &&
            h.cls.Get() && R::ClassOf(slot)==h.cls.Get()) {
            const auto offset=R::FindPropertyOffset(h.cls.Get(),L"allEvents");
            if(retirement!=g_epoch)return;
            if(offset!=h.offset || !h.slot.Get() || !h.cls.Get() || R::ClassOf(slot)!=h.cls.Get() ||
               h.world!=ue_wrap::world_identity::Generation())continue;
            auto* all = reinterpret_cast<NameArray*>(static_cast<uint8_t*>(slot) + h.offset);
            if (all->num == 0 && all->data == h.saved.data && all->max == h.saved.max) {
                all->num = h.saved.num;
                UE_LOGI("event_schedule: restored %d rows for the local scheduler", h.saved.num);
            }
        }
    }
}
}
