// coop/creatures/wisp_tear_mirror.cpp -- see coop/creatures/wisp_tear_mirror.h.

#include "coop/creatures/wisp_tear_mirror.h"

#include "coop/creatures/wisp_grab_hold.h"
#include "coop/element/element.h"
#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/actors/wisp.h"

#include <chrono>
#include <cstdint>
#include <vector>

namespace coop::wisp_tear_mirror {
namespace {

namespace R = ue_wrap::reflection;

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// Armed victim ragdoll-death deadline (0 = not armed). Game-thread only (set by OnWispGrab,
// discharged by Tick -- both on the game thread, so a plain value is safe).
uint64_t g_killAtMs = 0;
uint32_t g_killWispEid = 0;  // for logging only
ue_wrap::CachedObjRef g_killVictim;
constexpr uint64_t kTearRetryMs = 100;
struct PendingTear { coop::net::WispTearPayload payload{}; uint64_t until=0; uint64_t nextTry=0; };
std::vector<PendingTear> g_pendingTears;
uint64_t g_tearEpoch=0;

}  // namespace

void OnWispGrab(const coop::net::WispGrabPayload& p, uint8_t senderPeerSlot) {
    if (!p.wispElementId || !p.victimElementId) return;
    // Host-only origin (the host detected its wisp false-grabbing OUR puppet). Belt-and-
    // suspenders self-verify: the host directs this to our slot, but also confirm the
    // addressed Player Element id is OURS before we schedule our own death.
    const coop::element::ElementId localEid = coop::players::Registry::Get().LocalPlayerElementId();
    if (p.victimElementId != static_cast<uint32_t>(localEid)) {
        UE_LOGW("wisp_tear: WispGrab victimElementId=%u != our localPlayerElementId=%u -- dropping "
                "(misaddressed)", p.victimElementId, static_cast<uint32_t>(localEid));
        return;
    }
    void* local = coop::players::Registry::Get().Local();
    if (!local || !R::IsLive(local)) return;
    // Bind the delayed action now, before attachment can dispatch native callbacks.
    // Looking up Local only at the deadline would select a replacement pawn.
    g_killVictim.Set(local);
    // Sanitize the host-decided delay (clamp to a sane window so a corrupt packet can't park
    // the death forever or fire it instantly).
    uint32_t delay = p.killDelayMs;
    if (delay < 250u)  delay = 250u;
    if (delay > 8000u) delay = 8000u;
    g_killAtMs = NowMs() + delay;
    g_killWispEid = p.wispElementId;
    // Ride our local mirror's 'playerGrab' socket for the window -- the native Capture experience
    // (attach + MOVE_None + camera decouple), which grab_hold owns.
    coop::wisp_grab_hold::EngageSelf(p.wispElementId, delay);
    UE_LOGI("wisp_tear[victim]: WispGrab accepted (wispEid=%u) -- scheduling OUR ragdoll death in %u ms",
            p.wispElementId, delay);
}

// `wispActor` must arrive slot-validated (the caller passes el->LiveActor(); a fresh-null is a
// fine no-op) -- a bare IsLive here would probe a cached element actor.
bool PlayTearOnWisp(void* wispActor, uint32_t victimSlot) {
    if (!wispActor || !ue_wrap::wisp::IsKillerWisp(wispActor)) return false;
    ue_wrap::CachedObjRef ref; ref.Set(wispActor);
    // Force the parked mirror mesh to tick so a played montage advances, then play the fatality
    // tear. Both best-effort: the force-tick alone shows the mirror, the montage is the gore. The
    // limb gibs are still a follow-on; the victim's body placement belongs to wisp_grab_hold, which
    // both receivers engage.
    ue_wrap::wisp::ForceMeshTick(wispActor);
    if (!ref.Get()) return false;
    const bool montage = ue_wrap::wisp::PlayFatalityMontage(wispActor);
    UE_LOGI("wisp_tear: tear on wisp actor=%p (victimSlot=%u) -- force-tick + montage=%d",
            wispActor, victimSlot, montage ? 1 : 0);
    return montage && ref.Get();
}

namespace {
bool TryTear(const coop::net::WispTearPayload& p) {
    auto* el=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(p.wispElementId));
    void* actor=el ? el->LiveActor() : nullptr;
    if (!actor || !ue_wrap::wisp::IsKillerWisp(actor)) return false;
    const auto epoch=g_tearEpoch;
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    const bool played = PlayTearOnWisp(actor,p.victimSlot);
    if (epoch!=g_tearEpoch) return true;
    if (!ref.Get()) return false;
    if (p.victimSlot!=coop::players::Registry::Get().LocalPeerId())
        coop::wisp_grab_hold::EngagePuppet(p.wispElementId,static_cast<uint8_t>(p.victimSlot));
    return played;
}
}

void OnWispTear(const coop::net::WispTearPayload& p, uint8_t senderPeerSlot) {
    if (!p.wispElementId || p.victimSlot>=coop::players::kMaxPeers) return;
    if (senderPeerSlot != 0) {
        UE_LOGW("wisp_tear: WispTear from non-host slot=%u -- dropping", senderPeerSlot);
        return;
    }
    // Resolve the LOCAL wisp actor by its cross-peer Element id (a client mirror in
    // NpcMirrors, registered into the unified element Registry). null if the wisp already
    // despawned (race) -- then the tear is a no-op, which is fine.
    for (auto& row:g_pendingTears) if (row.payload.wispElementId==p.wispElementId) {
        row.payload=p; return;  // Do not bypass backoff or extend the original deadline.
    }
    const auto epoch = g_tearEpoch;
    if (TryTear(p) || epoch != g_tearEpoch) return;
    // The reliable attack can beat mirror materialization. Retain that one-shot
    // animation for the same 3.5s window as the host's native grab/death sequence.
    const auto now = NowMs();
    if (g_pendingTears.size()<coop::players::kMaxPeers)
        g_pendingTears.push_back({p,now+3500,now+kTearRetryMs});
}

void Tick() {
    const auto now=NowMs();
    const auto epoch=g_tearEpoch;
    auto pending=std::move(g_pendingTears); g_pendingTears.clear();
    for (auto row:pending) {
        if (epoch!=g_tearEpoch) return;
        if (now >= row.until) continue;
        if (now < row.nextTry) { g_pendingTears.push_back(row); continue; }
        if (!TryTear(row.payload) && epoch==g_tearEpoch) {
            row.nextTry = now + kTearRetryMs;
            g_pendingTears.push_back(row);
        }
    }
    if (g_killAtMs == 0) return;
    if (NowMs() < g_killAtMs) return;
    const auto wispEid = g_killWispEid;
    const auto victimRef = g_killVictim;
    void* local = victimRef.Get();
    g_killAtMs = 0;
    g_killWispEid = 0;
    g_killVictim.Reset();
    // The release can dispatch engine callbacks, including disconnect or pawn replacement.
    // Revalidate the original victim after it; never kill a successor pawn.
    // Undo the grab FIRST (detach + restore movement/camera) -- the native releasePlayer shape is
    // detach-then-ragdoll. Ahead of the liveness check below, and not after it: consuming the arm
    // and then returning would strand the ride, since ReleaseSelf is the only thing that clears
    // it in-session and the hold's own retry is gated on not-yet-attached. Safe no-op if the ride
    // never engaged, and it re-checks the local player itself.
    coop::wisp_grab_hold::ReleaseSelf();
    if (epoch != g_tearEpoch) return;
    if (!victimRef.Get() || local != coop::players::Registry::Get().Local()) {
        UE_LOGW("wisp_tear[victim]: kill deadline elapsed but local player not live -- skipping");
        return;
    }
    // The kill is a DIRECT ragdollMode(true,false,true), not kill(): kill() no-ops behind
    // immortal/startInvinc and ragdollMode does not. Native death -> menu -> rejoin, the settled
    // permadeath-rejoinable policy.
    const bool ok = ue_wrap::engine::SetMainPlayerRagdollMode(local, /*ragdoll=*/true,
                                                              /*passOut=*/false, /*death=*/true);
    UE_LOGI("wisp_tear[victim]: ragdoll DEATH fired (wispEid=%u) ok=%d -- native death->menu->rejoin",
            wispEid, ok ? 1 : 0);
}

void OnDisconnect() {
    ++g_tearEpoch;
    g_pendingTears.clear();
    // The armed death and the ride it came in with are one thing: a teardown inside the window
    // must not leave the local player detached-and-parked with nothing left to release it.
    g_killAtMs = 0;
    g_killWispEid = 0;
    g_killVictim.Reset();
    coop::wisp_grab_hold::ReleaseSelf();
}

}  // namespace coop::wisp_tear_mirror
