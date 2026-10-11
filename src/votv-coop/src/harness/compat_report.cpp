// harness/compat_report.cpp -- see harness/compat_report.h.

#include "harness/compat_report.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/paths.h"
#include "ue_wrap/core/sdk_profile.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace harness::compat_report {
namespace {

struct Section {
    std::string              name;
    std::vector<std::string> lines;
    int                      failures[3] = {};  // by Severity
    bool                     pending = false;   // still checking: the verdict waits for it
};
// Cold: a handful of calls a session. The boot thread and the game thread both write it, and the lock
// keeps their turns apart.
std::mutex           g_mu;
std::vector<Section> g_sections;
std::string          g_note;  // the verdict line's closing sentence, or empty

Section& Find(const char* name) {
    for (Section& s : g_sections)
        if (s.name == name) return s;
    g_sections.push_back(Section{name, {}});
    return g_sections.back();
}

void BeginLocked(const char* section) {
    Section& s = Find(section);
    s.lines.clear();
    std::memset(s.failures, 0, sizeof s.failures);
    s.pending = false;
}

}  // namespace

void Begin(const char* section) {
    std::lock_guard<std::mutex> lock(g_mu);
    BeginLocked(section);
}

void Pending(const char* section, const std::string& line) {
    std::lock_guard<std::mutex> lock(g_mu);
    BeginLocked(section);
    Section& s = Find(section);
    s.lines.push_back(line);
    s.pending = true;
}

void Line(const char* section, const std::string& text) {
    std::lock_guard<std::mutex> lock(g_mu);
    Find(section).lines.push_back(text);
}

void Failure(const char* section, Severity severity, int count) {
    std::lock_guard<std::mutex> lock(g_mu);
    Find(section).failures[static_cast<int>(severity)] += count;
}

void SetVerdictNote(const std::string& note) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_note = note;
}

void Write(bool announce) {
    std::lock_guard<std::mutex> lock(g_mu);
    const std::wstring path = ue_wrap::paths::ExeDir() + L"\\multivoid-compat-report.txt";
    FILE* f = nullptr;
    // Text mode: the file is read in Notepad by the player who attaches it, so its lines end in CRLF.
    if (_wfopen_s(&f, path.c_str(), L"w") != 0 || !f) {
        UE_LOGW("compat-report: failed to open '%ls' for writing", path.c_str());
        return;
    }
    int priority = 0;
    std::string pending;
    for (const Section& s : g_sections) {
        for (const std::string& line : s.lines) std::fprintf(f, "%s\n", line.c_str());
        std::fputc('\n', f);
        priority += s.failures[static_cast<int>(Severity::Critical)] + s.failures[static_cast<int>(Severity::Important)];
        if (s.pending) pending += (pending.empty() ? "" : ", ") + s.name;
    }
    std::fprintf(f, "=== VERDICT ===\n");
    for (const Section& s : g_sections)
        std::fprintf(f, "  %-20s %d critical, %d important, %d cosmetic\n", s.name.c_str(), s.failures[0], s.failures[1],
                     s.failures[2]);
    const std::string note = g_note.empty() ? std::string() : "; " + g_note;
    if (priority == 0 && !pending.empty()) {
        std::fprintf(f, "Status: PENDING -- %s still checking; this file is written again when it finishes%s\n",
                     pending.c_str(), note.c_str());
    } else if (priority == 0) {
        std::fprintf(f, "Status: HEALTHY (no critical or important failure against %s%s)\n",
                     ue_wrap::profile::kTargetBuild, note.c_str());
    } else {
        std::fprintf(f, "Status: DEGRADED -- the game differs from the build this mod was made for (%s%s).\n\n"
                        "Share this report when filing an issue at the mod repository: it names every\n"
                        "divergence, and the maintainer builds the mod for the current VOTV release from it.\n",
                     ue_wrap::profile::kTargetBuild, note.c_str());
    }
    std::fclose(f);
    if (!announce) return;
    UE_LOGI("compat-report: wrote '%ls' (%zu section(s), %s)", path.c_str(), g_sections.size(),
            priority ? "DEGRADED" : (pending.empty() ? "HEALTHY" : "PENDING"));
}

}  // namespace harness::compat_report
