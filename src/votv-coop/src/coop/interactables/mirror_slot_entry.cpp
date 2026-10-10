// coop/interactables/mirror_slot_entry.cpp -- see coop/interactables/mirror_slot_entry.h.

#include "coop/interactables/mirror_slot_entry.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/net/session.h"
#include "coop/props/remote_prop.h"  // IsActorUnderAnyDrive: a prop a remote player carries

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"  // FindParamOffset
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace coop::mirror_slot_entry {
namespace {

namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;

std::atomic<coop::net::Session*> g_session{nullptr};

// The devices whose overlap takes a prop, and whose take reaches the other peers through a lane or the relayed
// destroy: a receiver port's insert (coop/interactables/drive_sync's slot line), a desk slot's module
// (coop/interactables/physmods_sync), the rack's (coop/interactables/drive_rack_sync), the tape wall's reel boxes
// (coop/interactables/tape_caddy_sync), and the takes that consume the prop -- a reel case's lid or reel and a drive
// box's drive -- whose record crosses by coop/props/prop_record_refresh, the prop's destroy relayed. A device whose
// take no lane carries is not here: refusing a mirror there would only hide that its state never crosses.
UE_FACT_FN(kRowDriveSlotPortOverlap, "mirror_slot_entry", L"driveSlot_C",
           L"BndEvt__driveSlot_drivePort_play_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDriveRackOverlap, "mirror_slot_entry", L"prop_driveRack_C", L"ReceiveActorBeginOverlap", Declared);
UE_FACT_FN(kRowBoxOverlap, "mirror_slot_entry", L"prop_box_C", L"ReceiveActorBeginOverlap", Declared);
UE_FACT_FN(kRowReelboxTriggerOverlap, "mirror_slot_entry", L"prop_reelbox_C",
           L"BndEvt__prop_reelbox_trigger_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowTapesReelboxSmallOverlap, "mirror_slot_entry", L"wallunit_tapes_C",
           L"BndEvt__wallunit_tapes_reelbox_small_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowTapesReelboxBigOverlap, "mirror_slot_entry", L"wallunit_tapes_C",
           L"BndEvt__wallunit_tapes_reelbox_big_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod1Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod1_K2Node_ComponentBoundEvent_9_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod2Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod2_K2Node_ComponentBoundEvent_10_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod3Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod3_K2Node_ComponentBoundEvent_11_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod4Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod4_K2Node_ComponentBoundEvent_12_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod5Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod5_K2Node_ComponentBoundEvent_13_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod6Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod6_K2Node_ComponentBoundEvent_14_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod7Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod7_K2Node_ComponentBoundEvent_15_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod8Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod8_K2Node_ComponentBoundEvent_16_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod9Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod9_K2Node_ComponentBoundEvent_17_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod10Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod10_K2Node_ComponentBoundEvent_18_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod11Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod11_K2Node_ComponentBoundEvent_19_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowDeskMod12Overlap, "mirror_slot_entry", L"analogDScreenTest_C",
           L"BndEvt__analogDScreenTest_mod12_K2Node_ComponentBoundEvent_20_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);

struct Entry {
    const ue_wrap::facts::Function* row;
};
const Entry kEntries[] = {
    {&kRowDriveSlotPortOverlap},
    {&kRowDriveRackOverlap},
    {&kRowBoxOverlap},
    {&kRowReelboxTriggerOverlap},
    {&kRowTapesReelboxSmallOverlap},
    {&kRowTapesReelboxBigOverlap},
    {&kRowDeskMod1Overlap},
    {&kRowDeskMod2Overlap},
    {&kRowDeskMod3Overlap},
    {&kRowDeskMod4Overlap},
    {&kRowDeskMod5Overlap},
    {&kRowDeskMod6Overlap},
    {&kRowDeskMod7Overlap},
    {&kRowDeskMod8Overlap},
    {&kRowDeskMod9Overlap},
    {&kRowDeskMod10Overlap},
    {&kRowDeskMod11Overlap},
    {&kRowDeskMod12Overlap},
};
constexpr int kEntryCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));
constexpr int kTagBase = 0x4D534C00;  // 'MSL' and the entry's index

enum class Reg : uint8_t { Pending, Registered, Refused };
Reg g_reg[kEntryCount] = {};
bool g_registered = false;
bool g_settled = false;
uint64_t g_refused = 0, g_letRun = 0;

// Each entry's OtherActor parameter, found by its name in the entry's own frame at its first call (a component's
// begin-overlap delegate carries it second, an actor's ReceiveActorBeginOverlap first): kUnread until then, -1 when
// the entry has none, said once.
constexpr int32_t kUnread = INT32_MIN;
int32_t g_actorOff[kEntryCount] = {};

// [dev] subject_drill=carryred: the control, whose entries run whoever carries the prop.
bool Control() {
    static const bool red =
        coop::config::ResolveEnum(::coop::config_registry::rows::subject_drill) == std::string("carryred");
    return red;
}

// The entry and a prop a remote player carries, either one: the prop entering, or the device itself in a remote
// player's hands (a rack or a reel case taking a resting prop). Our own calls are no exception: a carried mirror only
// ever moves by the pose drive's teleport, whose overlap update fires the entry inside that call.
sg::Verdict OnEntryPre(const sg::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || !call.locals) return sg::Verdict::Run;
    const int index = call.tag - kTagBase;
    if (index < 0 || index >= kEntryCount) return sg::Verdict::Run;
    int32_t& off = g_actorOff[index];
    if (off == kUnread) {
        off = R::FindParamOffset(call.function, L"OtherActor");
        if (off < 0)
            UE_LOGE("mirror_slot_entry: %ls::%ls has no OtherActor parameter -- this entry refuses nothing",
                    kEntries[index].row->Class(), kEntries[index].row->Name());
    }
    if (off < 0) return sg::Verdict::Run;
    void* actor = *reinterpret_cast<void* const*>(call.locals + off);
    if (!actor) return sg::Verdict::Run;
    const bool carried = coop::remote_prop::IsActorUnderAnyDrive(actor);
    if (!carried && !coop::remote_prop::IsActorUnderAnyDrive(call.object)) return sg::Verdict::Run;
    if (Control()) {
        if (++g_letRun <= 20)
            UE_LOGW("mirror_slot_entry: [control] %ls takes %ls, %s a remote player carries",
                    R::ClassNameOf(call.object).c_str(), R::ClassNameOf(actor).c_str(),
                    carried ? "a prop" : "the device");
        return sg::Verdict::Run;
    }
    if (++g_refused <= 20 || g_refused % 100 == 0)
        UE_LOGI("mirror_slot_entry: %ls does not take %ls, %s a remote player carries -- its carrier's world does "
                "(%llu refused)", R::ClassNameOf(call.object).c_str(), R::ClassNameOf(actor).c_str(),
                carried ? "a prop" : "the device", static_cast<unsigned long long>(g_refused));
    return sg::Verdict::Cancel;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_registered) return;
    g_registered = true;
    for (int32_t& off : g_actorOff) off = kUnread;
    for (int i = 0; i < kEntryCount; ++i)
        g_reg[i] = sg::WatchName(*kEntries[i].row, kTagBase + i, &OnEntryPre, nullptr)
                       ? Reg::Registered : Reg::Refused;
}

void Tick() {
    if (g_settled || !g_registered) return;
    sg::ResolvePendingNames();
    int live = 0, settled = 0;
    for (int i = 0; i < kEntryCount; ++i) {
        if (g_reg[i] == Reg::Refused) { ++settled; continue; }
        if (sg::NameWatchLive(*kEntries[i].row, kTagBase + i)) { ++live; ++settled; continue; }
        if (sg::NameWatchSettled(*kEntries[i].row, kTagBase + i)) ++settled;
    }
    if (settled < kEntryCount) return;
    g_settled = true;
    if (live == kEntryCount) {
        UE_LOGI("mirror_slot_entry: the %d device entries are watched (a receiver port, the rack, a drive box, a reel "
                "case, the tape wall's two reel boxes, the desk's twelve module slots)", kEntryCount);
        return;
    }
    for (int i = 0; i < kEntryCount; ++i)
        if (g_reg[i] == Reg::Refused || !sg::NameWatchLive(*kEntries[i].row, kTagBase + i))
            UE_LOGE("mirror_slot_entry: %ls::%ls is unwatched -- that device takes a remote player's carried prop on "
                    "every peer", kEntries[i].row->Class(), kEntries[i].row->Name());
}

uint64_t Refused() { return g_refused; }

void OnDisconnect() {
    if (g_refused || g_letRun)
        UE_LOGI("mirror_slot_entry: session end -- %llu entries refused, %llu let run by the control",
                static_cast<unsigned long long>(g_refused), static_cast<unsigned long long>(g_letRun));
    g_refused = g_letRun = 0;
}

}  // namespace coop::mirror_slot_entry
