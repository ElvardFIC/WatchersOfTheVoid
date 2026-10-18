// coop/dev/explosion_drill.h -- [dev] the explosion lane's drill (ini explosion_drill=off|dissolve|apply|coast|
// spawn_malformed|source_host|source_client / env VOTVCOOP_EXPLOSION_DRILL). Each arm runs only the roles named here.
// dissolve (CLIENT acts): a bomb barrel is broken in the world load (the dissolve comes by class name) and in the
// loaded world (by class); its explosion must never reach begin-play, and the dissolved actor must be gone.
// apply, coast, spawn_malformed (HOST and CLIENT; nobody walks or is teleported): a blast point B near the client;
// apply blasts the three classes (each applied once, narrowed, rune check refused, nothing dropped), coast blasts
// papers between B and the puppet (two copies fly over 100 cm, all four rest, none still driven), spawn_malformed
// sends one valid ExplosionSpawn and four with one broken field (each dropped).
// source_host, source_client (the named role acts, the other only reads; a count, no verdict): which peer's game
// reaches the birth of a grenade, pipe bomb, rocket, sticky bomb, salt rocket, a sticky bomb on a moved and destroyed
// physics prop, and the pilled acting player (through the game's own verbs; a refused one ends its leg alone).
// Every wait is a counter's rise, a rest or a registry state, with a failure bound (60 s per phase, the client's join
// and verdicts 120 s, a source leg's steps 5 to 20 s). Markers: "[EXPLOSION-DRILL] FAIL" (--fail-marker), "ABANDONED"
// (--dead-marker), "client DONE PASS arm=dissolve" and the same per arm (--done-marker). With ini explosion_birth_log
// on, the host also logs "[EXPLOSION] host birth src=" per blast.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::explosion_drill {

// The world half of dissolve, or either role of the other arms. Game thread, once per gameplay pump tick; a latched
// string compare when off.
void Tick(coop::net::Session* s);

// The load half of dissolve. Game thread, once per pump tick, the world load included.
void TickLoad(coop::net::Session* s);

// Back to the start: a rejoin's peers run their halves again; the host first destroys the papers its coast spawned.
void OnDisconnect();

}  // namespace coop::dev::explosion_drill
