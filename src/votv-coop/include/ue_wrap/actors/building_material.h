#pragma once
#include <cstdint>
namespace ue_wrap::save_record { struct SaveRecord; }
namespace ue_wrap::building_material {
bool IsBucket(void* actor);
bool IsTrowel(void* actor);
bool IsConcreteBucket(void* actor);
// Disposable remote-hand display only; never consumes or dries real stock.
bool SetRemoteConcreteBucketDisplay(void* actor, int32_t units, bool dry);
bool ReadConcrete(void* actor, int32_t& units);
// Refresh a COPY of this exact trowel's held save record before profile hashing.
// Reads live stock; never changes the actor, inventory or remote display.
bool RefreshTrowelRecord(void* actor, save_record::SaveRecord& record);
// Refresh native saved stock in a COPY of an exact supported held material record (trowel,
// water/concrete bucket, cement bag or sponge). Unsupported classes are unchanged successes;
// supported classes with missing/mismatched live identity return false without modifying record.
bool RefreshHeldMaterialRecord(void* actor, save_record::SaveRecord& record);
bool SetRemoteConcrete(void* actor, int32_t units);
enum class StockKind : uint8_t { None, ConcreteBucket, CementBag, Trowel, Sponge };
struct Stock { StockKind kind=StockKind::None; int32_t units=0; bool dry=false; float wetness=0; };
// Native saved amounts; dryTimer is deliberately not an amount and is not saved by the game.
bool ReadStock(void* actor, Stock& state);
struct Water { float height=0; int32_t soap=0; bool alcohol=false; };
bool ReadWater(void* actor, Water& state);
// loadData restores height but omits empty; repair it BEFORE native upd reads it.
bool RepairBucketEmpty(void* actor);
// Repaint a passive replica without running water loss, fermentation or projectile spawns.
bool RepaintBucket(void* actor);
// Hand display only: soap is presence, not an authoritative consumable amount.
bool SetRemoteBucketDisplay(void* actor,float height,bool soap,bool alcohol);
// Preserve the open container's own tick while the bucket's water simulation is parked.
bool TickBucketContents(void* actor,float seconds);
}
