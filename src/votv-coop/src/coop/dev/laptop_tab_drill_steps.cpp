// coop/dev/laptop_tab_drill_steps.cpp -- see coop/dev/laptop_tab_drill_steps.h.

#include "laptop_tab_drill_steps.h"  // co-located private header (src tree, not include/)

#include "coop/dev/director/director.h"
#include "coop/dev/director/routes.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/interactables/laptop_tab.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/devices/appliance.h"  // CallAction: the player's use, dispatched
#include "ue_wrap/devices/laptop.h"
#include "ue_wrap/devices/laptop_widget.h"
#include "ue_wrap/engine/engine.h"  // TryGetActorLocation, GetActorVelocity
#include "ue_wrap/engine/engine_component.h"  // GetComponentLocation
#include "ue_wrap/engine/engine_mainplayer.h"  // ReadMainPlayerHitActor
#include "ue_wrap/engine/engine_pawn.h"  // GetController, SetControlRotation, GetCameraLocation
#include "ue_wrap/engine/umg_build.h"  // IndexOfChild: where the sub-game page sits in the switcher

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace coop::dev::laptop_tab_drill_steps {
namespace {

namespace R  = ue_wrap::reflection;
namespace D  = coop::director;
namespace DO = coop::device_occupancy;
namespace L  = ue_wrap::laptop;
namespace LT = coop::laptop_tab;
namespace UL = ue_wrap::laptop_widget;
using ue_wrap::FVector;

// The two tab buttons that write the switcher without switcherSwitched.
constexpr const wchar_t* kFloppyClick =
    L"BndEvt__ui_laptop_button_tab_floppy_K2Node_ComponentBoundEvent_54_OnButtonClickedEvent__DelegateSignature";
constexpr const wchar_t* kSubgameClick =
    L"BndEvt__ui_laptop_Button_305_K2Node_ComponentBoundEvent_57_OnButtonClickedEvent__DelegateSignature";
constexpr uint8_t kEnterAction = 4;
constexpr uint64_t kHeldBoundMs   = 20000;  // enter to a confirmed claim
constexpr float    kRingCm        = 60.f;
constexpr float    kReachCm       = 60.f;
constexpr int      kStandpoints   = 6;
constexpr int      kFirstWalkDeadlineS = 400;
constexpr int      kNextWalkDeadlineS  = 120;
constexpr int      kMaxSeq        = 3;
constexpr uint64_t kStatesBound   = 20;  // states received without the final tab: the viewer's failure, by count
// The aim before the enter: a fan of headings about the laptop's boxScreen, each held kAimTicksPerPose ticks so the
// player's own trace and the laptop's lookAt run on it before it is judged.
constexpr float    kAimYaw[]      = {0.f, -6.f, 6.f, -12.f, 12.f};
constexpr float    kAimPitch[]    = {0.f, -8.f, 8.f, -16.f, 16.f};
constexpr int      kAimYawN       = static_cast<int>(sizeof kAimYaw / sizeof kAimYaw[0]);
constexpr int      kAimPoses      = kAimYawN * static_cast<int>(sizeof kAimPitch / sizeof kAimPitch[0]);
constexpr int      kAimTicksPerPose = 6;

// What the arm judges against, taken at its start: the module's counts only grow, and a seed or a refusal that came
// before the judging peer got to its first step must still count as before the arm.
constexpr LT::Cause kCauses[] = {LT::Cause::NotHolder, LT::Cause::BadIndex, LT::Cause::BadSlot, LT::Cause::NoWidget};
bool     g_armStarted = false;
uint64_t g_baseReceived = 0;
uint64_t g_baseRefused[4] = {};
const char* g_armName = "off";  // the arm's log name, given at its start

bool g_done = false;  // this peer's part of the arm has ended, its verdict said
// What the judging peer says if the session ends before its verdict; empty on a peer that judges nothing.
std::string g_endWhy;

// The tabs between a holder's first and last may coalesce under the lane's pace, so only the last is demanded: it passes
// when a state carrying it has arrived and the widget shows it, and fails after kStatesBound states without it, or when
// the session ends.
struct Reader {
    bool     active = false;
    int      finalTab = -1;
    uint64_t seen = 0;
};
Reader g_reader;

enum class HStep : uint8_t { Off, Find, Walk, Aim, Enter, Held, Act, Done };
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
    int      aimPose = 0;
    int      aimTick = 0;
};
Holder g_h;
uint64_t g_heldReceived = 0;  // the arrivals at the confirmed claim: what came after was sent while this peer held it

void HGo(HStep s) { g_h.step = s; g_h.stepMs = NowMs(); }
bool HExpired(uint64_t bound) { return NowMs() - g_h.stepMs > bound; }
void StartAim() {
    g_h.aimPose = g_h.aimTick = 0;
    HGo(HStep::Aim);
}

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

// A pc part's actor: the laptop's child-actor component `prop` (pcpart_keyboard, pcpart_mouse) and what it spawned.
void* PartActor(void* laptop, const wchar_t* prop) {
    const int32_t off = R::FindPropertyOffset(R::ClassOf(laptop), prop);
    if (off < 0) return nullptr;
    void* const comp = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(laptop) + off);
    if (!comp || !R::IsLive(comp)) return nullptr;
    const int32_t childOff = R::FindPropertyOffset(R::ClassOf(comp), L"ChildActor");
    if (childOff < 0) return nullptr;
    void* const actor = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(comp) + childOff);
    return (actor && R::IsLive(actor)) ? actor : nullptr;
}

constexpr float kUnread = -9.f;  // a value the drill could not read: outside every up-vector and distance it logs

// The actor's up-vector Z, which the game's enter wants above 0.5 for the keyboard and the mouse.
float UpZ(void* actor) {
    if (!actor) return kUnread;
    void* const fn = R::FindDispatchFunctionCached(R::ClassOf(actor), L"GetActorUpVector");
    if (!fn) return kUnread;
    ue_wrap::ParamFrame f(fn);
    if (!f.valid() || !ue_wrap::Call(actor, f)) return kUnread;
    return f.Get<FVector>(L"ReturnValue").Z;
}

float SpeedOf(void* actor) {
    if (!actor) return kUnread;
    const FVector v = ue_wrap::engine::GetActorVelocity(actor);
    return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
}

float DistanceBetween(void* a, void* b) {
    FVector pa, pb;
    if (!a || !b || !ue_wrap::engine::TryGetActorLocation(a, pa) || !ue_wrap::engine::TryGetActorLocation(b, pb))
        return kUnread;
    const float dx = pa.X - pb.X, dy = pa.Y - pb.Y, dz = pa.Z - pb.Z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The laptop's own aim flags as its lookAt last left them. lookAt sets use_floppybox and use_zipBox from the component
// the player's trace hit, and action 4 checks them first: either one set ejects the floppy and returns without a hint,
// so a player's enter is pressed while the aim is on the laptop off both hitboxes.
struct AimFlags { bool read = false, floppy = false, zip = false; };
AimFlags ReadAimFlags(void* laptop) {
    AimFlags a;
    void* const cls = R::ClassOf(laptop);
    const int32_t fOff = R::FindPropertyOffset(cls, L"use_floppybox");
    const int32_t zOff = R::FindPropertyOffset(cls, L"use_zipBox");
    if (fOff < 0 || zOff < 0) return a;
    auto* const base = reinterpret_cast<uint8_t*>(laptop);
    a.read = true;
    a.floppy = *reinterpret_cast<bool*>(base + fOff);
    a.zip = *reinterpret_cast<bool*>(base + zOff);
    return a;
}

// The box a player aims at to enter: getActionOptions offers action 4 as the enter only while the aimed component is
// boxScreen (on the floppy or zip hitbox the same action ejects).
void* BoxScreenOf(void* laptop) {
    const int32_t off = R::FindPropertyOffset(R::ClassOf(laptop), L"boxScreen");
    if (off < 0) return nullptr;
    void* screen = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(laptop) + off);
    return (screen && R::IsLive(screen)) ? screen : nullptr;
}

// The game's enter conditions as this peer reads them, for the claim that was never confirmed. actionOptionIndex returns
// without a word while the boot or shutdown latent runs (anim) or the wall power is off (powered); past those, action 4
// wants the keyboard and the mouse upright, still and within 150 uu of the laptop and the player inside its Box, and
// says a hint when one fails. The player's own distance and speed fill the named fields; the parts' follow, because the
// game judges the parts', not the player's. power=? is a power state that did not read; aim=? are flags that did not.
void LogEnterRefused(void* me) {
    void* const keyboard = PartActor(g_h.laptop, L"pcpart_keyboard");
    void* const mouse = PartActor(g_h.laptop, L"pcpart_mouse");
    L::PowerState power;
    const bool read = L::ReadPower(power);
    const AimFlags aim = ReadAimFlags(g_h.laptop);
    UE_LOGW("laptop_tab_drill: enter refused because keyboardUp=%.2f mouseUp=%.2f dist=%.0f inBox=%d speed=%.1f "
            "power=%hs powered=%d anim=%d isOpened=%d aim=%hs floppybox=%d zipBox=%d holder=%u partDist=%.0f/%.0f "
            "partSpeed=%.1f/%.1f",
            UpZ(keyboard), UpZ(mouse), DistanceBetween(me, g_h.laptop), InBox(g_h.box, me) ? 1 : 0, SpeedOf(me),
            read ? "read" : "?", power.powered ? 1 : 0, power.anim ? 1 : 0, power.isOpened ? 1 : 0,
            aim.read ? "read" : "?", aim.floppy ? 1 : 0, aim.zip ? 1 : 0, static_cast<unsigned>(DO::HolderOf(kClaim)),
            DistanceBetween(keyboard, g_h.laptop), DistanceBetween(mouse, g_h.laptop), SpeedOf(keyboard),
            SpeedOf(mouse));
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

}  // namespace

uint64_t NowMs() { return ::GetTickCount64(); }

void ArmBegin(const char* armName) {
    g_armName = armName;
    g_baseReceived = LT::ReceivedCount();
    for (LT::Cause c : kCauses) g_baseRefused[static_cast<int>(c)] = LT::RefusedCount(c);
    g_armStarted = true;
}
bool ArmStarted() { return g_armStarted; }
void ArmReset() { g_armStarted = false; }
uint64_t ReceivedSince() { return LT::ReceivedCount() - g_baseReceived; }
uint64_t RefusedSince(LT::Cause c) { return LT::RefusedCount(c) - g_baseRefused[static_cast<int>(c)]; }

void Pass(const char* what) {
    UE_LOGI("[LAPTOP-TAB-DRILL] %s PASS %s", g_armName, what);
    g_done = true;
}
void Fail(const char* what) {
    UE_LOGW("[LAPTOP-TAB-DRILL] %s FAIL %s", g_armName, what);
    g_done = true;
}
void Abandon(const char* why) {
    UE_LOGW("[LAPTOP-TAB-DRILL] ABANDONED %s: %s", g_armName, why);
    g_done = true;
}
bool Done() { return g_done; }
void MarkDone() { g_done = true; }
void SetEndWhy(const char* why) { g_endWhy = why; }
void ClearEndWhy() { g_endWhy.clear(); }
void VerdictOnDisconnect() {
    if (!g_done && !g_endWhy.empty()) Fail(g_endWhy.c_str());
}
void VerdictReset() {
    g_endWhy.clear();
    g_done = false;
}

uint8_t LocalSlot() { return coop::players::Registry::Get().LocalPeerId(); }

bool ReadOwnTab(int& idx) { return UL::ReadTab(UL::Widget(), idx); }

bool ReaderActive() { return g_reader.active; }

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

void ReaderVerdict(const char* passWhat) {
    char why[160] = {};
    switch (ReaderStep(why, sizeof why)) {
    case ReadState::Reached: Pass(passWhat); return;
    case ReadState::Failed:  Fail(why); return;
    default:                 return;
    }
}

void ReaderReset() { g_reader = Reader{}; }

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

bool HolderIdle() { return g_h.step == HStep::Off; }
void HolderReset() { g_h = Holder{}; }
uint64_t HeldReceived() { return g_heldReceived; }

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
        if (InBox(box, me)) { StartAim(); return; }
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
            StartAim();
            return;
        }
        NextStandpoint();
        return;
    }
    case HStep::Aim: {
        // Judged from the second tick of a pose: the first is the one the camera turned on. Ready is the game's own offer
        // of the enter: the trace on the laptop's boxScreen, and lookAt left neither hitbox flag set.
        const AimFlags fl = ReadAimFlags(g_h.laptop);
        void* const boxScreen = BoxScreenOf(g_h.laptop);
        void* const hitActor = ue_wrap::engine::ReadMainPlayerHitActor(me);
        void* const hitComp = hitActor ? ue_wrap::engine::ReadMainPlayerHitComponent(me) : nullptr;
        if (g_h.aimTick > 0 && fl.read && boxScreen && hitActor == g_h.laptop && hitComp == boxScreen && !fl.floppy &&
            !fl.zip) {
            UE_LOGI("laptop_tab_drill: aim on the laptop's boxScreen, neither hitbox flag set, at heading %d",
                    g_h.aimPose);
            HGo(HStep::Enter);
            return;
        }
        if (g_h.aimTick == 0 || g_h.aimTick >= kAimTicksPerPose) {
            if (g_h.aimPose >= kAimPoses) {
                UE_LOGW("laptop_tab_drill: the last heading's trace hit '%ls' component '%ls' (floppybox=%d zipBox=%d)",
                        hitActor && R::IsLive(hitActor) ? R::ClassNameOf(hitActor).c_str() : L"none",
                        hitComp && R::IsLive(hitComp) ? R::ToString(R::NameOf(hitComp)).c_str() : L"none",
                        fl.floppy ? 1 : 0, fl.zip ? 1 : 0);
                Abandon("no heading put the aim on the laptop's boxScreen with neither hitbox flag set");
                return;
            }
            void* const c = ue_wrap::engine::GetController(me);
            if (!c || !boxScreen) { Abandon("the laptop's boxScreen or the player's controller did not resolve"); return; }
            ue_wrap::FRotator r = D::LookAt(ue_wrap::engine::GetCameraLocation(),
                                            ue_wrap::engine::GetComponentLocation(boxScreen));
            r.Pitch += kAimPitch[g_h.aimPose / kAimYawN];
            r.Yaw += kAimYaw[g_h.aimPose % kAimYawN];
            ue_wrap::engine::SetControlRotation(c, r);
            ++g_h.aimPose;
            g_h.aimTick = 0;
        }
        ++g_h.aimTick;
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
            if (HExpired(kHeldBoundMs)) {
                LogEnterRefused(me);
                Abandon("the PC's claim was not confirmed after the enter (unpowered, a part unusable, or held by "
                        "another peer)");
            }
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

}  // namespace coop::dev::laptop_tab_drill_steps
