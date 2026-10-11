// harness/compat_report.cpp -- see harness/compat_report.h.

#include "harness/compat_report.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/paths.h"
#include "ue_wrap/core/sdk_profile.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace harness::compat_report {
namespace {

struct Section {
    std::string              name;
    std::vector<std::string> lines;
    int                      failures[3] = {};  // by Severity
};
std::vector<Section> g_sections;  // game thread only

Section& Find(const char* name) {
    for (Section& s : g_sections)
        if (s.name == name) return s;
    g_sections.push_back(Section{name, {}});
    return g_sections.back();
}

}  // namespace

void Begin(const char* section) {
    Section& s = Find(section);
    s.lines.clear();
    std::memset(s.failures, 0, sizeof s.failures);
}

void Line(const char* section, const std::string& text) { Find(section).lines.push_back(text); }

void Failure(const char* section, Severity severity, int count) {
    Find(section).failures[static_cast<int>(severity)] += count;
}

void Write() {
    const std::wstring path = ue_wrap::paths::ExeDir() + L"\\multivoid-compat-report.txt";
    FILE* f = nullptr;
    // Text mode: the file is read in Notepad by the player who attaches it, so its lines end in CRLF.
    if (_wfopen_s(&f, path.c_str(), L"w") != 0 || !f) {
        UE_LOGW("compat-report: failed to open '%ls' for writing", path.c_str());
        return;
    }
    int priority = 0;
    for (const Section& s : g_sections) {
        for (const std::string& line : s.lines) std::fprintf(f, "%s\n", line.c_str());
        std::fputc('\n', f);
        priority += s.failures[static_cast<int>(Severity::Critical)] + s.failures[static_cast<int>(Severity::Important)];
    }
    std::fprintf(f, "=== VERDICT ===\n");
    for (const Section& s : g_sections)
        std::fprintf(f, "  %-20s %d critical, %d important, %d cosmetic\n", s.name.c_str(), s.failures[0], s.failures[1],
                     s.failures[2]);
    if (priority == 0) {
        std::fprintf(f, "Status: HEALTHY (no critical or important failure against %s)\n",
                     ue_wrap::profile::kTargetGameVersion);
    } else {
        std::fprintf(f, "Status: DEGRADED -- the game differs from the build this mod was made for (%s).\n\n"
                        "Share this report when filing an issue at the mod repository: it names every\n"
                        "divergence, and the maintainer builds the mod for the current VOTV release from it.\n",
                     ue_wrap::profile::kTargetGameVersion);
    }
    std::fclose(f);
    UE_LOGI("compat-report: wrote '%ls' (%zu section(s), %s)", path.c_str(), g_sections.size(),
            priority ? "DEGRADED" : "HEALTHY");
}

}  // namespace harness::compat_report
