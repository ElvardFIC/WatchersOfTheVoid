#pragma once

#include <string>
#include <vector>

namespace ue_wrap::cremator {
// Class-family checks and the live machines used to give their runtime-created hatch a stable ID.
bool EnsureResolved();
bool IsCrematorDoor(void* actor);
bool IsCremator(void* actor);
const std::vector<void*>& Instances();
// The manual lever is actionOptionIndex(action=4) -> useLever(), gated by the
// actor's lookingAt_handle and anim fields. These helpers expose that exact BP path.
bool CanUseLever(void* actor);
bool CanReplayLever(void* actor);
bool TryReadLeverActive(void* actor, bool& active);
bool IsLeverActive(void* actor);
bool CallUseLever(void* actor);
std::wstring LeverWireKey(void* actor);
void* FindByLeverWireKey(const std::wstring& key);
}
