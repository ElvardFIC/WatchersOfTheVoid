// harness/compat_report.h -- the one writer of multivoid-compat-report.txt, the file a bug report
// attaches: what the running game is, measured against the build the mod was made for. Each check
// (harness/sdk_check, harness/fingerprint_census) owns a section; a section's lines replace that
// section's previous run, and the file is rewritten whole when a check finishes.

#pragma once

#include <string>

namespace harness::compat_report {

// Start `section` afresh: its lines from an earlier run are dropped. Game thread.
void Begin(const char* section);

// Append a line to `section`. Game thread.
void Line(const char* section, const std::string& text);

// Rewrite the file beside the exe with every section, in the order the sections first began. Game thread.
void Write();

}  // namespace harness::compat_report
