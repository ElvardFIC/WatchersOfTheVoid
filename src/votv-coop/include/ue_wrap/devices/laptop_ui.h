#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ue_wrap::laptop_ui {
// The base PC and portable terminals share this widget. Only cosmetic selection
// and the unpurchased cart live here; purchases keep using order_economy.
struct State {
    std::array<int32_t, 3> tabs{}; // main, media, media viewer
    std::vector<std::wstring> cart;
    bool operator==(const State&) const = default;
};
bool Valid(const State& state); // bounds and local catalog identities, never wire prices
bool Read(State& state);
bool Apply(const State& state); // native cart verbs; no purchase, camera possession or input replay
}
