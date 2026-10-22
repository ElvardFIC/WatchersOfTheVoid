// ue_wrap/actors/fossilhound.h -- fossil hound mesh world yaw. Game thread.
// Engine access only; implementation: src/ue_wrap/actors/fossilhound.cpp.
#pragma once
namespace ue_wrap::fossilhound {
// False for a non-hound or missing mesh. An unresolved class name retries on the next read.
bool ReadBodyYaw(void* actor, float& outYaw);
// Sets the hound mesh world yaw after its actor transform was driven; no-op on other NPCs.
void DriveBodyYaw(void* actor, float yaw);
}  // namespace ue_wrap::fossilhound
