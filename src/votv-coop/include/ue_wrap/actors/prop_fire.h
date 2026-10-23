// ue_wrap/actors/prop_fire.h -- the prop's fire members: the flame a prop carries once it burns, and the verbs that
// light it. Engine-wrapper layer (principle 7). Implementation: src/ue_wrap/actors/prop_fire.cpp.

#pragma once

namespace ue_wrap::prop {

// Whether the prop's `flameBase` (an object field of prop_C) holds a live object, so whether the prop is burning.
// False when the prop's class declares no such field. Game thread.
bool PropFlameBaseValid(void* prop);

// The prop's attemptIgnite(), resolved on its own class (FindDispatchFunctionCached). False when the function does
// not resolve or the call could not run. Game thread.
bool CallAttemptIgnite(void* prop);

// The prop_C's ignite(fuel) on `prop`, resolved on its own class as above. False when the function does not resolve
// or the call could not run. Game thread.
bool CallIgnite(void* prop, float fuel);

}  // namespace ue_wrap::prop
