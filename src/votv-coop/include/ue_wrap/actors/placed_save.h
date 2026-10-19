#pragma once
#include "ue_wrap/actors/save_record.h"

namespace ue_wrap::placed_save {
enum class Kind { None, Faucet, Hose, Plant, WallOutline, Wall };
inline bool IsWall(Kind kind) { return kind == Kind::WallOutline || kind == Kind::Wall; }
Kind Type(void* actor);
Kind ClassKind(const std::wstring& name);
// Only the native placement factories; world generation and save loading are excluded.
bool PlacementCaller(Kind kind, void* function);
bool Ready(void* actor);
bool Capture(void* actor, save_record::SaveRecord& record);
// Read the existing native identity without invoking getData/getKey or minting one.
std::wstring ExistingKey(void* actor);
bool CanHarvest(void* actor);
// Native plant action 4. The caller must refuse the collector's pocket verb
// during this call when loot should remain in the world.
bool Harvest(void* actor, void* collector);
enum class CareKind : uint8_t { Water, Fertilizer };
struct CareUse { void* plant=nullptr; std::wstring sourceKey; float amount=0; CareKind kind=CareKind::Water; };
// Measure the exact LMB path before it runs, and verify its native resource cost
// after it. No inventory writes or gameplay calls occur in these reads.
bool ReadCareUse(void* tool, void* player, CareKind kind, CareUse& use);
bool CareWasSpent(void* tool, CareKind kind);
bool ApplyCare(void* plant, CareKind kind, float amount);
// Native action 5 changes only faucet active/effects and its click sound.
bool ToggleFaucet(void* faucet);
// Native detach plus saved-key cleanup; an expected endpoint prevents an old
// grab request from disconnecting a newly attached sprinkler.
bool DetachSprinkler(void* hose, const std::wstring& expectedEndpoint);
bool SetKey(void* actor, const std::wstring& key);
// Copies only this family's measured fields; rejects malformed geometry/identity.
bool Normalize(const save_record::SaveRecord& input, save_record::SaveRecord& output);
void* Spawn(const save_record::SaveRecord& record);
bool Apply(void* actor, const save_record::SaveRecord& record);
// Resolve dependencies after loadData. False means a linked actor is still absent.
bool Link(void* actor, const save_record::SaveRecord& record);
// Validate hose endpoints without changing either actor. Existing endpoint
// ownership must be empty or belong to this hose.
bool HoseLinksAvailable(void* actor, const save_record::SaveRecord& record);
}
