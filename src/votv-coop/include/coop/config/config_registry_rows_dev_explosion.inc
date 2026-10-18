// coop/config/config_registry_rows_dev_explosion.inc -- the [dev] rows of the explosion lane: its drill's arms,
// the host's birth line and the red arms of its selftests. Included by config_registry_rows_dev.inc before the PI and
// NB rows, which stay the last ones, so each row keeps its place in every expansion of the list. The entry forms are
// config_registry_rows.inc's.

CFG_ENUM(explosion_drill, "explosion_drill", "dev", "off", "off|dissolve|apply|coast|spawn_malformed|source_host|source_client",
    "VOTVCOOP_EXPLOSION_DRILL",
    "Drill, the explosion lane's arms; each runs the roles it names. dissolve (the CLIENT acts, the host only "
    "hosts): a bomb barrel is spawned and broken once while the client's world loads and once in the loaded world; "
    "its explosion must be dissolved at its birth (matched by class name while the class table is unresolved in "
    "the load, by class in the world), must never run its begin-play, and its unfinished actor must be gone. apply "
    "(HOST and CLIENT): the host blasts the three explosion classes ahead of the client; the client must apply "
    "each once, narrow its force, refuse its rune check and drop nothing. coast (HOST and CLIENT): four papers lie "
    "between the blast and the client's puppet; at least two client copies must fly, all four must rest again "
    "and none may still be driven. spawn_malformed (HOST and CLIENT): the host "
    "sends one valid explosion and four with a broken field; the client must drop each by its reason. source_host "
    "and source_client (a measurement, no verdict): the named role walks the legs grenade, pipebomb, rocket and "
    "sticky through the game's hand verbs, then saltrocket (spawned in the world and lit by its attemptIgnite, "
    "attached to a simulating parent, then free); the other role only "
    "reads and reports; each leg prints one line and the run ends on a source DONE line. Run it on a fresh host "
    "world.")
CFG_FLAG(explosion_birth_log, "explosion_birth_log", "dev", false, "VOTVCOOP_EXPLOSION_BIRTH_LOG",
    "Off by default. When on, a host or single-player game logs one line per explosion born, naming the class of "
    "the Blueprint that spawned it (the client already logs this when it dissolves one). It reads the class of "
    "every Blueprint deferred spawn on the host, so leave it off outside a measurement run.")
CFG_FLAG(selftest_break_hand_history, "selftest_break_hand_history", "dev", false, "VOTVCOOP_SELFTEST_BREAK_HAND_HISTORY",
    "Red arm of the hand history selftest: its first Note is skipped, so every check that reads that hold must fail.")
CFG_FLAG(selftest_break_explosion_recipe, "selftest_break_explosion_recipe", "dev", false, "VOTVCOOP_SELFTEST_BREAK_EXPLOSION_RECIPE",
    "Red arm of the explosion recipe selftest: the player's row reads the unclamped level, so the clamp checks must fail.")
