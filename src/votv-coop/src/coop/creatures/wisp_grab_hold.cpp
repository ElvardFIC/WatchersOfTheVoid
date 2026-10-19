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
uint64_t g_selfRetryUntil  = 0;
ue_wrap::CachedObjRef g_selfVictim;
uint64_t g_selfGeneration = 0;
uint32_t g_selfWorld = 0;

// Cross-peer puppet holds (host: the real wisp; third peers: the mirror). Tiny set
// (at most one victim per wisp, at most kMaxPeers-1 victims).
struct Hold { uint8_t slot; uint32_t wispEid; };
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
    const auto victim = g_selfVictim;
    void* local = victim.Get();
    if (!local || local != coop::players::Registry::Get().Local()) return false;
    if (!ue_wrap::wisp::ApplyGrabToLocalPlayer(wisp, local)) return false;
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
    ++g_selfGeneration;
    g_selfVictim.Set(local);
    g_selfWorld = ue_wrap::world_identity::Generation();
    g_selfWispEid    = wispEid;
    g_selfAttached   = false;
    g_selfRetryUntil = NowMs() + killDelayMs;
    if (TryAttachSelf()) g_selfAttached = true;
    // else: Tick retries until the deadline (mirror spawn race) -- the flat death
    // remains the floor if the mirror never resolves.
}

void EngagePuppet(uint32_t wispEid, uint8_t victimSlot) {
    if (wispEid == 0 || victimSlot >= coop::players::kMaxPeers) return;
    for (auto& h : g_holds) {
        if (h.slot == victimSlot) {
            if (h.wispEid != wispEid) { h.wispEid = wispEid; ++g_holdRevision; }
            return;
        }
    }
    g_holds.push_back(Hold{victimSlot, wispEid});
    ++g_holdRevision;
    UE_LOGI("wisp_hold: holding puppet slot=%u at wispEid=%u 'playerGrab' socket "
            "(releases when the wisp despawns)", victimSlot, wispEid);
}

void ReleaseSelf() {
    if (g_selfWispEid == 0) return;
    const bool attached = g_selfAttached;
    const auto victim = g_selfVictim;
    const auto world = g_selfWorld;
    // Detach/movement calls may reenter teardown. Consume this hold first so a
    // nested release is a no-op and new state is not erased after the callback.
    g_selfWispEid = 0;
    g_selfAttached = false;
    g_selfRetryUntil = 0;
    ++g_selfGeneration;
    g_selfVictim.Reset();
    g_selfWorld = 0;
    if (attached) {
        // Release the pawn that was attached, even if Local now names its
        // replacement. A recycled slot must never receive native cleanup.
        if (world == ue_wrap::world_identity::Generation())
            if (void* local = victim.Get()) ue_wrap::wisp::ReleaseGrabOnLocalPlayer(local);
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
            alreadyDead || g_selfWorld != ue_wrap::world_identity::Generation()) {
            ReleaseSelf();
            return;
        }
    }
    // Victim-side attach retry (mirror spawn race), bounded by the kill deadline.
    if (g_selfWispEid != 0 && !g_selfAttached) {
        if (NowMs() > g_selfRetryUntil) {
            UE_LOGW("wisp_hold[self]: wisp mirror eid=%u never resolved inside the grab "
                    "window -- the flat death stands (no ride)", g_selfWispEid);
            g_selfWispEid = 0;
            g_selfRetryUntil = 0;
            g_selfVictim.Reset();
            g_selfWorld = 0;
        } else if (TryAttachSelf()) {
            g_selfAttached = true;
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
        if (!wisp) {
            UE_LOGI("wisp_hold: wispEid=%u gone -- released puppet slot=%u (pose stream "
                    "resumes)", hold.wispEid, hold.slot);
            drop();
            continue;
        }
        coop::RemotePlayer* rp = coop::players::Registry::Get().Puppet(hold.slot);
        void* puppet = (rp && rp->valid()) ? rp->GetActor() : nullptr;
        if (!puppet || !R::IsLive(puppet)) {
            UE_LOGI("wisp_hold: puppet slot=%u gone -- dropping its hold", hold.slot);
            drop();
            continue;
        }
        ue_wrap::FVector sock{};
        ue_wrap::CachedObjRef wispRef; wispRef.Set(wisp);
        ue_wrap::CachedObjRef puppetRef; puppetRef.Set(puppet);
        if (!ue_wrap::wisp::GrabSocketWorldLocation(wisp, sock)) continue;
        if (revision != g_holdRevision) return;
        if (!wispRef.Get() || !puppetRef.Get()) continue;
        rp = coop::players::Registry::Get().Puppet(hold.slot);
        if (!rp || !rp->valid() || rp->GetActor() != puppet) continue;
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
    ++g_holdRevision;
    g_holds.clear();
    ReleaseSelf();  // a mid-grab teardown must not strand the local player in MOVE_None
}

}  // namespace coop::wisp_grab_hold
