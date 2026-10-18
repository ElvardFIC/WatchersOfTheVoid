// ue_wrap/world/explosion_source.h -- the VOTV classes whose own Blueprint spawns an explosion_C when
// its fuse, flight or burn ends, named by kind. Engine facts only: which class each kind is.
#pragma once

#include <cstdint>

namespace ue_wrap::explosion_source {

// Seven classes that spawn an explosion_C of their own.
enum class Kind : uint8_t { None, Rocket, Sticky, Grenade, Pipebomb, Player, PoopGrenade, SoapGrenade };

// prop_rocket_C, prop_stickybomb_C, prop_grenade_C, prop_pipebomb_C, mainPlayer_C, prop_grenade_poop_C,
// prop_grenade_soap_C; null for None.
const wchar_t* ClassName(Kind k);

}  // namespace ue_wrap::explosion_source
