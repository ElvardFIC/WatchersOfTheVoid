#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/types.h"
#include <functional>
namespace ue_wrap::prop_explosion {
struct State {
    FVector position{};
    float scale=1, radius=0, damage=0, fireStrength=0, fireFuel=0, innerShake=0, outerShake=0;
    bool ignoreWalls=false, skipSetting=false;
};
bool IsGascan(void* actor);
bool IsExplosion(void* actor);
bool Valid(const State&);
bool Read(void* actor,State&);
bool BreakGascan(void* actor,bool fire);
// Callback runs before Finish/BeginPlay, so the controller can register the replica.
// Once finish is invoked return true even if a native callback destroys the actor:
// its one-shot side effects must never be retried.
bool Replay(const State&,const std::function<bool(void*)>& beforeFinish,
            const std::function<bool()>& current);
// General gameplay blast. PC-screen explosions are private minigame objects;
// underwater splashes are created by the native parent explosion itself.
struct Blast {
    FTransform transform{};
    float radius=1000,damage=50,fireStrength=0,fireFuel=0,innerShake=1000,outerShake=2000;
    float force=0,debrisForce=0;
    FVector impact{};
    int32_t debris=0;
    uint8_t variant=0; // 0 explosion_C, 1 explosion_saltrocket_C
    bool shake=true,ignoreWalls=false,skipSetting=false;
};
bool IsGameplayBlast(void*);
bool Valid(const Blast&);
bool ReadBlast(void*,Blast&);
bool ReplayBlast(const Blast&,bool authority,const std::function<bool(void*)>& beforeFinish,
                 const std::function<bool()>& current);
}
