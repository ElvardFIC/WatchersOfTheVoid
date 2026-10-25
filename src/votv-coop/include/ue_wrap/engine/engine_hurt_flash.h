// ue_wrap/engine/engine_hurt_flash.h -- the puppet's hurt flash: its two body meshes' materials swapped to a gore
// skin and put back. Engine-wrapper layer (principle 7): each call marshals reflected UFunction calls on the
// primitive components, with no gameplay, network or coop state. Game thread. Implementation:
// src/ue_wrap/engine/engine_hurt_flash.cpp.

#pragma once

#include "ue_wrap/core/types.h"

#include <cstdint>
#include <vector>

namespace ue_wrap::engine {

// SavedMaterial (types.h) is (component, slot, original, and a pin on a run-time original); the
// puppet renders two body meshes, the native ACharacter slot and mesh_playerVisible, so the saved
// set spans both. The caller owns the vector, and clearing it releases the pins.
using ue_wrap::SavedMaterial;

// Swap both body meshes' materials to the hurt-flash material (a skeletal gore skin, so it renders
// red), caching the originals into `saved`; Restore puts them back. False, with no effect, when
// anything does not resolve. Game thread.
bool ApplyHurtFlashMaterial(void* puppet, std::vector<SavedMaterial>& saved);

bool RestoreHurtFlashMaterial(void* puppet, std::vector<SavedMaterial>& saved);

// The material in `slot` of one of the puppet's two visible body meshes (the native character mesh when
// `playerVisible` is false, mesh_playerVisible when true) -- the override when one is set, else the mesh's own --
// or null. Game thread.
void* PuppetBodyMaterial(void* puppet, bool playerVisible, int32_t slot);

// Eager-resolve the hurt material and the material UFunctions once per puppet spawn. Game thread.
void WarmupHurtFlashCache();

// A loaded UMaterialInterface by object name; nullptr if not loaded. Game thread.
void* ResolveMaterialByName(const wchar_t* name);

}  // namespace ue_wrap::engine
