#pragma once
namespace coop {
struct RemotePlayer {
    void* actor=nullptr;
    bool valid()const{return actor!=nullptr;}
    void* GetActor()const{return actor;}
};
}
