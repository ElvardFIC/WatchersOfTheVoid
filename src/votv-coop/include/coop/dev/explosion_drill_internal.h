// coop/dev/explosion_drill_internal.h -- what the explosion drill's files share: the arm, the fixture and rest helpers
// (coop/dev/explosion_drill_fixture.cpp), the host's half (coop/dev/explosion_drill_host.cpp) and the client's half
// (coop/dev/explosion_drill_client.cpp), the source arms' legs both roles run (coop/dev/explosion_drill_source.cpp,
// its measure legs in coop/dev/explosion_drill_source_measure.cpp), the request arms' two halves
// (coop/dev/explosion_drill_request.cpp, coop/dev/explosion_drill_request_host.cpp) and the witch car arm
// (coop/dev/explosion_drill_witch.cpp).
// Dev only; game thread.

#pragma once

#include "coop/world/explosion_request.h"
#include "coop/world/explosion_sync.h"

#include "ue_wrap/core/types.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace coop::net { class Session; }

namespace coop::dev::explosion_drill::detail {

using Clock = std::chrono::steady_clock;
using Vec = ue_wrap::FVector;

constexpr auto kBound = std::chrono::seconds(60);  // a failure bound, never a wait
// The request arms' verdict, each half: the client's legs come first, and request_expiry's host half waits the shot
// table's life of a shot (30 s) and its slack (2 s) on top of them. One bound for both halves.
constexpr auto kVerdictBound = std::chrono::seconds(120);
constexpr float kFlyDistance = 100.f;
// One of the four papers can barely move, and its host copy barely moves too, so the stream is not the cause; the cause
// is not yet named (the host's fixture and papers lines log the spots). The coast is therefore shown by at least this
// many flying and all four resting again; without the host's coast no client copy moves at all.
constexpr int kMinFlown = 2;
constexpr int   kPapers = 4;

enum class Arm : uint8_t { Off, Dissolve, Apply, Coast, SpawnMalformed, SourceHost, SourceClient,
                           RequestRocket, RequestSticky, RequestDetonate, RequestDetonateSwitch, RequestHand,
                           RequestHandPoop, RequestHandSoap, RequestPipe, RequestSelf, RequestThrown, RequestSwitch,
                           RequestExpiry, RequestRocketEarly, RequestWitch };
inline bool IsSource(Arm a) { return a == Arm::SourceHost || a == Arm::SourceClient; }
inline bool IsRequest(Arm a) { return a >= Arm::RequestRocket; }

// ---- the fixture and the rest helpers (explosion_drill_fixture.cpp) ----

float Dist(const Vec& a, const Vec& b);
float Len(const Vec& v);
bool Past(Clock::time_point t, std::chrono::seconds bound);

// A class from the object index or else the load by path (a class nobody loaded is not in the index); null when
// neither has it.
void* ResolveClass(const wchar_t* name, const wchar_t* path);

void Info(const char* fmt, ...);
void Warn(const char* fmt, ...);

// One body's rest: the Ticks it has stayed under the rest speed, a STATE and never a count of ticks waited.
struct Rest {
    int               ticks = 0;
    bool              have = false;
    Vec               last{};
    Clock::time_point lastAt{};
};
bool AtRest(const Rest& r);
void NoteSpeed(Rest& r, float speed);
// A body that does not simulate (the puppet, a kinematic copy): its speed is its location change over the time
// since the last Tick.
void NoteMoved(Rest& r, const Vec& p);
// A simulating prop's speed is its root's physics velocity; an unreadable body is not at rest.
void NotePhysics(Rest& r, void* actor);

// The blast point B: at `body`'s Z along the clearest of eight directions from its yaw-forward, up to 600 cm and short
// of whatever the line hits (the trace is the static and dynamic one, pawns are not in its set).
enum class Fix : uint8_t { Wait, Ok, Blocked, NoTrace };
Fix ComputeB(void* body, void* ctx, Vec& out);

void RestLine(uint32_t eid, const Vec& at);

// ---- the two roles ----

// The host's half: one Tick of its phases, and back to the start (destroying the papers its coast spawned, which
// outlive its Done so the client can still judge its copies).
void HostTick(coop::net::Session& s, Arm arm);
void HostReset();

// The client's half: one Tick of its phases, and back to the start.
void ClientTick(Arm arm);
void ClientReset();

// ---- the source arms (explosion_drill_source.cpp) ----

// One Tick of a source arm on this role, once its bodies rest: the role the arm names walks its legs (blast point,
// then each source: the held ones through the game's hand verbs, the salt rocket spawned and lit, on the host only),
// the other only reads and reports. Back to the start destroys what the legs left in the world.
void SourceTick(Arm arm, bool isHost);
void SourceReset();

// ---- the request arms (explosion_drill_request.cpp, explosion_drill_request_host.cpp) ----
//
// A client's own explosion becomes the host's once, end to end. The client acts with the source verbs and judges its
// counters, the host judges its own, each as deltas against a baseline it took before the first send. A half prints
// its PASS only once its counters equal their wants and the arm's own rockets and sticky bombs are gone, so a second
// blast after the first is read; a counter past its want fails at once.

// What an arm's half must count. Client: shots and requests SENT, blasts APPLIED, own births dissolved (-1 = not
// asserted). Host: shots and requests ACCEPTED, blasts CAPTURED, shots expired unrequested, and the least ticks a
// judge waited on a hand state it was owed (0 = no floor).
struct RequestWant {
    uint32_t shots;
    uint32_t kind[3];  // by the request's kind less one: projectile, hand, self
    uint32_t blasts;
    int      born;
    uint32_t expired;
    uint32_t waitedMin;
};
const RequestWant& RequestWantOf(Arm a);

// The ini value of a request arm and back; false for a value that names none.
bool        RequestArmFromName(const std::string& value, Arm& out);
const char* RequestArmName(Arm a);

// One counted reading against its want: past it fails at once, short of it is only not yet. `first` keeps the
// worst reading seen (an overshoot over a shortfall, the first of each).
struct Miss {
    const char* name = nullptr;
    uint64_t    got = 0, want = 0;
    bool        over = false;
};
inline void Compare(Miss& first, const char* name, uint64_t got, uint64_t want) {
    if (first.over) return;
    if (got > want) first = Miss{name, got, want, true};
    else if (got < want && !first.name) first = Miss{name, got, want, false};
}

// The client's half: one Tick, true once it has printed its final line. `sync0` and `req0` are the counters at the
// world-ready edge. Back to the start puts back what the arm changed (the guard, the hand, the pill).
bool RequestClientTick(Arm arm, const coop::explosion_sync::Counters& sync0,
                       const coop::explosion_request::Counters& req0);
void RequestClientReset();

// The host's half. Begin takes the baseline once and holds the gamemode's fungun rule (the client's launcher runs
// on the host's world rule); true once held. Tick judges, true once it has printed its final line.
bool RequestHostBegin();
bool RequestHostTick(Arm arm);
void RequestHostReset();

// ---- the witch car arm (explosion_drill_witch.cpp) ----
//
// request_witch does not go through the request arms' table: the client fires from a car's seat, not from a hand. The
// host spawns the car, then a crate on the line the seated client's car camera points along, and judges the shot and
// the blast apart; the client walks to the car, takes the seat, says where its camera points and fires. Each Tick of a
// role is its half; WitchReset puts back the mode and isHalloween the arm wrote.
void WitchHostTick();
void WitchClientTick();
void WitchReset();

}  // namespace coop::dev::explosion_drill::detail
