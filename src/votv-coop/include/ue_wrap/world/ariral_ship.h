#pragma once
#include "ue_wrap/core/types.h"
#include <functional>
#include <string>
#include <vector>

namespace ue_wrap::ariral_ship {
struct Appearance {
    std::wstring name;
    FVector at{};
    float alpha = 0;
    bool visible = false;
};
bool IsActor(void* actor);
// Current-world, exact arirShipAppear_C instances; no object-array name walk.
std::vector<void*> Actors();
bool Read(void* actor, Appearance& state);
bool Paint(void* actor, const Appearance& state, const std::function<bool()>& current);
// The caller must suppress appear() while using the native trigger solely to
// bind/unbind its photo delegate. This does not invoke photoTaken or grant rewards.
bool PhotoSubscription(void* actor, bool active, const std::function<bool()>& current);
// Idempotent per-context native event bookkeeping; leaves other event senders alone.
bool Activity(void* actor, bool active, const std::function<bool()>& current);
bool Air(void* actor, const std::function<bool()>& current);
}
