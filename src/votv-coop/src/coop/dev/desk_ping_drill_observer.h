// coop/dev/desk_ping_drill_observer.h -- sibling-internal surface of the desk-ping drill's observer half
// (desk_ping_drill_observer.cpp): what a peer that does not run the ping sees of it, on the host and on a second client.
// The remover_drill_detail.h shape: private to the src tree, never part of the include surface.

#pragma once

#include <cstdint>
#include <string>

namespace coop::dev::desk_ping_drill::observer {

// Failure bounds both halves of the drill share: each step ends on the state it waits for.
inline constexpr int      kWalkDeadlineS = 240;
inline constexpr uint64_t kStartBoundMs = 60000;
inline constexpr uint64_t kEnterBoundMs = 15000;
inline constexpr uint64_t kVerdictBoundMs = 90000;

// Forget the scan being watched (the maxima and the sample log's memory).
void Reset();

// Read the desk's view of the scan and keep its maxima; `who` names the peer in the sample lines.
void Sample(uint64_t now, const char* who);

// Whether Settled has begun waiting for the scan's ending since the last Reset.
bool SettleStarted();

// The host's verdict rolled: true once the scan's ending was taken since `endedBefore` and the desk reads idle, or at
// the bound.
bool Settled(uint64_t now, uint32_t endedBefore);

// The scan was seen at both stages with coord_isPing clear and its ending taken since `endedBefore`.
bool Passed(uint32_t endedBefore);

// The maxima and the verdict, for a PASS or FAIL line.
std::string MaxText(uint32_t endedBefore);

// The second client's tick. False for the first client, which the drill's client half serves.
bool ClientTick();

// The session ended: the second client's state and the scan being watched are forgotten.
void OnDisconnect();

}  // namespace coop::dev::desk_ping_drill::observer
