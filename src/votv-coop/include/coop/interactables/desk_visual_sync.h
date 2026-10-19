#pragma once
#include "coop/net/protocol.h"
namespace coop::net { class Session; }
namespace coop::desk_visual_sync {
// GT only. Host canonical geometry + display progress; never runs the ping FSM.
void Install(net::Session* session);
void Tick();
void OnState(const net::DeskVisualPayload& state, uint8_t sender);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
