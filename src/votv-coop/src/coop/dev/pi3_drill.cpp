// coop/dev/pi3_drill.cpp -- see coop/dev/pi3_drill.h. The arm table, the verdict tests and the lines are
// pi3_arms.cpp's.

#include "coop/dev/pi3_drill.h"

#include "pi3_drill_internal.h"  // the table, the phases and the tests (src-local)

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/creatures/npc_adoption.h"
#include "coop/creatures/npc_sync.h"
#include "coop/dev/pi3_observer.h"
#include "coop/dev/director/director.h"
#include "coop/dev/director/routes.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/player/puppet_identity.h"
#include "coop/player/remote_player.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"  // FindPropertyOffset
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_pawn.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace coop::dev::pi3_drill {
namespace {

using namespace detail;
namespace E  = ue_wrap::engine;
namespace R  = ue_wrap::reflection;
namespace OI = ue_wrap::object_index;
namespace OB = coop::dev::pi3_observer;
namespace PL = coop::players;
namespace PI = coop::puppet_identity;
namespace DR = coop::director;

// Failure bounds only: each step ends on the state it waits for.
constexpr uint64_t kWaitBoundMs = 60000;    // this peer's world up to the arm's readiness
constexpr uint64_t kRestBoundMs = 60000;    // the still body coming to a stand
constexpr uint64_t kNoContactMs = 10000;    // the spawn to the first contact
constexpr float    kRingCm = 1800.f;        // the ring about the still body the spawner walks to
constexpr float    kStandReachCm = 150.f;
constexpr int      kRingPoints = 8;
constexpr int      kWalkDeadlineS = 60;     // Apart's bound
constexpr float    kApartMinCm = 1500.f;    // flat distance from the body at which Apart has ended
constexpr float    kRestMoveCm = 5.f;       // the body moved less than this in total over kRestStillMs
constexpr uint64_t kRestStillMs = 1000;
constexpr int      kPiWatches = 19;         // PI-2b's table: its module has settled when this many are live
constexpr uint64_t kStillLineMs = 500;      // a still peer's STILL lines: at most one in this long
constexpr uint32_t kContactEvery = 30;      // the contact watches are read once in this many ticks

ArmId ArmOf() {
    static const ArmId arm = [] {
        const std::string v = coop::config::ResolveEnum(::coop::config_registry::rows::pi3_drill);
        return ArmFromName(v.c_str());
    }();
    return arm;
}

uint64_t Now() { return ::GetTickCount64(); }

bool     g_latched = false;  // an arm began: the drill runs once per process
bool     g_active = false;   // ... and has not ended
bool     g_spawner = false;
bool     g_obsArmed = false;
bool     g_brokenSaid = false;
Phase    g_phase = Phase::Wait;
int      g_leg = 1;          // the client arm's leg
uint64_t g_worldUpAt = 0;
uint64_t g_phaseAt = 0;
uint64_t g_spawnAt = 0;
uint64_t g_anchorAt = 0;     // the drill's sight of the first contact: the horizon counts from it
bool     g_failSaid = false;
bool     g_legsOk = true;    // the client arm: every leg so far passed
bool     g_stamped = true;   // the spawn's stamp took (false: the spawn is abandoned)
uint32_t g_baseline = 0;
uint64_t g_restRefAt = 0;
uint64_t g_stillSaidAt = 0;
uint32_t g_contactTicks = 0;  // CheckContacts' call count, for its one-in-kContactEvery read
bool     g_stillDirty = false;  // the records changed since the last STILL line
bool     g_stillArmed = false;
bool     g_uncancelledSaid = false;
ue_wrap::FVector g_bodyPos{};
ue_wrap::FVector g_restRef{};
ue_wrap::CachedObjRef g_site;
std::shared_ptr<DR::BackgroundWalk> g_walk;

void Go(Phase p) {
    g_phase = p;
    g_phaseAt = Now();
}

// The distance between two points, or only their ground-plane part.
float Dist(const ue_wrap::FVector& a, const ue_wrap::FVector& b, bool flat = false) {
    const float dx = a.X - b.X, dy = a.Y - b.Y, dz = flat ? 0.f : a.Z - b.Z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The remote player's puppet actor the site is made at: the client's for a host spawner, the host's for a client.
void* StillBody(bool spawnerIsHost) {
    coop::RemotePlayer* rp = PL::Registry::Get().Puppet(spawnerIsHost ? 1 : PL::kPeerIdHost);
    void* actor = rp ? rp->GetActor() : nullptr;
    return actor && R::IsLive(actor) ? actor : nullptr;
}

void DestroySite() {
    if (void* site = g_site.Get()) E::DestroyActor(site);
    g_site.Reset();
}

// A contact watch the engine killed for its class (the class lacks the stub): the instrument is broken, said once.
// The watches are read once per kContactEvery ticks, and at once (`now`) before a verdict or seen line is said.
void CheckContacts(ArmId arm, bool now = false) {
    if (!g_obsArmed || g_brokenSaid) return;
    if (!now && g_contactTicks++ % kContactEvery != 0) return;
    if (OB::ContactWatchLive()) return;
    g_brokenSaid = true;
    const Spec& first = SpecOf(SpecIdOf(arm, 1));
    if (arm == ArmId::Client)
        UE_LOGI("[PI-3] BROKEN %s unwatched %ls,%ls", ArmName(arm), first.cls, SpecOf(kSpecClientWispP).cls);
    else
        UE_LOGI("[PI-3] BROKEN %s unwatched %ls", ArmName(arm), first.cls);
}

// A still peer's last change, coalesced and not yet said, is said before its records are let go.
void Disarm() {
    if (!g_spawner && g_stillArmed && (OB::TakeChanged() || g_stillDirty)) {
        g_stillDirty = false;
        int n = 0;
        const Seen* seen = OB::Report(n);
        CheckContacts(ArmOf(), /*now=*/true);
        LogStill(ArmName(ArmOf()), seen, n, OB::OwnContacts());
    }
    OB::Disarm();
    g_obsArmed = false;
}

void Abandon(ArmId arm, const char* why) {
    UE_LOGI("[PI-3] ABANDONED %s %s", ArmName(arm), why);
    if (g_walk) {
        DR::CancelBackgroundWalk(*g_walk);
        g_walk.reset();
    }
    DestroySite();
    Disarm();
    g_active = false;
    g_latched = true;
    g_phase = Phase::Done;
}

// A Wait that ran out of its bound. The observer's names the first thing it is still waiting on.
void AbandonWait(ArmId arm, const char* what) {
    char why[256];
    const int w = std::snprintf(why, sizeof why, "wait %s", what);
    char pending[192];
    if (w > 0 && std::strcmp(what, "observer") == 0 && OB::FirstPending(pending, sizeof pending))
        std::snprintf(why + w, sizeof why - static_cast<size_t>(w), " %s", pending);
    Abandon(arm, why);
}

// The specs whose classes an arm waits on: the still peer's (the arm's first row, and the client arm's wisp_p) or
// the spawner's (every leg's). Returns how many, into `ids`.
int WaitSpecs(ArmId arm, bool spawner, int* ids) {
    int n = 0;
    ids[n++] = SpecIdOf(arm, 1);
    if (arm == ArmId::Client) {
        ids[n++] = kSpecClientWispP;
        if (spawner) ids[n++] = kSpecClientReading;
    }
    return n;
}

// A spec's class and the listing identity it was resolved under. A class held across ticks stands only while its
// listing does; a class that does not is looked up again (the index, else its asset path).
struct ResolvedClass {
    void*    cls;
    uint64_t id;
};
ResolvedClass g_res[kSpecCount] = {};

void* ClassOfSpec(int specId) {
    ResolvedClass& r = g_res[specId];
    if (r.cls && r.id != 0 && OI::ClassListingId(r.cls) == r.id) return r.cls;
    void* const cls = ResolveClass(SpecOf(specId));
    r = ResolvedClass{cls, cls ? OI::ClassListingId(cls) : 0};
    return cls;
}

void StillTick(coop::net::Session* session, ArmId arm) {
    if (!g_stillArmed) {
        const bool late = Now() - g_worldUpAt >= kWaitBoundMs;
        if (!OB::Live()) {
            if (late) AbandonWait(arm, "observer");
            return;
        }
        void* classes[2] = {};
        int ids[3];
        const int n = WaitSpecs(arm, /*spawner=*/false, ids);
        bool resolved = true;
        for (int i = 0; i < n; ++i) {
            classes[i] = ClassOfSpec(ids[i]);
            resolved = resolved && classes[i];
        }
        if (!resolved) {
            if (late) AbandonWait(arm, "class");
            return;
        }
        OB::Arm(classes, n, OB::Mode::Still);
        g_obsArmed = true;
        g_stillArmed = true;
        g_latched = true;
        g_active = true;
        if (session->role() == coop::net::Role::Client && coop::net_pump::HasAnnouncedWorldReady())
            UE_LOGI("[PI-3] VOID %s armed-late", ArmName(arm));
        UE_LOGI("[PI-3] ARMED %s role=still", ArmName(arm));
        return;
    }
    CheckContacts(arm);
    int n = 0;
    const Seen* seen = OB::Report(n);
    if (!g_uncancelledSaid)
        if (const char* verb = FirstUncancelled(seen, n)) {
            g_uncancelledSaid = true;
            CheckContacts(arm, /*now=*/true);
            UE_LOGI("[PI-3] FAIL %s still-uncancelled %s", ArmName(arm), verb);
        }
    if (OB::TakeChanged()) g_stillDirty = true;
    // A verb that repeats at tick rate changes the records every tick: the lines are coalesced, and the last state
    // is said once the interval has passed.
    if (!g_stillDirty || Now() - g_stillSaidAt < kStillLineMs) return;
    g_stillDirty = false;
    g_stillSaidAt = Now();
    CheckContacts(arm, /*now=*/true);
    LogStill(ArmName(arm), seen, n, OB::OwnContacts());
}

// What stands between the spawner and its arm, or null when it is ready.
const char* NotReady(coop::net::Session* session, ArmId arm) {
    const bool host = session->role() == coop::net::Role::Host;
    if (!OB::Live()) return "observer";
    if (PI::WatchesLive() != kPiWatches) return "pi";
    if (host && !session->IsSlotWorldReady(1)) return "peer";
    if (!StillBody(host)) return "puppet";
    if (!host && !coop::npc_adoption::GhostSweepDone()) return "sweep";
    int ids[3];
    const int n = WaitSpecs(arm, /*spawner=*/true, ids);
    for (int i = 0; i < n; ++i)
        if (!ClassOfSpec(ids[i])) return "class";
    return nullptr;
}

// The stamp and the bind, inside the spawn: before the actor's BeginPlay and the overlaps its colliders report.
struct SpawnCtx {
    const Spec* spec;
    void*       cls;
};

void BeforeFinish(void* ctx, void* actor) {
    const SpawnCtx& c = *static_cast<const SpawnCtx*>(ctx);
    OB::Bind(actor);
    if (!c.spec->stamp) return;
    const int32_t off = R::FindPropertyOffset(c.cls, L"NewVar_0");
    g_stamped = off >= 0 && ue_wrap::field_io::WriteFStringField(actor, off, c.spec->stamp);
}

void DoSpawn(ArmId arm) {
    const Spec& spec = SpecOf(SpecIdOf(arm, g_leg));
    const bool reading = arm == ArmId::Client && g_leg == 3;
    void* cls = ClassOfSpec(SpecIdOf(arm, g_leg));
    ue_wrap::FVector at = g_bodyPos;
    bool placed = true;
    if (arm == ArmId::Control || reading) {
        void* pawn = PL::Registry::Get().Local();
        placed = pawn && E::TryGetActorLocation(pawn, at);
    }
    if (!cls || !placed) {
        Abandon(arm, "spawn");
        return;
    }
    void* classes[1] = {cls};
    OB::Arm(classes, 1, reading ? OB::Mode::Reading : OB::Mode::Spawner);
    g_obsArmed = true;
    g_baseline = Counter(spec.counter);
    g_stamped = true;
    if (arm == ArmId::Client) coop::npc_sync::MarkIncomingNpcSpawn(cls);
    SpawnCtx ctx{&spec, cls};
    void* site = E::SpawnActor(cls, at, /*inertPawn=*/false, &BeforeFinish, &ctx);
    if (arm == ArmId::Client) coop::npc_sync::ClearIncomingNpcSpawn();
    if (site) g_site.Set(site);
    if (!site || !g_stamped) {
        Abandon(arm, "spawn");
        return;
    }
    g_spawnAt = Now();
    g_anchorAt = 0;
    g_failSaid = false;
    Go(Phase::Observe);
}

// The leg ends: the next leg of the client arm, else the arm.
void EndLeg(ArmId arm) {
    DestroySite();
    Disarm();
    if (arm == ArmId::Client && g_leg < 3) {
        ++g_leg;
        Go(Phase::Spawn);
        return;
    }
    UE_LOGI("[PI-3] DONE %s", ArmName(arm));
    g_active = false;
    Go(Phase::Done);
}

void FinishLeg(ArmId arm, const Spec& spec, const char* label, const Seen* seen, int n) {
    const bool reading = arm == ArmId::Client && g_leg == 3;
    CheckContacts(arm, /*now=*/true);
    LogSeen(label, Complete(spec, seen, n), seen, n, OB::PuppetContacts(), OB::OwnContacts());
    if (reading) {
        char verbs[512];
        FormatVerbs(verbs, sizeof verbs, seen, n);
        UE_LOGI("[PI-3] MP-1 reading own-touch wisp_p %s", verbs);
    } else if (g_failSaid) {
        g_legsOk = false;
    } else {
        const uint32_t now = Counter(spec.counter);
        const char* unmet = Unmet(spec, seen, n, spec.counterMoves ? now > g_baseline : now == g_baseline);
        if (unmet) {
            g_legsOk = false;
            UE_LOGI("[PI-3] FAIL %s unmet %s", label, unmet);
        } else {
            UE_LOGI("[PI-3] PASS %s", label);
        }
    }
    if (arm == ArmId::Client && g_leg == 2 && g_legsOk) UE_LOGI("[PI-3] PASS client");
    EndLeg(arm);
}

void ObserveTick(ArmId arm) {
    const Spec& spec = SpecOf(SpecIdOf(arm, g_leg));
    const bool reading = arm == ArmId::Client && g_leg == 3;
    const bool puppetArm = arm != ArmId::Control && !reading;
    char label[40];
    Label(label, sizeof label, spec, arm == ArmId::Client ? g_leg : 0);
    int n = 0;
    const Seen* seen = OB::Report(n);
    const char* judged = FirstJudged(spec, seen, n);
    if (puppetArm && !judged && OB::OwnContacts() > 0) {
        Abandon(arm, "own-contact");
        return;
    }
    if (judged && !g_failSaid) {
        g_failSaid = true;
        CheckContacts(arm, /*now=*/true);
        UE_LOGI("[PI-3] FAIL %s %s %d", label, judged, Find(seen, n, judged)->firstMs);
    }
    const bool anchored = puppetArm ? OB::PuppetContacts() > 0 : OB::OwnContacts() > 0;
    if (anchored && !g_anchorAt) g_anchorAt = Now();
    if (!g_anchorAt) {
        if (Now() - g_spawnAt < kNoContactMs) return;
        if (reading) {
            CheckContacts(arm, /*now=*/true);
            UE_LOGI("[PI-3] MP-1 reading own-touch wisp_p none");
            EndLeg(arm);
        } else {
            Abandon(arm, "no-contact");
        }
        return;
    }
    if (Now() - g_anchorAt >= static_cast<uint64_t>(spec.horizonMs)) FinishLeg(arm, spec, label, seen, n);
}

void SpawnerTick(coop::net::Session* session, ArmId arm) {
    const bool host = session->role() == coop::net::Role::Host;
    if (g_phase == Phase::Wait) {
        if (const char* what = NotReady(session, arm)) {
            if (Now() - g_worldUpAt >= kWaitBoundMs) AbandonWait(arm, what);
            return;
        }
        g_spawner = true;
        g_latched = true;
        g_active = true;
        Go(Phase::Apart);
        return;
    }
    if (g_phase == Phase::Done || !g_active) return;
    CheckContacts(arm);
    void* pawn = PL::Registry::Get().Local();
    void* body = StillBody(host);
    // The control's site and the client arm's last leg are made at the spawner's own pawn, once it stands apart.
    const bool ownSite = arm == ArmId::Control || (arm == ArmId::Client && g_leg == 3);
    if ((g_phase <= Phase::Rest || !ownSite) && !body) {
        Abandon(arm, "peer-lost");
        return;
    }
    ue_wrap::FVector bodyAt{};
    switch (g_phase) {
    case Phase::Apart: {
        ue_wrap::FVector pawnAt{};
        if (!pawn || !E::TryGetActorLocation(body, bodyAt)) {
            Abandon(arm, "apart");
            return;
        }
        if (!g_walk) {
            const auto ring = DR::ReachableStandpoints(pawn, bodyAt, kRingCm, kRingPoints, kStandReachCm);
            if (ring.empty()) {
                Abandon(arm, "apart");
                return;
            }
            g_walk = DR::StartBackgroundWalk(ring.front(), kStandReachCm, kWalkDeadlineS);
            return;
        }
        const int state = g_walk->state.load();
        if (state == 0) return;
        g_walk.reset();
        if (state != 1 || !E::TryGetActorLocation(pawn, pawnAt) || Dist(pawnAt, bodyAt, true) <= kApartMinCm) {
            Abandon(arm, "apart");
            return;
        }
        Go(Phase::Face);
        return;
    }
    case Phase::Face:
        if (!pawn || !E::TryGetActorLocation(body, bodyAt)) {
            Abandon(arm, "apart");
            return;
        }
        // Load-bearing for the orborb: its timeline end quits the game only when the camera faces the orb.
        E::SetControlRotation(E::GetController(pawn), DR::LookAt(E::GetCameraLocation(), bodyAt));
        g_bodyPos = g_restRef = bodyAt;
        g_restRefAt = Now();
        Go(arm == ArmId::Control ? Phase::Spawn : Phase::Rest);
        return;
    case Phase::Rest: {
        // A puppet is placed by location writes, so its rest is a position delta, not a velocity.
        const bool read = E::TryGetActorLocation(body, bodyAt);
        if (read && Dist(bodyAt, g_restRef) <= kRestMoveCm) {
            if (Now() - g_restRefAt >= kRestStillMs) {
                g_bodyPos = bodyAt;
                Go(Phase::Spawn);
            }
            return;
        }
        if (read) {
            g_restRef = bodyAt;
            g_restRefAt = Now();
        }
        if (Now() - g_phaseAt >= kRestBoundMs) Abandon(arm, "rest");
        return;
    }
    case Phase::Spawn:
        DoSpawn(arm);
        return;
    case Phase::Observe:
        ObserveTick(arm);
        return;
    case Phase::Wait:
    case Phase::Done:
        return;
    }
}

}  // namespace

void Install(coop::net::Session*) {
    if (ArmOf() == ArmId::Off) return;
    OB::Install();
}

void Tick(coop::net::Session* session) {
    const ArmId arm = ArmOf();
    if (arm == ArmId::Off || !session || !session->connected() || (g_latched && !g_active)) return;
    OB::Tick();
    if (!g_worldUpAt) {
        if (!PL::Registry::Get().Local()) return;
        g_worldUpAt = Now();
    }
    const bool host = session->role() == coop::net::Role::Host;
    // The client arm has the client spawn; every other arm has the host spawn.
    if ((arm == ArmId::Client) == !host) SpawnerTick(session, arm);
    else StillTick(session, arm);
}

void OnDisconnect() {
    const ArmId arm = ArmOf();
    if (arm == ArmId::Off) return;
    if (g_active) {
        if (g_spawner) {
            Abandon(arm, "peer-lost");
        } else {
            Disarm();
            g_active = false;
        }
    }
    if (!g_latched) {
        g_worldUpAt = 0;
        g_phase = Phase::Wait;
    }
}

}  // namespace coop::dev::pi3_drill
