// coop/dev/remover_drill.cpp -- see coop/dev/remover_drill.h.

#include "coop/dev/remover_drill.h"

#include "coop/comms/chat_feed.h"
#include "coop/comms/chat_log.h"
#include "coop/comms/chat_sync.h"
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/spawn_npc.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/element/registry.h"
#include "coop/player/players_registry.h"
#include "coop/player/puppet_drive.h"
#include "coop/player/remote_player.h"
#include "coop/props/remover_request.h"
#include "coop/props/prop_snapshot.h"
#include "coop/save/join_window_baseline.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/trace.h"
#include "ue_wrap/desk/dish_writers.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_attach.h"
#include "ue_wrap/engine/hit_result.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace coop::dev::remover_drill {
namespace {

namespace E  = ue_wrap::engine;
namespace R  = ue_wrap::reflection;
namespace DW = ue_wrap::dish_writers;
namespace EL = coop::element;
using ue_wrap::FVector;

enum class Phase : uint8_t { Off, Granted, Withheld };

Phase PhaseOf() {
    static const Phase p = [] {
        const std::string v = coop::config::ResolveEnum(::coop::config_registry::rows::remover_drill);
        return v == "granted" ? Phase::Granted : v == "withheld" ? Phase::Withheld : Phase::Off;
    }();
    return p;
}
const char* PhaseName() { return PhaseOf() == Phase::Granted ? "granted" : "withheld"; }

// Three plain props of three classes (none trash, kerfur or keyed-special), one per role: the single prop a medium sack,
// the mashed one a small sack, the ring plain sacks. Sacks settle at a spawn at floor height; a dumbbell and a beer
// bottle spawned the same way fall through the floor on the host and are destroyed far below it.
constexpr const wchar_t* kSinglePath  = L"/Game/objects/prop_pointSack_m.prop_pointSack_m_C";
constexpr const wchar_t* kSingleClass = L"prop_pointSack_m_C";
constexpr const wchar_t* kMashPath    = L"/Game/objects/prop_pointSack_s.prop_pointSack_s_C";
constexpr const wchar_t* kMashClass   = L"prop_pointSack_s_C";
constexpr const wchar_t* kSackPath    = L"/Game/objects/prop_pointSack.prop_pointSack_C";
constexpr const wchar_t* kSackClass   = L"prop_pointSack_C";
constexpr const wchar_t* kToolPath    = L"/Game/objects/tool_remover.tool_remover_C";
constexpr const wchar_t* kToolClass   = L"tool_remover_C";
constexpr const wchar_t* kToolgunPath = L"/Game/objects/prop_toolgun.prop_toolgun_C";
constexpr const wchar_t* kNpcClass    = L"kerfurOmega_C";

// The notices the remover shows, by their text: the screen's own words, the drill's reading of them.
constexpr const char* kNoRight     = "The host does not allow the remover";
constexpr const char* kOutOfSight  = "You can't see that from there";
constexpr const char* kUnsupported = "The remover cannot remove that in multiplayer";

// The host's layout about its client's puppet, in centimetres along its facing: the single and the mashed props side by
// side ahead of it, the creature straight ahead beyond them, a ring of kRing about it clear of the cone it faces. The
// client tells the roles by class (kSingleClass, kMashClass, the sacks), never by where physics left them.
constexpr int   kRing         = 28;
constexpr int   kSweepFloor   = 10;    // the fewest ring props in sight that make the sweep a measurement
constexpr int   kMashShots    = 6;
constexpr float kSingleAheadCm = 120.f;
constexpr float kMashAheadCm   = 190.f;
constexpr float kPairSideCm    = 45.f;   // either side of the facing line, so neither prop stands in the other's way
constexpr float kRingConeDeg   = 30.f;   // the ring keeps this far off the facing line, each side
constexpr float kRingNearCm   = 260.f;
constexpr float kRingStepCm   = 30.f;
constexpr float kNpcAheadCm   = 330.f;   // the creature stands straight ahead, beyond the pair and inside the ring-free cone
constexpr float kNpcUpCm      = 60.f;    // the height of its middle above the floor, for the clear-segment test
constexpr float kTargetUpCm   = 30.f;    // the height of a sack's middle above the floor, for the same test
constexpr float kDropCm       = 40.f;  // above the floor, so a prop settles
constexpr float kFindRadiusCm = 700.f;
constexpr float kSightSlackCm = 25.f;
constexpr float kAimPastCm    = 50.f;   // the press aims this far past the target's middle
constexpr float kNearCm       = 400.f;  // host: props this near the eye are listed once per layout
constexpr int   kNearSaid     = 12;     // ...at most this many
constexpr float kRestMoveCm   = 1.f;    // a prop that moved less than this between two reads is at rest
constexpr int   kRestChecks   = 4;      // ...that many reads in a row, a check apart

constexpr uint64_t kFindBoundMs  = 60000;   // CLIENT: the join's end to every prop the host spawned being here
constexpr uint64_t kLegBoundMs   = 30000;   // CLIENT: a shot to what it must show (a failure bound, never a wait)
constexpr uint64_t kAwaitBoundMs = 900000;  // HOST: the spawn to the client's closing line
constexpr uint64_t kCheckEveryMs = 250;

std::string EndLine() { return std::string("remover-drill end ") + PhaseName(); }

int  g_session = 1;
bool g_done = false;
uint64_t g_stepMs = 0;
uint64_t g_nextCheckMs = 0;

bool Expired(uint64_t bound) { return ::GetTickCount64() - g_stepMs > bound; }

void End(const char* role, const char* word, const char* what, bool warn) {
    if (warn) UE_LOGW("[REMOVER-DRILL] %s %s %s in session %d: %s", role, word, PhaseName(), g_session, what);
    else      UE_LOGI("[REMOVER-DRILL] %s %s %s in session %d: %s", role, word, PhaseName(), g_session, what);
    g_done = true;
}

float Dist(const FVector& a, const FVector& b) {
    const float dx = a.X - b.X, dy = a.Y - b.Y, dz = a.Z - b.Z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ---- host -------------------------------------------------------------------------------------------------------

enum class HostStep : uint8_t { Arm, Await };
HostStep g_hostStep = HostStep::Arm;
ue_wrap::CachedObjRef g_hSingle, g_hMash, g_hNpc;
ue_wrap::CachedObjRef g_hRing[kRing];

// What testing one spot found: whether it stands, the floor under it, and, when it does not, the branch and the blocker.
struct Spot {
    bool         ok = false;
    float        floorZ = 0.f;
    const char*  why = "";
    std::wstring blocker;
    float        blockedAt = 0.f, toPoint = 0.f;
};

// How far under the eye a spot's floor is looked for.
constexpr float kFloorSearchCm = 500.f;

// The floor under the column at (x, y): the first WorldStatic or WorldDynamic thing a trace meets going down from `fromZ`,
// every player body ignored. False when none is met, or when the column starts inside geometry.
bool FloorAt(void* puppet, float x, float y, float fromZ, float* floorZ, const char** why) {
    const uint8_t types[2] = {0, 1};
    void* ignore[coop::players::kMaxPeers];
    const int nIgnore = coop::players::Registry::Get().LiveBodies(ignore, coop::players::kMaxPeers);
    ue_wrap::trace::Hit hit;
    if (!ue_wrap::trace::LineHitForObjects(puppet, {x, y, fromZ}, {x, y, fromZ - kFloorSearchCm}, types, 2, ignore,
                                           nIgnore, &hit)) {
        *why = "the floor trace did not run";
        return false;
    }
    if (!hit.blocked) { *why = "no floor within 500 cm under the spot"; return false; }
    if (Dist({x, y, fromZ}, hit.point) < 5.f) { *why = "the spot's column starts inside geometry"; return false; }
    *floorZ = hit.point.Z;
    return true;
}

// Can a prop stand at (x, y), `up` cm above the floor found there, and be seen from `eye`? The floor is found by a trace
// of its own, never read off the puppet's bounds, and the segment from the eye to the prop's middle is the judge's: the four
// object types, every player body ignored, nothing met before the point.
Spot CheckSpot(void* puppet, const FVector& eye, float x, float y, float up) {
    Spot s;
    if (!FloorAt(puppet, x, y, eye.Z, &s.floorZ, &s.why)) return s;
    const FVector pt{x, y, s.floorZ + up};
    const uint8_t types[4] = {0, 1, 2, 3};
    void* ignore[coop::players::kMaxPeers];
    const int nIgnore = coop::players::Registry::Get().LiveBodies(ignore, coop::players::kMaxPeers);
    ue_wrap::trace::Hit hit;
    if (!ue_wrap::trace::LineHitForObjects(puppet, eye, pt, types, 4, ignore, nIgnore, &hit)) {
        s.why = "the segment trace did not run";
        return s;
    }
    s.toPoint = Dist(eye, pt);
    if (hit.blocked && Dist(eye, hit.point) < s.toPoint - kSightSlackCm) {
        s.why = "the segment is blocked";
        s.blockedAt = Dist(eye, hit.point);
        s.blocker = hit.actor ? R::ClassNameOf(hit.actor) : std::wstring(L"(no actor)");
        return s;
    }
    s.ok = true;
    return s;
}

// The yaw steps, in degrees, a layout is tried at round the puppet's facing: straight ahead first, then alternating
// sides until the whole circle is covered.
constexpr float kFanDeg[] = {0.f, 25.f, -25.f, 50.f, -50.f, 75.f, -75.f, 100.f, -100.f, 130.f, -130.f, 160.f, -160.f, 180.f};
constexpr int kFanCount = static_cast<int>(sizeof(kFanDeg) / sizeof(kFanDeg[0]));

// Says what one tested direction found, one line a direction: a layout is attempted once, so the lines are bounded.
void SayDirection(float deg, const char* which, const Spot& s, const FVector& eye) {
    if (s.blocker.empty()) {
        UE_LOGI("[REMOVER-DRILL] host: direction %.0f deg, %s: %s (floor %.0f, eye %.0f)", deg, which,
                s.ok ? "clear" : s.why, s.floorZ, eye.Z);
        return;
    }
    UE_LOGI("[REMOVER-DRILL] host: direction %.0f deg, %s: %s -- %ls stands %.0f cm from the eye, the point is %.0f cm away "
            "(floor %.0f, eye %.0f)", deg, which, s.why, s.blocker.c_str(), s.blockedAt, s.toPoint, s.floorZ, eye.Z);
}

// Spawns the host's layout about the client's puppet, by its facing: the single and the mashed props side by side ahead
// of it (the single nearer), the creature straight ahead beyond them, and the ring about it clear of the cone it faces.
// The pair and the creature are laid only where each spot has a floor of its own and the segment from the puppet's eye
// to it is clear -- the trace the judge runs -- trying the facing and then a fan round it, so the drill measures the
// lane and not the map. The fan is walked from step `start` (a re-lay passes the step after the last), and `*chosen` is the
// step that was laid. Null on success; else the reason nothing was laid.
const char* SpawnLayout(coop::RemotePlayer& rp, const FVector& at, int start, int* chosen) {
    void* puppet = rp.GetActor();
    void* singleCls = ue_wrap::asset_load::LoadObjectByPath(kSinglePath);
    void* mashCls = ue_wrap::asset_load::LoadObjectByPath(kMashPath);
    void* ringCls = ue_wrap::asset_load::LoadObjectByPath(kSackPath);
    if (!singleCls || !mashCls || !ringCls) return "a prop class of the layout did not load";
    FVector eye{};
    if (!rp.TryGetEyePosition(eye)) return "the sender's eye did not read on the host";
    FVector fwd0 = E::GetActorForwardVector(puppet);
    const float flat = std::sqrt(fwd0.X * fwd0.X + fwd0.Y * fwd0.Y);
    if (flat < 0.01f) fwd0 = {1.f, 0.f, 0.f};
    else fwd0 = {fwd0.X / flat, fwd0.Y / flat, 0.f};
    const bool granted = PhaseOf() == Phase::Granted;
    UE_LOGI("[REMOVER-DRILL] host: laying the layout -- puppet at (%.0f, %.0f, %.0f), its eye at (%.0f, %.0f, %.0f)", at.X,
            at.Y, at.Z, eye.X, eye.Y, eye.Z);

    FVector fwd{}, right{};
    Spot single, mash, npc;
    bool found = false;
    for (int fan = start; fan < kFanCount; ++fan) {
        const float deg = kFanDeg[fan];
        const float rad = deg * 0.0174532925f;
        fwd = {fwd0.X * std::cos(rad) - fwd0.Y * std::sin(rad), fwd0.X * std::sin(rad) + fwd0.Y * std::cos(rad), 0.f};
        right = {-fwd.Y, fwd.X, 0.f};
        const auto where = [&](float ahead, float side) {
            return FVector{at.X + fwd.X * ahead + right.X * side, at.Y + fwd.Y * ahead + right.Y * side, 0.f};
        };
        const FVector p1 = where(kSingleAheadCm, kPairSideCm);
        single = CheckSpot(puppet, eye, p1.X, p1.Y, kTargetUpCm);
        SayDirection(deg, "the single prop", single, eye);
        if (!single.ok) continue;
        if (granted) {
            const FVector p2 = where(kMashAheadCm, -kPairSideCm);
            mash = CheckSpot(puppet, eye, p2.X, p2.Y, kTargetUpCm);
            SayDirection(deg, "the mashed prop", mash, eye);
            if (!mash.ok) continue;
            const FVector p3 = where(kNpcAheadCm, 0.f);
            npc = CheckSpot(puppet, eye, p3.X, p3.Y, kNpcUpCm);
            SayDirection(deg, "the creature", npc, eye);
            if (!npc.ok) continue;
        }
        found = true;
        *chosen = fan;
        break;
    }
    if (!found) return "no direction round the puppet has a floor and a clear segment from its eye to the layout";

    // The props the host holds within 400 cm of the eye, once per layout: what the client's own trace can be compared with
    // when it meets something the host's did not.
    {
        std::vector<EL::Registry::ActorIdPair> rows;
        EL::Registry::Get().SnapshotActorsByType(EL::ElementType::Prop, rows);
        int said = 0;
        for (const auto& r : rows) {
            FVector p{};
            if (said >= kNearSaid || !r.actor || !R::IsLiveByIndex(r.actor, r.internalIdx) ||
                !E::TryGetActorLocation(r.actor, p) || Dist(p, eye) > kNearCm)
                continue;
            ++said;
            UE_LOGI("[REMOVER-DRILL] host: near the eye: %ls, element %u, at (%.0f, %.0f, %.0f), %.0f cm away",
                    R::ClassNameOf(r.actor).c_str(), static_cast<unsigned>(r.id), p.X, p.Y, p.Z, Dist(p, eye));
        }
    }
    // The ring sits on the floor under the puppet's own column, found the same way.
    float ringFloor = single.floorZ;
    const char* ringWhy = "";
    if (granted && !FloorAt(puppet, at.X, at.Y, eye.Z, &ringFloor, &ringWhy)) ringFloor = single.floorZ;
    const auto spawn = [&](void* cls, float ahead, float side, float floorZ, ue_wrap::CachedObjRef& into) {
        void* p = E::SpawnActor(cls, {at.X + fwd.X * ahead + right.X * side, at.Y + fwd.Y * ahead + right.Y * side,
                                      floorZ + kDropCm});
        into.Set(p);
        return p != nullptr;
    };
    if (!spawn(singleCls, kSingleAheadCm, kPairSideCm, single.floorZ, g_hSingle)) return "the single prop did not spawn";
    if (!granted) return nullptr;
    if (!spawn(mashCls, kMashAheadCm, -kPairSideCm, mash.floorZ, g_hMash)) return "the mashed prop did not spawn";
    for (int i = 0; i < kRing; ++i) {
        const float a = (kRingConeDeg + (360.f - 2.f * kRingConeDeg) * static_cast<float>(i) /
                                            static_cast<float>(kRing - 1)) * 0.0174532925f;
        const float r = kRingNearCm + kRingStepCm * static_cast<float>(i % 3);
        if (!spawn(ringCls, std::cos(a) * r, std::sin(a) * r, ringFloor, g_hRing[i])) return "a ring prop did not spawn";
    }
    void* npcActor = coop::dev::spawn_npc::SpawnAt(kNpcClass, {at.X + fwd.X * kNpcAheadCm, at.Y + fwd.Y * kNpcAheadCm,
                                                               npc.floorZ + kDropCm});
    g_hNpc.Set(npcActor);
    return npcActor ? nullptr : "the creature did not spawn";
}

bool ClientEnded() {
    const std::string end = EndLine();
    bool found = false;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) {
        if (r.slot == 1 && r.text == end) found = true;
    });
    return found;
}

// The host's own count of its answers to slot 1, by reason.
uint32_t Answered(coop::net::RemoverReason r) { return coop::remover_request::AnsweredFor(1, r); }

// The prop is gone on the host AND the host performed it: the answers it gave are the host's own word, so a removal
// the client did natively and the host merely applied cannot pass.
void HostJudge() {
    using RR = coop::net::RemoverReason;
    char why[240];
    const uint32_t removed = Answered(RR::Removed);
    const uint32_t noRight = Answered(RR::NoRight);
    const uint32_t other = Answered(RR::NotReady) + Answered(RR::Unsupported) + Answered(RR::Held) +
                           Answered(RR::OutOfSight) + Answered(RR::Failed);
    if (PhaseOf() == Phase::Withheld) {
        if (g_hSingle.Get() && removed == 0 && noRight >= 1)
            End("host", "PASS", "the prop the host's permissions refuse stands here; NoRight was answered", false);
        else {
            std::snprintf(why, sizeof(why), "refused prop standing=%d, Removed answered %u, NoRight answered %u",
                          g_hSingle.Get() ? 1 : 0, removed, noRight);
            End("host", "FAIL", why, true);
        }
        return;
    }
    int ringGone = 0;
    for (const auto& r : g_hRing)
        if (!r.Get()) ++ringGone;
    const bool single = !g_hSingle.Get(), mash = !g_hMash.Get(), npc = g_hNpc.Get() != nullptr;
    // One Removed for the single prop, one for the mashed one (the rest of its presses were answered Gone), one per ring
    // prop; the sweep's presses were each judged in sight, and the creature was answered Unsupported.
    const uint32_t wantRemoved = 2u + static_cast<uint32_t>(ringGone);
    if (single && mash && ringGone >= kSweepFloor && npc && removed == wantRemoved && other == Answered(RR::Unsupported) &&
        Answered(RR::Unsupported) >= 1) {
        std::snprintf(why, sizeof(why), "the single prop, the mashed one and %d of %d ring props are gone here; the "
                      "host answered Removed %u times; the creature stands", ringGone, kRing, removed);
        End("host", "PASS", why, false);
        return;
    }
    std::snprintf(why, sizeof(why),
                  "single gone=%d mash gone=%d ring gone=%d (want at least %d) creature standing=%d; answered Removed %u "
                  "(want %u), OutOfSight %u, Failed %u, NotReady %u, Held %u, Unsupported %u",
                  single ? 1 : 0, mash ? 1 : 0, ringGone, kSweepFloor, npc ? 1 : 0, removed, wantRemoved,
                  Answered(RR::OutOfSight), Answered(RR::Failed), Answered(RR::NotReady), Answered(RR::Held),
                  Answered(RR::Unsupported));
    End("host", "FAIL", why, true);
}

// The handshake that lets the client judge its own sight. The client says "remover-drill relay n cam x y z eid E at x y z"
// when its own camera's trace meets something before a target; the host answers "remover-drill laid n=<n>;" once it has
// laid the layout again in the next fan direction, or "remover-drill relay-failed;" when the fan has none left.
constexpr const char* kRelayPrefix = "remover-drill relay ";
int g_fanIdx = 0;       // the fan step the layout stands at
int g_relaysDone = 0;   // the relay numbers the host has answered

void Destroy(ue_wrap::CachedObjRef& ref) {
    if (void* a = ref.Get()) E::DestroyActor(a);
    ref.Reset();
}

// Takes the next relay line the client said, if any: lays the layout again from the step after the last. The numbers in
// the line are the client's camera and the element its trace met with where it stands there, set here against the host's
// own eye and the host's position of that element: the measurement of whether the two peers agree.
void HostRelay() {
    char text[256] = "";
    const std::string want = std::string(kRelayPrefix) + std::to_string(g_relaysDone + 1);
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) {
        if (r.slot == 1 && r.text.compare(0, want.size(), want) == 0 && (r.text.size() == want.size() || r.text[want.size()] == ' '))
            std::snprintf(text, sizeof(text), "%s", r.text.c_str());
    });
    if (!text[0]) return;
    ++g_relaysDone;
    float cx = 0, cy = 0, cz = 0, bx = 0, by = 0, bz = 0;
    unsigned eid = 0;
    int n = 0;
    const int got = sscanf_s(text, "remover-drill relay %d cam %f %f %f eid %u at %f %f %f", &n, &cx, &cy, &cz, &eid, &bx,
                                &by, &bz);
    coop::RemotePlayer& rp = coop::puppet_drive::Puppet(1);
    FVector eye{}, at{};
    const bool haveEye = rp.valid() && rp.HasPose() && rp.TryGetEyePosition(eye) && E::TryGetActorLocation(rp.GetActor(), at);
    if (got == 8 && haveEye) {
        FVector hostAt{};
        const EL::Element* el = EL::Registry::Get().Get(eid);
        void* a = el ? el->LiveActor() : nullptr;
        const bool held = a && E::TryGetActorLocation(a, hostAt);
        UE_LOGI("[REMOVER-DRILL] host: relay %d -- the client's camera is %.0f cm from the host's eye: (%.0f, %.0f, %.0f) "
                "against (%.0f, %.0f, %.0f)", n, Dist({cx, cy, cz}, eye), cx, cy, cz, eye.X, eye.Y, eye.Z);
        if (held)
            UE_LOGI("[REMOVER-DRILL] host: relay %d -- the blocker, element %u, stands %.0f cm from where the client holds it: "
                    "(%.0f, %.0f, %.0f) here against (%.0f, %.0f, %.0f) there", n, eid, Dist({bx, by, bz}, hostAt), hostAt.X,
                    hostAt.Y, hostAt.Z, bx, by, bz);
        else
            UE_LOGI("[REMOVER-DRILL] host: relay %d -- the blocker, element %u, is not a live element here; the client holds "
                    "it at (%.0f, %.0f, %.0f)", n, eid, bx, by, bz);
    }
    if (!haveEye) {
        coop::chat_sync::QueueSend("remover-drill relay-failed;");
        End("host", "ABANDONED", "the sender's eye did not read for a relay", true);
        return;
    }
    Destroy(g_hSingle);
    Destroy(g_hMash);
    Destroy(g_hNpc);
    for (auto& r : g_hRing) Destroy(r);
    int chosen = g_fanIdx;
    if (const char* why = SpawnLayout(rp, at, g_fanIdx + 1, &chosen)) {
        UE_LOGW("[REMOVER-DRILL] host: relay %d -- no further direction: %s", n, why);
        coop::chat_sync::QueueSend("remover-drill relay-failed;");
        return;
    }
    g_fanIdx = chosen;
    UE_LOGI("[REMOVER-DRILL] host: relay %d -- the layout is laid again at fan step %d", n, chosen);
    coop::chat_sync::QueueSend("remover-drill laid n=" + std::to_string(n) + ";");
}

void HostTick(coop::net::Session* s) {
    if (g_done) return;
    const uint64_t now = ::GetTickCount64();
    if (now < g_nextCheckMs) return;
    g_nextCheckMs = now + kCheckEveryMs;
    if (g_hostStep == HostStep::Arm) {
        // The client's join and its late window are over, so what is spawned reaches it as a live spawn.
        if (!s->IsSlotWorldReady(1) || !coop::prop_snapshot::IsBracketClosed(1) ||
            coop::join_window_baseline::IsLateWindowOpen(1))
            return;
        // The layout is spawned only once the host holds a posed puppet for the client, and the client presses only
        // after it finds the layout: by then the host can place the client's eye. Before its first pose a puppet
        // stands at its spawn point.
        coop::RemotePlayer& rp = coop::puppet_drive::Puppet(1);
        void* puppet = rp.valid() && rp.HasPose() ? rp.GetActor() : nullptr;
        FVector at{};
        if (!puppet || !R::IsLive(puppet) || !E::TryGetActorLocation(puppet, at)) return;
        if (const char* why = SpawnLayout(rp, at, 0, &g_fanIdx)) { End("host", "ABANDONED", why, true); return; }
        UE_LOGI("[REMOVER-DRILL] host: the layout is spawned about the client's puppet (%s)", PhaseName());
        g_hostStep = HostStep::Await;
        g_stepMs = now;
        return;
    }
    HostRelay();
    if (ClientEnded()) { HostJudge(); return; }
    if (Expired(kAwaitBoundMs)) End("host", "ABANDONED", "the client's closing line never came", true);
}

// ---- client -----------------------------------------------------------------------------------------------------

enum class Step : uint8_t {
    Arm, Find, Settle, Relay, Cleared, Single, SingleWait, Mash, MashWait, Sweep, SweepWait, Npc, NpcWait, Finish
};
Step g_step = Step::Arm;
ue_wrap::CachedObjRef g_toolgun, g_single, g_mash, g_npc;
ue_wrap::CachedObjRef g_ring[kRing];    // every ring prop the host laid
int g_ringN = 0;
ue_wrap::CachedObjRef g_sweep[kRing];   // the ring props the camera's trace meets first: the shots of the sweep
int g_sweepN = 0;
int g_sweepAt = 0;
int g_noticeBase = 0;   // the leg's notice count before its shot
char g_aimWhy[420] = "";   // why the last AimedHit or press failed
FVector g_restAt[kRing + 2];   // where the pair and the ring stood at the last rest read
int g_restReads = 0;           // reads in a row at which none of them had moved
int g_relay = 0;                 // relays asked of the host for this run
std::string g_relayWhy;          // what each direction's blocked trace met, for the abandon that ends the fan
FVector g_aimCam{}, g_aimBlkAt{};   // the last miss: the camera, and where the actor its trace met stands
uint32_t g_aimBlkEid = 0;

void Next(Step s) { g_step = s; g_stepMs = ::GetTickCount64(); }
void Fail(const char* what) { End("client", "FAIL", what, true); }
void Abandon(const char* why) { End("client", "ABANDONED", why, true); }

// An abandon that carries the aim's own reason after `what`.
void AbandonAim(const char* what) {
    char why[560];
    std::snprintf(why, sizeof(why), "%s: %s", what, g_aimWhy);
    Abandon(why);
}

void LegPass(const char* name, Step next) {
    UE_LOGI("[REMOVER-DRILL] client leg %s PASS", name);
    Next(next);
}

// How many lines of this client's feed carry `text`.
int NoticeCount(const char* text) {
    int n = 0;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        if (r.line.find(text) != std::string_view::npos) ++n;
    });
    return n;
}

void DestroyIfLive(void* actor) {
    if (actor && R::IsLive(actor)) E::DestroyActor(actor);
}

// The hit the game itself would write for a press at `target`: the first thing a trace from this player's camera toward the
// target's middle (and a little past it, so the ray crosses its body) meets, over the four object types the toolgun's
// shot takes. Every player body is ignored, as the host's judge ignores them: with both players at one spawn point a native
// trace would stop at the other's capsule. True when that first thing is the target and the hit names a component.
bool AimedHit(void* player, void* target, ue_wrap::trace::Hit* hit) {
    g_aimCam = {};
    g_aimBlkAt = {};
    g_aimBlkEid = 0;
    FVector cam{}, at{};
    if (!E::ReadMainPlayerCameraLocation(player, cam) || !E::TryGetActorLocation(target, at)) {
        std::snprintf(g_aimWhy, sizeof(g_aimWhy), "the camera or the target's location did not read");
        return false;
    }
    const float len = Dist(cam, at);
    if (!(len > 1.f)) {
        std::snprintf(g_aimWhy, sizeof(g_aimWhy), "the target is at the camera");
        return false;
    }
    const float past = (len + kAimPastCm) / len;
    const FVector end{cam.X + (at.X - cam.X) * past, cam.Y + (at.Y - cam.Y) * past, cam.Z + (at.Z - cam.Z) * past};
    const uint8_t types[4] = {0, 1, 2, 3};
    void* ignore[coop::players::kMaxPeers];
    const int nIgnore = coop::players::Registry::Get().LiveBodies(ignore, coop::players::kMaxPeers);
    if (!ue_wrap::trace::LineHitForObjects(player, cam, end, types, 4, ignore, nIgnore, hit)) {
        std::snprintf(g_aimWhy, sizeof(g_aimWhy), "the trace did not run");
        return false;
    }
    if (hit->blocked && hit->actor == target && hit->component) return true;
    FVector bat{};   // where the actor the trace met stands, to set against where the other peer holds it
    if (hit->actor) E::TryGetActorLocation(hit->actor, bat);
    g_aimCam = cam;
    g_aimBlkAt = bat;
    g_aimBlkEid = hit->actor ? static_cast<uint32_t>(coop::element::Registry::Get().EidForActor(hit->actor)) : 0u;
    if (!hit->blocked)
        std::snprintf(g_aimWhy, sizeof(g_aimWhy), "the trace met nothing, %.0f cm to the target", len);
    else if (hit->actor == target)
        std::snprintf(g_aimWhy, sizeof(g_aimWhy), "the hit on the target names no component");
    else
        std::snprintf(g_aimWhy, sizeof(g_aimWhy),
                      "the trace met %ls (element %u, at %.0f, %.0f, %.0f) at %.0f cm of the %.0f cm to the target's middle "
                      "(camera %.0f, %.0f, %.0f; target %.0f, %.0f, %.0f)",
                      hit->actor ? R::ClassNameOf(hit->actor).c_str() : L"(no actor)",
                      static_cast<unsigned>(coop::element::Registry::Get().EidForActor(hit->actor)), bat.X, bat.Y,
                      bat.Z, Dist(cam, hit->point), len, cam.X, cam.Y, cam.Z, at.X, at.Y, at.Z);
    return false;
}

// One press of the remover on `target` with the hit the toolgun is to hold: a fresh tool, outHit written, and the tool's
// init(toolgun) -- the body the toolgun's press runs. The tool is gone after it on every path (the game's body destroys
// it, the lane's watch destroys it); one left standing is destroyed here.
bool PressWith(void* player, void* target, const ue_wrap::trace::Hit& hit) {
    void* toolgun = g_toolgun.Get();
    void* cls = ue_wrap::object_index::ClassByName(kToolClass);
    FVector me{};
    if (!toolgun || !cls || !E::TryGetActorLocation(player, me)) {
        std::snprintf(g_aimWhy, sizeof(g_aimWhy), "the toolgun, the tool class or the player's location is missing");
        return false;
    }
    void* tool = DW::SpawnTool(cls, {me.X, me.Y, me.Z + 90.f});
    if (!tool) std::snprintf(g_aimWhy, sizeof(g_aimWhy), "the tool did not spawn");
    const bool ran = tool && ue_wrap::hit_result::WriteField(toolgun, L"outHit", target, hit.component, hit.point) &&
                     DW::CallToolInit(tool, toolgun);
    if (tool && !ran) std::snprintf(g_aimWhy, sizeof(g_aimWhy), "outHit could not be written or init did not run");
    DestroyIfLive(tool);
    return ran;
}

// One press at `target` aimed afresh: the real hit, then the press. False with g_aimWhy saying which part failed.
bool Press(void* player, void* target) {
    ue_wrap::trace::Hit hit;
    return AimedHit(player, target, &hit) && PressWith(player, target, hit);
}

struct Seen { void* actor; float cm; };

// The host's props of one class about `from`, nearest first.
std::vector<Seen> PropsAbout(const wchar_t* className, const FVector& from) {
    std::vector<Seen> out;
    void* cls = ue_wrap::object_index::ClassByName(className);
    if (!cls) return out;
    struct Ctx { const FVector* from; std::vector<Seen>* out; } ctx{&from, &out};
    ue_wrap::object_index::ForEachInstance(cls, [](void* c, void* obj, int32_t) {
        auto* x = static_cast<Ctx*>(c);
        FVector at{};
        if (R::NameStartsWith(R::NameOf(obj), L"Default__") || !E::TryGetActorLocation(obj, at)) return;
        const float cm = Dist(at, *x->from);
        if (cm <= kFindRadiusCm) x->out->push_back({obj, cm});
    }, &ctx);
    std::sort(out.begin(), out.end(), [](const Seen& a, const Seen& b) { return a.cm < b.cm; });
    return out;
}

void* NearestNpc(const FVector& from) {
    void* cls = ue_wrap::object_index::ClassByName(kNpcClass);
    if (!cls) return nullptr;
    struct Ctx { const FVector* from; void* best; float cm; } ctx{&from, nullptr, kFindRadiusCm};
    ue_wrap::object_index::ForEachInstance(cls, [](void* c, void* obj, int32_t) {
        auto* x = static_cast<Ctx*>(c);
        FVector at{};
        if (R::NameStartsWith(R::NameOf(obj), L"Default__") || !E::TryGetActorLocation(obj, at)) return;
        const float cm = Dist(at, *x->from);
        if (cm < x->cm) { x->best = obj; x->cm = cm; }
    }, &ctx);
    return ctx.best;
}

// The host's roles, each a class of its own (the single prop a medium sack, the mashed one a small sack, the ring plain
// sacks, the creature a kerfurOmega), so a role is found by what it is and not by where physics left it. Fills the client's
// roles; false while the host's props are not all here. Whether the camera can press each is Choose's, once they rest.
bool Find(void* player) {
    FVector me{};
    if (!E::TryGetActorLocation(player, me)) return false;
    const bool granted = PhaseOf() == Phase::Granted;
    const std::vector<Seen> single = PropsAbout(kSingleClass, me);
    if (single.empty()) return false;
    if (!granted) {
        g_single.Set(single[0].actor);
        return true;
    }
    const std::vector<Seen> mash = PropsAbout(kMashClass, me);
    const std::vector<Seen> ring = PropsAbout(kSackClass, me);
    void* npc = NearestNpc(me);
    if (mash.empty() || ring.size() < static_cast<size_t>(kRing) || !npc) return false;
    g_single.Set(single[0].actor);
    g_mash.Set(mash[0].actor);
    g_npc.Set(npc);
    g_ringN = kRing;
    for (int i = 0; i < kRing; ++i) g_ring[i].Set(ring[static_cast<size_t>(i)].actor);
    return true;
}

// Have the pair and the ring stopped moving: no one of them has moved more than a centimetre since the last read, as
// sack_drill waits for its sack. A press at a prop still falling or coasting is not a press at the layout the host laid.
bool AtRest() {
    bool still = true;
    int n = 0;
    const auto visit = [&](const ue_wrap::CachedObjRef& ref) {
        FVector at{};
        void* a = ref.Get();
        if (!a || !E::TryGetActorLocation(a, at)) { still = false; ++n; return; }
        if (Dist(at, g_restAt[n]) >= kRestMoveCm) still = false;
        g_restAt[n++] = at;
    };
    visit(g_single);
    if (PhaseOf() == Phase::Granted) {
        visit(g_mash);
        for (int i = 0; i < g_ringN; ++i) visit(g_ring[i]);
    }
    return still;
}

// Says `text` to the host as a chat line, through the session's own reliable lane (the host reads it from its lobby record).
bool SayToHost(coop::net::Session* s, const std::string& text) {
    coop::net::ChatMessagePayload p{};
    if (text.empty() || text.size() > sizeof(p.text)) return false;
    std::memcpy(p.text, text.data(), text.size());
    p.len = static_cast<uint8_t>(text.size());
    return s->SendReliable(coop::net::ReliableKind::ChatMessage, &p, sizeof(p));
}

// A target the camera's own trace does not meet first: the host's trace judged the segment clear from its eye, and the
// client is the judge of its own shot. It asks the host, in a chat line that carries its camera and what its trace met, to
// lay the layout again in the next fan direction, up to the fan's size; then it abandons with every direction's reason.
// False: the run goes on only after the relay, or it has ended.
bool Blocked(coop::net::Session* s, const char* which) {
    char one[460];
    std::snprintf(one, sizeof(one), "%s%s: %s", g_relayWhy.empty() ? "" : "; ", which, g_aimWhy);
    if (g_relayWhy.size() < 1400) g_relayWhy += one;
    if (g_relay >= kFanCount - 1) {
        Abandon(("every direction the host laid was blocked from this camera -- " + g_relayWhy).c_str());
        return false;
    }
    ++g_relay;
    char line[200];
    std::snprintf(line, sizeof(line), "remover-drill relay %d cam %.0f %.0f %.0f eid %u at %.0f %.0f %.0f", g_relay, g_aimCam.X,
                  g_aimCam.Y, g_aimCam.Z, static_cast<unsigned>(g_aimBlkEid), g_aimBlkAt.X, g_aimBlkAt.Y, g_aimBlkAt.Z);
    if (!SayToHost(s, line)) { Abandon("the relay line was not sent"); return false; }
    UE_LOGI("[REMOVER-DRILL] client: %s: %s -- asking the host to lay again (relay %d)", which, g_aimWhy, g_relay);
    Next(Step::Relay);
    return false;
}

// Once the layout is at rest: the camera's own trace must meet each target first (a press must be one the game itself could
// make), and the sweep is every ring prop it meets first. True when the run goes on.
bool Choose(void* player, coop::net::Session* s) {
    ue_wrap::trace::Hit probe;
    if (!AimedHit(player, g_single.Get(), &probe)) return Blocked(s, "the single prop");
    if (PhaseOf() != Phase::Granted) return true;
    if (!AimedHit(player, g_mash.Get(), &probe)) return Blocked(s, "the mashed prop");
    if (!AimedHit(player, g_npc.Get(), &probe)) return Blocked(s, "the creature");
    int n = 0;
    for (int i = 0; i < g_ringN; ++i) {
        void* r = g_ring[i].Get();
        if (r && AimedHit(player, r, &probe)) g_sweep[n++].Set(r);
    }
    g_sweepN = n;
    if (n < kSweepFloor) {
        char why[120];
        std::snprintf(why, sizeof(why), "%d of the ring props are in this client's sight, %d are needed", n, kSweepFloor);
        Abandon(why);
        return false;
    }
    return true;
}

bool SpawnToolgun(void* player) {
    FVector me{};
    ue_wrap::asset_load::LoadObjectByPath(kToolgunPath);
    ue_wrap::asset_load::LoadObjectByPath(kToolPath);
    if (!E::TryGetActorLocation(player, me) || !ue_wrap::object_index::ClassByName(kToolClass)) return false;
    void* gun = DW::SpawnToolgun({me.X, me.Y, me.Z + 60.f});
    g_toolgun.Set(gun);
    return gun != nullptr;
}

// A press at `ref`'s actor: the notice count for the leg is taken first.
bool Fire(void* player, const ue_wrap::CachedObjRef& ref, const char* notice) {
    void* target = ref.Get();
    if (!target) { Abandon("the target is gone before its press"); return false; }
    g_noticeBase = notice ? NoticeCount(notice) : 0;
    if (!Press(player, target)) { AbandonAim("the press did not run"); return false; }
    return true;
}

// Is the leg's target gone from this copy. Fails the drill on a refusal the leg did not expect.
bool GoneOrFail(const ue_wrap::CachedObjRef& ref, const char* unexpected, const char* failWhy) {
    if (!ref.Alive()) return true;
    if (unexpected && NoticeCount(unexpected) > g_noticeBase) { Fail(failWhy); return false; }
    if (Expired(kLegBoundMs)) Fail("the target was not removed within 30 s of the press");
    return false;
}

void Finish(coop::net::Session* s) {
    // The host judges when it commits the closing line.
    if (!SayToHost(s, EndLine())) {
        Abandon("the closing chat line was not sent");
        return;
    }
    End("client", "PASS", "every leg held on this copy; the closing line is sent for the host to judge", false);
}

void ClientTick(coop::net::Session* s) {
    if (g_done) return;
    const uint64_t now = ::GetTickCount64();
    if (now < g_nextCheckMs) return;
    g_nextCheckMs = now + kCheckEveryMs;
    void* player = coop::players::Registry::Get().Local();
    if (!player) return;
    const bool granted = PhaseOf() == Phase::Granted;
    switch (g_step) {
    case Step::Arm:
        if (!coop::net_pump::HasAnnouncedWorldReady() ||
            coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle)
            return;
        Next(Step::Find);
        return;
    case Step::Find:
        if (!Find(player)) {
            if (Expired(kFindBoundMs)) Abandon("the host's props did not all reach this client within 60 s");
            return;
        }
        if (g_done) return;
        if (!g_toolgun.Alive() && !SpawnToolgun(player)) {
            Abandon("the toolgun or the remover tool could not be spawned");
            return;
        }
        UE_LOGI("[REMOVER-DRILL] client: the host's props are here and the toolgun is up (%s)", PhaseName());
        Next(Step::Settle);
        return;
    case Step::Settle:
        g_restReads = AtRest() ? g_restReads + 1 : 0;
        if (g_restReads < kRestChecks) {
            if (Expired(kFindBoundMs)) Abandon("the host's props did not come to rest within 60 s");
            return;
        }
        if (!Choose(player, s)) return;
        UE_LOGI("[REMOVER-DRILL] client: the host's props are at rest and the camera's trace meets each target first");
        Next(Step::Single);
        return;
    case Step::Relay:
        // The host lays the layout again and says so; or it has no further direction.
        if (NoticeCount(("remover-drill laid n=" + std::to_string(g_relay) + ";").c_str()) > 0) {
            Next(Step::Cleared);
        } else if (NoticeCount("remover-drill relay-failed;") > 0) {
            Abandon(("the host has no direction left that its own trace clears; every one it laid was blocked from this "
                     "camera -- " + g_relayWhy).c_str());
        } else if (Expired(kFindBoundMs)) {
            Abandon("the host did not answer the relay within 60 s");
        }
        return;
    case Step::Cleared: {
        // The old layout's mirrors go with the host's destroys; the roles are found again only once every one has gone, or
        // the find would take an old prop for a new one.
        bool gone = !g_single.Alive() && !g_mash.Alive() && !g_npc.Alive();
        for (int i = 0; i < g_ringN; ++i) gone = gone && !g_ring[i].Alive();
        if (!gone) {
            if (Expired(kFindBoundMs)) Abandon("the old layout's mirrors did not go within 60 s of the re-lay");
            return;
        }
        g_single.Reset();
        g_mash.Reset();
        g_npc.Reset();
        for (auto& r : g_ring) r.Reset();
        for (auto& r : g_sweep) r.Reset();
        for (auto& p : g_restAt) p = {};
        g_ringN = 0;
        g_sweepN = 0;
        g_restReads = 0;
        Next(Step::Find);
        return;
    }
    case Step::Single:
        if (Fire(player, g_single, kNoRight)) Next(Step::SingleWait);
        return;
    case Step::SingleWait:
        if (granted) {
            if (GoneOrFail(g_single, kNoRight, "the first prop was refused though the grant stands"))
                LegPass("single", Step::Mash);
            return;
        }
        // withheld: the copy must never go, and the refusal must show.
        if (!g_single.Alive()) { Fail("the refused prop's copy vanished on this client"); return; }
        if (NoticeCount(kNoRight) > g_noticeBase) { LegPass("withheld", Step::Finish); return; }
        if (Expired(kLegBoundMs)) Fail("the host's refusal was not shown within 30 s");
        return;
    case Step::Mash: {
        g_noticeBase = NoticeCount(kNoRight);
        // One aim, then every press in this tick from it, before any destroy can come back: the mash is six presses of one
        // shot, so the host answers the first Removed and the rest Gone.
        void* target = g_mash.Get();
        ue_wrap::trace::Hit hit;
        if (!target) { Abandon("the mashed prop is gone before its presses"); return; }
        if (!AimedHit(player, target, &hit)) { AbandonAim("the mash aim failed"); return; }
        int shots = 0;
        for (int i = 0; i < kMashShots; ++i) {
            void* t = g_mash.Get();
            if (!t) break;   // a press that removed the prop here at once leaves nothing to press
            if (!PressWith(player, t, hit)) { AbandonAim("a mash press did not run"); return; }
            ++shots;
        }
        UE_LOGI("[REMOVER-DRILL] client: %d presses on one prop in one tick", shots);
        Next(Step::MashWait);
        return;
    }
    case Step::MashWait:
        if (GoneOrFail(g_mash, kNoRight, "the mashed prop was refused though the grant stands")) {
            g_sweepAt = 0;
            LegPass("mash", Step::Sweep);
        }
        return;
    case Step::Sweep:
        if (g_sweepAt >= g_sweepN) { LegPass("sweep", Step::Npc); return; }
        if (Fire(player, g_sweep[g_sweepAt], kOutOfSight)) Next(Step::SweepWait);
        return;
    case Step::SweepWait:
        if (GoneOrFail(g_sweep[g_sweepAt], kOutOfSight, "a prop in this client's sight was refused for sight")) {
            ++g_sweepAt;
            Next(Step::Sweep);
        }
        return;
    case Step::Npc:
        if (Fire(player, g_npc, kUnsupported)) Next(Step::NpcWait);
        return;
    case Step::NpcWait:
        if (!g_npc.Alive()) { Fail("the creature was removed on this client"); return; }
        if (NoticeCount(kUnsupported) > g_noticeBase) { LegPass("unsupported", Step::Finish); return; }
        if (Expired(kLegBoundMs)) Fail("the remover's refusal of the creature was not shown within 30 s");
        return;
    case Step::Finish:
        Finish(s);
        return;
    }
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (PhaseOf() == Phase::Off || !s) return;
    if (s->role() == coop::net::Role::Host) HostTick(s);
    else if (s->connected()) ClientTick(s);
}

void OnDisconnect() {
    if (PhaseOf() == Phase::Off) return;
    ++g_session;
    g_done = false;
    g_hostStep = HostStep::Arm;
    g_fanIdx = 0;
    g_relaysDone = 0;
    g_step = Step::Arm;
    g_stepMs = g_nextCheckMs = 0;
    g_hSingle.Reset();
    g_hMash.Reset();
    g_hNpc.Reset();
    for (auto& r : g_hRing) r.Reset();
    g_toolgun.Reset();
    g_single.Reset();
    g_mash.Reset();
    g_npc.Reset();
    for (auto& r : g_sweep) r.Reset();
    g_sweepAt = 0;
    g_sweepN = 0;
    g_ringN = 0;
    g_relay = 0;
    g_relayWhy.clear();
    g_restReads = 0;
    for (auto& r : g_ring) r.Reset();
    for (auto& p : g_restAt) p = {};
    g_noticeBase = 0;
}

}  // namespace coop::dev::remover_drill
