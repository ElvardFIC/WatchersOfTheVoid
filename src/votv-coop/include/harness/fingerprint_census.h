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

// Game thread, after the world is up (posted with sdk_check::Run). A single flag read when the capture
// is off and no census row is due.
void Run();

}  // namespace harness::fingerprint_census
