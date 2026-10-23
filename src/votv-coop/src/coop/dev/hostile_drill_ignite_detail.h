// coop/dev/hostile_drill_ignite_detail.h -- what the ignite family's files share: the arm table, the verdict print and
// the lobby-chat lines the two peers say to each other (coop/dev/drill_chat.h). Private to coop/dev:
// hostile_drill_ignite*.cpp.

#pragma once

#include "coop/dev/hostile_drill_ignite.h"

#include "coop/world/salt_rocket_ignite.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace coop::dev::hostile_drill::ignite::detail {

using Clock = std::chrono::steady_clock;
constexpr auto kFailBound = std::chrono::seconds(180);   // a failure bound, never a wait
constexpr uint8_t kSlot = 1;                             // the one slot the family acts on

// The game's asset paths of the classes the family loads: the targets the host spawns, the tools the client takes.
constexpr const wchar_t* kRocketCls = L"prop_saltrocket_C";
constexpr const wchar_t* kRocketPath = L"/Game/objects/prop_saltrocket.prop_saltrocket_C";
constexpr const wchar_t* kLighterCls = L"prop_lighter_C";
constexpr const wchar_t* kLighterPath = L"/Game/objects/prop_lighter.prop_lighter_C";
constexpr const wchar_t* kTorchCls = L"prop_torch_C";
constexpr const wchar_t* kTorchPath = L"/Game/objects/prop_torch.prop_torch_C";

// What the client holds in its hand for the arm.
enum class Hand : uint8_t { Empty, Lighter, Torch };
// Plain: one run of messages. The others carry a mechanism of their own.
enum class Mode : uint8_t { Plain, NotReady, Flood };

// One arm: the messages the client sends (`sent`, the accounting total), and the host's expected deltas: the lights
// performed by verb, and the one refusal reason with its count. Every counter not named here must stay at 0. The
// flood expects its own shape (queue_full and not_rocket, each at least 1, adding to `sent`).
struct Spec {
    const char*                          name;
    Arm                                  arm;
    Hand                                 hand;
    Mode                                 mode;
    uint32_t                             sent;
    uint32_t                             performed[2];  // attemptIgnite, ignite
    coop::salt_rocket_ignite::Refusal    refusal;       // the one reason expected, Count for none
    uint32_t                             refused;
};
const Spec* SpecOf(Arm a);

// The verdict line: PASS to the info log, FAIL and INCONCLUSIVE as warnings.
void Verdict(Arm a, const char* what, const std::string& why);

// The lobby chat lines the peers say (coop/dev/drill_chat.h): the host's once it is ready, and the client's once its
// start set is recorded, which the host spawns the targets after.
std::string ReadyLine(Arm a);
std::string ClientReadyLine(Arm a);

// The tool the hand holds for the arm: the class name the hand lane compares, the game's row name of the item, and the
// class's asset path; null for an empty hand.
const wchar_t* HandClass(Hand h);
const wchar_t* HandItem(Hand h);
const wchar_t* HandPath(Hand h);

void ClientTick(coop::net::Session* s, const Spec& sp);
void ClientReset();
void HostTick(coop::net::Session* s, const Spec& sp);
void HostReset();

}  // namespace coop::dev::hostile_drill::ignite::detail
