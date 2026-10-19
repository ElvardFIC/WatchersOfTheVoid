// coop/world/explosion_request_host.cpp -- the host's judgement of a client's shot and blast, and the spawn of the
// blast: see coop/world/explosion_request.h. Each judge answers Done for an accepted or a refused message and Wait
// only where an honest sender's message is early (the hand lane still owes the slot's state, or the shot a blast
// belongs to is still in the slot's own Fired queue). Game thread.

#include "explosion_request_internal.h"  // co-located private header (src tree, not include/)

#include "coop/interactables/atv_sync.h"
#include "coop/net/net_clock.h"
#include "coop/net/session.h"
#include "coop/player/hand_history.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/world/explosion_recipe.h"
#include "coop/world/explosion_sync.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/world/explosion.h"
#include "ue_wrap/world/explosion_source.h"
#include "ue_wrap/world/game_mode.h"

#include <cmath>
#include <cstdint>
#include <cwchar>

namespace coop::explosion_request::detail {
namespace {

namespace RC = coop::explosion_recipe;
namespace XS = ue_wrap::explosion_source;
namespace X  = ue_wrap::explosion;
namespace E  = ue_wrap::engine;
namespace R  = ue_wrap::reflection;
using ue_wrap::FVector;
using coop::net::Served;

// The launcher must have been in the slot's hand within this window of the shot: an honest switch-then-shoot is
// judged against the hand lane's applied history, which trails the client's own hand by the lane's pace.
constexpr uint64_t kLauncherWindowMs = 2000;
// A grenade or pipe bomb is held to its fuse; this much more is allowed on top.
constexpr uint64_t kFuseSlackMs = 2000;
constexpr uint64_t kGrenadeFuseMs  = 3000;
constexpr uint64_t kPipebombFuseMs = 6000;

// How far from the shooter a shot's origin or a hand blast may be, before the speed travelled in one round trip.
constexpr float kShotOriginCm = 400.0f;
constexpr float kHandBlastCm  = 300.0f;

// A rocket's speed from each launcher that fires one, cm/s, and the slack on its reach.
constexpr float kRpgRocketSpeed  = 5000.0f;
constexpr float kWitchRocketSpeed = 10000.0f;
constexpr float kRocketReachSlackCm = 500.0f;
// The retransmit slack: a shot's packet may be resent after its blast's.
constexpr float kRetransmitSlackMs = 1000.0f;
// A sticky bomb rides whatever it struck, so its point is unbounded but by the world.
constexpr float kStickyReachCm = 20000.0f;

// The round trip when the session has not measured one yet.
constexpr float kUnmeasuredRttMs = 500.0f;

constexpr const wchar_t* kWitchCarClass = L"car1_witch_C";

float Dist(const FVector& a, const float b[3]) {
    const float dx = a.X - b[0], dy = a.Y - b[1], dz = a.Z - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float Magnitude(const FVector& v) { return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z); }

float RttMs(uint8_t slot) {
    coop::net::Session* s = SessionPtr();
    const int rtt = s ? s->rttMsForSlot(slot) : -1;
    return rtt < 0 ? kUnmeasuredRttMs : static_cast<float>(rtt);
}

// The slot's puppet and its location, or null.
coop::RemotePlayer* PuppetOf(uint8_t slot, FVector& loc) {
    coop::RemotePlayer* rp = coop::players::Registry::Get().Puppet(slot);
    if (!rp || !rp->valid() || !rp->GetActor() || !rp->TryGetLocation(loc)) return nullptr;
    return rp;
}

// The reach a body may travel in one round trip from where the host last saw it: its base, widened by its velocity.
float Slack(void* actor, uint8_t slot, float base) {
    return base + Magnitude(E::GetActorVelocity(actor)) * RttMs(slot) / 1000.0f;
}

const char* ProjName(uint8_t projClass) {
    return projClass == static_cast<uint8_t>(RC::ProjClass::Rocket) ? "rocket" : "sticky";
}

// Remove the entry at `i`, keeping the arrival order of the rest.
void EraseShot(Slot& s, size_t i) {
    for (size_t j = i + 1; j < s.count; ++j) s.shots[j - 1] = s.shots[j];
    --s.count;
}

// The oldest sticky entry, else the oldest.
size_t EvictionVictim(const Slot& s) {
    for (size_t i = 0; i < s.count; ++i)
        if (s.shots[i].projClass == static_cast<uint8_t>(RC::ProjClass::Sticky)) return i;
    return 0;
}

// The blast itself: the host's own parameters for the source, spawned natively inside the lane's scope so the birth
// seam leaves it alone; the host's capture of its begin-play carries it to every peer.
void SpawnBlast(uint8_t slot, const char* kind, XS::Kind src, const float loc[3], float level) {
    if (!X::ResolveNow(X::Cls::Explosion)) return Refuse(slot, Refusal::Unresolved);
    X::Params params{};
    float scale[3];
    RC::RecipeFor(src, level, params, scale);
    const float rot[3] = {0.0f, 0.0f, 0.0f};
    void* actor = nullptr;
    {
        coop::explosion_sync::LaneSpawnScope scope;
        actor = X::SpawnDeferred(X::Cls::Explosion, loc, rot, scale, params, nullptr);
    }
    if (!actor) return Refuse(slot, Refusal::SpawnFailed);
    ++MutCounters().requestsAccepted;
    UE_LOGI("[EXPLOSION] request accepted kind=%s class=%ls slot=%u", kind, XS::ClassName(src),
            static_cast<unsigned>(slot));
}

}  // namespace

Served JudgeShot(uint8_t slot, Slot& s, const FiredItem& it, uint64_t now) {
    const coop::net::ProjectileFiredPayload& p = it.p;
    for (size_t i = 0; i < s.count; ++i)
        if (s.shots[i].shotId == p.shotId) {
            Refuse(slot, Refusal::DuplicateShot);
            return Served::Done;
        }

    // MTA relays the projectile to every peer (CGame.cpp:2953); here it is not relayed, the blast reaches every peer.
    // MTA checks no weapon on its server (CGame.cpp:2831-2932); here the launcher must be in the shooter's hand or seat.
    void* car = nullptr;
    if (p.launcher == static_cast<uint8_t>(RC::Launcher::WitchCar)) {
        car = coop::atv_sync::ActorSeatedBy(slot);
        if (!car || ::_wcsicmp(R::ClassNameOf(car).c_str(), kWitchCarClass) != 0) {
            Refuse(slot, Refusal::NotSeated);
            return Served::Done;
        }
    } else {
        if (!coop::hand_history::HeldWithin(slot, RC::LauncherClassName(p.launcher), kLauncherWindowMs, now)) {
            if (coop::hand_item::HandStateOwed(slot)) return Served::Wait;
            Refuse(slot, Refusal::NoLauncher);
            return Served::Done;
        }
        if (!ue_wrap::game_mode::FungunHoldAllowed()) {
            Refuse(slot, Refusal::FungunRule);
            return Served::Done;
        }
    }

    // The origin: a launcher fires from the player's hand or the car's mount.
    FVector anchor{};
    void* anchorActor = car;
    if (car) {
        if (!E::TryGetActorLocation(car, anchor)) {
            Refuse(slot, Refusal::NotSeated);
            return Served::Done;
        }
    } else {
        coop::RemotePlayer* rp = PuppetOf(slot, anchor);
        if (!rp) {
            Refuse(slot, Refusal::NoPuppet);
            return Served::Done;
        }
        anchorActor = rp->GetActor();
    }
    // Written so that a NaN fails.
    if (!(Dist(anchor, p.origin) <= Slack(anchorActor, slot, kShotOriginCm))) {
        Refuse(slot, Refusal::OriginBound);
        return Served::Done;
    }

    if (s.count == kShotsMax) {
        EraseShot(s, EvictionVictim(s));
        ++MutCounters().evicted;
    }
    Shot& sh = s.shots[s.count++];
    sh.shotId    = p.shotId;
    sh.projClass = p.projClass;
    sh.launcher  = p.launcher;
    sh.origin[0] = p.origin[0];
    sh.origin[1] = p.origin[1];
    sh.origin[2] = p.origin[2];
    sh.recvMs    = it.recvMs;
    ++MutCounters().shotsAccepted;
    UE_LOGI("[EXPLOSION] shot accepted slot=%u id=%u class=%s launcher=%u", static_cast<unsigned>(slot),
            static_cast<unsigned>(p.shotId), ProjName(p.projClass), static_cast<unsigned>(p.launcher));
    return Served::Done;
}

Served JudgeShotBlast(uint8_t slot, Slot& s, const RequestItem& it, uint64_t) {
    const coop::net::ExplosionRequestPayload& p = it.p;
    const XS::Kind src = RC::KindOfSrcClass(p.srcClass);
    const uint8_t wanted = static_cast<uint8_t>(src == XS::Kind::Rocket ? RC::ProjClass::Rocket : RC::ProjClass::Sticky);
    size_t at = s.count;
    for (size_t i = 0; i < s.count; ++i)
        if (s.shots[i].shotId == p.shotId && s.shots[i].projClass == wanted) {
            at = i;
            break;
        }
    if (at == s.count) {
        // Its shot may still be queued behind the shots paced before it, but only while the slot's Fired queue holds any.
        if (!s.fired.empty()) return Served::Wait;
        Refuse(slot, Refusal::UnknownShot);
        return Served::Done;
    }

    // MTA re-bases a position sent relative to an origin element on the element's server position
    // (CGame.cpp:2841-2870); a projectile has no host element, so the absolute point is bounded against the shot's origin.
    const Shot sh = s.shots[at];
    EraseShot(s, at);  // the shot's one blast is spent, accepted or not
    float reach = kStickyReachCm;
    if (wanted == static_cast<uint8_t>(RC::ProjClass::Rocket)) {
        const float speed = sh.launcher == static_cast<uint8_t>(RC::Launcher::WitchCar) ? kWitchRocketSpeed
                                                                                        : kRpgRocketSpeed;
        const float flightMs = it.recvMs > sh.recvMs ? static_cast<float>(it.recvMs - sh.recvMs) : 0.0f;
        reach = speed * (flightMs + RttMs(slot) + kRetransmitSlackMs) / 1000.0f + kRocketReachSlackCm;
    }
    FVector origin{sh.origin[0], sh.origin[1], sh.origin[2]};
    if (!(Dist(origin, p.loc) <= reach)) {
        Refuse(slot, Refusal::Reach);
        return Served::Done;
    }
    SpawnBlast(slot, "projectile", src, p.loc, 0.0f);
    return Served::Done;
}

Served JudgeBody(uint8_t slot, Slot&, const RequestItem& it, uint64_t now) {
    const coop::net::ExplosionRequestPayload& p = it.p;
    const XS::Kind src = RC::KindOfSrcClass(p.srcClass);
    FVector at{};
    coop::RemotePlayer* rp = nullptr;

    // MTA marks no player's explosion once and checks no weapon (CGame.cpp:2831-2932); here a hand blast needs its
    // explosive in the hand within its fuse, and a self blast needs a living puppet and the bucket's pace.
    if (p.kind == static_cast<uint8_t>(RC::RequestKind::Hand)) {
        const uint64_t fuse = src == XS::Kind::Pipebomb ? kPipebombFuseMs : kGrenadeFuseMs;
        if (!coop::hand_history::HeldWithin(slot, XS::ClassName(src), fuse + kFuseSlackMs, now)) {
            if (coop::hand_item::HandStateOwed(slot)) return Served::Wait;
            Refuse(slot, Refusal::NoExplosive);
            return Served::Done;
        }
        rp = PuppetOf(slot, at);
        if (!rp) {
            Refuse(slot, Refusal::NoPuppet);
            return Served::Done;
        }
        if (!(Dist(at, p.loc) <= Slack(rp->GetActor(), slot, kHandBlastCm))) {
            Refuse(slot, Refusal::HandBound);
            return Served::Done;
        }
        SpawnBlast(slot, "hand", src, p.loc, 0.0f);
        return Served::Done;
    }
    if (p.kind == static_cast<uint8_t>(RC::RequestKind::Self)) {
        rp = PuppetOf(slot, at);
        if (!rp) {
            Refuse(slot, Refusal::NoPuppet);
            return Served::Done;
        }
        // The same alive test as the hit judge: the health the puppet's pose stream last carried is above 0.
        if (!(rp->GetHealth() > 0.0f)) {
            Refuse(slot, Refusal::PuppetDead);
            return Served::Done;
        }
        const float loc[3] = {at.X, at.Y, at.Z};
        SpawnBlast(slot, "self", src, loc, RC::ClampLevel(p.pillLevel));
        return Served::Done;
    }
    Refuse(slot, Refusal::Malformed);  // the receive edge queues only these two kinds here
    return Served::Done;
}

}  // namespace coop::explosion_request::detail
