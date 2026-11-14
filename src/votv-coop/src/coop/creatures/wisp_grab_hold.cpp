// coop/creatures/wisp_grab_hold.cpp -- see coop/creatures/wisp_grab_hold.h.

#include "coop/creatures/wisp_grab_hold.h"

#include "coop/element/element.h"
#include "coop/element/registry.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/actors/wisp.h"
#include "ue_wrap/actors/vitals.h"

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace coop::wisp_grab_hold {
namespace {

namespace R = ue_wrap::reflection;
namespace E = ue_wrap::engine;

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// Victim-side self grab (game-thread only). eid 0 = not engaged. The retry window
// exists because the WispGrab reliable can beat the wisp mirror's spawn by a frame.
uint32_t g_selfWispEid     = 0;
bool     g_selfAttached    = false;
bool     g_selfAttempting  = false;
uint64_t g_selfRetryUntil  = 0;
uint64_t g_selfNextTry = 0;
constexpr uint64_t kSelfRetryMs = 100;
ue_wrap::CachedObjRef g_selfVictim;
ue_wrap::CachedObjRef g_selfSource;
uint64_t g_selfGeneration = 0;
uint64_t g_selfRequests = 0;
uint32_t g_selfWorld = 0;

// Cross-peer puppet holds (host: the real wisp; third peers: the mirror). Tiny set
// (at most one victim per wisp, at most kMaxPeers-1 victims).
struct Hold {
    uint8_t slot; uint32_t wispEid; uint32_t world;
    ue_wrap::CachedObjRef wisp, victim;
};
std::vector<Hold> g_holds;
uint64_t g_holdRevision = 0;

// Resolve the LOCAL actor for a wisp Element id (the real wisp on the host, the
// npc mirror on a client -- both live in the unified element Registry). Null if
// despawned / not yet spawned / not a killerwisp.
void* ResolveWispActor(uint32_t wispEid) {
    auto* el = coop::element::Registry::Get().Get(
        static_cast<coop::element::ElementId>(wispEid));
    if (!el) return nullptr;
    void* actor = el->LiveActor();  // slot-validated; never bare IsLive on a cached actor
    if (!actor || !ue_wrap::wisp::IsKillerWisp(actor)) return nullptr;
    return actor;
}

// One victim-side attach attempt. True once attached (latched by the caller).
bool TryAttachSelf() {
    const auto generation = g_selfGeneration;
    void* wisp = ResolveWispActor(g_selfWispEid);
    if (!wisp) return false;
    if (g_selfSource.Raw() && g_selfSource.Get()!=wisp) {ReleaseSelf();return false;}
    if (!g_selfSource.Raw()) g_selfSource.Set(wisp);
    const auto victim = g_selfVictim;
    void* local = victim.Get();
    if (!local || local != coop::players::Registry::Get().Local()) return false;
    g_selfAttempting=true;
    const auto current=[&]{
        if (generation!=g_selfGeneration || !victim.Get() ||
            g_selfWorld!=ue_wrap::world_identity::Generation() || local!=coop::players::Registry::Get().Local()) return false;
        float dead=0;
        return ue_wrap::vitals::Read(ue_wrap::vitals::Field::Dead,&dead) && dead==0 &&
            generation==g_selfGeneration && victim.Get() && g_selfWorld==ue_wrap::world_identity::Generation() &&
            local==coop::players::Registry::Get().Local();
    };
    const bool attached=ue_wrap::wisp::ApplyGrabToLocalPlayerWhile(wisp, local, current);
    if (generation!=g_selfGeneration) return false;
    if (!current()) {
        if (generation==g_selfGeneration) ReleaseSelf();
        return false;
    }
    g_selfAttempting=false;
    if (!attached) return false;
    // Native attachment can run teardown or install another hold. Only the
    // attempt that still owns this generation may publish its attached state.
    if (generation != g_selfGeneration || !victim.Get() ||
        local != coop::players::Registry::Get().Local()) return false;
    UE_LOGI("wisp_hold[self]: grabbed by wispEid=%u -- riding the mirror's 'playerGrab' "
            "socket until the scheduled death", g_selfWispEid);
    return true;
}

}  // namespace

void EngageSelf(uint32_t wispEid, uint32_t killDelayMs) {
    if (wispEid == 0) return;
    void* local = coop::players::Registry::Get().Local();
    if (!local || !R::IsLive(local)) return;
    const auto request=++g_selfRequests;
    const auto world=ue_wrap::world_identity::Generation();
    ue_wrap::CachedObjRef incoming;incoming.Set(local);
    // Retire any physical partial/complete capture before installing another.
    // Teardown or a newer request inside that cleanup wins over this caller.
    if (g_selfWispEid) ReleaseSelf();
    if (request!=g_selfRequests || world!=ue_wrap::world_identity::Generation() ||
        incoming.Get()!=local || local!=coop::players::Registry::Get().Local()) return;
    ++g_selfGeneration;
    g_selfVictim.Set(local);
    g_selfSource.Reset();
    g_selfWorld = ue_wrap::world_identity::Generation();
    g_selfWispEid    = wispEid;
    g_selfAttached   = false;
    g_selfAttempting = false;
    g_selfRetryUntil = NowMs() + killDelayMs;
    g_selfNextTry = NowMs() + kSelfRetryMs;
    if (TryAttachSelf()) g_selfAttached = true;
    // else: Tick retries until the deadline (mirror spawn race) -- the flat death
    // remains the floor if the mirror never resolves.
}

void EngagePuppet(uint32_t wispEid, uint8_t victimSlot) {
    if (wispEid == 0 || victimSlot >= coop::players::kMaxPeers) return;
    auto* rp = coop::players::Registry::Get().Puppet(victimSlot);
    void* victim = rp && rp->valid() && rp->GetHealth() > 0.f ? rp->GetActor() : nullptr;
    void* wisp = ResolveWispActor(wispEid);
    if (!victim || !R::IsLive(victim) || !wisp) return;
    Hold next{victimSlot, wispEid, ue_wrap::world_identity::Generation(), {}, {}};
    next.wisp.Set(wisp); next.victim.Set(victim);
    for (auto& h : g_holds) {
        if (h.slot == victimSlot) {
            // Repeated readiness attempts for an old attack cannot rebind it
            // to a successor pawn in the same peer slot.
            if (h.wispEid != wispEid) { h = next; ++g_holdRevision; }
            return;
        }
    }
    g_holds.push_back(next);
    ++g_holdRevision;
    UE_LOGI("wisp_hold: holding puppet slot=%u at wispEid=%u 'playerGrab' socket "
            "(releases when the wisp despawns)", victimSlot, wispEid);
}

void ReleaseSelf() {
    if (g_selfWispEid == 0) return;
    const bool attached = g_selfAttached || g_selfAttempting;
    const auto victim = g_selfVictim;
    const auto world = g_selfWorld;
    // Detach/movement calls may reenter teardown. Consume this hold first so a
    // nested release is a no-op and new state is not erased after the callback.
    g_selfWispEid = 0;
    g_selfAttached = false;
    g_selfAttempting = false;
    g_selfRetryUntil = 0;
    g_selfNextTry = 0;
    ++g_selfGeneration;
    const auto releaseGeneration=g_selfGeneration;
    g_selfVictim.Reset();
    g_selfSource.Reset();
    g_selfWorld = 0;
    if (attached) {
        // Release the pawn that was attached, even if Local now names its
        // replacement. A recycled slot must never receive native cleanup.
        if (world == ue_wrap::world_identity::Generation())
            if (void* local = victim.Get()) ue_wrap::wisp::ReleaseGrabOnLocalPlayerWhile(local,[&]{
                return releaseGeneration==g_selfGeneration && world==ue_wrap::world_identity::Generation() && victim.Get();
            });
    }
}

void Tick() {
    if (g_selfWispEid) {
        const auto generation = g_selfGeneration;
        const bool sameVictim = g_selfVictim.Get() &&
            g_selfVictim.Raw() == coop::players::Registry::Get().Local();
        float dead = 0;
        const bool alreadyDead = sameVictim && ue_wrap::vitals::Read(ue_wrap::vitals::Field::Dead, &dead) && dead != 0;
        if (generation != g_selfGeneration) return;
        if (!sameVictim || !g_selfVictim.Get() || g_selfVictim.Raw() != coop::players::Registry::Get().Local() ||
            alreadyDead || g_selfWorld != ue_wrap::world_identity::Generation() ||
            (g_selfSource.Raw() && (!g_selfSource.Get() || g_selfSource.Get()!=ResolveWispActor(g_selfWispEid)))) {
            ReleaseSelf();
            return;
        }
    }
    // Victim-side attach retry (mirror spawn race), bounded by the kill deadline.
    if (g_selfWispEid != 0 && !g_selfAttached) {
        const auto now=NowMs();
        if (now >= g_selfRetryUntil) {
            UE_LOGW("wisp_hold[self]: wisp mirror eid=%u never resolved inside the grab "
                    "window -- the flat death stands (no ride)", g_selfWispEid);
            ReleaseSelf();
        } else if (now >= g_selfNextTry) {
            g_selfNextTry=now+kSelfRetryMs;
            if (TryAttachSelf()) g_selfAttached = true;
        }
    }
    // Puppet holds: snap each held puppet to its wisp's socket. Runs AFTER the puppet
    // pose apply (puppet_drive::DriveTick ordering), so this write is the one the frame renders.
    if (g_holds.empty()) return;
    // Socket/location calls dispatch engine callbacks. Iterate a copy so teardown,
    // peer removal or vector growth cannot invalidate the active iterator.
    const auto holds = g_holds;
    const auto revision = g_holdRevision;
    for (const auto& hold : holds) {
        if (revision != g_holdRevision) return;
        const auto drop = [&] {
            std::erase_if(g_holds, [&](const Hold& h) { return h.slot == hold.slot; });
        };
        void* wisp = ResolveWispActor(hold.wispEid);
        if (!wisp || hold.world != ue_wrap::world_identity::Generation() || hold.wisp.Get() != wisp) {
            UE_LOGI("wisp_hold: wispEid=%u gone -- released puppet slot=%u (pose stream "
                    "resumes)", hold.wispEid, hold.slot);
            drop();
            continue;
        }
        coop::RemotePlayer* rp = coop::players::Registry::Get().Puppet(hold.slot);
        void* puppet = (rp && rp->valid()) ? rp->GetActor() : nullptr;
        if (!puppet || hold.victim.Get() != puppet || !(rp->GetHealth() > 0.f)) {
            UE_LOGI("wisp_hold: puppet slot=%u gone -- dropping its hold", hold.slot);
            drop();
            continue;
        }
        ue_wrap::FVector sock{};
        ue_wrap::CachedObjRef wispRef; wispRef.Set(wisp);
        ue_wrap::CachedObjRef puppetRef; puppetRef.Set(puppet);
        if (!ue_wrap::wisp::GrabSocketWorldLocation(wisp, sock)) continue;
        if (revision != g_holdRevision) return;
        if (!wispRef.Get() || !puppetRef.Get() || hold.world != ue_wrap::world_identity::Generation()) continue;
        rp = coop::players::Registry::Get().Puppet(hold.slot);
        if (!rp || !rp->valid() || rp->GetActor() != puppet || !(rp->GetHealth() > 0.f)) continue;
        E::SetActorLocation(puppet, sock);
    }
}

void OnPeerLeft(uint8_t slot) {
    for (auto it = g_holds.begin(); it != g_holds.end();) {
        if (it->slot == slot) {
            ++g_holdRevision;
            UE_LOGI("wisp_hold: peer slot=%u left -- dropping its hold", slot);
            it = g_holds.erase(it);
        } else {
            ++it;
        }
    }
}

void OnDisconnect() {
    ++g_selfRequests;
    ++g_holdRevision;
    g_holds.clear();
    ReleaseSelf();  // a mid-grab teardown must not strand the local player in MOVE_None
}

}  // namespace coop::wisp_grab_hold
