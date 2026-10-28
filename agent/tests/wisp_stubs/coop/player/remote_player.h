#pragma once
namespace coop {
struct RemotePlayer {
    void* actor=nullptr;
    float health=1.f;
    bool valid()const{return actor!=nullptr;}
    void* GetActor()const{return actor;}
    float GetHealth()const{return health;}
};
}
