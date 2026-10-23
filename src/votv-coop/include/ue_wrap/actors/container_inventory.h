// ue_wrap/actors/container_inventory.h -- a world container (prop_container_C) and the propInventory_C
// component that holds its contents: the reflected fields that tie the two together, the slot of the
// save slot's shared GObjStack the contents live in, and the game's own verbs that re-derive what the
// container shows from them. Engine-wrapper layer (principle 7): no wire and no arbitration, which
// coop/props/container_contents_sync owns. Reflected offsets belong to the actual
// class incarnation and refresh after replacement. Game thread.
#pragma once

#include "ue_wrap/actors/save_record.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ue_wrap::container_inventory {

// Whether `actor` is a container, and `obj` a propInventory component. A class not loaded yet, or
// still loading, answers false and is asked for again at the next call.
bool IsContainer(void* actor);
bool IsInventory(void* obj);

// The propInventory component of a container actor, live, or null.
void* InventoryOf(void* container);
// The container that owns a propInventory component, live, or null.
void* OwnerOf(void* inventory);

// Fail closed: true only for a WORLD container's own inventory. Player true is a personal inventory
// (mainPlayer and ui_playerInventory share the same global GObjStack, and GObjStack[0] is the local
// player's inventory by construction, baked by the player container's component template); an
// unresolvable flag reads as personal too. ue_wrap::inventory::ReadLivePersonalStore asserts the same
// flag in the other direction.
bool IsWorldInventory(void* inventory);
// The actual Player bit, shared with the personal-store reader. False when its
// class/schema is unavailable; neighboring packed flags never imply personal.
bool ReadPersonalFlag(void* inventory, bool& personal);

// The live TArray<Fstruct_save> holding this inventory's contents inside saveSlot.GObjStack (the
// struct_mObject element's single field, at +0), or null when the save slot, the offsets or the index
// do not resolve. An index of -1 is a component never initialised, with nothing to read or write.
uint8_t* ContentsSlot(void* inventory);
// Network world-container address: index zero is reserved for the personal
// store even when a broken world component has Player=false.
uint8_t* WorldContentsSlot(void* inventory);

// The records the slot holds, read whole. False when the slot does not resolve, or when it holds more
// than `maxRecords` (nothing is read then: a cut read is a silent lie); `countOut`, when given, is the
// slot's record count, or -1 when it does not resolve or its header is malformed.
// A malformed header is never reported as an empty container; out is cleared.
bool ReadContents(void* inventory, std::vector<save_record::SaveRecord>& out, size_t maxRecords,
                  int32_t* countOut = nullptr);

// Return one locally removed row to this exact world store. Stage checked data
// before swapping its header; preserve current other rows and nested indices.
// An existing same-key/class row is success without overwriting its newer state.
// True means committed/already present, including reentry during old-buffer cleanup.
bool RefundRecordWhile(void* inventory, const save_record::SaveRecord& record,
                       const std::function<bool()>& current);

// Whether a record's class is a container. A container record's ints[0][0] is its own GObjStack index,
// which prop_container::loadData reads unguarded and propInventory::init binds when it is >= 0; -1 is
// the class default's "none", the value the init guard is written against.
bool IsContainerRecord(const save_record::SaveRecord& r);
bool CarriesInventoryIndex(const save_record::SaveRecord& r);
// Set ints[0][0] to -1, keeping every other entry. Never by clearing ints: an empty array reads as
// index 0, which the guard passes, and the container would bind GObjStack[0].
void ClearInventoryIndex(save_record::SaveRecord& r);

// Re-derive what the container shows -- currVol, Mass, the display names -- through the game's own
// verbs (updateVolumesAndMass on the container, recalculateNames on the inventory), never by raw
// writes. Either argument may be null. A verb that does not resolve is said once.
// Rebuild names first: the native volume/mass calculation reads that name list.
void RederiveShownState(void* container, void* inventory);
void RederiveShownStateWhile(void* container, void* inventory, const std::function<bool()>& current);

// The inventory's currVol. False when the field does not resolve.
bool CurrentVolume(void* inventory, float& out);

}  // namespace ue_wrap::container_inventory
