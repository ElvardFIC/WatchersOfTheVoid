// ue_wrap/actors/kerfus.h -- engine substrate for the plain kerfur (the Kerfus: p_kerfus_C and its
// colour variants): the fields its sync reads and writes, and its refresh, E-press and possess verbs.
// Pure reflection and UFunction access; no net or gameplay logic (principle 7).
// State layouts follow live class incarnations; missing layouts retry. Guarded
// accessors also retain the caller's lifetime proof through reflection work.
// The kerfurOmega NPC is ue_wrap/actors/kerfur. Game thread, every function.

#pragma once

#include "ue_wrap/core/types.h"
#include <functional>

#include <cstdint>

namespace ue_wrap::kerfus {

// The family's base class, the name the script gate's class-scoped watches take.
constexpr const wchar_t* kClassName = L"p_kerfus_C";

// p_kerfus_C or a colour variant; false while the class is not loaded.
bool IsKerfus(void* obj);

// The first live Kerfus, of any colour, that `accept` takes with `arg` (a null `accept` takes the first):
// default objects and dying or unreadable slots are passed over. Walks the object index by each exact class.
void* FindLive(bool (*accept)(void* obj, const void* arg) = nullptr, const void* arg = nullptr);

// `active` (on or off), `charging` (on its cord) and `energy`: false when this group did not resolve,
// the value left as it was.
bool ReadActive(void* k, bool& on);
bool WriteActive(void* k, bool on);
bool ReadCharging(void* k, bool& on);
bool WriteCharging(void* k, bool on);
bool ReadEnergy(void* k, float& energy);
bool WriteEnergy(void* k, float energy);

// Its navigation pawn (`pawn`): the Character its BeginPlay spawns 40 cm above it and welds onto its body, whose
// AI controller finds the path its wheel drives along. Null when not spawned, not live, or the field did not
// resolve.
void* NavPawn(void* k);

// The game's own refresh after `active` changes: upd(skipFace) -- the face, the sounds, the camera.
bool RunUpd(void* k, bool skipFace);

// Writes expose a committed active bit; upd exposes dispatch even if its body fails.
bool IsKerfusWhile(void* k, const std::function<bool()>& current);
bool ReadActiveWhile(void* k, bool& value, const std::function<bool()>& current);
bool ReadChargingWhile(void* k, bool& value, const std::function<bool()>& current);
bool ReadEnergyWhile(void* k, float& value, const std::function<bool()>& current);
bool WriteActiveWhile(void* k, bool value, const std::function<bool()>& current, bool* committed = nullptr);
bool WriteChargingWhile(void* k, bool value, const std::function<bool()>& current);
bool WriteEnergyWhile(void* k, float value, const std::function<bool()>& current);
bool RunUpdWhile(void* k, bool skipFace, const std::function<bool()>& current, bool* dispatched);

// Its E-press as `player` makes it: actionOptionIndex(player, action), action 8 the on/off. The verbs
// read no player, so a null one runs too. False when the call did not resolve or its body faulted.
bool RunActionOptionIndex(void* k, void* player, uint8_t action);
bool RunActionOptionIndexWhile(void* k, void* player, uint8_t action,
                              const std::function<bool()>& current, bool* dispatched);

// The haunting's verb (kerfusPossessor_C calls it on every Kerfus): possess(at) turns it on, drops its
// task and drives it to `at`, where its own arrival test turns it off again.
bool RunPossess(void* k, const FVector& at);

}  // namespace ue_wrap::kerfus
