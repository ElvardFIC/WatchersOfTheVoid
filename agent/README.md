# Agent context

The user leads the project. Assistant scope is bug fixes only; `another agent` is
retired. Historical archives and backup copies were removed at the owner's request.

Use the three active work documents: [TODO.md](TODO.md) for current priorities,
[WORK.md](WORK.md) for the unified backlog and implementation progress, and
[TESTLOG.md](TESTLOG.md) for user reports and verification status. Update them in
place; do not create numbered versions or parallel tracking files.

Completed per-change WORK reports have been removed; unresolved implementation and
gameplay follow-ups remain in WORK.md.

The Git checkout's agent/ is the only active copy. Run tests and tools from the
checkout; copied relative-path test fixtures are not a second independently runnable
test tree. Do not retain backup source trees or historical test outputs.
