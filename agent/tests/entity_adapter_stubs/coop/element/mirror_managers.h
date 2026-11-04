#pragma once
#include "coop/element/npc.h"
#include "coop/element/world_actor.h"
#include <vector>
namespace coop::element {
template<class T> struct Manager {
    std::vector<T*> entries;
    void Snapshot(std::vector<T*>& out){out=entries;}
    T* Get(uint32_t id){for(auto* row:entries)if(row&&row->id==id)return row;return nullptr;}
};
inline Manager<Npc>& NpcMirrors(){static Manager<Npc> m;return m;}
inline Manager<WorldActor>& WaMirrors(){static Manager<WorldActor> m;return m;}
}
