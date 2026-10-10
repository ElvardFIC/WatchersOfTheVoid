// coop/dev/sat_console_drill.cpp -- see coop/dev/sat_console_drill.h.

#include "coop/dev/sat_console_drill.h"

#include "coop/config/config.h"
#include "coop/interactables/sat_console_sync.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"  // LocalPeerId: the typist is slot 1, an observer is not
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/dish.h"
#include "ue_wrap/desk/sat_console.h"
#include "ue_wrap/devices/serverbox.h"
#include "ue_wrap/engine/engine.h"  // ForceGarbageCollection
#include "ue_wrap/facts/function_row.h"

#include <windows.h>

#include <cmath>
#include <string>
#include <vector>

namespace coop::dev::sat_console_drill {
namespace {

namespace SC = ue_wrap::sat_console;
namespace SU = coop::sat_console_sync;
namespace D  = ue_wrap::dish;
namespace SB = ue_wrap::serverbox;
namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;

constexpr float    kFixture       = 0.4242f; // the host's lowered calibration, a value no dish rests at: 35 s back to full
constexpr float    kFixtureNear   = 0.002f;  // the client knows the fixture dish by its value
constexpr int      kRejoinDishes  = 5;       // rejoin: dishes lowered to nothing, a minute's ramp each
constexpr float    kFull          = 0.999f;
constexpr uint64_t kArmBoundMs    = 60000;   // the fixture crosses by the calibration lane's poll
constexpr uint64_t kCalBoundMs    = 120000;  // a ramp from 0.4242 takes 35 s; a round trip and the lane's poll on top
constexpr float    kZeroed        = 0.01f;   // rejoin, discard: a dish the host zeroed has reached this client
constexpr uint64_t kSettleMs      = 5000;    // discard: after the forced GC, before the verdict
constexpr float    kRise          = 0.02f;   // discard: a zeroed dish that rose more ran on after the discard
constexpr uint64_t kAnswerBoundMs = 30000;
constexpr int      kDishes        = 64;
constexpr uint64_t kCheckEveryMs  = 250;     // a wait reads the log and counts gift boxes (an object walk) 4 a second
constexpr int      kTagCal        = 0x53434443;  // 'SCDC'
UE_FACT_FN(kRowCalibratteDish, "dev.sat_console_drill", L"ui_console_C", L"calibratteDish", Declared);

enum class Step : uint8_t { Arm, CalWait, SauceWait, HashWait, AllWait, Done };
Step     g_step = Step::Arm;
uint64_t g_stepMs = 0;
int      g_session = 1;
int32_t  g_dish = -1;
int      g_boxesBefore = 0;
bool     g_sawBusy = false;  // the terminal read busy while the host's calibration ran
bool     g_watched = false;
int      g_calHere = 0;        // calibratteDish bodies on this peer's own terminal
int      g_calForClients = 0;  // HOST: calibratteDish bodies on a terminal it runs for a client
uint64_t g_nextCheckMs = 0;
bool     g_hostArmed = false;
bool     g_hostDone = false;
int      g_hostBoxesBefore = 0;  // HOST: gift boxes when it armed

// HOST, discard: what the session's end left, read as it ended -- the terminals it kept, and the zeroed dishes'
// precision -- to be judged in the next session, after a forced GC.
struct Left {
    int32_t index;
    float   cal;
};
std::vector<ue_wrap::CachedObjRef> g_discarded;
std::vector<Left> g_leftDishes;
bool     g_discardPending = false;
uint64_t g_gcAtMs = 0;
int      g_rose = 0;  // zeroed dishes that rose while the typist was away, read as the next session opened

const std::string& Mode() {
    static const std::string s = coop::config::ResolveString(::coop::config_registry::rows::sat_console_drill);
    return s;
}
bool Rejoin() { return Mode() == "rejoin"; }
bool Discard() { return Mode() == "discard"; }
bool Leaves() { return Rejoin() || Discard(); }  // the arms whose typist leaves mid-command
bool Enabled() { return Mode() == "run" || Leaves(); }

void Go(Step s) {
    g_step = s;
    g_stepMs = ::GetTickCount64();
}

bool Expired(uint64_t bound) { return ::GetTickCount64() - g_stepMs > bound; }

void Fail(const char* why) {
    UE_LOGW("[SAT-DRILL] FAIL in session %d: %s", g_session, why);
    g_step = Step::Done;
}

void Abandon(const char* why) {
    UE_LOGW("[SAT-DRILL] ABANDONED on the client in session %d: %s", g_session, why);
    g_step = Step::Done;
}

sg::Verdict OnCalPre(const sg::Call& c) {
    if (c.object == SC::LocalTerminal()) ++g_calHere;
    else ++g_calForClients;
    return sg::Verdict::Run;
}

void EnsureWatched() {
    if (g_watched) return;
    g_watched = true;
    sg::WatchName(kRowCalibratteDish, kTagCal, OnCalPre, nullptr);
}

std::wstring Log(void* term) {
    std::wstring log;
    SC::ReadLog(term, log);
    return log;
}

// Each step starts from a cleared log: the log keeps only its last 3000 characters, so a position in it
// does not survive a long answer. `clr` is the terminal's own, run here and never sent: the control.
bool Type(void* term, const wchar_t* line, Step next) {
    const uint64_t sent = SU::LaneCounts().linesSent;
    SC::RunCommand(term, L"clr");
    if (SU::LaneCounts().linesSent != sent) {
        Fail("clr, the terminal's own command, was sent to the host");
        return false;
    }
    SC::RunCommand(term, line);
    UE_LOGI("[SAT-DRILL] client: typed '%ls'", line);
    Go(next);
    return true;
}

int GiftBoxes() {
    int n = 0;
    for (void* o : R::FindObjectsByClass(L"prop_container_giftbox_C"))
        if (o && R::IsLive(o) && !R::NameStartsWith(R::NameOf(o), L"Default__")) ++n;
    return n;
}

// The dish the host lowered for the drill, known by its value, or -1 until the calibration lane brings it.
int32_t FixtureDish(float& cal) {
    D::DishCalibration rows[kDishes];
    const int32_t n = D::ReadCalibrations(rows, kDishes);
    for (int32_t i = 0; i < n; ++i) {
        if (std::fabs(rows[i].value - kFixture) <= kFixtureNear) {
            cal = rows[i].value;
            return rows[i].index;
        }
    }
    return -1;
}

float CalibrationOf(int32_t index) {
    D::DishCalibration rows[kDishes];
    const int32_t n = D::ReadCalibrations(rows, kDishes);
    for (int32_t i = 0; i < n; ++i)
        if (rows[i].index == index) return rows[i].value;
    return -1.f;
}

void ClientTick() {
    // The typist is the client in slot 1; an --observer keeping the session alive through a rejoin runs the
    // same rows and only watches.
    if (g_step == Step::Done || coop::players::Registry::Get().LocalPeerId() != 1) return;
    const uint64_t now = ::GetTickCount64();
    if (g_step != Step::Arm && now < g_nextCheckMs) return;
    g_nextCheckMs = now + kCheckEveryMs;
    if (g_calHere > 0) {
        Fail("calibratteDish ran on this client's own terminal -- the calibration was written here");
        return;
    }
    void* term = SC::LocalTerminal();
    switch (g_step) {
    case Step::Arm: {
        if (!coop::net_pump::HasAnnouncedWorldReady() ||
            coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle || !term) {
            g_stepMs = ::GetTickCount64();
            return;
        }
        if (Leaves()) {
            // sd.calall runs for minutes once a dish the host zeroed has reached this client.
            D::DishCalibration rows[kDishes];
            const int32_t n = D::ReadCalibrations(rows, kDishes);
            bool zeroed = false;
            for (int32_t i = 0; i < n && !zeroed; ++i) zeroed = rows[i].value <= kZeroed;
            if (!zeroed) {
                if (Expired(kArmBoundMs)) Abandon("no dish the host zeroed reached this client within 60 s");
                return;
            }
            Type(term, L"sd.calall", Step::AllWait);
            return;
        }
        float cal = 1.f;
        const int32_t d = FixtureDish(cal);
        if (d < 0) {
            if (Expired(kArmBoundMs)) Abandon("the host's fixture dish did not reach this client within 60 s");
            return;
        }
        g_dish = d;
        // The context, as a panel's con() gives it: the dish and its name.
        if (!SC::CallInit(term, D::TechName(d), false, D::DishByIndex(d))) {
            Abandon("the terminal's init did not run");
            return;
        }
        UE_LOGI("[SAT-DRILL] client: the terminal points at dish %d '%ls' at %.2f", d, D::TechName(d).c_str(), cal);
        Type(term, L"sd.cal", Step::CalWait);
        return;
    }
    case Step::CalWait: {
        bool busy = false;
        if (SC::ReadProcessing(term, busy) && busy) g_sawBusy = true;
        const std::wstring log = Log(term);
        const size_t echo = log.find(L"sd.cal");
        const size_t done = log.find(L"Completed");
        // Read at "Completed": the dish's own precision loss writes it again afterwards.
        const float cal = CalibrationOf(g_dish);
        if (echo == std::wstring::npos || done == std::wstring::npos || done < echo || cal < kFull || busy) {
            if (Expired(kCalBoundMs))
                Fail("within 120 s the host's lines did not show the echo then \"Completed\", the dish did not reach "
                     "full precision here, or the terminal stayed busy");
            return;
        }
        if (!g_sawBusy) {
            Fail("the terminal never read busy while the host's calibration ran");
            return;
        }
        UE_LOGI("[SAT-DRILL] client: the host calibrated dish %d -- its echo and \"Completed\" came back, the terminal "
                "busy for the run, %.3f here", g_dish, cal);
        g_boxesBefore = GiftBoxes();
        Type(term, L"sauce.get", Step::SauceWait);
        return;
    }
    case Step::SauceWait: {
        const std::wstring log = Log(term);
        const int boxes = GiftBoxes();
        if (log.find(L"you got mail") == std::wstring::npos || boxes != g_boxesBefore + 1) {
            if (Expired(kAnswerBoundMs))
                Fail("within 30 s sauce.get did not answer from the host, or the gift boxes here did not go up by one");
            return;
        }
        UE_LOGI("[SAT-DRILL] client: sauce.get answered from the host; gift boxes here %d -> %d", g_boxesBefore, boxes);
        Type(term, L"sv.hash", Step::HashWait);
        return;
    }
    case Step::HashWait: {
        const std::wstring log = Log(term);
        std::wstring code;
        D::ReadHashcode(g_dish, code);
        const std::wstring want = code.empty() ? L"No hashcodes found" : code.substr(0, code.find(L'\n'));
        if (log.find(want) == std::wstring::npos) {
            if (Expired(kAnswerBoundMs)) Fail("within 30 s sv.hash did not answer with this dish's code from the host");
            return;
        }
        const SU::Counts c = SU::LaneCounts();
        UE_LOGI("[SAT-DRILL] client DONE in session %d: %llu line(s) ran on the host, calibratteDish never ran here, "
                "dish %d at full precision, one gift box more, sv.hash answered -- PASS", g_session,
                static_cast<unsigned long long>(c.linesSent), g_dish);
        g_step = Step::Done;
        return;
    }
    case Step::AllWait: {
        // The first life sees its own run start and leaves. Rejoin: the second finds the host's terminal still
        // busy with it, is told so, and gets its lines again. Discard: the session ended with the first life, so
        // the second starts a run of its own, as the first did -- the host judges what the discard left.
        bool busy = false;
        SC::ReadProcessing(term, busy);
        const std::wstring log = Log(term);
        const size_t told = log.find(L"The terminal is busy");
        if (told != std::wstring::npos) {
            const size_t more = log.find(L"recision", told);  // a later "Precision" or "Satellite precision" line
            if (more == std::wstring::npos || !busy) {
                if (Expired(kAnswerBoundMs))
                    Fail("back at the terminal, told it is busy, but no line of the running command came, or the "
                         "terminal did not read busy, within 30 s");
                return;
            }
            UE_LOGI("[SAT-DRILL] client DONE in session %d (rejoin): back at a terminal still busy with the command "
                    "left running, told so, and its lines came again -- PASS", g_session);
            g_step = Step::Done;
            return;
        }
        if (busy && log.find(L"Precision") != std::wstring::npos) {
            UE_LOGI("[SAT-DRILL] client: leaving mid-command -- the host's sd.calall is busy and printing");
            g_step = Step::Done;
            return;
        }
        if (Expired(kAnswerBoundMs)) Fail("sd.calall neither started a busy run nor answered busy within 30 s");
        return;
    }
    default:
        return;
    }
}

// The fixture: the first dish whose server works lowered to a value no dish rests at; for the rejoin and the
// discard, the first five lowered to nothing, so an sd.calall outlasts the client's absence. One attempt: a
// world with no such dish abandons the drill rather than search every frame. A discard judged in this session
// arms nothing: its dishes are the evidence.
void HostArm(coop::net::Session* s) {
    if (g_hostArmed || g_hostDone || g_discardPending || !s->AnyWorldReadyPeer()) return;
    if (!D::EnsureResolved() || !SB::EnsureResolved() || !SB::EnsureBreakResolved()) return;
    g_hostArmed = true;
    std::vector<void*> servers;
    SB::ReadServers(servers);
    const int want = Leaves() ? kRejoinDishes : 1;
    const float to = Leaves() ? 0.f : kFixture;
    int lowered = 0;
    for (int32_t i = 0; i < D::Count() && lowered < want; ++i) {
        const std::wstring name = D::TechName(i);
        for (void* box : servers) {
            if (!box || SB::ReadName(box) != name || SB::ReadIsBroken(box)) continue;
            D::WriteCalibration(i, to);
            ++lowered;
            UE_LOGI("[SAT-DRILL] host: dish %d '%ls' lowered to %.2f for the client to calibrate", i, name.c_str(), to);
            break;
        }
    }
    if (lowered == 0) {
        UE_LOGW("[SAT-DRILL] ABANDONED on the host in session %d: no dish whose server works", g_session);
        g_hostDone = true;
        return;
    }
    g_hostBoxesBefore = GiftBoxes();
}

// Discard, the next session. Its first tick, before the returning typist can type a run of its own, reads the
// dishes the discarded terminal was calibrating: a run that went on through the absence, a minute or more, would
// have raised them. Then a GC is forced, and the discarded terminals must be gone after it.
void HostJudgeDiscard() {
    const uint64_t now = ::GetTickCount64();
    if (g_gcAtMs == 0) {
        D::DishCalibration rows[kDishes];
        const int32_t n = D::ReadCalibrations(rows, kDishes);
        g_rose = 0;
        for (const Left& l : g_leftDishes)
            for (int32_t i = 0; i < n; ++i)
                if (rows[i].index == l.index && rows[i].value > l.cal + kRise) ++g_rose;
        ue_wrap::engine::ForceGarbageCollection();
        g_gcAtMs = now;
        return;
    }
    if (now - g_gcAtMs < kSettleMs) return;
    g_discardPending = false;
    g_hostDone = true;
    int alive = 0;
    for (const ue_wrap::CachedObjRef& r : g_discarded)
        if (r.Get()) ++alive;
    const int rose = g_rose;
    if (g_discarded.empty() || g_leftDishes.empty() || alive > 0 || rose > 0) {
        UE_LOGW("[SAT-DRILL] FAIL in session %d (discard): %zu terminal(s) discarded, %d still alive after a GC; %zu "
                "dish(es) left mid-ramp, %d rose since", g_session, g_discarded.size(), alive, g_leftDishes.size(), rose);
        return;
    }
    UE_LOGI("[SAT-DRILL] host DONE in session %d (discard): %zu terminal(s) discarded as the session ended, gone after "
            "a GC, and none of %zu dish(es) left mid-ramp rose since", g_session, g_discarded.size(),
            g_leftDishes.size());
}

void HostTick(coop::net::Session* s) {
    if (g_discardPending) {
        HostJudgeDiscard();
        return;
    }
    HostArm(s);
    if (g_hostDone || g_calForClients < 1) return;
    const SU::Counts c = SU::LaneCounts();
    if (Discard()) return;  // judged in the next session, from what this one leaves
    if (Rejoin()) {
        // The returning typist is bound to its terminal as its world is ready, still busy with sd.calall.
        if (c.busyRebinds < 1) return;
        g_hostDone = true;
        UE_LOGI("[SAT-DRILL] host DONE in session %d (rejoin): the returning typist was bound to its terminal while it "
                "still ran, %llu line(s) printed back", g_session, static_cast<unsigned long long>(c.linesReturned));
        return;
    }
    if (c.linesRun < 3) return;
    g_hostDone = true;
    const int boxes = GiftBoxes();
    if (boxes != g_hostBoxesBefore + 1) {
        UE_LOGW("[SAT-DRILL] FAIL in session %d: the client's sauce.get left the host with %d gift box(es), not %d",
                g_session, boxes, g_hostBoxesBefore + 1);
        return;
    }
    UE_LOGI("[SAT-DRILL] host DONE in session %d: ran %llu line(s) for the client on its terminal, calibratteDish %d "
            "time(s) there and %d on its own, %llu line(s) printed back, gift boxes %d -> %d", g_session,
            static_cast<unsigned long long>(c.linesRun), g_calForClients, g_calHere,
            static_cast<unsigned long long>(c.linesReturned), g_hostBoxesBefore, boxes);
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (!Enabled() || !s || !s->connected()) return;
    EnsureWatched();
    if (s->role() == coop::net::Role::Host) HostTick(s);
    else ClientTick();
}

void OnDisconnect() {
    if (!Enabled()) return;
    // Discard, the host: this runs as the session ends, before the lane discards its terminals.
    if (Discard() && g_hostArmed && !g_discardPending) {
        void* kept[16];
        const size_t n = SU::KeptTerminals(kept, 16);
        g_discarded.clear();
        for (size_t i = 0; i < n; ++i) {
            ue_wrap::CachedObjRef r;
            r.Set(kept[i]);
            g_discarded.push_back(r);
        }
        D::DishCalibration rows[kDishes];
        const int32_t m = D::ReadCalibrations(rows, kDishes);
        g_leftDishes.clear();
        for (int32_t i = 0; i < m; ++i)
            if (rows[i].value < 0.99f) g_leftDishes.push_back({rows[i].index, rows[i].value});
        g_discardPending = true;
        g_gcAtMs = 0;
        UE_LOGI("[SAT-DRILL] host: the session ends with %zu terminal(s) kept and %zu dish(es) below full precision",
                n, g_leftDishes.size());
    }
    ++g_session;
    g_step = Step::Arm;
    g_stepMs = 0;
    g_dish = -1;
    g_sawBusy = false;
    g_calHere = g_calForClients = 0;
    g_hostArmed = g_hostDone = false;
    g_hostBoxesBefore = 0;
    g_nextCheckMs = 0;
}

}  // namespace coop::dev::sat_console_drill
