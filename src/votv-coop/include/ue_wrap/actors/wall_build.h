#pragma once
#include <string>

namespace ue_wrap::wall_build {
inline constexpr const wchar_t* kOverlap =
    L"BndEvt__customWall_unfinished_overlap_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature";
// Replay a measured native material overlap only while the two actors still
// overlap. True means handled/no longer overlapping; false permits a retry.
bool Feed(void* wall, void* material);
// Native loadData restores wallType but does not restore the selected successor
// class on an unfinished wall. Re-establish it through the native table lookup.
bool RestoreMaterial(void* wall, const wchar_t* name);
// Stable actor_save identity, read without minting/registering a new key.
std::wstring Key(void* wall);
// Host replay of a completed wall's measured native damage body. Visual impact
// uses the wall origin; material debris/destruction remains the native graph's.
bool Damage(void* wall, float amount);
}
