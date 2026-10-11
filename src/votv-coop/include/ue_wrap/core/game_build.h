// ue_wrap/core/game_build.h -- which build of the game this process runs, read from the game's own files.
//
// Two facts name a build, both measured on the rig's installs: the executable's link stamp
// (IMAGE_FILE_HEADER::TimeDateStamp, the same in the packed and the unpacked image of one build) and
// the main pak's index hash (FPakInfo::IndexHash, plaintext even where the index is encrypted). The
// file size names nothing (one build measured three sizes, packed and two unpacked dumps), and the
// game's displayed version is no better (***REMOVED*** displays a0.9.0n). The builds the mod knows
// are rows (ue_wrap/facts/game_builds.inc), oldest first, so a row's position orders builds.
//
// This layer reads and matches. What a build means for the mod -- the target, newer, older, untested
// -- is coop/game_build's to decide. Any thread; no engine call.

#pragma once

#include <cstddef>
#include <cstdint>

namespace ue_wrap::game_build {

inline constexpr size_t kPakHashBytes = 20;
inline constexpr size_t kKeyMax = 32;  // the key's text, the terminator excluded

// The build as read. `key` is what two peers compare: "<stamp, 8 hex>-<hash, its first 16 hex>", or
// "<stamp, 8 hex>-nopak" when the pak could not be read -- text, so two players on a build no row
// names still match each other.
struct Identity {
    uint32_t    exeStamp = 0;
    uint8_t     pakHash[kPakHashBytes] = {};
    bool        pakRead = false;
    const char* pakProblem = nullptr;  // why pakRead is false (a static sentence), else nullptr
    char        key[kKeyMax + 1] = {};
};

// One build the mod knows: facts about the game only.
struct Row {
    ***REMOVED***
    const char* version;          // the game version line as a Multivoid target names it: "0.10.0"
    ***REMOVED***
    uint32_t    exeStamp;         // IMAGE_FILE_HEADER::TimeDateStamp
    const char* pakIndexHashHex;  // FPakInfo::IndexHash, 40 lowercase hex
};

// The running build, read once on the first call, from any thread.
const Identity& Running();

// The known builds, oldest first.
const Row* Rows(size_t* count);

// The row both halves of `id` match, else nullptr: one half matching is an unknown build.
const Row* Match(const Identity& id);

// The row whose executable, or whose game data, this is -- for saying which half of an unknown build
// matched. nullptr when none.
const Row* MatchExe(const Identity& id);
const Row* MatchPak(const Identity& id);

***REMOVED***
const Row* FindById(const char* id);
const Row* FindByKey(const char* key);

// The row the engine profile was derived against (profile::kTargetBuild). Always present: a profile
// naming no row, or a row whose version is not its id, fails the selftest.
const Row* Target();

// The row's position, oldest first; -1 for nullptr.
int OrderOf(const Row* row);

// The identity a row describes: what the build the row names reads as. For the coop layer's drill arm,
// which runs the decision on a build this install is not.
Identity IdentityOf(const Row& row);

// An identity no row names, for the same drill arm.
Identity UnknownIdentity();

// The pure halves, un-gated on every boot: the footer parse, the key, the matching. Logs; true when
// every check passed.
bool RunSelftest();

}  // namespace ue_wrap::game_build
