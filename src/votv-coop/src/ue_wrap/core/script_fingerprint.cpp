// ue_wrap/core/script_fingerprint.cpp -- the bytecode walker, the flow order and the canonical text. The
// spec is docs/VERSION_PORT_DRILL_ARC.md section 7; tools/port/bp_fingerprint.py is the offline twin and
// every rule here has its line there. The opcode operand table follows UAssetAPI's Kismet expression
// readers (MIT, Atenfyr; THIRD-PARTY-NOTICES.md), whose iCode accounting is the in-memory layout.

#include "ue_wrap/core/script_fingerprint.h"

#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace ue_wrap::script_fingerprint {
namespace R = ue_wrap::reflection;
namespace P = ue_wrap::profile;

namespace {

// Operand kinds, in bp_fingerprint.py's letters: e L o p n i I x b h B l u f d s w c.
enum Kind : uint8_t {
    kNone = 0, kExpr, kList, kObj, kProp, kName, kI32, kCode, kSkip, kU8, kU16, kBool, kI64, kU64, kF32, kF64,
    kStr, kWStr, kCount,
};
enum Special : uint8_t { sTable = 0, sText, sSwitch, sCast, sFloats3, sTransform };

struct Op {
    const char* name = nullptr;  // without EX_; null = not an opcode
    Kind        ops[4] = {};
    Special     special = sTable;
    uint8_t     listEnd = 0;     // the token a kList operand ends on
};

constexpr uint8_t EX_Jump = 0x06, EX_Return = 0x04, EX_IntConst = 0x1D, EX_FinalFunction = 0x1C,
                  EX_LocalFinalFunction = 0x46, EX_PopExecutionFlow = 0x4D, EX_ComputedJump = 0x4E,
                  EX_EndOfScript = 0x53;

const Op* Table() {
    static Op t[256];
    static bool built = false;
    if (built) return t;
    auto set = [](uint8_t code, const char* name, std::initializer_list<Kind> ops, Special sp = sTable,
                  uint8_t end = 0) {
        Op& o = t[code];
        o.name = name;
        int i = 0;
        for (Kind k : ops) o.ops[i++] = k;
        o.special = sp;
        o.listEnd = end;
    };
    constexpr uint8_t kParms = 0x16;
    set(0x00, "LocalVariable", {kProp});
    set(0x01, "InstanceVariable", {kProp});
    set(0x02, "DefaultVariable", {kProp});
    set(0x04, "Return", {kExpr});
    set(0x06, "Jump", {kCode});
    set(0x07, "JumpIfNot", {kCode, kExpr});
    set(0x09, "Assert", {kU16, kBool, kExpr});
    set(0x0B, "Nothing", {});
    set(0x0F, "Let", {kProp, kExpr, kExpr});
    set(0x12, "ClassContext", {kExpr, kSkip, kProp, kExpr});
    set(0x13, "MetaCast", {kObj, kExpr});
    set(0x14, "LetBool", {kExpr, kExpr});
    set(0x15, "EndParmValue", {});
    set(0x16, "EndFunctionParms", {});
    set(0x17, "Self", {});
    set(0x18, "Skip", {kSkip, kExpr});
    set(0x19, "Context", {kExpr, kSkip, kProp, kExpr});
    set(0x1A, "Context_FailSilent", {kExpr, kSkip, kProp, kExpr});
    set(0x1B, "VirtualFunction", {kName, kList}, sTable, kParms);
    set(0x1C, "FinalFunction", {kObj, kList}, sTable, kParms);
    set(0x1D, "IntConst", {kI32});
    set(0x1E, "FloatConst", {kF32});
    set(0x1F, "StringConst", {kStr});
    set(0x20, "ObjectConst", {kObj});
    set(0x21, "NameConst", {kName});
    set(0x22, "RotationConst", {}, sFloats3);
    set(0x23, "VectorConst", {}, sFloats3);
    set(0x24, "ByteConst", {kU8});
    set(0x25, "IntZero", {});
    set(0x26, "IntOne", {});
    set(0x27, "True", {});
    set(0x28, "False", {});
    set(0x29, "TextConst", {}, sText);
    set(0x2A, "NoObject", {});
    set(0x2B, "TransformConst", {}, sTransform);
    set(0x2C, "IntConstByte", {kU8});
    set(0x2D, "NoInterface", {});
    set(0x2E, "DynamicCast", {kObj, kExpr});
    set(0x2F, "StructConst", {kObj, kI32, kList}, sTable, 0x30);
    set(0x30, "EndStructConst", {});
    set(0x31, "SetArray", {kExpr, kList}, sTable, 0x32);
    set(0x32, "EndArray", {});
    set(0x33, "PropertyConst", {kProp});
    set(0x34, "UnicodeStringConst", {kWStr});
    set(0x35, "Int64Const", {kI64});
    set(0x36, "UInt64Const", {kU64});
    set(0x37, "DoubleConst", {kF64});
    set(0x38, "PrimitiveCast", {}, sCast);
    set(0x39, "SetSet", {kExpr, kCount, kList}, sTable, 0x3A);
    set(0x3A, "EndSet", {});
    set(0x3B, "SetMap", {kExpr, kCount, kList}, sTable, 0x3C);
    set(0x3C, "EndMap", {});
    set(0x3D, "SetConst", {kProp, kCount, kList}, sTable, 0x3E);
    set(0x3E, "EndSetConst", {});
    set(0x3F, "MapConst", {kProp, kProp, kCount, kList}, sTable, 0x40);
    set(0x40, "EndMapConst", {});
    set(0x42, "StructMemberContext", {kProp, kExpr});
    set(0x43, "LetMulticastDelegate", {kExpr, kExpr});
    set(0x44, "LetDelegate", {kExpr, kExpr});
    set(0x45, "LocalVirtualFunction", {kName, kList}, sTable, kParms);
    set(0x46, "LocalFinalFunction", {kObj, kList}, sTable, kParms);
    set(0x48, "LocalOutVariable", {kProp});
    set(0x4A, "DeprecatedOp4A", {});
    set(0x4B, "InstanceDelegate", {kName});
    set(0x4C, "PushExecutionFlow", {kCode});
    set(0x4D, "PopExecutionFlow", {});
    set(0x4E, "ComputedJump", {kExpr});
    set(0x4F, "PopExecutionFlowIfNot", {kExpr});
    set(0x50, "Breakpoint", {});
    set(0x51, "InterfaceContext", {kExpr});
    set(0x52, "ObjToInterfaceCast", {kObj, kExpr});
    set(0x53, "EndOfScript", {});
    set(0x54, "CrossInterfaceCast", {kObj, kExpr});
    set(0x55, "InterfaceToObjCast", {kObj, kExpr});
    set(0x5A, "WireTracepoint", {});
    set(0x5B, "SkipOffsetConst", {kCode});
    set(0x5C, "AddMulticastDelegate", {kExpr, kExpr});
    set(0x5D, "ClearMulticastDelegate", {kExpr});
    set(0x5E, "Tracepoint", {});
    set(0x5F, "LetObj", {kExpr, kExpr});
    set(0x60, "LetWeakObjPtr", {kExpr, kExpr});
    set(0x61, "BindDelegate", {kName, kExpr, kExpr});
    set(0x62, "RemoveMulticastDelegate", {kExpr, kExpr});
    set(0x63, "CallMulticastDelegate", {kObj, kExpr, kList}, sTable, kParms);
    set(0x64, "LetValueOnPersistentFrame", {kProp, kExpr});
    set(0x65, "ArrayConst", {kProp, kCount, kList}, sTable, 0x66);
    set(0x66, "EndArrayConst", {});
    set(0x67, "SoftObjectConst", {kExpr});
    set(0x68, "CallMath", {kObj, kList}, sTable, kParms);
    set(0x69, "SwitchValue", {}, sSwitch);
    set(0x6B, "ArrayGetByRef", {kExpr, kExpr});
    set(0x6C, "ClassSparseDataVariable", {kProp});
    set(0x6D, "FieldPathConst", {kExpr});
    built = true;
    return t;
}

// ---- text helpers (the spec's folding and lowercasing) ----

void AppendFolded(std::string& out, uint32_t unit, bool lower) {
    if (unit == '"' || unit == '\\') {
        out += '\\';
        out += static_cast<char>(unit);
    } else if (unit >= 0x20 && unit <= 0x7E) {
        char c = static_cast<char>(unit);
        if (lower && c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        out += c;
    } else {
        out += '?';
    }
}

std::string FoldLower(const std::wstring& s) {
    std::string out;
    out.reserve(s.size());
    for (wchar_t ch : s) AppendFolded(out, static_cast<uint16_t>(ch), true);
    return out;
}

// A name's text, lowercase, with its number as the engine renders it ("foo_3" for number 4). Rendering
// costs an engine alloc and free (reflection.cpp), so each comparison index renders once per pass.
struct NameCache {
    std::unordered_map<int32_t, std::string> base;
    std::string Of(int32_t comparisonIndex, uint32_t number) {
        auto it = base.find(comparisonIndex);
        if (it == base.end()) {
            R::FName n{comparisonIndex, 0};
            it = base.emplace(comparisonIndex, FoldLower(R::ToString(n))).first;
        }
        if (number == 0) return it->second;
        return it->second + "_" + std::to_string(number - 1);
    }
};

std::string StripNumber(const std::string& name) {
    std::string out;
    size_t start = 0;
    bool first = true;
    while (start <= name.size()) {
        size_t end = name.find('_', start);
        if (end == std::string::npos) end = name.size();
        std::string part = name.substr(start, end - start);
        bool digits = !part.empty() && std::all_of(part.begin(), part.end(), [](char c) { return c >= '0' && c <= '9'; });
        if (first || !digits) {
            if (!first) out += '_';
            out += part;
        }
        first = false;
        start = end + 1;
    }
    return out;
}

uint64_t Fnv1a64(const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return h;
}

template <typename T>
T Rd(const void* base, size_t off) {
    T v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + off, sizeof(T));
    return v;
}

// ---- the context of one fingerprint ----

struct Stmt {
    uint32_t off = 0, end = 0;
    uint8_t  op = 0;
    std::vector<uint32_t> targets;  // code offsets inside, in operand order
    int32_t  uberEntry = -1;        // >= 0: a call into the ubergraph at this entry
    void*    uberFn = nullptr;
};

struct Ctx {
    NameCache names;
    std::unordered_map<void*, int> locals;  // a frame's own property -> its first-appearance index
    std::string problem;
    void Fail(const char* what, size_t at) {
        if (!problem.empty()) return;
        char b[96];
        std::snprintf(b, sizeof b, "%s at %zu", what, at);
        problem = b;
    }
};

std::string ObjPath(Ctx& c, void* obj);

bool IsFunctionClass(Ctx& c, void* obj) {
    void* cls = R::ClassOf(obj);
    if (!cls) return false;
    const R::FName& n = R::NameOf(cls);
    const std::string s = c.names.Of(n.ComparisonIndex, static_cast<uint32_t>(n.Number));
    return s == "function" || s == "delegatefunction" || s == "sparsedelegatefunction";
}

std::string ObjPath(Ctx& c, void* obj) {
    std::vector<std::string> parts;
    for (int guard = 0; obj && R::OuterOf(obj) && guard < 64; ++guard) {
        const R::FName& n = R::NameOf(obj);
        parts.push_back(c.names.Of(n.ComparisonIndex, static_cast<uint32_t>(n.Number)));
        obj = R::OuterOf(obj);
    }
    std::string out;
    for (size_t i = parts.size(); i-- > 0;) {
        out += parts[i];
        if (i) out += '.';
    }
    return out;
}

std::string FieldName(Ctx& c, const void* field) {
    const R::FName n = Rd<R::FName>(field, P::off::FField_NamePrivate);
    return c.names.Of(n.ComparisonIndex, static_cast<uint32_t>(n.Number));
}

std::string PropText(Ctx& c, void* field) {
    if (!field) return "p:-";
    std::vector<std::string> chain;
    void* f = field;
    void* ownerObj = nullptr;
    for (int guard = 0; f && guard < 16; ++guard) {
        chain.push_back(FieldName(c, f));
        void* owner = Rd<void*>(f, P::off::FField_Owner);
        if (Rd<uint8_t>(f, P::off::FField_OwnerIsUObject)) {
            ownerObj = owner;
            break;
        }
        f = owner;
    }
    std::string path = "p:" + (ownerObj ? ObjPath(c, ownerObj) : std::string()) + ":";
    for (size_t i = chain.size(); i-- > 0;) {
        path += chain[i];
        if (i) path += '.';
    }
    if (ownerObj && IsFunctionClass(c, ownerObj)) {
        const int k = c.locals.emplace(field, static_cast<int>(c.locals.size())).first->second;
        return "l" + std::to_string(k) + ":" + StripNumber(chain.front());
    }
    return path;
}

// ---- the walker: one expression, scanned (targets only) or rendered ----

struct Walker {
    Ctx&           c;
    const uint8_t* s;
    size_t         n;
    size_t         pos = 0;
    bool           render = false;
    std::string*   out = nullptr;
    std::vector<uint32_t>* targets = nullptr;
    // label of a code offset (render mode); null = scan mode
    const std::unordered_map<uint32_t, int>* labels = nullptr;
    uint32_t (*resolve)(const void*, uint32_t) = nullptr;
    const void* resolveCtx = nullptr;

    bool Need(size_t k) {
        if (pos + k > n) {
            c.Fail("script ends inside an expression", pos);
            return false;
        }
        return true;
    }
    template <typename T>
    bool Take(T& v) {
        if (!Need(sizeof(T))) return false;
        std::memcpy(&v, s + pos, sizeof(T));
        pos += sizeof(T);
        return true;
    }
    void Emit(const std::string& t) {
        if (render) *out += t;
    }
    void Emit(const char* t) {
        if (render) *out += t;
    }
    void Hex32(const char* pre, uint32_t v) {
        if (!render) return;
        char b[24];
        std::snprintf(b, sizeof b, "%s%08x", pre, v);
        *out += b;
    }

    bool Operand(Kind k, uint8_t listEnd, bool& wrote) {
        auto sep = [&]() {
            if (wrote) Emit(",");
            wrote = true;
        };
        switch (k) {
        case kExpr: sep(); return Expr();
        case kList: {
            sep();
            Emit("[");
            for (bool firstItem = true;;) {
                if (!Need(1)) return false;
                if (s[pos] == listEnd) {
                    ++pos;
                    break;
                }
                if (!firstItem) Emit(",");
                firstItem = false;
                if (!Expr()) return false;
            }
            Emit("]");
            return true;
        }
        case kObj: {
            void* o = nullptr;
            if (!Take(o)) return false;
            sep();
            if (render) Emit(o ? "o:" + ObjPath(c, o) : std::string("o:-"));
            return true;
        }
        case kProp: {
            void* f = nullptr;
            if (!Take(f)) return false;
            sep();
            if (render) Emit(PropText(c, f));
            return true;
        }
        case kName: {
            int32_t ci = 0, di = 0;
            uint32_t num = 0;
            if (!Take(ci) || !Take(di) || !Take(num)) return false;
            sep();
            if (render) Emit("n:" + c.names.Of(ci, num));
            return true;
        }
        case kI32: { int32_t v; if (!Take(v)) return false; sep(); if (render) Emit("i:" + std::to_string(v)); return true; }
        case kU8: { uint8_t v; if (!Take(v)) return false; sep(); if (render) Emit("b:" + std::to_string(v)); return true; }
        case kU16: { uint16_t v; if (!Take(v)) return false; sep(); if (render) Emit("h:" + std::to_string(v)); return true; }
        case kBool: { uint8_t v; if (!Take(v)) return false; sep(); Emit(v ? "B:1" : "B:0"); return true; }
        case kI64: { int64_t v; if (!Take(v)) return false; sep(); if (render) Emit("l:" + std::to_string(v)); return true; }
        case kU64: { uint64_t v; if (!Take(v)) return false; sep(); if (render) Emit("u:" + std::to_string(v)); return true; }
        case kF32: { uint32_t v; if (!Take(v)) return false; sep(); Hex32("f:", v); return true; }
        case kF64: {
            uint64_t v;
            if (!Take(v)) return false;
            sep();
            if (render) {
                char b[32];
                std::snprintf(b, sizeof b, "d:%016llx", static_cast<unsigned long long>(v));
                Emit(b);
            }
            return true;
        }
        case kCode: {
            uint32_t v;
            if (!Take(v)) return false;
            sep();
            if (targets) targets->push_back(v);
            if (render) {
                const uint32_t r = resolve ? resolve(resolveCtx, v) : v;
                const auto it = labels ? labels->find(r) : decltype(labels->find(r)){};
                Emit(labels && it != labels->end() ? "@" + std::to_string(it->second) : std::string("@?"));
            }
            return true;
        }
        case kSkip:
        case kCount: { uint32_t v; return Take(v); }  // derived from the layout: dropped
        case kStr: {
            sep();
            Emit("s:\"");
            for (;;) {
                uint8_t ch;
                if (!Take(ch)) return false;
                if (!ch) break;
                if (render) AppendFolded(*out, ch, false);
            }
            Emit("\"");
            return true;
        }
        case kWStr: {
            sep();
            Emit("s:\"");
            for (;;) {
                uint16_t ch;
                if (!Take(ch)) return false;
                if (!ch) break;
                if (render) AppendFolded(*out, ch, false);
            }
            Emit("\"");
            return true;
        }
        default: return true;
        }
    }

    bool Expr() {
        if (!Need(1)) return false;
        const size_t at = pos;
        const uint8_t code = s[pos++];
        const Op& op = Table()[code];
        if (!op.name) {
            c.Fail("unknown opcode", at);
            return false;
        }
        Emit(op.name);
        switch (op.special) {
        case sFloats3:
        case sTransform: {
            const int count = op.special == sTransform ? 10 : 3;
            Emit("(");
            for (int i = 0; i < count; ++i) {
                uint32_t v;
                if (!Take(v)) return false;
                if (i) Emit(",");
                Hex32("f:", v);
            }
            Emit(")");
            return true;
        }
        case sText: {
            uint8_t kind;
            if (!Take(kind)) return false;
            static const char* kKinds[] = {"Empty", "LocalizedText", "InvariantText", "LiteralString", "StringTableEntry"};
            if (kind > 4) {
                c.Fail("unknown text literal type", pos - 1);
                return false;
            }
            Emit("(t:");
            Emit(kKinds[kind]);
            const int subs = kind == 1 ? 3 : (kind == 2 || kind == 3) ? 1 : kind == 4 ? 2 : 0;
            if (kind == 4) {
                void* o = nullptr;
                if (!Take(o)) return false;
                if (render) Emit(o ? ",o:" + ObjPath(c, o) : std::string(",o:-"));
            }
            for (int i = 0; i < subs; ++i) {
                Emit(",");
                if (!Expr()) return false;
            }
            Emit(")");
            return true;
        }
        case sSwitch: {
            uint16_t cases;
            uint32_t endGoto;
            if (!Take(cases) || !Take(endGoto)) return false;
            Emit("(");
            if (!Expr()) return false;
            Emit(",[");
            for (uint16_t i = 0; i < cases; ++i) {
                if (i) Emit(",");
                Emit("(");
                if (!Expr()) return false;
                uint32_t next;
                if (!Take(next)) return false;
                Emit(",");
                if (!Expr()) return false;
                Emit(")");
            }
            Emit("],");
            if (!Expr()) return false;
            Emit(")");
            return true;
        }
        case sCast: {
            uint8_t ct;
            if (!Take(ct)) return false;
            const char* name = ct == 0x46 ? "ObjectToInterface" : ct == 0x47 ? "ObjectToBool" : ct == 0x49 ? "InterfaceToBool" : nullptr;
            Emit("(k:");
            if (name) {
                Emit(name);
            } else if (render) {
                Emit("?" + std::to_string(ct));
            }
            if (ct == 0x46) {
                void* o = nullptr;
                if (!Take(o)) return false;
                if (render) Emit(o ? ",o:" + ObjPath(c, o) : std::string(",o:-"));
            }
            Emit(",");
            if (!Expr()) return false;
            Emit(")");
            return true;
        }
        default: break;
        }
        if (op.ops[0] == kNone) return true;  // no operand: the name alone
        Emit("(");
        bool wrote = false;
        for (Kind k : op.ops) {
            if (k == kNone) break;
            if (!Operand(k, op.listEnd, wrote)) return false;
        }
        Emit(")");
        return true;
    }
};

// ---- statements, flow order, rendering ----

bool IsUbergraph(Ctx& c, void* fn) {
    if (!fn) return false;
    const R::FName& n = R::NameOf(fn);
    return c.names.Of(n.ComparisonIndex, 0).rfind("executeubergraph_", 0) == 0;
}

bool Scan(Ctx& c, const uint8_t* s, size_t n, std::vector<Stmt>& out) {
    size_t pos = 0;
    while (pos < n) {
        Stmt st;
        st.off = static_cast<uint32_t>(pos);
        st.op = s[pos];
        Walker w{c, s, n};
        w.pos = pos;
        w.targets = &st.targets;
        if (!w.Expr()) return false;
        st.end = static_cast<uint32_t>(w.pos);
        // an event stub's call into its ubergraph: (Local)FinalFunction(ExecuteUbergraph_*, IntConst(entry), ...)
        if ((st.op == EX_LocalFinalFunction || st.op == EX_FinalFunction) && st.end - st.off >= 14) {
            void* fn = Rd<void*>(s, pos + 1);
            if (s[pos + 9] == EX_IntConst && IsUbergraph(c, fn)) {
                st.uberFn = fn;
                st.uberEntry = Rd<int32_t>(s, pos + 10);
            }
        }
        pos = w.pos;
        out.push_back(std::move(st));
    }
    return true;
}

struct Block {
    const std::vector<Stmt>* stmts = nullptr;
    std::unordered_map<uint32_t, int> index;  // offset -> statement index
    uint32_t Resolve(uint32_t off) const {
        for (size_t guard = 0; guard <= stmts->size(); ++guard) {
            auto it = index.find(off);
            if (it == index.end() || (*stmts)[it->second].op != EX_Jump) return off;
            off = (*stmts)[it->second].targets.empty() ? off : (*stmts)[it->second].targets[0];
        }
        return off;
    }
};

uint32_t ResolveThunk(const void* blk, uint32_t off) { return static_cast<const Block*>(blk)->Resolve(off); }

bool IsTerminator(uint8_t op) {
    return op == EX_Jump || op == EX_Return || op == EX_EndOfScript || op == EX_PopExecutionFlow || op == EX_ComputedJump;
}

// bp_fingerprint.py flow_order: follow a straight run (an EX_Jump is layout glue), defer every other
// code target LIFO in operand order, mark where a run merges into a labelled statement.
void RenderFlow(Ctx& c, const uint8_t* s, size_t n, const std::vector<Stmt>& stmts, uint32_t entry,
                std::string& text) {
    Block b;
    b.stmts = &stmts;
    for (size_t i = 0; i < stmts.size(); ++i) b.index[stmts[i].off] = static_cast<int>(i);
    std::vector<uint32_t> order;
    std::unordered_map<uint32_t, int> label;
    std::unordered_map<uint32_t, uint32_t> merges;
    std::vector<uint32_t> pending{b.Resolve(entry)};
    while (!pending.empty()) {
        uint32_t off = pending.back();
        pending.pop_back();
        if (label.count(off)) continue;
        bool havePrev = false;
        uint32_t prev = 0;
        for (;;) {
            auto it = b.index.find(off);
            if (it == b.index.end()) {
                c.Fail("code offset is not a statement", off);
                break;
            }
            if (label.count(off)) {
                if (havePrev) merges[prev] = off;
                break;
            }
            label[off] = static_cast<int>(order.size());
            order.push_back(off);
            const Stmt& st = stmts[it->second];
            for (size_t k = st.targets.size(); k-- > 0;) pending.push_back(b.Resolve(st.targets[k]));
            if (IsTerminator(st.op)) break;
            const size_t i = static_cast<size_t>(it->second);
            if (i + 1 >= stmts.size()) break;
            havePrev = true;
            prev = off;
            off = b.Resolve(stmts[i + 1].off);
        }
    }
    for (uint32_t off : order) {
        const Stmt& st = stmts[b.index[off]];
        text += std::to_string(label[off]) + " ";
        Walker w{c, s, n};
        w.pos = off;
        w.render = true;
        w.out = &text;
        w.labels = &label;
        w.resolve = &ResolveThunk;
        w.resolveCtx = &b;
        if (st.uberEntry >= 0) {
            // the stub's call: its entry offset is layout, the region appended after is the logic
            text += Table()[st.op].name;
            text += "(o:" + ObjPath(c, st.uberFn) + ",[IntConst(@U)";
            w.pos = off + 1 + 8 + 5;  // past the opcode, the function pointer and IntConst(entry)
            for (;;) {
                if (w.pos >= n) break;
                if (s[w.pos] == 0x16) break;
                text += ",";
                if (!w.Expr()) break;
            }
            text += "])";
        } else {
            w.Expr();
        }
        auto m = merges.find(off);
        if (m != merges.end()) text += " >@" + std::to_string(label[m->second]);
        text += "\n";
    }
}

// ---- the signature line ----

std::string TypeText(Ctx& c, void* prop, int depth) {
    void* fc = Rd<void*>(prop, P::off::FField_ClassPrivate);
    std::string type;
    if (fc) {
        const R::FName n = Rd<R::FName>(fc, P::off::FFieldClass_Name);
        type = c.names.Of(n.ComparisonIndex, static_cast<uint32_t>(n.Number));
    }
    if (depth > 4) return type;
    auto obj = [&](size_t off) {
        void* o = Rd<void*>(prop, off);
        if (o) type += ":" + ObjPath(c, o);
    };
    auto inner = [&](size_t off) { void* p = Rd<void*>(prop, off); return p ? TypeText(c, p, depth + 1) : std::string("-"); };
    if (type == "objectproperty" || type == "softobjectproperty" || type == "weakobjectproperty" ||
        type == "lazyobjectproperty" || type == "classproperty" || type == "softclassproperty" ||
        type == "interfaceproperty" || type == "structproperty" || type == "byteproperty" ||
        type == "delegateproperty" || type == "multicastdelegateproperty" ||
        type == "multicastinlinedelegateproperty" || type == "multicastsparsedelegateproperty") {
        obj(P::off::FProperty_TypeObject);
    } else if (type == "enumproperty") {
        obj(P::off::FProperty_TypeObject + 8);
    } else if (type == "arrayproperty" || type == "setproperty") {
        type += "<" + inner(P::off::FProperty_TypeObject) + ">";
    } else if (type == "mapproperty") {
        type += "<" + inner(P::off::FProperty_TypeObject) + "," + inner(P::off::FProperty_TypeObject + 8) + ">";
    }
    return type;
}

std::string SigLine(Ctx& c, void* fn) {
    std::string out = "sig(";
    bool first = true;
    void* f = Rd<void*>(fn, P::off::UStruct_ChildProperties);
    for (int guard = 0; f && guard < 512; ++guard, f = Rd<void*>(f, P::off::FField_Next)) {
        const uint64_t flags = Rd<uint64_t>(f, P::off::FProperty_PropertyFlags);
        if (!(flags & P::cpf::Parm)) continue;
        if (!first) out += ",";
        first = false;
        // no size: a native struct's cooked size is not its run-time one (FHitResult 144 in the asset, 136
        // in the game, measured by the agreement test), and the type already names the struct
        out += FieldName(c, f) + ":";
        if (flags & P::cpf::ConstParm) out += 'C';
        if (flags & P::cpf::Parm) out += 'P';
        if (flags & P::cpf::OutParm) out += 'O';
        if (flags & P::cpf::ReturnParm) out += 'R';
        if (flags & P::cpf::ReferenceParm) out += 'F';
        out += ":" + TypeText(c, f, 0);
    }
    return out + ")\n";
}

// ---- one function ----

void Build(void* fn, Result& r, Ctx& c) {
    const uint32_t fflags = Rd<uint32_t>(fn, P::off::UFunction_FunctionFlags);
    r.text = SigLine(c, fn);
    if (fflags & P::off::FUNC_Native) {
        r.native = true;
        return;
    }
    const uint8_t* s = Rd<const uint8_t*>(fn, P::off::UStruct_Script);
    const int32_t n = Rd<int32_t>(fn, P::off::UStruct_ScriptNum);
    if (!s || n <= 0) return;  // a delegate signature or an empty function: its signature is all there is
    std::vector<Stmt> stmts;
    if (!Scan(c, s, static_cast<size_t>(n), stmts)) return;
    if (IsUbergraph(c, fn)) {
        // the ubergraph itself: its dispatch is a computed jump, so its logic is the region of every event
        // stub of the class that enters it, stub by stub in name order (a fresh frame map per entry)
        std::vector<std::pair<std::string, int32_t>> entries;
        void* cls = R::OuterOf(fn);
        void* f = cls ? Rd<void*>(cls, P::off::UStruct_Children) : nullptr;
        for (int guard = 0; f && guard < 4096; ++guard, f = Rd<void*>(f, P::off::UField_Next)) {
            const uint8_t* ss = Rd<const uint8_t*>(f, P::off::UStruct_Script);
            const int32_t sn = Rd<int32_t>(f, P::off::UStruct_ScriptNum);
            if (!ss || sn <= 0 || f == fn) continue;
            std::vector<Stmt> st;
            Ctx probe;  // a stub that does not decode is not an entry, and not this function's problem
            if (!Scan(probe, ss, static_cast<size_t>(sn), st)) continue;
            for (const Stmt& x : st)
                if (x.uberFn == fn) {
                    const R::FName& nm = R::NameOf(f);
                    entries.emplace_back(c.names.Of(nm.ComparisonIndex, static_cast<uint32_t>(nm.Number)), x.uberEntry);
                }
        }
        std::sort(entries.begin(), entries.end());
        for (const auto& e : entries) {
            r.text += "== entry " + e.first + " ==\n";
            c.locals.clear();
            RenderFlow(c, s, static_cast<size_t>(n), stmts, static_cast<uint32_t>(e.second), r.text);
        }
        return;
    }
    RenderFlow(c, s, static_cast<size_t>(n), stmts, 0, r.text);
    for (const Stmt& st : stmts) {
        if (st.uberEntry < 0) continue;
        const uint8_t* us = Rd<const uint8_t*>(st.uberFn, P::off::UStruct_Script);
        const int32_t un = Rd<int32_t>(st.uberFn, P::off::UStruct_ScriptNum);
        std::vector<Stmt> ust;
        if (!us || un <= 0 || !Scan(c, us, static_cast<size_t>(un), ust)) {
            c.Fail("ubergraph does not decode", 0);
            return;
        }
        r.text += "== " + ObjPath(c, st.uberFn) + " ==\n";
        RenderFlow(c, us, static_cast<size_t>(un), ust, static_cast<uint32_t>(st.uberEntry), r.text);
    }
}

int BuildSEH(void* fn, Result* r, Ctx* c) {
    __try {
        Build(fn, *r, *c);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

}  // namespace

std::string PathOf(void* uobject) {
    Ctx c;
    return uobject ? ObjPath(c, uobject) : std::string();
}

Result Of(void* ufunction) {
    Result r;
    if (!ufunction) {
        r.problem = "null function";
        return r;
    }
    Ctx c;
    if (BuildSEH(ufunction, &r, &c)) {
        r.problem = "fault while reading the function";
        r.text.clear();
        return r;
    }
    r.problem = c.problem;
    r.ok = c.problem.empty();
    r.hash = Fnv1a64(r.text);
    const size_t sigEnd = r.text.find('\n');
    r.sigHash = Fnv1a64(r.text.substr(0, sigEnd == std::string::npos ? r.text.size() : sigEnd + 1));
    r.bodyHash = Fnv1a64(sigEnd == std::string::npos ? std::string() : r.text.substr(sigEnd + 1));
    return r;
}

}  // namespace ue_wrap::script_fingerprint
