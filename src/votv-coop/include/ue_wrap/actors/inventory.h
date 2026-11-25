// ue_wrap/actors/inventory.h -- reads and writes the player save-slot inventory.
// It owns the Fstruct_save and Fstruct_equipment walks; offsets and aligned array
// strides are kept in the implementation. coop/inventory_wire handles POD
// serialization, while coop/player_inventory_sync owns transport and persistence.
// Inventory records are data, with nested value arrays and portable names.

#pragma once

#include "ue_wrap/actors/save_record.h"  // SaveRecord + its engine codec (the shared Fstruct_save home)

#include <cstdint>
#include <string>
#include <functional>
#include <vector>

namespace ue_wrap::inventory {

// One Fstruct_equipment record (a propDynamic id + an embedded Fstruct_save + a tag).
struct EquipRecord {
    std::wstring propName;  // Fstruct_propDynamic.name FName
    std::wstring propKey;   // Fstruct_propDynamic.key FName
    ue_wrap::save_record::SaveRecord data;  // embedded Fstruct_save (occupies a 0x100 slot)
    std::wstring tag;       // FName
};

// The full player-scoped inventory snapshot.
struct PlayerInventory {
    std::vector<ue_wrap::save_record::SaveRecord> inventory;  // carried: saveSlot.GObjStack[0]
    std::vector<EquipRecord> equipment;  // saveSlot.equipment (worn)
    std::vector<EquipRecord> hold;       // saveSlot.hold (hands)
};

// Resolve the live UsaveSlot_C (via mainGamemode_C.saveSlot). Cached + IsLive-revalidated;
// re-walks on a level transition. null if not yet resolvable. Game thread.
void* ResolveSaveSlot();

// Read what the local player carries, wears and holds into `out` (cleared first): the live
// personal store below, plus saveSlot.equipment and saveSlot.hold, all three of which gameplay
// reads and writes in place. False before the world is up or when an outer array
// header is malformed; a partial snapshot is never returned. Pure field
// reads plus FName::ToString, no UFunction dispatch. Game thread.
//
// saveSlot.inventoryData is deliberately not read: it is a projection mainGamemode::saveObjects
// writes and only the save-slot menu's repair routine reads, and on a client it never refreshes.
bool ReadAll(PlayerInventory& out);
// Persist exact-identity water/material stock before native hand reconstruction.
// Update existing scalar groups together; never repair keys or grow native arrays.
bool PersistHeldMaterialStock(void* actor, const std::function<bool()>& current = {});

// ---- THE LIVE PERSONAL STORE (read-only) ------------------------------------------------------
//
// What the player is ACTUALLY carrying right now, as the game maintains it during play:
//   mainGamemode.playerContainer -> .propInventory -> saveSlot.GObjStack[.Index]
//
// The player's inventory IS a container (Aprop_inventoryContainer_player_C derives from
// Aprop_container_C), so its contents live in the ONE global saveSlot.GObjStack addressed by
// propInventory.Index, exactly like a world container's. That slot is BAKED AT CONSTRUCTION
// rather than restored: the class's component template serializes index=0, player=true,
// customVolume=50000, so GObjStack[0] IS the player's inventory -- which is why the class's
// loadData override is an empty stub. Both live write paths land here: the world pickup
// (mainPlayer::putObjectInventory2 -> playerContainer.propInventory.addObject) and the
// container slot press (getObject -> addObject).
struct LivePersonalStore {
    int32_t slotIndex = -1;  // propInventory.Index -- measured to be 0 by construction
    std::vector<ue_wrap::save_record::SaveRecord> records;  // the live contents
};

// Read the live personal store into `out` (cleared first). False, with `out` left empty, if
// the gamemode, playerContainer, component or GObjStack slot is unresolvable, the index is
// not the baked personal slot, a header is malformed, or the component is not flagged personal. Pure field reads, no UFunction
// dispatch. Game thread.
//
// `Player == true` is the ADDRESS ASSERTION here, and the same flag is a REFUSAL in
// coop/props/container_contents_sync -- deliberately, from opposite sides of one boundary:
// that lane must never author a personal store, and this reader must never read anything
// else. ReadAll is the lane's reader and goes through this; coop/dev/live_store_readout prints
// the same records by content. The only writer is ApplyToSaveObject below, which the lane calls
// on a save object whose world does not exist yet.
bool ReadLivePersonalStore(LivePersonalStore& out);

// How many records the live personal store holds RIGHT NOW, without decoding any of them: the
// slot's TArray count alone. -1 when the save object or the slot is unresolvable.
//
// It exists because ReadLivePersonalStore interns every FName into a string, which is far too much
// to run from inside a Blueprint body -- and "how many records were in the store at the instant
// the game refreshed the quick-slot bar" is a question that can only be asked from in there. Pure
// field reads. Game thread.
int32_t LivePersonalStoreCount();

// Apply the join snapshot to the registered save object before native loadObjects materializes it.
// The GObjStack, equipment and hold arrays are staged as engine-owned buffers and published
// together; equipment and hold retain the save object's fixed slot counts. Names and classes are
// resolved while encoding. Prior nested buffers are left pinned because recursive release is less
// safe than the bounded per-join leak. False without publication if the target, world, binding,
// array headers, allocation or encoding fail during staging. A newer apply supersedes older work.
// Game thread.
bool ApplyToSaveObject(void* saveSlot, const PlayerInventory& inv);
// Same transaction, also refusing publication after a caller's join/session
// request is superseded during native encoding. The callback is synchronous.
bool ApplyToSaveObjectWhile(void* saveSlot, const PlayerInventory& inv,const std::function<bool()>& current);

// For the equipment drill's red (equip_blank_zeroed), called by its caller after the apply: every empty equipment and
// hold slot of `saveSlot` rewritten as a blank C++ record, the zeroed transform the game never sees as free. False,
// having written nothing, when `saveSlot` is dead or GMalloc is unresolved. Game thread.
bool ZeroEmptyEquipment(void* saveSlot);

// Writes the live save slot's first hold slot, the hand, for a dev drill that takes an item into the
// hand: the item's name (list_props' row), and in its record the class the player's updateHold
// spawns as the hand item and the name that item takes as it loads ("None" and a null class empty
// the hand). False when the save slot or its hold slot cannot be reached, or `classLeaf` names no
// loaded class, full encoding fails, or the hand/save changes during staging.
// The previous complete slot survives refusal. Game thread.
bool WriteHeldItem(const std::wstring& name, const wchar_t* classLeaf, const float* fuel = nullptr);
bool WriteHeldItemWhile(const std::wstring& name, const wchar_t* classLeaf,
                        const std::function<bool()>& current, const float* fuel = nullptr);

// The item name in the live save slot's first hold slot, the hand: the name the player's updateHold
// keys the game's item tables by (list_props, list_weapons). Empty when the hand holds nothing or the
// slot cannot be read. Game thread.
std::wstring ReadHeldItemName();

}  // namespace ue_wrap::inventory
