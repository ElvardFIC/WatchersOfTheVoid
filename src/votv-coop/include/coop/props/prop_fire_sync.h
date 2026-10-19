#pragma once
#include <cstdint>
namespace coop::net {class Session;struct PropFirePayload;}
namespace coop::prop_fire_sync {
void Install(net::Session* session);
void Tick();
// A burning client hand actor released as a world prop keeps its fire on the host.
void OnHandReleased(void* actor);
void Receive(const net::PropFirePayload& p,uint8_t sender);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
