// Install a selected player mesh with a complete, compatible articulated body.
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/log.h"
#include <cstring>
#include <vector>
#include <cmath>

namespace ue_wrap::engine {
namespace {
namespace R = reflection;
CachedObjRef g_playerSkeleton, g_playerPhysics;

void* ObjectField(void* object, const wchar_t* name) {
    if (!object || !R::IsLive(object)) return nullptr;
    const int32_t offset = R::FindPropertyOffset(R::ClassOf(object), name);
    if (offset < 0) return nullptr;
    void* value = nullptr;
    std::memcpy(&value, static_cast<uint8_t*>(object) + offset, sizeof(value));
    return value && R::IsLive(value) ? value : nullptr;
}

// Read the asset's own body list, never assume that matching skeleton names
// imply the selected mesh contains every bone used by those bodies.
bool HasPhysicsBones(void* mesh, void* physics) {
    CachedObjRef physicsRef; physicsRef.Set(physics);
    if (!physicsRef.Get()) return false;
    const int32_t offset = R::FindPropertyOffset(R::ClassOf(physics), L"SkeletalBodySetups");
    if (offset < 0) return false;
    struct Array { void** data; int32_t num, max; } bodies{};
    std::memcpy(&bodies, static_cast<uint8_t*>(physics) + offset, sizeof(bodies));
    if (!bodies.data || bodies.num < 3 || bodies.num > 256 || bodies.max < bodies.num) return false;
    // A collection of unconnected collision shapes is not an articulated ragdoll.
    const int32_t jointOffset = R::FindPropertyOffset(R::ClassOf(physics), L"ConstraintSetup");
    if (jointOffset < 0) return false;
    Array joints{};
    std::memcpy(&joints, static_cast<uint8_t*>(physics) + jointOffset, sizeof(joints));
    if (!joints.data || joints.num < bodies.num - 1 || joints.num > 512 || joints.max < joints.num)
        return false;
    for (int32_t i = 0; i < joints.num; ++i)
        if (!joints.data[i] || !R::IsLive(joints.data[i])) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(mesh), L"GetBoneIndex");
    if (!fn) return false;
    CachedObjRef meshRef; meshRef.Set(mesh);
    std::vector<R::FName> names;
    names.reserve(static_cast<size_t>(bodies.num));
    bool hasPelvis = false;
    for (int32_t i = 0; i < bodies.num; ++i) {
        void* setup = bodies.data[i];
        if (!setup || !R::IsLive(setup) || !meshRef.Get()) return false;
        const int32_t boneOffset = R::FindPropertyOffset(R::ClassOf(setup), L"BoneName");
        if (boneOffset < 0) return false;
        R::FName bone{};
        std::memcpy(&bone, static_cast<uint8_t*>(setup) + boneOffset, sizeof(bone));
        if (bone.ComparisonIndex == 0 && bone.Number == 0) return false;
        for (const auto& previous : names)
            if (previous.ComparisonIndex == bone.ComparisonIndex && previous.Number == bone.Number)
                return false;
        hasPelvis = hasPelvis || R::NameEquals(bone, L"pelvis");
        names.push_back(bone);
    }
    // Attachment, position correction and velocity drive all require this body.
    if (!hasPelvis) return false;
    for (const auto& bone : names) {
        if (!meshRef.Get()) return false;
        ParamFrame frame(fn);
        int32_t index = -1;
        if (!frame.Set<R::FName>(L"BoneName", bone) || !Call(mesh, frame) ||
            !frame.GetRaw(L"ReturnValue", &index, sizeof(index)) || index < 0) return false;
    }
    return meshRef.Get() != nullptr && physicsRef.Get() != nullptr;
}
}

// Rebuild after the new mesh has evaluated its reference pose. The game's full
// Kerfur asset contains an old, centimetre-sized chest pivot on a metre-sized
// bone (UserConstraint). Correct separated anchors for this fallback only while
// PhysX copies the templates; restore them before returning so NPCs are untouched.
bool RebuildPlayerRagdollPhysics(void* target, void* physics) {
    CachedObjRef targetRef, physicsRef;
    targetRef.Set(target); physicsRef.Set(physics);
    if (!targetRef.Get() || !physicsRef.Get()) return false;
    struct SavedPivot { CachedObjRef joint; int offset; FVector original; };
    struct Restore {
        std::vector<SavedPivot> pivots;
        ~Restore() { for (auto& p : pivots) if (void* joint = p.joint.Get())
            std::memcpy(static_cast<uint8_t*>(joint) + p.offset, &p.original, sizeof(p.original)); }
    } restore;
    if (physics == g_playerPhysics.Get()) {
        const int offset = R::FindPropertyOffset(R::ClassOf(physics), L"ConstraintSetup");
        struct Array { void** data; int32_t num, max; } joints{};
        if (offset < 0) return false;
        std::memcpy(&joints, static_cast<uint8_t*>(physics) + offset, sizeof(joints));
        if (!joints.data || joints.num < 1 || joints.num > 64 || joints.max < joints.num) return false;
        const auto rotate = [](const FTransform& t, FVector p, bool inverse) {
            const float sign = inverse ? -1.f : 1.f;
            const FVector q{t.RotX * sign, t.RotY * sign, t.RotZ * sign};
            const FVector c{2.f * (q.Y*p.Z-q.Z*p.Y), 2.f * (q.Z*p.X-q.X*p.Z), 2.f * (q.X*p.Y-q.Y*p.X)};
            return FVector{p.X+t.RotW*c.X+q.Y*c.Z-q.Z*c.Y,
                p.Y+t.RotW*c.Y+q.Z*c.X-q.X*c.Z, p.Z+t.RotW*c.Z+q.X*c.Y-q.Y*c.X};
        };
        const auto toWorld = [&](const FTransform& t, const FVector& p) {
            auto v = rotate(t, {p.X*t.SX,p.Y*t.SY,p.Z*t.SZ}, false);
            return FVector{v.X+t.TX,v.Y+t.TY,v.Z+t.TZ};
        };
        for (int i = 0; i < joints.num; ++i) {
            void* joint = joints.data[i];
            if (!joint || !R::IsLive(joint)) return false;
            CachedObjRef jointRef; jointRef.Set(joint);
            const int instanceOffset = R::FindPropertyOffset(R::ClassOf(joint), L"DefaultInstance");
            void* type = R::PropertyInnerStruct(R::ClassOf(joint), L"DefaultInstance");
            if (instanceOffset < 0 || !type) return false;
            uint8_t* instance = static_cast<uint8_t*>(joint) + instanceOffset;
            FTransform world[2]; FVector pivot[2]; int pivotOffset[2];
            for (int side = 0; side < 2; ++side) {
                const int boneOffset = R::FindPropertyOffset(type, side ? L"ConstraintBone2" : L"ConstraintBone1");
                pivotOffset[side] = R::FindPropertyOffset(type, side ? L"Pos2" : L"Pos1");
                if (boneOffset < 0 || pivotOffset[side] < 0) return false;
                R::FName bone{};
                std::memcpy(&bone, instance + boneOffset, sizeof(bone));
                std::memcpy(&pivot[side], instance + pivotOffset[side], sizeof(FVector));
                ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(target), L"GetSocketTransform"));
                if (!frame.Set<R::FName>(L"InSocketName", bone) || !frame.Set<uint8_t>(L"TransformSpace", 0) ||
                    !Call(target, frame) || !frame.GetRaw(L"ReturnValue", &world[side], sizeof(FTransform)) ||
                    !targetRef.Get() || !physicsRef.Get() || !jointRef.Get()) return false;
                const float values[] = {world[side].SX,world[side].SY,world[side].SZ};
                for (float value : values) if (!std::isfinite(value) || std::fabs(value) < 0.0001f) return false;
                const float pose[] = {world[side].TX,world[side].TY,world[side].TZ,world[side].RotX,
                    world[side].RotY,world[side].RotZ,world[side].RotW,pivot[side].X,pivot[side].Y,pivot[side].Z};
                for (float value : pose) if (!std::isfinite(value)) return false;
                const float norm = world[side].RotX*world[side].RotX + world[side].RotY*world[side].RotY +
                    world[side].RotZ*world[side].RotZ + world[side].RotW*world[side].RotW;
                if (std::fabs(norm - 1.f) > 0.01f) return false;
            }
            const auto a = toWorld(world[0], pivot[0]), b = toWorld(world[1], pivot[1]);
            const FVector delta{a.X-b.X,a.Y-b.Y,a.Z-b.Z};
            const float distanceSq = delta.X*delta.X+delta.Y*delta.Y+delta.Z*delta.Z;
            if (!std::isfinite(distanceSq)) return false;
            if (distanceSq <= 1.f) continue;
            auto corrected = rotate(world[0], {b.X-world[0].TX,b.Y-world[0].TY,b.Z-world[0].TZ}, true);
            corrected.X /= world[0].SX; corrected.Y /= world[0].SY; corrected.Z /= world[0].SZ;
            SavedPivot saved; saved.joint.Set(joint); saved.offset = instanceOffset + pivotOffset[0]; saved.original = pivot[0];
            restore.pivots.push_back(saved);
            std::memcpy(instance + pivotOffset[0], &corrected, sizeof(corrected));
            UE_LOGI("ragdoll_skin: corrected separated joint anchors (gap=%.1fcm)", std::sqrt(distanceSq));
        }
    }
    ParamFrame asset(R::FindDispatchFunctionCached(R::ClassOf(target), L"SetPhysicsAsset"));
    return asset.Set<void*>(L"NewPhysicsAsset", physics) && asset.Set<bool>(L"bForceReInit", true) &&
        Call(target, asset) && targetRef.Get() && physicsRef.Get();
}

bool ConfigurePlayerRagdollMesh(void* source, void* target) {
    CachedObjRef sourceRef, targetRef;
    sourceRef.Set(source); targetRef.Set(target);
    if (!sourceRef.Get() || !targetRef.Get()) return false;
    void* mesh = puppet::GetComponentSkeletalMeshAsset(source);
    if (!mesh || !R::IsLive(mesh)) return false;
    CachedObjRef meshRef; meshRef.Set(mesh);
    // Prefer the model's own articulated physics asset. Its shapes and joints
    // were authored for its proportions, and it may use a different skeleton.
    void* physics = ObjectField(mesh, L"PhysicsAsset");
    if (!physics || !HasPhysicsBones(source, physics)) {
        // Built-in Kel, Kerfur variants and the skin converter share this rig.
        // Its default _PhysicsAsset is only pelvis/chest; _Physics has the full
        // 17-body/16-joint ragdoll. Never force it onto another skeleton.
        if (!g_playerSkeleton.Get()) g_playerSkeleton.Set(asset_load::LoadObjectByPath(
            L"/Game/meshes/kerfurAnthro/sk/kerfurOmegaV1_Skeleton.kerfurOmegaV1_Skeleton"));
        if (!g_playerPhysics.Get()) g_playerPhysics.Set(asset_load::LoadObjectByPath(
            L"/Game/meshes/kerfurAnthro/sk/kerfurOmegaV1_Physics.kerfurOmegaV1_Physics"));
        physics = g_playerPhysics.Get();
        if (!sourceRef.Get() || !targetRef.Get() || !meshRef.Get() || !g_playerSkeleton.Get() || !physics ||
            ObjectField(mesh, L"Skeleton") != g_playerSkeleton.Get() || !HasPhysicsBones(source, physics)) {
            UE_LOGW("ragdoll_skin: selected mesh lacks a compatible articulated physics rig");
            return false;
        }
    }
    CachedObjRef physicsRef; physicsRef.Set(physics);
    if (!physicsRef.Get() || !sourceRef.Get() || !targetRef.Get() || !meshRef.Get()) return false;

    const auto function = [](void* object, const wchar_t* name) {
        return R::FindDispatchFunctionCached(R::ClassOf(object), name);
    };
    // Capture appearance/transform before mutating the target. Cap material count
    // and recheck identities after calls, which may run Blueprint callbacks.
    ParamFrame transformFrame(function(source, L"K2_GetComponentToWorld"));
    FTransform transform{};
    ParamFrame countFrame(function(source, L"GetNumMaterials"));
    int32_t count = 0;
    if (!transformFrame.valid() || !Call(source, transformFrame) ||
        !transformFrame.GetRaw(L"ReturnValue", &transform, sizeof(transform)) ||
        !sourceRef.Get() || !countFrame.valid() || !Call(source, countFrame) ||
        !countFrame.GetRaw(L"ReturnValue", &count, sizeof(count)) || count < 0 || count > 64) return false;
    std::vector<CachedObjRef> materials(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!sourceRef.Get()) return false;
        ParamFrame frame(function(source, L"GetMaterial"));
        void* material = nullptr;
        if (!frame.Set<int32_t>(L"ElementIndex", i) || !Call(source, frame) ||
            !frame.GetRaw(L"ReturnValue", &material, sizeof(material))) return false;
        if (material) materials[i].Set(material);
    }
    if (!sourceRef.Get() || !targetRef.Get() || !meshRef.Get() || !physicsRef.Get()) return false;
    ParamFrame stop(function(target, L"SetSimulatePhysics"));
    if (!stop.Set<bool>(L"bSimulate", false) || !Call(target, stop) || !targetRef.Get()) return false;
    // Physics initialization captures constraint frames at the component's current
    // scale. Place the selected rig before rebuilding bodies, not after creating
    // full-size joints at the plushie's transform/scale.
    ParamFrame pose(function(target, L"K2_SetWorldTransform"));
    if (!pose.Set<FTransform>(L"NewTransform", transform) || !pose.Set<bool>(L"bSweep", false) ||
        !pose.Set<bool>(L"bTeleport", true) || !Call(target, pose) || !targetRef.Get()) return false;
    // Prevent the plushie's AnimBP from writing a six-bone pose over this rig.
    ParamFrame animation(function(target, L"SetAnimationMode"));
    if (!animation.Set<uint8_t>(L"InAnimationMode", 1) || !Call(target, animation) || !targetRef.Get() ||
        !meshRef.Get() || !SetSkeletalMesh(target, mesh) || !targetRef.Get() || !physicsRef.Get()) return false;
    ParamFrame asset(function(target, L"SetPhysicsAsset"));
    if (!asset.Set<void*>(L"NewPhysicsAsset", physics) || !asset.Set<bool>(L"bForceReInit", true) ||
        !Call(target, asset) || !targetRef.Get()) return false;
    ParamFrame kinematic(function(target, L"SetAllBodiesSimulatePhysics"));
    if (!kinematic.Set<bool>(L"bNewSimulate", false) || !Call(target, kinematic) || !targetRef.Get()) return false;
    for (int32_t i = 0; i < count; ++i) {
        if (!targetRef.Get() || !SetComponentMaterial(target, i, materials[i].Get())) return false;
    }
    // A character AnimBP expects a Pawn owner, locomotion and player-only bone
    // controls. This component belongs to a ragdoll Actor; those controls can
    // collapse/retarget its physics bones after the bodies have initialized.
    // Keep a stable reference pose for the articulated physics to take over.
    if (!SetAnimTickAlways(target) || !targetRef.Get()) return false;
    ParamFrame blend(function(target, L"SetAllBodiesPhysicsBlendWeight"));
    // Evaluate the selected mesh's reference transforms before physics takes ownership.
    return targetRef.Get() && blend.Set<float>(L"PhysicsBlendWeight", 0.f) &&
        blend.Set<bool>(L"bSkipCustomPhysicsType", false) && Call(target, blend) && targetRef.Get();
}
}
