# Synchronization continuation for package 0.9.2157

Development protocol/DLL: **2185**. Public release 0.9.2157 remains the previously
published package; this continuation does not replace its archive or deploy a server.

## Reported failures

The owner reports that player flashlights, light switches, garage/base doors and
the client's drone-container contents fail immediately after joining 2157.
These reports remain open for acceptance with the new matching DLLs.

### Script watch registration

The original 2048-slot table admits only 768 keyed entries at its 3/8 load ceiling.
Each resolved name watch retains a placeholder and a resolved entry, so the
effective budget is 384 watches. The combined controllers exceed that budget;
the previous release notes already record refused watches during native startup.
Door, light-group, toggle and container verbs use this shared registry.

The table now has 8192 slots with the same load ceiling: 3072 keyed entries,
or 1536 placeholder/resolved watch pairs. Registration and retirement live in
their own production translation unit and are exercised directly by a fixture
with 1100 pairs and eight reopen cycles. The original capacity fails the same
fixture. This fixes the registration defect; it does not prove every reported
in-game door or switch failure had that cause.

### Flashlight state

The state sampler's class hash is initialized before the player's Blueprint class
or ProcessEvent observers are available. A component that is not ready, or a
reflected setter that refuses, leaves the latest incoming state pending.
Completion checks its revision so an older apply cannot erase a newer payload.
Native calls validate the pawn, component and world before continuing.

A connect seed no longer satisfies the regular broadcast's deduplication state.
Consequently, a refused seed or missing initial pawn can still publish the current
local state when the sampler runs. Existing battery-empty, mode, unequip and
send-refusal behavior is retained.

### Container initial contents

A targeted join seed previously fell back into the ordinary dirty drain. That
drain could see the same hash as an earlier successful broadcast and report
success without sending the new peer any contents. Unreadable GObjStack contents
also incorrectly completed a seed as though there were nothing to send.

Seeds now wait for class/component/contents readiness and retry specifically to
the original seat's occupancy generation. Another recipient accepting a broadcast
cannot retire that debt. The bounded retry cursor serves at most 16 pending
container/recipient pairs each sweep and preserves newer or reentrant work.
Destroyed containers, departed occupancies and session teardown end their jobs.

The fixture compiles the actual publication, connect and retry functions. It
covers an old published hash, refused transport, one blocked recipient beside a
successful recipient, missing class/component/contents, seat reuse, destroyed
actors and callback replacement/teardown. The old dirty-drain retry fails its
negative control.

## Choosing 2–32 players

The native hosting screen has minus/plus controls beside the session name; the
fallback ImGui browser has a Max players slider. The value includes the host,
defaults to four, and is clamped to 2–32. HostWithSave carries the choice through
direct, hidden, automatic and fallback configurations. Host admission enforces
the selected limit rather than just advertising it.

Capacity is shared by the player registry, join seeder, protocol acknowledgements
and transport arrays. Recipient masks are 32 bits. Held-food addresses now reserve
five seat bits, and world-sound delivery rotates its bounded recipient budget.
The largest generator-state payload remains exactly 228 bytes inside the existing
256-byte transport. All participating peers must use protocol 2185.

The master source accepts `players_max` from 2 through 32. An already running
master with the old four-seat clamp must be updated for automatic joins beyond
four players. No deployed master was changed. Rust compilation was not run because
this workspace has no Cargo toolchain.

## Validation and acceptance

Focused entry point: `agent/tests/run_sync_2157.ps1`.
Flashlight/material regression entry point: `agent/tests/run_2145.ps1`.
Full qualification: `package/check_current.ps1 -BuildDirectory build/votv-coop-2154
-EvidenceDirectory build/sync-2157-qualified`.

The first full run exposed two fixtures that included script_gate.cpp without its
new registry implementation. Their shared fixture include is corrected and both
reruns pass. Windows PowerShell also treated a CMake dependency deprecation on
stderr as a terminating error; the runner now records stderr and checks CMake's
exit status. Neither correction relaxes an assertion or a build failure.

Final qualification passes **67/67 groups**, Release identity/ABI and 785 source
entries with unchanged production inputs. Results are in
`build/sync-2157-qualified/verification.json`. The final incremental build reports
no work remaining. Source snapshot SHA256: `9bc95f5f4c6c29e80604ccae7f23916b1ef80671f918513e668ecca558932908`.
DLL SHA256: `5b5b5a4a745f11c1e2dc31840f886b7188569a6faa77a94ef81451aaa8109207`.
DLL: `build/votv-coop-2154/main.dll`.
No ordinary rendered host/client session or 32-player session is claimed here.

Accept immediate join, both flashlight directions, short/held-F mode changes,
switches from each role, ordinary and garage door open/close, populated drone
contents and extraction, late join, disconnect/reconnect and a refused-send retry.
For capacity, exercise limits two and 32, refusal when full and reuse after the
previous occupant's teardown, then measure the host under a real larger group.

Fork findings and the 137-commit upstream comparison are in
[FORK_AUDIT_20261022.md](FORK_AUDIT_20261022.md).
