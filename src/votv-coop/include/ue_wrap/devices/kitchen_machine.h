#pragma once
#include <cstdint>
#include <functional>
#include "ue_wrap/core/types.h"

namespace ue_wrap::kitchen_machine {
enum class Kind : uint8_t { None, Microwave, CoffeeMaker, CoffeeVending, MiniOven, Grill, Cremator };
enum Flag : uint8_t { Active=1, Powered=2, Opened=4, HasCup=8, HasCoffee=16, CoffeeReady=32, Processing=64, HasLid=128 };
struct State {
    Kind kind = Kind::None;
    uint8_t flags = 0;
    int32_t mode = 0;
    float remaining = 0, timer = 0, power = 0;
    int32_t coals = 0;
    float burn = 0;
    bool cooking = false;
    float cookVolume = 0;
    bool hasDoorPose = false;
    FTransform doorPose{}; // relative to the appliance, so a carried appliance keeps its hinge
};
Kind Type(void* actor);
bool Read(void* actor, State& out);
// Paint only: never starts cooking, consumes ingredients, spends credits or spawns a cup.
bool Apply(void* actor, const State& state,const std::function<bool()>& current={});
bool Press(void* actor, void* player, uint8_t action, uint8_t control);
bool SetGrillFire(void* actor,bool active); // native ignition; extinguish without lid/loot verbs
uint8_t ReadControl(void* actor); // minioven: 1=switch, 2=temperature; other machines: 0
void* Door(void* actor);
bool OwnsDoor(void* actor); // kitchen children always use the parent's shared identity
void* DoorParent(void* child);
bool SetDoor(void* actor, bool opened);
bool ReadDoorPose(void* actor, FTransform& pose);
bool ApplyDoorPose(void* actor, const FTransform& pose);
void* DoorUser(void* actor);
bool MoveDoor(void* actor, void* player, const FVector2D& mouse);
void Animate(void* actor, float seconds);
}
