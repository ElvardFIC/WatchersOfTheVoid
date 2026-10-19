#pragma once
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <cmath>

namespace coop::props {
inline bool ConvergePropScale(void* actor, const ue_wrap::FVector& desired) {
    if (!actor || !std::isfinite(desired.X) || !std::isfinite(desired.Y) ||
        !std::isfinite(desired.Z) || desired.X <= 0 || desired.Y <= 0 || desired.Z <= 0)
        return false;
    ue_wrap::CachedObjRef identity;
    identity.Set(actor);
    const auto current = ue_wrap::engine::GetActorScale3D(actor);
    if (!identity.Is(actor)) return false;
    if (std::fabs(current.X-desired.X) < .0001f &&
        std::fabs(current.Y-desired.Y) < .0001f && std::fabs(current.Z-desired.Z) < .0001f) return true;
    return ue_wrap::engine::SetActorScale3D(actor, desired);
}
}
