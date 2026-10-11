// harness/fingerprint_census.cpp -- see harness/fingerprint_census.h. The rows are ue_wrap/facts'; the
// reference is the table the port tooling generated for the mod's target build; the outcomes and the
// report are docs/bytecode-fingerprint.md's.

#include "harness/fingerprint_census.h"

#include "fingerprint_census_detail.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "harness/compat_report.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
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
using namespace detail;
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
    bool               warnedRefused = false;  // and its refused registration, once
    // An any-class row: its subjects checked so far (the next one to check, how many were read, how many fell
    // outside the reference). Checked as their classes load, each once.
    size_t             anyNext = 0, anyChecked = 0, anyOutside = 0;
    std::string        detail;
    std::string        path;
    uint64_t           sig = 0, body = 0, refSig = 0, refBody = 0;
    double             resolveUs = 0, printUs = 0;  // the row's measured cost, for the slowest-rows line
};

// What a finished pass owes: nothing, the report file (a count moved), or the file and the log lines (an
// outcome moved, or a registration was refused).
enum class ReportKind { None, File, Full };

std::vector<RowState> g_rows;
size_t                g_next = 0;
bool                  g_running = false;    // a pass is under way
ReportKind            g_reportDue = ReportKind::None;  // the pass's report, the next Tick's work
bool                  g_waiting = false;    // unsettled rows are checked again on a re-try
bool                  g_firstPass = true;   // the report of the first pass is printed whatever it holds
bool                  g_outcomeMoved = false;  // a product row's outcome moved this pass
bool                  g_detailMoved = false;   // only a detail, or a dev row's outcome, moved
bool                  g_redSaid = false;    // the red arm's verdict line is out
bool                  g_redWaitSaid = false;   // and its row's wait, while it is not settled
uint64_t              g_classSet = 0;       // ClassSetVersion when the pass began
uint64_t              g_passClasses = 0;    // ClassUnlistVersion when the pass's caches began
Clock::time_point     g_lastTry;            // when the pass began
double                g_ms = 0;             // the census's measured work
std::string           g_red;                // [dev] fingerprint_census_red: the row whose reference is falsified
// When the next Tick is due, for the runtime thread that posts it: 0 now (a pass or its report runs), a
// steady-clock millisecond while unsettled rows wait, -1 never (idle).
std::atomic<int64_t>  g_dueMs{-1};
std::atomic<bool>     g_posted{false};      // a Tick is queued; no second one is posted
uint64_t              g_lastDrain = 0;      // the game-thread drain the last Tick ran in
// The registration record the last report covered (facts::RecordVersion, RefusalVersion), once a report exists. A
// refusal is reported at once; an acceptance refreshes the file at most once a second (lanes register in bursts).
std::atomic<bool>     g_reportedOnce{false};
std::atomic<uint64_t> g_refusalsReported{0};
std::atomic<uint64_t> g_recordsReported{0};
std::atomic<int64_t>  g_recordReportAtMs{0};
// [dev] fingerprint_capture: written once the first pass has met the classes, from the census's own subjects.
bool                  g_captureDue = false;

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


std::string Hex(uint64_t v) {
    char b[20];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
    return b;
}

// ---- any-class subjects: every function of a name an any-class row names, met as its class loads ----

// A subject: a function of a wanted name, the name it had when met (checked again before it is read, so a
// function that died and an object put at its address since are told apart), and its signature hash once
// read: one function can be the subject of several rows.
struct Subject {
    void*    fn;
    int32_t  index;  // its slot when met: liveness is tested by slot, never by a read through the pointer
    int32_t  name;
    bool     read = false;
    uint64_t sig = 0;
};
// A name watch depends on every function of its name a session loads, so the census meets each class once,
// as it loads: the object index logs every class it lists, and the census reads each one's functions from
// where it stopped (a class still loading is kept and read once it has loaded). Nothing walks the object
// array; a class that loads costs its own functions.
std::unordered_map<std::string, std::vector<Subject>> g_anySubjects;  // lowercase name -> functions, as met
std::unordered_set<std::string>   g_anyWanted;     // the any-class rows' names, lowercase
std::unordered_map<int32_t, bool> g_anyNames;      // a function name's comparison index -> wanted (rendered once)
std::vector<size_t>               g_anyLoading;    // class-log positions met while their class was loading
size_t                            g_anyLogNext = 0;   // the next class-log position to read
size_t                            g_anyGone = 0;      // listed classes gone before they were met (loaded and dropped)
void*                             g_fnClasses[2] = {};  // Function and DelegateFunction, native
bool                              g_anyMet = false;   // this pass has met the classes listed before it began

bool AnyClassesNew() { return g_anyLogNext != OI::ClassLogSize() || !g_anyLoading.empty(); }

void MeetFunctionsOf(void* cls) {
    void* f = *reinterpret_cast<void**>(static_cast<uint8_t*>(cls) + P::off::UStruct_Children);
    for (int guard = 0; f && guard < 4096; ++guard, f = *reinterpret_cast<void**>(static_cast<uint8_t*>(f) + P::off::UField_Next)) {
        void* fc = R::ClassOf(f);
        if (fc != g_fnClasses[0] && fc != g_fnClasses[1]) continue;
        const R::FName& nm = R::NameOf(f);
        if (nm.Number != 0) continue;
        auto it = g_anyNames.find(nm.ComparisonIndex);
        if (it == g_anyNames.end()) {
            const std::string s = Lower(Narrow(R::ToString(R::FName{nm.ComparisonIndex, 0}).c_str()));
            it = g_anyNames.emplace(nm.ComparisonIndex, g_anyWanted.count(s) != 0).first;
        }
        if (it->second)
            g_anySubjects[Lower(Narrow(R::ToString(nm).c_str()))].push_back(Subject{f, R::InternalIndexOf(f), nm.ComparisonIndex});
    }
    // A class declares a few hundred functions at most; one that reaches the guard is read short, and says so once.
    static bool s_saidLong = false;
    if (f && !s_saidLong) {
        s_saidLong = true;
        UE_LOGW("fingerprint-census: class '%ls' has more than 4096 children; the any-class rows read its first 4096",
                R::ToString(R::NameOf(cls)).c_str());
    }
}

// Meet the classes listed since the last call, and the ones that were still loading then, until `deadline`
// (asked every 64 classes). True when every listed class has been met or is still loading.
bool MeetNewClasses(Clock::time_point deadline) {
    if (!g_fnClasses[0]) {
        g_fnClasses[0] = R::FindClass(L"Function");
        g_fnClasses[1] = R::FindClass(L"DelegateFunction");
    }
    if (g_anyWanted.empty())
        for (const RowState& s : g_rows)
            if (s.row->GetMode() == F::Mode::AnyClass) g_anyWanted.insert(Lower(Narrow(s.row->Name())));
    for (size_t k = 0; k < g_anyLoading.size();) {
        bool loaded = false;
        void* cls = OI::ClassLogAt(g_anyLoading[k], &loaded);
        if (cls && !loaded) {
            ++k;
            continue;
        }
        if (cls) MeetFunctionsOf(cls);
        else ++g_anyGone;
        g_anyLoading[k] = g_anyLoading.back();
        g_anyLoading.pop_back();
    }
    const size_t n = OI::ClassLogSize();
    for (size_t i = 1; g_anyLogNext < n; ++g_anyLogNext, ++i) {
        if ((i & 63) == 0 && Clock::now() >= deadline) return false;
        bool loaded = false;
        void* cls = OI::ClassLogAt(g_anyLogNext, &loaded);
        if (!cls) {
            ++g_anyGone;  // gone since it was listed, and its functions with it
            continue;
        }
        if (!loaded) {
            g_anyLoading.push_back(g_anyLogNext);
            continue;
        }
        MeetFunctionsOf(cls);
    }
    return true;
}

// Start meeting every class again, from the log's start (a new census pass of every row).
void ResetAnySubjects() {
    g_anySubjects.clear();
    g_anyLoading.clear();
    g_anyLogNext = 0;
    g_anyGone = 0;
}

// The maps hold every function name met: kept while an any-class row is still checked as classes load.
void ReleaseAnySubjects() {
    std::unordered_map<std::string, std::vector<Subject>>().swap(g_anySubjects);
    std::unordered_set<std::string>().swap(g_anyWanted);
    std::unordered_map<int32_t, bool>().swap(g_anyNames);
    std::vector<size_t>().swap(g_anyLoading);
    g_anyLogNext = 0;
    g_anyGone = 0;
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
    const auto it = g_anySubjects.find(Lower(Narrow(s.row->Name())));
    if (it != g_anySubjects.end()) {
        for (; s.anyNext < it->second.size(); ++s.anyNext) {
            Subject& sub = it->second[s.anyNext];
            if (!sub.read) {
                if (!R::IsLiveByIndex(sub.fn, sub.index) || R::NameOf(sub.fn).ComparisonIndex != sub.name) continue;  // gone, unread
                const FP::Result r = FP::SignatureOf(sub.fn);
                sub.sig = r.ok ? r.sigHash : 0;  // a signature that does not read is not a known one
                sub.read = true;
            }
            ++s.anyChecked;
            bool known = false;
            for (int k = 0; k < ref->sigCount && !known; ++k) known = kRefAnySigs[ref->firstSig + k] == sub.sig;
            if (!known) ++s.anyOutside;
        }
    }
    const std::string of = " (" + std::to_string(ref->classes) + " declaring class(es) on " + kRefBuild + ")";
    if (s.anyOutside) {
        s.outcome = Outcome::AnySigChanged;
        s.detail = std::to_string(s.anyOutside) + " of " + std::to_string(s.anyChecked) +
                   " signature(s) read outside the reference's " + std::to_string(ref->sigCount) + of;
        s.settled = true;
    } else if (!s.anyChecked) {
        // Nothing to judge: no declaring class has loaded, or the name has no class on this build any more.
        s.outcome = Outcome::Unchecked;
        s.detail = "no function of the name has loaded" + of;
    } else {
        // Never settled: a class that loads later is checked too.
        s.outcome = Outcome::AnyHolds;
        s.detail = std::to_string(s.anyChecked) + " read as their classes loaded, every signature inside the "
                   "reference's " + std::to_string(ref->sigCount) + of;
    }
}

void CheckClass(RowState& s) {
    const F::Function& row = *s.row;
    void* cls = nullptr;
    std::wstring witness;
    if (row.Class()) {
        cls = OI::ClassByName(row.Class());
    } else {
        // A run-time class row is keyed on the class its site resolved it on.
        witness = WitnessName(row);
        if (witness.empty()) {
            s.outcome = Outcome::Unchecked;
            s.detail = "the class is known only at run time, and its site has not resolved it yet";
            return;  // re-tried: the site may resolve it later in the session
        }
        cls = OI::ClassByName(witness.c_str());
        s.ref = FindRef(witness.c_str(), row.Name());
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
        s.detail = "the reference has no entry for " + (witness.empty() ? std::string("this class") : Narrow(witness.c_str()));
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
        CheckAny(s);  // Slice has met the new classes before any any-class row is checked
    } else {
        CheckClass(s);
    }
    if (s.outcome != was && !s.dev) {
        g_outcomeMoved = true;
    } else if (s.outcome != was || s.detail != wasDetail) {
        g_detailMoved = true;
    }
}

// ---- the pass, its report ----

// The report file every time; the log lines (the summary and the costliest rows) only when `log`: an outcome
// moved, or the pass is the first. A problem row and the red arm's verdict are said once, whichever report
// meets them first.
void Report(bool log) {
    ue_wrap::ScopedWalkTimer walk("fingerprint-census report");
    g_refusalsReported.store(F::RefusalVersion(), std::memory_order_release);
    g_recordsReported.store(F::RecordVersion(), std::memory_order_release);
    g_reportedOnce.store(true, std::memory_order_release);
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
        const bool problem = IsProblem(s.outcome);
        if (!s.dev && (problem || refused)) {
            ++problems;
            CR::Failure("fingerprint-census", CR::Severity::Important);
            if ((problem && !s.warned) || (refused && !s.warnedRefused)) {
                s.warned = s.warned || problem;
                s.warnedRefused = s.warnedRefused || refused;
                UE_LOGW("fingerprint-census: %s %s (feature %s)%s%s", Name(s.outcome), s.key.c_str(), row.Feature(),
                        refused ? " -- its registration was REFUSED" : "",
                        s.detail.empty() ? "" : (" -- " + s.detail).c_str());
            }
        }
        if (!g_redSaid && !g_red.empty() && g_red == s.key) {
            if (s.settled) {
                g_redSaid = true;
                if (s.outcome == Outcome::BodyChanged)
                    UE_LOGW("fingerprint-census: RED ARM %s reported body changed", g_red.c_str());
                else
                    UE_LOGE("fingerprint-census: RED ARM FAILED %s (%s)", g_red.c_str(), Name(s.outcome));
            } else if (!g_redWaitSaid) {
                g_redWaitSaid = true;  // a run waiting on the verdict reads why it has none yet
                UE_LOGI("fingerprint-census: RED ARM %s waits for its row: %s -- %s", g_red.c_str(), Name(s.outcome),
                        s.detail.c_str());
            }
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
    // What the any-class rows have met: an unchecked one reads against it (a class can load with the world and
    // be dropped before the census meets it, so the answer can differ from run to run).
    std::string met;
    if (!g_anyWanted.empty()) {
        std::string loading;
        for (size_t k = 0; k < g_anyLoading.size() && k < 5; ++k) {
            bool loaded = false;
            if (void* cls = OI::ClassLogAt(g_anyLoading[k], &loaded))
                loading += (loading.empty() ? " (" : ", ") + Narrow(R::ToString(R::NameOf(cls)).c_str());
        }
        if (!loading.empty()) loading += ")";
        char b[256];
        std::snprintf(b, sizeof b,
                      "fingerprint-census: %zu of %zu listed class(es) met for the any-class rows, %zu gone before they "
                      "were met, %zu still loading",
                      g_anyLogNext - g_anyLoading.size() - g_anyGone, OI::ClassLogSize(), g_anyGone, g_anyLoading.size());
        met = b + loading;
        CR::Line("fingerprint-census", met);
    }
    char sum[256];
    std::snprintf(sum, sizeof sum,
                  "fingerprint-census: %d of %d class rows match, %d of %d any-class rows hold, %d unchecked, %d "
                  "problem row(s), %d dev row(s) apart (reference %s, %.1f ms)",
                  matched, classRows, anyHold, anyRows, unchecked, problems, devRows, kRefBuild, g_ms);
    CR::Line("fingerprint-census", sum);
    CR::Write(log);
    if (!log) return;
    // Where the census's time went: the five costliest rows, resolve and fingerprint apart.
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
    if (!met.empty()) UE_LOGI("%s", met.c_str());
    if (problems) {
        UE_LOGW("%s -- DEGRADED: the game differs from the build this mod was made for", sum);
    } else {
        UE_LOGI("%s", sum);
    }
}

// The pass is done: wait for the rows that are not settled, if any. Run before the report, so a report that
// faults leaves the census waiting or idle, never posting a Tick every loop.
void Finish() {
    g_firstPass = false;
    g_outcomeMoved = false;
    g_detailMoved = false;
    g_waiting = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& s) { return !s.settled; });
    const bool anyWaits = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& s) {
        return !s.settled && s.row->GetMode() == F::Mode::AnyClass;
    });
    if (!anyWaits) ReleaseAnySubjects();
    g_dueMs.store(g_waiting ? NowMs() + 1000 : -1, std::memory_order_release);
}

void BeginPassState() {
    FP::BeginPass();
    g_passClasses = OI::ClassUnlistVersion();
    g_anyMet = !AnyClassesNew();
    g_classSet = OI::ClassSetVersion();
    g_lastTry = Clock::now();
    g_next = 0;
    g_running = true;
    g_dueMs.store(0, std::memory_order_release);
}

void Slice() {
    // The object index applies the engine's create and delete events in batches; while some wait, a class it
    // still lists may already be gone. The census reads nothing until the index has caught up.
    if (OI::Backlog() != 0) return;
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
    if (anyPending && !g_anyMet) {
        if (!MeetNewClasses(t0 + budget)) {
            g_ms += std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
            return;  // the classes go on next slice; the rows wait for them
        }
        g_anyMet = true;
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
    if (g_firstPass || g_outcomeMoved) {
        g_reportDue = ReportKind::Full;  // in a Tick of its own, outside the rows' budget
    } else if (g_detailMoved) {
        g_reportDue = ReportKind::File;
    } else {
        Finish();  // a re-try that moved nothing says nothing
    }
}

void Tick() {
    if (g_running) {
        Slice();
        return;
    }
    if (g_reportDue != ReportKind::None) {
        const bool log = g_reportDue == ReportKind::Full;
        g_reportDue = ReportKind::None;
        Finish();
        if (g_captureDue) {
            g_captureDue = false;
            Capture();
        }
        Report(log);
        return;
    }
    // The registration record moved since the last report: a refusal is named at once, an acceptance refreshes
    // the file (its "not registered" marks), at most once a second.
    if (g_reportedOnce.load(std::memory_order_acquire)) {
        if (F::RefusalVersion() != g_refusalsReported.load(std::memory_order_acquire)) {
            Report(true);
            return;
        }
        if (F::RecordVersion() != g_recordsReported.load(std::memory_order_acquire) &&
            NowMs() >= g_recordReportAtMs.load(std::memory_order_acquire)) {
            g_recordReportAtMs.store(NowMs() + 1000, std::memory_order_release);
            Report(false);
            return;
        }
    }
    if (!g_waiting) return;
    const bool classRowWaits = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& s) {
        return !s.settled && s.row->GetMode() != F::Mode::AnyClass;
    });
    const bool anyRowWaits = std::any_of(g_rows.begin(), g_rows.end(), [](const RowState& s) {
        return !s.settled && s.row->GetMode() == F::Mode::AnyClass;
    });
    const bool classSetMoved =
        OI::ClassSetVersion() != g_classSet || Clock::now() - g_lastTry >= std::chrono::seconds(5);
    if (!(classRowWaits && classSetMoved) && !(anyRowWaits && AnyClassesNew())) {
        g_dueMs.store(NowMs() + 1000, std::memory_order_release);  // ask again in a second
        return;
    }
    // A waiting class row's class may have loaded, or a class has loaded whose functions an any-class row checks.
    BeginPassState();
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

namespace detail {

std::string Key(const F::Function& row) {
    return Lower((row.Class() ? Narrow(row.Class()) : std::string("*")) + "." + Narrow(row.Name()));
}

std::wstring WitnessName(const F::Function& row) {
    const uint64_t w = row.WitnessClass();
    if (!w) return {};
    return R::ToString(R::FName{static_cast<int32_t>(w >> 32), static_cast<int32_t>(w & 0xffffffffu)});
}

void ForEachLiveSubject(const std::function<void(void*)>& fn) {
    for (const auto& [name, subjects] : g_anySubjects)
        for (const Subject& sub : subjects)
            if (R::IsLiveByIndex(sub.fn, sub.index)) fn(sub.fn);
}

}  // namespace detail

void Run() {
    ResetAnySubjects();
    g_captureDue = coop::config::ResolveFlag(::coop::config_registry::rows::fingerprint_capture);
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
    g_redWaitSaid = false;
    CheckRedKey();
    g_ms = 0;
    g_waiting = false;
    g_firstPass = true;
    g_outcomeMoved = false;
    g_detailMoved = false;
    g_reportDue = ReportKind::None;
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
    const bool reported = g_reportedOnce.load(std::memory_order_acquire);
    const bool refused = reported && F::RefusalVersion() != g_refusalsReported.load(std::memory_order_acquire);
    const bool recorded = reported && F::RecordVersion() != g_recordsReported.load(std::memory_order_acquire) &&
                          NowMs() >= g_recordReportAtMs.load(std::memory_order_acquire);
    if (!refused && !recorded && !(due == 0 || (due > 0 && NowMs() >= due))) return;
    bool idle = false;
    if (!g_posted.compare_exchange_strong(idle, true, std::memory_order_acq_rel)) return;
    GT::Post([] {
        // Cleared as the task ends, a faulted one included (the image unwinds on a structured exception).
        struct Clear {
            ~Clear() { g_posted.store(false, std::memory_order_release); }
        } clear;
        // One Tick a drain: a drain runs every task posted while it runs, so a stall ends in one slice, not a
        // run of them back to back (harness/pump.h's composite, the same rule).
        const uint64_t drain = GT::DrainSerial();
        if (drain == g_lastDrain) return;
        g_lastDrain = drain;
        Tick();
    });
}

}  // namespace harness::fingerprint_census
