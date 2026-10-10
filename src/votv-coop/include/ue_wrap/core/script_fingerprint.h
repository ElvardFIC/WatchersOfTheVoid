// ue_wrap/core/script_fingerprint.h -- a game function as canonical, name-resolved text and its 64-bit
// fingerprint, so a port can ask "is this the function the mod was built against?" (the version port
// arc, docs/VERSION_PORT_DRILL_ARC.md system 2; the canonical form is specified in its section 7).
//
// A Blueprint function is read from its in-memory bytecode (UStruct::Script) by a walker over every
// UE4.27 opcode, in the layout UAssetAPI (MIT) documents for the iCode stream: a pointer is 8 bytes,
// an FScriptName 12. Names, objects and properties are rendered as lowercase text, never as indices
// or addresses; statements are written in flow order with code offsets as labels; an event stub is
// followed into its ubergraph; a frame's own slots are renamed by first appearance. The offline tool
// tools/port/bp_fingerprint.py renders the same text from the cooked asset, and the two are held
// equal by a measured agreement test. A native function has no script: its text is the signature line.
// Engine-wrapper layer (principle 7): no gameplay or network logic.

#pragma once

#include <cstdint>
#include <string>

namespace ue_wrap::script_fingerprint {

struct Result {
    bool        ok = false;      // the whole script decoded and every code offset resolved
    bool        native = false;  // a native function: the text is its signature line only
    uint64_t    hash = 0;        // FNV-1a 64 over `text`
    uint64_t    sigHash = 0;     // over its first line, the signature: a change means wrong parameters
    uint64_t    bodyHash = 0;    // over the rest, the statements: a change means the logic drifted
    std::string text;            // the canonical text, one statement per line
    std::string problem;         // the first problem met, empty when ok
};

// Fingerprint one UFunction. Game thread: names render through FName::ToString. Not hot -- a census
// pass at boot reads a few hundred functions once. A fault while reading is absorbed and reported in
// `problem`.
Result Of(void* ufunction);

// The function's path as the canonical text writes objects: its outer chain without the package,
// lowercase, '.'-joined ("serverbox_c.fix", "actor.k2_destroyactor").
std::string PathOf(void* uobject);

}  // namespace ue_wrap::script_fingerprint
