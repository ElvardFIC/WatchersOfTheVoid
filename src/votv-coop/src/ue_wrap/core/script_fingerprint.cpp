// ue_wrap/core/script_fingerprint.cpp -- the flow order, the signature line and the hashes over the
// decoder of script_bytecode.h; every rule is docs/bytecode-fingerprint.md's.

#include "ue_wrap/core/script_fingerprint.h"

#include "script_bytecode.h"
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

// Flow order: follow a straight run (an EX_Jump is layout glue), defer every other code target LIFO in
// operand order, mark where a run merges into a labelled statement.
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

// ---- the pass cache ----

// An ubergraph's decoded statements, kept for a census pass: every event stub of a class follows the same
// ubergraph, and mainPlayer_C's is decoded once instead of once per stub. Keyed on the script's address
// and size; a pass is a few seconds of one world. Game thread.
struct PassCache {
    std::unordered_map<const uint8_t*, std::pair<size_t, std::vector<Stmt>>> scans;
};
PassCache* g_pass = nullptr;

const std::vector<Stmt>* ScanUbergraph(Ctx& c, const uint8_t* s, size_t n, std::vector<Stmt>& local) {
    if (g_pass) {
        auto it = g_pass->scans.find(s);
        if (it != g_pass->scans.end() && it->second.first == n) return &it->second.second;
    }
    if (!Scan(c, s, n, local)) return nullptr;
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
    std::vector<Stmt> own;
    const bool uber = IsUbergraph(c, fn);
    const std::vector<Stmt>* scanned =
        uber ? ScanUbergraph(c, s, static_cast<size_t>(n), own) : (Scan(c, s, static_cast<size_t>(n), own) ? &own : nullptr);
    if (!scanned) return;
    const std::vector<Stmt>& stmts = *scanned;
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
        std::vector<Stmt> local;
        const std::vector<Stmt>* ust = us && un > 0 ? ScanUbergraph(c, us, static_cast<size_t>(un), local) : nullptr;
        if (!ust) {
            c.Fail("ubergraph does not decode", 0);
            return;
        }
        r.text += "== " + ObjPath(c, st.uberFn) + " ==\n";
        RenderFlow(c, us, static_cast<size_t>(un), *ust, static_cast<uint32_t>(st.uberEntry), r.text);
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

void SignatureLine(void* fn, Result& r, Ctx& c) { r.text = SigLine(c, fn); }

int SignatureSEH(void* fn, Result* r, Ctx* c) {
    __try {
        SignatureLine(fn, *r, *c);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

}  // namespace

Result SignatureOf(void* ufunction) {
    Result r;
    Ctx c;
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
    delete g_pass;
    g_pass = new PassCache;
}

void EndPass() {
    delete g_pass;
    g_pass = nullptr;
}

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
