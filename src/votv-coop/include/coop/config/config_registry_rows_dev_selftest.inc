// coop/config/config_registry_rows_dev_selftest.inc -- the [dev] red arms of the selftests whose rows closed the
// [dev] list before the explosion rows: the permissions and action-log arms. Included by config_registry_rows_dev.inc
// at the place the rows stood, so each row keeps its place in every expansion of the list. The entry forms are
// config_registry_rows.inc's.

CFG_FLAG(selftest_break_permissions, "selftest_break_permissions", "dev", false, "VOTVCOOP_SELFTEST_BREAK_PERMISSIONS",
    "Red arm of the permissions selftest: its first case expects the opposite answer, so it must fail.")
CFG_FLAG(selftest_break_action_log, "selftest_break_action_log", "dev", false, "VOTVCOOP_SELFTEST_BREAK_ACTION_LOG",
    "Red arm of the commands selftest's action-log cases: the first acting case of each cases file expects "
    "no record, so it must fail.")
