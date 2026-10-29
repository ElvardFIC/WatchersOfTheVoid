#pragma once
#include "ue_wrap/core/types.h"

namespace ue_wrap::cord {
bool IsCord(void* actor);
bool IsSocket(void* actor);
bool IsCordedProp(void* actor);
void* CordOf(void* actor);
void* SocketOf(void* actor);
void* Parent(void* actor);
void* Socket(void* actor);
bool ReadHeadPose(void* actor, FVector& location, FRotator& rotation);
bool WriteHeadPose(void* actor, const FVector& location, const FRotator& rotation);
void* Occupant(void* socket);
bool Dragging(void* actor);
bool Powered(void* socket, bool& out);
bool Connect(void* actor, void* socket);
bool Disconnect(void* actor);
bool SetPower(void* socket, bool powered);
bool InCableReach(void* actor, void* socket);
}
