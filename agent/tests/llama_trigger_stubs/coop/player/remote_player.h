#pragma once
#include "ue_wrap/core/types.h"
#include <functional>
namespace coop {
class RemotePlayer {
public:
    void* actor=nullptr;bool live=true,posed=true,ragdoll=false,bodyOk=true,eyeOk=true;
    ue_wrap::FVector body{100,0,0},eye{100,0,100};mutable std::function<void()> onRead;
    bool valid()const{return live;}bool HasPose()const{return posed;}bool IsRagdollDisplayed()const{return ragdoll;}
    void* GetActor()const{return actor;}
    bool TryGetLocation(ue_wrap::FVector& out)const{out=body;auto f=std::move(onRead);onRead={};if(f)f();return bodyOk;}
    bool TryGetEyePosition(ue_wrap::FVector& out)const{out=eye;return eyeOk;}
};
}
