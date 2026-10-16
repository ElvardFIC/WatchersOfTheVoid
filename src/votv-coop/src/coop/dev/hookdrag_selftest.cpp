// coop/dev/hookdrag_selftest.cpp -- see coop/dev/hookdrag_selftest.h.

#include "coop/dev/hookdrag_selftest.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"            // CollectHandAxisActors: the hand is not a world prop
#include "coop/player/players_registry.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_key_index.h"  // Find: the world prop a key names
#include "coop/props/remote_prop.h"           // ResolveMirrorEidByActor: the eid a client's copy carries
#include "coop/player/remote_player.h"
#include "coop/props/prop_snapshot.h"  // IsBracketClosed: the host's end of a join
#include "coop/session/join_progress.h"  // CurrentPhase: the client's end of a join
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady
#include "ue_wrap/actors/hook.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_nav.h"  // AddMovementInput: the walk is real input, never a teleport

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

namespace coop::dev::hookdrag_selftest {
namespace {

namespace E  = ue_wrap::engine;
namespace H  = ue_wrap::hook;
namespace PR = ue_wrap::prop;
namespace R  = ue_wrap::reflection;

std::atomic<coop::net::Session*> g_session{nullptr};

bool Enabled() {
    static const bool s = coop::config::ResolveFlag(::coop::config_registry::rows::hookdrag_selftest);
    return s;
}

// Who plants: the host (the driven-prop channel alone), the client (the host ties the client's
// hook on its mirror and the channel carries the result back), or both into the same prop (two
// host constraints on one body). Read once; the same value is set on both peers.
const std::string& Role() {
    static const std::string s = coop::config::ResolveEnum(::coop::config_registry::rows::hookdrag_role);
    return s;
}
bool PlantsHere(bool isHost) {
    const std::string& r = Role();
    if (r == "both") return true;
    return (r == "client") ? !isHost : isHost;
}

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// The phases, each ended by what happened and not by a count of seconds. The origin is the first
// tick at which BOTH peers are in the world, the shared point the disc driver uses and for the same
// reason: the two connect clocks sit about ten seconds apart. Every phase has a bound, the backstop
// that turns a phase that never comes into a FAIL line naming it; no bound is a schedule.
//   planter: Pick -> Plant -> WalkOut -> WalkBack -> Unhook -> Rest -> Ended
//   other:   Pick -> Drag -> Rest -> Ended
enum class Phase { Pick, Plant, WalkOut, WalkBack, Unhook, Drag, Rest, Ended };

constexpr uint64_t kSampleMs     =   250;   // the position log, both roles
constexpr float    kPickRadiusCm =  1500.f;
// The host-role runs on record walked 1199..1211 cm in the 3 s the old schedule gave the leg out.
constexpr float    kWalkCm       =  1200.f; // the walker is this far from where it planted: the leg out is over
constexpr float    kBackCm       =   100.f; // ... and this close to it again: the leg back is over (the runs
                                            // on record ended 7..22 cm from it)
constexpr float    kMovedCm      =   100.f; // the prop stood this far from its start: a drag happened (the
                                            // distance the driver's verdict counts a drag by)
constexpr float    kRestCm       =     5.f; // a sample this close to the one before it is a prop at rest ...
constexpr int      kRestSamples  =       6; // ... and this many in a row (1.5 s) end the run
// The backstops. Plant and Unhook are one dispatch each: their event arrives in the tick they start.
constexpr uint64_t kPickBoundMs  =  60000;  // the join is over and the anchor reads
constexpr uint64_t kWalkBoundMs  =  20000;  // one leg of the walk
constexpr uint64_t kDragBoundMs  =  90000;  // the peer that does not plant: the other's pick, plant and walk out
constexpr uint64_t kRestBoundMs  =  60000;

uint64_t g_originMs     = 0;
uint64_t g_nextSampleMs = 0;
uint64_t g_phaseMs      = 0;                // when the current phase began
Phase    g_phase = Phase::Pick;
int      g_restRun = 0;                     // consecutive samples within kRestCm of the one before
const char* g_waitWhy = "";                 // what the current phase is waiting on, for its FAIL line
float    g_waitCm = -1.f;                   // the last distance it read while waiting, or -1
bool     g_havePrev = false;
ue_wrap::FVector g_prevLoc{};
ue_wrap::CachedObjRef g_target;
ue_wrap::CachedObjRef g_hook;
std::wstring          g_targetKey;
ue_wrap::FVector      g_targetStart{};
ue_wrap::FVector      g_walkBase{}, g_walkDir{};
float                 g_maxMovedCm = 0.f;
float                 g_goneAtS    = -1.f;   // when the target died under the sampler, or -1
float                 g_unreadAtS  = -1.f;   // when the target's location stopped reading, or -1
const char*           g_plantNote  = "not attempted";

const char* PhaseName(Phase p) {
    switch (p) {
    case Phase::Pick:     return "pick";
    case Phase::Plant:    return "plant";
    case Phase::WalkOut:  return "walk out";
    case Phase::WalkBack: return "walk back";
    case Phase::Unhook:   return "unhook";
    case Phase::Drag:     return "drag";
    case Phase::Rest:     return "rest";
    case Phase::Ended:    return "ended";
    }
    return "?";
}

// 0: the phase waits on nothing, so it has no bound.
uint64_t BoundMs(Phase p) {
    switch (p) {
    case Phase::Pick:     return kPickBoundMs;
    case Phase::WalkOut:
    case Phase::WalkBack: return kWalkBoundMs;
    case Phase::Drag:     return kDragBoundMs;
    case Phase::Rest:     return kRestBoundMs;
    default:              return 0;
    }
}

void EnterPhase(Phase p, uint64_t now) {
    g_phase = p;
    g_phaseMs = now;
    g_waitWhy = "";
    g_waitCm = -1.f;
}

void* LocalPlayer() {
    void* p = coop::players::Registry::Get().Local();
    return (p && R::IsLive(p)) ? p : nullptr;
}

// Where both roles look for the target: the host at its own player, the client at the host's
// puppet, which stands where the host player does.
bool Anchor(bool isHost, ue_wrap::FVector& out, const char*& why) {
    why = "its location could not be read";
    if (isHost) {
        void* p = LocalPlayer();
        if (!p) { why = "the host player is not here yet"; return false; }
        return E::TryGetActorLocation(p, out);
    }
    coop::RemotePlayer* pup = coop::players::Registry::Get().Puppet(0);
    if (!pup || !pup->valid() || !pup->GetActor()) { why = "the host puppet is not here yet"; return false; }
    return E::TryGetActorLocation(pup->GetActor(), out);
}

// The nearest keyed, simulating, light Aprop_C to `anchor` within the radius: one object-array
// walk, once per run, the nearest-prop shape with the static, frozen and heavy props left out,
// since a hook drags none of them cleanly. Two more exclusions make it a WORLD prop: the hand
// axis (the local hotbar hand and every peer's display mirror stand at a player's hands and
// follow the player whether or not anything drags them), and membership in the tracked-prop key
// index, which is what the census gives a world prop and withholds from a hand actor. Both roles
// run the same rule over the same world, and the log carries the key so the driver can tell when
// they disagreed.
void* PickTarget(const ue_wrap::FVector& anchor, std::wstring& keyOut, ue_wrap::FVector& locOut) {
    void* best = nullptr;
    float bestD2 = kPickRadiusCm * kPickRadiusCm;
    void* handAxis[1 + coop::players::kMaxPeers];
    const size_t handAxisN =
        coop::hand_item::CollectHandAxisActors(handAxis, 1 + coop::players::kMaxPeers);
    const int32_t n = R::NumObjects();
    for (int32_t i = 0; i < n; ++i) {
        void* obj = R::ObjectAt(i);
        if (!obj || !PR::IsDescendantOfProp(obj)) continue;
        bool onHand = false;
        for (size_t h = 0; h < handAxisN; ++h) if (handAxis[h] == obj) { onHand = true; break; }
        if (onHand) continue;
        const std::wstring nm = R::ToString(R::NameOf(obj));
        if (nm.rfind(L"Default__", 0) == 0) continue;
        if (PR::IsStatic(obj) || PR::IsFrozen(obj) || PR::IsHeavy(obj)) continue;
        const std::wstring key = PR::GetInteractableKeyString(obj);
        if (key.empty() || key == L"None") continue;
        if (coop::prop_key_index::Find(key) != obj) continue;  // not the world prop its key names
        ue_wrap::FVector loc{};
        if (!E::TryGetActorLocation(obj, loc)) continue;   // unreadable: never the target
        const float dx = loc.X - anchor.X, dy = loc.Y - anchor.Y, dz = loc.Z - anchor.Z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < bestD2) { bestD2 = d2; best = obj; keyOut = key; locOut = loc; }
    }
    return best;
}

// The item's own plant, by hand: a hook spawned at the prop, attach_a with the head on the prop's
// mesh and the tail on the player, then the field the item writes so the owner lane adopts it.
//
// The hook is spawned as a MIRROR, not a canonical one: a canonical hook is a save participant,
// and the game's own autosave and save-on-death would write a driver-planted one into the slot,
// where it outlives the run. A mirror is kept out of the save and has its tick disabled, and a
// drag needs neither: the constraint attach_a builds between the prop's mesh and the player's
// root is independent of both.
bool Plant(void* player, void* target) {
    void* mesh = PR::GetStaticMesh(target);
    if (!mesh) { g_plantNote = "no-static-mesh"; return false; }
    ue_wrap::FVector loc{};
    if (!E::TryGetActorLocation(target, loc)) { g_plantNote = "target-location-unread"; return false; }
    void* hook = H::SpawnMirror(H::Kind::Hook, loc, ue_wrap::FRotator{});
    if (!hook) { g_plantNote = "hook-did-not-spawn"; return false; }
    if (!H::AttachHead(hook, target, mesh, ue_wrap::FVector{loc.X, loc.Y, loc.Z + 20.f},
                       ue_wrap::FVector{0.f, 0.f, 1.f}, player, /*checkLen=*/true,
                       /*unfreezeFrozen=*/false)) {
        E::DestroyActor(hook);
        g_plantNote = "attach_a-did-not-dispatch";
        return false;
    }
    H::WriteActiveHook(player, hook);
    g_hook.Set(hook);
    g_plantNote = "planted";
    return true;
}

// The shared clock the two peers' samples are aligned on: both run on one machine, so the system
// clock is common to the millisecond, where each peer's own origin is a second apart.
uint64_t WallMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

void Sample(uint64_t since, bool isHost) {
    void* t = g_target.Get();
    if (!t) {
        // Said once; the sampling stops with the target, and the verdict carries the time.
        g_goneAtS = static_cast<float>(since / 1000.0);
        UE_LOGW("hookdrag_selftest: POS t=%.2f key='%ls' -- the target is GONE; sampling stops",
                since / 1000.0, g_targetKey.c_str());
        return;
    }
    ue_wrap::FVector loc{};
    if (!E::TryGetActorLocation(t, loc)) {
        // Said once, like a gone target: the read repeats, so sampling stops and the verdict carries the time.
        g_unreadAtS = static_cast<float>(since / 1000.0);
        UE_LOGW("hookdrag_selftest: POS t=%.2f key='%ls' -- the target's location could not be read; sampling stops",
                since / 1000.0, g_targetKey.c_str());
        return;
    }
    const float dx = loc.X - g_targetStart.X, dy = loc.Y - g_targetStart.Y, dz = loc.Z - g_targetStart.Z;
    const float moved = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (moved > g_maxMovedCm) g_maxMovedCm = moved;
    // The rest the last phase waits on: samples in a row that stood where the one before did.
    if (g_havePrev) {
        const float px = loc.X - g_prevLoc.X, py = loc.Y - g_prevLoc.Y, pz = loc.Z - g_prevLoc.Z;
        g_restRun = std::sqrt(px * px + py * py + pz * pz) < kRestCm ? g_restRun + 1 : 0;
    }
    g_prevLoc = loc;
    g_havePrev = true;
    // The walker's own position beside the prop's, so a drag that stops short can be read as the
    // player stopping (a wall, or the hook's own pull on the player winning over the walk) rather
    // than guessed. The local player on both peers: on the non-planting peer it is a bystander.
    ue_wrap::FVector me{};
    void* self = LocalPlayer();
    const bool meRead = self && E::TryGetActorLocation(self, me);
    UE_LOGI("hookdrag_selftest: POS t=%.2f wall=%llu key='%ls' (%.1f,%.1f,%.1f) moved=%.1f role=%s "
            "me=(%.1f,%.1f,%.1f)%s",
            since / 1000.0, static_cast<unsigned long long>(WallMs()), g_targetKey.c_str(),
            loc.X, loc.Y, loc.Z, moved, isHost ? "HOST" : "CLIENT", me.X, me.Y, me.Z, meRead ? "" : " (unread)");
}

// One tick of the host's walk: movement INPUT, re-issued every tick, which the character movement
// consumes -- walls stop the player and the floor carries it, where a teleported step can land the
// player inside geometry.
void WalkStep(bool outward) {
    void* p = LocalPlayer();
    if (!p) return;
    const float s = outward ? 1.f : -1.f;
    E::AddMovementInput(p, ue_wrap::FVector{g_walkDir.X * s, g_walkDir.Y * s, 0.f}, 1.0f, true);
}


// Is this peer's end of the join over: the host's bracket for the joiner closed, the client's join
// progress idle. The pick reads props and a puppet the join delivers, so it waits for the join.
bool JoinOver(bool isHost) {
    return isHost ? coop::prop_snapshot::IsBracketClosed(1)
                  : coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle;
}

// The local player's horizontal distance from where the walk began; false while its location is unread.
bool WalkerDistance(float& out) {
    void* p = LocalPlayer();
    ue_wrap::FVector me{};
    if (!p || !E::TryGetActorLocation(p, me)) return false;
    const float dx = me.X - g_walkBase.X, dy = me.Y - g_walkBase.Y;
    out = std::sqrt(dx * dx + dy * dy);
    return true;
}

// Where the leg out heads: away from the prop in the horizontal plane; straight +X if the two
// coincide, there is no prop, or the prop's position is unread. False when the walker's own
// position is unread: a walk with no base cannot be measured.
bool BeginWalkOut(bool isHost) {
    void* p = LocalPlayer();
    void* t = g_target.Get();
    if (!p || !E::TryGetActorLocation(p, g_walkBase)) return false;
    ue_wrap::FVector tl{};
    const bool tlRead = t && E::TryGetActorLocation(t, tl);
    float dx = 1.f, dy = 0.f;
    if (tlRead) {
        dx = g_walkBase.X - tl.X; dy = g_walkBase.Y - tl.Y;
        const float n = std::sqrt(dx * dx + dy * dy);
        if (n > 1.f) { dx /= n; dy /= n; } else { dx = 1.f; dy = 0.f; }
    }
    // With both peers pulling, the client walks at a right angle to the host: two
    // players standing together would otherwise drag along one line, and a tug in
    // opposite directions leaves the prop where it was, which a drag verdict cannot
    // tell from a channel that carried nothing.
    if (!isHost && Role() == "both") { const float t2 = dx; dx = -dy; dy = t2; }
    g_walkDir = ue_wrap::FVector{dx, dy, 0.f};
    UE_LOGI("hookdrag_selftest: WALK OUT to %.0f cm along (%.2f,%.2f) from (%.1f,%.1f,%.1f)", kWalkCm, dx, dy,
            g_walkBase.X, g_walkBase.Y, g_walkBase.Z);
    return true;
}

// The run is over for this peer: the verdict line the driver waits on, said once.
void End() {
    g_phase = Phase::Ended;
    EmitVerdict();
}

// A phase did not come, or could not be measured: the run ends at it, naming it.
void Fail(bool isHost, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    UE_LOGW("hookdrag_selftest: FAIL role=%s phase=%s -- %s", isHost ? "HOST" : "CLIENT", PhaseName(g_phase), buf);
    End();
}

}  // namespace

void Install(coop::net::Session* session) {
    if (!Enabled()) return;
    g_session.store(session, std::memory_order_release);
    // The subsystem install list is a per-tick ensure, so the line is said once.
    static bool sSaid = false;
    if (sSaid) return;
    sSaid = true;
    UE_LOGI("hookdrag_selftest: ENABLED (role=%s) -- the planting peer hooks the nearest keyed "
            "physics prop to the host and drags it; both peers log the prop's position",
            Role().c_str());
}

void Tick() {
    if (!Enabled()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || g_phase == Phase::Ended) return;
    const bool isHost = s->role() == coop::net::Role::Host;
    const uint64_t now = NowMs();
    if (!g_originMs) {
        // Only a CLIENT announces ClientWorldReady; the host learns the same fact as that slot
        // going world-ready.
        if (isHost ? !s->IsSlotWorldReady(1) : !coop::net_pump::HasAnnouncedWorldReady()) return;
        g_originMs = now;
        EnterPhase(Phase::Pick, now);
        UE_LOGI("hookdrag_selftest: ARMED role=%s plants=%d (each phase ends on its event: pick, plant, "
                "walk %.0f cm out and back to within %.0f cm, unhook, rest)", isHost ? "HOST" : "CLIENT",
                PlantsHere(isHost) ? 1 : 0, kWalkCm, kBackCm);
    }
    const uint64_t since = now - g_originMs;
    if (const uint64_t bound = BoundMs(g_phase); bound && now - g_phaseMs > bound) {
        if (g_waitCm >= 0.f)
            return Fail(isHost, "no event within %llu s: %s (last read %.0f cm)",
                        static_cast<unsigned long long>(bound / 1000), g_waitWhy, g_waitCm);
        return Fail(isHost, "no event within %llu s: %s", static_cast<unsigned long long>(bound / 1000), g_waitWhy);
    }
    if (!H::EnsureResolved()) return;

    if (g_target.Raw() && g_goneAtS < 0.f && g_unreadAtS < 0.f && now >= g_nextSampleMs) {
        g_nextSampleMs = now + kSampleMs;
        Sample(since, isHost);
    }
    if (g_target.Raw() && (g_goneAtS >= 0.f || g_unreadAtS >= 0.f))
        return Fail(isHost, "the target is %s; the series stops there", g_goneAtS >= 0.f ? "gone" : "unreadable");

    switch (g_phase) {
    case Phase::Pick: {
        // Ends when the pick succeeds. One shot: it waits for the join and the anchor, and a miss
        // after that is final.
        g_waitWhy = "the join is not over";
        if (!JoinOver(isHost)) return;
        ue_wrap::FVector anchor{};
        const char* why = "";
        if (!Anchor(isHost, anchor, why)) { g_waitWhy = why; return; }
        void* t = PickTarget(anchor, g_targetKey, g_targetStart);
        if (!t) return Fail(isHost, "no keyed physics prop within %.0f cm", kPickRadiusCm);
        g_target.Set(t);
        const float dx = g_targetStart.X - anchor.X, dy = g_targetStart.Y - anchor.Y,
                    dz = g_targetStart.Z - anchor.Z;
        // The eid is the cross-peer identity: the host's local element, the client's mirror
        // row from the join snapshot. Two keys can differ where two eids agree.
        const coop::element::ElementId eid =
            isHost ? coop::prop_element_tracker::GetPropElementIdForActor(t)
                   : coop::remote_prop::ResolveMirrorEidByActor(t);
        UE_LOGI("hookdrag_selftest: TARGET role=%s key='%ls' eid=%u at (%.1f,%.1f,%.1f), %.0f cm "
                "from the anchor", isHost ? "HOST" : "CLIENT", g_targetKey.c_str(),
                eid == coop::element::kInvalidId ? 0u : static_cast<unsigned>(eid),
                g_targetStart.X, g_targetStart.Y, g_targetStart.Z,
                std::sqrt(dx * dx + dy * dy + dz * dz));
        g_nextSampleMs = now;
        EnterPhase(PlantsHere(isHost) ? Phase::Plant : Phase::Drag, now);
        return;
    }
    case Phase::Plant: {
        // Ends when the attach dispatched. A client's tie is built on the host and nothing of it
        // reaches this peer, so the host's own log carries that line.
        void* p = LocalPlayer();
        void* t = g_target.Get();
        if (!p || !t) g_plantNote = !p ? "no-local-player" : "target-gone";
        if (!p || !t || !Plant(p, t)) return Fail(isHost, "%s", g_plantNote);
        UE_LOGI("hookdrag_selftest: PLANT key='%ls' hook=%p -- attach_a with the head on the "
                "prop and the tail on the player", g_targetKey.c_str(), g_hook.Raw());
        if (!BeginWalkOut(isHost)) return Fail(isHost, "the walker's position could not be read");
        EnterPhase(Phase::WalkOut, now);
        return;
    }
    case Phase::WalkOut:
        // Ends when the walker is kWalkCm from where it planted.
        g_waitWhy = "the walker did not get that far";
        if (WalkerDistance(g_waitCm) && g_waitCm >= kWalkCm) {
            UE_LOGI("hookdrag_selftest: WALK BACK from %.0f cm", g_waitCm);
            EnterPhase(Phase::WalkBack, now);
            return;
        }
        WalkStep(/*outward=*/true);
        return;
    case Phase::WalkBack:
        // Ends when the walker is back within kBackCm of where it planted.
        g_waitWhy = "the walker did not get back";
        if (WalkerDistance(g_waitCm) && g_waitCm <= kBackCm) {
            EnterPhase(Phase::Unhook, now);
            return;
        }
        WalkStep(/*outward=*/false);
        return;
    case Phase::Unhook:
        // Ends when the hook is destroyed. The item's cancel: the field this driver wrote is
        // cleared whether or not the hook still stands, then the actor goes.
        if (void* p = LocalPlayer()) H::WriteActiveHook(p, nullptr);
        if (void* h = g_hook.Get()) {
            E::DestroyActor(h);
            UE_LOGI("hookdrag_selftest: UNHOOK -- the hook is destroyed; the prop coasts and rests");
        } else {
            UE_LOGW("hookdrag_selftest: UNHOOK -- the hook had already died");
        }
        g_hook.Reset();
        g_restRun = 0;
        EnterPhase(Phase::Rest, now);
        return;
    case Phase::Drag:
        // The peer that does not plant: ends when the prop has stood kMovedCm from its start.
        g_waitWhy = "the prop did not move that far";
        g_waitCm = g_maxMovedCm;
        if (g_maxMovedCm < kMovedCm) return;
        UE_LOGI("hookdrag_selftest: DRAG seen -- the prop stood %.0f cm from its start", g_maxMovedCm);
        g_restRun = 0;
        EnterPhase(Phase::Rest, now);
        return;
    case Phase::Rest:
        // Ends when the prop has stood still for kRestSamples samples since the unhook (or, for
        // the peer that did not plant, since it was seen dragged).
        g_waitWhy = "the prop did not come to rest";
        if (g_restRun < kRestSamples) return;
        UE_LOGI("hookdrag_selftest: REST -- the prop stood still for %d samples", g_restRun);
        End();
        return;
    case Phase::Ended:
        return;
    }
}

void EmitVerdict() {
    if (!Enabled() || !g_originMs) return;
    auto* s = g_session.load(std::memory_order_acquire);
    const bool isHost = s && s->role() == coop::net::Role::Host;
    UE_LOGI("hookdrag_selftest: VERDICT role=%s key='%ls' plant=%s maxMoved=%.1f cm goneAt=%.2f unreadAt=%.2f",
            isHost ? "HOST" : "CLIENT", g_targetKey.c_str(),
            PlantsHere(isHost) ? g_plantNote : "n/a", g_maxMovedCm, g_goneAtS, g_unreadAtS);
}

void OnDisconnect() {
    g_originMs = 0;
    g_nextSampleMs = 0;
    g_phaseMs = 0;
    g_phase = Phase::Pick;
    g_restRun = 0;
    g_havePrev = false;
    g_waitWhy = "";
    g_waitCm = -1.f;
    g_target.Reset();
    g_hook.Reset();
    g_targetKey.clear();
    g_maxMovedCm = 0.f;
    g_goneAtS    = -1.f;
    g_unreadAtS  = -1.f;
    g_plantNote  = "not attempted";
}

}  // namespace coop::dev::hookdrag_selftest
