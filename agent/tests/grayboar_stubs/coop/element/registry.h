#pragma once
#include "coop/element/element.h"
#include <vector>
#include "ue_wrap/core/reflection.h"
namespace coop::element {
struct Registry {
    struct Row {
        void* actor=nullptr;
        bool mirror=true,live=true;
        bool IsMirror()const{return mirror;}
        void* LiveActor()const{return live?actor:nullptr;}
    } row;
    ElementId id=kInvalidId;
    bool present=true;
    static Registry& Get(){static Registry r;return r;}
    ElementId EidForActor(void* p)const{return p==row.actor?id:kInvalidId;}
    Row* Get(ElementId value){return present&&value==id?&row:nullptr;}
    struct ActorIdPair {void* actor;ElementId id;int32_t internalIdx;bool mirror;};
    size_t SnapshotActorsByType(ElementType,std::vector<ActorIdPair>& out)const {
        out.clear();if(present&&row.live&&row.actor)out.push_back({row.actor,id,ue_wrap::reflection::InternalIndexOf(row.actor),row.mirror});
        return out.size();
    }
};
}
