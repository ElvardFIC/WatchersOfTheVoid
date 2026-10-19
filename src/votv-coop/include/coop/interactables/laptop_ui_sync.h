#pragma once
#include "coop/net/protocol.h"
namespace coop::net { class Session; }
namespace coop::laptop_ui_sync {
void Install(net::Session* session);
void Tick();
bool BeforeLocalRelease(); // retain the claim if the final cart/page cannot enter the reliable queue yet
void OnChunk(const net::BlobChunkPayload& chunk, int slot);
void QueueConnectBroadcastForSlot(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
