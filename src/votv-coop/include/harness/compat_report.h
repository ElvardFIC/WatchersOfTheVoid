// harness/compat_report.h -- the one writer of multivoid-compat-report.txt, the file a bug report
// attaches: what the running game is, measured against the build the mod was made for. Each check
// (harness/sdk_check, harness/fingerprint_census) owns a section; a section's lines and failures replace
// that section's previous run, and the file is rewritten whole when a check finishes, its one verdict last.

#pragma once

#include <string>

namespace harness::compat_report {

// What a failure costs. Critical: the mod cannot run (the engine substrate). Important: a feature is
// silently disabled or reads the game wrong. Cosmetic: a dev tool or a visual nicety.
enum class Severity { Critical, Important, Cosmetic };

// Start `section` afresh: its lines and failures from an earlier run are dropped. Game thread.
void Begin(const char* section);

// Append a line to `section`. Game thread.
void Line(const char* section, const std::string& text);

// Count `count` failures of `severity` against `section`, for the file's verdict. Game thread.
void Failure(const char* section, Severity severity, int count = 1);

// Rewrite the file beside the exe with every section, in the order the sections first began, then the one
// verdict over all of them: DEGRADED when any section counted a Critical or Important failure. Game thread.
void Write();

}  // namespace harness::compat_report
