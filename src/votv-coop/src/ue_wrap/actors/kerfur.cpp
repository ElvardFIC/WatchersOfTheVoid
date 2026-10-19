// ue_wrap/actors/kerfur.cpp -- see ue_wrap/actors/kerfur.h for the contract.

#include "ue_wrap/actors/kerfur.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/devices/floppy_slot.h"       // MeshForSlotType -- the disc the reports slot shows
#include "ue_wrap/engine/engine_component.h"  // SetStaticMesh / ClearStaticMesh / SetComponentVisible

#include <cstring>
#include <cwchar>

namespace ue_wrap::kerfur {
namespace {

namespace R = ue_wrap::reflection;
namespace P = ue_wrap::profile;

// kerfurOmega_C resolves once (loads with the level; re-resolve while still null).
void* ResolveKerfurClass() {
    static void* sCls = nullptr;
    if (!sCls) sCls = R::FindClass(P::name::NpcClass_KerfurOmega);
    return sCls;
}

// Cached field offsets (resolved ONCE vs kerfurOmega_C; the offsets are inherited by every
// skin subclass, so resolving on the base is correct + stable). Sentinel -2 = "not attempted";
// FindPropertyOffset returns -1 when absent (>= 0 is a valid offset). The hot reads
// (ReadKerfurState per tick) never re-walk once these settle.
int32_t g_offState     = -2;  // TEnumAsByte<enum_kerfurCommand> "State"
int32_t g_offSpooky    = -2;  // bool "isSpooky"
int32_t g_offFace      = -2;  // int32 "faceMaterialIndex"
int32_t g_offKill      = -2;  // bool "kill" (murderfur-mode guard)

void ResolveKerfurOffsets() {
    void* kc = ResolveKerfurClass();
    if (!kc) return;
    if (g_offState  == -2) g_offState  = R::FindPropertyOffset(kc, L"State");
    if (g_offSpooky == -2) g_offSpooky = R::FindPropertyOffset(kc, L"isSpooky");
    if (g_offFace   == -2) g_offFace   = R::FindPropertyOffset(kc, L"faceMaterialIndex");
    if (g_offKill   == -2) g_offKill   = R::FindPropertyOffset(kc, L"kill");
}

}  // namespace

bool IsKerfurActor(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    void* kc = ResolveKerfurClass();
    if (!kc) return false;
    void* cls = R::ClassOf(actor);
    if (!cls) return false;
    if (cls == kc) return true;
    void* bases[1] = {kc};
    return R::IsDescendantOfAny(cls, bases, 1);
}

bool HasSaveKey(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = R::ClassOf(actor);
    if (!cls) return false;
    // 1-entry memo: the host sender iterates same-class NPCs (kerfurs) at the connect edge;
    // avoid re-walking the FProperty chain for the repeated class. Cold path (per-NPC at
    // connect), so a single-slot memo is plenty.
    static void*   sMemoCls = nullptr;
    static int32_t sMemoOff = -2;
    int32_t off;
    if (cls == sMemoCls) {
        off = sMemoOff;
    } else {
        off = R::FindPropertyOffset(cls, L"Key");
        sMemoCls = cls;
        sMemoOff = off;
    }
    if (off < 0) return false;  // class has no int_save "Key" (host-spawned transient enemy)
    const R::FName key = *reinterpret_cast<const R::FName*>(
        reinterpret_cast<const uint8_t*>(actor) + off);
    const std::wstring w = R::ToString(key);
    return !(w.empty() || w == L"None");  // a non-None key -> this NPC is a save object
}

bool ReadKerfurState(void* actor, uint8_t& state, bool& spooky, uint8_t& face) {
    if (!IsKerfurActor(actor)) return false;
    ResolveKerfurOffsets();
    if (g_offState < 0) return false;
    state  = *reinterpret_cast<const uint8_t*>(reinterpret_cast<const uint8_t*>(actor) + g_offState);
    spooky = (g_offSpooky >= 0) &&
             *reinterpret_cast<const bool*>(reinterpret_cast<const uint8_t*>(actor) + g_offSpooky);
    face   = (g_offFace >= 0)
                 ? static_cast<uint8_t>(*reinterpret_cast<const int32_t*>(
                       reinterpret_cast<const uint8_t*>(actor) + g_offFace))
                 : uint8_t{0};
    return true;
}

void DriveKerfurState(void* actor, uint8_t state, bool spooky) {
    if (!IsKerfurActor(actor)) return;
    ResolveKerfurOffsets();
    if (g_offState >= 0)
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(actor) + g_offState) = state;
    if (g_offSpooky >= 0)
        *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(actor) + g_offSpooky) = spooky;
}

void NeutralizeAiTimers(void* actor) {
    if (!IsKerfurActor(actor)) return;
    static void* sKslCdo  = nullptr;
    static void* sClearFn = nullptr;
    if (!sKslCdo) sKslCdo = R::FindClassDefaultObject(L"KismetSystemLibrary");
    if (sKslCdo && !sClearFn) {
        if (void* kc = R::FindClass(L"KismetSystemLibrary"))
            sClearFn = R::FindFunction(kc, L"K2_ClearTimer");
    }
    if (!sKslCdo || !sClearFn) {
        static bool sWarned = false;
        if (!sWarned) { sWarned = true;
            UE_LOGW("kerfur: K2_ClearTimer unresolved -- mirror AI timers NOT neutralized "
                    "(timer_face/timer_kerf/checkDoor may run local AI on the parked mirror)"); }
        return;
    }
    // K2_ClearTimer(Object, FunctionName) -- the exact inverse of the BP's K2_SetTimerDelegate
    // ({self, name}) arm (kerfur BeginPlay @8184/@8714/@24444). FunctionName is an FString
    // {data, num=len+1, max} (num/max include the null terminator -- the space_renderer pattern).
    static const wchar_t* kTimers[3] = {L"timer_face", L"timer_kerf", L"checkDoor"};
    for (const wchar_t* fn : kTimers) {
        ue_wrap::ParamFrame f(sClearFn);
        if (!f.valid()) continue;
        f.Set<void*>(L"Object", actor);
        const int32_t num = static_cast<int32_t>(std::wcslen(fn)) + 1;
        struct { const wchar_t* data; int32_t n; int32_t m; } fs{fn, num, num};
        if (!f.SetRaw(L"FunctionName", &fs, sizeof(fs))) continue;
        ue_wrap::Call(sKslCdo, f);
    }
}

bool ReadKill(void* actor) {
    if (!IsKerfurActor(actor)) return false;
    ResolveKerfurOffsets();
    if (g_offKill < 0) return false;
    return *reinterpret_cast<const bool*>(reinterpret_cast<const uint8_t*>(actor) + g_offKill);
}

bool RunActionName(void* kerfurActor, void* playerActor, const wchar_t* name) {
    if (!IsKerfurActor(kerfurActor) || !name) return false;
    static void* sActionNameFn = nullptr;
    if (!sActionNameFn) {
        if (void* kc = ResolveKerfurClass())
            sActionNameFn = R::FindFunction(kc, L"actionName");  // case-insensitive (engine FName)
    }
    if (!sActionNameFn) {
        static bool sWarned = false;
        if (!sWarned) { sWarned = true;
            UE_LOGW("kerfur: actionName unresolved -- cannot run menu verb on host"); }
        return false;
    }
    ue_wrap::ParamFrame f(sActionNameFn);
    if (!f.valid()) return false;
    f.Set<void*>(L"Player", playerActor);
    const int32_t num = static_cast<int32_t>(std::wcslen(name)) + 1;
    struct { const wchar_t* data; int32_t n; int32_t m; } fs{name, num, num};
    f.SetRaw(L"Name", &fs, sizeof(fs));
    // Hit (FHitResult) is left zeroed -- the State-change verbs do not read it.
    return ue_wrap::Call(kerfurActor, f);
}

bool IsInsertPlayerRead(void* callerFunction, const uint8_t* callerLocals, const uint8_t* out) {
    // Resolved per class object, and again when the ubergraph function held is no longer that class's live
    // one: a world that reloads the class brings a new function, possibly at the old class's address.
    static void*   sCls = nullptr;
    static void*   sUber = nullptr;
    static int32_t sUberIdx = -1;
    static int32_t sOff = -1;
    void* kc = ue_wrap::object_index::ClassByName(P::name::NpcClass_KerfurOmega);
    if (!kc) return false;
    if (kc != sCls || (sUber && (!R::IsLiveByIndex(sUber, sUberIdx) || R::OuterOf(sUber) != kc))) {
        sCls = kc;
        sUber = R::FindFunction(kc, L"ExecuteUbergraph_kerfurOmega");
        sUberIdx = sUber ? R::InternalIndexOf(sUber) : -1;
        sOff = sUber ? R::FindPropertyOffset(sUber, L"CallFunc_getMainPlayer_AsMain_Player_1") : -1;
        if (sOff < 0)
            UE_LOGW("kerfur: the disc insert's player variable did not resolve (ubergraph %p) -- a served "
                    "Omega's insert reads player 0 while this class lives", sUber);
    }
    return sOff >= 0 && callerFunction == sUber && callerLocals && out == callerLocals + sOff;
}

bool ReadHasFloppy(void* actor, bool& has) {
    if (!IsKerfurActor(actor)) return false;
    static int32_t sOff = -2;
    static uint8_t sMask = 0;
    if (sOff == -2 && !R::FindBoolProperty(ResolveKerfurClass(), L"hasFloppy", sOff, sMask)) sOff = -1;
    if (sOff < 0) return false;
    has = (static_cast<const uint8_t*>(actor)[sOff] & sMask) != 0;
    return true;
}

// hasFloppy and floppyType, resolved once. -1 offset means the field is absent on this class.
struct FloppyFields { int32_t hasOff = -2; uint8_t hasMask = 0; int32_t typeOff = -2; int32_t meshOff = -2; };
static FloppyFields g_floppy;

static void ResolveFloppyFields() {
    void* kc = ResolveKerfurClass();
    if (!kc) return;
    if (g_floppy.hasOff == -2 && !R::FindBoolProperty(kc, L"hasFloppy", g_floppy.hasOff, g_floppy.hasMask))
        g_floppy.hasOff = -1;
    if (g_floppy.typeOff == -2) g_floppy.typeOff = R::FindPropertyOffset(kc, L"floppyType");
    if (g_floppy.meshOff == -2) g_floppy.meshOff = R::FindPropertyOffset(kc, L"floppyMesh");
}

bool ReadKerfurFloppy(void* actor, bool& has, uint8_t& type) {
    if (!IsKerfurActor(actor)) return false;
    ResolveFloppyFields();
    if (g_floppy.hasOff < 0 || g_floppy.typeOff < 0) return false;
    const auto* base = static_cast<const uint8_t*>(actor);
    has = (base[g_floppy.hasOff] & g_floppy.hasMask) != 0;
    const int32_t raw = *reinterpret_cast<const int32_t*>(base + g_floppy.typeOff);
    // lib_C::floppyFromType indexes seven meshes and nothing else. A type outside that, or a flag
    // with no type, is a robot that is not showing a disc.
    if (!has || raw < 0 || raw >= ue_wrap::floppy_slot::kSlotTypeCount) {
        has = false;
        type = 0;
        return true;
    }
    type = static_cast<uint8_t>(raw);
    return true;
}

bool DriveKerfurFloppy(void* actor, bool has, uint8_t type) {
    if (!IsKerfurActor(actor)) return true;
    ResolveFloppyFields();
    // Absent fields are a fact about this class, not a delay. Stop asking.
    if (g_floppy.hasOff < 0 || g_floppy.typeOff < 0 || g_floppy.meshOff < 0) return true;
    void* comp = *reinterpret_cast<void**>(static_cast<uint8_t*>(actor) + g_floppy.meshOff);
    if (!comp || !R::IsLive(comp)) return false;
    const int32_t written = (has && type < ue_wrap::floppy_slot::kSlotTypeCount)
                                ? static_cast<int32_t>(type) : -1;
    if (written >= 0) {
        void* mesh = nullptr;
        if (!ue_wrap::floppy_slot::MeshForSlotType(actor, written, mesh)) return false;
        // A Static component ignores SetStaticMesh. The disc rides the skeleton, so it is Movable;
        // saying so again is what makes a mirror that spawned Static actually show the mesh.
        // A successful ask that names no mesh is an empty answer, not a delay: retrying it would
        // call the library every frame.
        if (mesh) {
            ue_wrap::engine::SetComponentMobility(comp, 2);
            if (!ue_wrap::engine::SetStaticMesh(comp, mesh)) return false;
            ue_wrap::engine::SetComponentVisible(comp, true, false);
        } else {
            ue_wrap::engine::ClearStaticMesh(comp);
            ue_wrap::engine::SetComponentVisible(comp, false, false);
        }
    } else {
        ue_wrap::engine::ClearStaticMesh(comp);
        ue_wrap::engine::SetComponentVisible(comp, false, false);
    }
    auto* base = static_cast<uint8_t*>(actor);
    if (written >= 0) base[g_floppy.hasOff] = static_cast<uint8_t>(base[g_floppy.hasOff] | g_floppy.hasMask);
    else              base[g_floppy.hasOff] = static_cast<uint8_t>(base[g_floppy.hasOff] & ~g_floppy.hasMask);
    *reinterpret_cast<int32_t*>(base + g_floppy.typeOff) = written;
    return true;
}

void* ReadHoldObject(void* actor) {
    if (!IsKerfurActor(actor)) return nullptr;
    static int32_t sOff = -2;
    if (sOff == -2) sOff = R::FindPropertyOffset(ResolveKerfurClass(), L"holdObject");
    if (sOff < 0) return nullptr;
    void* t = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(actor) + sOff);
    return (t && R::IsLive(t)) ? t : nullptr;
}

void* ReadTargetActor(void* actor) {
    if (!IsKerfurActor(actor)) return nullptr;
    static int32_t sOff = -2;
    if (sOff == -2) sOff = R::FindPropertyOffset(ResolveKerfurClass(), L"targetActor");
    if (sOff < 0) return nullptr;
    void* t = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(actor) + sOff);
    return (t && R::IsLive(t)) ? t : nullptr;
}

}  // namespace ue_wrap::kerfur
