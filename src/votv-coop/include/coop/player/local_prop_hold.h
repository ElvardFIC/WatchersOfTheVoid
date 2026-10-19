#pragma once
#include "coop/player/players_registry.h"
#include "coop/items/gravity_motion.h"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/actors/physics_gun.h"

namespace coop::local_prop_hold {
inline bool Matches(const ue_wrap::engine::MainPlayerGrabState& state, void* actor) {
    return actor && (state.grabbingActor == actor || state.gravityActor == actor || state.physicsActor == actor ||
                     state.holdingActor == actor);
}
inline bool IsHeld(void* actor) {
    if (actor && gravity_motion::Target()==actor) return true;
    // Gameplay subsystems run before local_streams publishes this frame's held actor.
    // Read the pawn now: yesterday's stream edge must not park a newly grabbed body.
    ue_wrap::engine::MainPlayerGrabState state{};
    return actor && ue_wrap::engine::ReadMainPlayerGrabState(players::Registry::Get().Local(), state) &&
        Matches(state, actor);
}
inline bool Yield(void* actor) {
    if (!actor) return false;
    void* player=players::Registry::Get().Local();
    ue_wrap::engine::MainPlayerGrabState state{};
    if(!ue_wrap::engine::ReadMainPlayerGrabState(player,state))return false;
    if(state.grabbingActor==actor)ue_wrap::engine::ReleaseMainPlayerGrabIfHolding(player,actor);
    if(state.physicsActor==actor)ue_wrap::physics_gun::ReleaseHeldProp(state.holdingActor,actor,player);
    gravity_motion::YieldTarget(actor);
    return !IsHeld(actor);
}
}
