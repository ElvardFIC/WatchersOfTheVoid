// coop/config/config_registry_rows_dev_fire.inc -- the [dev] rows of the fire lane: its ignite drill. Included by
// config_registry_rows_dev.inc after the explosion rows, before the PI and NB rows, which stay the last ones, so each
// row keeps its place in every expansion of the list. The entry forms are config_registry_rows.inc's.

CFG_ENUM(fire_ignite_drill, "fire_ignite_drill", "dev", "off", "off|client|host", "VOTVCOOP_FIRE_IGNITE_DRILL",
    "Drill, the salt rocket's ignite lane; set on BOTH peers, the arm names who lights. client: the host spawns a "
    "salt rocket before the client's puppet and the client lights it with a lighter; the host must perform that "
    "light on the client's word (a fuse, a flight, a blast) while the client's own copy never fuses. host: the "
    "host spawns a rocket before its own player and lights it; the client must see it fly and blow. Each peer "
    "prints one DONE line on a pass. Run it on a fresh host world.")
