// coop/dev/camera_drill.cpp -- see coop/dev/camera_drill.h.

#include "coop/dev/camera_drill.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/director/aimed_grab.h"
#include "coop/dev/director/director.h"
#include "coop/dev/director/routes.h"
#include "coop/net/protocol.h"  // propspawn_flags
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_stick_sync.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/trace.h"
#include "ue_wrap/engine/engine.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace coop::dev::camera_drill {
namespace {

namespace E  = ue_wrap::engine;
namespace D  = coop::director;
namespace pf = coop::net::propspawn_flags;
using ue_wrap::FVector;

constexpr const wchar_t* kCameraPath = L"/Game/objects/prop_camera_bad.prop_camera_bad_C";
constexpr const wchar_t* kCameraClass = L"prop_camera_bad_C";

constexpr int      kProbeDirs     = 8;       // HOST: horizontal probe directions from each origin (45 degrees apart)
constexpr int      kRingDirs      = 8;       // HOST: origins round the player's start: this many yaw steps...
constexpr float    kRingCm[]      = {300.f, 600.f, 900.f};  // ...at each of these distances, besides the start itself
constexpr int      kMaxRouted     = 24;      // HOST: wall candidates a search asks a route of, nearest first
constexpr float    kRoutableProbeCm = 300.f; // HOST: the readiness probe: a route to a point this far off, in any of 8 directions
constexpr float    kRoutableBoundS  = 60.f;  // HOST: world seconds of a routable director without a wall before it gives up
constexpr uint64_t kHostEveryMs   = 1000;
constexpr float    kProbeUpCm     = 40.f;    // HOST: the probe's height above the player's location (about eye height)
constexpr float    kProbeReachCm  = 1500.f;
constexpr float    kMinWallCm     = 120.f;   // HOST: a wall nearer than this to the origin that probed it is not used
constexpr float    kWallFloorZ    = 0.15f;   // HOST: a hit whose normal leans more than this to vertical is no wall
constexpr float    kFacingDot     = 0.5f;    // HOST: the wall must face the probe at least this squarely
constexpr float    kFlatSideCm    = 25.f;    // HOST: the wall is probed this far to each side...
constexpr float    kFlatSlackCm   = 2.f;     // ...and must lie in one plane to within this
constexpr float    kOutCm         = 1.5f;    // the camera's origin stands this far off the wall (the stick trace spans -2.5..+5 cm)
constexpr float    kStandoffCm    = 70.f;    // where a walker stands, off the wall
constexpr float    kRouteReachCm  = 100.f;   // a route must end this near the standpoint, flat
constexpr float    kWalkReachCm   = 80.f;    // the director's stop at the standpoint
constexpr int      kWalkDeadlineS = 300;
constexpr float    kHoldWorldS    = 3.f;     // CLIENT: the hold, on the engine's world clock
constexpr float    kTailWorldS    = 2.5f;    // CLIENT: the watch after the release, on the same clock
constexpr uint64_t kSeenBoundMs   = 120000;  // CLIENT: world-ready to a stuck camera in its world
constexpr uint64_t kClockBoundMs  = 30000;   // CLIENT: the world clock must move the hold's or the tail's span within this
constexpr uint64_t kCheckEveryMs  = 250;
constexpr uint64_t kScanEveryMs   = 1000;
constexpr int      kMaxScanned    = 16;

enum class HStep : uint8_t { Wait, Done };
enum class CStep : uint8_t { Ready, Find, Walk, Grab, Hold, Tail, Done };
HStep    g_host = HStep::Wait;
CStep    g_client = CStep::Ready;
uint64_t g_stepMs = 0;
uint64_t g_nextCheckMs = 0;
uint64_t g_nextScanMs = 0;
float    g_routableSinceS = -1.f;  // HOST: the world clock when the director first routed
int      g_searches = 0;           // HOST: wall searches that found none
bool     g_saidWaiting = false;    // HOST: the wait for the director said once
ue_wrap::CachedObjRef g_hostCam;   // HOST: the camera it hung
ue_wrap::CachedObjRef g_cam;       // CLIENT: the camera it walks to, grabs and watches
std::shared_ptr<D::BackgroundWalk> g_walk;
std::unique_ptr<D::AimedGrab> g_grab;
float    g_t0 = 0.f;            // CLIENT: the world clock at the grab, then at the release
bool     g_restuck = false;     // CLIENT: a stuck sample between the grab and the release
bool     g_stuckEnd = false;    // CLIENT: the last stuck sample after the release

bool Enabled() {
    static const bool on = coop::config::ResolveString(::coop::config_registry::rows::camera_drill) == "grab";
    return on;
}
bool Expired(uint64_t bound) { return ::GetTickCount64() - g_stepMs > bound; }
void CGo(CStep s) { g_client = s; g_stepMs = ::GetTickCount64(); }

void Fail(const char* role, const char* what) {
    UE_LOGW("[CAMERA-DRILL] FAIL %s: %s", role, what);
    g_host = HStep::Done;
    g_client = CStep::Done;
}
void Abandon(const char* why) {
    UE_LOGW("[CAMERA-DRILL] ABANDONED %s", why);
    g_host = HStep::Done;
    g_client = CStep::Done;
}

bool Due(uint64_t every) {
    const uint64_t now = ::GetTickCount64();
    if (now < g_nextCheckMs) return false;
    g_nextCheckMs = now + every;
    return true;
}

// What the engine says of a camera: the prop's frozen or static flag (the wall-attach component keeps no flag of its
// own -- it writes these two on the prop), and the actor it is attached to (a real stick attaches it to the wall it
// found; the raw frozen-write fallback attaches it to nothing).
bool StuckFlags(void* cam) { return ue_wrap::prop::IsFrozen(cam) || ue_wrap::prop::IsStatic(cam); }
bool AttachedToSomething(void* cam) {
    void* parent = nullptr;
    return E::GetAttachParentActor(cam, parent) && parent != nullptr;
}
std::wstring KeyOf(void* cam) { return ue_wrap::prop::GetKeyString(cam); }

// ---- HOST ------------------------------------------------------------------------------------------------

struct Wall {
    float   dist = 0.f;
    FVector point{};
    FVector n{};  // the wall's normal, flat and unit, facing the player's start
};

float Dot2(const FVector& a, float x, float y) { return a.X * x + a.Y * y; }

bool Probe(void* me, const FVector& from, float dx, float dy, void* const* ignore, int nIgnore,
           ue_wrap::trace::Hit& hit) {
    const uint8_t types[1] = {0};  // WorldStatic only: level geometry, which never simulates
    const FVector to{from.X + dx * kProbeReachCm, from.Y + dy * kProbeReachCm, from.Z};
    return ue_wrap::trace::LineHitForObjects(me, from, to, types, 1, ignore, nIgnore, &hit) && hit.blocked;
}

// What one wall search found and why each probe failed, for the one line the host says when it gives up.
struct Tally {
    int origins = 0, probes = 0;
    int noHit = 0, slope = 0, facing = 0, tooNear = 0, notPlanar = 0;  // probes rejected, by the first test they failed
    int walls = 0, routed = 0, noRoute = 0, untried = 0;            // flat walls, those asked a route, and the outcomes
};

float Dist2D(const FVector& a, const FVector& b) { return std::sqrt((a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y)); }

// The probe origins: the player's start, then a ring of kRingDirs yaw steps at each of kRingCm.
std::vector<FVector> Origins(const FVector& start) {
    std::vector<FVector> o{start};
    for (const float d : kRingCm)
        for (int j = 0; j < kRingDirs; ++j) {
            const float ang = 6.28318530718f * static_cast<float>(j) / static_cast<float>(kRingDirs);
            o.push_back({start.X + std::cos(ang) * d, start.Y + std::sin(ang) * d, start.Z});
        }
    return o;
}

// The walls the probes find that are flat, face their origin and lie in one plane across 50 cm, nearest to `at` first.
std::vector<Wall> FlatWalls(void* me, const FVector& at, const FVector& start, Tally& t) {
    std::vector<Wall> walls;
    void* ignore[coop::players::kMaxPeers];
    const int nIgnore = coop::players::Registry::Get().LiveBodies(ignore, coop::players::kMaxPeers);
    for (const FVector& origin : Origins(start)) {
        ++t.origins;
        for (int i = 0; i < kProbeDirs; ++i) {
            ++t.probes;
            const float ang = 6.28318530718f * static_cast<float>(i) / static_cast<float>(kProbeDirs);
            const float dx = std::cos(ang), dy = std::sin(ang);
            ue_wrap::trace::Hit hit;
            if (!Probe(me, origin, dx, dy, ignore, nIgnore, hit)) { ++t.noHit; continue; }
            const float len = std::sqrt(hit.normal.X * hit.normal.X + hit.normal.Y * hit.normal.Y);
            if (len < 0.1f || std::fabs(hit.normal.Z) > kWallFloorZ) { ++t.slope; continue; }
            Wall w;
            w.n = {hit.normal.X / len, hit.normal.Y / len, 0.f};
            w.point = hit.point;
            w.dist = Dist2D(hit.point, at);
            if (Dist2D(hit.point, origin) < kMinWallCm) { ++t.tooNear; continue; }
            if (Dot2(w.n, dx, dy) > -kFacingDot) { ++t.facing; continue; }
            bool flat = true;
            for (const float off : {-kFlatSideCm, kFlatSideCm}) {
                const FVector side{origin.X - w.n.Y * off, origin.Y + w.n.X * off, origin.Z};
                ue_wrap::trace::Hit sh;
                if (!Probe(me, side, dx, dy, ignore, nIgnore, sh) ||
                    std::fabs(Dot2(w.n, sh.point.X - w.point.X, sh.point.Y - w.point.Y)) > kFlatSlackCm) {
                    flat = false;
                    break;
                }
            }
            if (!flat) { ++t.notPlanar; continue; }
            walls.push_back(w);
            ++t.walls;
        }
    }
    std::sort(walls.begin(), walls.end(), [](const Wall& a, const Wall& b) { return a.dist < b.dist; });
    return walls;
}

// Whether the director can route at all from the player's start: a route to a point kRoutableProbeCm off, in any of
// eight directions. Before the NavMesh is up no route exists and a wall search would only fail on it.
bool DirectorRoutes(void* me, const FVector& at) {
    for (int i = 0; i < 8; ++i) {
        const float ang = 6.28318530718f * static_cast<float>(i) / 8.f;
        const FVector to{at.X + std::cos(ang) * kRoutableProbeCm, at.Y + std::sin(ang) * kRoutableProbeCm, at.Z};
        if (D::RouteInLegs(me, at, to, kRouteReachCm)) return true;
    }
    return false;
}

// The walker's standpoint for a camera at `at` whose back is to the wall facing `n`.
FVector Standpoint(const FVector& at, float nx, float ny) {
    return {at.X + nx * kStandoffCm, at.Y + ny * kStandoffCm, at.Z};
}

void HostTick(coop::net::Session* s) {
    if (!s->running()) return;
    void* me = coop::players::Registry::Get().Local();
    FVector at{};
    if (!me || !E::TryGetActorLocation(me, at)) return;
    void* cls = ue_wrap::asset_load::LoadObjectByPath(kCameraPath);
    if (!cls) { Abandon("host: prop_camera_bad_C did not load"); return; }

    // Readiness, not a clock: nothing is searched until the director can route at all.
    float worldNow = 0.f;
    if (!E::TryGetWorldTimeSeconds(worldNow)) return;
    if (!DirectorRoutes(me, at)) {
        if (!g_saidWaiting) {
            g_saidWaiting = true;
            UE_LOGI("camera_drill: host waits for the director to route from the player's start");
        }
        return;
    }
    if (g_routableSinceS < 0.f) {
        g_routableSinceS = worldNow;
        UE_LOGI("camera_drill: host: the director routes; searching for a wall (retried every second)");
    }

    const FVector probe{at.X, at.Y, at.Z + kProbeUpCm};
    Tally t;
    const std::vector<Wall> walls = FlatWalls(me, at, probe, t);
    const Wall* chosen = nullptr;
    for (const Wall& w : walls) {
        if (t.routed >= kMaxRouted) { ++t.untried; continue; }
        ++t.routed;
        if (D::RouteInLegs(me, at, Standpoint(w.point, w.n.X, w.n.Y), kRouteReachCm)) { chosen = &w; break; }
        ++t.noRoute;
    }
    if (!chosen) {
        ++g_searches;
        if (worldNow - g_routableSinceS < kRoutableBoundS) return;
        UE_LOGW("camera_drill: host gave up after %d searches -- last search: %d origins, %d probes: %d hit nothing, "
                "%d on a slope, %d too near, %d facing away, %d not in one plane; %d flat walls, %d asked a route "
                "(%d refused), %d left untried", g_searches, t.origins, t.probes, t.noHit, t.slope, t.tooNear, t.facing,
                t.notPlanar, t.walls, t.routed, t.noRoute, t.untried);
        Abandon("host: the director routed for 60 s of world time and no wall near the player's start was flat, "
                "facing its probe and at the end of a route");
        return;
    }
    UE_LOGI("camera_drill: host wall at (%.0f, %.0f, %.0f) %.0f cm out, normal (%.2f, %.2f); %d flat walls, %d asked a "
            "route, search %d", chosen->point.X, chosen->point.Y, chosen->point.Z, chosen->dist, chosen->n.X,
            chosen->n.Y, t.walls, t.routed, g_searches + 1);

    // The camera's back is its stick arrow (yaw 180 off the actor's forward): the actor faces away from the wall.
    const FVector origin{chosen->point.X + chosen->n.X * kOutCm, chosen->point.Y + chosen->n.Y * kOutCm,
                         chosen->point.Z};
    const ue_wrap::FRotator rot{0.f, std::atan2(chosen->n.Y, chosen->n.X) * 57.2957795131f, 0.f};
    void* cam = E::BeginDeferredSpawn(cls, origin, rot);
    if (!cam || !E::FinishDeferredSpawn(cam, origin, rot)) {
        Abandon("host: the camera did not spawn");
        return;
    }
    g_hostCam.Set(cam);
    g_host = HStep::Done;

    const std::wstring key = KeyOf(cam);
    if (key.empty() || key == L"None") {
        Fail("host", "the camera did not stick (it carries no prop key, so it cannot cross to a client)");
        return;
    }
    const bool before = StuckFlags(cam);
    coop::prop_stick_sync::ConvergeStuck(cam, static_cast<uint8_t>(pf::kLiveState | pf::kFrozen));
    const bool flags = StuckFlags(cam);
    const bool attached = AttachedToSomething(cam);
    UE_LOGI("camera_drill: host stick read -- stuck before the converge=%d, flags after=%d, attached to a wall actor=%d",
            before ? 1 : 0, flags ? 1 : 0, attached ? 1 : 0);
    if (!flags) {
        Fail("host", "the camera did not stick (the component's forceStick found no surface within its trace)");
        return;
    }
    if (!attached) {
        Fail("host", "the camera did not stick (it is frozen where it stands and attached to no wall: the raw fallback)");
        return;
    }
    FVector now{};
    E::TryGetActorLocation(cam, now);
    UE_LOGI("camera_drill: host READY stuck key=%ls at=%.0f,%.0f,%.0f", key.c_str(), now.X, now.Y, now.Z);
}

// ---- CLIENT ----------------------------------------------------------------------------------------------

struct Scan {
    void* found[kMaxScanned];
    int   n = 0;
};

void CollectCameras(void* ctx, void* obj, int32_t) {
    auto* sc = static_cast<Scan*>(ctx);
    if (sc->n < kMaxScanned) sc->found[sc->n++] = obj;
}

// The nearest stuck prop_camera_bad_C to `from` in this world; null when none. `stuckSeen` counts the stuck ones.
void* NearestStuckCamera(const FVector& from, int& stuckSeen) {
    stuckSeen = 0;
    void* cls = ue_wrap::object_index::ClassByName(kCameraClass);
    if (!cls) return nullptr;
    Scan sc;
    ue_wrap::object_index::ForEachInstance(cls, &CollectCameras, &sc);
    void* best = nullptr;
    float bestD2 = 0.f;
    for (int i = 0; i < sc.n; ++i) {
        FVector p{};
        if (!StuckFlags(sc.found[i]) || !E::TryGetActorLocation(sc.found[i], p)) continue;
        ++stuckSeen;
        const float d2 = (p.X - from.X) * (p.X - from.X) + (p.Y - from.Y) * (p.Y - from.Y) +
                         (p.Z - from.Z) * (p.Z - from.Z);
        if (!best || d2 < bestD2) { best = sc.found[i]; bestD2 = d2; }
    }
    return best;
}

void FindStep(void* player) {
    if (::GetTickCount64() < g_nextScanMs) return;
    g_nextScanMs = ::GetTickCount64() + kScanEveryMs;
    FVector me{};
    if (!E::TryGetActorLocation(player, me)) return;
    int stuckSeen = 0;
    void* cam = NearestStuckCamera(me, stuckSeen);
    if (!cam) {
        if (Expired(kSeenBoundMs)) Fail("client", "no stuck camera ever stood in this world");
        return;
    }
    g_cam.Set(cam);
    UE_LOGI("camera_drill: client sees stuck key=%ls", KeyOf(cam).c_str());
    UE_LOGI("camera_drill: client's copy -- %d stuck camera(s) in this world, attached to a wall actor=%d", stuckSeen,
            AttachedToSomething(cam) ? 1 : 0);

    FVector camAt{};
    const FVector fwd = E::GetActorForwardVector(cam);
    const float len = std::sqrt(fwd.X * fwd.X + fwd.Y * fwd.Y);
    if (!E::TryGetActorLocation(cam, camAt) || len < 0.1f) {
        Abandon("client: the camera's place or facing did not read");
        return;
    }
    const FVector stand = Standpoint(camAt, fwd.X / len, fwd.Y / len);
    if (!D::RouteInLegs(player, me, stand, kRouteReachCm)) {
        Abandon("client: no NavMesh route reaches the camera's wall");
        return;
    }
    g_walk = D::StartBackgroundWalk(stand, kWalkReachCm, kWalkDeadlineS);
    CGo(CStep::Walk);
}

void ClientTick(void* player) {
    switch (g_client) {
    case CStep::Ready:
        if (!coop::net_pump::HasAnnouncedWorldReady() || coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle)
            return;
        CGo(CStep::Find);
        return;
    case CStep::Find:
        FindStep(player);
        return;
    case CStep::Walk: {
        const int st = g_walk ? g_walk->state.load() : 2;
        if (st == 0) return;
        if (st == 2) { Abandon("client: the walk to the camera did not arrive"); return; }
        void* cam = g_cam.Get();
        if (!cam) { Abandon("client: the camera died during the walk"); return; }
        g_grab = std::make_unique<D::AimedGrab>(player, cam);
        CGo(CStep::Grab);
        return;
    }
    case CStep::Grab: {
        const D::GrabState st = g_grab->Tick();
        if (st == D::GrabState::Working) return;
        if (st == D::GrabState::Failed) { Abandon(g_grab->Why()); return; }
        void* cam = g_cam.Get();
        float now = 0.f;
        if (!cam || !E::TryGetWorldTimeSeconds(now)) { Abandon("client: the camera or the world clock did not read at the grab"); return; }
        const bool stuck = StuckFlags(cam);
        UE_LOGI("camera_drill: client grabbed key=%ls stuck_at_grab=%d", KeyOf(cam).c_str(), stuck ? 1 : 0);
        if (stuck) { Fail("client", "the grab left the camera stuck: its unstick did not run, so no re-stick is measured"); return; }
        g_t0 = now;
        g_restuck = false;
        CGo(CStep::Hold);
        return;
    }
    case CStep::Hold: {
        void* cam = g_cam.Get();
        float now = 0.f;
        if (!cam || !E::TryGetWorldTimeSeconds(now)) { Abandon("client: the camera or the world clock did not read while held"); return; }
        if (!g_restuck && StuckFlags(cam)) {
            g_restuck = true;
            UE_LOGI("camera_drill: re-stuck while held at +%.0fms", static_cast<double>(now - g_t0) * 1000.0);
        }
        if (now - g_t0 < kHoldWorldS) {
            if (Expired(kClockBoundMs)) Fail("client", "the world clock did not advance through the hold");
            return;
        }
        const bool held = D::Grabbing(player) == cam;
        if (!D::CallOnPlayer(player, L"dropGrabObject")) { Abandon("client: dropGrabObject did not run"); return; }
        UE_LOGI("camera_drill: client at the release -- still held=%d, stuck=%d", held ? 1 : 0, StuckFlags(cam) ? 1 : 0);
        UE_LOGI("camera_drill: client released key=%ls", KeyOf(cam).c_str());
        g_t0 = now;
        g_stuckEnd = false;
        CGo(CStep::Tail);
        return;
    }
    case CStep::Tail: {
        void* cam = g_cam.Get();
        float now = 0.f;
        if (!cam || !E::TryGetWorldTimeSeconds(now)) { Abandon("client: the camera or the world clock did not read in the tail"); return; }
        g_stuckEnd = StuckFlags(cam);
        if (now - g_t0 < kTailWorldS) {
            if (Expired(kClockBoundMs)) Fail("client", "the world clock did not advance through the tail");
            return;
        }
        UE_LOGI("camera_drill: client end state -- stuck=%d, attached to a wall actor=%d", g_stuckEnd ? 1 : 0,
                AttachedToSomething(cam) ? 1 : 0);
        UE_LOGI("[CAMERA-DRILL] client DONE stuck_after_grab=%d", g_restuck ? 1 : 0);
        g_client = CStep::Done;
        return;
    }
    default:
        return;
    }
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (!Enabled() || !s) return;
    if (s->role() == coop::net::Role::Host) {
        if (g_host == HStep::Wait && Due(kHostEveryMs)) HostTick(s);
        return;
    }
    if (!s->connected() || g_client == CStep::Done) return;
    void* player = coop::players::Registry::Get().Local();
    if (!player) return;
    if (g_client == CStep::Ready && !Due(kCheckEveryMs)) return;
    ClientTick(player);
}

void OnDisconnect() {
    if (!Enabled()) return;
    if (g_walk && g_walk->state.load() == 0) D::CancelBackgroundWalk(*g_walk);
    g_walk.reset();
    g_grab.reset();
    g_client = CStep::Ready;
    g_stepMs = g_nextCheckMs = g_nextScanMs = 0;
    g_restuck = g_stuckEnd = false;
    g_t0 = 0.f;
    g_cam.Reset();
    // The host's camera outlives a client, so a rejoin's client finds it again; one that is gone is hung afresh.
    if (!g_hostCam.Get()) {
        g_host = HStep::Wait;
        g_routableSinceS = -1.f;
        g_searches = 0;
        g_saidWaiting = false;
    }
}

}  // namespace coop::dev::camera_drill
