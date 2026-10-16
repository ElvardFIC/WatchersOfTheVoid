// coop/dev/remover_drill.cpp -- see coop/dev/remover_drill.h.

#include "coop/dev/remover_drill.h"

#include "coop/comms/chat_feed.h"
#include "coop/comms/chat_log.h"
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/spawn_npc.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
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

// The host's layout about its client's puppet, in centimetres along its facing: the single and the mashed props side
// by side ahead of it, a ring of kRing about it clear of the cone it faces, the creature beyond the ring. The client
// tells the roles by distance: the nearer of the pair is the single prop.
constexpr int   kRing         = 28;
constexpr int   kSweepFloor   = 10;    // the fewest ring props in sight that make the sweep a measurement
constexpr int   kMashShots    = 6;
constexpr float kSingleAheadCm = 120.f;
constexpr float kMashAheadCm   = 190.f;
constexpr float kPairSideCm    = 45.f;   // either side of the facing line, so neither prop stands in the other's way
constexpr float kRingConeDeg   = 30.f;   // the ring keeps this far off the facing line, each side
constexpr float kRingNearCm   = 260.f;
constexpr float kRingStepCm   = 30.f;
constexpr float kNpcCm        = 420.f;
constexpr float kPuppetHalfHeightCm = 90.f;   // the floor when the puppet's bounds do not read
constexpr float kDropCm       = 20.f;  // above the floor, so a prop settles
constexpr float kFindRadiusCm = 700.f;
constexpr float kPairMaxCm    = 230.f; // inside this from the client: one of the pair (the ring starts at 260)
constexpr float kSightSlackCm = 25.f;

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

// Spawns the host's layout about the client's puppet, on the floor and by its facing: the single and the mashed props
// side by side ahead of it (the single nearer), nothing of the ring in the cone it faces and nothing of it between the
// client and the pair, and the creature to its right beyond the ring. False when a prop did not spawn.
bool SpawnLayout(void* puppet, const FVector& at) {
    void* cls = ue_wrap::asset_load::LoadObjectByPath(kSackPath);
    if (!cls) return false;
    FVector fwd = E::GetActorForwardVector(puppet);
    const float flat = std::sqrt(fwd.X * fwd.X + fwd.Y * fwd.Y);
    if (flat < 0.01f) fwd = {1.f, 0.f, 0.f};
    else fwd = {fwd.X / flat, fwd.Y / flat, 0.f};
    const FVector right{-fwd.Y, fwd.X, 0.f};
    // The floor under the puppet: the bottom of its bounds, else a capsule's half-height under its centre.
    FVector centre{}, extent{};
    const bool bounds = E::GetActorBounds(puppet, false, centre, extent) && extent.Z > 0.f;
    const float floorZ = bounds ? centre.Z - extent.Z : at.Z - kPuppetHalfHeightCm;
    const auto spawn = [&](float ahead, float side, ue_wrap::CachedObjRef& into) {
        void* p = E::SpawnActor(cls, {at.X + fwd.X * ahead + right.X * side, at.Y + fwd.Y * ahead + right.Y * side,
                                      floorZ + kDropCm});
        into.Set(p);
        return p != nullptr;
    };
    if (!spawn(kSingleAheadCm, kPairSideCm, g_hSingle)) return false;
    if (PhaseOf() == Phase::Withheld) return true;
    if (!spawn(kMashAheadCm, -kPairSideCm, g_hMash)) return false;
    for (int i = 0; i < kRing; ++i) {
        const float a = (kRingConeDeg + (360.f - 2.f * kRingConeDeg) * static_cast<float>(i) /
                                            static_cast<float>(kRing - 1)) * 0.0174532925f;
        const float r = kRingNearCm + kRingStepCm * static_cast<float>(i % 3);
        if (!spawn(std::cos(a) * r, std::sin(a) * r, g_hRing[i])) return false;
    }
    void* npc = coop::dev::spawn_npc::SpawnAt(kNpcClass, {at.X + right.X * kNpcCm, at.Y + right.Y * kNpcCm,
                                                          floorZ + kDropCm});
    g_hNpc.Set(npc);
    return npc != nullptr;
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
        if (!SpawnLayout(puppet, at)) { End("host", "ABANDONED", "a prop of the layout did not spawn", true); return; }
        UE_LOGI("[REMOVER-DRILL] host: the layout is spawned about the client's puppet (%s)", PhaseName());
        g_hostStep = HostStep::Await;
        g_stepMs = now;
        return;
    }
    if (ClientEnded()) { HostJudge(); return; }
    if (Expired(kAwaitBoundMs)) End("host", "ABANDONED", "the client's closing line never came", true);
}

// ---- client -----------------------------------------------------------------------------------------------------

enum class Step : uint8_t {
    Arm, Find, Single, SingleWait, Mash, MashWait, Sweep, SweepWait, Npc, NpcWait, Finish
};
Step g_step = Step::Arm;
ue_wrap::CachedObjRef g_toolgun, g_single, g_mash, g_npc;
ue_wrap::CachedObjRef g_sweep[kRing];   // every ring prop the client has in sight, the shots of the sweep
int g_sweepN = 0;
int g_sweepAt = 0;
int g_noticeBase = 0;   // the leg's notice count before its shot

void Next(Step s) { g_step = s; g_stepMs = ::GetTickCount64(); }
void Fail(const char* what) { End("client", "FAIL", what, true); }
void Abandon(const char* why) { End("client", "ABANDONED", why, true); }

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

// One press of the remover on `target`: a fresh tool, the toolgun's outHit a blocking hit on the target's root at its
// location, and the tool's init(toolgun) -- the body the toolgun's press runs. The tool is gone after it on every path
// (the game's body destroys it, the lane's watch destroys it); one left standing is destroyed here.
bool Press(void* player, void* target) {
    void* toolgun = g_toolgun.Get();
    void* cls = ue_wrap::object_index::ClassByName(kToolClass);
    void* comp = E::ActorRootComponent(target);
    FVector at{}, me{};
    if (!toolgun || !cls || !comp || !E::TryGetActorLocation(target, at) || !E::TryGetActorLocation(player, me))
        return false;
    void* tool = DW::SpawnTool(cls, {me.X, me.Y, me.Z + 90.f});
    const bool ran = tool && ue_wrap::hit_result::WriteField(toolgun, L"outHit", target, comp, at) &&
                     DW::CallToolInit(tool, toolgun);
    DestroyIfLive(tool);
    return ran;
}

// Is `target` in this player's sight: the segment from its camera to the target meets nothing before the target (its
// own body is the first thing met, or nothing is), over the object types the remover's own trace takes.
bool InSight(void* player, void* target) {
    FVector cam{}, at{};
    if (!E::ReadMainPlayerCameraLocation(player, cam) || !E::TryGetActorLocation(target, at)) return false;
    const uint8_t types[4] = {0, 1, 2, 3};
    void* ignore[1] = {player};
    ue_wrap::trace::Hit hit;
    if (!ue_wrap::trace::LineHitForObjects(player, cam, at, types, 4, ignore, 1, &hit)) return false;
    return !hit.blocked || hit.actor == target || Dist(cam, hit.point) >= Dist(cam, at) - kSightSlackCm;
}

struct Seen { void* actor; float cm; };

// The sacks about `from`, nearest first.
std::vector<Seen> SacksAbout(const FVector& from) {
    std::vector<Seen> out;
    void* cls = ue_wrap::object_index::ClassByName(kSackClass);
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

// The host's pair is the two nearest props, on either side along X; the rest is the ring. Fills the client's roles.
// False while the host's props are not all here; Abandons when they are but cannot be told apart.
bool Find(void* player) {
    FVector me{};
    if (!E::TryGetActorLocation(player, me)) return false;
    const std::vector<Seen> seen = SacksAbout(me);
    const bool granted = PhaseOf() == Phase::Granted;
    const size_t want = granted ? 2 + kRing : 1;
    if (seen.size() < want) return false;
    void* npc = granted ? NearestNpc(me) : nullptr;
    if (granted && !npc) return false;
    std::vector<void*> pair, ring;
    for (const Seen& s : seen) (s.cm < kPairMaxCm ? pair : ring).push_back(s.actor);
    if (pair.size() != (granted ? 2u : 1u)) { Abandon("the host's pair of props did not stand apart from the ring"); return true; }
    // `seen` is nearest first, so the nearer of the pair is the single prop and the other the mashed one.
    g_single.Set(pair[0]);
    if (granted) {
        g_mash.Set(pair[1]);
        g_npc.Set(npc);
        int n = 0;
        for (void* r : ring) {
            if (n < kRing && InSight(player, r)) g_sweep[n++].Set(r);
        }
        g_sweepN = n;
        if (n < kSweepFloor) {
            char why[120];
            std::snprintf(why, sizeof(why), "%d of the ring props are in this client's sight, %d are needed", n, kSweepFloor);
            Abandon(why);
            return true;
        }
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
    if (!Press(player, target)) { Abandon("the press did not run (tool, toolgun, hit or init)"); return false; }
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
    coop::net::ChatMessagePayload p{};
    const std::string line = EndLine();
    std::memcpy(p.text, line.data(), line.size());
    p.len = static_cast<uint8_t>(line.size());
    // Sent through the session's own reliable lane; the host judges when it commits the line.
    if (!s->SendReliable(coop::net::ReliableKind::ChatMessage, &p, sizeof(p))) {
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
        if (!SpawnToolgun(player)) { Abandon("the toolgun or the remover tool could not be spawned"); return; }
        UE_LOGI("[REMOVER-DRILL] client: the host's props are here and the toolgun is up (%s)", PhaseName());
        Next(Step::Single);
        return;
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
        int shots = 0;
        for (int i = 0; i < kMashShots; ++i) {
            void* target = g_mash.Get();
            if (!target) break;   // a press that removed the prop here at once leaves nothing to press
            if (!Press(player, target)) { Abandon("a mash press did not run"); return; }
            ++shots;
        }
        if (shots == 0) { Abandon("the mashed prop is gone before its presses"); return; }
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
    g_noticeBase = 0;
}

}  // namespace coop::dev::remover_drill
