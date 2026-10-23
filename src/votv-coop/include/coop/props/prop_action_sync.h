#pragma once

#include "coop/net/protocol.h"

namespace coop::net { class Session; }

namespace coop::prop_action_sync {
void Install(coop::net::Session* session);
void Tick();
void OnDisconnect();
void OnReliable(const coop::net::PropActionIntentPayload& payload, uint8_t senderSlot);
// Exact active personal MRE spawn body; world package loot belongs to the host.
bool IsLocalMreOutput(void* actor,void* callerFunction);
}
