#pragma once
#include <cstdint>
namespace coop::net {
inline constexpr int kMaxPeers = 4;
enum class Role { Host, Client };
enum class ReliableKind { SleepState, DreamScene };
struct SleepStatePayload { uint8_t op{}, flag{}, count{}, total{}; };
struct DreamScenePayload { char dreamClass[32]{}; };
}
