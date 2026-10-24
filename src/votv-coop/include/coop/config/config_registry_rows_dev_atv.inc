// coop/config/config_registry_rows_dev_atv.inc -- the [dev] rows of the ATV lane's health drill and its red arm.
// Included by config_registry_rows_dev.inc after the fire rows, before the PI and NB rows, which stay the last ones,
// so each row keeps its place in every expansion of the list. The entry forms are config_registry_rows.inc's.

CFG_ENUM(atv_health_drill, "atv_health_drill", "dev", "off", "off|sit|grab", nullptr,
    "Drill, the ATV lane's table; set on BOTH peers, the arm names what the client does to the world's own ATV. sit: "
    "the client walks to the ATV with the director, takes its seat and leaves it; the host must name the client as "
    "occupant and syncer, then free the seat, then take the ATV back once it rests. grab: the client grabs the ATV "
    "with the grav hand and lets it go; the host must name the client as syncer with the seat free, then take the "
    "ATV back at rest. Each peer prints one DONE line on a pass, the host's with the times it measured. Run it on a "
    "fresh host world; only the client in slot 1 acts.")
CFG_ENUM(atv_health_red, "atv_health_red", "dev", "off", "off|back", nullptr,
    "Developer red arm: never set. back (HOST): the at-rest hand-back of a client-synced ATV is skipped, so the ATV "
    "health drill's `back` phase must fail; range and silence still take an ATV back.")
