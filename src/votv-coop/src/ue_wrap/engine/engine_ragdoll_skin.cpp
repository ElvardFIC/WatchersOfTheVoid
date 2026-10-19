// Install a selected player mesh with a complete, compatible articulated body.
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/log.h"
#include <cstring>
#include <vector>

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
    // Prevent the plushie's AnimBP from writing a six-bone pose over this rig.
    ParamFrame animation(function(target, L"SetAnimationMode"));
    if (!animation.Set<uint8_t>(L"InAnimationMode", 1) || !Call(target, animation) || !targetRef.Get() ||
        !meshRef.Get() || !SetSkeletalMesh(target, mesh) || !targetRef.Get() || !physicsRef.Get()) return false;
    ParamFrame asset(function(target, L"SetPhysicsAsset"));
    if (!asset.Set<void*>(L"NewPhysicsAsset", physics) || !asset.Set<bool>(L"bForceReInit", true) ||
        !Call(target, asset) || !targetRef.Get()) return false;
    ParamFrame kinematic(function(target, L"SetAllBodiesSimulatePhysics"));
    if (!kinematic.Set<bool>(L"bNewSimulate", false) || !Call(target, kinematic) || !targetRef.Get()) return false;
    ParamFrame pose(function(target, L"K2_SetWorldTransform"));
    if (!pose.Set<FTransform>(L"NewTransform", transform) || !pose.Set<bool>(L"bSweep", false) ||
        !pose.Set<bool>(L"bTeleport", true) || !Call(target, pose) || !targetRef.Get()) return false;
    for (int32_t i = 0; i < count; ++i) {
        if (!targetRef.Get() || !SetComponentMaterial(target, i, materials[i].Get())) return false;
    }
    // Keep the selected rig's animation graph for bones without physics bodies
    // (hands, toes, face). SingleNode with no animation leaves those in reference
    // pose even when pelvis/limbs have valid physics.
    if (void* animClass = ObjectField(source, L"AnimClass")) {
        if (!SetAnimClass(target, animClass) || !targetRef.Get()) return false;
        SetAnimTickAlways(target);
    }
    ParamFrame blend(function(target, L"SetAllBodiesPhysicsBlendWeight"));
    // Let the new AnimBP supply a real pose before physics takes ownership.
    return targetRef.Get() && blend.Set<float>(L"PhysicsBlendWeight", 0.f) &&
        blend.Set<bool>(L"bSkipCustomPhysicsType", false) && Call(target, blend) && targetRef.Get();
}
}
