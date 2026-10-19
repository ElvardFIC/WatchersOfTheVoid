#include "ue_wrap/actors/floor_bed.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include <set>

namespace ue_wrap::floor_bed {
namespace R = reflection;
namespace {
std::set<void*> g_initializing; // GT only; scopes survive callbacks/world teardown
struct KeyScope {
    explicit KeyScope(void* a) : actor(a) { g_initializing.insert(actor); }
    ~KeyScope() { g_initializing.erase(actor); }
    void* actor;
};
bool HasKey(void* actor) {
    const auto key = prop::GetInteractableKeyString(actor);
    return !key.empty() && key != L"None";
}
}

bool IsFloorBed(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    return IsFloorBedClass(R::ClassOf(actor));
}

bool IsFloorBedClass(void* cls) {
    if (!cls) return false;
    for (int i = 0; cls && i < 16; ++i, cls = R::SuperStructOf(cls))
        if (R::NameEquals(R::NameOf(cls), L"sleepingbagWrap_C")) return true;
    return false;
}

bool EnsureKey(void* actor) {
    if (!IsFloorBed(actor) || g_initializing.count(actor)) return false;
    CachedObjRef identity;
    identity.Set(actor);
    if (identity.Get() != actor) return false;
    if (HasKey(actor)) return true;
    KeyScope scope(actor);
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(actor), L"getKey");
    if (!fn) return false;
    ParamFrame frame(fn);
    if (!frame.valid() || !Call(actor, frame) || identity.Get() != actor) return false;
    return HasKey(actor); // native assignKey writes the live field and its gamemode index
}
}
