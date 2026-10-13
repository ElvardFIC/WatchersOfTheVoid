#pragma once
#include "ue_wrap/core/types.h"
namespace coop { class RemotePlayer {
public:
 void* actor=nullptr;
 bool posed=true;
 bool HasPose()const{return posed;}
 bool valid() const {return actor!=nullptr;}
 void* GetActor()const{return actor;}
 bool TryGetHeadPosition(ue_wrap::FVector& out)const {out={};return true;}
 ue_wrap::FVector GetSyncedAimDirection()const{return {1,0,0};}
}; }
