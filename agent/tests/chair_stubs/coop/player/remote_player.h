#pragma once
#include "coop/net/protocol.h"
namespace coop {class RemotePlayer {public:
 void* actor=nullptr;uint8_t curStateBits_=net::kStateBitSitting;
 int viewRefreshes=0;void RefreshSeatView(){++viewRefreshes;}
 void* GetActor()const{return actor;}bool valid()const{return actor!=nullptr;}
 #include "chair_sitting.inc"
};}
