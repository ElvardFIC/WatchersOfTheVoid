#pragma once
namespace ue_wrap::physics_gun {
// Native selected-hand prop_physgun (including soft/super subclasses), not the
// main player's comp_gravitygun. Read only; never replay the weapon's input graph.
void* HeldProp(void* gun);
}
