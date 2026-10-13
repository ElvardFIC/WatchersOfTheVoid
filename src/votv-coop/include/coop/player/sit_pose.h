// coop/player/sit_pose.h -- seated pose for every player body that sits: a chair, a sitBox, or
// the ATV driver seat. The game attaches the pawn but leaves the kerfur AnimBP in idle, so
// custom skins (and puppets) stand through the furniture. This lane drives the Omega's own
// sitOnAtv sequence and streams a pose bit so the puppet does the same.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct SitAnchorPayload;
}

namespace coop::sit_pose {

void Install(coop::net::Session* session);

// Local body: poll sittingOn / ATV drive and drive the AnimBP. Cheap no-op when standing.
// Game thread.
void Tick();

// After the puppet pose: snap a remote driver's body onto the ATV saddle so they do not trail
// the rig. Game thread.
void DriveAtvDrivers();

// True iff the local player should stream the sitting bit this tick.
bool LocalIsSitting();

// A peer sat on a chair, or stood up. Game thread.
void OnAnchor(uint8_t slot, const coop::net::SitAnchorPayload& payload);

// After the puppet pose: put a remote sitter on the chair they named.
void DriveRemotes();

void OnPeerLeft(uint8_t slot);
void OnDisconnect();

}  // namespace coop::sit_pose
