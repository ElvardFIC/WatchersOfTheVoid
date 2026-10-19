#pragma once
#include "ue_wrap/core/types.h"
namespace coop {class RemotePlayer {public:
 void* actor=nullptr;ue_wrap::FVector pos{};bool pose=true;
 void* GetActor()const{return actor;}bool valid()const{return actor!=nullptr;}bool HasPose()const{return pose;}
 bool TryGetLocation(ue_wrap::FVector& out)const{out=pos;return valid();}
 ue_wrap::FRotator GetSyncedAimRotation()const{return {0,42,0};}
};}
