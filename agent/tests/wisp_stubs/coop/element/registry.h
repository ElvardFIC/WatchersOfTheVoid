#pragma once
#include "coop/element/element.h"
namespace coop::element {
struct Registry {
    bool ready=false; Element element;
    static Registry& Get(){static Registry r;return r;}
    Element* Get(ElementId id){return ready&&id==100?&element:nullptr;}
};
}
