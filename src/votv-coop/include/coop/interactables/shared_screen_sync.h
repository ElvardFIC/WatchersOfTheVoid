#pragma once
#include "coop/net/protocol.h"
namespace coop::net { class Session; }
namespace coop::shared_screen_sync {
void Install(net::Session* session);
void Tick();
void OnChunk(const net::BlobChunkPayload& chunk, uint8_t sender);
void QueueConnectBroadcastForSlot(int slot);
void OnDisconnectForSlot(int slot);
void OnDisconnect();
}
