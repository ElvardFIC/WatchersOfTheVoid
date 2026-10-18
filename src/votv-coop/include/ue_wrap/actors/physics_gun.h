#pragma once
#include "ue_wrap/core/types.h"
#include <cstdint>
namespace ue_wrap::physics_gun {
// Native selected-hand prop_physgun (including soft/super subclasses), not the
// main player's comp_gravitygun. Read only; never replay the weapon's input graph.
void* HeldProp(void* gun);
struct Visuals { uint8_t kind=0, flags=0; FVector target{}; void* object=nullptr; };
// Cosmetic only: 1=physgun, 2=gravgun; hold/grab/throw/release/attract bits.
Visuals ReadVisuals(void* gun);
void ApplyVisuals(void* gun, const Visuals& state);
}
