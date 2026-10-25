// coop/dev/hostile_drill_explosion_client.cpp -- the explosion family's sender: see coop/dev/hostile_drill_explosion.h.
// The client takes what the arm needs into its hand, waits for the host to say it holds the matching state, then sends
// raw ProjectileFired / ExplosionRequest payloads through the session's own send, bypassing its request lane.

#include "hostile_drill_explosion_detail.h"  // co-located private header (src tree, not include/)

#include "explosion_drill_source.h"  // Guard, LegOf, HeldOf, ResolveClass (co-located private header)

#include "coop/dev/announce_hold.h"
#include "coop/dev/drill_hand.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/engine.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

namespace coop::dev::hostile_drill::explosion::detail {
namespace {

namespace XD = coop::dev::explosion_drill::detail;
namespace E  = ue_wrap::engine;
using Kind = coop::net::ReliableKind;
using ue_wrap::FVector;

// The wire's own values, written as literals from the protocol's field comments.
constexpr uint8_t kRocket = 1, kStickyBomb = 2;
constexpr uint8_t kRpg = 1, kStickyLauncher = 2, kWitchCar = 3;
constexpr uint8_t kProjectile = 1, kHandKind = 2, kSelfKind = 3;
constexpr uint8_t kSrcRocket = 1, kSrcSticky = 2, kSrcGrenade = 3, kSrcPlayer = 5;
constexpr uint32_t kIdBase = 1000;          // forged shot ids run from here, one per message
constexpr uint32_t kRejoinShotId = 777;     // the rejoin arm's fixed id, the same in both lives
constexpr float kFarCm = 10000.f;           // 100 m, past any origin bound the host allows
constexpr float kHandFarCm = 5000.f;        // 50 m, past any hand bound
constexpr float kReachCm = 500000.f;        // 5 km, past any rocket reach

enum class CStep { Prep, Await, Stream, AwaitTaken, Done };
CStep    g_step = CStep::Prep;
bool     g_idleSaid = false, g_guarded = false, g_took = false, g_haveB = false, g_ready = false;
bool     g_life2 = false;   // the rejoin arm's second life, known from the host's live ready line
bool     g_taken = false;   // the host's live shot-taken line has been seen
Clock::time_point g_readyAt{};   // when the live ready line was first seen
FVector  g_B{};
uint32_t g_streamed = 0;
Clock::time_point g_since{};
const char* g_waiting = "the client's own join";

void Inconclusive(const Spec& sp, const std::string& why) {
    Verdict(sp.arm, "INCONCLUSIVE", why);
    g_step = CStep::Done;
}

bool WaitedTooLong() {
    if (g_since == Clock::time_point{}) g_since = Clock::now();
    return Clock::now() - g_since > kFailBound;
}

bool OwnJoinOver() {
    return coop::net_pump::HasAnnouncedWorldReady() &&
           coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle;
}

bool SendFired(coop::net::Session* s, uint8_t proj, uint8_t launcher, uint32_t id, const FVector& at) {
    coop::net::ProjectileFiredPayload p{};
    p.projClass = proj;
    p.launcher = launcher;
    p.shotId = id;
    p.origin[0] = at.X; p.origin[1] = at.Y; p.origin[2] = at.Z;
    return s->SendReliable(Kind::ProjectileFired, &p, sizeof(p));
}

bool SendRequest(coop::net::Session* s, uint8_t kind, uint8_t src, uint32_t id, float pill, const FVector& at) {
    coop::net::ExplosionRequestPayload p{};
    p.kind = kind;
    p.srcClass = src;
    p.shotId = id;
    p.pillLevel = pill;
    p.loc[0] = at.X; p.loc[1] = at.Y; p.loc[2] = at.Z;
    return s->SendReliable(Kind::ExplosionRequest, &p, sizeof(p));
}

// One forged run, counting what it sent; a send that fails stops the run.
struct Out {
    coop::net::Session* s;
    uint32_t            n = 0;
    bool                ok = true;
    void Fired(uint8_t proj, uint8_t launcher, uint32_t id, const FVector& at) {
        ok = ok && SendFired(s, proj, launcher, id, at);
        ++n;
    }
    void Request(uint8_t kind, uint8_t src, uint32_t id, float pill, const FVector& at) {
        ok = ok && SendRequest(s, kind, src, id, pill, at);
        ++n;
    }
};

bool NeedsB(Arm a) { return a == Arm::Consumed || a == Arm::WaitBehindShot; }

// The arm's messages from `p`, the client's own place (the host's puppet of it stands there), and `b`, the blast point.
// The evicted arm sends its own, one per tick.
void SendArm(Out& o, Arm a, const FVector& p, const FVector& b) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const uint32_t id = kIdBase;
    switch (a) {
    case Arm::Malformed:
        o.Fired(kStickyBomb, kRpg, id, p);                                      // a pair no launcher fires
        o.Fired(kRocket, kRpg, id + 1, FVector{nan, p.Y, p.Z});                 // a non-finite coordinate
        o.Fired(kRocket, kRpg, 0, p);                                           // shot id 0
        o.Request(kHandKind, kSrcGrenade, 0, 5.f, p);                           // a pill level on a hand request
        break;
    case Arm::Duplicate:
        o.Fired(kRocket, kRpg, id, p);
        o.Fired(kRocket, kRpg, id, p);
        break;
    case Arm::UnknownShot:
        o.Request(kProjectile, kSrcRocket, id, 0.f, p);
        break;
    case Arm::Consumed:
        o.Fired(kRocket, kRpg, id, p);
        o.Request(kProjectile, kSrcRocket, id, 0.f, b);
        o.Request(kProjectile, kSrcRocket, id, 0.f, b);
        break;
    case Arm::NoLauncher:
        o.Fired(kRocket, kRpg, id, p);
        break;
    case Arm::Origin:
        o.Fired(kRocket, kRpg, id, FVector{p.X + kFarCm, p.Y, p.Z});
        break;
    case Arm::Reach:
        o.Fired(kRocket, kRpg, id, p);
        o.Request(kProjectile, kSrcRocket, id, 0.f, FVector{p.X + kReachCm, p.Y, p.Z});
        break;
    case Arm::NotSeated:
        o.Fired(kRocket, kWitchCar, id, p);
        break;
    case Arm::NoExplosive:
        o.Request(kHandKind, kSrcGrenade, 0, 0.f, p);
        break;
    case Arm::HandBound:
        o.Request(kHandKind, kSrcGrenade, 0, 0.f, FVector{p.X + kHandFarCm, p.Y, p.Z});
        break;
    case Arm::Story:
        o.Fired(kRocket, kRpg, id, p);
        break;
    case Arm::WaitBehindShot:
        for (uint32_t i = 0; i < 40; ++i) o.Fired(kRocket, kRpg, id + i, p);
        o.Request(kProjectile, kSrcRocket, id + 39, 0.f, b);                    // the last shot may still be queued
        break;
    case Arm::Flood:
        for (uint32_t i = 0; i < 100; ++i) o.Fired(kRocket, kRpg, id + i, p);
        break;
    case Arm::Rejoin:
        o.Fired(kStickyBomb, kStickyLauncher, kRejoinShotId, p);
        break;
    case Arm::SelfClamp:
        o.Request(kSelfKind, kSrcPlayer, 0, 1e9f, FVector{});
        break;
    default:
        break;
    }
}

// not_ready: the shot goes out while this client's own announce is held, then the hold is let go. The edge is the one
// the act family uses: the hold set, the local player up, the slot assigned. A location read that still fails past
// the failure bound ends the arm and lets the hold go.
void TickNotReady(coop::net::Session* s, const Spec& sp) {
    if (coop::net_pump::HasAnnouncedWorldReady()) {
        Inconclusive(sp, "the hold did not take: the announce was already out before the shot could be sent");
        return;
    }
    if (!coop::dev::announce_hold::Held()) return;   // Install sets the hold at the session's start
    // One clock for every wait below, started at the tick the hold is seen set.
    const bool late = WaitedTooLong();
    auto giveUp = [&](const char* why) {
        if (!late) return;
        coop::dev::announce_hold::Set(false);
        Inconclusive(sp, why);
    };
    const uint8_t self = coop::players::Registry::Get().LocalPeerId();
    void* player = coop::players::Registry::Get().Local();
    if (!player || !s->connected() || self == 0 || self >= coop::players::kMaxPeers) {
        giveUp("the local player, the connection or the slot did not resolve before the shot could be sent");
        return;
    }
    if (self != kSlot) {
        UE_LOGI("[HOSTILE-DRILL] %s idles: this client is slot %u, the family acts on slot %u", sp.name,
                static_cast<unsigned>(self), static_cast<unsigned>(kSlot));
        coop::dev::announce_hold::Set(false);
        g_step = CStep::Done;
        return;
    }
    FVector at{};
    if (!E::TryGetActorLocation(player, at)) {
        giveUp("the local player's location did not resolve before the shot could be sent");
        return;
    }
    const bool ok = SendFired(s, kRocket, kRpg, kIdBase, at);
    coop::dev::announce_hold::Set(false);
    g_step = CStep::Done;
    if (!ok) {
        Verdict(sp.arm, "FAIL", "client: the forged shot was not sent");
        return;
    }
    UE_LOGI("[HOSTILE-DRILL] %s sent one shot with the announce held, and let it go", sp.name);
}

// The guard, the blast point and the hand item the arm needs, in that order; true once all are in.
bool Prepare(const Spec& sp, void* player) {
    if (!g_guarded) {
        XD::g_src.host = false;
        if (XD::Guard(false)) { g_waiting = "the guard (startInvinc, unlockFungunsInStory) to resolve"; return false; }
        g_guarded = true;
    }
    if (NeedsB(sp.arm) && !g_haveB) {
        const XD::Fix f = XD::ComputeB(player, player, g_B);
        if (f == XD::Fix::Wait) { g_waiting = "the blast point"; return false; }
        if (f != XD::Fix::Ok) { Inconclusive(sp, "no clear blast point beside this client"); return false; }
        g_haveB = true;
    }
    if (sp.hand == Hand::Empty) return true;
    const XD::LegDef* d = XD::LegOf(sp.hand == Hand::Rpg      ? XD::Kind::Rocket
                                    : sp.hand == Hand::Sticky ? XD::Kind::Sticky
                                                              : XD::Kind::Grenade);
    if (!d) { Inconclusive(sp, "the hand item's row is missing"); return false; }
    if (!g_took) {
        if (!XD::ResolveClass(d->cls, d->path)) { Inconclusive(sp, "the item's class is neither loaded nor loadable"); return false; }
        if (!coop::dev::drill_hand::TakeIntoHandNow(d->row, d->cls)) { Inconclusive(sp, "the hand could not take the item"); return false; }
        g_took = true;
    }
    if (!XD::HeldOf(player, d->cls)) { g_waiting = "the hand to hold the item"; return false; }
    return true;
}

// What the host said, latched on sight from the arm's first tick, whatever step the client is in: a live chat row
// retires after seconds, so a line is read when it lands, never when a later step comes to look. Live rows only: a
// seeded row is the record of an earlier life. The rejoin arm lives the life the first ready line names (the
// returning occupant's is life-2); the other arms have one life. Polled a few times a second.
void Observe(const Spec& sp) {
    if (!PollDue()) return;
    if (!g_ready) {
        if (sp.mode == Mode::Rejoin && ChatHasLive(ReadyLine(sp.arm, 2))) {
            g_life2 = true;
            g_ready = true;
        } else if (ChatHasLive(ReadyLine(sp.arm, 1))) {
            g_ready = true;
        }
        if (g_ready) g_readyAt = Clock::now();
    }
    if (sp.mode == Mode::Rejoin && !g_taken && ChatHasLive(TakenLine(sp.arm))) g_taken = true;
}

// The rejoin arm's second life: a request for the first life's shot.
void SendLife2(coop::net::Session* s, const Spec& sp, const FVector& at) {
    Out o{s};
    o.Request(kProjectile, kSrcSticky, kRejoinShotId, 0.f, at);
    g_step = CStep::Done;
    if (!o.ok) { Verdict(sp.arm, "FAIL", "client: the life-2 request was not sent"); return; }
    UE_LOGI("[HOSTILE-DRILL] %s life-2 sent a request for shot %u", sp.name, static_cast<unsigned>(kRejoinShotId));
}

}  // namespace

void ClientTick(coop::net::Session* s, const Spec& sp) {
    if (g_step == CStep::Done) return;
    if (sp.mode == Mode::NotReady) return TickNotReady(s, sp);
    Observe(sp);
    if (!OwnJoinOver()) return;
    const uint8_t self = coop::players::Registry::Get().LocalPeerId();
    void* player = coop::players::Registry::Get().Local();
    if (!player || self == 0 || self >= coop::players::kMaxPeers) return;
    if (self != kSlot) {
        if (!g_idleSaid) {
            g_idleSaid = true;
            UE_LOGI("[HOSTILE-DRILL] %s idles: this client is slot %u, the family acts on slot %u", sp.name,
                    static_cast<unsigned>(self), static_cast<unsigned>(kSlot));
        }
        g_step = CStep::Done;
        return;
    }
    if (g_step == CStep::AwaitTaken) {
        if (WaitedTooLong()) return Inconclusive(sp, "the host never said it took the shot");
        if (!g_taken) return;
        UE_LOGI("[HOSTILE-DRILL] %s life-1 shot-taken", sp.name);   // the rejoin marker, read from this log
        g_step = CStep::Done;
        return;
    }
    if (g_step == CStep::Prep) {
        if (g_life2) {
            g_step = CStep::Await;   // life 2 needs neither the guard nor a hand item
        } else {
            if (!Prepare(sp, player)) {
                if (g_step != CStep::Done && WaitedTooLong())
                    Inconclusive(sp, std::string("timed out waiting for ") + g_waiting);
                return;
            }
            g_step = CStep::Await;
            g_since = {};
        }
    }
    if (g_step == CStep::Await) {
        if (!g_ready) {
            if (WaitedTooLong()) Inconclusive(sp, "the host never said it was ready");
            return;
        }
        g_step = CStep::Stream;
    }
    FVector here{};
    if (!E::TryGetActorLocation(player, here)) {
        if (Clock::now() - g_readyAt > kFailBound)
            Inconclusive(sp, "the local player's location did not resolve after the host said it was ready");
        return;
    }
    if (g_life2) return SendLife2(s, sp, here);
    // evicted: one sticky shot per tick, so the host's queue never holds the whole run.
    if (sp.arm == Arm::Evicted) {
        Out o{s};
        o.Fired(kStickyBomb, kStickyLauncher, kIdBase + g_streamed, here);
        if (!o.ok) {
            g_step = CStep::Done;
            return Verdict(sp.arm, "FAIL", "client: a forged shot was not sent");
        }
        if (++g_streamed == sp.sent) {
            g_step = CStep::Done;
            UE_LOGI("[HOSTILE-DRILL] %s sent %u sticky shots, one per tick", sp.name, static_cast<unsigned>(sp.sent));
        }
        return;
    }
    Out o{s};
    SendArm(o, sp.arm, here, g_B);
    const bool rejoin = sp.mode == Mode::Rejoin;
    g_step = rejoin ? CStep::AwaitTaken : CStep::Done;
    g_since = {};
    if (!o.ok) {
        g_step = CStep::Done;
        return Verdict(sp.arm, "FAIL", "client: a forged message was not sent");
    }
    UE_LOGI("[HOSTILE-DRILL] %s sent %u messages", sp.name, static_cast<unsigned>(o.n));
}

void ClientReset() {
    if (g_guarded) XD::Unguard();
    g_step = CStep::Prep;
    g_idleSaid = g_guarded = g_took = g_haveB = g_ready = false;
    g_life2 = g_taken = false;
    g_readyAt = {};
    g_B = FVector{};
    g_streamed = 0;
    g_since = {};
    g_waiting = "the client's own join";
}

}  // namespace coop::dev::hostile_drill::explosion::detail
