#pragma once
#include <cstdint>
namespace coop::net {class Session;}
namespace coop::atv_steering_sync {
int16_t Capture(void* actor);
bool Valid(int16_t yaw);
void Install(net::Session* session);
// Called only after the ATV lane accepts occupancy and excludes the local author.
void Receive(void* actor,int16_t yaw,uint8_t author);
void OnPeerLeft(int slot);
void OnDisconnect();
}
