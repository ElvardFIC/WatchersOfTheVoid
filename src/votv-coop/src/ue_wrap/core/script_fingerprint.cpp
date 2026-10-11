// ue_wrap/core/script_fingerprint.cpp -- the flow order, the signature line and the hashes over the
// decoder of script_bytecode.h; every rule is docs/bytecode-fingerprint.md's.

#include "ue_wrap/core/script_fingerprint.h"

#include "script_bytecode.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/sdk_profile.h"

#include <windows.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace ue_wrap::script_fingerprint {
namespace R = ue_wrap::reflection;
namespace P = ue_wrap::profile;
using namespace detail;

namespace {

// A script's statements and the index from a statement's offset to its position, built once per script.
struct Decoded {
    std::vector<Stmt>                  stmts;
    std::unordered_map<uint32_t, int> index;
};

bool Decode(Ctx& c, const uint8_t* s, size_t n, Decoded& out) {
    if (!Scan(c, s, n, out.stmts)) return false;
    out.index.reserve(out.stmts.size());
    for (size_t i = 0; i < out.stmts.size(); ++i) out.index[out.stmts[i].off] = static_cast<int>(i);
    return true;
}

struct Block {
    const Decoded* d = nullptr;
    uint32_t Resolve(uint32_t off) const {
        for (size_t guard = 0; guard <= d->stmts.size(); ++guard) {
            auto it = d->index.find(off);
            if (it == d->index.end() || d->stmts[it->second].op != EX_Jump) return off;
            off = d->stmts[it->second].targets.empty() ? off : d->stmts[it->second].targets[0];
        }
        return off;
    }
};

uint32_t ResolveThunk(const void* blk, uint32_t off) { return static_cast<const Block*>(blk)->Resolve(off); }

bool IsTerminator(uint8_t op) {
    return op == EX_Jump || op == EX_Return || op == EX_EndOfScript || op == EX_PopExecutionFlow || op == EX_ComputedJump;
}

// Flow order: follow a straight run (an EX_Jump is layout glue), defer every other code target LIFO in
// operand order, mark where a run merges into a labelled statement.
void RenderFlow(Ctx& c, const uint8_t* s, size_t n, const Decoded& d, uint32_t entry, std::string& text) {
    const std::vector<Stmt>& stmts = d.stmts;
    Block b;
    b.d = &d;
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
            auto it = d.index.find(off);
            if (it == d.index.end()) {
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
        const Stmt& st = stmts[d.index.at(off)];
        text += std::to_string(label[off]) + " ";
        const Labels labels{&label, &ResolveThunk, &b};
        if (st.uberEntry >= 0) {
            // the stub's call: its entry offset is layout, the region appended after is the logic
            text += OpName(st.op);
            text += "(o:" + ObjPath(c, st.uberFn) + ",[IntConst(@U)";
            size_t pos = off + 1 + 8 + 5;  // past the opcode, the function pointer and IntConst(entry)
            while (pos && pos < n && s[pos] != EX_EndFunctionParms) {
                text += ",";
                pos = RenderExpr(c, s, n, pos, labels, text);
            }
            text += "])";
        } else {
            RenderExpr(c, s, n, off, labels, text);
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
        type = c.names->Of(n.ComparisonIndex, static_cast<uint32_t>(n.Number));
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

// ---- the pass cache ----

// A script's decoded statements, kept for a census pass: every event stub of a class follows the same
// ubergraph, so mainPlayer_C's is decoded once instead of once per stub, and the stubs an ubergraph row
// reads to find its entries are not decoded again when their own rows come. Keyed on the script's address
// and size; the census begins a new pass when a class unloads, so a key cannot outlive its script.
// Game thread.
struct PassCache {
    std::unordered_map<const uint8_t*, std::pair<size_t, Decoded>> scans;
    NameCache names;  // every fingerprint of the pass renders a name once
    std::unordered_map<void*, Result> results;  // two lanes' rows on one function read it once
};
PassCache* g_pass = nullptr;

const Decoded* DecodeCached(Ctx& c, const uint8_t* s, size_t n, Decoded& local) {
    if (g_pass) {
        auto it = g_pass->scans.find(s);
        if (it != g_pass->scans.end() && it->second.first == n) return &it->second.second;
    }
    if (!Decode(c, s, n, local)) return nullptr;
    if (!g_pass) return &local;
    auto& e = g_pass->scans[s];
    e.first = n;
    e.second = std::move(local);
    return &e.second;
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
    Decoded own;
    const bool uber = IsUbergraph(c, fn);
    const Decoded* decoded = DecodeCached(c, s, static_cast<size_t>(n), own);
    if (!decoded) return;
    const std::vector<Stmt>& stmts = decoded->stmts;
    if (uber) {
        // the ubergraph itself: its dispatch is a computed jump, so its logic is the region of every event
        // stub of the class that enters it, stub by stub in name order (a fresh frame map per entry)
        std::vector<std::pair<std::string, int32_t>> entries;
        void* cls = R::OuterOf(fn);
        void* f = cls ? Rd<void*>(cls, P::off::UStruct_Children) : nullptr;
        for (int guard = 0; f && guard < 4096; ++guard, f = Rd<void*>(f, P::off::UField_Next)) {
            const uint8_t* ss = Rd<const uint8_t*>(f, P::off::UStruct_Script);
            const int32_t sn = Rd<int32_t>(f, P::off::UStruct_ScriptNum);
            if (!ss || sn <= 0 || f == fn) continue;
            // A function of the class that does not decode may be a stub, and a stub left out is a region
            // left out: the ubergraph's text would be short and still read as whole.
            Ctx probe;  // its own frame map and problem; the pass's names
            probe.names = c.names;
            Decoded probeOwn;
            const Decoded* pd = DecodeCached(probe, ss, static_cast<size_t>(sn), probeOwn);
            if (!pd) {
                c.Fail("a function of the ubergraph's class does not decode", 0);
                return;
            }
            for (const Stmt& x : pd->stmts)
                if (x.uberFn == fn) {
                    const R::FName& nm = R::NameOf(f);
                    entries.emplace_back(c.names->Of(nm.ComparisonIndex, static_cast<uint32_t>(nm.Number)), x.uberEntry);
                }
        }
        std::sort(entries.begin(), entries.end());
        for (const auto& e : entries) {
            r.text += "== entry " + e.first + " ==\n";
            c.locals.clear();
            RenderFlow(c, s, static_cast<size_t>(n), *decoded, static_cast<uint32_t>(e.second), r.text);
        }
        return;
    }
    RenderFlow(c, s, static_cast<size_t>(n), *decoded, 0, r.text);
    for (const Stmt& st : stmts) {
        if (st.uberEntry < 0) continue;
        const uint8_t* us = Rd<const uint8_t*>(st.uberFn, P::off::UStruct_Script);
        const int32_t un = Rd<int32_t>(st.uberFn, P::off::UStruct_ScriptNum);
        Decoded local;
        const Decoded* ust = us && un > 0 ? DecodeCached(c, us, static_cast<size_t>(un), local) : nullptr;
        if (!ust) {
            c.Fail("ubergraph does not decode", 0);
            return;
        }
        r.text += "== " + ObjPath(c, st.uberFn) + " ==\n";
        RenderFlow(c, us, static_cast<size_t>(un), *ust, static_cast<uint32_t>(st.uberEntry), r.text);
    }
}

// A read fault is absorbed: the function reads as undecodable. A stack overflow is passed on: the guard page
// is gone, and the engine would run on an exhausted stack (script_gate's frames make the same choice).
int ReadFaultFilter(unsigned long code) {
    return code == EXCEPTION_STACK_OVERFLOW ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER;
}

int BuildSEH(void* fn, Result* r, Ctx* c) {
    __try {
        Build(fn, *r, *c);
        return 0;
    } __except (ReadFaultFilter(GetExceptionCode())) {
        return 1;
    }
}

void SignatureLine(void* fn, Result& r, Ctx& c) { r.text = SigLine(c, fn); }

int SignatureSEH(void* fn, Result* r, Ctx* c) {
    __try {
        SignatureLine(fn, *r, *c);
        return 0;
    } __except (ReadFaultFilter(GetExceptionCode())) {
        return 1;
    }
}

}  // namespace

Result SignatureOf(void* ufunction) {
    UE_ASSERT_GAME_THREAD("script_fingerprint::SignatureOf");
    Result r;
    Ctx c;
    if (g_pass) c.names = &g_pass->names;
    if (!ufunction || SignatureSEH(ufunction, &r, &c)) {
        r.problem = ufunction ? "fault while reading the signature" : "null function";
        r.text.clear();
        return r;
    }
    r.ok = true;
    r.hash = r.sigHash = Fnv1a64(r.text);
    return r;
}

void BeginPass() {
    UE_ASSERT_GAME_THREAD("script_fingerprint::BeginPass");
    delete g_pass;
    g_pass = new PassCache;
}

void EndPass() {
    UE_ASSERT_GAME_THREAD("script_fingerprint::EndPass");
    delete g_pass;
    g_pass = nullptr;
}

std::string PathOf(void* uobject) {
    UE_ASSERT_GAME_THREAD("script_fingerprint::PathOf");
    Ctx c;
    if (g_pass) c.names = &g_pass->names;
    return uobject ? ObjPath(c, uobject) : std::string();
}

Result Of(void* ufunction) {
    UE_ASSERT_GAME_THREAD("script_fingerprint::Of");
    Result r;
    if (!ufunction) {
        r.problem = "null function";
        return r;
    }
    if (g_pass) {
        auto it = g_pass->results.find(ufunction);
        if (it != g_pass->results.end()) return it->second;
    }
    Ctx c;
    if (g_pass) c.names = &g_pass->names;
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
    if (g_pass) g_pass->results.emplace(ufunction, r);
    return r;
}

}  // namespace ue_wrap::script_fingerprint
