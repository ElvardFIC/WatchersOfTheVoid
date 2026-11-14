#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <cstddef>
#include <functional>
namespace ue_wrap::dingus_music {
struct State { bool playing=false,used=false; float volume=0,pitch=1,elapsed=0;uint32_t episode=0; };
struct Clock {CachedObjRef audio,sound;float duration=0,pitch=1;};
struct Playback {CachedObjRef audio,sound;uint32_t episode=0;uint64_t revision=0;};
bool IsDingus(void* actor);
// Sound-only support also covers prop_abplush's qwak component.
bool IsSqueakyToy(void* actor);
bool IsAltUse(void* function,const uint8_t* params);
size_t Collect(CachedObjRef* out,size_t cap);
bool Read(void* actor,State& out);
// Optional position support for a native looping SoundWave; no guessed cue duration.
bool ReadClock(void* actor,Clock& out,const std::function<bool()>& current={});
bool Apply(void* actor,const State& state,const std::function<bool()>& current={});
bool ApplyPlayback(void* actor,const State& state,Playback& playback,const std::function<bool()>& current={});
bool StopMusic(void* actor,const std::function<bool()>& current={});
// Native action5 is touch followed by ws1.ToggleActive. No replay of action6,
// which also changes gameplay counters and can eventually destroy the toy.
bool Interact(void* actor,bool toggle,const std::function<bool()>& current={});
bool ReadSqueakPitch(void* actor,float& pitch);
// True once playback was dispatched; a post-dispatch fault must not replay a one-shot.
bool Squeak(void* actor,float pitch,const std::function<bool()>& current={});
}
