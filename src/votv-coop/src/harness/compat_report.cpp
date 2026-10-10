// harness/compat_report.cpp -- see harness/compat_report.h.

#include "harness/compat_report.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/paths.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace harness::compat_report {
namespace {

struct Section {
    std::string              name;
    std::vector<std::string> lines;
};
std::vector<Section> g_sections;  // game thread only

Section& Find(const char* name) {
    for (Section& s : g_sections)
        if (s.name == name) return s;
    g_sections.push_back(Section{name, {}});
    return g_sections.back();
}

}  // namespace

void Begin(const char* section) { Find(section).lines.clear(); }

void Line(const char* section, const std::string& text) { Find(section).lines.push_back(text); }

void Write() {
    const std::wstring path = ue_wrap::paths::ExeDir() + L"\\multivoid-compat-report.txt";
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
        UE_LOGW("compat-report: failed to open '%ls' for writing", path.c_str());
        return;
    }
    size_t bytes = 0;
    for (const Section& s : g_sections) {
        for (const std::string& line : s.lines) {
            std::fwrite(line.data(), 1, line.size(), f);
            std::fputc('\n', f);
            bytes += line.size() + 1;
        }
        std::fputc('\n', f);
        ++bytes;
    }
    std::fclose(f);
    UE_LOGI("compat-report: wrote '%ls' (%zu bytes, %zu section(s))", path.c_str(), bytes, g_sections.size());
}

}  // namespace harness::compat_report
