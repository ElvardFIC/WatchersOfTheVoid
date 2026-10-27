#pragma once

#include "ue_wrap/world/game_mode.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace ui::host_game_mode {
struct Choice { uint8_t mode; const char* label; const wchar_t* wideLabel; };
// Display order is independent of enum_gamemode's cooked ordinal order.
inline constexpr std::array<Choice,6> kChoices{{
    {ue_wrap::game_mode::kTutorial,"Tutorial",L"Tutorial"},
    {ue_wrap::game_mode::kStory,"Story Mode",L"Story Mode"},
    {ue_wrap::game_mode::kInfinite,"Infinite Mode",L"Infinite Mode"},
    {ue_wrap::game_mode::kAmbient,"Ambience Mode",L"Ambience Mode"},
    {ue_wrap::game_mode::kSandbox,"Sandbox Mode",L"Sandbox Mode"},
    {ue_wrap::game_mode::kSolar,"Solar Mode",L"Solar Mode"},
}};
inline constexpr int IndexOf(int mode) {
    for (size_t i=0;i<kChoices.size();++i) if (kChoices[i].mode==mode) return static_cast<int>(i);
    return -1;
}
// Shared by the render-thread fallback and game-thread native hosting screen.
inline std::atomic<uint8_t> g_mode{ue_wrap::game_mode::kStory};
inline uint8_t Value() { return g_mode.load(std::memory_order_acquire); }
inline bool Select(int mode) {
    if (IndexOf(mode)<0) return false;
    g_mode.store(static_cast<uint8_t>(mode),std::memory_order_release);
    return true;
}
inline const wchar_t* Label(int mode) {
    const int index=IndexOf(mode);
    return index<0?L"Unknown":kChoices[static_cast<size_t>(index)].wideLabel;
}
}
