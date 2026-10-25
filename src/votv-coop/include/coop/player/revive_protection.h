// coop/player/revive_protection.h -- a revived player is protected for the game's own spawn window.
// The protection is the game's own flag, not a filter of ours: mainPlayer_C.startInvinc, which the
// game sets for a player who just loaded the world and clears after its own 5 s, and which Add
// Player Damage and kill both refuse on. A revive holds it for the same 5 s of wall-clock time; the
// window ends early on the revived player's melee swing (the watch on mainPlayer_C::attack), on a
// session teardown, or on the next revive. Local to the revived machine, no wire.

#pragma once

#include <cstdint>

namespace coop::revive_protection {

enum class EndWhy : uint8_t { None, Timeout, Attack, Session, Revive };

// Registers, then settles, the attack watch. Called every frame by the harness pump; cheap once
// settled. Game thread.
void Install();

// A local player was just revived: set its startInvinc and open the window. A window already open
// ends first (Revive). Game thread.
void Begin(void* player);

// Ends a window whose deadline passed. Every pump tick, game thread.
void Tick();

// Closes the window and clears the flag on the pinned pawn if it is still alive. No-op when none is
// open. Game thread.
void End(EndWhy why);

// For the drill, any thread.
bool Active();
uint32_t EndedCount();
EndWhy LastEndWhy();
uint64_t LastWindowMs();

}  // namespace coop::revive_protection
