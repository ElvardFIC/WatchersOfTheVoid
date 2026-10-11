// coop/game_build/game_build.cpp -- see coop/game_build/game_build.h.

#include "coop/game_build/game_build.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "ue_wrap/core/log.h"

#include <cstring>
#include <mutex>

namespace coop::game_build {
namespace {

namespace GB = ue_wrap::game_build;

Detected g_running;
std::once_flag g_once;

struct LastBuild {
    const char* version;
    unsigned    build;
};
// The table may be empty: the sentinel row ends it and is never a version.
constexpr LastBuild kLastBuilds[] = {
#define LAST_BUILD(version, build) {version, build},
#include "coop/game_build/last_builds.inc"
#undef LAST_BUILD
    {nullptr, 0},
};

Kind KindOf(const GB::Row* row) {
    const GB::Row* target = GB::Target();
    if (!row) return Kind::Untested;
    if (row == target) return Kind::Target;
    return GB::OrderOf(row) > GB::OrderOf(target) ? Kind::Newer : Kind::Older;
}

// The phrase for a decided build. Running() logs it while still deciding, so it reads the value it is
// handed and never Running() itself: a call_once re-entered from its own thread does not return.
std::string DescribeOf(const Detected& d) {
    if (!d.row) return "an untested version (" + std::string(d.id.key) + ")";
    return std::string("VOTV ") + d.row->display + " (" + KindWord(d.kind) + ")";
}

}  // namespace

const Detected& Running() {
    std::call_once(g_once, [] {
        Detected d;
        const GB::Identity& real = GB::Running();  // read, and logged, whatever the drill arm says
        // The drill arm: a row's id, or "unknown" for a build no row names.
        const std::string force = coop::config::ResolveString(coop::config_registry::rows::game_build_force);
        if (force == "unknown") {
            d.id = GB::UnknownIdentity();
            d.forced = true;
        } else if (const GB::Row* row = force.empty() ? nullptr : GB::FindById(force.c_str())) {
            d.id = GB::IdentityOf(*row);
            d.forced = true;
        } else {
            if (!force.empty())
                UE_LOGW("game-build: [dev] game_build_force='%s' is no row's id and not 'unknown' -- ignored",
                        force.c_str());
            d.id = real;
        }
        d.row = GB::Match(d.id);
        d.kind = KindOf(d.row);
        g_running = d;
        UE_LOGI("game-build: running %s%s -- key %s", DescribeOf(d).c_str(),
                d.forced ? " [FORCED by [dev] game_build_force]" : "", d.id.key);
    });
    return g_running;
}

const char* KindWord(Kind kind) {
    switch (kind) {
    case Kind::Target: return "the target";
    case Kind::Newer:  return "newer";
    case Kind::Older:  return "older";
    case Kind::Untested: break;
    }
    return "untested";
}

std::string Describe() { return DescribeOf(Running()); }

bool AnswerOwed() {
    const Detected& d = Running();
    if (d.kind == Kind::Target) return false;
    return coop::config::ResolveString(coop::config_registry::rows::game_build_ack) != d.id.key;
}

void Acknowledge() {
    coop::config::SetValue(coop::config_registry::rows::game_build_ack, Running().id.key);
}

unsigned LastBuildFor(const char* version) {
    if (!version) return 0;
    for (const LastBuild& last : kLastBuilds)
        if (last.version && std::strcmp(last.version, version) == 0) return last.build;
    return 0;
}

std::string UntestedDetail() {
    const Detected& d = Running();
    if (d.row) return {};
    const GB::Row* exe = GB::MatchExe(d.id);
    if (!d.id.pakRead) {
        return (exe ? "its executable is " + std::string(exe->display) + "'s; " : std::string()) +
               "its game data could not be read: " + (d.id.pakProblem ? d.id.pakProblem : "no reason given");
    }
    const GB::Row* pak = GB::MatchPak(d.id);
    if (exe) return std::string("its executable is ") + exe->display + "'s, its game data no known build's";
    if (pak) return std::string("its game data is ") + pak->display + "'s, its executable no known build's";
    return "no known build";
}

}  // namespace coop::game_build
