// harness/fingerprint_census.cpp -- see harness/fingerprint_census.h. The rows are ue_wrap/facts'; the
// reference is the table the port tooling generated for the mod's target build; the outcomes and the
// report are docs/bytecode-fingerprint.md's.

#include "harness/fingerprint_census.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "harness/compat_report.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/paths.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_fingerprint.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/walk_timer.h"
#include "ue_wrap/facts/function_row.h"

#include <algorithm>
#include <atomic>
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
namespace GT = ue_wrap::game_thread;
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

enum class Outcome { Match, SigChanged, BodyChanged, Appeared, Gone, KnownGone, Unchecked, Undecodable, NoReference,
                     AnyHolds, AnySigChanged };

const char* Name(Outcome o) {
    switch (o) {
    case Outcome::Match: return "match";
    case Outcome::SigChanged: return "sig-changed";
    case Outcome::BodyChanged: return "body-changed";
    case Outcome::Appeared: return "appeared";
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

// Every outcome that says the running game is not the reference build, or that the census cannot judge a
// row it was released to judge.
bool IsProblem(Outcome o) {
    return o == Outcome::SigChanged || o == Outcome::BodyChanged || o == Outcome::Appeared || o == Outcome::Gone ||
           o == Outcome::Undecodable || o == Outcome::NoReference || o == Outcome::AnySigChanged;
}

struct RowState {
    const F::Function* row = nullptr;
    const RefFn*       ref = nullptr;     // a class row's entry (a run-time class row's once its site resolved it)
    const RefAny*      refAny = nullptr;  // an any-class row's
    std::string        key;               // "class.fn", lowercase
    std::string        order;             // the report's order: feature, then key
    Outcome            outcome = Outcome::Unchecked;
    bool               settled = false;   // false: checked again on a re-try
    bool               dev = false;
    bool               warned = false;    // its problem is logged once; the report file lists it every time
    uint64_t           anyList = 0;       // an any-class row: the subject list it was last checked on
    std::string        detail;
    std::string        path;
    uint64_t           sig = 0, body = 0, refSig = 0, refBody = 0;
    double             resolveUs = 0, printUs = 0;  // the row's measured cost, for the slowest-rows line
};

std::vector<RowState> g_rows;
size_t                g_next = 0;
bool                  g_running = false;    // a pass is under way
bool                  g_reportDue = false;  // the pass's rows are done; its report is the next Tick's work
bool                  g_waiting = false;    // unsettled rows are checked again on a re-try
bool                  g_firstPass = true;   // the report of the first pass is printed whatever it holds
bool                  g_changed = false;    // a row's outcome or detail moved this pass: the report is printed again
bool                  g_redSaid = false;    // the red arm's verdict line is out
uint64_t              g_classSet = 0;
uint64_t              g_passClasses = 0;    // ClassUnlistVersion when the pass's caches began
Clock::time_point     g_lastTry;
double                g_ms = 0;             // the pass's measured work
std::string           g_red;                // [dev] fingerprint_census_red: the row whose reference is falsified
// When the next Tick is due, for the runtime thread that posts it: 0 now (a pass or its report runs), a
// steady-clock millisecond while unsettled rows wait, -1 never (idle).
std::atomic<int64_t>  g_dueMs{-1};
std::atomic<bool>     g_posted{false};      // a Tick is queued; no second one is posted
uint64_t              g_lastDrain = 0;      // the game-thread drain the last Tick ran in

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

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

const RefFn* FindRef(const wchar_t* cls, const wchar_t* fn) {
    for (const RefFn* r = kRefFns; r->cls; ++r)
        if (_wcsicmp(r->cls, cls) == 0 && _wcsicmp(r->fn, fn) == 0) return r;
    return nullptr;
}

const RefAny* FindAny(const wchar_t* fn) {
    for (const RefAny* r = kRefAny; r->fn; ++r)
        if (_wcsicmp(r->fn, fn) == 0) return r;
    return nullptr;
}

// The name of the class a RuntimeClass row's site last resolved it on; empty before that.
std::wstring WitnessName(const F::Function& row) {
    const uint64_t w = row.WitnessClass();
    if (!w) return {};
    return R::ToString(R::FName{static_cast<int32_t>(w >> 32), static_cast<int32_t>(w & 0xffffffffu)});
}

std::string Hex(uint64_t v) {
    char b[20];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
    return b;
}

// ---- any-class subjects: every loaded function whose name an any-class row names ----

// A subject and the name it had when the walk found it: checked again before it is read, so a function that
// died and an object put at its address since are told apart.
struct Subject {
    void*   fn;
    int32_t name;
};
std::unordered_map<std::string, std::vector<Subject>> g_anySubjects;  // lowercase name -> functions
void*    g_fnClasses[2] = {};  // Function and DelegateFunction, native: found once
uint64_t g_anyList = 0;        // moves with every walk that completes: the subject list's identity
uint64_t g_anyClasses = 0;     // ClassListVersion when that walk began
size_t   g_anyFns = 0;         // and the function objects the index held then
bool     g_anyScanned = false; // this pass has its walk (one walk a pass at most)

size_t FunctionCount() { return OI::InstanceCount(g_fnClasses[0]) + OI::InstanceCount(g_fnClasses[1]); }

// A function exists only with its class, and the index counts every function object: while neither the class
// list nor that count moved, the last walk's list is the array's.
bool AnyListCurrent() {
    return g_anyList != 0 && g_anyClasses == OI::ClassListVersion() && g_anyFns == FunctionCount();
}

// One walk of the object array, resumable across frames: it stops at `deadline` (checked every 256
// objects) and goes on from there on the next slice. True when the whole array has been read.
struct AnyScan {
    bool                               begun = false;
    int32_t                            next = 0;
    uint64_t                           classes = 0;
    size_t                             fns = 0;
    std::unordered_set<std::string>    wanted;
    std::unordered_map<int32_t, bool>  interesting;  // comparison index -> its name is wanted (kept across walks)
};
AnyScan g_scan;

bool ScanAnySubjectsStep(Clock::time_point deadline) {
    AnyScan& sc = g_scan;
    if (!sc.begun) {
        sc.begun = true;
        sc.next = 0;
        if (sc.wanted.empty())
            for (const RowState& s : g_rows)
                if (s.row->GetMode() == F::Mode::AnyClass) sc.wanted.insert(Lower(Narrow(s.row->Name())));
        if (!g_fnClasses[0]) {
            g_fnClasses[0] = R::FindClass(L"Function");
            g_fnClasses[1] = R::FindClass(L"DelegateFunction");
        }
        sc.classes = OI::ClassListVersion();
        sc.fns = FunctionCount();
        g_anySubjects.clear();
    }
    const int32_t n = R::NumObjects();
    for (; sc.next < n; ++sc.next) {
        if ((sc.next & 255) == 0 && Clock::now() >= deadline) return false;
        void* obj = R::ObjectAt(sc.next);
        if (!obj) continue;
        void* cls = R::ClassOf(obj);
        if (cls != g_fnClasses[0] && cls != g_fnClasses[1]) continue;
        const R::FName& nm = R::NameOf(obj);
        auto it = sc.interesting.find(nm.ComparisonIndex);
        if (it == sc.interesting.end()) {
            const std::string s = Lower(Narrow(R::ToString(R::FName{nm.ComparisonIndex, 0}).c_str()));
            it = sc.interesting.emplace(nm.ComparisonIndex, sc.wanted.count(s) != 0).first;
        }
        if (it->second && nm.Number == 0)
            g_anySubjects[Lower(Narrow(R::ToString(nm).c_str()))].push_back(Subject{obj, nm.ComparisonIndex});
    }
    sc.begun = false;
    ++g_anyList;
    g_anyClasses = sc.classes;
    g_anyFns = sc.fns;
    g_anyScanned = true;
    return true;
}

void ScanAnySubjects() {  // the capture's one walk, whole
    g_scan.begun = false;
    while (!ScanAnySubjectsStep(Clock::time_point::max())) {}
}

// The walk's maps hold every function name of the process: kept only while an any-class row waits for more
// of its classes to load.
void ReleaseAnySubjects() {
    g_scan = AnyScan{};
    std::unordered_map<std::string, std::vector<Subject>>().swap(g_anySubjects);
    g_anyList = 0;
}

// ---- one row ----

void CheckAny(RowState& s) {
    const RefAny* ref = s.refAny;
    if (!ref) {
        s.outcome = Outcome::NoReference;
        s.detail = "the reference has no entry for the name";
        s.settled = true;
        return;
    }
    if (s.anyList == g_anyList) return;  // checked on this very list: nothing it reads has moved
    s.anyList = g_anyList;
    size_t loaded = 0, outside = 0;
    const auto it = g_anySubjects.find(Lower(Narrow(s.row->Name())));
    if (it != g_anySubjects.end()) {
        for (const Subject& sub : it->second) {
            if (!R::IsLive(sub.fn) || R::NameOf(sub.fn).ComparisonIndex != sub.name) continue;  // gone since the walk
            ++loaded;
            const FP::Result r = FP::SignatureOf(sub.fn);
            bool known = false;
            for (int k = 0; k < ref->sigCount && !known; ++k) known = kRefAnySigs[ref->firstSig + k] == r.sigHash;
            if (!known) ++outside;
        }
    }
    const std::string of = " (" + std::to_string(ref->classes) + " declaring class(es) on " + kRefBuild + ")";
    if (outside) {
        s.outcome = Outcome::AnySigChanged;
        s.detail = std::to_string(outside) + " of " + std::to_string(loaded) + " loaded signature(s) outside the reference's " +
                   std::to_string(ref->sigCount) + of;
        s.settled = true;
    } else if (!loaded) {
        // Nothing to judge: no declaring class is loaded yet, or the name has no class on this build any more.
        s.outcome = Outcome::Unchecked;
        s.detail = "no function of the name is loaded" + of;
    } else {
        s.outcome = Outcome::AnyHolds;
        s.detail = std::to_string(loaded) + " loaded, every signature inside the reference's " +
                   std::to_string(ref->sigCount) + of;
        s.settled = loaded >= static_cast<size_t>(ref->classes);  // until then, a class loading later is checked too
    }
}

void CheckClass(RowState& s) {
    const F::Function& row = *s.row;
    void* cls = nullptr;
    if (row.Class()) {
        cls = OI::ClassByName(row.Class());
    } else {
        // A run-time class row is keyed on the class its site resolved it on.
        const std::wstring witness = WitnessName(row);
        if (witness.empty()) {
            s.outcome = Outcome::Unchecked;
            s.detail = "the class is known only at run time, and its site has not resolved it yet";
            return;  // re-tried: the site may resolve it later in the session
        }
        cls = OI::ClassByName(witness.c_str());
        s.ref = FindRef(witness.c_str(), row.Name());
        s.detail.clear();
    }
    const RefFn* ref = s.ref;
    s.refSig = s.refBody = 0;
    if (ref && ref->path) {
        s.refSig = ref->sig;
        s.refBody = ref->body;
        if (!g_red.empty() && g_red == s.key) s.refBody ^= 1;  // the red arm: a reference that cannot match
    }
    if (!cls) {
        s.outcome = (ref && !ref->path) ? Outcome::KnownGone : Outcome::Unchecked;
        s.detail = ref && !ref->path ? "absent on the reference build too" : "class not loaded";
        s.settled = ref && !ref->path;
        return;
    }
    const Clock::time_point r0 = Clock::now();
    const F::Resolved fn = F::ResolveOn(row, cls);  // the class is in hand: no second class lookup
    s.resolveUs = std::chrono::duration<double, std::micro>(Clock::now() - r0).count();
    if (!fn) {
        s.outcome = !ref ? Outcome::NoReference : (ref->path ? Outcome::Gone : Outcome::KnownGone);
        s.detail = "the class does not have it";
        s.settled = true;
        return;
    }
    const Clock::time_point p0 = Clock::now();
    const FP::Result r = FP::Of(fn.Fn());
    s.printUs = std::chrono::duration<double, std::micro>(Clock::now() - p0).count();
    s.path = FP::PathOf(fn.Fn());
    s.sig = r.sigHash;
    s.body = r.bodyHash;
    s.settled = true;
    s.detail.clear();
    if (!r.ok) {
        s.outcome = Outcome::Undecodable;
        s.detail = r.problem;
    } else if (!ref) {
        s.outcome = Outcome::NoReference;
        s.detail = "the reference has no entry for this class";
    } else if (!ref->path) {
        s.outcome = Outcome::Appeared;
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
    const Outcome was = s.outcome;
    const std::string wasDetail = s.detail;
    if (s.row->GetMode() == F::Mode::AnyClass) {
        CheckAny(s);  // Slice has its walk before any any-class row is checked
    } else {
        CheckClass(s);
    }
    if (s.outcome != was || s.detail != wasDetail) g_changed = true;
}

// ---- the pass, its report ----

void Report() {
    int classRows = 0, matched = 0, anyRows = 0, anyHold = 0, unchecked = 0, problems = 0, devRows = 0;
    std::vector<RowState*> sorted;
    sorted.reserve(g_rows.size());
    for (RowState& s : g_rows) sorted.push_back(&s);
    std::sort(sorted.begin(), sorted.end(), [](const RowState* a, const RowState* b) { return a->order < b->order; });
    CR::Begin("fingerprint-census");
    CR::Line("fingerprint-census", std::string("=== Function fingerprints against ") + kRefBuild +
                                       " (docs/bytecode-fingerprint.md) ===");
    for (RowState* sp : sorted) {
        RowState& s = *sp;
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
        // A kind that refused and later accepted (a retry from a tick) registered in the end.
        const bool refused = (row.Refused() & ~row.Accepted()) != 0;
        if (!s.dev && (IsProblem(s.outcome) || refused)) {
            ++problems;
            CR::Failure("fingerprint-census", CR::Severity::Important);
            // Once per row: a re-try's report repeats the count and the file, not the lines already said.
            if (!s.warned) {
                s.warned = true;
                UE_LOGW("fingerprint-census: %s %s (feature %s)%s%s", Name(s.outcome), s.key.c_str(), row.Feature(),
                        refused ? " -- its registration was REFUSED" : "",
                        s.detail.empty() ? "" : (" -- " + s.detail).c_str());
            }
        }
        if (!g_redSaid && !g_red.empty() && g_red == s.key && s.settled) {
            g_redSaid = true;
            if (s.outcome == Outcome::BodyChanged)
                UE_LOGW("fingerprint-census: RED ARM %s reported body changed", g_red.c_str());
            else
                UE_LOGE("fingerprint-census: RED ARM FAILED %s (%s)", g_red.c_str(), Name(s.outcome));
        }
        std::string line = std::string(Name(s.outcome)) + "  " + row.Feature() + "  " + s.key;
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
    // Where the pass's time went: the five costliest rows, resolve and fingerprint apart.
    std::vector<const RowState*> slow;
    for (const RowState& s : g_rows) slow.push_back(&s);
    std::sort(slow.begin(), slow.end(), [](const RowState* a, const RowState* b) {
        return a->resolveUs + a->printUs > b->resolveUs + b->printUs;
    });
    std::string top;
    for (size_t i = 0; i < slow.size() && i < 5; ++i) {
        char b[160];
        std::snprintf(b, sizeof b, "%s%s resolve %.0f us + fingerprint %.0f us", i ? "; " : "", slow[i]->key.c_str(),
                      slow[i]->resolveUs, slow[i]->printUs);
        top += b;
    }
    UE_LOGI("fingerprint-census: costliest rows: %s", top.c_str());
    if (problems) {
        UE_LOGW("%s -- DEGRADED: the game differs from the build this mod was made for", sum);
    } else {
        UE_LOGI("%s", sum);
    }
}

// The pass and its report are done: wait for the rows that are not settled, if any.
void Finish() {
    g_firstPass = false;
    g_changed = false;
    g_waiting = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& s) { return !s.settled; });
    const bool anyWaits = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& s) {
        return !s.settled && s.row->GetMode() == F::Mode::AnyClass;
    });
    if (!anyWaits) ReleaseAnySubjects();
    g_dueMs.store(g_waiting ? NowMs() + 1000 : -1, std::memory_order_release);
    g_classSet = OI::ClassSetVersion();
    g_lastTry = Clock::now();
}

void BeginPassState() {
    FP::BeginPass();
    g_passClasses = OI::ClassUnlistVersion();
    g_anyScanned = AnyListCurrent();
    g_next = 0;
    g_running = true;
    g_dueMs.store(0, std::memory_order_release);
}

void Slice() {
    ue_wrap::ScopedWalkTimer walk("fingerprint-census slice");
    const Clock::time_point t0 = Clock::now();
    const auto budget = std::chrono::microseconds(2000);
    // A class unloaded since the pass began: a function the pass's caches hold may be gone, and its address a
    // new one's. Settled rows keep what they read; the rest read afresh. A class loading frees nothing, and
    // classes load all through a world's first seconds: dropping the caches then cost a pass half again.
    if (OI::ClassUnlistVersion() != g_passClasses) {
        FP::BeginPass();
        g_passClasses = OI::ClassUnlistVersion();
    }
    const bool anyPending = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& r) {
        return !r.settled && r.row->GetMode() == F::Mode::AnyClass && r.refAny;
    });
    if (anyPending && !g_anyScanned && !ScanAnySubjectsStep(t0 + budget)) {
        g_ms += std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        return;  // the walk goes on next frame; the rows wait for it
    }
    while (g_next < g_rows.size() && Clock::now() - t0 < budget) {
        RowState& s = g_rows[g_next++];
        if (s.settled) continue;
        Check(s);
    }
    g_ms += std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    if (g_next < g_rows.size()) return;
    g_running = false;
    FP::EndPass();
    if (g_firstPass || g_changed) {
        g_reportDue = true;  // the report in a Tick of its own, outside the rows' budget
        return;
    }
    Finish();  // a re-try that changed nothing says nothing
}

void Tick() {
    if (g_running) {
        Slice();
        return;
    }
    if (g_reportDue) {
        g_reportDue = false;
        Report();
        Finish();
        return;
    }
    if (!g_waiting) return;
    const uint64_t classSet = OI::ClassSetVersion();
    if (classSet == g_classSet && Clock::now() - g_lastTry < std::chrono::seconds(5)) {
        g_dueMs.store(NowMs() + 1000, std::memory_order_release);  // ask the class set again in a second
        return;
    }
    // Some row waits for its class: try those again now that the class set moved or the timer ran out.
    BeginPassState();
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
        for (const Subject& sub : subjects) fns.emplace_back(FP::PathOf(sub.fn), sub.fn);
    std::vector<std::string> rowLines;
    for (const F::Function* row = F::First(); row; row = row->Next()) {
        F::Resolved r;
        if (row->GetMode() != F::Mode::AnyClass) {
            const std::wstring witness = row->Class() ? std::wstring() : WitnessName(*row);
            if (void* cls = OI::ClassByName(row->Class() ? row->Class() : witness.c_str())) r = F::ResolveOn(*row, cls);
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

// The red arm's row, said at once when the key names none or names an any-class row: a run waiting on the
// verdict line would otherwise wait on a clock.
void CheckRedKey() {
    if (g_red.empty()) return;
    for (const RowState& s : g_rows) {
        if (s.key != g_red) continue;
        if (s.row->GetMode() == F::Mode::AnyClass) {
            g_redSaid = true;
            UE_LOGE("fingerprint-census: RED ARM FAILED %s (an any-class row: the red arm falsifies a class row's body)",
                    g_red.c_str());
        }
        return;
    }
    g_redSaid = true;
    UE_LOGE("fingerprint-census: RED ARM FAILED %s (no row has that key)", g_red.c_str());
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
        s.key = Key(*row);
        s.order = std::string(row->Feature()) + " " + s.key;
        if (row->GetMode() == F::Mode::AnyClass) {
            s.refAny = FindAny(row->Name());
        } else if (row->Class()) {
            s.ref = FindRef(row->Class(), row->Name());
        }
        g_rows.push_back(s);
    }
    g_redSaid = false;
    CheckRedKey();
    g_ms = 0;
    g_waiting = false;
    g_firstPass = true;
    g_changed = false;
    g_reportDue = false;
    if (g_rows.empty()) {
        g_dueMs.store(-1, std::memory_order_release);
        return;
    }
    BeginPassState();
    UE_LOGI("fingerprint-census: %zu row(s) against the %s reference, checked over the next frames", g_rows.size(),
            kRefBuild);
}

void PostIfDue() {
    const int64_t due = g_dueMs.load(std::memory_order_acquire);
    if (!(due == 0 || (due > 0 && NowMs() >= due))) return;
    bool idle = false;
    if (!g_posted.compare_exchange_strong(idle, true, std::memory_order_acq_rel)) return;
    GT::Post([] {
        // Cleared as the task ends, a faulted one included (the image unwinds on a structured exception).
        struct Clear {
            ~Clear() { g_posted.store(false, std::memory_order_release); }
        } clear;
        // One Tick a drain: a drain runs every task posted while it runs, so a stall ends in one slice, not a
        // run of slices with no engine frame between them (harness/pump.h's composite, the same rule).
        const uint64_t drain = GT::DrainSerial();
        if (drain == g_lastDrain) return;
        g_lastDrain = drain;
        Tick();
    });
}

}  // namespace harness::fingerprint_census
