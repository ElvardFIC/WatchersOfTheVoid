#pragma once
#include <cstdint>
namespace coop::hand_item {
void* LocalHandActor();uint32_t Generation(uint8_t slot);
void* MirrorActorForSlot(uint8_t slot);uint32_t MirrorGeneration(uint8_t slot);
}
