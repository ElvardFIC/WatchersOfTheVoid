#pragma once
#include "ue_wrap/core/types.h"
namespace coop {class RemotePlayer {public:void* actor=nullptr;void* GetActor()const{return actor;}bool valid()const{return actor!=nullptr;}bool HasPose()const{return valid();}bool TryGetHeadPosition(ue_wrap::FVector& p)const{p={1,2,3};return valid();}ue_wrap::FRotator GetSyncedAimRotation()const{return {12,34,0};}};}
