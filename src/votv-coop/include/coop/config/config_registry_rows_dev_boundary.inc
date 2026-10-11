// coop/config/config_registry_rows_dev_boundary.inc -- the receive boundary's drills: the drills that
// forge what a peer sends and prove the receiving peer's boundary refuses it, grouped as one concept.
// Included from config_registry_rows_dev.inc the way _dev_port.inc is, so each row keeps its place in
// every expansion of the list. Entry forms are config_registry_rows.inc's.
//
// One rule for every row here: a row arms the SENDING side only. The receiving side's gates have no
// knob, no dev branch and no env check, so a drill exercises the real refusal; a drill's red, the
// proof that it can fail, is run on bytes without the gate (the commit before the fix, or a lane that
// deletes the gate in source and never lands), never by a switch in the shipped DLL.

// The admission drill, client side only (the rule above): a knob on the host would make the drill's
// own pass conditional on a bypass existing in shipped code.
CFG_ENUM(auth_drill, "auth_drill", "dev", "off", "off|corrupt|silent|mismatch",
    "VOTVCOOP_AUTH_DRILL",
    "Sabotage THIS CLIENT's identity proof, to prove the host refuses it. corrupt = "
    "send a flipped signature; silent = open the exchange and never answer. Both must "
    "leave this peer unseated and with no save. mismatch = pretend we were sent to a "
    "DIFFERENT host than the one that answered, which THIS peer must refuse itself "
    "-- it is the only arm whose refusal is ours, not the host's. "
    "Drill only.")

// The hostile-peer drill: a client sends deliberately malformed coop packets of a chosen kind and the
// receiving host judges whether the boundary refused them WITHOUT stalling the session, the verdict
// printed by the mod so the rig reads a marker. Set on BOTH peers: the client sends, and on the host
// the row only names the judge; it changes nothing the host's gates do. Off by default.
CFG_ENUM(hostile_drill, "hostile_drill", "dev", "off",
    "off|signal_park|chat_ill_formed", "VOTVCOOP_HOSTILE_DRILL",
    "Receive-boundary drill (BOTH peers). signal_park: a client floods refused saved-signal rows "
    "(size 0, so the game's own gate rejects each) and the host must keep them from filling the apply "
    "park and pausing the receive loop, while an honest saved signal sent behind them still applies. "
    "chat_ill_formed: a client sends an ill-formed UTF-8 chat line; the host must refuse it and keep a "
    "well-formed one sent behind it. Drill only.")
