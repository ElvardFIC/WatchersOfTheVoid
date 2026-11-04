// Native tower calls retain actor, class, world and function incarnations.
#pragma once

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/types.h"
#include <cmath>
#include <cstdint>

namespace ue_wrap::tower_access {

struct Lifetime {
    CachedObjRef actor, cls;
    uint32_t world = world_identity::Generation();
    explicit Lifetime(void* object) {
        if (!object || !reflection::IsLive(object)) return;
        actor.Set(object);
        if (actor.Get()) cls.Set(reflection::ClassOf(object));
    }
    bool Current() const {
        return world == world_identity::Generation() && actor.Get() && cls.Get() &&
               reflection::ClassOf(actor.Raw()) == cls.Raw();
    }
};

inline bool Span(int32_t off, int32_t bytes, int32_t size) {
    return off >= 0 && bytes > 0 && size >= bytes && off <= size - bytes;
}
inline bool Array(const field_io::TArrayView& a, int32_t limit) {
    return a.num >= 0 && a.num <= limit && a.max >= a.num && (a.num == 0 || a.data);
}
inline bool Finite(const FVector& p) {
    return std::isfinite(p.X) && std::isfinite(p.Y) && std::isfinite(p.Z);
}
struct Busy {
    bool& flag;
    explicit Busy(bool& f) : flag(f) { flag = true; }
    ~Busy() { flag = false; }
    Busy(const Busy&) = delete;
    Busy& operator=(const Busy&) = delete;
};

template<class Current, class Fill, class Finish>
bool Dispatch(void* object, const wchar_t* name, Current current, Fill fill, Finish finish) {
    Lifetime target(object);
    if (!target.Current() || !current()) return false;
    void* fn = reflection::FindDispatchFunctionCached(target.cls.Raw(), name);
    if (!target.Current() || !current() || !fn || !reflection::IsLive(fn)) return false;
    CachedObjRef function;
    function.Set(fn);
    auto valid = [&] { return target.Current() && current() && function.Get(); };
    const int32_t bytes = reflection::FunctionFrameSize(fn);
    // Includes the native FHitResult plus player, component and held-object inputs.
    if (!valid() || bytes < 0 || bytes > 1024) return false;
    ParamFrame frame(fn);
    if (!valid() || !frame.valid() || frame.FrameSize() > 1024 || !fill(frame) || !valid()) return false;
    const bool called = Call(object, frame);
    return valid() && called && finish(frame) && valid();
}

template<class Current, class Fill>
bool Dispatch(void* object, const wchar_t* name, Current current, Fill fill) {
    return Dispatch(object, name, current, fill, [](ParamFrame&) { return true; });
}

template<class Current>
bool Location(void* component, Current current, FVector& out) {
    FVector value{};
    const bool ok = Dispatch(component, L"K2_GetComponentLocation", current,
        [](ParamFrame&) { return true; },
        [&](ParamFrame& f) { return f.GetRaw(L"ReturnValue", &value, sizeof(value)) && Finite(value); });
    if (ok) out = value;
    return ok;
}

} // namespace ue_wrap::tower_access
