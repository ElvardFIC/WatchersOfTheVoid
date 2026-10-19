#include "ue_wrap/actors/sleep_display.h"
#include "ue_wrap/actors/sleep.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/trace.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/world/world_singleton.h"
#include <map>
#include <cstring>
#include <chrono>

namespace ue_wrap::sleep_display {
namespace {
namespace R = reflection;
struct Display {
    CachedObjRef pawn, mesh, asset;
    std::chrono::steady_clock::time_point settleUntil{};
};
std::map<void*, Display> g_displays;
void* Fn(void* object, const wchar_t* name) { return R::FindDispatchFunctionCached(R::ClassOf(object), name); }
void Show(void* pawn, bool visible) {
    engine::SetSceneComponentVisibility(puppet::GetNativeBodyMeshComponent(pawn), visible, false);
    engine::SetSceneComponentVisibility(puppet::GetMeshPlayerVisibleComponent(pawn), visible, false);
}
bool Position(void* mesh, const FVector& anchor, float yaw) {
    ParamFrame rotation(Fn(mesh, L"K2_SetWorldRotation"));
    if (!rotation.Set<FRotator>(L"NewRotation", {90.f, yaw - 90.f, 0.f}) ||
        !rotation.Set<bool>(L"bTeleport", true) || !Call(mesh, rotation)) return false;
    FVector pelvis{};
    if (!engine::GetBoneWorldLocationByName(mesh, L"pelvis", pelvis)) return false;
    const FVector origin = engine::GetComponentLocation(mesh);
    ParamFrame location(Fn(mesh, L"K2_SetWorldLocation"));
    return location.Set<FVector>(L"NewLocation", {origin.X + anchor.X - pelvis.X,
        origin.Y + anchor.Y - pelvis.Y, origin.Z + anchor.Z - pelvis.Z}) &&
        location.Set<bool>(L"bTeleport", true) && Call(mesh, location);
}
}
bool LocalAnchor(FVector& position, float& yaw) {
    if (!sleep::EnsureResolved() || !sleep::IsSleeping()) return false;
    void* gm = world_singleton::Gamemode();
    const int off = gm ? R::FindPropertyOffset(R::ClassOf(gm), L"bed") : -1;
    void* bed = nullptr;
    if (off >= 0) std::memcpy(&bed, static_cast<uint8_t*>(gm) + off, sizeof(bed));
    if (!bed || !R::IsLive(bed)) {
        // Native sleep can also start without a bed (ground/exhaustion).
        const int playerOff = gm ? R::FindPropertyOffset(R::ClassOf(gm), L"mainPlayer") : -1;
        void* player = nullptr;
        if (playerOff >= 0) std::memcpy(&player, static_cast<uint8_t*>(gm) + playerOff, sizeof(player));
        FRotator rotation{};
        if (!player || !R::IsLive(player) || !engine::TryGetActorLocation(player, position) ||
            !engine::TryGetActorRotation(player, rotation)) return false;
        position.Z -= engine::GetActorCharacterHalfHeight(player);
        FVector floor{};
        if (trace::FindFloor(gm, {position.X, position.Y, position.Z + 50.f},
                            {position.X, position.Y, position.Z - 100.f}, floor)) position = floor;
        position.Z += 12.f; yaw = NormalizeAxis(rotation.Yaw);
        return true;
    }
    FRotator rotation{};
    ParamFrame bounds(Fn(bed, L"GetActorBounds"));
    FVector origin{}, extent{};
    if (!engine::TryGetActorRotation(bed, rotation) || !bounds.Set<bool>(L"bOnlyCollidingComponents", false) ||
        !Call(bed, bounds) || !bounds.GetRaw(L"Origin", &origin, sizeof(origin)) ||
        !bounds.GetRaw(L"BoxExtent", &extent, sizeof(extent))) return false;
    // The mattress is the selected bed actor (including unfolded sleeping bags),
    // not a world-global base-bed lookup. Moving/rotating it moves this anchor.
    position = {origin.X, origin.Y, origin.Z + extent.Z};
    // A bed's bounds can include a tall headboard. Trace through its centre
    // to use the mattress surface rather than the headboard's maximum Z.
    FVector surface{};
    if (trace::FindFloor(gm, {origin.X, origin.Y, origin.Z + extent.Z + 5.f},
                        {origin.X, origin.Y, origin.Z - extent.Z - 5.f}, surface) &&
        surface.Z >= origin.Z - extent.Z - 1.f) position = surface;
    position.Z += 12.f;
    yaw = NormalizeAxis(rotation.Yaw);
    return std::isfinite(position.X) && std::isfinite(position.Y) && std::isfinite(position.Z);
}
void Remove(void* pawn) {
    auto it = g_displays.find(pawn);
    if (it == g_displays.end()) return;
    if (it->second.pawn.Get()) Show(pawn, true);
    if (void* mesh = it->second.mesh.Get()) {
        ParamFrame frame(Fn(mesh, L"K2_DestroyComponent"));
        Call(mesh, frame);
    }
    g_displays.erase(it);
}
void* Mesh(void* pawn) {
    const auto it = g_displays.find(pawn);
    return it != g_displays.end() && it->second.pawn.Get() ? it->second.mesh.Get() : nullptr;
}
bool Drive(void* pawn, bool sleeping, const FVector& position, float yaw) {
    // Reap destroyed/world-swapped pawns even if their owner missed teardown.
    for (auto it = g_displays.begin(); it != g_displays.end();) {
        if (!it->second.pawn.Get()) it = g_displays.erase(it); else ++it;
    }
    if (!sleeping) { Remove(pawn); return false; }
    if (!pawn || !R::IsLive(pawn)) return false;
    void* source = puppet::GetMeshPlayerVisibleComponent(pawn);
    void* asset = puppet::GetComponentSkeletalMeshAsset(source);
    if (!source || !asset) return false;
    auto it = g_displays.find(pawn);
    if (it != g_displays.end() && (!it->second.mesh.Get() || it->second.asset.Get() != asset)) {
        Remove(pawn); it = g_displays.end();
    }
    if (it == g_displays.end()) {
        void* cls = R::FindClass(L"PoseableMeshComponent");
        ParamFrame add(Fn(pawn, L"AddComponentByClass"));
        void* mesh = nullptr;
        if (!cls || !add.Set<void*>(L"Class", cls) || !add.Set<bool>(L"bManualAttachment", false) ||
            !add.Set<FTransform>(L"RelativeTransform", FTransform{}) ||
            !add.Set<bool>(L"bDeferredFinish", false) || !Call(pawn, add) ||
            !add.GetRaw(L"ReturnValue", &mesh, sizeof(mesh)) || !mesh) return false;
        Display display; display.pawn.Set(pawn); display.mesh.Set(mesh); display.asset.Set(asset);
        display.settleUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        it = g_displays.emplace(pawn, display).first;
        if (!engine::SetSkeletalMesh(mesh, asset)) { Remove(pawn); return false; }
        FTransform sourceTransform{};
        ParamFrame readTransform(Fn(source, L"K2_GetComponentToWorld"));
        ParamFrame setTransform(Fn(mesh, L"K2_SetWorldTransform"));
        if (!Call(source, readTransform) || !readTransform.GetRaw(L"ReturnValue", &sourceTransform, sizeof(sourceTransform)) ||
            !setTransform.Set<FTransform>(L"NewTransform", sourceTransform) ||
            !setTransform.Set<bool>(L"bTeleport", true) || !Call(mesh, setTransform)) { Remove(pawn); return false; }
        ParamFrame copy(Fn(mesh, L"CopyPoseFromSkeletalComponent"));
        if (!copy.Set<void*>(L"InComponentToCopy", source) || !Call(mesh, copy)) { Remove(pawn); return false; }
        int32_t count = 0;
        ParamFrame materials(Fn(source, L"GetNumMaterials"));
        if (!Call(source, materials) || !materials.GetRaw(L"ReturnValue", &count, sizeof(count)) || count < 0 || count > 64) {
            Remove(pawn); return false;
        }
        for (int32_t slot = 0; slot < count; ++slot) {
            ParamFrame get(Fn(source, L"GetMaterial")); void* material = nullptr;
            if (!get.Set<int32_t>(L"ElementIndex", slot) || !Call(source, get) ||
                !get.GetRaw(L"ReturnValue", &material, sizeof(material)) ||
                !engine::SetComponentMaterial(mesh, slot, material)) { Remove(pawn); return false; }
        }
        // Display only: it must neither hit the mattress nor produce overlaps.
        ParamFrame collision(Fn(mesh, L"SetCollisionEnabled"));
        collision.Set<uint8_t>(L"NewType", 0); Call(mesh, collision);
    }
    void* mesh = it->second.mesh.Get();
    if (mesh && std::chrono::steady_clock::now() < it->second.settleUntil) {
        // Let a just-ended sit/locomotion animation settle into the rest pose.
        // Copying only the first frame freezes the old chair pose in the bed.
        ParamFrame copy(Fn(mesh, L"CopyPoseFromSkeletalComponent"));
        if (!copy.Set<void*>(L"InComponentToCopy", source) || !Call(mesh, copy)) return false;
    }
    if (!mesh || !Position(mesh, position, yaw)) return false;
    engine::SetSceneComponentVisibility(mesh, true, false);
    Show(pawn, false);
    return true;
}
bool DriveArticulated(void* pawn, const std::vector<engine::ArticulatedBone>& bones) {
    if (bones.empty()) return false;
    const auto& anchor = bones.front().world;
    if (!Drive(pawn, true, {anchor.TX, anchor.TY, anchor.TZ}, 0.f)) return false;
    void* mesh = g_displays[pawn].mesh.Get();
    if (!mesh) return false;
    // Bone indices are ordered parent before child in the authored skeleton.
    // Set in world space each frame, so the local physics fallback cannot bend
    // a streamed limb differently after a collision on this machine.
    for (const auto& bone : bones) {
        ParamFrame name(Fn(mesh, L"GetBoneName")); R::FName boneName{};
        if (!name.Set<int32_t>(L"BoneIndex", bone.index) || !Call(mesh, name) ||
            !name.GetRaw(L"ReturnValue", &boneName, sizeof(boneName))) return false;
        if (boneName.ComparisonIndex == 0) return false;
        FTransform transform = bone.world;
        ParamFrame previous(Fn(mesh, L"GetBoneTransformByName")); FTransform old{};
        if (previous.Set<R::FName>(L"BoneName", boneName) && previous.Set<uint8_t>(L"BoneSpace", 0) &&
            Call(mesh, previous) && previous.GetRaw(L"ReturnValue", &old, sizeof(old))) {
            transform.SX = old.SX; transform.SY = old.SY; transform.SZ = old.SZ;
        }
        ParamFrame set(Fn(mesh, L"SetBoneTransformByName"));
        if (!set.Set<R::FName>(L"BoneName", boneName) || !set.Set<FTransform>(L"InTransform", transform) ||
            !set.Set<uint8_t>(L"BoneSpace", 0) || !Call(mesh, set)) return false;
    }
    return true;
}
}
