// coop/config/config_registry_rows_dev_atv.inc -- the [dev] rows of the ATV lane's health drill and its red arm.
// Included by config_registry_rows_dev.inc after the fire rows, before the PI and NB rows, which stay the last ones,
// so each row keeps its place in every expansion of the list. The entry forms are config_registry_rows.inc's.
// The drill's arms: sit, grab, occupied, synced, silent; the red arm's values: back, occupied, takeover, silence.

CFG_ENUM(atv_health_drill, "atv_health_drill", "dev", "off", "off|sit|grab|occupied|synced|silent",
    "VOTVCOOP_ATV_HEALTH_DRILL",
    "Drill, the ATV lane's table; set on BOTH peers, the arm names what is done to the world's own ATV. sit: "
    "the client walks to the ATV with the director, takes its seat and leaves it; the host must name the client as "
    "occupant and syncer, then free the seat, then take the ATV back once it rests. grab: the client grabs the ATV "
    "with the grav hand and lets it go; the host must name the client as syncer with the seat free, then take the "
    "ATV back at rest. occupied: the host sits in the ATV; the client's raw sit request must be refused as occupied "
    "and grant nothing. synced: the host grabs the ATV; the client's raw takeover request must be refused as synced "
    "and grant nothing. silent: the client sits and then stops streaming; the host must take the seat and the ATV "
    "back by its silence rule, and the client's own copy must give up the seat. Each peer prints one DONE line on a "
    "pass, the host's with the times it measured. Run it on a fresh host world; only the client in slot 1 acts.")
CFG_ENUM(atv_health_red, "atv_health_red", "dev", "off", "off|back|occupied|takeover|silence",
    "VOTVCOOP_ATV_HEALTH_RED",
    "Developer red arm: never set. back (HOST): the at-rest hand-back of a client-synced ATV is skipped, so the ATV "
    "health drill's `back` phase must fail; range and silence still take an ATV back. occupied (HOST): a sit on an "
    "occupied seat is not refused, so the `occupied` arm must fail. takeover (HOST): a takeover of an ATV the host "
    "carries is not refused, so the `synced` arm must fail. silence (HOST): a silent syncer is not taken back, so the "
    "`silent` arm must fail.")
