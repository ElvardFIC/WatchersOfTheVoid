// coop/creatures/wisp_tear_mirror.cpp -- see coop/creatures/wisp_tear_mirror.h.

#include "coop/creatures/wisp_tear_mirror.h"

#include "coop/creatures/wisp_grab_hold.h"
#include "coop/element/element.h"
#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/actors/wisp.h"
#include "ue_wrap/actors/vitals.h"

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <functional>
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
uint32_t g_killWorld = 0;
uint64_t g_killRevision = 0;
constexpr uint64_t kTearRetryMs = 100;
struct PendingTear {
    coop::net::WispTearPayload payload{};
    uint64_t until=0, nextTry=0, token=0;
    uint32_t world=0;
    bool localVictim=false;
    ue_wrap::CachedObjRef wisp, victim;
};
std::vector<PendingTear> g_pendingTears;
uint64_t g_tearEpoch=0;
uint64_t g_nextTearToken=0;
bool g_drainingTears=false;

}  // namespace

void OnWispGrab(const coop::net::WispGrabPayload& p, uint8_t senderPeerSlot) {
    if (senderPeerSlot != 0 || !p.wispElementId || !p.victimElementId) return;
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
    g_killWorld = ue_wrap::world_identity::Generation();
    ++g_killRevision;
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
namespace {
bool PlayTearWhile(void* wispActor, uint32_t victimSlot, const std::function<bool()>& current) {
    if (!wispActor || !ue_wrap::wisp::IsKillerWisp(wispActor)) return false;
    ue_wrap::CachedObjRef ref; ref.Set(wispActor);
    const auto world = ue_wrap::world_identity::Generation();
    // Force the parked mirror mesh to tick so a played montage advances, then play the fatality
    // tear. Both best-effort: the force-tick alone shows the mirror, the montage is the gore. The
    // limb gibs are still a follow-on; the victim's body placement belongs to wisp_grab_hold, which
    // both receivers engage.
    if (!ue_wrap::wisp::ForceMeshTick(wispActor)) return false;
    if (!ref.Get() || world != ue_wrap::world_identity::Generation() || (current && !current())) return false;
    const bool montage = ue_wrap::wisp::PlayFatalityMontage(wispActor);
    UE_LOGI("wisp_tear: tear on wisp actor=%p (victimSlot=%u) -- force-tick + montage=%d",
            wispActor, victimSlot, montage ? 1 : 0);
    return montage && ref.Get() && world == ue_wrap::world_identity::Generation() && (!current || current());
}
}

bool PlayTearOnWisp(void* wispActor, uint32_t victimSlot) {
    return PlayTearWhile(wispActor, victimSlot, {});
}

namespace {
bool OwnsTear(const PendingTear& row) {
    return row.world==ue_wrap::world_identity::Generation() &&
        std::any_of(g_pendingTears.begin(),g_pendingTears.end(),[&](const PendingTear& live){return live.token==row.token;});
}
enum class VictimState {Waiting, Alive, Cancelled};
VictimState BindVictim(PendingTear& row) {
    auto& players=coop::players::Registry::Get();
    if (row.localVictim != (row.payload.victimSlot==players.LocalPeerId())) return VictimState::Cancelled;
    auto* puppet=row.localVictim?nullptr:players.Puppet(static_cast<uint8_t>(row.payload.victimSlot));
    void* victim=row.localVictim?players.Local():puppet&&puppet->valid()?puppet->GetActor():nullptr;
    if (row.victim.Raw() && (!victim || row.victim.Get()!=victim)) return VictimState::Cancelled;
    if (!victim || !R::IsLive(victim)) return VictimState::Waiting;
    // Body identity is known even while vitals/animation metadata is not.
    // Preserve it now so a readiness retry cannot adopt a respawned pawn.
    if (!row.victim.Raw()) row.victim.Set(victim);
    if (!row.localVictim && !(puppet->GetHealth()>0.f)) return VictimState::Cancelled;
    if (row.localVictim) {
        float dead=0;
        if (!ue_wrap::vitals::Read(ue_wrap::vitals::Field::Dead,&dead)) return VictimState::Waiting;
        if (dead!=0) return VictimState::Cancelled;
    }
    if (row.victim.Get()!=victim || (row.localVictim && players.Local()!=victim)) return VictimState::Cancelled;
    return VictimState::Alive;
}
bool TryTear(PendingTear& row) {
    if (!OwnsTear(row)) return true;
    const auto p=row.payload;
    const auto epoch=g_tearEpoch;
    const auto victim=BindVictim(row);
    if (epoch!=g_tearEpoch || !OwnsTear(row)) return true;
    if (victim!=VictimState::Alive) return victim==VictimState::Cancelled;
    auto* el=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(p.wispElementId));
    void* actor=el ? el->LiveActor() : nullptr;
    if (row.wisp.Raw() && (!row.wisp.Get() || row.wisp.Get()!=actor)) return true;
    if (!actor || !ue_wrap::wisp::IsKillerWisp(actor)) return false;
    if (!row.wisp.Raw()) row.wisp.Set(actor);
    const auto world=ue_wrap::world_identity::Generation();
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    const auto current=[&] {
        if (epoch!=g_tearEpoch || !OwnsTear(row) || row.wisp.Get()!=actor) return false;
        return BindVictim(row)==VictimState::Alive && epoch==g_tearEpoch &&
            OwnsTear(row) && row.wisp.Get()==actor;
    };
    const bool played = PlayTearWhile(actor,p.victimSlot,current);
    if (epoch!=g_tearEpoch || world!=ue_wrap::world_identity::Generation()) return true;
    if (BindVictim(row)!=VictimState::Alive || epoch!=g_tearEpoch || !OwnsTear(row)) return true;
    el=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(p.wispElementId));
    if (!ref.Get() || !el || el->LiveActor()!=actor) return true;
    if (p.victimSlot!=coop::players::Registry::Get().LocalPeerId())
        coop::wisp_grab_hold::EngagePuppet(p.wispElementId,static_cast<uint8_t>(p.victimSlot));
    return played;
}

void DrainTears(uint64_t now) {
    if (g_drainingTears) return;
    g_drainingTears=true;
    struct Guard {~Guard(){g_drainingTears=false;}} guard;
    const auto epoch=g_tearEpoch;
    // Keep the admitted rows visible during native calls: a reentrant receive
    // sees its existing request instead of recursively replaying the montage.
    const auto pending=g_pendingTears;
    for (auto row:pending) {
        if (epoch!=g_tearEpoch) return;
        if (now<row.nextTry && now<row.until && row.world==ue_wrap::world_identity::Generation()) continue;
        const bool done=now>=row.until || row.world!=ue_wrap::world_identity::Generation() || TryTear(row);
        if (epoch!=g_tearEpoch) return;
        const auto it=std::find_if(g_pendingTears.begin(),g_pendingTears.end(),[&](const PendingTear& live){return live.token==row.token;});
        if (it==g_pendingTears.end()) continue;
        if (done) g_pendingTears.erase(it);
        else {row.nextTry=now+kTearRetryMs;*it=row;}
    }
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
    const auto world = ue_wrap::world_identity::Generation();
    std::erase_if(g_pendingTears, [=](const PendingTear& row){return row.world != world;});
    for (auto& row:g_pendingTears) if (row.payload.wispElementId==p.wispElementId) {
        return;  // Do not retarget, bypass backoff or extend the original deadline.
    }
    // The reliable attack can beat mirror materialization. Retain that one-shot
    // animation for the same 3.5s window as the host's native grab/death sequence.
    const auto now = NowMs();
    if (g_pendingTears.size()>=coop::players::kMaxPeers) return;
    PendingTear row;
    row.payload=p;row.until=now+3500;row.nextTry=now;row.world=world;
    row.token=++g_nextTearToken;
    row.localVictim=p.victimSlot==coop::players::Registry::Get().LocalPeerId();
    g_pendingTears.push_back(row);
    DrainTears(now);
}

void Tick() {
    if (g_drainingTears) return;
    const auto now=NowMs();
    const auto epoch=g_tearEpoch;
    DrainTears(now);
    if (epoch!=g_tearEpoch) return;
    if (g_killAtMs == 0) return;
    float dead = 0;
    const bool sameVictim = g_killVictim.Get() &&
        g_killVictim.Raw() == coop::players::Registry::Get().Local();
    const auto revision = g_killRevision;
    const bool alreadyDead = sameVictim && ue_wrap::vitals::Read(ue_wrap::vitals::Field::Dead, &dead) && dead != 0;
    if (epoch != g_tearEpoch || revision != g_killRevision) return;
    if (!sameVictim || !g_killVictim.Get() || g_killVictim.Raw() != coop::players::Registry::Get().Local() ||
        alreadyDead || g_killWorld != ue_wrap::world_identity::Generation()) {
        g_killAtMs = 0; g_killWispEid = 0; g_killWorld = 0;
        g_killVictim.Reset(); ++g_killRevision;
        coop::wisp_grab_hold::ReleaseSelf();
        return;
    }
    if (NowMs() < g_killAtMs) return;
    const auto wispEid = g_killWispEid;
    const auto victimRef = g_killVictim;
    const auto killWorld = g_killWorld;
    void* local = victimRef.Get();
    g_killAtMs = 0;
    g_killWispEid = 0;
    g_killVictim.Reset();
    g_killWorld = 0;
    const auto consumed = ++g_killRevision;
    // The release can dispatch engine callbacks, including disconnect or pawn replacement.
    // Revalidate the original victim after it; never kill a successor pawn.
    // Undo the grab FIRST (detach + restore movement/camera) -- the native releasePlayer shape is
    // detach-then-ragdoll. Ahead of the liveness check below, and not after it: consuming the arm
    // and then returning would strand the ride, since ReleaseSelf is the only thing that clears
    // it in-session and the hold's own retry is gated on not-yet-attached. Safe no-op if the ride
    // never engaged, and it re-checks the local player itself.
    coop::wisp_grab_hold::ReleaseSelf();
    if (epoch != g_tearEpoch || consumed != g_killRevision) return;
    if (killWorld != ue_wrap::world_identity::Generation() || !victimRef.Get() ||
        local != coop::players::Registry::Get().Local()) {
        UE_LOGW("wisp_tear[victim]: kill deadline elapsed but local player not live -- skipping");
        return;
    }
    // Cleanup itself may dispatch death, another grab or a world transition.
    // Recheck after that last native callback before admitting the old kill.
    float releasedDead=0;
    const bool alive=ue_wrap::vitals::Read(ue_wrap::vitals::Field::Dead,&releasedDead) && releasedDead==0;
    if (!alive || epoch!=g_tearEpoch || consumed!=g_killRevision ||
        killWorld!=ue_wrap::world_identity::Generation() || !victimRef.Get() ||
        local!=coop::players::Registry::Get().Local()) return;
    // The kill is a DIRECT ragdollMode(true,false,true), not kill(): kill() no-ops behind
    // immortal/startInvinc and ragdollMode does not. Native death -> menu -> rejoin, the settled
    // permadeath-rejoinable policy.
    const bool ok = ue_wrap::engine::SetMainPlayerRagdollMode(local, /*ragdoll=*/true,
                                                              /*passOut=*/false, /*death=*/true);
    UE_LOGI("wisp_tear[victim]: ragdoll DEATH fired (wispEid=%u) ok=%d -- native death->menu->rejoin",
            wispEid, ok ? 1 : 0);
}

void OnPeerLeft(uint8_t slot) {
    std::erase_if(g_pendingTears,[=](const PendingTear& row){return row.payload.victimSlot==slot;});
}

void OnDisconnect() {
    ++g_tearEpoch;
    g_pendingTears.clear();
    // The armed death and the ride it came in with are one thing: a teardown inside the window
    // must not leave the local player detached-and-parked with nothing left to release it.
    g_killAtMs = 0;
    g_killWispEid = 0;
    g_killVictim.Reset();
    g_killWorld = 0;
    ++g_killRevision;
    coop::wisp_grab_hold::ReleaseSelf();
}

}  // namespace coop::wisp_tear_mirror
