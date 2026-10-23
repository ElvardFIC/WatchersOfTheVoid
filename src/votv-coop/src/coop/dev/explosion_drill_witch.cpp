// coop/dev/explosion_drill_witch.cpp -- see coop/dev/explosion_drill_internal.h.
//
// The witch car arm (request_witch). A client sitting in the witch car fires the rocket its left-mouse event spawns;
// the host must take that shot from the client's seat. The host spawns the car and says where, the client walks to
// its mirror with the director, takes the seat through the game's own verb and says where its rocket would fly; the
// host puts a crate on that line, out of the blast's reach of the car, and says so; the client then presses the car's
// left mouse once. Each half judges its own lane counters as deltas against a baseline taken before the first send.
// The host reports its shot half (accepted under the witch car's launcher, nothing refused) apart from its blast
// half (the request accepted, the blast captured): a rocket that meets nothing expires, which leaves the blast half
// INCONCLUSIVE. The car destroys itself at begin-play in the story mode or outside Halloween, each peer on its own
// state, so both halves put the sandbox mode and isHalloween in force before the car exists on them and back at the
// end; the host spawns only after the client's chat line says its own state is in.

#include "coop/dev/explosion_drill_internal.h"

#include "explosion_drill_source.h"          // Guard, ReadBool, WriteBool, Count (co-located private header)
#include "atv_seat.h"                        // TrySit (co-located private header)
#include "hostile_drill_explosion_detail.h"  // Say, PollDue (co-located private header)

#include "coop/comms/chat_feed.h"
#include "coop/dev/director/director.h"
#include "coop/interactables/atv_sync.h"
#include "coop/net/net_clock.h"
#include "coop/player/players_registry.h"
#include "coop/props/join_membership_sweep.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"
#include "coop/world/explosion_recipe.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_component.h"
#include "ue_wrap/engine/engine_physics.h"
#include "ue_wrap/world/game_mode.h"
#include "ue_wrap/world/world_singleton.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

namespace coop::dev::explosion_drill::detail {

namespace E  = ue_wrap::engine;
namespace R  = ue_wrap::reflection;
namespace ES = coop::explosion_sync;
namespace ER = coop::explosion_request;
namespace GM = ue_wrap::game_mode;
namespace HD = coop::dev::hostile_drill::explosion::detail;
namespace RC = coop::explosion_recipe;

namespace {

constexpr const char*    kName = "request_witch";
constexpr const wchar_t* kCarCls = L"car1_witch_C";
constexpr const wchar_t* kCarPath = L"/Game/objects/car1_witch.car1_witch_C";
// The car's left-mouse input event: the game's own fire, which spawns the rocket along the car's camera.
constexpr const wchar_t* kFireFn = L"InpActEvt_LeftMouseButton_K2Node_InputKeyEvent_0";
// The lobby-chat lines the two peers say to each other: the client's state is in force, the host's car stands at
// <x y z>, the client sits and its rocket would leave <x y z> along <fx fy fz>, the host's crate stands on that line.
constexpr const char*    kReadyLine = "hostile-drill request_witch client-ready";
constexpr const char*    kHostLine = "hostile-drill request_witch host-ready";
constexpr const char*    kSeatedLine = "hostile-drill request_witch client-seated";
constexpr const char*    kTargetLine = "hostile-drill request_witch target-ready";
// The crate's distance from the rocket's spawn point along its line: past the explosion's 1000 cm radius
// (explosion.cpp:9) from the car and from the host's player, who stands within 600 cm of the car's spawn point.
constexpr float          kTargetCm = 2500.f;
// The mirror the client walks to is the witch car within this distance of the point the host said its car stands at:
// the host's spawn (HostSpawn) puts the car there and the announce places the mirror there, where only the fall to rest
// differs (tens of cm).
constexpr float          kMirrorCm = 500.f;
constexpr float          kReachCm = 400.f;         // the director's walk ends within this distance of the car
constexpr int            kWalkDeadlineS = 90;      // the director's walk gives up after this long
// The host's verdict bound, from the client's ready line: the client's chain after it is the walk (kWalkDeadlineS),
// the seat (three tries two seconds apart) and the shot's life (32 s), which kVerdictBound covers with the host's own
// spawn and rest waits.
constexpr auto           kHostBound = kVerdictBound + std::chrono::seconds(kWalkDeadlineS);

unsigned long long U(uint64_t v) { return static_cast<unsigned long long>(v); }

// The arm's chat rows are its own run's: a LIVE row (the retained tier holds earlier history) with a key past the
// newest one the feed held when the arm began, so a line an earlier run on this process said, or a joiner's seeded
// history, is never read. The live-tier rule is the one prop_act_drill_client.cpp (HostSaidLive) reads its host's
// lines by; `key` rises with the lines' order (chat_feed.h, Line).
using Mark = unsigned long long;

Mark FeedMark() {
    Mark newest = 0;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        if (r.key > newest) newest = r.key;
    });
    return newest;
}

bool RunRow(const coop::chat_feed::RowView& r, Mark mark) { return !r.retained && r.key > mark; }

bool ChatHasNew(const char* needle, Mark mark) {
    bool found = false;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        if (RunRow(r, mark) && r.line.find(needle) != std::string_view::npos) found = true;
    });
    return found;
}

enum class Said : uint8_t { No, Ok, Malformed };

// The numbers a peer said after `prefix` in this run's rows of the lobby chat: Ok once a row holds `n` finite ones,
// Malformed (the row in `whole`) when the first row with the prefix does not.
Said ChatNumbers(const char* prefix, float* out, int n, Mark mark, std::string& whole) {
    Said said = Said::No;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        if (said != Said::No || !RunRow(r, mark)) return;
        const size_t at = r.line.find(prefix);
        if (at == std::string_view::npos) return;
        const std::string tail(r.line.substr(at + std::strlen(prefix)));
        const char*       p = tail.c_str();
        said = Said::Ok;
        for (int i = 0; i < n; ++i) {
            char* end = nullptr;
            out[i] = std::strtof(p, &end);
            if (end == p || !std::isfinite(out[i])) said = Said::Malformed;
            if (said == Said::Malformed) break;
            p = end;
        }
        if (said == Said::Malformed) whole = std::string(r.line);
    });
    return said;
}

// An edge's time, taken the first time it is seen.
void Edge(Clock::time_point& t) {
    if (t == Clock::time_point{}) t = Clock::now();
}

// The game's mode and the gamemode's isHalloween as the arm found them, while its own are in force.
struct Env {
    bool held = false;
    int  mode = -1;
    bool halloween = false;
};

bool HoldEnv(Env& e) {
    if (e.held) return true;
    void* gi = ue_wrap::world_singleton::GameInstance();
    void* gm = ue_wrap::world_singleton::Gamemode();
    bool halloween = false;
    if (!gi || !gm || !R::IsLive(gm) || !ReadBool(gm, L"isHalloween", halloween)) return false;
    const int mode = GM::ReadFrom(gi);
    if (mode < 0) return false;
    GM::WriteTo(gi, GM::kSandbox);
    WriteBool(gm, L"isHalloween", true);
    e = Env{true, mode, halloween};
    Info("witch mode %d and isHalloween %d put in force (sandbox, true)", mode, halloween ? 1 : 0);
    return true;
}

void PutEnv(Env& e) {
    if (!e.held) return;
    void* gi = ue_wrap::world_singleton::GameInstance();
    void* gm = ue_wrap::world_singleton::Gamemode();
    if (gi) GM::WriteTo(gi, e.mode);
    if (gm && R::IsLive(gm)) WriteBool(gm, L"isHalloween", e.halloween);
    Info("witch mode and isHalloween put back (%d, %d)", e.mode, e.halloween ? 1 : 0);
    e = Env{};
}

// ---- the host ----

enum class HP : uint8_t { Wait, Spawn, Rest, Aim, Judge, Done };

struct Host {
    HP                    phase = HP::Wait;
    Clock::time_point     at{}, readyAt{};
    Env                   env;
    ES::Counters          sync0{};
    ER::Counters          req0{};
    ue_wrap::CachedObjRef car, crate;
    Rest                  rest;
    bool                  shotSaid = false;
    bool                  seated = false;      // the client's seated line has been read: seatedLine holds its numbers
    float                 seatedLine[6]{};     // the rocket's spawn point and the camera's forward, as the client said
    Mark                  mark = 0;  // the newest chat key at the arm's start: the rows after it are this run's
};
Host g_h;

void HostEnter(HP p) {
    g_h.phase = p;
    g_h.at = Clock::now();
}

// The car and the crate stay in the world: the run ends on the done line.
void HostEnd() {
    Unguard();
    PutEnv(g_h.env);
    g_h.phase = HP::Done;
}

void HostStop(const char* why) {
    Warn("ABANDONED %s", why);
    HostEnd();
}

// True (and the arm abandoned) once the phase has waited its bound.
bool HostLate(const char* why) {
    if (!Past(g_h.at, kBound)) return false;
    HostStop(why);
    return true;
}

// The state in force on the host, then the car at the blast point; the client is told where it stands.
void HostSpawn() {
    void* me = coop::players::Registry::Get().Local();
    Vec B{};
    // The family's host half (Guard(true): the gamemode's rule alone), released with the arm. It does not shield the
    // host's player or the car: a rocket that meets something inside the car's radius blasts the car, which can add a
    // second capture, which the captured compare reads as a FAIL; a single blast there still passes the counters (the
    // arm proves the request crossed and was served, not where the rocket hit), and the blast log's distances say
    // where it fell.
    g_src.host = true;
    const Fix f = HoldEnv(g_h.env) && !Guard(true) ? ComputeB(me, me, B) : Fix::Wait;
    if (f == Fix::Wait) {
        HostLate("the gamemode or the fixture never resolved");
        return;
    }
    void* cls = ResolveClass(kCarCls, kCarPath);
    if (f != Fix::Ok || !cls) return HostStop("the fixture is blocked or untraceable, or car1_witch_C is unloadable");
    void* car = E::SpawnActor(cls, {B.X, B.Y, B.Z + kLift});
    if (!car) {
        HostLate("the witch car could not be spawned");
        return;
    }
    g_h.car.Set(car);
    Info("host witch car spawned at (%.0f,%.0f,%.0f)", B.X, B.Y, B.Z + kLift);
    char line[160];
    std::snprintf(line, sizeof(line), "%s %.0f %.0f %.0f", kHostLine, B.X, B.Y, B.Z + kLift);
    HD::Say(line);
    HostEnter(HP::Rest);
}

// The car at rest; the crate waits for the client's rocket line.
void HostRest() {
    void* car = g_h.car.Get();
    if (!car) return HostStop("the witch car is gone before it rested (it destroys itself in story mode)");
    NotePhysics(g_h.rest, car);
    if (!AtRest(g_h.rest)) {
        HostLate("the witch car never came to rest");
        return;
    }
    Vec at{};
    E::TryGetActorLocation(car, at);
    Info("host witch car at rest (%.0f,%.0f,%.0f)", at.X, at.Y, at.Z);
    HostEnter(HP::Aim);
}

// The client's seated line, read from the Spawn phase on: a live row retires from the live tier after a few seconds,
// and the client can sit while the host's car is still coming to rest. False when the line was malformed and the arm
// has ended.
bool PollSeated() {
    std::string whole;
    const Said  said = ChatNumbers(kSeatedLine, g_h.seatedLine, 6, g_h.mark, whole);
    if (said == Said::Malformed) {
        Warn("FAIL host arm=%s malformed line: %s", kName, whole.c_str());
        HostEnd();
        return false;
    }
    if (said == Said::Ok) g_h.seated = true;
    return true;
}

// The crate goes on the rocket's line (its spawn point, the car camera's direction), with physics off so it stays where
// the line points; the client fires once the host says so.
void HostAim() {
    if (Past(g_h.readyAt, kHostBound)) return HostStop("the client never said it was seated");
    if (!g_h.seated || !HD::PollDue()) return;   // the crate's spawn is tried once per poll, until the bound
    const float* v = g_h.seatedLine;
    void* cls = ResolveClass(kCrateCls, kCratePath);
    if (!cls) return HostStop("the crate's class is unloadable");
    void* crate = E::SpawnActor(cls, {v[0] + v[3] * kTargetCm, v[1] + v[4] * kTargetCm, v[2] + v[5] * kTargetCm});
    if (!crate) return;  // a refused spawn is tried again on the next poll, until the bound
    if (!E::SetComponentSimulatePhysics(ue_wrap::prop::GetStaticMesh(crate), false))
        return HostStop("the crate's physics could not be turned off");
    g_h.crate.Set(crate);
    Info("host witch crate on the rocket's line (%.0f,%.0f,%.0f) + %.0f cm along (%.3f,%.3f,%.3f)", v[0], v[1], v[2],
         kTargetCm, v[3], v[4], v[5]);
    HD::Say(kTargetLine);
    HostEnter(HP::Judge);
}

void HostJudge() {
    Host&              h = g_h;
    const ER::Counters c = ER::GetCounters();
    const uint64_t     shots = c.shotsAccepted - h.req0.shotsAccepted;
    // The lane counts each accepted shot under its launcher (explosion_request_host.cpp, JudgeShot), so the shot is the
    // witch car's when its counter is the one that moved.
    constexpr size_t   kWitch = static_cast<size_t>(RC::Launcher::WitchCar);
    const uint64_t     witchShots = c.shotsAcceptedBy[kWitch] - h.req0.shotsAcceptedBy[kWitch];
    const uint64_t     captured = ES::GetCounters().captured - h.sync0.captured;
    if (shots >= 1 && witchShots != 1) {
        Warn("FAIL host arm=%s launcher got=%llu want=1 (shots accepted from the witch car's launcher, %u)", kName,
             U(witchShots), static_cast<unsigned>(RC::Launcher::WitchCar));
        return HostEnd();
    }
    uint64_t refused = 0, accepted = 0;
    Miss     m;
    Compare(m, "shotsAccepted", shots, 1);
    Compare(m, "captured", captured, 1);
    Compare(m, "evicted", c.evicted - h.req0.evicted, 0);
    for (int r = 0; r < static_cast<int>(ER::Refusal::Count); ++r) {
        const uint64_t d = c.refused[r] - h.req0.refused[r];
        refused += d;
        Compare(m, ER::RefusalName(static_cast<ER::Refusal>(r)), d, 0);
    }
    for (int k = 0; k < 3; ++k) {
        const uint64_t d = c.requestsAccepted[k] - h.req0.requestsAccepted[k];
        accepted += d;
        Compare(m, k == 0 ? "requestsAccepted[projectile]" : "requestsAccepted[hand or self]", d, k == 0 ? 1 : 0);
    }
    if (!h.shotSaid && shots == 1 && witchShots == 1 && refused == 0) {
        h.shotSaid = true;
        Info("host shot half PASS arm=%s shotsAccepted=1 launcher=%u not_seated=0", kName,
             static_cast<unsigned>(RC::Launcher::WitchCar));
    }
    if (!m.over && c.expired - h.req0.expired > 0) {
        Warn("ABANDONED INCONCLUSIVE arm=%s the rocket expired: its blast half is not shown", kName);
        return HostEnd();
    }
    if (!m.name) {
        Info("host DONE PASS arm=%s accepted=%llu captured=%llu", kName, U(accepted), U(captured));
        return HostEnd();
    }
    if (!m.over && !Past(h.readyAt, kHostBound)) return;
    Warn("FAIL host arm=%s %s got=%llu want=%llu", kName, m.name, U(m.got), U(m.want));
    HostEnd();
}

// ---- the client ----

enum class CP : uint8_t { Wait, Walk, Sit, Target, Judge, Done };

struct Client {
    CP                    phase = CP::Wait;
    Clock::time_point     at{}, dissAt{}, appliedAt{}, firedAt{};
    Env                   env;
    ES::Counters          sync0{};
    ER::Counters          req0{};
    ue_wrap::CachedObjRef car;
    std::shared_ptr<coop::director::BackgroundWalk> walk;
    atv_seat::SitTry      sit;
    Vec                   spawn{};  // where the host said its car stands
    bool                  haveSpawn = false;
    Mark                  mark = 0;  // the newest chat key when the client said it was ready
};
Client g_c;

void ClientEnter(CP p) {
    g_c.phase = p;
    g_c.at = Clock::now();
}

void ClientEnd() {
    if (g_c.walk && g_c.walk->state.load() == 0) coop::director::CancelBackgroundWalk(*g_c.walk);
    g_c.walk.reset();
    Unguard();
    PutEnv(g_c.env);
    g_c.phase = CP::Done;
}

void ClientAbandon(const char* why) {
    Warn("ABANDONED %s", why);
    ClientEnd();
}

// True (and the arm abandoned) once the phase has waited its bound.
bool ClientLate(std::chrono::seconds bound, const char* why) {
    if (!Past(g_c.at, bound)) return false;
    ClientAbandon(why);
    return true;
}

// The client acts once its own streams run, as a player in a settled world does; its state is then put in force,
// its car class resolved (so the mirror can spawn) and the host told.
void ClientWait() {
    g_src.host = false;
    const bool streams = coop::net_pump::HasAnnouncedWorldReady() &&
                         coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle &&
                         coop::net_pump::IsWorldSettled() && coop::join_membership_sweep::HasLoadTailQuiesced();
    // Guard holds startInvinc (the host's blast copy reaches the seated client) and also unlockFungunsInStory, which this
    // arm does not need: it writes sandbox, and the helper has no hold of the first alone.
    if (!streams || !HoldEnv(g_c.env) || Guard(false)) {
        ClientLate(kVerdictBound, "the client's streams, the gamemode or the guard never resolved");
        return;
    }
    if (!ResolveClass(kCarCls, kCarPath)) return ClientAbandon("car1_witch_C is neither loaded nor loadable by path");
    g_c.sync0 = ES::GetCounters();
    g_c.req0 = ER::GetCounters();
    SnapshotPre();  // the arm's rocket is the one born after this
    g_c.mark = FeedMark();  // the host's lines are the rows after the chat as it stands now
    HD::Say(kReadyLine);
    Info("client witch state in force, the host told");
    ClientEnter(CP::Walk);
}

// The witch car's mirror here: the nearest one within kMirrorCm of the point the host said its car stands at, which
// the host's announce places. A car farther away (a leftover, or a mirror not yet placed, at the origin) is not it,
// and none means the caller keeps waiting.
void* FindMirror(const Vec& spawn) {
    struct Best {
        Vec   spawn;
        void* car = nullptr;
        float dist = 0.f;
    } best{spawn};
    void* cls = ue_wrap::object_index::ClassByName(kCarCls);
    if (cls)
        ue_wrap::object_index::ForEachInstance(
            cls,
            [](void* ctx, void* obj, int32_t) {
                if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
                auto* b = static_cast<Best*>(ctx);
                Vec   at{};
                if (!E::TryGetActorLocation(obj, at)) return;
                const float d = Dist(at, b->spawn);
                if (d > kMirrorCm) return;
                if (!b->car || d < b->dist) {
                    b->car = obj;
                    b->dist = d;
                }
            },
            &best);
    return best.car;
}

// The host said where its car stands, the mirror found, the walk to it started with the director and waited for.
void ClientWalk() {
    Vec at{};
    if (!g_c.walk) {
        if (!g_c.haveSpawn) {
            float       p[3];
            std::string whole;
            const Said  said = HD::PollDue() ? ChatNumbers(kHostLine, p, 3, g_c.mark, whole) : Said::No;
            if (said == Said::Malformed) {
                Warn("FAIL client arm=%s malformed line: %s", kName, whole.c_str());
                return ClientEnd();
            }
            if (said == Said::Ok) {
                g_c.spawn = {p[0], p[1], p[2]};
                g_c.haveSpawn = true;
            } else {
                ClientLate(kVerdictBound, "the host never said where its witch car stands");
            }
            return;
        }
        void* car = FindMirror(g_c.spawn);
        if (!car || !E::TryGetActorLocation(car, at)) {
            ClientLate(kVerdictBound, "the witch car's mirror never stood here");
            return;
        }
        g_c.car.Set(car);
        g_c.walk = coop::director::StartBackgroundWalk(at, kReachCm, kWalkDeadlineS);
        Info("client witch car at (%.0f,%.0f,%.0f), walking to it", at.X, at.Y, at.Z);
        return;
    }
    if (!g_c.car.Get()) return ClientAbandon("the witch car is gone before the walk ended");
    const int state = g_c.walk->state.load();
    if (state == 2) return ClientAbandon("the director's walk to the car did not arrive");
    if (state == 1) ClientEnter(CP::Sit);
}

// A component the car's Blueprint holds in a member of that name; null when it is unresolved or dead.
void* CarComponent(void* car, R::InstanceOffset& member) {
    const int32_t off = member.Of(car);
    void* comp = off < 0 ? nullptr : *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(car) + off);
    return comp && R::IsLive(comp) ? comp : nullptr;
}

// The rocket's line as the car's left-mouse event builds it (car1_witch, bp_cpp): the world location of the `fire`
// arrow and the world rotation of the Camera. False when either does not read, a failed rotation read included.
bool FireLine(void* car, Vec& origin, Vec& fwd) {
    static R::InstanceOffset sFire{L"fire"};
    static R::InstanceOffset sCamera{L"Camera"};
    void* arrow = CarComponent(car, sFire);
    void* cam = CarComponent(car, sCamera);
    ue_wrap::FRotator rot;
    if (!arrow || !cam || !E::TryGetComponentWorldRotation(cam, rot)) return false;
    origin = E::GetComponentLocation(arrow);
    constexpr float kDeg = 3.14159265f / 180.f;
    const float     pitch = rot.Pitch * kDeg, yaw = rot.Yaw * kDeg;
    fwd = {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
    return Len(origin) > 0.f;
}

// The seat through the game's own verb. Seated, the rocket's line is said to the host, which puts the crate on it. The
// seat claim and its first state leave in the tick the lane makes the claim, so the host has the seat before the shot.
void ClientSit() {
    void* car = g_c.car.Get();
    void* player = coop::players::Registry::Get().Local();
    if (!car) return ClientAbandon("the witch car is gone before the seat");
    if (!player) {
        ClientLate(kBound, "the local player is gone before the seat");
        return;
    }
    if (!atv_seat::TrySit(player, car, g_c.sit)) return;
    if (coop::atv_sync::ActorSeatedBy(coop::players::Registry::Get().LocalPeerId()) != car) {
        Warn("FAIL client arm=%s seated got=0 want=1 (see the [ATVP] sit lines)", kName);
        return ClientEnd();
    }
    Vec loc{}, fwd{};
    if (!FireLine(car, loc, fwd)) return ClientAbandon("the rocket's line (fire arrow, camera) did not read");
    char line[200];
    std::snprintf(line, sizeof(line), "%s %.0f %.0f %.0f %.3f %.3f %.3f", kSeatedLine, loc.X, loc.Y, loc.Z, fwd.X,
                  fwd.Y, fwd.Z);
    HD::Say(line);
    Info("client witch seated, rocket from (%.0f,%.0f,%.0f) facing (%.3f,%.3f,%.3f)", loc.X, loc.Y, loc.Z, fwd.X, fwd.Y,
         fwd.Z);
    ClientEnter(CP::Target);
}

// Once the host says the crate stands, the car's left-mouse event runs once with an empty key.
void ClientTarget() {
    void* car = g_c.car.Get();
    if (!car) return ClientAbandon("the witch car is gone before the shot");
    if (ClientLate(kBound, "the host never said the crate stood")) return;
    if (!HD::PollDue() || !ChatHasNew(kTargetLine, g_c.mark)) return;
    Vec loc{}, fwd{};
    if (FireLine(car, loc, fwd))  // the line at the shot, to read against the line the host used
        Info("client witch fires, rocket from (%.0f,%.0f,%.0f) facing (%.3f,%.3f,%.3f)", loc.X, loc.Y, loc.Z, fwd.X,
             fwd.Y, fwd.Z);
    void* fn = R::FindFunction(R::ClassOf(car), kFireFn);
    ue_wrap::ParamFrame f(fn);
    if (!fn || !f.valid() || !ue_wrap::Call(car, f)) return ClientAbandon("the car's left-mouse event did not run");
    g_c.firedAt = Clock::now();
    Info("client witch fired");
    ClientEnter(CP::Judge);
}

void ClientJudge() {
    Client&            c = g_c;
    const ER::Counters rc = ER::GetCounters();
    const ES::Counters sc = ES::GetCounters();
    uint64_t           sent = 0, applied = 0;
    Miss               m;
    for (int k = 0; k < 3; ++k) {
        const uint64_t out = rc.requestsSent[k] - c.req0.requestsSent[k];
        sent += out;
        applied += sc.applied[k] - c.sync0.applied[k];
        Compare(m, k == 0 ? "requestsSent[projectile]" : "requestsSent[hand or self]", out, k == 0 ? 1 : 0);
    }
    const uint64_t dissolved = sc.dissolved - c.sync0.dissolved;
    if (dissolved >= 1) Edge(c.dissAt);
    if (applied >= 1) Edge(c.appliedAt);
    Compare(m, "shotsSent", rc.shotsSent - c.req0.shotsSent, 1);
    Compare(m, "dissolved", dissolved, 1);  // the client's own rocket is dissolved once
    Compare(m, "applied", applied, 1);      // and the host's blast copy is applied once
    // The counters the arm does not use stay at 0.
    Compare(m, "pendingDropped", rc.pendingDropped - c.req0.pendingDropped, 0);
    Compare(m, "pendingDead", rc.pendingDead - c.req0.pendingDead, 0);
    Compare(m, "pendingOrphaned", rc.pendingOrphaned - c.req0.pendingOrphaned, 0);
    Compare(m, "clientDropped", rc.clientDropped - c.req0.clientDropped, 0);
    Compare(m, "unmappedProjectile", rc.unmappedProjectile - c.req0.unmappedProjectile, 0);
    Compare(m, "shotNotAnnounced", rc.shotNotAnnounced - c.req0.shotNotAnnounced, 0);
    if (!m.name) {  // equal counters are not the end: the rocket must be gone too, so a second blast is read
        const int standing = Count(kRocketCls, true);
        if (standing > 0) m = Miss{"projectiles standing", static_cast<uint64_t>(standing), 0, false};
    }
    if (m.name) {
        if (!m.over && !Past(c.at, kVerdictBound)) return;
        Warn("FAIL client arm=%s %s got=%llu want=%llu", kName, m.name, U(m.got), U(m.want));
        return ClientEnd();
    }
    const Clock::time_point from = c.dissAt != Clock::time_point{} ? c.dissAt : c.firedAt;
    const long long ms = (std::max)(0LL, static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             c.appliedAt - from).count()));
    Info("client DONE PASS arm=%s sent=%llu applied=%llu ms=%lld", kName, U(sent), U(applied), ms);
    ClientEnd();
}

}  // namespace

void WitchHostTick() {
    Host& h = g_h;
    switch (h.phase) {
    case HP::Wait:
        if (h.at == Clock::time_point{}) {
            h.sync0 = ES::GetCounters();
            h.req0 = ER::GetCounters();
            h.mark = FeedMark();
            h.at = Clock::now();
        }
        if (HD::PollDue() && ChatHasNew(kReadyLine, h.mark)) {
            h.readyAt = Clock::now();
            return HostEnter(HP::Spawn);
        }
        if (Past(h.at, kVerdictBound)) {
            Warn("ABANDONED the client never said its state was in force");
            HostEnd();
        }
        return;
    case HP::Spawn:
    case HP::Rest:
    case HP::Aim:
        if (!h.seated && HD::PollDue() && !PollSeated()) return;
        if (h.phase == HP::Spawn) return HostSpawn();
        if (h.phase == HP::Rest) return HostRest();
        return HostAim();
    case HP::Judge: return HostJudge();
    case HP::Done: return;
    }
}

void WitchClientTick() {
    Client& c = g_c;
    if (c.at == Clock::time_point{}) {  // the bounds run from the world-ready edge, not from the first Tick
        if (!coop::net_pump::HasAnnouncedWorldReady()) return;
        c.at = Clock::now();
    }
    switch (c.phase) {
    case CP::Wait: return ClientWait();
    case CP::Walk: return ClientWalk();
    case CP::Sit: return ClientSit();
    case CP::Target: return ClientTarget();
    case CP::Judge: return ClientJudge();
    case CP::Done: return;
    }
}

void WitchReset() {
    if (g_c.phase != CP::Done) ClientEnd();
    Unguard();  // the host half's hold, when its arm was still running at the disconnect
    PutEnv(g_h.env);
    g_c = Client{};
    g_h = Host{};
}

}  // namespace coop::dev::explosion_drill::detail
