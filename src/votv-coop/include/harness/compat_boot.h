// harness/compat_boot.h -- the sections of multivoid-compat-report.txt the boot writes: the game build this
// process runs, first, then the engine's -- the boot's health check -- and either the world checks marked
// pending or that the mod stood down.
//
// Two writes, both on the boot thread and both before anything of ours runs on the game thread (so the
// report's sequences never interleave): the first before the health check, so a boot that crashes or stands
// down still leaves a report whose first line names the build, and the second once the health check has
// decided (docs/VERSION_PORT_DRILL_ARC.md 8.3). The world checks (harness/sdk_check, harness/fingerprint_census)
// replace their pending lines with their results.

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

}  // namespace harness::compat_boot
