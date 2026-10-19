#pragma once
#include <cstdint>
namespace coop::net { class Session; struct EventCuePayload; }
namespace coop::world_sound_sync {
// EventCue ids 1..5 are reserved here; id 0 remains the meteor emitter.
inline constexpr uint32_t kEarthIn = 1, kEarthOut = 2, kAlien = 3, kPlantGather = 4, kFaucet = 5;
void Install(net::Session* session);
bool OnReliable(const net::EventCuePayload& payload);
void OnPeerLeftSlot(int slot);
void OnDisconnect();
}
