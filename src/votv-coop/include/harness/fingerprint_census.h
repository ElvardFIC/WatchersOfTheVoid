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
// posted with sdk_check::Run; the rows are checked over the next frames.
void Run();

// Post the census's tick when one is due: a 2 ms slice each runtime loop (about 60 Hz) while a pass runs,
// its report in a tick of its own, then, once a second, a re-try of the unsettled rows when a waiting class
// row's class may have loaded (the class set moved, or 5 s went by) or a class loaded whose functions an
// any-class row checks; a report when a registration is refused; nothing when idle. A posted task of its
// own, so a fault in another lane's per-frame work cannot keep the census from finishing; never a second
// one while one is queued, and one slice a game-thread drain. The runtime thread calls it every loop.
void PostIfDue();

}  // namespace harness::fingerprint_census
