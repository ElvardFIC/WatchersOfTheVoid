#pragma once
#include <cstdint>
namespace coop::net { class Session; struct CordStatePayload; struct PropPoseSnapshot; struct PropReleasePayload; }
namespace coop::cord_sync {
void Install(net::Session* session);
void Tick();
void Apply(const net::CordStatePayload& p);
void OnIntent(const net::CordStatePayload& p, uint8_t senderSlot);
void SendAllToSlot(int slot);
// Reuse the per-player prop pose stream for the cord plug's moving head.
bool UpdateLocalDragPose(net::Session& session, bool anotherPropIsHeld);
bool IsDragPose(const net::PropPoseSnapshot& pose);
void ApplyDragPose(const net::PropPoseSnapshot& pose, uint8_t senderSlot);
// True for the reserved cord identity, including stale releases.
bool ApplyDragRelease(const net::PropReleasePayload& p, uint8_t senderSlot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
