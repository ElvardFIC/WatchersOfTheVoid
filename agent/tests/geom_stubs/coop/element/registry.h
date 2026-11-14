#pragma once
#include "coop/element/element.h"
#include <array>
namespace coop::element {
struct Registry {
    std::array<Element,16> rows{};
    static Registry& Get(){static Registry r;return r;}
    static bool IsAllowedHostAllocatedEid(uint32_t id){return id && id<kHostRangeSize;}
    Element* Get(ElementId id){for(auto& r:rows)if(r.eid==id)return &r;return nullptr;}
    const Element* Get(ElementId id)const{for(auto& r:rows)if(r.eid==id)return &r;return nullptr;}
    ElementId EidForActor(void* a)const{for(auto& r:rows)if(r.actor==a)return r.eid;return 0;}
};
inline void* LiveActorOfType(ElementId id,ElementType type){auto* r=Registry::Get().Get(id);return r && r->type==type?r->actor:nullptr;}
}
