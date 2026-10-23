// coop/dev/explosion_drill.h -- [dev] the explosion lane's drill (ini explosion_drill=off|dissolve|apply|coast|
// spawn_malformed|source_host|source_client|request_* / env VOTVCOOP_EXPLOSION_DRILL). Each arm runs the roles named.
// dissolve (CLIENT acts): a bomb barrel is broken in the world load (the dissolve comes by class name) and in the
// loaded world (by class); its explosion must never reach begin-play, and the dissolved actor must be gone.
// apply, coast, spawn_malformed (HOST and CLIENT; nobody walks): a blast point B near the client; apply blasts the
// three classes (each applied once, narrowed, rune check refused, nothing dropped), coast blasts papers between B and
// the puppet (two copies fly over 100 cm, all four rest, none driven), spawn_malformed sends one valid ExplosionSpawn
// and four with one broken field (each dropped).
// source_host, source_client (the named role acts, the other reads; a count, no verdict): which peer's game reaches
// the birth of each explosion source, through the game's own verbs (a refused one ends its leg alone).
// request_* (HOST and CLIENT, a verdict): the client's game causes ONE explosion of its own (a rocket, sticky bombs,
// a held grenade or pipe bomb, the pilled player ...); each half counts its own request lane, the host's blast reaches
// the client once, and PASS prints only after the arm's projectiles are gone. Every wait is a counter's rise, a rest or
// a registry state, bounded (60 s per phase, 120 s for the client's join and each request half's verdict). Markers:
// "[EXPLOSION-DRILL] FAIL" (--fail-marker), "ABANDONED" (--dead-marker), "host DONE PASS arm=<arm>" (--done-marker).

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::explosion_drill {

// The world half of dissolve, or either role of the other arms. Game thread, once per gameplay pump tick; a latched
// string compare when off. request_witch is the one request arm with a seat for a launcher: the host spawns a witch
// car, the client walks to it with the director, sits in it and fires its rocket, and the host judges the shot and the
// blast apart (an expired rocket leaves the blast half INCONCLUSIVE).
void Tick(coop::net::Session* s);

// The load half of dissolve. Game thread, once per pump tick, the world load included.
void TickLoad(coop::net::Session* s);

// Back to the start: a rejoin's peers run their halves again; the host first destroys the papers its coast spawned.
// A witch arm that wrote the mode and isHalloween puts them back.
void OnDisconnect();

}  // namespace coop::dev::explosion_drill
