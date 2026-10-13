// ue_wrap/devices/atv.h -- standalone engine access for the VOTV ATV/quadbike (AATV_C).
// Principle-7 engine-wrapper layer (NO network/coop state); coop::atv_sync drives the sync.
//
// AATV_C : APawn is a CUSTOM physics rig (NOT a UWheeledVehicleMovementComponent vehicle):
// the root Mesh simulates PhysX, the four wheels are SEPARATE simulating bodies held by
// UPhysicsConstraintComponents (sus_*/ax_*). Identity = the save-placed Key (cross-peer
// stable). Mesh IS the actor ROOT component, so the actor transform == the physics body
// transform: we read/drive at the ACTOR level, reusing ue_wrap::engine.
//
// A non-authoring peer runs the rig NATIVELY -- physics ON, tick ON -- and is CORRECTED toward
// the authority: the sync layer changes NOTHING about the actor itself, because the rig's entire
// visible output IS suspension travel and a kinematic root teleported 20x/s drags four
// constrained bodies behind it. What a mirror may not do is author COLLISION damage, cancelled
// at the seven UFunctions below. The model: docs/vehicles.md. MTA: CClientVehicle::Update-
// TargetPosition + CNetAPI::ReadVehiclePuresync.

#pragma once

#include "ue_wrap/core/types.h"  // FVector, FRotator

#include <string>

namespace ue_wrap::atv {

// Resolve AATV_C + the field offsets. Idempotent; true once resolved. Game thread.
bool EnsureResolved();

// True iff `obj`'s class is ATV_C or a subclass. False while ATV_C is not loaded, or still loading.
// ATV_C loads with the main menu and stays one object for the process (measured across a rejoin),
// so the object scan hub's one judgement per class holds. Game thread.
bool IsAtv(void* obj);

// The ATV's save-persistent Key as a wide string ("" on failure, L"None" if unkeyed).
std::wstring GetKeyString(void* atv);

// How many live ATV_C actors the object index lists, the class default excluded.
// -1 while that index still owes births from a load: a count taken then is short, and a
// companion minted from it becomes a third scooter once the missing one appears.
int CountLiveSettled();

// A runtime ATV the host minted has to be its own save object. When its Key is still None,
// this writes `coopatv1` through the ATV's own setKey, unless another scooter already owns
// that key: a live one, one the game's key map holds before it is live, or one the loaded
// world save names. The next free key is `coopatv2`. An actor that already carries a
// colliding key is moved there, or this returns empty when it cannot, so the caller removes
// the companion instead of folding both scooters into one record. Clears skipSave/ignoreSave
// so the game's save walk records THIS scooter.
// Returns the key now on the actor, or empty when a colliding key could not be left.
std::wstring EnsureOwnSaveKey(void* atv);

// Write `key` through setKey. False when the function is missing or the call fails.
bool WriteSaveKey(void* atv, const std::wstring& key);

// A live ATV in the current world whose save Key is `key`, or null.
void* FindLiveByKey(const std::wstring& key);

// A live ATV in the current world other than `self` whose save Key is `key`, or null.
void* FindOtherLiveByKey(const std::wstring& key, void* self);

// True when the loaded world save's object records name `key`. The scooter may not be a
// live actor yet. False when the save slot cannot be read; that miss is not cached, so a
// slot that appears later is asked again.
bool LoadedSaveNamesKey(const wchar_t* key);

// How many of those records are ATV_C or a subclass. -1 when the save slot cannot be read.
// A record counted here may not have a live actor yet.
int LoadedSaveAtvCount();

// skipSave / ignoreSave. A client mirror is excluded so it cannot become a third saved scooter.
// The host's companion is included, which is what EnsureOwnSaveKey does.
void SetIncludedInSave(void* atv, bool included);

// Root body world transform (loc + FULL rotation -- the ATV tips/flips). The pose-stream read.
// False on null/unresolved or an unreadable location or rotation.
bool GetRootTransform(void* atv, FVector& loc, FRotator& rot);

// The current driver AmainPlayer_C* (the Player field), or nullptr if unoccupied. The COOP layer
// (atv_sync) resolves this raw pointer to a peer slot -- ue_wrap owns no coop state (principle 7).
void* GetOccupantPlayer(void* atv);

// isDriven -- TRUE while a player is seated (the master "occupied" flag).
bool IsDriven(void* atv);

// Native dismount restores possession, camera and the local player's body.
// False if it cannot run or leaves the vehicle driven; callers retry safely.
bool Dismount(void* atv);

// Nearby supported, unobstructed position for a second identical rig. Never
// searches through a wall or more than 3.5 m from the source. False -> retry later.
bool FindCompanionLocation(void* atv, FVector& location);

// Display state (cheap field reads, resolved by name). 0 or false on failure.
float GetFuel(void* atv);    // fuel, 0..100
float GetHealth(void* atv);  // health, 0..100
bool  GetBrake(void* atv);   // the handbrake
bool  GetLights(void* atv);
bool  GetTurbo(void* atv);
// Whether the scooter's own radio MediaPlayer is playing. `radio` is the radio actor and
// `hasRadio` is the upgrade; neither is this.
bool  GetRadio(void* atv);
// The radio MediaPlayer's current url, or empty when it has none.
std::wstring RadioUrl(void* atv);
// The map upgrade (`hasMap`). False when the field did not resolve.
bool  GetHasMap(void* atv);
void  SetBrake(void* atv, bool on);
void  SetLights(void* atv, bool on);
void  SetTurbo(void* atv, bool on);
// Play or pause the scooter's own radio. No url change; that is Drive on the media player.
void  SetRadio(void* atv, bool on);
// Open `url` on this scooter's radio (empty leaves the source) and play or pause it.
void  DriveRadio(void* atv, const wchar_t* url, bool playing);
// Write `hasMap` and show or hide the digital-map child actor.
void  SetHasMap(void* atv, bool on);
// Bit i is one frame module (air control, belt, big lights, container, floaties, front, radio, solar).
uint8_t ReadModuleMask(void* atv);
void ApplyModuleMask(void* atv, uint8_t mask);
// The ATV's own honk input, so a mirror makes the same sound. No-op if that function is absent.
void PlayHonk(void* atv);
// The native tyre eject: getTire then damageWheel, so the wheel prop is born on this machine.
bool EjectWheel(void* atv, int index);

// The game's OWN rig-consistent teleport: ATV_C::teleportVehicle(NewLocation, NewRotation) --
// K2_SetActorLocation(bTeleport=true) + K2_SetActorRotation(bTeleportPhysics=true), and THEN it
// re-places frontWheel_R onto ax_FR1, frontWheel_L onto ax_FL1 and backWheelRoot onto `back`
// [V, disasm ATV.teleportVehicle @0..393]. This is the WARP primitive and the only transform the
// sync layer ever writes: a bare SetActorLocation moves the ROOT only
// (engine_attach.cpp:74-82) and leaves the constrained wheel bodies behind -- which is exactly
// why the game ships two teleport forms. False on unresolved/null. Game thread.
bool TeleportRig(void* atv, const FVector& loc, const FRotator& rot);

// Resolve ATV_C's SEVEN ComponentHitSignature bound-event UFunctions (`mesh`, `car1_Capsule`,
// `car1_backWheel_R/L`, `car1_frontWheel_R`, `car1_frontWheelRoot`, `car1_backWheelRoot`) into
// `out`; returns how many resolved. These are the ONLY PE-visible seam for the ATV's
// collision-authored state: each BndEvt stub just copies its params and jumps into the ubergraph
// [V, disasm], and the work it reaches -- impulse() -> `health -= |NormalImpulse|/500000 * 2 *
// getBumperMult()` -> explode() at <=0, and processTire() -> tire durability -> ejectWheel -- is
// all EX_Local* and therefore invisible one level down. A non-owner that runs them explodes its
// own copy of a vehicle the authority still has. Game thread.
int ResolveHitDelegates(void** out, int max);

// Runtime-ATV materialization: fresh-spawn an AATV_C-or-subclass (`className`) at `loc`/`rot`
// via GameplayStatics BeginDeferred + FinishSpawning, leaving physics ON -- the result is a NATIVE
// idle ATV the local player can grab/drive (NOT a frozen mirror). coop::atv_sync uses this when
// the host announces a RUNTIME-SPAWNED ATV (AtvSpawn) that this client has no save-twin of.
// Nothing sells an ATV: the source is the list_props row 'atv' -> spawnAsObject = ATV_C, reached
// from ui_spawnmenu. `className` is validated to descend from ATV_C (trust boundary -- a peer
// could send any string). Returns the spawned actor, or nullptr on resolve/spawn failure.
// Game thread only.
void* SpawnMirror(const std::wstring& className, const FVector& loc, const FRotator& rot);

// Tear down a SpawnMirror'd runtime-ATV mirror -- K2_DestroyActor on the actor -- when the host
// announces it gone (AtvDestroy), or on disconnect. False, and nothing done, on a null or
// already-dead actor or a destroy that did not dispatch. Game thread.
bool DestroyMirror(void* atv);

}  // namespace ue_wrap::atv
