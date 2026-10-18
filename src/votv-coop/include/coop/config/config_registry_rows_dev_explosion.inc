// coop/config/config_registry_rows_dev_explosion.inc -- the [dev] rows of the explosion lane: the red arms of
// its selftests. Included by config_registry_rows_dev.inc after its last row, as _dev_nb.inc is, so each row
// keeps its place in every expansion of the list. The entry forms are config_registry_rows.inc's.

CFG_FLAG(selftest_break_hand_history, "selftest_break_hand_history", "dev", false, "VOTVCOOP_SELFTEST_BREAK_HAND_HISTORY",
    "Red arm of the hand history selftest: its first Note is skipped, so every check that reads that hold must fail.")
