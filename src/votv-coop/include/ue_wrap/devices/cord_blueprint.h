#pragma once
#include <cstdint>

namespace ue_wrap::cord::blueprint {
// VotV 0.9.0n cooked ExecuteUbergraph_cord entries. A game recook requires
// remeasurement against the compiled Blueprint.
inline constexpr int32_t kReattachResume = 15;
inline constexpr int32_t kAction = 262;
inline constexpr int32_t kUsed = 2110;
inline constexpr int32_t kDestroyed = 2095;
inline bool UserDisconnect(int32_t entry) { return entry == kAction || entry == kUsed; }
inline bool StaleReattach(int32_t entry, bool connected) {
    return entry == kReattachResume && connected;
}
}
