// ue_wrap/core/script_bytecode.h -- private to script_fingerprint: the bytecode decoder it renders with.
// The decoder (script_bytecode.cpp) walks one UE4.27 Kismet expression of a UFunction's in-memory
// Script, in UAssetAPI's iCode layout (a pointer 8 bytes, an FScriptName 12), either collecting its code
// offsets or rendering it as canonical text (docs/bytecode-fingerprint.md). script_fingerprint.cpp
// orders the statements by flow and writes the signature.

#pragma once

#include "ue_wrap/core/reflection.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace ue_wrap::script_fingerprint::detail {

constexpr uint8_t EX_Return = 0x04, EX_Jump = 0x06, EX_EndFunctionParms = 0x16, EX_IntConst = 0x1D,
                  EX_FinalFunction = 0x1C, EX_LocalFinalFunction = 0x46, EX_PopExecutionFlow = 0x4D,
                  EX_ComputedJump = 0x4E, EX_EndOfScript = 0x53;

template <typename T>
T Rd(const void* base, size_t off) {
    T v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + off, sizeof(T));
    return v;
}

// A name's text, lowercase, with its number as the engine renders it ("foo_3" for number 4). Rendering
// costs an engine alloc and free (reflection.cpp), so each comparison index renders once per pass.
struct NameCache {
    std::unordered_map<int32_t, std::string> base;
    std::string Of(int32_t comparisonIndex, uint32_t number);
};

// One fingerprint's state: the names rendered so far (its own cache, or a census pass's shared one: a
// name's comparison index is stable for the process's life), a frame's own properties by first
// appearance, and the first problem met.
struct Ctx {
    Ctx() = default;
    Ctx(const Ctx&) = delete;  // `names` may point at `own`: a copy would read the original's cache
    Ctx& operator=(const Ctx&) = delete;
    NameCache  own;
    NameCache* names = &own;
    std::unordered_map<void*, int> locals;
    std::string problem;
    void Fail(const char* what, size_t at);
};

// One top-level statement of a script.
struct Stmt {
    uint32_t off = 0, end = 0;
    uint8_t  op = 0;
    std::vector<uint32_t> targets;  // code offsets inside, in operand order
    int32_t  uberEntry = -1;        // >= 0: an event stub's call into its ubergraph, at this entry
    void*    uberFn = nullptr;
};

// How a rendered code offset becomes a label: through `resolve` (an EX_Jump chain followed) into `map`.
struct Labels {
    const std::unordered_map<uint32_t, int>* map = nullptr;
    uint32_t (*resolve)(const void* ctx, uint32_t off) = nullptr;
    const void* ctx = nullptr;
};

// Every statement of a script, with its code offsets; false (and ctx.problem) when one does not decode.
bool Scan(Ctx& c, const uint8_t* s, size_t n, std::vector<Stmt>& out);

// Render the expression at `pos` onto `out`; the position after it, or 0 when it does not decode.
size_t RenderExpr(Ctx& c, const uint8_t* s, size_t n, size_t pos, const Labels& labels, std::string& out);

const char* OpName(uint8_t op);
bool IsUbergraph(Ctx& c, void* function);
std::string ObjPath(Ctx& c, void* obj);       // outer chain without the package, lowercase, '.'-joined
std::string FieldName(Ctx& c, const void* field);
uint64_t Fnv1a64(const std::string& s);

}  // namespace ue_wrap::script_fingerprint::detail
