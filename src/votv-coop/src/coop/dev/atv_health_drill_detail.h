// coop/dev/atv_health_drill_detail.h -- what the ATV health drill's files share: the arm table, the phase clock, the
// verdict lines and the lobby-chat lines the two peers say to each other (coop/dev/drill_chat.h). Private to coop/dev:
// atv_health_drill*.cpp.

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace coop::net { class Session; }

namespace coop::dev::atv_health_drill::detail {

using Clock = std::chrono::steady_clock;

enum class Arm : uint8_t { Off, Sit, Grab };

// An arm's names: its own, the host's first edge, the client's act and the client's undo (the phases' names).
struct Spec {
    Arm         arm;
    const char* name;
    const char* first;
    const char* act;
    const char* undo;
};
const Spec& SpecOf(Arm a);
Arm         ArmOf();

inline constexpr uint8_t           kSlot = 1;               // the one slot the drill acts on
inline constexpr const wchar_t*    kKey = L"ATV";           // the world's own ATV, under its game key
inline constexpr uint8_t           kNoSeat = 0xFF;
inline constexpr auto              kFailBound = std::chrono::seconds(180);      // a failure bound, never a wait
inline constexpr auto              kHostFirstBound = std::chrono::seconds(360); // the client's walk and its act
inline constexpr int               kWalkDeadlineS = 300;
inline constexpr float             kSitReachCm = 400.f;     // the director's walk ends this close to the ATV's root
inline constexpr float             kGrabReachCm = 150.f;    // the use key's reach

// One role's phase: the arm's own enum value, its name for the lines, and when it began. `done` once the role has
// printed its verdict.
struct Run {
    int               phase = 0;
    const char*       name = "wait";
    Clock::time_point at{};
    bool              done = false;
};
void Enter(Run& r, int phase, const char* name);
bool Past(const Run& r, std::chrono::seconds bound);
long long Ms(Clock::time_point from, Clock::time_point to);

// The verdict lines. Both end the role's run.
void Fail(const char* role, Arm a, Run& r, const std::string& what, const std::string& got, const std::string& want);
void Pass(const char* role, Arm a, Run& r, const std::string& numbers);

// The lobby-chat lines: the client's ready line (the host's start) and the host's phase edges (the client's).
std::string ReadyLine(Arm a);
std::string PhaseLine(const char* name);

void HostTick(coop::net::Session* s, Arm a);
void ClientTick(Arm a);
void HostReset();
void ClientReset();

}  // namespace coop::dev::atv_health_drill::detail
