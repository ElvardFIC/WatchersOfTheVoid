#pragma once
namespace ue_wrap::rufus {
bool IsFamilyClass(void* cls);
bool IsFamily(void* actor);
// Only the mesh/AnimBP setup from native BeginPlay; no HUD, AI or gameplay calls.
bool PreparePresentation(void* actor);
bool ReadBodyYaw(void* actor,float& yaw);
bool DriveBodyYaw(void* actor,float yaw);
}
