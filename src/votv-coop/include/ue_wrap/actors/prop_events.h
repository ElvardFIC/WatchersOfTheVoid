// ue_wrap/actors/prop_events.h -- exact-owner prop events. Base calls use prop_C's
// own body; the remote grab adapter also handles the native glowstick replacement.
// Arbitrary subclass overrides are not dispatched. Engine-wrapper layer
// (principle 7): no network or coop state.
// Implementation: src/ue_wrap/actors/prop_events.cpp.

#pragma once

namespace ue_wrap::prop {

// prop_C's playerGrabbed_pre, the first thing a grab runs on the prop: its lifespan cleared, its
// `touched` broadcast, and awakeUnfreeze when it is frozen or asleep. The base body never reads its
// player, so none is passed. False for a null prop or when the class or the event did not resolve.
// Game thread.
bool CallBaseGrabPrelude(void* prop);

// The receiver's native prelude: glowsticks release their constraint through
// prop_glowstick_C.releaseGlowstick instead of running prop_C's prelude. Other
// non-wall-attachable props retain the base event; arbitrary overrides never run.
// NotReady means no native call was admitted and readiness may be retried.
// Failed means dispatch was attempted; its effects must not be replayed blindly.
enum class GrabPreludeResult { NotReady, Applied, Failed };
GrabPreludeResult ReplayRemoteGrabPrelude(void* prop);

// prop_C's own setPropProps(static, frozen, active, sleeping): the flags written and init() run. The
// base body never reads `active`, where a subclass's override does, as its own state -- a spotlight's
// on switch, an explosive's arming, the plasma TV's stick switch -- and forces sleep off on the
// wall-attach component's owners, so a flag that only the physics state should change is written
// through the base. False for a null prop or when the class or the function did not resolve. Game
// thread.
bool CallBaseSetPropProps(void* prop, bool isStatic, bool frozen, bool sleeping);

// prop_C's thrown(Player) event, run as a throw runs it, so the Blueprint plays the swing's trail and
// sound. False for a null prop or player, or when the class or the event did not resolve. Game
// thread.
bool CallPropThrown(void* prop, void* player);

bool ReadIgnoreSave(void* prop, bool& out);
bool ReadCanPickup(void* prop, bool& out);

}  // namespace ue_wrap::prop
