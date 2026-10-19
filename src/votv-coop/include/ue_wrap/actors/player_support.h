#pragma once
namespace ue_wrap::player_support {
// Actor owning the Character's current MovementBase component; null in the air.
// Reads the live BasedMovement struct layout, never a guessed engine offset.
void* Read(void* player);
}
