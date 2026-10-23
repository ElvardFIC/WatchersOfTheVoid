#include "ue_wrap/actors/radial_impulse.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/facts/function_row.h"
#include <array>
#include <cmath>
#include <cstring>

namespace ue_wrap::radial_impulse {
namespace {
namespace R=reflection;
UE_FACT_FN(kOverlap, "radial_impulse", L"KismetSystemLibrary", L"SphereOverlapActors", Declared);
struct Array { void* data=nullptr; int32_t num=0,max=0; };
static_assert(sizeof(Array)==16);
template<class T> bool Field(void* actor,const wchar_t* name,T& out) {
    const int off=R::FindPropertyOffset(R::ClassOf(actor),name);
    if (off<0) return false;
    std::memcpy(&out,static_cast<uint8_t*>(actor)+off,sizeof(out)); return true;
}
bool WorldActor(void* actor) {
    return actor && R::IsLive(actor) && world_identity::CurrentWorld() &&
        world_identity::WorldOf(actor)==world_identity::CurrentWorld();
}
}
bool CollectActors(void* component,std::vector<CachedObjRef>& out,const std::function<bool()>& current) {
    if (!current || !current() || !WorldActor(component)) return false;
    CachedObjRef ref; ref.Set(component);
    const auto world=world_identity::Generation();
    const auto valid=[&]{return current() && ref.Get() && world==world_identity::Generation();};
    float radius=0,strength=0; Array types;
    if (!Field(component,L"Radius",radius) || !Field(component,L"ImpulseStrength",strength) ||
        !Field(component,L"ObjectTypesToAffect",types) || !std::isfinite(radius) || radius<=0 || radius>1e6f ||
        !std::isfinite(strength) || std::fabs(strength)>1e7f || types.num<0 || types.num>32 || types.max<types.num ||
        (types.num && !types.data)) return false;
    if (!types.num || strength==0) return valid();
    // Borrowed native input arrays are copied before calling into the engine.
    // The output array is engine-owned and must be freed even on a failed call.
    std::array<uint8_t,32> copied{};
    std::memcpy(copied.data(),types.data,types.num);
    for (int i=0;i<types.num;++i) if (copied[i]>=32) return false;
    Array filter{copied.data(),types.num,types.num},ignored;
    ParamFrame location(R::FindDispatchFunctionCached(R::ClassOf(component),L"K2_GetComponentLocation"));
    FVector center{};
    if (!location.valid() || !valid() || !Call(component,location) || !valid() ||
        !location.GetRaw(L"ReturnValue",&center,sizeof(center)) ||
        !std::isfinite(center.X) || !std::isfinite(center.Y) || !std::isfinite(center.Z)) return false;
    void* library=R::FindClassDefaultObject(L"KismetSystemLibrary");
    ParamFrame query(facts::Resolve(kOverlap).Fn());
    if (!library || !query.valid() || !query.Set<void*>(L"WorldContextObject",component) ||
        !query.Set(L"SpherePos",center) || !query.Set(L"SphereRadius",radius) ||
        !query.SetRaw(L"ObjectTypes",&filter,sizeof(filter)) || !query.Set<void*>(L"ActorClassFilter",nullptr) ||
        !query.SetRaw(L"ActorsToIgnore",&ignored,sizeof(ignored)) || !valid()) return false;
    const bool called=Call(library,query);
    Array actors;
    const bool read=query.GetRaw(L"OutActors",&actors,sizeof(actors));
    struct Release { void* data; ~Release(){if(data) R::EngineFree(data);} } release{actors.data};
    if (!called || !read || !valid() || actors.num<0 || actors.num>65536 || actors.max<actors.num ||
        (actors.num && !actors.data)) return false;
    std::vector<CachedObjRef> found;
    found.reserve(actors.num);
    auto** pointers=static_cast<void**>(actors.data);
    for (int i=0;i<actors.num;++i) if (WorldActor(pointers[i])) {
        CachedObjRef actor; actor.Set(pointers[i]); found.push_back(actor);
    }
    if (!valid()) return false;
    out.insert(out.end(),found.begin(),found.end()); return true;
}
}
