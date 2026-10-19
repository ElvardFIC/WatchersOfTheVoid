#pragma once
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile_names.h"
namespace coop::npc_class_policy {
// A spawn's actual ancestry remains available before the class cache catches up.
inline bool MatchesAncestryName(void* cls) {
    namespace R=ue_wrap::reflection;
    for (int depth=0;cls && depth<64;++depth,cls=R::SuperStructOf(cls))
        for (const auto* name : ue_wrap::profile::name::kNpcAllowlist)
            if (R::NameEquals(R::NameOf(cls),name)) return true;
    return false;
}
}
