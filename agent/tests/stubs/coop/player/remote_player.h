#pragma once
namespace coop {
class RemotePlayer { public: void* actor=nullptr; void* GetActor() const { return actor; } bool valid()const{return actor!=nullptr;} };
}
