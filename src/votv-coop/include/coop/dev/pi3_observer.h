// coop/dev/pi3_observer.h -- [dev] the verb observer of the PI-3 drill: which machine-global verbs a creature (or a
// prop) spawned at a still peer's puppet reaches, counted per verb, with the save, quit and travel verbs cancelled so
// no arm ever saves, quits or travels a machine.
//
// A verb is one of a fixed list (the strings of Report). The script verbs are watched by name in the script gate (a
// pre callback with the calling Blueprint frame), the native ones by a Func patch armed only while an arm is armed.
// A call counts when its caller (the frame's object) is the arm's site: a creature of one of the armed classes and,
// once Bind has named the spawned site, that site alone. The CONTACT of an armed class' overlap stub is counted too,
// by who touched: a puppet body, or this machine's own player. Verdicts are read from the records, never from the
// machine's state. On BOTH peers whenever the drill's row is on; game thread only, plain statics.

// The verbs the observer cancels, in every mode and build, when attributed: lib_C::loadLevel, lib_C::end,
// lib_C::setSettings, saveSlot_C::save, save_main_C::save, mainGamemode_C::save and QuitGame. The ones it records
// and lets run: daynightCycle_C::rewind, mainPlayer_C::punch (on this machine's player), SetControlRotation (a
// PlayerController's), PlaySound2D, AddToViewport and gib (BeginDeferredActorSpawnFromClass of a prop_C).

#pragma once

namespace coop::dev::pi3_observer {

// Spawner: the contact only records (the module under test decides). Still: the contact also cancels the creature's
// touch of this machine's own player, so a mirror never runs it on the real player. Reading: as Spawner, the first
// own contact is the anchor, and only the cancel list cancels (the client arm's own-touch leg).
enum class Mode : unsigned char { Spawner, Still, Reading };

// One verb with at least one call in the arm. `firstMs` counts from the arm's anchor (the first contact; on a Still
// peer the Arm call), negative for a verb that ran before any contact.
struct Seen {
    const char* verb;
    int         count;
    int         cancelled;
    int         firstMs;
};

// Register every watch and hook; the second call returns. A hook whose row did not resolve is retried by Tick.
void Install();

// Settle the name watches (the script gate resolves them on its consumer's pump) while any is pending and while
// armed, and install a pending hook. Says once that the observer is live, and once for each hook the engine
// refused (a BROKEN line).
void Tick();

// Every name watch is live, every contact watch included (only the armed classes' once armed), and every hook is
// installed.
bool Live();

// Every armed class' contact watch is still live. A class is judged for the function it watches when it gets its
// first listed instance, so this can turn false after Live was true; true while unarmed.
bool ContactWatchLive();

// Attribute the calls whose caller descends from one of `classes[0..n)` (n is 1 or 2), reset the records, arm the
// hooks. Before the spawn: an overlap fires as the collider registers, inside the spawn.
void Arm(void* const* classes, int n, Mode mode);

// The spawned site: from here a call counts only when its caller IS the site.
void Bind(void* site);

void Disarm();

// The verbs seen since Arm, in the list's order; `n` is how many.
const Seen* Report(int& n);

// Contacts of the armed classes' stubs since Arm, by toucher.
int PuppetContacts();
int OwnContacts();

}  // namespace coop::dev::pi3_observer
