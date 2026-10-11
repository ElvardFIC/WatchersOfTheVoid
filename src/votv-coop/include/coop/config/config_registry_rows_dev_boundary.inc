// coop/config/config_registry_rows_dev_boundary.inc -- the receive boundary's drills (the RB arc). The
// drills that forge what a peer sends and prove the host's receive boundary refuses it, grouped as one
// concept. Included from config_registry_rows_dev.inc the way _dev_port.inc is, so each row keeps its
// place in every expansion of the list. Entry forms are config_registry_rows.inc's.

// The admission drill, client side only, and that is the whole design: the host's gate has no
// knob, no dev branch and no env check anywhere, so the drill exercises the real refusal or
// it proves nothing; a knob on the host would make the drill's own pass conditional on a
// bypass existing in shipped code.
CFG_ENUM(auth_drill, "auth_drill", "dev", "off", "off|corrupt|silent|mismatch",
    "VOTVCOOP_AUTH_DRILL",
    "Sabotage THIS CLIENT's identity proof, to prove the host refuses it. corrupt = "
    "send a flipped signature; silent = open the exchange and never answer. Both must "
    "leave this peer unseated and with no save. mismatch = pretend we were sent to a "
    "DIFFERENT host than the one that answered, which THIS peer must refuse itself "
    "-- it is the only arm whose refusal is ours, not the host's. "
    "Drill only.")

// The hostile-peer drill: a client sends deliberately malformed coop packets of a chosen kind and the
// receiving host judges whether the boundary refused them WITHOUT stalling the session -- red on the
// vulnerable bytes, green on the fixed ones, the verdict printed by the mod so the rig reads a marker.
// Set on BOTH peers (the client sends, the host judges). Off by default; the enum gates every path.
CFG_ENUM(hostile_drill, "hostile_drill", "dev", "off",
    "off|signal_park|chat_ill_formed|chat_ill_formed_red", "VOTVCOOP_HOSTILE_DRILL",
    "Receive-boundary drill (BOTH peers). signal_park: a client floods refused saved-signal rows "
    "(size 0, so the game's own gate rejects each) and the host must keep them from filling the apply "
    "park and pausing the receive loop, while an honest saved signal sent behind them still applies. "
    "chat_ill_formed: a client sends an ill-formed UTF-8 chat line; the host must refuse it and keep a "
    "well-formed one sent behind it. chat_ill_formed_red: the MUST-FAIL control -- the host's chat "
    "UTF-8 guard is turned off, so the ill-formed line is committed and the drill prints FAIL, proving "
    "the guard is load-bearing. Drill only.")
