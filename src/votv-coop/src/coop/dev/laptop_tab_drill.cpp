// coop/dev/laptop_tab_drill.cpp -- see coop/dev/laptop_tab_drill.h.

#include "coop/dev/laptop_tab_drill.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/director/director.h"
#include "coop/dev/director/routes.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/interactables/laptop_tab.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/device_screen.h"
#include "ue_wrap/devices/appliance.h"  // CallAction: the player's use, dispatched
#include "ue_wrap/devices/laptop.h"
#include "ue_wrap/devices/laptop_widget.h"
#include "ue_wrap/engine/engine_component.h"  // GetComponentLocation
#include "ue_wrap/engine/umg_build.h"  // IndexOfChild: where the sub-game page sits in the switcher
#include "ue_wrap/facts/function_row.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace coop::dev::laptop_tab_drill {
namespace {

namespace R  = ue_wrap::reflection;
namespace D  = coop::director;
namespace DO = coop::device_occupancy;
namespace DS = ue_wrap::device_screen;
namespace L  = ue_wrap::laptop;
namespace LT = coop::laptop_tab;
namespace UL = ue_wrap::laptop_widget;
namespace sg = ue_wrap::script_gate;
using ue_wrap::FVector;

// The viewer's createRC counter (tab-norc): a Declared POST watch, so a body our drill or the game runs is counted.
UE_FACT_FN(kRowCreateRc, "dev.laptop_tab_drill", L"ui_laptop_C", L"createRC", Declared);
constexpr int kTagCreateRc = 0x4C544352;  // 'LTCR'

// The two tab buttons that write the switcher without switcherSwitched.
constexpr const wchar_t* kFloppyClick =
    L"BndEvt__ui_laptop_button_tab_floppy_K2Node_ComponentBoundEvent_54_OnButtonClickedEvent__DelegateSignature";
constexpr const wchar_t* kSubgameClick =
    L"BndEvt__ui_laptop_Button_305_K2Node_ComponentBoundEvent_57_OnButtonClickedEvent__DelegateSignature";
constexpr int kFloppyTab = 10;
constexpr int kCamerasTab = 5;
// In a sequence, a step that is not a plain tab number: the sub-game button, whose tab is where canvas_subgame sits in
// the switcher, and a raw report past the switcher's last tab, which the drill sends itself.
constexpr int kSubgameMark = 100;
constexpr int kRangeMark   = 101;
constexpr int kRangeTab    = 255;

constexpr const wchar_t* kClaim = L"laptop";
constexpr uint8_t kEnterAction = 4;

constexpr uint64_t kFindBoundMs   = 120000;
constexpr uint64_t kHeldBoundMs   = 20000;  // enter to a confirmed claim
constexpr float    kRingCm        = 60.f;
constexpr float    kReachCm       = 60.f;
constexpr int      kStandpoints   = 6;
constexpr int      kFirstWalkDeadlineS = 400;
constexpr int      kNextWalkDeadlineS  = 120;
constexpr int      kMaxSeq        = 3;
constexpr int      kForgedTab     = 4;
constexpr int      kJoinTab       = 4;
constexpr uint64_t kStatesBound   = 20;  // states received without the final tab: the viewer's failure, by count

enum class Arm : uint8_t {
    Off, Tab, TabHost, TabHandover, TabNorc, TabForged, TabSlot, TabJoin, TabSubgame, TabRange, TabRelay
};
Arm ArmOf() {
    static const Arm arm = [] {
        const std::string& v = coop::config::ResolveString(::coop::config_registry::rows::laptop_tab_drill);
        return v == "tab" ? Arm::Tab : v == "tab-host" ? Arm::TabHost : v == "tab-handover" ? Arm::TabHandover
             : v == "tab-norc" ? Arm::TabNorc : v == "tab-forged" ? Arm::TabForged : v == "tab-slot" ? Arm::TabSlot
             : v == "tab-join" ? Arm::TabJoin : v == "tab-subgame" ? Arm::TabSubgame
             : v == "tab-range" ? Arm::TabRange : v == "tab-relay" ? Arm::TabRelay : Arm::Off;
    }();
    return arm;
}
const char* ArmName() {
    switch (ArmOf()) {
    case Arm::Tab:         return "tab";
    case Arm::TabHost:     return "tab-host";
    case Arm::TabHandover: return "tab-handover";
    case Arm::TabNorc:     return "tab-norc";
    case Arm::TabForged:   return "tab-forged";
    case Arm::TabSlot:     return "tab-slot";
    case Arm::TabJoin:     return "tab-join";
    case Arm::TabSubgame:  return "tab-subgame";
    case Arm::TabRange:    return "tab-range";
    case Arm::TabRelay:    return "tab-relay";
    default:               return "off";
    }
}

uint64_t NowMs() { return ::GetTickCount64(); }

// What the arm judges against, taken at its start: the module's counts only grow, and a seed or a refusal that came
// before the judging peer got to its first step must still count as before the arm.
constexpr LT::Cause kCauses[] = {LT::Cause::NotHolder, LT::Cause::BadIndex, LT::Cause::BadSlot, LT::Cause::NoWidget};
bool     g_armStarted = false;
uint64_t g_baseReceived = 0;
uint64_t g_baseRefused[4] = {};
void ArmBegin() {
    g_baseReceived = LT::ReceivedCount();
    for (LT::Cause c : kCauses) g_baseRefused[static_cast<int>(c)] = LT::RefusedCount(c);
    g_armStarted = true;
}
uint64_t ReceivedSince() { return LT::ReceivedCount() - g_baseReceived; }
uint64_t RefusedSince(LT::Cause c) { return LT::RefusedCount(c) - g_baseRefused[static_cast<int>(c)]; }

bool g_done = false;  // this peer's part of the arm has ended, its verdict said
// What the judging peer says if the session ends before its verdict; empty on a peer that judges nothing.
std::string g_endWhy;
void Pass(const char* what) {
    UE_LOGI("[LAPTOP-TAB-DRILL] %s PASS %s", ArmName(), what);
    g_done = true;
}
void Fail(const char* what) {
    UE_LOGW("[LAPTOP-TAB-DRILL] %s FAIL %s", ArmName(), what);
    g_done = true;
}
void Abandon(const char* why) {
    UE_LOGW("[LAPTOP-TAB-DRILL] ABANDONED %s: %s", ArmName(), why);
    g_done = true;
}

uint8_t LocalSlot() { return coop::players::Registry::Get().LocalPeerId(); }

bool OwnJoinOver() {
    return coop::net_pump::HasAnnouncedWorldReady() &&
           coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle;
}

bool ReadOwnTab(int& idx) { return UL::ReadTab(UL::Widget(), idx); }

// The tab the sub-game button shows: where the switcher holds canvas_subgame, asked of the switcher. -1 when the widget,
// its switcher, the page or the page's place in the switcher does not resolve.
int SubgameTab() {
    void* const widget = UL::Widget();
    if (!widget) return -1;
    void* const cls = R::ClassOf(widget);
    const int32_t swOff = R::FindPropertyOffset(cls, L"switcher");
    const int32_t pgOff = R::FindPropertyOffset(cls, L"canvas_subgame");
    if (swOff < 0 || pgOff < 0) return -1;
    void* const sw = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(widget) + swOff);
    void* const pg = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(widget) + pgOff);
    if (!sw || !pg || !R::IsLive(sw) || !R::IsLive(pg)) return -1;
    return ue_wrap::umg::IndexOfChild(sw, pg);
}

// A sequence entry as the tab number it leaves the widget on; -1 when it does not resolve.
int ResolveTab(int mark) { return mark == kSubgameMark ? SubgameTab() : mark; }

// ---- the createRC counter (tab-norc) ---------------------------------------------------------------------

uint32_t g_createRc = 0;
bool     g_rcRegistered = false;
uint64_t g_rcAskedMs = 0;
void OnCreateRcPost(const sg::Call&) { ++g_createRc; }

// True once the watch is live on this peer; registers it on the first ask. A watch the gate refused, or never made
// live, ends the arm: there would be nothing to count with.
bool CreateRcWatchLive() {
    if (g_rcAskedMs == 0) g_rcAskedMs = NowMs();
    if (!g_rcRegistered) g_rcRegistered = sg::WatchName(kRowCreateRc, kTagCreateRc, nullptr, &OnCreateRcPost);
    if (g_rcRegistered && sg::NameWatchLive(kRowCreateRc, kTagCreateRc)) return true;
    if (NowMs() - g_rcAskedMs > kFindBoundMs) Abandon("the createRC watch never went live on this peer");
    return false;
}

// ---- the reader: a viewer that judges the FINAL tab of a holder's run -------------------------------------
// The tabs between may coalesce under the lane's pace, so only the last is demanded: it passes when a state carrying it
// has arrived and the widget shows it, and fails after kStatesBound states without it, or when the session ends.

struct Reader {
    bool     active = false;
    int      finalTab = -1;
    uint64_t seen = 0;
};
Reader g_reader;

enum class ReadState : uint8_t { Waiting, Reached, Failed };

// False, the arm abandoned, when the final tab does not resolve on this peer.
bool ReaderStart(int finalMark) {
    const int tab = ResolveTab(finalMark);
    if (tab < 0) { Abandon("the final tab does not resolve on this peer's widget"); return false; }
    g_reader = Reader{};
    g_reader.active = true;
    g_reader.finalTab = tab;
    char end[128];
    std::snprintf(end, sizeof end, "the session ended before a state carrying tab %d reached the widget", tab);
    g_endWhy = end;
    return true;
}

// One look at the arrivals since the arm began. `why` says what failed.
ReadState ReaderStep(char* why, size_t whySize) {
    const uint64_t got = ReceivedSince();
    if (got == 0) return ReadState::Waiting;
    if (got != g_reader.seen) {
        g_reader.seen = got;
        UE_LOGI("laptop_tab_drill: %u state(s) received, the last carries tab %d", static_cast<unsigned>(got),
                LT::LastReceivedIndex());
    }
    if (LT::LastReceivedIndex() == g_reader.finalTab) {
        int idx = -1;
        if (ReadOwnTab(idx) && idx == g_reader.finalTab) {
            UE_LOGI("laptop_tab_drill: this peer's widget reads tab %d", idx);
            return ReadState::Reached;
        }
        const unsigned refused = static_cast<unsigned>(RefusedSince(LT::Cause::NoWidget) +
                                                       RefusedSince(LT::Cause::BadIndex));
        if (refused > 0) {
            std::snprintf(why, whySize, "the state carrying tab %d arrived and was refused (%u refusal(s) for no "
                          "widget or a bad index)", g_reader.finalTab, refused);
            return ReadState::Failed;
        }
        return ReadState::Waiting;  // an owed report is written when its token pays
    }
    if (got >= kStatesBound) {
        std::snprintf(why, whySize, "%u states arrived and none carried tab %d (the last carried %d)",
                      static_cast<unsigned>(got), g_reader.finalTab, LT::LastReceivedIndex());
        return ReadState::Failed;
    }
    return ReadState::Waiting;
}

// ---- the holder: walks to the PC, enters it, and switches tabs ---------------------------------------------

enum class HStep : uint8_t { Off, Find, Walk, Enter, Held, Act, Done };
struct Holder {
    HStep    step = HStep::Off;
    int      seq[kMaxSeq] = {};
    size_t   n = 0;
    size_t   next = 0;
    int      entryTab = -1;   // a tab the widget must read on at the confirmed claim (the shared tab), or -1
    uint64_t stepMs = 0;
    void*    laptop = nullptr;
    void*    box = nullptr;
    std::vector<FVector> ring;
    size_t   stand = 0;
    std::shared_ptr<D::BackgroundWalk> walk;
};
Holder g_h;
uint64_t g_heldReceived = 0;  // the arrivals at the confirmed claim: what came after was sent while this peer held it

void HGo(HStep s) { g_h.step = s; g_h.stepMs = NowMs(); }
bool HExpired(uint64_t bound) { return NowMs() - g_h.stepMs > bound; }

void HolderStart(const int* seq, size_t n, int entryTab) {
    g_h = Holder{};
    for (size_t i = 0; i < n && i < kMaxSeq; ++i) g_h.seq[g_h.n++] = seq[i];
    g_h.entryTab = entryTab;
    HGo(HStep::Find);
}

void HolderCancelWalk() {
    if (g_h.walk && g_h.walk->state.load() == 0) D::CancelBackgroundWalk(*g_h.walk);
    g_h.walk.reset();
}

void* BoxOf(void* laptop) {
    const int32_t off = R::FindPropertyOffset(R::ClassOf(laptop), L"Box");
    if (off < 0) return nullptr;
    void* box = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(laptop) + off);
    return (box && R::IsLive(box)) ? box : nullptr;
}

// The laptop's Box holds the player: the volume a use of the laptop's screen is judged against.
bool InBox(void* box, void* me) {
    void* const fn = R::FindDispatchFunctionCached(R::ClassOf(box), L"IsOverlappingActor");
    if (!fn) return false;
    ue_wrap::ParamFrame f(fn);
    if (!f.valid()) return false;
    f.Set<void*>(L"Other", me);
    return ue_wrap::Call(box, f) && f.Get<bool>(L"ReturnValue");
}

// One tab, through the body a player's click runs: the game's own tab call, or a tab button's click handler.
bool DoTab(void* widget, int mark, int tab) {
    if (mark == kSubgameMark) return ue_wrap::component_calls::CallParamlessNamed(widget, kSubgameClick);
    if (tab == kFloppyTab) return ue_wrap::component_calls::CallParamlessNamed(widget, kFloppyClick);
    void* const fn = R::FindDispatchFunctionCached(R::ClassOf(widget), L"switcherSwitched");
    if (!fn) return false;
    ue_wrap::ParamFrame f(fn);
    if (!f.valid()) return false;
    f.Set<int32_t>(L"Index", tab);
    return ue_wrap::Call(widget, f);
}

bool NextStandpoint() {
    HolderCancelWalk();
    while (++g_h.stand < g_h.ring.size()) {
        g_h.walk = D::StartBackgroundWalk(g_h.ring[g_h.stand], kReachCm, kNextWalkDeadlineS);
        if (g_h.walk) return true;
    }
    Abandon("no standpoint about the laptop's Box was reached and inside it");
    return false;
}

void HolderTick(coop::net::Session* s) {
    void* me = coop::players::Registry::Get().Local();
    if (!me) return;
    switch (g_h.step) {
    case HStep::Find: {
        L::EnsureResolved();
        void* laptop = L::Instance();
        void* box = laptop ? BoxOf(laptop) : nullptr;
        if (!box) {
            if (HExpired(kFindBoundMs)) Abandon("this world holds no base laptop with a Box");
            return;
        }
        g_h.laptop = laptop;
        g_h.box = box;
        if (InBox(box, me)) { HGo(HStep::Enter); return; }
        const FVector at = ue_wrap::engine::GetComponentLocation(box);
        g_h.ring = D::ReachableStandpoints(me, at, kRingCm, kStandpoints, kReachCm);
        UE_LOGI("laptop_tab_drill: a route reaches %zu of the %d standpoints about the laptop's Box", g_h.ring.size(),
                kStandpoints);
        if (g_h.ring.empty()) { Abandon("no NavMesh route reaches a point about the laptop's Box"); return; }
        g_h.stand = 0;
        g_h.walk = D::StartBackgroundWalk(g_h.ring[0], kReachCm, kFirstWalkDeadlineS);
        HGo(HStep::Walk);
        return;
    }
    case HStep::Walk: {
        const int state = g_h.walk ? g_h.walk->state.load() : 2;
        if (state == 0) return;
        if (state == 1 && InBox(g_h.box, me)) {
            UE_LOGI("laptop_tab_drill: walked to standpoint %zu, inside the laptop's Box", g_h.stand + 1);
            g_h.walk.reset();
            HGo(HStep::Enter);
            return;
        }
        NextStandpoint();
        return;
    }
    case HStep::Enter:
        if (!ue_wrap::appliance::CallAction(g_h.laptop, me, kEnterAction)) {
            Abandon("the laptop's enter (action 4) did not dispatch");
            return;
        }
        HGo(HStep::Held);
        return;
    case HStep::Held: {
        if (!DO::LocalHolds(kClaim) || DO::HolderOf(kClaim) != LocalSlot()) {
            if (HExpired(kHeldBoundMs))
                Abandon("the PC's claim was not confirmed after the enter (unpowered, a part unusable, or held by "
                        "another peer)");
            return;
        }
        UE_LOGI("laptop_tab_drill: holder READY: the claim on the PC is confirmed for slot %u",
                static_cast<unsigned>(LocalSlot()));
        if (g_h.entryTab >= 0) {
            int idx = -1;
            if (!ReadOwnTab(idx) || idx != g_h.entryTab) {
                char what[128];
                std::snprintf(what, sizeof what, "this peer entered the PC on tab %d, not the shared tab %d", idx,
                              g_h.entryTab);
                Fail(what);
                return;
            }
            UE_LOGI("laptop_tab_drill: holder entered on the shared tab %d", idx);
        }
        g_heldReceived = LT::ReceivedCount();
        HGo(HStep::Act);
        return;
    }
    case HStep::Act: {
        // One step a tick: the holder runs at its own pace, and the viewer judges the last tab, not each.
        const int mark = g_h.seq[g_h.next];
        if (mark == kRangeMark) {
            coop::net::LaptopTabPayload p{};
            p.index = static_cast<uint8_t>(kRangeTab);
            if (!s->SendReliableToSlot(0, coop::net::ReliableKind::LaptopTabReport, &p, sizeof(p))) {
                Abandon("the raw out-of-range report could not be sent");
                return;
            }
            UE_LOGI("laptop_tab_drill: holder sent a raw report for tab %d, past the switcher's last", kRangeTab);
            if (++g_h.next >= g_h.n) HGo(HStep::Done);
            return;
        }
        const int tab = ResolveTab(mark);
        void* widget = UL::Widget();
        int got = -1;
        if (tab < 0) { Abandon("the sub-game page's tab does not resolve on this peer's widget"); return; }
        if (!widget || !DoTab(widget, mark, tab)) { Abandon("the tab call did not dispatch"); return; }
        if (!UL::ReadTab(widget, got) || got != tab) {
            char what[128];
            std::snprintf(what, sizeof what, "the holder's own widget did not take tab %d (it reads %d)", tab, got);
            Abandon(what);
            return;
        }
        UE_LOGI("laptop_tab_drill: holder switched to tab %d", tab);
        if (++g_h.next >= g_h.n) HGo(HStep::Done);
        return;
    }
    default:
        return;
    }
}

bool HolderDone() { return g_h.step == HStep::Done; }

// ---- the arms ----------------------------------------------------------------------------------------------

constexpr int kTab3[]    = {3, kFloppyTab};
constexpr int kTab5[]    = {kCamerasTab};
constexpr int kTab3o[]   = {3};
constexpr int kTab6[]    = {6};
constexpr int kTab4[]    = {kJoinTab};
constexpr int kSubSeq[]  = {3, kSubgameMark};
constexpr int kRangeSeq[] = {kRangeMark};

// Host: reading, then waiting for the PC to be free, then holding it. Client: holding, leaving, reading.
enum class Phase : uint8_t { Start, Holding, Leaving, Reading, Watching };
Phase g_phase = Phase::Start;

int g_baseIdx = -1;

void ReaderVerdict(const char* passWhat) {
    char why[160] = {};
    switch (ReaderStep(why, sizeof why)) {
    case ReadState::Reached: Pass(passWhat); return;
    case ReadState::Failed:  Fail(why); return;
    default:                 return;
    }
}

// The refusal arms (tab-forged, tab-range, tab-slot): the message carrying `marker` is to be refused for `want`, and
// this peer's own tab is to stay where it was. Judged on arrival of the message, not on a clock.
void JudgeRefusal(const char* who, LT::Cause want, int marker, const char* passWhat) {
    if (g_baseIdx < 0) {
        int idx = -1;
        if (!ReadOwnTab(idx)) return;
        g_baseIdx = idx;
        g_endWhy = "the session ended before the refused message arrived and was counted";
        UE_LOGI("laptop_tab_drill: %s READY on tab %d", who, idx);
    }
    if (ReceivedSince() == 0 || LT::LastReceivedIndex() != marker) return;
    int idx = -1;
    if (!ReadOwnTab(idx)) return;
    if (idx != g_baseIdx) {
        char what[160];
        std::snprintf(what, sizeof what, "%s applied the message it had to refuse (tab %d -> %d)", who, g_baseIdx,
                      idx);
        Fail(what);
        return;
    }
    if (RefusedSince(want) > 0) { Pass(passWhat); return; }
    Fail("the message arrived and no refusal of its cause was counted");
}

// A holder that ran its whole sequence says so: the viewer's line is the verdict.
void HolderDoneLine() {
    UE_LOGI("[LAPTOP-TAB-DRILL] %s DONE holder: switched through its tabs", ArmName());
    g_done = true;
}

void HostTick(coop::net::Session* s) {
    if (!s->running()) return;
    void* me = coop::players::Registry::Get().Local();
    if (!me) return;
    const Arm arm = ArmOf();
    switch (arm) {
    case Arm::Tab:
    case Arm::TabNorc:
    case Arm::TabSubgame: {
        if (arm == Arm::TabNorc && !CreateRcWatchLive()) return;
        if (!g_reader.active) {
            if (!s->AnyWorldReadyPeer()) return;
            if (!ReaderStart(arm == Arm::TabNorc ? kCamerasTab : arm == Arm::TabSubgame ? kSubgameMark : kFloppyTab))
                return;
            UE_LOGI("laptop_tab_drill: host READY to read the client's tabs");
            return;
        }
        if (arm != Arm::TabNorc) {
            ReaderVerdict(arm == Arm::Tab ? "the host's widget reached tab 10 (the floppy tab), the client's last"
                                          : "the host's widget reached the sub-game tab, the client's last");
            return;
        }
        char why[160] = {};
        const ReadState r = ReaderStep(why, sizeof why);
        if (r == ReadState::Failed) { Fail(why); return; }
        if (r != ReadState::Reached) return;
        if (g_createRc != 0) {
            char what[128];
            std::snprintf(what, sizeof what, "the viewer's createRC ran %u time(s) for the cameras tab",
                          static_cast<unsigned>(g_createRc));
            Fail(what);
            return;
        }
        Pass("the viewer read the cameras tab (5) and its createRC ran 0 times");
        return;
    }
    case Arm::TabHost:
    case Arm::TabJoin:
        if (g_h.step == HStep::Off) {
            if (arm == Arm::TabHost && !s->AnyWorldReadyPeer()) return;
            if (arm == Arm::TabHost) HolderStart(kTab3, 2, -1); else HolderStart(kTab4, 1, -1);
            return;
        }
        HolderTick(s);
        if (HolderDone()) {
            if (arm == Arm::TabJoin)
                UE_LOGI("[LAPTOP-TAB-DRILL] tab-join host READY on tab %d: a client may join now", kJoinTab);
            HolderDoneLine();
        }
        return;
    case Arm::TabHandover:
        if (g_phase == Phase::Start) {
            if (!s->AnyWorldReadyPeer()) return;
            if (!ReaderStart(3)) return;
            g_phase = Phase::Reading;
            return;
        }
        if (g_phase == Phase::Reading) {
            char why[160] = {};
            const ReadState r = ReaderStep(why, sizeof why);
            if (r == ReadState::Failed) { Fail(why); return; }
            if (r == ReadState::Reached) {
                g_endWhy.clear();  // the host's reading is judged; what follows is the holder's leg
                g_phase = Phase::Leaving;
            }
            return;
        }
        if (g_phase == Phase::Leaving) {
            // The client left the PC when its claim is released; entering before that is a deny, not a handover.
            if (DO::HolderOf(kClaim) != 0xFF) return;
            UE_LOGI("laptop_tab_drill: host read tab 3 and the PC is free; walking to it");
            HolderStart(kTab6, 1, 3);
            g_phase = Phase::Holding;
            return;
        }
        HolderTick(s);
        if (HolderDone()) Pass("the host entered on the shared tab 3 and switched to tab 6");
        return;
    case Arm::TabForged:
        if (!s->AnyWorldReadyPeer()) return;
        JudgeRefusal("host", LT::Cause::NotHolder, kForgedTab,
                     "the host dropped and counted a report from a peer that does not hold the PC, and kept its tab");
        return;
    case Arm::TabRange:
        if (!s->AnyWorldReadyPeer()) return;
        JudgeRefusal("host", LT::Cause::BadIndex, kRangeTab,
                     "the host dropped and counted a holder's report past the switcher's last tab, and kept its tab");
        return;
    case Arm::TabSlot: {
        if (g_phase != Phase::Start || !s->AnyWorldReadyPeer()) return;
        for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers); ++slot) {
            if (!s->IsSlotWorldReady(slot)) continue;
            coop::net::LaptopTabPayload p{};
            p.index = static_cast<uint8_t>(kForgedTab);
            // The header's origin is the host's to stamp: the late-join replay and the relay use it. Slot 2 is a peer.
            if (!s->SendReliableToSlot(slot, coop::net::ReliableKind::LaptopTabState, &p, sizeof(p), 2)) {
                Abandon("the host could not send the stamped state");
                return;
            }
            UE_LOGI("[LAPTOP-TAB-DRILL] tab-slot DONE host: sent a LaptopTabState for tab %d stamped slot 2 to slot %d",
                    kForgedTab, slot);
            g_phase = Phase::Watching;
            g_done = true;
            return;
        }
        return;
    }
    default:
        return;
    }
}

void ClientTick(coop::net::Session* s) {
    if (ArmOf() == Arm::TabNorc && !CreateRcWatchLive()) return;
    void* me = coop::players::Registry::Get().Local();
    if (!me) return;
    const Arm arm = ArmOf();
    // The stamped state reaches a joiner as soon as its world is ready, which is before its own join is over.
    if (arm == Arm::TabSlot) {
        JudgeRefusal("client", LT::Cause::BadSlot, kForgedTab,
                     "the client dropped and counted a state stamped with another sender's slot, and kept its tab");
        return;
    }
    if (!OwnJoinOver()) return;
    switch (arm) {
    case Arm::Tab:
    case Arm::TabNorc:
    case Arm::TabSubgame:
    case Arm::TabRange:
        if (g_h.step == HStep::Off) {
            if (arm == Arm::TabNorc) HolderStart(kTab5, 1, -1);
            else if (arm == Arm::TabSubgame) HolderStart(kSubSeq, 2, -1);
            else if (arm == Arm::TabRange) HolderStart(kRangeSeq, 1, -1);
            else HolderStart(kTab3, 2, -1);
            return;
        }
        HolderTick(s);
        if (!HolderDone()) return;
        if (arm == Arm::TabNorc) {
            // The control: the holder's own switch ran createRC, so a watch that counted nothing could judge nothing.
            if (g_createRc == 0) { Fail("the holder's own createRC never ran, so the watch cannot judge the viewer"); return; }
            UE_LOGI("[LAPTOP-TAB-DRILL] tab-norc control: the holder's createRC ran %u time(s)",
                    static_cast<unsigned>(g_createRc));
        }
        HolderDoneLine();
        return;
    case Arm::TabHost:
    case Arm::TabJoin:
        if (!g_reader.active) {
            if (!ReaderStart(arm == Arm::TabHost ? kFloppyTab : kJoinTab)) return;
            UE_LOGI("laptop_tab_drill: client READY to read the host's tab");
            return;
        }
        ReaderVerdict(arm == Arm::TabHost ? "the client's widget reached tab 10 (the floppy tab), the host's last"
                                          : "the joiner's widget read the host's tab 4");
        return;
    case Arm::TabHandover:
        if (g_phase == Phase::Start) {
            if (g_h.step == HStep::Off) { HolderStart(kTab3o, 1, -1); return; }
            HolderTick(s);
            if (!HolderDone()) return;
            // Leave at once: the host reads tab 3 from the report, which the lane carries ahead of the claim's release.
            DS::ForceExitInterface(me);
            g_phase = Phase::Leaving;
            return;
        }
        if (g_phase == Phase::Leaving) {
            if (DO::LocalHolds(kClaim) || DO::HolderOf(kClaim) != 0xFF) return;
            UE_LOGI("laptop_tab_drill: client left the PC; reading the host's next tab");
            if (!ReaderStart(6)) return;
            g_phase = Phase::Reading;
            return;
        }
        ReaderVerdict("the client, a viewer again, reached the host's tab 6");
        return;
    case Arm::TabForged: {
        if (DO::LocalHolds(kClaim)) { Abandon("this client holds the PC, so its report would not be forged"); return; }
        coop::net::LaptopTabPayload p{};
        p.index = static_cast<uint8_t>(kForgedTab);
        if (!s->SendReliableToSlot(0, coop::net::ReliableKind::LaptopTabReport, &p, sizeof(p))) {
            Abandon("the forged report could not be sent");
            return;
        }
        UE_LOGI("[LAPTOP-TAB-DRILL] tab-forged DONE client: sent a LaptopTabReport for tab %d holding nothing",
                kForgedTab);
        g_done = true;
        return;
    }
    case Arm::TabRelay:
        // Three peers (the rig's --observer): the first client holds, the second reads, and the host relays between
        // them. The holder's own count of states is the proof that the relay left the source out.
        if (LocalSlot() != 1) {
            if (!g_reader.active) {
                if (!ReaderStart(kFloppyTab)) return;
                UE_LOGI("laptop_tab_drill: client READY to read the other client's tabs");
                return;
            }
            ReaderVerdict("the second client's widget reached tab 10 (the floppy tab), the first client's last");
            return;
        }
        if (g_phase == Phase::Start) {
            if (g_h.step == HStep::Off) {
                // The other client is here once the host relays its pose to this one: the session's own word that
                // it is in the world. A relay with no one to relay to would prove nothing.
                if (!coop::players::Registry::Get().Puppet(2)) return;
                HolderStart(kTab3, 2, -1);
                return;
            }
            HolderTick(s);
            if (!HolderDone()) return;
            // Leave at once: the host's broadcast of the release comes behind any state it sent this holder.
            DS::ForceExitInterface(me);
            g_phase = Phase::Leaving;
            return;
        }
        if (DO::LocalHolds(kClaim) || DO::HolderOf(kClaim) != 0xFF) return;
        if (LT::ReceivedCount() != g_heldReceived) {
            char what[128];
            std::snprintf(what, sizeof what, "the relay sent the source %u state(s) for its own reports",
                          static_cast<unsigned>(LT::ReceivedCount() - g_heldReceived));
            Fail(what);
            return;
        }
        Pass("the first client held the PC, switched twice, and was sent no state for its own reports");
        return;
    default:
        return;
    }
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (ArmOf() == Arm::Off || !s || g_done) return;
    if (!g_armStarted) ArmBegin();
    if (s->role() == coop::net::Role::Host) {
        HostTick(s);
        return;
    }
    if (!s->connected()) return;
    ClientTick(s);
}

void OnDisconnect() {
    if (ArmOf() == Arm::Off) return;
    if (!g_done && !g_endWhy.empty()) Fail(g_endWhy.c_str());
    HolderCancelWalk();
    g_h = Holder{};
    g_reader = Reader{};
    g_phase = Phase::Start;
    g_armStarted = false;
    g_baseIdx = -1;
    g_endWhy.clear();
    g_done = false;
}

}  // namespace coop::dev::laptop_tab_drill
