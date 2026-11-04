#pragma once
#include <cstdint>
// Older held-item fixtures exercise the ordinary hold route; optional emote
// capture/presentation is exercised against the real controller in run_emote.
namespace coop::emote_sync {
enum class HandDrive {Fallback,Applied,Retired};
inline HandDrive DriveHand(void*,void*){return HandDrive::Fallback;}
}
