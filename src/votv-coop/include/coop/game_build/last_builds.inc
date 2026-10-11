// coop/game_build/last_builds.inc -- the last Multivoid build for each game version the mod has moved past.
//
// LAST_BUILD(version, build)
//   version  a game version as a Multivoid target names it ("0.9.0n"): a ue_wrap/facts/game_builds.inc row's version
//   build    the last Multivoid release made for it (its kProtocolVersion)
//
// The mod's own release history, so it is coop's and not the game-build rows' (principle 7). A retarget adds the
// previous target's line (docs/GAME_UPDATE_RUNBOOK.md section 7); the version popup then tells a player on that
// older game which Multivoid to use. Empty while 0.9.0n is the only target. Included by coop/game_build only.
