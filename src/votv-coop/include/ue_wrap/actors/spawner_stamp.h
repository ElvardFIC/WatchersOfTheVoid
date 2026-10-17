// ue_wrap/actors/spawner_stamp.h -- what a class's native spawner writes into the actor between the two halves of
// its spawn (BeginDeferredActorSpawnFromClass, then FinishSpawningActor), because the class's BeginPlay reads it:
// the value is a constant of the spawner's own bytecode, so a spawn made any other way writes it too. One table
// row per class; today the black orb (npc_orborb_C), whose BeginPlay ends the game unless NewVar_0 holds the string
// blackballSpawner_C writes. Engine-wrapper layer (principle 7): no network and no coop state; the caller decides
// what a failed stamp means.

#pragma once

namespace ue_wrap::spawner_stamp {

// Write every row of the table for `actorClass` into `deferredActor` (spawned, its Finish not yet made). True when
// the class has no row or every write landed. False when a row's property is missing on the loaded class, is not an
// FString, or the write failed: the game changed and the stamp is no longer known. The property's offset and type
// are resolved once per listing of the class (object_index::ClassListingId), the cost of a call after that being
// the listing compare. The first stamp of a listing says `[NQ] stamped <class> <property>`, a failed one
// `[NQ] stamp failed <class> <property>`, each once per listing. Game thread.
bool Stamp(void* actorClass, void* deferredActor);

// How many Stamp calls returned false in this process (a drill's read). Game thread.
int FailureCount();

}  // namespace ue_wrap::spawner_stamp
