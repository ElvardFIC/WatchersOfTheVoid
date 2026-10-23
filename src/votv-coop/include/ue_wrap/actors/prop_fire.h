// ue_wrap/actors/prop_fire.h -- the prop's fire members: the flame a prop carries once it burns. Engine-wrapper
// layer (principle 7). Implementation: src/ue_wrap/actors/prop_fire.cpp.

#pragma once

namespace ue_wrap::prop {

// Whether the prop's `flameBase` (an object field of prop_C) holds a live object, so whether the prop is burning.
// False when the prop's class declares no such field. Game thread.
bool PropFlameBaseValid(void* prop);

}  // namespace ue_wrap::prop
