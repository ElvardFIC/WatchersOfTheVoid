// ue_wrap/world/bad_sun.cpp -- see ue_wrap/world/bad_sun.h.

#include "ue_wrap/world/bad_sun.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/world/active_events.h"
#include "ue_wrap/world/world_instances.h"

#include <cstdint>

namespace ue_wrap::bad_sun {
namespace {

namespace R = ue_wrap::reflection;

constexpr const wchar_t* kClassName = L"badSun_C";

int32_t g_offWidget = -1;  // badSun_C::widget (ui_badSun_C object pointer)
bool g_resolveFailed = false;

constexpr int32_t kCap = 8;  // a second copy is a finding, and more than a few cannot exist

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
    void* out[kCap] = {};
    const int32_t n = world_instances::Find(kClassName, out, kCap);
    if (first) *first = n > 0 ? out[0] : nullptr;
    return static_cast<int>(n);
}

bool IsActive(void* copy, bool& active) {
    active = false;
    if (!copy || !EnsureResolved()) return false;
    active = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(copy) + g_offWidget) != nullptr;
    return true;
}

bool IsActiveEventSender(void* copy, bool& sender) {
    sender = false;
    if (!copy) return false;
    active_events::Senders s{};
    if (!active_events::EnsureResolved() || !active_events::ReadSenders(s)) return false;
    for (int32_t i = 0; i < s.num; ++i)
        if (s.data[i] == copy) sender = true;
    return true;
}

}  // namespace ue_wrap::bad_sun
