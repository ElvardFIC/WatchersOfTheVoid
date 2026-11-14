#pragma once
#include "coop/element/mirror_managers.h"
namespace coop::element {
struct Registry {
    static Registry& Get(){static Registry r;return r;}
    static bool IsAllowedHostAllocatedEid(uint32_t id){return id&&id<kHostRangeSize;}
    struct ActorIdPair {void* actor;ElementId id;int32_t internalIdx;bool mirror;};
    size_t SnapshotActorsByType(ElementType,std::vector<ActorIdPair>& out) {
        out.clear();for(auto* row:WaMirrors().entries)if(row)out.push_back({row->actor,row->id,row->GetInternalIdx(),row->mirror});
        return out.size();
    }
};
}
