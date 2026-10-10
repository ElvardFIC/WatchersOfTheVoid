// harness/fingerprint_census.cpp -- see harness/fingerprint_census.h. The rows are ue_wrap/facts'; the
// reference is the table the port tooling generated for the mod's target build; the outcomes and the
// report are docs/bytecode-fingerprint.md's.

#include "harness/fingerprint_census.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "harness/compat_report.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/paths.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_fingerprint.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/walk_timer.h"
#include "ue_wrap/facts/function_row.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace harness::fingerprint_census {
namespace {

namespace R = ue_wrap::reflection;
namespace P = ue_wrap::profile;
namespace FP = ue_wrap::script_fingerprint;
namespace F = ue_wrap::facts;
namespace OI = ue_wrap::object_index;
namespace CR = harness::compat_report;
using Clock = std::chrono::steady_clock;

// The reference of the target build: one entry per class row key (present with its two hashes, or absent),
// one per any-class name (its distinct signature hashes). Each array ends with a null entry.
struct RefFn {
    const wchar_t* cls;
    const wchar_t* fn;
    const wchar_t* path;  // the declaring function, as the canonical text writes it; null when absent
    uint64_t       sig;
    uint64_t       body;
};
struct RefAny {
    const wchar_t* fn;
    int            classes;   // classes that declare the name on that build
    int            firstSig;  // into kRefAnySigs
    int            sigCount;
};
#include VOTVCOOP_FINGERPRINT_TABLE

enum class Outcome { Match, SigChanged, BodyChanged, Gone, KnownGone, Unchecked, Undecodable, NoReference, AnyHolds,
                     AnySigChanged };

const char* Name(Outcome o) {
    switch (o) {
    case Outcome::Match: return "match";
    case Outcome::SigChanged: return "sig-changed";
    case Outcome::BodyChanged: return "body-changed";
    case Outcome::Gone: return "gone";
    case Outcome::KnownGone: return "known-gone";
    case Outcome::Unchecked: return "unchecked";
    case Outcome::Undecodable: return "undecodable";
    case Outcome::NoReference: return "no-reference";
    case Outcome::AnyHolds: return "holds";
    case Outcome::AnySigChanged: return "sig-changed";
    }
    return "?";
}

bool IsProblem(Outcome o) {
    return o == Outcome::SigChanged || o == Outcome::BodyChanged || o == Outcome::Gone || o == Outcome::Undecodable ||
           o == Outcome::AnySigChanged;
}

struct RowState {
    const F::Function* row = nullptr;
    Outcome            outcome = Outcome::Unchecked;
    bool               settled = false;  // false: re-tried when the class set moves or on the timer
    bool               dev = false;
    std::string        detail;
    std::string        path;
    uint64_t           sig = 0, body = 0, refSig = 0, refBody = 0;
};

std::vector<RowState> g_rows;
size_t                g_next = 0;
bool                  g_running = false;   // a pass is under way
bool                  g_waiting = false;   // unsettled rows wait for their class
bool                  g_firstPass = true;  // the report of the first pass is printed whatever it holds
bool                  g_settledMore = false;  // a re-try settled a row: the report is printed again
uint64_t              g_classSet = 0;
Clock::time_point     g_lastTry;
double                g_ms = 0;            // the pass's measured work
std::string           g_red;               // [dev] fingerprint_census_red: the row whose reference is falsified

std::string Narrow(const wchar_t* s) {
    std::string out;
    for (; s && *s; ++s) out += (*s < 0x80) ? static_cast<char>(*s) : '?';
    return out;
}

std::string Lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string Key(const F::Function& row) {
    return Lower((row.Class() ? Narrow(row.Class()) : std::string("*")) + "." + Narrow(row.Name()));
}

const RefFn* FindRef(const F::Function& row) {
    const wchar_t* cls = row.Class() ? row.Class() : L"*";  // a run-time class row is keyed on its name alone
    for (const RefFn* r = kRefFns; r->cls; ++r)
        if (_wcsicmp(r->cls, cls) == 0 && _wcsicmp(r->fn, row.Name()) == 0) return r;
    return nullptr;
}

// The class a RuntimeClass row's site last resolved it on, found again by name; null before that.
void* WitnessClass(const F::Function& row) {
    const uint64_t w = row.WitnessClass();
    if (!w) return nullptr;
    const R::FName n{static_cast<int32_t>(w >> 32), static_cast<int32_t>(w & 0xffffffffu)};
    return OI::ClassByName(R::ToString(n).c_str());
}

const RefAny* FindAny(const F::Function& row) {
    for (const RefAny* r = kRefAny; r->fn; ++r)
        if (_wcsicmp(r->fn, row.Name()) == 0) return r;
    return nullptr;
}

std::string Hex(uint64_t v) {
    char b[20];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
    return b;
}

// ---- any-class subjects: every loaded function whose name an any-class row names ----

std::unordered_map<std::string, std::vector<void*>> g_anySubjects;  // lowercase name -> functions
bool g_anyScanned = false;

// One walk of the object array, resumable across frames: it stops at `deadline` (checked every 256
// objects) and goes on from there on the next slice. True when the whole array has been read.
struct AnyScan {
    bool                               begun = false;
    int32_t                            next = 0;
    void*                              fnClasses[2] = {};
    std::unordered_set<std::string>    wanted;
    std::unordered_map<int32_t, bool>  interesting;  // comparison index -> its name is wanted
};
AnyScan g_scan;

bool ScanAnySubjectsStep(Clock::time_point deadline) {
    AnyScan& sc = g_scan;
    if (!sc.begun) {
        sc = AnyScan{};
        sc.begun = true;
        g_anySubjects.clear();
        for (const RowState& s : g_rows)
            if (s.row->GetMode() == F::Mode::AnyClass) sc.wanted.insert(Lower(Narrow(s.row->Name())));
        sc.fnClasses[0] = R::FindClass(L"Function");
        sc.fnClasses[1] = R::FindClass(L"DelegateFunction");
    }
    const int32_t n = R::NumObjects();
    for (; sc.next < n; ++sc.next) {
        if ((sc.next & 255) == 0 && Clock::now() >= deadline) return false;
        void* obj = R::ObjectAt(sc.next);
        if (!obj) continue;
        void* cls = R::ClassOf(obj);
        if (cls != sc.fnClasses[0] && cls != sc.fnClasses[1]) continue;
        const R::FName& nm = R::NameOf(obj);
        auto it = sc.interesting.find(nm.ComparisonIndex);
        if (it == sc.interesting.end()) {
            const std::string s = Lower(Narrow(R::ToString(R::FName{nm.ComparisonIndex, 0}).c_str()));
            it = sc.interesting.emplace(nm.ComparisonIndex, sc.wanted.count(s) != 0).first;
        }
        if (it->second && nm.Number == 0) g_anySubjects[Lower(Narrow(R::ToString(nm).c_str()))].push_back(obj);
    }
    sc.begun = false;
    g_anyScanned = true;
    return true;
}

void ScanAnySubjects() {  // the capture's one pass, whole
    g_scan.begun = false;
    while (!ScanAnySubjectsStep(Clock::time_point::max())) {}
}

// ---- one row ----

void CheckAny(RowState& s) {
    const RefAny* ref = FindAny(*s.row);
    const auto it = g_anySubjects.find(Lower(Narrow(s.row->Name())));
    const size_t loaded = it == g_anySubjects.end() ? 0 : it->second.size();
    if (!ref) {
        s.outcome = Outcome::NoReference;
        s.settled = true;
        return;
    }
    size_t outside = 0;
    if (loaded) {
        for (void* fn : it->second) {
            if (!R::IsLive(fn)) continue;  // found on an earlier frame: its class may have unloaded since
            const FP::Result r = FP::SignatureOf(fn);
            bool known = false;
            for (int k = 0; k < ref->sigCount && !known; ++k) known = kRefAnySigs[ref->firstSig + k] == r.sigHash;
            if (!known) ++outside;
        }
    }
    s.outcome = outside ? Outcome::AnySigChanged : Outcome::AnyHolds;
    s.detail = std::to_string(outside) + " of " + std::to_string(loaded) + " loaded signature(s) outside the reference's " +
               std::to_string(ref->sigCount) + " (" + std::to_string(ref->classes) + " declaring classes on " +
               kRefBuild + ")";
    s.settled = true;
}

void CheckClass(RowState& s) {
    const F::Function& row = *s.row;
    const RefFn* ref = FindRef(row);
    if (ref && ref->path) {
        s.refSig = ref->sig;
        s.refBody = ref->body;
        if (!g_red.empty() && g_red == Key(row)) s.refBody ^= 1;  // the red arm: a reference that cannot match
    }
    void* cls = row.Class() ? OI::ClassByName(row.Class()) : WitnessClass(row);
    if (!row.Class() && !cls) {
        s.outcome = Outcome::Unchecked;
        s.detail = "the class is known only at run time, and its site has not resolved it yet";
        return;  // re-tried: the site may resolve it later in the session
    }
    if (!cls) {
        s.outcome = (ref && !ref->path) ? Outcome::KnownGone : Outcome::Unchecked;
        s.detail = ref && !ref->path ? "absent on the reference build too" : "class not loaded";
        s.settled = ref && !ref->path;
        return;
    }
    const F::Resolved fn = row.GetMode() == F::Mode::RuntimeClass ? F::ResolveOn(row, cls) : F::Resolve(row);
    if (!fn) {
        s.outcome = !ref ? Outcome::NoReference : (ref->path ? Outcome::Gone : Outcome::KnownGone);
        s.detail = "the class does not have it";
        s.settled = true;
        return;
    }
    const FP::Result r = FP::Of(fn.Fn());
    s.path = FP::PathOf(fn.Fn());
    s.sig = r.sigHash;
    s.body = r.bodyHash;
    s.settled = true;
    if (!r.ok) {
        s.outcome = Outcome::Undecodable;
        s.detail = r.problem;
    } else if (!ref) {
        s.outcome = Outcome::NoReference;
    } else if (!ref->path) {
        s.outcome = Outcome::BodyChanged;
        s.detail = "present, but absent on the reference build";
    } else if (r.sigHash != s.refSig) {
        s.outcome = Outcome::SigChanged;
    } else if (r.bodyHash != s.refBody) {
        s.outcome = Outcome::BodyChanged;
    } else {
        s.outcome = Outcome::Match;
    }
}

void Check(RowState& s) {
    if (s.row->GetMode() == F::Mode::AnyClass) {
        CheckAny(s);  // Slice has finished the scan before any any-class row is checked
    } else {
        CheckClass(s);
    }
}

// ---- the pass, its report ----

void Report() {
    int classRows = 0, matched = 0, anyRows = 0, anyHold = 0, unchecked = 0, problems = 0, devRows = 0;
    std::vector<const RowState*> sorted;
    for (const RowState& s : g_rows) sorted.push_back(&s);
    std::sort(sorted.begin(), sorted.end(), [](const RowState* a, const RowState* b) {
        return std::string(a->row->Feature()) + Key(*a->row) < std::string(b->row->Feature()) + Key(*b->row);
    });
    CR::Begin("fingerprint-census");
    CR::Line("fingerprint-census", std::string("=== Function fingerprints against ") + kRefBuild +
                                       " (docs/bytecode-fingerprint.md) ===");
    for (const RowState* sp : sorted) {
        const RowState& s = *sp;
        const F::Function& row = *s.row;
        if (s.dev) {
            ++devRows;
        } else if (row.GetMode() == F::Mode::AnyClass) {
            ++anyRows;
            if (s.outcome == Outcome::AnyHolds) ++anyHold;
        } else {
            ++classRows;
            if (s.outcome == Outcome::Match) ++matched;
        }
        if (s.outcome == Outcome::Unchecked && !s.dev) ++unchecked;
        const bool refused = row.Refused() != 0;
        if (!s.dev && (IsProblem(s.outcome) || refused)) {
            ++problems;
            UE_LOGW("fingerprint-census: %s %s (feature %s)%s%s", Name(s.outcome), Key(row).c_str(), row.Feature(),
                    refused ? " -- its registration was REFUSED" : "", s.detail.empty() ? "" : (" -- " + s.detail).c_str());
        }
        if (!g_red.empty() && g_red == Key(row)) {
            if (s.outcome == Outcome::BodyChanged)
                UE_LOGW("fingerprint-census: RED ARM %s reported body changed", g_red.c_str());
            else
                UE_LOGE("fingerprint-census: RED ARM FAILED %s (%s)", g_red.c_str(), Name(s.outcome));
        }
        std::string line = std::string(Name(s.outcome)) + "  " + row.Feature() + "  " + Key(row);
        if (!s.path.empty()) line += " -> " + s.path;
        if (s.sig || s.body) line += "  live " + Hex(s.sig) + "/" + Hex(s.body);
        if (s.refSig || s.refBody) line += "  ref " + Hex(s.refSig) + "/" + Hex(s.refBody);
        if (row.Accepted() == 0 && row.Refused() == 0) line += "  (not registered this session)";
        if (refused) line += "  (registration refused)";
        if (!s.detail.empty()) line += "  -- " + s.detail;
        CR::Line("fingerprint-census", line);
    }
    char sum[256];
    std::snprintf(sum, sizeof sum,
                  "fingerprint-census: %d of %d class rows match, %d of %d any-class rows hold, %d unchecked, %d "
                  "problem row(s), %d dev row(s) apart (reference %s, %.1f ms)",
                  matched, classRows, anyHold, anyRows, unchecked, problems, devRows, kRefBuild, g_ms);
    CR::Line("fingerprint-census", sum);
    CR::Write();
    if (problems) {
        UE_LOGW("%s -- DEGRADED: the game differs from the build this mod was made for", sum);
    } else {
        UE_LOGI("%s", sum);
    }
}

void Slice() {
    ue_wrap::ScopedWalkTimer walk("fingerprint-census slice");
    const Clock::time_point t0 = Clock::now();
    const auto budget = std::chrono::microseconds(2000);
    const bool anyPending = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& r) {
        return !r.settled && r.row->GetMode() == F::Mode::AnyClass;
    });
    if (anyPending && !g_anyScanned && !ScanAnySubjectsStep(t0 + budget)) {
        g_ms += std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        return;  // the scan goes on next frame; the rows wait for it
    }
    while (g_next < g_rows.size() && Clock::now() - t0 < budget) {
        RowState& s = g_rows[g_next++];
        if (s.settled) continue;
        Check(s);
        if (s.settled && !g_firstPass) g_settledMore = true;
    }
    g_ms += std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    if (g_next < g_rows.size()) return;
    g_running = false;
    if (g_firstPass || g_settledMore) Report();  // a re-try that settled nothing says nothing
    g_firstPass = false;
    g_settledMore = false;
    g_waiting = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& s) { return !s.settled; });
    g_classSet = OI::ClassSetVersion();
    g_lastTry = Clock::now();
}

// ---- the capture ([dev] fingerprint_capture) ----

// Every function of every loaded Blueprint class with its canonical text, sorted by path so two captures
// diff line by line, and every row with what it resolves to (the native half of a reference table).
void Capture() {
    void* const bpClasses[] = {R::FindClass(L"BlueprintGeneratedClass"), R::FindClass(L"WidgetBlueprintGeneratedClass"),
                               R::FindClass(L"AnimBlueprintGeneratedClass")};
    void* const fnClasses[] = {R::FindClass(L"Function"), R::FindClass(L"DelegateFunction")};
    std::vector<std::pair<std::string, void*>> fns;
    const int32_t n = R::NumObjects();
    int classes = 0;
    for (int32_t i = 0; i < n; ++i) {
        void* obj = R::ObjectAt(i);
        if (!obj) continue;
        void* cls = R::ClassOf(obj);
        if (!cls || (cls != bpClasses[0] && cls != bpClasses[1] && cls != bpClasses[2])) continue;
        ++classes;
        void* field = *reinterpret_cast<void**>(static_cast<uint8_t*>(obj) + P::off::UStruct_Children);
        for (int guard = 0; field && guard < 4096;
             ++guard, field = *reinterpret_cast<void**>(static_cast<uint8_t*>(field) + P::off::UField_Next)) {
            void* fc = R::ClassOf(field);
            if (fc == fnClasses[0] || fc == fnClasses[1]) fns.emplace_back(FP::PathOf(field), field);
        }
    }
    // An any-class row's subjects, natives included: the reference's signature set is the union of the
    // pak's Blueprint declarations and these.
    g_rows.clear();
    for (const F::Function* row = F::First(); row; row = row->Next()) {
        RowState s;
        s.row = row;
        g_rows.push_back(s);
    }
    ScanAnySubjects();
    for (const auto& [name, subjects] : g_anySubjects)
        for (void* fn : subjects) fns.emplace_back(FP::PathOf(fn), fn);
    std::vector<std::string> rowLines;
    for (const F::Function* row = F::First(); row; row = row->Next()) {
        F::Resolved r;
        if (row->GetMode() == F::Mode::Declared || row->GetMode() == F::Mode::Dispatch) {
            if (row->Class() && OI::ClassByName(row->Class())) r = F::Resolve(*row);
        } else if (row->GetMode() == F::Mode::RuntimeClass) {
            if (void* cls = row->Class() ? OI::ClassByName(row->Class()) : WitnessClass(*row)) r = F::ResolveOn(*row, cls);
        }
        static const char* kModes[] = {"Declared", "Dispatch", "AnyClass", "RuntimeClass"};
        rowLines.push_back("#row " + std::string(row->Feature()) + " " + kModes[static_cast<int>(row->GetMode())] + " " +
                           Key(*row) + " -> " + (r ? FP::PathOf(r.Fn()) : std::string("-")));
        if (r) fns.emplace_back(FP::PathOf(r.Fn()), r.Fn());
    }
    std::sort(fns.begin(), fns.end());
    fns.erase(std::unique(fns.begin(), fns.end()), fns.end());
    std::sort(rowLines.begin(), rowLines.end());
    const std::wstring path = ue_wrap::paths::ExeDir() + L"\\multivoid-fingerprints-capture.txt";
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
        UE_LOGE("fingerprint-census: cannot open '%ls' for the capture", path.c_str());
        return;
    }
    for (const std::string& l : rowLines) std::fprintf(f, "%s\n", l.c_str());
    int ok = 0, problems = 0;
    for (const auto& [p, fn] : fns) {
        const FP::Result r = FP::Of(fn);
        if (r.ok) ++ok; else ++problems;
        std::fprintf(f, "### %s %016llx %s\n", p.c_str(), static_cast<unsigned long long>(r.hash),
                     r.ok ? (r.native ? "native" : "ok") : r.problem.c_str());
        std::fwrite(r.text.data(), 1, r.text.size(), f);
    }
    std::fclose(f);
    UE_LOGW("fingerprint-census: CAPTURE %zu function(s) (%d loaded Blueprint class(es) and %zu row(s)), %d decoded, "
            "%d with a problem -> '%ls'", fns.size(), classes, rowLines.size(), ok, problems, path.c_str());
}

}  // namespace

void Run() {
    if (coop::config::ResolveFlag(::coop::config_registry::rows::fingerprint_capture)) Capture();
    g_red = Lower(coop::config::ResolveString(::coop::config_registry::rows::fingerprint_census_red));
    g_rows.clear();
    for (const F::Function* row = F::First(); row; row = row->Next()) {
        RowState s;
        s.row = row;
        s.dev = std::strncmp(row->Feature(), "dev.", 4) == 0;
        g_rows.push_back(s);
    }
    g_next = 0;
    g_ms = 0;
    g_anyScanned = false;
    g_scan.begun = false;
    g_running = !g_rows.empty();
    g_waiting = false;
    g_firstPass = true;
    g_settledMore = false;
    UE_LOGI("fingerprint-census: %zu row(s) against the %s reference, checked over the next frames", g_rows.size(),
            kRefBuild);
}

void Tick() {
    if (g_running) {
        Slice();
        return;
    }
    if (!g_waiting) return;
    const uint64_t classSet = OI::ClassSetVersion();
    if (classSet == g_classSet && Clock::now() - g_lastTry < std::chrono::seconds(5)) return;
    // Some row's class was not loaded: try those again now that the class set moved or the timer ran out.
    g_next = 0;
    g_running = true;
    g_anyScanned = false;
    g_scan.begun = false;
}

}  // namespace harness::fingerprint_census
