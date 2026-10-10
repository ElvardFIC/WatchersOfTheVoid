// harness/fingerprint_census.h -- the bytecode-fingerprint census of the functions the mod depends on
// (docs/VERSION_PORT_DRILL_ARC.md systems 2 and 3, VP-1). Runs once gameplay is loaded, beside the SDK
// check: every function the mod watches or hooks is fingerprinted on the running game and compared
// with the fingerprint of the build the mod was made for.
//
// [dev] fingerprint_capture writes every function of every loaded Blueprint class (and the census
// rows) with its canonical text to multivoid-fingerprints-capture.txt beside the exe: the input of the
// agreement test against tools/port/bp_fingerprint.py, and of the reference table a port generates.

#pragma once

namespace harness::fingerprint_census {

// Game thread, after the world is up (posted with sdk_check::Run). A single flag read when the capture
// is off and no census row is due.
void Run();

}  // namespace harness::fingerprint_census
