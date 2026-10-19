#pragma once
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/engine_playerragdoll.h"
namespace ue_wrap::sleep_display {
// World-space pelvis anchor sampled from the current bed surface each frame.
bool LocalAnchor(FVector& position, float& yaw);
bool Drive(void* pawn, bool sleeping, const FVector& position, float yaw);
void Remove(void* pawn);
void* Mesh(void* pawn);
bool DriveArticulated(void* pawn, const std::vector<engine::ArticulatedBone>& bones);
}
