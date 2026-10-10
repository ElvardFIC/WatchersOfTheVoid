// harness/fingerprint_census.h -- the bytecode-fingerprint census of the functions the mod depends on
// (docs/bytecode-fingerprint.md). Runs once gameplay is loaded, beside the SDK check: every function the
// mod watches or hooks is fingerprinted on the running game and compared with the fingerprint of the
// build the mod was made for.
//
// [dev] fingerprint_capture writes every function of every loaded Blueprint class with its canonical
// text to multivoid-fingerprints-capture.txt beside the exe: the mod's half of the comparison with the
// text rendered from the cooked assets, and the native half of a build's reference table.

#pragma once

namespace harness::fingerprint_census {

// Start a pass over every row (and the capture when its flag is on). Game thread, after the world is up,
// posted with sdk_check::Run; the rows are checked over the next frames by Tick.
void Run();

// A 2 ms slice of a pass while one runs, then a re-try of the rows whose class was not loaded when the
// class set moves or every 5 s. A posted task of its own (harness/session_runtime posts it when Due),
// so a fault in another lane's per-frame work cannot keep the census from finishing. Game thread.
void Tick();

// Is a Tick due: every frame while a pass runs, once a second while rows wait for their class, never
// when idle. Any thread (the runtime thread asks before posting).
bool Due();

}  // namespace harness::fingerprint_census
