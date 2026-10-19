#pragma once

#include "coop/net/protocol.h"

namespace coop::net { class Session; }

namespace coop::hand_extinguisher_sync {

void Install(coop::net::Session* session);
void Tick();
void Receive(const coop::net::HandExtinguisherPayload& payload, uint8_t senderSlot);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();

}  // namespace coop::hand_extinguisher_sync
