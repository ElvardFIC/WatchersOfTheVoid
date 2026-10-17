// ue_wrap/world/bad_sun.cpp -- see ue_wrap/world/bad_sun.h.

#include "ue_wrap/world/bad_sun.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/world/active_events.h"

#include <cstdint>

namespace ue_wrap::bad_sun {
namespace {

namespace R = ue_wrap::reflection;

constexpr const wchar_t* kClassName = L"badSun_C";

int32_t g_offWidget = -1;  // badSun_C::widget (ui_badSun_C object pointer)
bool g_resolveFailed = false;

struct Tally {
    int   n = 0;
    void* first = nullptr;
};

void CountOne(void* ctx, void* obj, int32_t /*index*/) {
    if (!R::IsLive(obj)) return;
    auto* t = static_cast<Tally*>(ctx);
    if (!t->first) t->first = obj;
    ++t->n;
}

}  // namespace

bool EnsureResolved() {
    if (g_offWidget >= 0) return true;
    if (g_resolveFailed) return false;
    void* cls = object_index::ClassByName(kClassName);
    if (!cls) return false;  // not loaded: no copy exists yet
    g_offWidget = R::FindPropertyOffset(cls, L"widget");
    if (g_offWidget >= 0) {
        UE_LOGI("bad_sun: resolved (widget=0x%X)", g_offWidget);
        return true;
    }
    g_resolveFailed = true;  // a member missing from a loaded class does not appear later
    UE_LOGW("bad_sun: badSun_C.widget did not resolve on a loaded class -- latched OFF; game version mismatch?");
    return false;
}

int CountLive(void** first) {
    if (first) *first = nullptr;
    if (object_index::DrainAll() == SIZE_MAX) return -1;
    void* cls = object_index::ClassByName(kClassName);
    if (!cls) return 0;
    Tally t;
    object_index::ForEachInstance(cls, &CountOne, &t);
    if (first) *first = t.first;
    return t.n;
}

bool IsActive(void* copy) {
    if (!copy || !EnsureResolved()) return false;
    return *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(copy) + g_offWidget) != nullptr;
}

bool IsActiveEventSender(void* copy) {
    if (!copy) return false;
    active_events::Senders s{};
    if (!active_events::EnsureResolved() || !active_events::ReadSenders(s)) return false;
    for (int32_t i = 0; i < s.num; ++i)
        if (s.data[i] == copy) return true;
    return false;
}

}  // namespace ue_wrap::bad_sun
