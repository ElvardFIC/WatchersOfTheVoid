// ue_wrap/engine/engine_hurt_flash.cpp -- see ue_wrap/engine/engine_hurt_flash.h.

#include "ue_wrap/engine/engine_hurt_flash.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace ue_wrap::engine {
namespace {

namespace P = profile;
namespace R = reflection;

// The damage body pulse: the cached gore hurt material and the primitive component's
// material UFunctions (count, get, set).
ue_wrap::CachedObjRef g_hurtMat;  // a slot-validated cache
void* g_getNumMatFn = nullptr, *g_getMatFn = nullptr, *g_setMatFn = nullptr;

void* ResolveHurtMat() {
    if (g_hurtMat.Alive()) return g_hurtMat.Raw();
    g_hurtMat.Set(ResolveMaterialByName(P::name::PlayerHurtFlashMaterialName));
    return g_hurtMat.Raw();
}
void ResolveMatFns() {
    if (g_getNumMatFn && g_getMatFn && g_setMatFn) return;
    void* c = R::FindClass(L"PrimitiveComponent");
    if (!c) return;
    if (!g_getNumMatFn) g_getNumMatFn = R::FindFunction(c, L"GetNumMaterials");
    if (!g_getMatFn)    g_getMatFn    = R::FindFunction(c, L"GetMaterial");
    if (!g_setMatFn)    g_setMatFn    = R::FindFunction(c, L"SetMaterial");
}

// A material instance made at run time (MaterialInstanceDynamic): nothing but the override that
// holds it keeps it alive, unlike an asset's material, which its package holds.
bool IsRuntimeMaterial(void* m) {
    static void* sMid = nullptr;
    if (!sMid) sMid = R::FindClass(L"MaterialInstanceDynamic");
    void* cls = m ? R::ClassOf(m) : nullptr;
    return sMid && cls && R::IsDescendantOfAny(cls, &sMid, 1);
}

// Swap every material slot on one component to `mat`, appending each (component, index,
// original) into `saved`, caller-owned, for the restore.
void SwapComponentMaterials(void* comp, void* mat, std::vector<SavedMaterial>& saved) {
    if (!comp || !R::IsLive(comp) || !mat) return;
    ResolveMatFns();
    if (!g_getNumMatFn || !g_getMatFn || !g_setMatFn) return;
    int32_t num = 0;
    { ParamFrame f(g_getNumMatFn); if (Call(comp, f)) num = f.Get<int32_t>(L"ReturnValue"); }
    for (int32_t i = 0; i < num && i < 16; ++i) {
        void* orig = nullptr;
        { ParamFrame f(g_getMatFn); f.Set<int32_t>(L"ElementIndex", i); if (Call(comp, f)) orig = f.Get<void*>(L"ReturnValue"); }
        SavedMaterial sm;
        sm.component.Set(comp);  // comp validated at entry; orig fresh from GetMaterial
        sm.index = i;
        sm.original.Set(orig);
        // Held until the entry goes (types.h); a material both meshes share (a skin's face instance) is pinned once.
        if (IsRuntimeMaterial(orig) &&
            std::none_of(saved.begin(), saved.end(), [orig](const SavedMaterial& e) { return e.pin.Raw() == orig; }))
            sm.pin.Pin(orig);
        saved.push_back(std::move(sm));
        { ParamFrame f(g_setMatFn); f.Set<int32_t>(L"ElementIndex", i); f.Set<void*>(L"Material", mat); Call(comp, f); }
    }
}

// The puppet's two visible body meshes (the native character mesh and the player-visible
// mesh): both render, and both carry the skin.
void* PuppetVisibleMesh(void* puppet, size_t off) {
    if (!puppet || !R::IsLive(puppet)) return nullptr;
    void* c = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(puppet) + off);
    return (c && R::IsLive(c)) ? c : nullptr;
}

}  // namespace

void* ResolveMaterialByName(const wchar_t* name) {
    if (!name) return nullptr;
    // A material instance constant first (most pak materials), then a base material, then an
    // any-class fallback.
    void* m = R::FindObject(name, P::name::MaterialInstanceConstantClassName);
    if (m && R::IsLive(m)) return m;
    m = R::FindObject(name, L"Material");
    if (m && R::IsLive(m)) return m;
    m = R::FindObject(name, nullptr);
    return (m && R::IsLive(m)) ? m : nullptr;
}

bool ApplyHurtFlashMaterial(void* puppet, std::vector<SavedMaterial>& saved) {
    // Self-protecting: a non-empty `saved` means a flash is already applied, and a second apply
    // without a restore in between would re-read the hurt material as the original and stick it
    // permanently. The caller is edge-gated, but the guard makes the API safe for any caller.
    if (!saved.empty()) return false;
    void* hurtMat = ResolveHurtMat();
    if (!hurtMat || !puppet || !R::IsLive(puppet)) return false;
    SwapComponentMaterials(PuppetVisibleMesh(puppet, P::off::ACharacter_Mesh), hurtMat, saved);
    SwapComponentMaterials(PuppetVisibleMesh(puppet, P::off::AmainPlayer_mesh_playerVisible), hurtMat, saved);
    return !saved.empty();
}

void* PuppetBodyMaterial(void* puppet, bool playerVisible, int32_t slot) {
    void* comp = PuppetVisibleMesh(puppet, playerVisible ? P::off::AmainPlayer_mesh_playerVisible
                                                         : P::off::ACharacter_Mesh);
    ResolveMatFns();
    if (!comp || !g_getMatFn) return nullptr;
    ParamFrame f(g_getMatFn);
    f.Set<int32_t>(L"ElementIndex", slot);
    if (!Call(comp, f)) return nullptr;
    void* m = f.Get<void*>(L"ReturnValue");
    return (m && R::IsLive(m)) ? m : nullptr;
}

bool RestoreHurtFlashMaterial(void* /*puppet*/, std::vector<SavedMaterial>& saved) {
    ResolveMatFns();
    if (g_setMatFn) {
        for (const auto& s : saved) {
            void* comp = s.component.Get();  // slot-validated
            if (!comp) continue;  // component GC'd
            // A run-time original is pinned for the window (SavedMaterial::pin), so it is here; an
            // asset's material, if gone, reverts the slot to the mesh's own with null.
            void* orig = s.original.Get();
            ParamFrame f(g_setMatFn);
            f.Set<int32_t>(L"ElementIndex", s.index);
            f.Set<void*>(L"Material", orig);
            Call(comp, f);
        }
    }
    saved.clear();
    return true;
}

// Eager-resolve the hurt material and the material UFunctions, so the first damage flash
// does no object-array name walks. Called once per puppet spawn, cached forever on success;
// if the material is not loaded yet the warm-up is a no-op and the first flash resolves
// lazily, but it is resident base content, so steady-state flashes never re-walk the array.
void WarmupHurtFlashCache() {
    ResolveHurtMat();
    ResolveMatFns();
}

}  // namespace ue_wrap::engine
