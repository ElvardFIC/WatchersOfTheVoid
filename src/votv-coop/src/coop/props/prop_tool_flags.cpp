#include "coop/props/prop_tool_flags.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_key_index.h"
#include "coop/props/prop_wire_parity.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/prop_events.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/engine/hit_result.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/facts/function_row.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace coop::prop_tool_flags {
namespace {

namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
namespace PF = coop::net::propspawn_flags;

UE_FACT_FN(kRowInit, "prop_tool_flags", nullptr, L"init", AnyClass);
constexpr int kTag = 0x54464C47;
constexpr uint8_t kFlagBits = PF::kStatic | PF::kFrozen | PF::kSleep;
constexpr uint8_t kAllowedFlags = kFlagBits | PF::kLiveState;
constexpr size_t kMaxPending = 64;

std::atomic<coop::net::Session*> g_session{nullptr};
enum class Registration : uint8_t { Pending, Registered, Refused };
Registration g_registration = Registration::Pending;
bool g_watchSettled = false;

struct Press {
    uint64_t invocation = 0;
    coop::net::Session* session = nullptr;
    uint32_t sessionSerial = 0;
    uint32_t world = 0;
    ue_wrap::CachedObjRef target;
    std::wstring toolClass;
    std::wstring key;
    uint8_t before = 0;
};

std::vector<Press> g_pending;
uint32_t g_sent = 0;
uint32_t g_applied = 0;
uint32_t g_same = 0;
uint32_t g_held = 0;
uint32_t g_unresolved = 0;

bool IsToolClass(void* object) {
    return object && R::ClassNameOf(object).rfind(L"tool_", 0) == 0;
}

SG::Verdict OnInitPre(const SG::Call& call) {
    if (!IsToolClass(call.object)) return SG::Verdict::Run;

    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->connected() || !call.locals || !call.function) return SG::Verdict::Run;
    if (g_pending.size() >= kMaxPending) {
        UE_LOGW("prop_tool_flags: too many tool init calls are pending; this action will not be replicated");
        return SG::Verdict::Run;
    }

    static R::ParamOffset toolgunOffset{L"toolgun"};
    const int32_t offset = toolgunOffset.Of(call.function);
    const int32_t frameSize = R::FunctionFrameSize(call.function);
    if (offset < 0 || frameSize < 0 || offset > frameSize ||
        sizeof(void*) > static_cast<size_t>(frameSize - offset)) {
        return SG::Verdict::Run;
    }
    void* toolgun = nullptr;
    std::memcpy(&toolgun, call.locals + offset, sizeof(toolgun));
    if (!toolgun || !R::IsLive(toolgun)) return SG::Verdict::Run;

    ue_wrap::hit_result::Hit hit;
    if (!ue_wrap::hit_result::ReadField(toolgun, L"outHit", &hit) || !hit.blocking || !hit.actor ||
        !ue_wrap::prop::IsDescendantOfProp(hit.actor)) {
        return SG::Verdict::Run;
    }

    const uint8_t flags = coop::prop_wire_parity::PhysFlagsOf(hit.actor);
    if (!(flags & PF::kLiveState)) return SG::Verdict::Run;

    const std::wstring key = ue_wrap::prop::GetInteractableKeyString(hit.actor);
    coop::net::WireKey wireKey{};
    if (key.empty() || key == L"None" || key.size() > sizeof(wireKey.data)) return SG::Verdict::Run;

    Press press{};
    press.invocation = call.invocation;
    press.session = session;
    press.sessionSerial = coop::net::session_serial::Current();
    press.world = ue_wrap::world_identity::Generation();
    press.target.Set(hit.actor);
    press.toolClass = R::ClassNameOf(call.object);
    press.key = key;
    press.before = flags & kFlagBits;
    g_pending.push_back(std::move(press));
    return SG::Verdict::Run;
}

void OnInitPost(const SG::Call& call) {
    const auto it = std::find_if(g_pending.begin(), g_pending.end(),
                                 [&](const Press& press) { return press.invocation == call.invocation; });
    if (it == g_pending.end()) return;

    Press press = std::move(*it);
    g_pending.erase(it);
    auto* session = g_session.load(std::memory_order_acquire);
    void* target = press.target.Get();
    if (!target || !session || session != press.session || !session->connected() ||
        coop::net::session_serial::Current() != press.sessionSerial ||
        ue_wrap::world_identity::Generation() != press.world) {
        return;
    }

    const uint8_t flags = coop::prop_wire_parity::PhysFlagsOf(target) & kFlagBits;
    if (flags == press.before) return;

    coop::net::PropToolFlagsPayload payload{};
    coop::net::WireKeyFromString(press.key, payload.key);
    payload.physFlags = static_cast<uint8_t>(flags | PF::kLiveState);
    if (!session->SendReliable(coop::net::ReliableKind::PropToolFlags, &payload, sizeof(payload))) {
        UE_LOGW("prop_tool_flags: %ls changed '%ls' but PropToolFlags could not be queued",
                R::ClassNameOf(call.object).c_str(), press.key.c_str());
        return;
    }
    ++g_sent;
    UE_LOGI("prop_tool_flags: %ls changed '%ls' static=%d frozen=%d sleep=%d (#%u)",
            press.toolClass.c_str(), press.key.c_str(), (flags & PF::kStatic) ? 1 : 0,
            (flags & PF::kFrozen) ? 1 : 0, (flags & PF::kSleep) ? 1 : 0, g_sent);
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_registration != Registration::Pending) return;
    g_registration = SG::WatchName(kRowInit, kTag, &OnInitPre, &OnInitPost)
                         ? Registration::Registered
                         : Registration::Refused;
}

void Tick() {
    for (auto it = g_pending.begin(); it != g_pending.end();) {
        if (!SG::IsInvocationActive(it->invocation)) {
            UE_LOGW("prop_tool_flags: a tool init ended without its post callback; dropping its pending state");
            it = g_pending.erase(it);
        } else {
            ++it;
        }
    }

    if (g_watchSettled || g_registration == Registration::Pending) return;
    if (g_registration == Registration::Registered) SG::ResolvePendingNames();
    if (g_registration == Registration::Registered && SG::NameWatchLive(kRowInit, kTag)) {
        g_watchSettled = true;
        UE_LOGI("prop_tool_flags: the watch on every class's init is live");
    } else if (g_registration == Registration::Refused || SG::NameWatchSettled(kRowInit, kTag)) {
        g_watchSettled = true;
        UE_LOGE("prop_tool_flags: the watch on every class's init is unavailable; toolgun flag changes "
                "will remain local");
    }
}

void OnToolFlags(int senderSlot, const coop::net::PropToolFlagsPayload& payload) {
    const std::wstring key = coop::net::StringFromWireKey(payload.key);
    void* actor = coop::prop_key_index::Find(key);
    if (!actor || !ue_wrap::prop::IsDescendantOfProp(actor)) {
        ++g_unresolved;
        UE_LOGW("prop_tool_flags: slot %d changed '%ls' but no keyed prop resolves here",
                senderSlot, key.c_str());
        return;
    }

    void* localPlayer = coop::players::Registry::Get().Local();
    if (localPlayer && ue_wrap::engine::IsMainPlayerGrabbing(localPlayer, actor)) {
        ++g_held;
        UE_LOGI("prop_tool_flags: slot %d changed '%ls'; this peer holds it, leaving the state to the hold",
                senderSlot, key.c_str());
        return;
    }

    const bool wantStatic = (payload.physFlags & PF::kStatic) != 0;
    const bool wantFrozen = (payload.physFlags & PF::kFrozen) != 0;
    const bool wantSleep = (payload.physFlags & PF::kSleep) != 0;
    const uint8_t current = coop::prop_wire_parity::PhysFlagsOf(actor) & kFlagBits;
    if (current == (payload.physFlags & kFlagBits)) {
        ++g_same;
        return;
    }
    if (!ue_wrap::prop::CallBaseSetPropProps(actor, wantStatic, wantFrozen, wantSleep)) {
        UE_LOGW("prop_tool_flags: slot %d changed '%ls'; the base setPropProps did not dispatch",
                senderSlot, key.c_str());
        return;
    }
    ++g_applied;
    UE_LOGI("prop_tool_flags: slot %d set '%ls' static=%d frozen=%d sleep=%d (#%u)", senderSlot,
            key.c_str(), wantStatic ? 1 : 0, wantFrozen ? 1 : 0, wantSleep ? 1 : 0, g_applied);
}

void OnDisconnect() {
    if (g_sent || g_applied || g_same || g_held || g_unresolved) {
        UE_LOGI("prop_tool_flags: session end -- sent=%u applied=%u same=%u held=%u unresolved=%u",
                g_sent, g_applied, g_same, g_held, g_unresolved);
    }
    g_session.store(nullptr, std::memory_order_release);
    g_pending.clear();
    g_sent = g_applied = g_same = g_held = g_unresolved = 0;
}

}  // namespace coop::prop_tool_flags
