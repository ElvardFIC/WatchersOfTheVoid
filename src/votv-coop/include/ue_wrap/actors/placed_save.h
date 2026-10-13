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
bool SetKey(void* actor, const std::wstring& key);
// Copies only this family's measured fields; rejects malformed geometry/identity.
bool Normalize(const save_record::SaveRecord& input, save_record::SaveRecord& output);
void* Spawn(const save_record::SaveRecord& record);
bool Apply(void* actor, const save_record::SaveRecord& record);
// Resolve dependencies after loadData. False means a linked actor is still absent.
bool Link(void* actor, const save_record::SaveRecord& record);
}
