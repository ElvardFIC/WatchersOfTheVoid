#pragma once
#include <cstdint>
#include <vector>
namespace coop::net { class Session; struct BlobChunkPayload; }
namespace coop::placed_save_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::BlobChunkPayload& chunk, uint8_t senderSlot);
void QueueConnectBroadcastForSlot(int slot);
void OnPeerLeftSlot(int slot);
void OnDisconnect();
// Capture a pre-existing host wall before a native mutation can destroy it.
void TrackWall(void* actor);
// Native host mutation completed; publish its current save record on the next tick.
void MarkChanged(void* actor);
// Share the PlacedSave chunk sequence with the garden action producer.
bool SendGardenAction(const std::vector<uint8_t>& bytes);
}
