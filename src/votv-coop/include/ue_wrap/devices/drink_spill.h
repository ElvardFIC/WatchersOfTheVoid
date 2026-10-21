#pragma once
#include "ue_wrap/core/types.h"
#include <functional>
#include <string>
namespace ue_wrap::ufunction_hook {struct CallerFrame;}
namespace ue_wrap::drink_spill {
inline constexpr uint8_t kExplosionScorch=4; // Canonical decal only; never a liquid launch or pour.
struct State {
    FVector location{},scale{1,1,1};
    FRotator rotation{},decalRotation{};
    float process=100;
    int32_t type=0;
    uint8_t kind=0;
};
bool Valid(const State& state);
bool IsProjectile(void* actor);
// Beer-family sources with beer/wine Grunge, or the exact native coffee cup.
bool SourceKind(void* actor,uint8_t& kind);
// Break-only sources also include the exact gasoline bottle; never client pours.
bool BreakSourceKind(void* actor,uint8_t& kind);
bool SourceClassKind(const std::wstring& className,uint8_t& kind);
bool DecalKind(void* actor,uint8_t& kind);
bool Read(void* actor,State& state,const std::function<bool()>& current);
struct Spawn {void* actor=nullptr;bool attempted=false;};
// A request uses native class defaults; a canonical mirror restores host process/type.
Spawn Author(const State& state,bool canonical,const std::function<bool()>& current);
bool ApplyExisting(void* actor,const State& state,const std::function<bool()>& current);
bool ImpactSound(const FVector& point,const std::function<bool()>& current);
struct Launch {FVector location{},velocity{},scale{.25f,.25f,.25f};FRotator rotation{};float size=1;uint8_t kind=0;};
bool Valid(const Launch& launch);
bool ReadLaunch(void* actor,Launch& launch,const std::function<bool()>& current);
bool FreezeLaunch(void* actor,const std::function<bool()>& current);
Spawn AuthorLaunch(const Launch& launch,const std::function<bool()>& current);
bool ProjectileKind(void* actor,uint8_t& kind);
bool PourReady(void* source);
bool ReadPourCue(void* source,const ufunction_hook::CallerFrame& frame,uint8_t& kind,FVector& point,const std::function<bool()>& current);
bool PourSound(uint8_t kind,const FVector& point,const std::function<bool()>& current);
}
