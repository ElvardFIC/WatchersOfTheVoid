#pragma once
#include "ue_wrap/core/types.h"
#include <functional>
namespace ue_wrap::water_pour {
bool IsPour(void* actor);
bool IsShot(void* actor);
bool IsProjectile(void* actor);
enum class Kind : uint8_t {Bucket,Washer};
struct Birth {FVector location{},velocity{};float soap=0;Kind kind=Kind::Bucket;float power=1;FRotator rotation{};};
struct Impact {FVector location{},normal{},audioLocation{};bool launch=false;};
bool Valid(const Birth& state);
bool Valid(const Impact& state);
// Read only after the bucket's source body returns: it assigns velocity after FinishSpawningActor.
bool ReadBirth(void* actor,Birth& state,const std::function<bool()>& current);
bool ReadImpact(void* actor,void* other,void* function,const void* hit,Impact& state);
struct Spawn {void* actor=nullptr;bool attempted=false;};
// A missing class may retry. Once native construction is attempted it is terminal,
// including an interrupted/faulted call; never duplicate a partially authored birth.
Spawn Author(const Birth& state,const std::function<bool()>& current);
// Each one-shot returns true once dispatch is attempted, including native interruption.
bool Surface(const Impact& state,const std::function<bool()>& current);
bool Sound(const Impact& state,const std::function<bool()>& current);
struct BucketCue {uint8_t kind=0;FVector location{};};
bool ReadBucketCue(void* source,void* function,const uint8_t* params,bool twoD,BucketCue& cue,const std::function<bool()>& current);
bool BucketSound(const BucketCue& cue,const std::function<bool()>& current);
}
