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
    struct Extra {ElementId id;Row row;};
    std::vector<Extra> extra;
    ElementId id=kInvalidId;
    bool present=true;
    static Registry& Get(){static Registry r;return r;}
    ElementId EidForActor(void* p)const {
        if(p==row.actor)return id;
        for(const auto& entry:extra)if(p==entry.row.actor)return entry.id;
        return kInvalidId;
    }
    Row* Get(ElementId value) {
        if(present&&value==id)return &row;
        for(auto& entry:extra)if(value==entry.id)return &entry.row;
        return nullptr;
    }
    struct ActorIdPair {void* actor;ElementId id;int32_t internalIdx;bool mirror;};
    size_t SnapshotActorsByType(ElementType,std::vector<ActorIdPair>& out)const {
        out.clear();if(present&&row.live&&row.actor)out.push_back({row.actor,id,ue_wrap::reflection::InternalIndexOf(row.actor),row.mirror});
        for(const auto& entry:extra)if(entry.row.live&&entry.row.actor)
            out.push_back({entry.row.actor,entry.id,ue_wrap::reflection::InternalIndexOf(entry.row.actor),entry.row.mirror});
        return out.size();
    }
};
}
