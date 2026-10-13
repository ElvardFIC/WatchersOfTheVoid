// ue_wrap/core/trace.cpp -- see ue_wrap/core/trace.h.

#include "ue_wrap/core/trace.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"

#include <cstdint>
#include <cmath>
#include <vector>

namespace ue_wrap::trace {
namespace {

namespace R = reflection;

// Resolved once; the KismetSystemLibrary CDO + UFunction are process-stable native
// objects -- never GC'd, latch-safe (same latch shape as kerfur.cpp's KSL latch).
void* g_kslCdo  = nullptr;  // Default__KismetSystemLibrary (the static-call dispatch object)
void* g_traceFn = nullptr;  // LineTraceSingleForObjects

// The object set: {WorldStatic, WorldDynamic} (EObjectTypeQuery1=0, 2=1;
// TEnumAsByte = 1 byte each). Static storage: the engine only READS an in-param
// TArray (const ref), it never reallocs/frees it.
uint8_t g_traceObjTypes[2] = {0, 1};
#pragma pack(push, 4)
struct TArrayControl { void* data; int32_t num; int32_t max; };
#pragma pack(pop)

}  // namespace

int LineBlockedStatDyn(void* worldCtx, const FVector& start, const FVector& end) {
    if (!worldCtx) return -1;
    if (!g_traceFn || !g_kslCdo) {
        if (!g_kslCdo) g_kslCdo = R::FindClassDefaultObject(L"KismetSystemLibrary");
        if (!g_traceFn) {
            if (void* kc = R::FindClass(L"KismetSystemLibrary"))
                g_traceFn = R::FindFunction(kc, L"LineTraceSingleForObjects");
        }
        if (!g_traceFn || !g_kslCdo) return -1;
    }
    ParamFrame f(g_traceFn);
    if (!f.valid()) return -1;
    f.Set<void*>(L"WorldContextObject", worldCtx);
    f.Set<ue_wrap::FVector>(L"Start", start);
    f.Set<ue_wrap::FVector>(L"End", end);
    TArrayControl objTypes{g_traceObjTypes, 2, 2};
    f.SetRaw(L"ObjectTypes", &objTypes, sizeof(objTypes));
    f.Set<bool>(L"bTraceComplex", false);
    // ActorsToIgnore stays the zero-initialized empty TArray (frame is zeroed);
    // DrawDebugType 0 = None; OutHit is an in-frame zeroed FHitResult (POD members
    // only -- FName/floats/weak ptrs -- so no destructor concerns on our frame).
    f.Set<bool>(L"bIgnoreSelf", true);
    if (!ue_wrap::Call(g_kslCdo, f)) return -1;
    return f.Get<bool>(L"ReturnValue") ? 1 : 0;
}

int BoxBlocked(void* worldCtx, const FVector& start, const FVector& end, const FVector& halfSize) {
    if (!worldCtx) return -1;
    static void* fn = nullptr;
    if (!g_kslCdo) g_kslCdo = R::FindClassDefaultObject(L"KismetSystemLibrary");
    if (!fn) if (void* cls = R::FindClass(L"KismetSystemLibrary"))
        fn = R::FindFunction(cls, L"BoxTraceSingleForObjects");
    ParamFrame f(fn);
    uint8_t types[] = {0, 1, 2, 3, 4, 5}; // static/dynamic, pawn, physics body, vehicle, destructible
    void* ignored[] = {worldCtx};
    TArrayControl objects{types, 6, 6}, ignore{ignored, 1, 1};
    if (!g_kslCdo || !f.valid() || !f.Set<void*>(L"WorldContextObject", worldCtx) ||
        !f.Set(L"Start", start) || !f.Set(L"End", end) || !f.Set(L"HalfSize", halfSize) ||
        !f.Set(L"Orientation", FRotator{}) || !f.SetRaw(L"ObjectTypes", &objects, sizeof(objects)) ||
        !f.SetRaw(L"ActorsToIgnore", &ignore, sizeof(ignore)) || !f.Set(L"bIgnoreSelf", true) ||
        !Call(g_kslCdo, f)) return -1;
    bool hit = false;
    return f.GetRaw(L"ReturnValue", &hit, sizeof(hit)) ? (hit ? 1 : 0) : -1;
}

bool FindFloor(void* worldCtx, const FVector& start, const FVector& end, FVector& point) {
    if (!worldCtx) return false;
    if (!g_kslCdo) g_kslCdo = R::FindClassDefaultObject(L"KismetSystemLibrary");
    if (!g_traceFn) if (void* cls = R::FindClass(L"KismetSystemLibrary"))
        g_traceFn = R::FindFunction(cls, L"LineTraceSingleForObjects");
    static void* cdo = nullptr;
    static void* fn = nullptr;
    if (!cdo) cdo = R::FindClassDefaultObject(L"GameplayStatics");
    if (!fn) if (void* cls = R::FindClass(L"GameplayStatics"))
        fn = R::FindFunction(cls, L"BreakHitResult");
    ParamFrame f(g_traceFn), b(fn);
    void* ignored[] = {worldCtx};
    TArrayControl objects{g_traceObjTypes, 2, 2}, ignore{ignored, 1, 1};
    if (!g_kslCdo || !cdo || !f.valid() || !b.valid() ||
        !f.Set<void*>(L"WorldContextObject", worldCtx) || !f.Set(L"Start", start) || !f.Set(L"End", end) ||
        !f.SetRaw(L"ObjectTypes", &objects, sizeof(objects)) ||
        !f.SetRaw(L"ActorsToIgnore", &ignore, sizeof(ignore)) || !f.Set(L"bIgnoreSelf", true) ||
        !Call(g_kslCdo, f)) return false;
    bool hit = false;
    if (!f.GetRaw(L"ReturnValue", &hit, sizeof(hit)) || !hit) return false;
    const int size = R::FindParamSize(g_traceFn, L"OutHit");
    if (size <= 0 || size > 512 || size != R::FindParamSize(fn, L"Hit")) return false;
    std::vector<uint8_t> bytes(size);
    if (!f.GetRaw(L"OutHit", bytes.data(), size) || !b.SetRaw(L"Hit", bytes.data(), size) ||
        !Call(cdo, b)) return false;
    bool overlap = false;
    FVector normal{};
    if (!b.GetRaw(L"bInitialOverlap", &overlap, sizeof(overlap)) || overlap ||
        !b.GetRaw(L"ImpactPoint", &point, sizeof(point)) || !b.GetRaw(L"ImpactNormal", &normal, sizeof(normal)))
        return false;
    return std::isfinite(point.X) && std::isfinite(point.Y) && std::isfinite(point.Z) &&
        std::isfinite(normal.Z) && normal.Z >= 0.85f;
}

}  // namespace ue_wrap::trace
