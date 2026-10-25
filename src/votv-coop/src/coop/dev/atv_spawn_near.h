// coop/dev/atv_spawn_near.h -- what two drills share to put an ATV of their own into the world: the host spawns an ATV_C
// at a NavMesh point near a place it names, and reads the key the lane's table holds it under once that key can ride a
// lobby-chat line. The ATV health drill's `spawned` arm and the hostile drill's `atv_` family both place their ATVs
// through it. Private to coop/dev. Dev only; game thread.

#pragma once

#include "ue_wrap/core/types.h"

#include <string>

namespace coop::dev::atv_spawn_near {

inline constexpr float kProjectCm = 300.f;   // the NavMesh query's box about the point, on all three axes
inline constexpr float kLiftCm = 50.f;       // the ATV is born this far above the NavMesh point

// One spawn: the actor, and on a failure the three words a drill's FAIL line carries.
struct Spawned {
    void*            actor = nullptr;
    ue_wrap::FVector onMesh{};                // the NavMesh point the ATV was lifted from
    const char*      what = nullptr;          // null on success
    const char*      want = nullptr;
    char             got[96] = {};
    bool             noNav = false;           // the failure was only that no NavMesh point lies near `point`
};

// Spawns an ATV_C kLiftCm above the NavMesh point nearest `point`. `world` is any live actor of this world. False, with
// the failure words and `noNav` set, when the class is not loaded, no NavMesh point lies within kProjectCm of `point`,
// or the spawn returned nothing.
bool SpawnAtNav(void* world, const ue_wrap::FVector& point, Spawned& out);

// The lane's table row for `atv` under a key a chat line can carry: Ok with the key, Wait while the table does not hold it
// yet, Bad with `*why` when the key holds a space or a non-ASCII character.
enum class Key : unsigned char { Wait, Ok, Bad };
Key KeyOf(void* atv, std::wstring* key, const char** why);

}  // namespace coop::dev::atv_spawn_near
