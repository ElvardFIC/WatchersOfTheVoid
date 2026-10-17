// ue_wrap/world/bad_sun.cpp -- see ue_wrap/world/bad_sun.h.

#include "ue_wrap/world/bad_sun.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine_pawn.h"
#include "ue_wrap/world/active_events.h"
#include "ue_wrap/world/daynightcycle.h"
#include "ue_wrap/world/world_instances.h"
#include "ue_wrap/world/world_singleton.h"

#include <cstdint>

namespace ue_wrap::bad_sun {
namespace {

namespace R = ue_wrap::reflection;

constexpr const wchar_t* kClassName = L"badSun_C";
constexpr const wchar_t* kSpawnFn = L"Spawn Bad Sun";  // on mainGamemode_C

int32_t g_offWidget = -1;  // badSun_C::widget (ui_badSun_C object pointer)
bool g_resolveFailed = false;

int32_t g_offGmCopy = -1;  // mainGamemode_C::badSun (badSun_C object pointer)
bool g_gmCopyFailed = false;

// The gamemode's own member, found once off its class: the class's layout is the same in every world.
bool EnsureGmCopy(void* gamemode) {
    if (g_offGmCopy >= 0) return true;
    if (g_gmCopyFailed) return false;
    g_offGmCopy = R::FindPropertyOffset(R::ClassOf(gamemode), L"badSun");
    if (g_offGmCopy >= 0) {
        UE_LOGI("bad_sun: resolved (mainGamemode.badSun=0x%X)", g_offGmCopy);
        return true;
    }
    g_gmCopyFailed = true;  // a member missing from a loaded class does not appear later
    UE_LOGW("bad_sun: mainGamemode_C.badSun did not resolve -- latched OFF; game version mismatch?");
    return false;
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

int CountLive(void** copies, int cap) {
    if (copies)
        for (int i = 0; i < cap; ++i) copies[i] = nullptr;
    if (object_index::DrainAll() == SIZE_MAX) return -1;
    void* out[kMaxLive] = {};
    const int32_t n = world_instances::Find(kClassName, out, kMaxLive);
    if (copies)
        for (int i = 0; i < cap && i < n; ++i) copies[i] = out[i];
    return static_cast<int>(n);
}

bool ReadActive(void* copy, bool& active) {
    active = false;
    if (!copy || !EnsureResolved()) return false;
    active = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(copy) + g_offWidget) != nullptr;
    return true;
}

bool ReadIsSender(void* copy, bool& sender) {
    sender = false;
    if (!copy) return false;
    active_events::Senders s{};
    if (!active_events::EnsureResolved() || !active_events::ReadSenders(s)) return false;
    for (int32_t i = 0; i < s.num; ++i)
        if (s.data[i] == copy) sender = true;
    return true;
}

bool ReadGamemodeCopy(void* gamemode, bool& present) {
    present = false;
    void* gm = gamemode ? gamemode : world_singleton::Gamemode();
    if (!gm || !EnsureGmCopy(gm)) return false;
    void* copy = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(gm) + g_offGmCopy);
    present = copy && R::IsLive(copy);
    return true;
}

bool HasCycle() { return daynightcycle::Cycle() != nullptr; }

bool CallSpawn() {
    void* gm = world_singleton::Gamemode();
    void* fn = gm ? R::FindDispatchFunctionCached(R::ClassOf(gm), kSpawnFn) : nullptr;
    if (!fn) return false;
    ParamFrame f(fn);
    if (!f.valid()) return false;
    return Call(gm, f);
}

bool DestroyInactive(void* copy) {
    bool active = false;
    if (!copy || !ReadActive(copy, active) || active) return false;
    return engine::DestroyActor(copy);
}

}  // namespace ue_wrap::bad_sun
