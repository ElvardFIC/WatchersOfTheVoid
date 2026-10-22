// Distinguish player-authored client births from ambient native spawns.
// The spawn menu is identified by its caller; toolgun and workbench output by
// their declaring function and class. Bed fold/unfold replaces keyed props,
// excluding begin-play generation. A source-bound hand MRE is handled separately.
// Other ambient births stay local. See docs/coop-entity-expression-map.md.
// Gameplay layer: engine access through ue_wrap, on the game thread.

#pragma once

namespace coop::prop_spawn_authoring {

// Install the script-body gate watch on the gamemode's spawn verb. A no-op on the host;
// host floor-bed identity setup only uses the direct caller-frame unfold branch. Idempotent
// and retry-throttled while mainGamemode_C is unresolved (it loads on gameplay entry). Safe to
// call every subsystem-install tick. Game thread.
void Install(bool isClient);

// Is the birth the caller is looking at RIGHT NOW authored by a player's own spawn verb? Call it
// from inside a FinishSpawningActor post callback and nowhere else: it reads that callback's
// calling frame (ue_wrap::ufunction_hook::CurrentCallerFrame) and this thread's open menu
// bracket, both of which mean nothing outside one. False when no admitted verb is on the stack, which
// is every ambient spawn the world makes for itself. Game thread.
// sourceObject is FFrame::Object from the native finish-spawn seam, when available.
// A local hand MRE's own spawn body is admitted only with that exact source identity.
bool BirthIsPlayerAuthored(void* sourceObject = nullptr);

// Session teardown: report both columns of the caller tally -- the menu's calls, which crossed as
// intents, and every other caller's, which stayed local -- and zero it. The watch and the resolved
// UFunctions stay: they name the game's own classes, not this session's world. Game thread.
void Reset();

}  // namespace coop::prop_spawn_authoring
