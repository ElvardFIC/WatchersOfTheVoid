#pragma once
#include "ue_wrap/actors/emote_catalogue.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <array>
#include <functional>

namespace ue_wrap::emote_wheel {
struct Playback {
    CachedObjRef actor, audio;
    std::array<CachedObjRef,2> instances, montages;
    std::array<uint32_t,2> episodes{};
    std::array<uint16_t,2> parts{};
    State state{};
    uint32_t world=0;
    uint64_t musicAt=0;
    float musicStart=0;
};
// Local observation is read-only; no dependency on the Lua mod or its globals.
void Observe(void* localPawn);
State Sample();
void ResetObservation();
void Stop(Playback& playback);
bool Apply(void* pawn, const State& state, Playback& playback, const std::function<bool()>& current);
bool Eligible(void* pawn);
Hand CaptureHand(void* pawn, void* item, uint32_t generation);
bool PlaceHand(void* pawn, void* item, const Hand& hand, const std::function<bool()>& current);
}
