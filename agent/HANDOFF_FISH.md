# AquaFishhh source handoff

Historical source handoff; current priorities are in [TODO.md](TODO.md) and
[WORK.md](WORK.md). Current development: 2161 / b2217.
Upstream through `ad5315611` is now merged; the unmerged status below describes
the original handoff snapshot only.

Integrated locally into main as protocol 2176 on 21 October 2026. This file
records the original branch, not the integrated build validation; see WORK.md
and TESTLOG.md for adaptations, retained main behavior and current results.

Branch: `watchers-of-the-void-fish` in `ElvardFIC/WatchersOfTheVoid`.
Game target: Voices of the Void 0.9.0n. Current protocol/build: 2175.

This branch preserves the current local source tree on baseline
`b6651cb91d4a7ee5ddc9c92fc558d3e3913b3b00`. It contains the accumulated
non-Entity synchronization work described in WORK.md, TODO.md and TESTLOG.md.
Existing Entity code inherited from the baseline remains part of the source tree;
the continuation summarized here did not implement the other owner's Entity work.

At publication preparation, remote main was
`a1238fe4fe0c35460b2e2493365e689480929903`. That newer main has not been merged
into this source snapshot. Use the shared baseline to review the two sets of
changes and integrate the other owner's Entity work deliberately.

## Integration points

Check shared edits to protocol.h, sources.cmake, event_dispatch_world.cpp,
event_dispatch_intent.cpp, session_lanes.h, subsystems.cpp and native fingerprint
references. Preserve both sides' required dispatch, registration and build entries.
Resolve protocol and reliable-kind changes together; peers require matching wire
layouts and binaries. A historical build number in a test filename does not mean
that file is the newest implementation or protocol.

The latest continuation adds canonical host explosion/salt-rocket scorch decals,
cleaning identities, quiet joins and retirement. Private computer-game explosions
remain local. Known scorch provenance survives reconnect in the same world;
full save/world-reload provenance migration is still open. Grill terminal replies
survive transport backpressure and publication guards against actor key changes.

Personal resource debit/output acceptance/refund, ordinary paid cement repairs,
remaining dynamic grime sources and two-peer gameplay checks remain open. Do not
close composite TODO rows based only on offline regression results.

## Validation before this source handoff

Release b2175 passed with 1903 stable production/config inputs, 26 regression
runners, 89 positive executable invocations, 115 production-source compiles and
214 compiled fault controls. Native contracts used 58 corpus files compared with
the installed game pak. Reliable routing covered 223 kinds and 18 rejection
controls; the DLL ABI check and forbidden-import drill passed.

The selected 104-file public-prose sweep had zero findings. The full repository
prose gate still reported 13 baseline counters; the fact-row gate reported findings
over 524 rows/156 files, including missing live native-hook references. Full CI is
not claimed green. No two-peer gameplay test was run.

Qualified DLL SHA256:
`5C9E922F015903468AF40871B7FD04DCF8D0C0F1BFEE4F73C6F6D07C46B88083`.
DLLs, build output, local verification JSON and extracted game assets are excluded
from this source publication. Follow BUILDING.md for a fresh build. The current
focused regression entry points and coverage are listed in tests/README.md.
