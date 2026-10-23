// coop/world/explosion_recipe.h -- what the explosion lane's wire values mean and the parameters each
// explosion source blasts with. The values and the pill level's clamp are the wire's; the numbers per
// source are the source Blueprints' own, over explosion_C's defaults. Engine-free apart from the
// parameter struct; game thread.
#pragma once

#include "ue_wrap/world/explosion.h"
#include "ue_wrap/world/explosion_source.h"

#include <cstddef>
#include <cstdint>

namespace coop::explosion_recipe {

// The wire's values. Anything else is not a value of the field.
enum class ProjClass : uint8_t { Rocket = 1, Sticky = 2 };
enum class Launcher : uint8_t { Rpg = 1, StickyLauncher = 2, WitchCar = 3 };
// The length of an array indexed by a Launcher's value (index 0 is no launcher), from the enum's last value.
// A launcher added after WitchCar must move this to the new last value.
inline constexpr size_t kLauncherSlots = static_cast<size_t>(Launcher::WitchCar) + 1;
enum class RequestKind : uint8_t { Projectile = 1, Hand = 2, Self = 3 };

// L"prop_funGun_rpg_C", L"prop_funGun_stickyLauncher_C", L"car1_witch_C"; null for any other value.
const wchar_t* LauncherClassName(uint8_t launcher);

// srcClass: 1 rocket, 2 sticky bomb, 3 grenade, 4 pipe bomb, 5 the player, 6 poop grenade, 7 soap grenade;
// anything else maps to None.
ue_wrap::explosion_source::Kind KindOfSrcClass(uint8_t srcClass);
uint8_t SrcClassOf(ue_wrap::explosion_source::Kind k);

// A finite level clamped to [0, 200]: the game adds up to 15 a drink with no cap. A non-finite level is
// refused before it gets here.
float ClampLevel(float pillLevel);

// explosion_C's own defaults with the source's own values over them (None gets the defaults alone).
// scale: the player's Lerp(0.2, 1, level/100) on all three axes; 1 for every other source.
void RecipeFor(ue_wrap::explosion_source::Kind k, float clampedLevel, ue_wrap::explosion::Params& out,
               float scale[3]);

// Once per process; logs ALL PASS / n/m checks passed, and later calls return the first verdict silently.
bool RunSelftest();

}  // namespace coop::explosion_recipe
