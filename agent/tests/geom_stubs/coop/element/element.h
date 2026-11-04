#pragma once
#include <cstdint>
#include <string>
namespace coop::element {
using ElementId=uint32_t;
inline constexpr ElementId kInvalidId=0, kHostRangeSize=0x80000000u;
enum class ElementType { Npc, Prop };
struct Element {
    ElementId eid=0;
    void* actor=nullptr;
    bool mirror=false;
    ElementType type=ElementType::Npc;
    std::string typeName="geomOcta_C";
    const std::string& GetTypeName()const{return typeName;}
    void* LiveActor()const{return actor;}
    ElementType GetType()const{return type;}
    bool IsMirror()const{return mirror;}
};
void* LiveActorOfType(ElementId,ElementType);
}
