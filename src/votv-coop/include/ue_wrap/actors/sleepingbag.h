// ue_wrap/actors/sleepingbag.h -- reflected access to the folded and deployed sleepingbag props.
// Engine layer only: class checks and calls into the game's own Blueprint verbs.

#pragma once

#include <cstdint>

namespace ue_wrap::sleepingbag {

bool IsPacked(void* actor);
bool IsDeployed(void* actor);
// bed_bunkMattress_C: the mattress lying on a bunk bed. Its action 7 spawns the packed mattress
// and destroys it.
bool IsBunkMattress(void* actor);

// Replay the packed prop's (or the bunk mattress's) actionOptionIndex on the host's copy. Hit_0 is
// zeroed: the graphs perform their own traces from the prop, and the host's player puppet is
// passed for branches that use the presser. Game thread.
bool CallUnwrap(void* actor, void* player, uint8_t action);

// Run sleepingbagWrap_C::fold() on the host's deployed prop. The Blueprint creates the packed
// variant and destroys this actor; ordinary host prop birth/death observers carry both edges.
bool CallFold(void* actor);

}  // namespace ue_wrap::sleepingbag
