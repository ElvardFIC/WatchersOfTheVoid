#include "ue_wrap/devices/atv_steering.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/facts/function_row.h"
#include <cmath>
#include <cstring>

namespace ue_wrap::atv_steering {
namespace {
namespace R=reflection;
UE_FACT_FN(kFront, "atv_steering", L"ATV_C", L"setFrontWheelsOrientation", Declared);
void* Handle(void* actor) {
    if (!actor || !R::IsLive(actor)) return nullptr;
    static R::InstanceOffset offset(L"handleAxis");
    const int off=offset.Of(actor); void* component=nullptr;
    if (off>=0) std::memcpy(&component,static_cast<uint8_t*>(actor)+off,sizeof(component));
    return component && R::IsLive(component)?component:nullptr;
}
}
bool Read(void* actor,float& yaw) {
    void* handle=Handle(actor); if (!handle) return false;
    static R::InstanceOffset rotation(L"RelativeRotation");
    const int off=rotation.Of(handle); FRotator value{};
    if (off<0) return false;
    std::memcpy(&value,static_cast<uint8_t*>(handle)+off,sizeof(value));
    if (!std::isfinite(value.Yaw) || std::fabs(value.Yaw)>45.01f) return false;
    yaw=value.Yaw; return true;
}
bool Apply(void* actor,float yaw,const std::function<bool()>& current) {
    if (!std::isfinite(yaw) || std::fabs(yaw)>45.f || !current || !current()) return false;
    CachedObjRef owner; owner.Set(actor);
    void* handle=Handle(actor); if (!handle || !owner.Get() || !current()) return false;
    CachedObjRef component; component.Set(handle);
    CachedObjRef turn; turn.Set(facts::Resolve(kFront).Fn());
    CachedObjRef rotate; rotate.Set(R::FindDispatchFunctionCached(R::ClassOf(handle),L"K2_SetRelativeRotation"));
    ParamFrame wheel(turn.Get()),bar(rotate.Get());
    // Native MakeRotator(roll=0,pitch=20,yaw=angle), followed by the same
    // angle in setFrontWheelsOrientation (which owns the half-angle constraints).
    if (!wheel.valid() || !bar.valid() || !wheel.Set(L"A",yaw) ||
        !bar.Set(L"NewRotation",FRotator{20.f,yaw,0.f}) ||
        !bar.Set<bool>(L"bSweep",false) || !bar.Set<bool>(L"bTeleport",false)) return false;
    const auto valid=[&]{return current() && owner.Get() && component.Get() && turn.Get() && rotate.Get();};
    if (!valid() || !Call(handle,bar) || !valid()) return false;
    return Call(actor,wheel) && valid();
}
}
