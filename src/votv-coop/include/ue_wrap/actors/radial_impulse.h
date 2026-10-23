#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <functional>
#include <vector>
namespace ue_wrap::radial_impulse {
// Read the native radial component's world sphere and object-type filter.
// Call synchronously at FireImpulse, before physics integrates body positions.
// Append live actor identities only after all native calls remain current.
bool CollectActors(void* component,std::vector<CachedObjRef>& out,
                   const std::function<bool()>& current);
}
