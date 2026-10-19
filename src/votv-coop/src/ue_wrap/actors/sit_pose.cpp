// ue_wrap/actors/sit_pose.cpp -- see ue_wrap/actors/sit_pose.h.

#include "ue_wrap/actors/sit_pose.h"

#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflected_offset.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_component.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ue_wrap::sit_pose {
namespace {

namespace P = profile;
namespace R = reflection;

template <class T>
T ReadAt(void* base, ptrdiff_t off) {
    if (!base || off < 0) return T{};
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(base) + off);
}
template <class T>
void WriteAt(void* base, ptrdiff_t off, T value) {
    if (!base || off < 0) return;
    *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(base) + off) = value;
}

void* LiveAnim(void* skeletalMeshComponent) {
    if (!skeletalMeshComponent || !R::IsLive(skeletalMeshComponent)) return nullptr;
    void* anim = ReadAt<void*>(skeletalMeshComponent,
                               static_cast<ptrdiff_t>(P::off::USkeletalMesh_AnimScriptInstance));
    return (anim && R::IsLive(anim)) ? anim : nullptr;
}

bool IsKerfurAnimBP(void* anim) {
    if (!anim || !R::IsLive(anim)) return false;
    static void* kerfurAnimClass = nullptr;
    if (!kerfurAnimClass) kerfurAnimClass = R::FindClass(P::name::AnimBPKerfurRegularClass);
    if (!kerfurAnimClass) return false;
    void* cls = R::ClassOf(anim);
    if (!cls) return false;
    if (cls == kerfurAnimClass) return true;
    void* bases[1] = { kerfurAnimClass };
    return R::IsDescendantOfAny(cls, bases, 1);
}

void DriveAnim(void* anim, bool sitting) {
    if (!IsKerfurAnimBP(anim)) return;
    const int32_t sitOff = reflected_offset::AnimBP_kerfur_isOnAtv();
    const int32_t ikOff  = reflected_offset::AnimBP_kerfur_useLegIK();
    if (sitOff >= 0) WriteAt<bool>(anim, sitOff, sitting);
    if (ikOff >= 0)  WriteAt<bool>(anim, ikOff, !sitting);
}

}  // namespace

void DriveSitting(void* pawn, bool sitting) {
    if (!pawn || !R::IsLive(pawn)) return;
    if (void* vis = puppet::GetMeshPlayerVisibleComponent(pawn))
        DriveAnim(LiveAnim(vis), sitting);
    if (void* body = puppet::GetNativeBodyMeshComponent(pawn))
        DriveAnim(LiveAnim(body), sitting);
}

bool AnimIsOnAtv(void* anim) {
    if (!IsKerfurAnimBP(anim)) return false;
    const int32_t sitOff = reflected_offset::AnimBP_kerfur_isOnAtv();
    if (sitOff < 0) return false;
    return ReadAt<bool>(anim, sitOff);
}

bool ReadIsOnAtv(void* pawn) {
    if (!pawn || !R::IsLive(pawn)) return false;
    if (void* vis = puppet::GetMeshPlayerVisibleComponent(pawn)) {
        if (AnimIsOnAtv(LiveAnim(vis))) return true;
    }
    if (void* body = puppet::GetNativeBodyMeshComponent(pawn)) {
        if (AnimIsOnAtv(LiveAnim(body))) return true;
    }
    return false;
}

bool IsSittingOnSomething(void* pawn) {
    if (!pawn || !R::IsLive(pawn)) return false;
    const int32_t off = reflected_offset::MainPlayer_sittingOn();
    if (off < 0) return false;
    void* seat = ReadAt<void*>(pawn, off);
    return seat && R::IsLive(seat);
}

void* ComponentAt(void* atv, int32_t off) {
    if (!atv || !R::IsLive(atv) || off < 0) return nullptr;
    void* comp = ReadAt<void*>(atv, off);
    return (comp && R::IsLive(comp)) ? comp : nullptr;
}

void* AtvDriverSeat(void* atv) {
    return ComponentAt(atv, reflected_offset::Atv_sitarrow());
}

float ModelHalfLength(void* pawn) {
    // The capsule is the body's length. Half of it is the first part of the chair mesh drop.
    const float half = engine::GetActorCharacterHalfHeight(pawn);
    // 88 is the engine's ordinary capsule half-height, used when this pawn has not grown one yet.
    return (half > 10.f) ? half : 88.f;
}

float ModelHead(void* pawn) {
    // One head of an eight-head figure. The ATV sit is finished at one and a half of these up
    // and three forward. The chair mesh is half the body, then two and a half heads down and
    // four heads forward.
    const float full = ModelHalfLength(pawn) * 2.f;
    return full > 0.f ? full / 8.f : 0.f;
}

namespace {

struct MeshBase {
    // Cached across frames. Alive/Is read the object array only, so a purged mesh does not
    // fault and a new component at the same address does not inherit this base.
    CachedObjRef comp;
    FVector base{};
    bool held = false;
};

struct PawnShift {
    CachedObjRef pawn;
    MeshBase a{};
    MeshBase b{};
};

PawnShift g_shift[16];

void* AttachParent(void* comp) {
    if (!comp) return nullptr;
    void* p = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(comp) + P::off::USceneComponent_AttachParent);
    return (p && R::IsLive(p)) ? p : nullptr;
}

bool SetRelative(void* comp, const FVector& loc) {
    static void* fn = nullptr;
    if (!fn) {
        if (void* sc = R::FindClass(P::name::SceneComponentClass))
            fn = R::FindFunction(sc, L"K2_SetRelativeLocation");
    }
    if (!fn || !comp) return false;
    ParamFrame f(fn);
    if (!f.valid()) return false;
    f.Set<FVector>(L"NewLocation", loc);
    f.Set<bool>(L"bSweep", false);
    f.Set<bool>(L"bTeleport", true);
    return Call(comp, f);
}

// Parent space, so a mesh whose parent is yawed still moves along the actor's facing and world up.
FVector ToParent(void* comp, const FVector& world) {
    void* parent = AttachParent(comp);
    FRotator rot{};
    if (!parent) return world;
    rot = engine::GetComponentWorldRotation(parent);
    const float yaw = rot.Yaw * 0.01745329252f;
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    return FVector{ c * world.X + s * world.Y, -s * world.X + c * world.Y, world.Z };
}

void Hold(MeshBase& slot, void* comp) {
    if (slot.held && slot.comp.Is(comp)) return;
    slot.comp.Set(comp);
    slot.base = slot.comp.Alive() ? engine::GetComponentRelativeLocation(comp) : FVector{};
    slot.held = slot.comp.Alive();
}

void Apply(MeshBase& slot, const FVector& worldDelta) {
    void* comp = slot.held ? slot.comp.Get() : nullptr;
    if (!comp) return;
    const FVector d = ToParent(comp, worldDelta);
    SetRelative(comp, FVector{ slot.base.X + d.X, slot.base.Y + d.Y, slot.base.Z + d.Z });
}

void Restore(MeshBase& slot) {
    if (void* comp = slot.held ? slot.comp.Get() : nullptr)
        SetRelative(comp, slot.base);
    slot = {};
}

PawnShift* FindShift(void* pawn, bool create) {
    PawnShift* free = nullptr;
    for (PawnShift& s : g_shift) {
        // Drop a dead slot before the pointer compare. A recycled address compares equal and
        // would otherwise keep the previous sitter's mesh base. Do not restore: that write
        // would land on whoever owns the address now.
        if (s.pawn.Raw() && !s.pawn.Alive()) s = {};
        if (s.pawn.Is(pawn)) return &s;
        if (!s.pawn.Raw() && !free) free = &s;
    }
    if (!create || !free) return nullptr;
    *free = {};
    free->pawn.Set(pawn);
    if (!free->pawn.Alive()) { *free = {}; return nullptr; }
    return free;
}

}  // namespace

void ShiftMesh(void* pawn, float upCm, float forwardCm) {
    if (!pawn || !R::IsLive(pawn)) return;
    if (upCm == 0.f && forwardCm == 0.f) return;
    void* body = puppet::GetNativeBodyMeshComponent(pawn);
    void* vis = puppet::GetMeshPlayerVisibleComponent(pawn);
    // The skin hangs on the inherited mesh. Moving that parent carries the skin with it. Moving
    // both would slide the skin twice.
    const bool visOnBody = vis && body && AttachParent(vis) == body;
    PawnShift* s = FindShift(pawn, true);
    if (!s) return;
    Hold(s->a, body ? body : vis);
    Hold(s->b, (visOnBody || s->a.comp.Is(vis)) ? nullptr : vis);
    const FVector fwd = engine::GetActorForwardVector(pawn);
    const FVector world{ fwd.X * forwardCm, fwd.Y * forwardCm, upCm };
    Apply(s->a, world);
    Apply(s->b, world);
}

void ClearMeshShift(void* pawn) {
    if (!pawn) return;
    PawnShift* s = FindShift(pawn, false);
    if (!s) return;
    Restore(s->a);
    Restore(s->b);
    *s = {};
}

}  // namespace ue_wrap::sit_pose
