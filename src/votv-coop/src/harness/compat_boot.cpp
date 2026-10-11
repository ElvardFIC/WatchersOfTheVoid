// harness/compat_boot.cpp -- see harness/compat_boot.h.

#include "harness/compat_boot.h"

#include "coop/game_build/game_build.h"
#include "coop/net/protocol.h"
#include "coop/version.h"
#include "harness/compat_report.h"
#include "harness/fingerprint_census.h"
#include "harness/sdk_check.h"

#include <cstdio>

namespace harness::compat_boot {
namespace {

namespace CR = harness::compat_report;
namespace GB = coop::game_build;

std::string HashHex(const uint8_t* bytes, size_t n) {
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < n; ++i) {
        out.push_back(kHex[bytes[i] >> 4]);
        out.push_back(kHex[bytes[i] & 0xF]);
    }
    return out;
}

***REMOVED***
std::string RunningSentence(const GB::Detected& d) {
    if (!d.row) return "an untested version: " + GB::UntestedDetail() + ".";
    const std::string name = std::string("VOTV ") + d.row->display;
    switch (d.kind) {
    case GB::Kind::Target:   return name + " (the target).";
    case GB::Kind::Newer:    return name + " -- a newer version, untested.";
    case GB::Kind::Older:    return name + " -- an older version, untested.";
    case GB::Kind::Untested: break;
    }
    return name + ".";
}

// The engine section: the boot's health check, passed or the reason the mod stood down.
void EngineSection(const std::string& standDownWhy) {
    CR::Begin(kEngineSection);
    CR::Line(kEngineSection, "=== Engine ===");
    if (standDownWhy.empty()) {
        CR::Line(kEngineSection, "The boot's health check passed: the engine's objects, names and calls read as the "
                                 "profile says.");
        return;
    }
    CR::Line(kEngineSection, "Multivoid did not start: " + standDownWhy);
    CR::Line(kEngineSection, "The game runs on without it. multivoid.log has the details (look for 'STANDING DOWN').");
    CR::Failure(kEngineSection, CR::Severity::Critical);
}

}  // namespace

void WriteBuild() {
    const GB::Detected& d = GB::Running();
    char made[128];
    std::snprintf(made, sizeof made, "Multivoid %s b%u is made for VOTV %s.", coop::version::kGameTarget,
                  static_cast<unsigned>(coop::net::kProtocolVersion), coop::version::kGameTarget);
    const std::string pak = d.id.pakRead ? HashHex(d.id.pakHash, sizeof d.id.pakHash)
                                         : std::string("not read (") + (d.id.pakProblem ? d.id.pakProblem : "?") + ")";
    char stamp[16];
    std::snprintf(stamp, sizeof stamp, "%08x", d.id.exeStamp);
    CR::Begin(kBuildSection);
    CR::Line(kBuildSection, "=== Game build ===");
    CR::Line(kBuildSection, made);
    CR::Line(kBuildSection, "The game running: " + RunningSentence(d));
    CR::Line(kBuildSection, std::string("Identity: exe ") + stamp + ", pak index " + pak +
                                (d.forced ? "  [FORCED by [dev] game_build_force]" : ""));
    if (d.kind == GB::Kind::Target) {
        CR::SetVerdictNote({});
    } else {
        const std::string running =
            d.row ? std::string("VOTV ") + d.row->display + ", untested" : std::string("an untested version");
        CR::SetVerdictNote("the game running is " + running);
    }
    CR::Pending(kEngineSection, "engine: the boot's health check is running");
    CR::Write();
}

void WritePassed() {
    EngineSection({});
    CR::Pending(harness::sdk_check::kReportSection, "sdk-check: runs when the first gameplay world has loaded");
    CR::Pending(harness::fingerprint_census::kReportSection,
                "fingerprint-census: runs when the first gameplay world has loaded");
    CR::Write();
}

void WriteStandDown(const std::string& why) {
    EngineSection(why);
    CR::Write();
}

}  // namespace harness::compat_boot
