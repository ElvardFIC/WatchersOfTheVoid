// coop/dev/explosion_drill.h -- [dev] the explosion lane's drill (ini explosion_drill=off|dissolve|apply|coast|
// spawn_malformed|source_host|source_client / env VOTVCOOP_EXPLOSION_DRILL). Each arm runs only the roles named here.
// dissolve (CLIENT acts): a bomb barrel is spawned and broken in the world load (the dissolve comes by class name)
// and in the loaded world (by class); its explosion must never reach begin-play, and the world half also that the
// dissolved actor is gone. apply, coast, spawn_malformed (HOST and CLIENT; nobody walks or is teleported): a blast
// point B near the client; apply blasts the three classes (each applied once, narrowed, rune check refused, nothing
// dropped), coast blasts papers between B and the puppet (two client copies fly over 100 cm, all four rest, none is
// still driven), spawn_malformed sends one valid ExplosionSpawn and four with one broken field (each dropped).
// source_host, source_client (the named role acts, the other only reads; a count, no verdict): which peer's game
// reaches the birth of a grenade, a pipe bomb, a rocket, a sticky bomb and a salt rocket, through the game's own hand
// verbs; a refused verb ends its leg alone. Every wait is a counter's rise, a rest (under 5 cm/s for 10 Ticks) or a
// registry state, with a failure bound: 60 s per phase, the client's join and verdicts 120 s from its world-ready edge.
// "[EXPLOSION-DRILL] FAIL" is a step failing (--fail-marker), "[EXPLOSION-DRILL] ABANDONED" the drill unable to do
// its part (--dead-marker), "[EXPLOSION-DRILL] client DONE PASS arm=dissolve" the run's end (--done-marker), and the
// same for each arm. With ini explosion_birth_log on, the host also logs "[EXPLOSION] host birth src=" per blast.

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
