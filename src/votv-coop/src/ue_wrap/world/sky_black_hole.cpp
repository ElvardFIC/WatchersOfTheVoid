#include "ue_wrap/world/skysphere.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include <cstring>

namespace ue_wrap::skysphere {
namespace R = reflection;
namespace {
void* SkyObject(void* actor, const wchar_t* name) {
    const int32_t off = actor ? R::FindPropertyOffset(R::ClassOf(actor), name) : -1;
    void* result = nullptr;
    if (off >= 0) std::memcpy(&result, static_cast<const uint8_t*>(actor) + off, sizeof(result));
    return result && R::IsLive(result) ? result : nullptr;
}
bool SkyBool(void* actor, const wchar_t* name, bool& value) {
    int32_t off = -1; uint8_t mask = 0;
    if (!actor || !R::FindBoolProperty(R::ClassOf(actor), name, off, mask) || off < 0 || !mask) return false;
    value = (static_cast<const uint8_t*>(actor)[off] & mask) != 0;
    return true;
}
bool BlackHoleReady(void* sky, void*& light) {
    light = SkyObject(sky, L"light_blackhole");
    return light && SkyObject(sky, L"daylightCycleObject") && SkyObject(sky, L"sky") &&
           SkyObject(sky, L"dynmat_sun") && SkyObject(sky, L"dynmat_bhole");
}
}

bool ReadBlackHole(bool& hidden, bool& lightVisible) {
    void* sky = Sky(); void* light = nullptr;
    bool h = false, v = false;
    if (!sky || !BlackHoleReady(sky, light) || !SkyBool(sky, L"skyHidden", h) ||
        !SkyBool(light, L"bVisible", v)) return false;
    hidden = h; lightVisible = v; return true;
}

bool ApplyBlackHole(bool hidden, bool lightVisible) {
    void* sky = Sky(); void* light = nullptr;
    bool h = false, v = false;
    if (!sky || !BlackHoleReady(sky, light) || !SkyBool(sky, L"skyHidden", h) ||
        !SkyBool(light, L"bVisible", v)) return false;
    CachedObjRef skyRef, lightRef; skyRef.Set(sky); lightRef.Set(light);
    if (h != hidden) {
        ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(sky), L"skyVisibility"));
        if (!f.valid() || !f.Set<bool>(L"bNewHidden", hidden) || !Call(sky, f) ||
            !skyRef.Get() || !lightRef.Get()) return false;
    }
    if (v != lightVisible) {
        ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(light), L"SetVisibility"));
        if (!f.valid() || !f.Set<bool>(L"bNewVisibility", lightVisible) ||
            !f.Set<bool>(L"bPropagateToChildren", false) || !Call(light, f) ||
            !skyRef.Get() || !lightRef.Get()) return false;
    }
    return SkyBool(sky, L"skyHidden", h) && SkyBool(light, L"bVisible", v) && h == hidden && v == lightVisible;
}

} // namespace ue_wrap::skysphere
