#pragma once
namespace coop { class RemotePlayer { public: void* actor=nullptr; bool valid() const {return actor!=nullptr;} void* GetActor() const {return actor;} }; }
