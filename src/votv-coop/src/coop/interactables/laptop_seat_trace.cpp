#include "coop/interactables/laptop_seat_trace.h"

#include "coop/interactables/device_occupancy.h"
#include "coop/net/session.h"
#include "coop/props/prop_wire_parity.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/laptop.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace coop::laptop_seat_trace {
namespace {

namespace DO = coop::device_occupancy;
namespace L = ue_wrap::laptop;
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;

constexpr int kTagStabilizeSeat = 0x4C535453;
constexpr int kTagSetPropProps = 0x4C535450;
constexpr size_t kMaxPending = 32;

std::atomic<coop::net::Session*> g_session{nullptr};
enum class Registration : uint8_t { Pending, Registered, Refused };
Registration g_stabilizeRegistration = Registration::Pending;
Registration g_setPropsRegistration = Registration::Pending;
bool g_stabilizeReported = false;
bool g_setPropsReported = false;

struct Stabilize {
    uint64_t invocation = 0;
    ue_wrap::CachedObjRef object;
    std::wstring key;
    bool isProp = false;
    uint8_t flags = 0;
    uint32_t setterCalls = 0;
};

struct Setter {
    uint64_t invocation = 0;
    uint64_t stabilizeInvocation = 0;
    ue_wrap::CachedObjRef target;
    std::wstring key;
    std::wstring targetClass;
    std::wstring callerClass;
    std::wstring callerFunction;
    uint8_t before = 0;
    bool isProp = false;
    bool localLaptopClaim = false;
    bool baseLaptopTerminal = false;
    bool activeRead = false;
    bool active = false;
};

std::vector<Stabilize> g_stabilizers;
std::vector<Setter> g_setters;
uint32_t g_stabilizeCalls = 0;

bool ReadBoolParam(const SG::Call& call, const wchar_t* name, bool& out) {
    if (!call.locals || !call.function) return false;
    R::ParamOffset offset{name};
    const int32_t at = offset.Of(call.function);
    const int32_t size = R::FunctionFrameSize(call.function);
    if (at < 0 || size < 0 || at > size ||
        sizeof(out) > static_cast<size_t>(size - at)) return false;
    std::memcpy(&out, call.locals + at, sizeof(out));
    return true;
}

uint64_t ActiveStabilizeInvocation() {
    for (auto it = g_stabilizers.rbegin(); it != g_stabilizers.rend(); ++it)
        if (SG::IsInvocationActive(it->invocation)) return it->invocation;
    return 0;
}

SG::Verdict OnStabilizePre(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->connected()) return SG::Verdict::Run;
    if (g_stabilizers.size() >= kMaxPending) {
        UE_LOGW("laptop_seat_trace: too many stabilizeSeat calls are pending");
        return SG::Verdict::Run;
    }

    Stabilize row{};
    row.invocation = call.invocation;
    row.object.Set(call.object);
    row.isProp = ue_wrap::prop::IsDescendantOfProp(call.object);
    if (row.isProp) {
        row.key = ue_wrap::prop::GetInteractableKeyString(call.object);
        row.flags = coop::prop_wire_parity::PhysFlagsOf(call.object);
    }
    g_stabilizers.push_back(std::move(row));

    ++g_stabilizeCalls;
    L::EnsureResolved();
    void* const laptop = L::Instance();
    void* const terminal = L::TerminalInUse();
    const auto& captured = g_stabilizers.back();
    UE_LOGI("laptop_seat_trace: stabilizeSeat #%u object=%p class='%ls' key='%ls' flags=0x%02x "
            "laptop=%p terminal=%p localLaptopClaim=%u terminalIsBaseLaptop=%u",
            g_stabilizeCalls, call.object, R::ClassNameOf(call.object).c_str(), captured.key.c_str(),
            static_cast<unsigned>(captured.flags), laptop, terminal,
            DO::LocalHolds(L"laptop") ? 1u : 0u, laptop && terminal == laptop ? 1u : 0u);
    return SG::Verdict::Run;
}

void OnStabilizePost(const SG::Call& call) {
    const auto it = std::find_if(g_stabilizers.begin(), g_stabilizers.end(),
                                 [&](const Stabilize& row) { return row.invocation == call.invocation; });
    if (it == g_stabilizers.end()) return;
    void* const object = it->object.Get();
    UE_LOGI("laptop_seat_trace: stabilizeSeat returned object=%p key='%ls' flags=0x%02x "
            "setPropPropsCalls=%u",
            object, it->key.c_str(), static_cast<unsigned>(it->flags), it->setterCalls);
    g_stabilizers.erase(it);
}

SG::Verdict OnSetPropPropsPre(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->connected()) return SG::Verdict::Run;

    const uint64_t stabilizeInvocation = ActiveStabilizeInvocation();
    const bool localLaptopClaim = DO::LocalHolds(L"laptop");
    L::EnsureResolved();
    void* const laptop = L::Instance();
    const bool baseLaptopTerminal = laptop && L::TerminalInUse() == laptop;
    if (!stabilizeInvocation && !(localLaptopClaim && baseLaptopTerminal)) return SG::Verdict::Run;
    if (g_setters.size() >= kMaxPending) {
        UE_LOGW("laptop_seat_trace: too many setPropProps calls are pending");
        return SG::Verdict::Run;
    }

    Setter row{};
    row.invocation = call.invocation;
    row.stabilizeInvocation = stabilizeInvocation;
    row.target.Set(call.object);
    row.targetClass = R::ClassNameOf(call.object);
    row.callerClass = R::ClassNameOf(call.callerObject);
    row.callerFunction = call.callerFunction ? R::ToString(R::NameOf(call.callerFunction)) : L"";
    row.localLaptopClaim = localLaptopClaim;
    row.baseLaptopTerminal = baseLaptopTerminal;
    row.isProp = ue_wrap::prop::IsDescendantOfProp(call.object);
    if (row.isProp) {
        row.key = ue_wrap::prop::GetInteractableKeyString(call.object);
        row.before = coop::prop_wire_parity::PhysFlagsOf(call.object);
    }
    row.activeRead = ReadBoolParam(call, L"active", row.active);

    if (stabilizeInvocation) {
        const auto active = std::find_if(g_stabilizers.begin(), g_stabilizers.end(),
                                         [&](const Stabilize& state) {
                                             return state.invocation == stabilizeInvocation;
                                         });
        if (active != g_stabilizers.end()) ++active->setterCalls;
    }
    g_setters.push_back(std::move(row));
    return SG::Verdict::Run;
}

void OnSetPropPropsPost(const SG::Call& call) {
    const auto it = std::find_if(g_setters.begin(), g_setters.end(),
                                 [&](const Setter& row) { return row.invocation == call.invocation; });
    if (it == g_setters.end()) return;
    Setter row = std::move(*it);
    g_setters.erase(it);

    void* const target = row.target.Get();
    if (!target) {
        UE_LOGW("laptop_seat_trace: setPropProps target ended before POST");
        return;
    }
    const uint8_t after = row.isProp ? coop::prop_wire_parity::PhysFlagsOf(target) : 0;
    UE_LOGI("laptop_seat_trace: setPropProps key='%ls' targetClass='%ls' callerClass='%ls' "
            "callerFunction='%ls' activeArg=%ls flags=0x%02x->0x%02x stabilize=%u "
            "localLaptopClaim=%u terminalIsBaseLaptop=%u",
            row.key.c_str(), row.targetClass.c_str(), row.callerClass.c_str(), row.callerFunction.c_str(),
            row.activeRead ? (row.active ? L"true" : L"false") : L"<unread>",
            static_cast<unsigned>(row.before), static_cast<unsigned>(after),
            row.stabilizeInvocation ? 1u : 0u, row.localLaptopClaim ? 1u : 0u,
            row.baseLaptopTerminal ? 1u : 0u);
}

void RegisterWatches() {
    if (g_stabilizeRegistration == Registration::Pending) {
        g_stabilizeRegistration =
            SG::WatchName(L"stabilizeSeat", kTagStabilizeSeat, &OnStabilizePre, &OnStabilizePost)
                ? Registration::Registered
                : Registration::Refused;
    }
    if (g_setPropsRegistration == Registration::Pending) {
        g_setPropsRegistration =
            SG::WatchName(L"setPropProps", kTagSetPropProps, &OnSetPropPropsPre, &OnSetPropPropsPost)
                ? Registration::Registered
                : Registration::Refused;
    }
}

void ReportWatchState(Registration registration, const wchar_t* name, int tag, bool& reported) {
    if (reported || registration == Registration::Pending) return;
    if (registration == Registration::Registered && SG::NameWatchLive(name, tag)) {
        reported = true;
        UE_LOGI("laptop_seat_trace: watch on '%ls' is live", name);
    } else if (registration == Registration::Refused || SG::PendingNameCount() == 0) {
        reported = true;
        UE_LOGE("laptop_seat_trace: watch on '%ls' is unavailable; the laptop seat path cannot be traced",
                name);
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    RegisterWatches();
}

void Tick() {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running()) return;

    SG::ResolvePendingNames();
    ReportWatchState(g_stabilizeRegistration, L"stabilizeSeat", kTagStabilizeSeat, g_stabilizeReported);
    ReportWatchState(g_setPropsRegistration, L"setPropProps", kTagSetPropProps, g_setPropsReported);

    for (auto it = g_stabilizers.begin(); it != g_stabilizers.end();) {
        if (SG::IsInvocationActive(it->invocation)) {
            ++it;
        } else {
            UE_LOGW("laptop_seat_trace: stabilizeSeat ended without POST");
            it = g_stabilizers.erase(it);
        }
    }
    for (auto it = g_setters.begin(); it != g_setters.end();) {
        if (SG::IsInvocationActive(it->invocation)) {
            ++it;
        } else {
            UE_LOGW("laptop_seat_trace: setPropProps ended without POST");
            it = g_setters.erase(it);
        }
    }
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_stabilizers.clear();
    g_setters.clear();
    g_stabilizeCalls = 0;
}

}  // namespace coop::laptop_seat_trace
