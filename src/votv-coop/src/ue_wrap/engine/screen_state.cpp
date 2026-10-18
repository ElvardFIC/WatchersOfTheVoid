// ue_wrap/engine/screen_state.cpp -- see ue_wrap/engine/screen_state.h.

#include "ue_wrap/engine/screen_state.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/sdk_profile_names.h"
#include "ue_wrap/world/world_singleton.h"

#include <cstdint>
#include <vector>

namespace ue_wrap::engine {
namespace {

namespace R = reflection;
namespace P = profile;
namespace OI = object_index;

struct Counts {
    int live = 0;        // live instances, on the viewport or detached
    int onScreen = 0;    // of those, the ones IsInViewport answered true for
    int unreadable = 0;  // of those, the ones it could not be asked about
};

// The live instances of the class named `name`, counted from the object index (the caller has drained it). A slot
// that is dying or not yet readable and a class default are left out; the list is taken before any call, since a
// call runs engine code the index's callback must not. An instance is UNREADABLE when the question could not be asked
// (the function did not resolve, its frame is not valid, the call did not run); a call that ran and answered false
// is off the viewport and counts nowhere.
Counts CountOnScreen(const wchar_t* name) {
    Counts out;
    void* cls = OI::ClassByName(name);
    if (!cls) return out;
    std::vector<void*> widgets;
    OI::ForEachInstance(cls, [](void* ctx, void* obj, int32_t index) {
        if (R::SlotFlags(index) & (R::slot_flags::Dying | R::slot_flags::NotYetReadable)) return;
        if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
        static_cast<std::vector<void*>*>(ctx)->push_back(obj);
    }, &widgets);
    for (void* w : widgets) {
        if (!R::IsLive(w)) continue;
        ++out.live;
        void* inViewportFn = R::FindDispatchFunctionCached(R::ClassOf(w), L"IsInViewport");
        ParamFrame f(inViewportFn);
        if (!inViewportFn || !f.valid() || !Call(w, f)) {
            ++out.unreadable;
            continue;
        }
        if (f.Get<bool>(L"ReturnValue")) ++out.onScreen;
    }
    return out;
}

}  // namespace

ScreenState ReadScreenState() {
    ScreenState s;
    s.listed = OI::IsSeeded() && OI::DrainAll() != SIZE_MAX;
    if (s.listed) {
        const Counts black = CountOnScreen(P::name::BlackScreenWidgetClass);
        s.blackLive = black.live;
        s.blackOnScreen = black.onScreen;
        s.blackUnreadable = black.unreadable;
        const Counts blur = CountOnScreen(P::name::BloodLossBlurWidgetClass);
        s.blurOnScreen = blur.onScreen;
        s.blurUnreadable = blur.unreadable;
    }
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
