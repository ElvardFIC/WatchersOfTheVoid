// coop/props/prop_spawn_authoring.h -- did a PLAYER author this prop birth, or did the game?
//
// A client's world mints keyed props constantly with nobody asking: spawners, a dirthole, a
// lightning strike, an impact, lib_C::replaceProp. Each resolves the same catalog and finishes the
// same GameplayStatics spawn the sandbox spawn menu does, and none may cross -- every peer's game
// makes its own, so an intent per birth would double the world. So the drop-intent lane cannot
// admit by CLASS; the discriminator is WHO ASKED. Background:
// docs/coop-entity-expression-map.md, "Who asked, for a client's fresh birth".

// The two player spawn verbs, read from bytecode on the shipped pak. The MENU: a catalog click
// runs ui_spawnmenu_C::spawn, which calls gamemode->spawnPropThroughGamemode -- a verb replaceProp
// and the impact component call too, so it is identified by its CALLER, through a script-body gate
// watch. The TOOLGUN: tool_spawn_C::init enters ExecuteUbergraph_tool_spawn, which finishes its
// own spawns and holds no other FinishSpawningActor, so the native seam's caller frame names it.
// The WORKBENCH also finishes crafted output directly in its own ubergraph. The
// exact function AND declaring class identify it; other workbench calls do not.
// Sleeping bags/mattresses use prop_sleepingbag's unfold graph and sleepingbagWrap's
// fold verb. These replace one keyed prop with another; begin-play gen is excluded.

// Gameplay layer (principle 7): reaches the engine only through ue_wrap. Game thread only -- the
// gate and the native seam both hand their callbacks the VM's live frame.

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
