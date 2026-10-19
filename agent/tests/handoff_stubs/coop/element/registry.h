#pragma once
#include "coop/element/element.h"
namespace coop::element {
inline ElementId testCoinEid=20;
class Registry {
public:
 static Registry& Get(){static Registry r;return r;}
 ElementId EidForActor(void*)const{return testCoinEid;}
 static bool IsAllowedHostAllocatedEid(ElementId id){return id>0&&id<1000;}
};
}
