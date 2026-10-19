#pragma once
namespace coop::element {
enum class ElementType {Npc};
class Element {
public:
    explicit Element(ElementType){}
    void* actor=nullptr;
    void* GetActor()const{return actor;}
    int GetInternalIdx()const{return 1;}
};
}
