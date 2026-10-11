// harness/compat_report.h -- the one writer of multivoid-compat-report.txt, the file a bug report
// attaches: what the running game is, measured against the build the mod was made for. Each part owns a
// section -- the boot's (harness/compat_boot: the game build and the engine's checks) and the world checks'
// (harness/sdk_check, harness/fingerprint_census); a section's lines and failures replace that section's
// previous run, and the file is rewritten whole when a part finishes, its one verdict last. Any thread: the
// boot thread writes it before the game thread runs anything of ours, the game thread after.

#pragma once

#include <string>

namespace harness::compat_report {

// What a failure costs. Critical: the mod cannot run (the engine substrate). Important: a feature is
// silently disabled or reads the game wrong. Cosmetic: a dev tool or a visual nicety.
enum class Severity { Critical, Important, Cosmetic };

// Start `section` afresh: its lines and failures from an earlier run are dropped.
void Begin(const char* section);

// Append a line to `section`.
void Line(const char* section, const std::string& text);

// Count `count` failures of `severity` against `section`, for the file's verdict.
void Failure(const char* section, Severity severity, int count = 1);

// A sentence the verdict line ends with, or "" for none: what the verdict is about when it is not plain from
// the sections -- the boot sets it when the game running is not the build the mod was made for.
void SetVerdictNote(const std::string& note);

// Start `section` afresh as still checking: `line` stands for it, and the verdict reads PENDING rather than
// HEALTHY until the section begins again with its result. A check that reports later marks itself so before
// a sooner one writes the file.
void Pending(const char* section, const std::string& line);

// Rewrite the file beside the exe with every section, in the order the sections first began, then the one
// verdict over all of them: DEGRADED when any section counted a Critical or Important failure. Says so in the log
// unless `announce` is false (a rewrite that only refreshes counts).
void Write(bool announce = true);

}  // namespace harness::compat_report
