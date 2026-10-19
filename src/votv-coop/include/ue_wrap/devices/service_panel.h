#pragma once
#include "ue_wrap/core/types.h"
#include <cstdint>
namespace ue_wrap::service_panel {
// One signal desk (1) and the radio tower (2). bytes are deliberately fixed
// size: arrays in cooked Blueprints are read/written only at their exact size.
uint8_t Family(void* actor);
bool Read(void* actor, uint8_t out[16]);
bool Apply(void* actor, const uint8_t state[16]);
bool RadioLook(void* actor, uint8_t& op, uint8_t& index);
bool RadioUse(void* actor, uint8_t op, uint8_t index);
bool PartLocation(void* actor, uint8_t op, uint8_t index, FVector& at);
bool FuseLook(void* actor, uint8_t& index);
bool SetFuse(void* actor, uint8_t index, uint8_t value);
bool DeskLook(void* actor, uint8_t& index);
// A host-approved removal drops one physical part beside its socket.
void* SpawnLoosePart(void* actor, uint8_t index, bool goodFuse=false, const FVector* dropAt=nullptr);
int CoverIndex(void* cover);
bool CanReturnCover(void* desk,void* cover); // ready, empty matching socket and actual overlap
bool TryReturnCover(void* desk,void* cover); // true after the matching socket accepted the part
}
