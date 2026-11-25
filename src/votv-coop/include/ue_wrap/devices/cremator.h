#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ue_wrap/core/types.h"

namespace ue_wrap::cremator {
// Class-family checks and the live machines used to give their runtime-created hatch a stable ID.
bool EnsureResolved();
bool IsCremator(void* actor);
bool LoopActor(void* callerFunction, const uint8_t* callerLocals, void*& actor);
bool ReadAnim(void* actor, bool& value);
bool ReadActive(void* actor, bool& value);
bool ReadClosed(void* actor, bool& value);
bool ReadLookingAtHandle(void* actor, bool& value);
bool WriteLookingAtHandle(void* actor, bool value);
bool ReadLookingAtValve(void* actor, bool& value);
bool ReadWindup(void* actor, float& value);
bool ReadGasAmount(void* actor, float& value);
inline constexpr uint8_t kPressAction = 4;
bool ReadDoorOpened(void* actor, bool& value);
void* Door(void* actor);
void* Firebox(void* actor);
void* Handle(void* actor);
bool HandleLocation(void* actor, FVector& at);
void* BurnVolume(void* actor);
bool OverlapsBurnVolume(void* actor, void* other);
bool FireboxLocation(void* actor, FVector& at);
bool ShutDoor(void* actor, bool& wasOpened);
bool OpenDoor(void* actor, bool& opened);
struct Nearest { FVector from{}; float best=0; void* actor=nullptr; FVector at{}; };
Nearest FindNearestFirebox(const FVector& from);
struct FireboxMove { void* actor=nullptr; FVector at{}; FRotator rot{}; };
enum class MoveResult : uint8_t { Found, NoCremator, RotationUnread };
MoveResult FindFireboxMove(const FVector& from, FireboxMove& out);
inline constexpr float kNoHealth=-999.f;
struct Machine { int32_t actors=-1; float health0=kNoHealth; };
Machine ReadMachine(void* actor);
const std::vector<void*>& Instances();
// The manual lever is actionOptionIndex(action=4) -> useLever(), gated by the
// actor's lookingAt_handle and anim fields. These helpers expose that exact BP path.
bool CanUseLever(void* actor);
bool CanReplayLever(void* actor);
bool TryReadLeverActive(void* actor, bool& active);
bool CallUseLever(void* actor);
std::wstring LeverWireKey(void* actor);
void* FindByLeverWireKey(const std::wstring& key);
}
