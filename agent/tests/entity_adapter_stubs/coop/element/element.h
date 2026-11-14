#pragma once
#include "ue_wrap/core/reflection.h"
#include <string>
namespace coop::element {
using ElementId=uint32_t;
inline constexpr ElementId kInvalidId=0,kHostRangeSize=0x80000000u;
enum class ElementType {Npc,WorldActor};
struct Element {
    ElementId id=0;void* actor=nullptr;bool mirror=false;std::string name;ElementType type=ElementType::Npc;
    ElementId GetId()const{return id;}void* GetActor()const{return actor;}
    int GetInternalIdx()const{return ue_wrap::reflection::InternalIndexOf(actor);}
    void* LiveActor()const{return ue_wrap::reflection::IsLive(actor)?actor:nullptr;}
    bool IsMirror()const{return mirror;}const std::string& GetTypeName()const{return name;}
};
}
