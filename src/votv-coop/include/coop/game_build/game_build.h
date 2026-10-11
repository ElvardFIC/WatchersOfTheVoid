// coop/game_build/game_build.h -- what the game build this process runs means for this Multivoid: the
// build it was made for, a known newer or older one, or one it has not been tested on.
//
// The reading is ue_wrap/core/game_build's (the exe's link stamp and the pak's index hash, matched
// against the known-build rows). This layer decides, names a build for a person, and holds the drill
// arm `[dev] game_build_force`, which runs the decision on a build this install is not, so the untested
// and the mismatch paths can be shown on a healthy install (the precedent is VOTVCOOP_FORCE_HEALTH_FAIL).
// Every coop consumer -- the boot line, the stand-down, the popup, the label, the report, the Join and
// the lobby -- reads the build through here. Any thread.

#pragma once

#include "ue_wrap/core/game_build.h"

#include <string>

namespace coop::game_build {

enum class Kind {
    Target,    // the build this Multivoid was made for
    Newer,     // a known build released after it
    Older,     // a known build released before it
    Untested,  // no row names it
};

struct Detected {
    ue_wrap::game_build::Identity    id;
    const ue_wrap::game_build::Row*  row = nullptr;  // nullptr when Untested
    Kind                             kind = Kind::Untested;
    bool                             forced = false;  // [dev] game_build_force chose it
};

// The running build, decided once on the first call.
const Detected& Running();

// "the target", "newer", "older", "untested".
const char* KindWord(Kind kind);

// The running build in one phrase for a log or report line: "VOTV 0.9.0n (the target)", "VOTV 0.10.0
***REMOVED***
std::string Describe();

// For an untested build, which half matched a known one, in words: "its executable is 0.9.0n's",
// "its game data is 0.9.0n's", or "no known build" -- the report's detail. Empty for a known build.
std::string UntestedDetail();

// The version popup is owed: the game is not the target, and the player has not answered for this build
// (the ini row game_build_ack holds the key last answered -- the ini lives beside one install, so one slot
// asks once per build, and an install updated in place asks again).
bool AnswerOwed();

// The player answered "Continue anyway": this build's key into game_build_ack.
void Acknowledge();

// The last Multivoid build made for a game version the mod has moved past (coop/game_build/last_builds.inc),
// or 0 when none is recorded.
unsigned LastBuildFor(const char* version);

}  // namespace coop::game_build
