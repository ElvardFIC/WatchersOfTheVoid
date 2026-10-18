#include "coop/creatures/grayboar_sync.h"
#include "coop/net/session.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_element_tracker.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include <atomic>

namespace coop::grayboar_sync {
namespace {
namespace SG = ue_wrap::script_gate;
std::atomic<coop::net::Session*> g_session{nullptr};
std::atomic<bool> g_watched{false};
constexpr int kBodyTickTag = 0x474254;

SG::Verdict OnBodyTick(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || session->role() != coop::net::Role::Host ||
        !call.object) return SG::Verdict::Run;
    // Init/record authoring may finish after the first tick. Retry naturally at
    // the next tick, without assigning a second identity or claiming a mirror.
    const auto eid = coop::prop_element_tracker::GetPropElementIdForActor(call.object);
    if (eid == 0 || eid >= coop::element::kHostRangeSize) return SG::Verdict::Run;
    // ReceiveTick applies force/torque to the prop's StaticMesh. Streaming the
    // helper pawn would not reproduce that movement. Claim is idempotent and
    // already handles held-item arbitration, join repair and dead-body cleanup.
    coop::prop_drive_host::Claim(call.object, "grayboar body");
    return SG::Verdict::Run;
}
}

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_watched.load(std::memory_order_acquire)) return;
    // Class-scoped body watch also sees subclasses inheriting this function,
    // including the overcharged variant. Other props' ticks never enter here.
    if (SG::WatchClassName(L"grayboar_C", L"ReceiveTick", kBodyTickTag, &OnBodyTick, nullptr))
        g_watched.store(true, std::memory_order_release);
    else
        UE_LOGW("grayboar: body movement watch unavailable");
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    // prop_drive_host owns and clears the claims at the session disconnect edge.
}
}
