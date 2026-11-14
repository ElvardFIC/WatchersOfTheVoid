// ue_wrap/actors/prop_events.cpp -- see ue_wrap/actors/prop_events.h.

#include "ue_wrap/actors/prop_events.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"

#include <cstdint>

namespace ue_wrap::prop {
namespace {

namespace P = profile;
namespace R = reflection;

// One of prop_C's own events, found on prop_C itself (FindFunction does not climb, which is the
// point: an override on a subclass is a different function). Resolved once the class has loaded;
// an event missing from a loaded class is said once and never looked up again.
struct BaseEvent {
    const wchar_t* name;
    ue_wrap::CachedObjRef cls;
    ue_wrap::CachedObjRef fn;
    bool  missed = false;
    const wchar_t* owner = P::name::PropClass;
};

BaseEvent g_grabPrelude{L"playerGrabbed_pre"};
BaseEvent g_thrown{P::name::PropThrownFn};
BaseEvent g_setPropProps{L"setPropProps"};
BaseEvent g_glowstickRelease{L"releaseGlowstick", {}, {}, false, L"prop_glowstick_C"};

void* Resolve(BaseEvent& e) {
    if (!e.cls.Alive()) {
        e.fn.Reset();
        e.missed = false;
        e.cls.Set(R::FindClass(e.owner));
    }
    void* cls = e.cls.Get();
    if (!cls || e.missed) return nullptr;
    if (void* fn = e.fn.Get()) return fn;
    e.fn.Set(R::FindFunction(cls, e.name));
    if (!e.fn.Alive()) {
        UE_LOGW("prop_events: %ls.%ls did not resolve -- a receiver does not replay it", e.owner,
                e.name);
        e.missed = true;
    }
    return e.fn.Get();
}

}  // namespace

bool CallBaseGrabPrelude(void* prop) {
    if (!prop) return false;
    void* fn = Resolve(g_grabPrelude);
    if (!fn) return false;
    ParamFrame f(fn);  // the player and the hit result stay zeroed: the base body reads neither
    return f.valid() && Call(prop, f);
}

GrabPreludeResult ReplayRemoteGrabPrelude(void* prop) {
    if (!prop || !R::IsLive(prop)) return GrabPreludeResult::NotReady;
    CachedObjRef target;
    target.Set(prop);
    void* glowstick = object_index::ClassByName(L"prop_glowstick_C");
    if (!target.Alive()) return GrabPreludeResult::NotReady;
    const bool isGlowstick = glowstick && R::IsDescendantOfAny(R::ClassOf(prop), &glowstick, 1);
    BaseEvent& event = isGlowstick ? g_glowstickRelease : g_grabPrelude;
    void* fn = Resolve(event); // exact declaring owner, including inherited colour variants
    if (!fn || !target.Alive()) return GrabPreludeResult::NotReady;
    ParamFrame frame(fn);
    if (!frame.valid() || !target.Alive() || !event.fn.Is(fn)) return GrabPreludeResult::NotReady;
    return Call(prop, frame) ? GrabPreludeResult::Applied : GrabPreludeResult::Failed;
}

bool CallBaseSetPropProps(void* prop, bool isStatic, bool frozen, bool sleeping) {
    if (!prop) return false;
    void* fn = Resolve(g_setPropProps);
    if (!fn) return false;
    ParamFrame f(fn);  // `active` stays false: the base body never reads it
    return f.valid() && f.Set<bool>(L"static", isStatic) && f.Set<bool>(L"frozen", frozen) &&
           f.Set<bool>(L"sleeping", sleeping) && Call(prop, f);
}

bool CallPropThrown(void* prop, void* player) {
    if (!prop || !player) return false;
    void* fn = Resolve(g_thrown);
    if (!fn) return false;
    ParamFrame f(fn);
    return f.valid() && f.Set<void*>(L"Player", player) && Call(prop, f);
}

namespace {

// A pure BP function with one out bool, run on the prop's own class (a Dispatch lookup, memoised per class).
bool AskBool(void* prop, const wchar_t* fnName, const wchar_t* outName, bool& out) {
    if (!prop || !R::IsLive(prop)) return false;
    void* const fn = R::FindDispatchFunctionCached(R::ClassOf(prop), fnName);
    if (!fn) return false;
    ParamFrame f(fn);
    bool value = false;
    if (!f.valid() || !Call(prop, f) || !f.GetRaw(outName, &value, static_cast<int32_t>(sizeof value))) return false;
    out = value;
    return true;
}

}  // namespace

bool ReadIgnoreSave(void* prop, bool& out) { return AskBool(prop, L"ignoreSave", L"ignoreSave", out); }

bool ReadCanPickup(void* prop, bool& out) { return AskBool(prop, L"canPickup", L"return", out); }

}  // namespace ue_wrap::prop
