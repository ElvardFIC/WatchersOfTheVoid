// harness/compat_boot.h -- the sections of multivoid-compat-report.txt the boot writes: the game build this
// process runs, first, then the engine's -- the boot's health check and every scan of the game's code
// (ue_wrap/core/scan_ledger), with what a miss takes away -- and either the world checks marked pending or
// that the mod stood down.
//
// Two writes on the boot thread, both before anything of ours runs on the game thread: the first before the
// health check, so a boot that crashes or stands down still leaves a report whose first line names the build,
// and the second once the health check has decided (docs/versioning.md, "Which game is running"). After them the game
// thread is the report's only writer -- the engine section's refresh here and the world checks
// (harness/sdk_check, harness/fingerprint_census) replacing their pending lines -- so no two writers' sequences
// interleave.

#pragma once

#include <string>

namespace harness::compat_boot {

inline constexpr const char* kBuildSection = "game-build";
inline constexpr const char* kEngineSection = "engine";

// Write 1, before the health check: the build section, the verdict's note when the build is not the target,
// and the engine section marked pending. Writes the file.
void WriteBuild();

// Write 2 on a boot that passed: the engine section, and the world checks marked pending. Writes the file.
void WritePassed();

// Write 2 on a boot that stood down: the engine section with `why`, CRITICAL. The world checks never run, so
// none is marked pending. Writes the file.
void WriteStandDown(const std::string& why);

// After a passing boot, every runtime tick (at the menu and in a world): when a scan was decided since the
// engine section was written -- the overlay's after the boot, the save hook's with a session -- rewrite the
// section and the file. One atomic load when nothing moved. Game thread.
void RefreshIfScansMoved();

}  // namespace harness::compat_boot
