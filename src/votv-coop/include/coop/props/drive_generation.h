#pragma once
#include <cstdint>

namespace coop::drive_generation {
// A byte sequence has an unambiguous forward half-window; duplicates and the exactly
// opposite byte are not newer. Shared by datagrams, reliable ends and hand-yield fences.
inline constexpr bool After(uint8_t candidate, uint8_t previous) {
    const auto distance = static_cast<uint8_t>(candidate - previous);
    return distance != 0 && distance < 128;
}
inline constexpr bool AtLeast(uint8_t candidate, uint8_t previous) {
    return candidate == previous || After(candidate, previous);
}
inline constexpr uint8_t Next(uint8_t previous) {
    return previous == 255 ? 1 : static_cast<uint8_t>(previous + 1);
}
}
