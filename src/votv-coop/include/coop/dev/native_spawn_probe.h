// coop/dev/native_spawn_probe.h -- [dev] the native-spawn probe of the NPC-spawner arc (NB): every allowlisted
// creature this machine spawns from a Blueprint graph, recorded per source, and (orb) whether a client spawns its
// own black orb. Ini native_spawn_probe=off|record|orb; BOTH peers. Game thread; plain statics.
//
// The record is a post hook of its own on GameplayStatics::BeginDeferredActorSpawnFromClass. A call is kept when
// its result descends from an NPC-allowlisted class and its calling frame's object is not the GameplayStatics
// default object (a ProcessEvent-dispatched call). The key is the source class, the spawned class, the calling
// function and its code offset (the END of the call expression, the `end` of script_calls::CallSite); a new key
// prints once, a client's also as NATIVE, and every key's count prints when the session ends.
//
// orb, on a CLIENT after the ghost sweep: every blackballSpawner_C gets laps=241 and angle=540 on each Tick until
// the record sees an npc_orborb_C from blackballSpawner_C (the orb is destroyed on the next Tick) or 60 s pass;
// the spawners' laps return to 0 and "orb DONE" ends the run.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::native_spawn_probe {

// Install the record when the row is on; once it has, a call returns at the latch. Re-entered every pump tick.
void Install(coop::net::Session* session);

// The pump tick's entry: the orb drive. A latched read when the row is off.
void Tick(coop::net::Session* session);

// Session end: the per-key counts are printed and the records cleared; an orb drive mid-way is abandoned.
void OnDisconnect();

}  // namespace coop::dev::native_spawn_probe
