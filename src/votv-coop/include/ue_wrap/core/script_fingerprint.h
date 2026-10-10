// ue_wrap/core/script_fingerprint.h -- a game function as canonical, name-resolved text and two 64-bit
// fingerprints, so a port can ask "is this the function the mod was built against?" The canonical form is
// specified in docs/bytecode-fingerprint.md.
//
// A Blueprint function is read from its in-memory bytecode (UStruct::Script) by a decoder of every UE4.27
// opcode (script_bytecode.h). Names, objects and properties are written as lowercase text, never as
// indices or addresses; statements in flow order, code offsets as labels; an event stub is followed into
// its ubergraph; a frame's own slots are renamed by first appearance. The same text can be rendered from
// the cooked asset, and the two renderings are compared function by function. A native function has no
// script: its text is the signature line. Engine-wrapper layer (principle 7): no gameplay or network logic.

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

// Fingerprint one UFunction. Game thread: names render through FName::ToString. A census reads a few
// hundred functions once (about 45 us each, measured over every loaded Blueprint function). A fault while
// reading is absorbed and reported in `problem`.
Result Of(void* ufunction);

// The signature line alone (its text and sigHash), without walking the script: what a name watch on every
// class's function of one name depends on. Game thread; a fault reads as a zero hash.
Result SignatureOf(void* ufunction);

// A census pass: between the two, an ubergraph is decoded once however many event stubs follow it (a
// class's stubs all enter its one ubergraph). Game thread; Begin again starts a fresh pass.
void BeginPass();
void EndPass();

// The function's path as the canonical text writes objects: its outer chain without the package,
// lowercase, '.'-joined ("serverbox_c.fix", "actor.k2_destroyactor").
std::string PathOf(void* uobject);

}  // namespace ue_wrap::script_fingerprint
