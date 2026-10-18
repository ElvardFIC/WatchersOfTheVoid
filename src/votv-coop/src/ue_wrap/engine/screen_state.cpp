// ue_wrap/engine/screen_state.cpp -- see ue_wrap/engine/screen_state.h.

#include "ue_wrap/engine/screen_state.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/world/world_singleton.h"

#include <cstdint>

namespace ue_wrap::engine {
namespace {

namespace R = reflection;
namespace P = profile;

// The live instances of `cls` (its class default left out), counted into `live` when given; returns how many are on
// the viewport (none when IsInViewport did not resolve).
int CountOnScreen(const wchar_t* cls, void* inViewportFn, int* live) {
    int onScreen = 0;
    for (void* w : R::FindObjectsByClass(cls)) {
        if (!w || !R::IsLive(w) || R::NameStartsWith(R::NameOf(w), L"Default__")) continue;
        if (live) ++*live;
        if (!inViewportFn) continue;
        ParamFrame f(inViewportFn);
        if (f.valid() && Call(w, f) && f.Get<bool>(L"ReturnValue")) ++onScreen;
    }
    return onScreen;
}

}  // namespace

ScreenState ReadScreenState() {
    ScreenState s;
    void* userWidget = R::FindClass(P::name::UserWidgetClass);
    void* inViewport = userWidget ? R::FindFunction(userWidget, L"IsInViewport") : nullptr;
    s.blackOnScreen = CountOnScreen(L"blackScreen_C", inViewport, &s.blackLive);
    s.blurOnScreen = CountOnScreen(L"ui_bloodLossBlur_C", inViewport, nullptr);
    void* pcm = world_singleton::Find(P::name::PlayerCameraManagerClass);
    if (!pcm || !R::IsLive(pcm)) return s;
    void* cls = R::ClassOf(pcm);
    auto* base = static_cast<uint8_t*>(pcm);
    const int32_t ownerOff = R::FindPropertyOffset(cls, L"PCOwner");
    void* pc = ownerOff >= 0 ? *reinterpret_cast<void* const*>(base + ownerOff) : nullptr;
    void* getViewTarget = (pc && R::IsLive(pc)) ? R::FindDispatchFunctionCached(R::ClassOf(pc), L"GetViewTarget") : nullptr;
    if (getViewTarget) {
        ParamFrame f(getViewTarget);
        if (f.valid() && Call(pc, f)) {
            void* target = f.Get<void*>(L"ReturnValue");
            if (target && R::IsLive(target)) s.viewTarget = R::ClassNameOf(target);
        }
    }
    return s;
}

}  // namespace ue_wrap::engine
