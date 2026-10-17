// coop/player/puppet_identity_scan.cpp -- the four native seams of coop/player/puppet_identity: the three scans a
// Blueprint reads the result of (birch's overlap loop, the angry erie's sphere, the shower's trace), which drop a
// puppet from what the Blueprint is about to read, and the camera call the grunt's touch ends in, which is refused
// while a puppet's touch of a grunt is running.

#include "puppet_identity_internal.h"  // the rows and seams shared with the main file (src-local)

#include "ue_wrap/core/caller_locals.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"  // FindPropertyOffset
#include "ue_wrap/core/script_calls.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace coop::puppet_identity::scan {
namespace {

namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace UH = ue_wrap::ufunction_hook;
namespace SC = ue_wrap::script_calls;
namespace CL = ue_wrap::caller_locals;
namespace OI = ue_wrap::object_index;

UE_FACT_FN(kRowPuppetGetOverlappingActors, "puppet_identity", L"PrimitiveComponent", L"GetOverlappingActors",
           Declared);
UE_FACT_FN(kRowPuppetSphereOverlapActors, "puppet_identity", L"KismetSystemLibrary", L"SphereOverlapActors", Declared);
UE_FACT_FN(kRowPuppetLineTraceSingleForObjects, "puppet_identity", L"KismetSystemLibrary",
           L"LineTraceSingleForObjects", Declared);
UE_FACT_FN(kRowPuppetSetControlRotation, "puppet_identity", L"Controller", L"SetControlRotation", Declared);

void OnOverlapPost(void* context, void* sourceObject, void* result);
void OnSpherePost(void* context, void* sourceObject, void* result);
void OnTracePost(void* context, void* sourceObject, void* result);
sg::Verdict OnSetRotationPre(void* context, void* sourceObject, const uint8_t* parms);

// One native: a post hook that watches the scan, or the pre hook of the grunt window.
struct Seam {
    const ue_wrap::facts::Function* row;
    UH::PostNativeCallback          post;
    UH::PreNativeCallback           pre;
    void*                           fn;
    bool                            installed;
    int                             triedGen;
};
enum SeamIndex : int { kOverlap, kSphere, kTrace, kSetRotation, kSeamCount };
Seam g_seams[kSeamCount] = {
    {&kRowPuppetGetOverlappingActors, &OnOverlapPost, nullptr, nullptr, false, -1},
    {&kRowPuppetSphereOverlapActors, &OnSpherePost, nullptr, nullptr, false, -1},
    {&kRowPuppetLineTraceSingleForObjects, &OnTracePost, nullptr, nullptr, false, -1},
    {&kRowPuppetSetControlRotation, nullptr, &OnSetRotationPre, nullptr, false, -1},
};

// One scan: the Blueprint function whose pinned calls of the native write the out local, and where that local
// sits in the function's frame. The calls are named by the offset each ends at (CallerFrame::codeOffset).
constexpr int kMaxEnds = 8;
struct ScanRow {
    Row            counter;
    SeamIndex      seam;
    const wchar_t* cls;
    const wchar_t* ubergraphName;
    const wchar_t* local;
    int            pinned;
    void*          resolvedCls;
    void*          ubergraph;
    int32_t        outOffset;
    int32_t        ends[kMaxEnds];
    int            endCount;
    bool           ready;
};
enum ScanIndex : int { kBirchRow, kErieRow, kShowerRow, kScanCount };
ScanRow g_scan[kScanCount] = {
    {kBirch, kOverlap, L"birch_C", L"ExecuteUbergraph_birch", L"CallFunc_GetOverlappingActors_OverlappingActors", 5,
     nullptr, nullptr, -1, {}, 0, false},
    {kErie, kSphere, L"npc_angryErieFlesh_C", L"ExecuteUbergraph_npc_angryErieFlesh",
     L"CallFunc_SphereOverlapActors_OutActors", 1, nullptr, nullptr, -1, {}, 0, false},
    {kShower, kTrace, L"prop_shower_C", L"ExecuteUbergraph_prop_shower", L"CallFunc_LineTraceSingleForObjects_OutHit",
     1, nullptr, nullptr, -1, {}, 0, false},
};

constexpr uint64_t kNoVersion = UINT64_MAX;
uint64_t              g_scanVer = kNoVersion;
CL::HitLayout         g_layout;
bool                  g_layoutOk = false;
bool                  g_gruntArmed = false;

// The native's UFunction, from its row: found at install, else here.
void* NativeFn(Seam& seam) {
    if (!seam.fn) seam.fn = ue_wrap::facts::Resolve(*seam.row).Fn();
    return seam.fn;
}

// Is the call this post reports one of the row's pinned calls: the resolve current, the caller the row's
// ubergraph, the code standing at one of the calls' ends. Fills `frame` with the caller's frame.
bool OwnCall(const ScanRow& row, UH::CallerFrame& frame) {
    if (!row.ready || OI::ClassUnlistVersion() != g_scanVer) return false;
    frame = UH::CurrentCallerFrame();
    if (frame.function != row.ubergraph || !frame.locals) return false;
    for (int i = 0; i < row.endCount; ++i)
        if (frame.codeOffset == row.ends[i]) return true;
    return false;
}

// Birch's five overlap scans of its kill loop: the puppets leave the list the loop is about to walk.
void OnOverlapPost(void*, void*, void*) {
    const ScanRow& row = g_scan[kBirchRow];
    UH::CallerFrame frame;
    if (!OwnCall(row, frame)) return;
    if (CL::RemoveActors(frame.locals, row.outOffset, &IsPuppetBodyFn, nullptr) > 0) NoteRefused(row.counter, nullptr);
}

// The angry erie's sphere: the puppets leave the list, and the answer the Blueprint tests is whether any is left.
void OnSpherePost(void*, void*, void*) {
    const ScanRow& row = g_scan[kErieRow];
    UH::CallerFrame frame;
    if (!OwnCall(row, frame)) return;
    int remaining = -1;
    if (CL::RemoveActors(frame.locals, row.outOffset, &IsPuppetBodyFn, nullptr, &remaining) > 0)
        NoteRefused(row.counter, nullptr);
    if (remaining < 0) return;
    if (bool* answer = static_cast<bool*>(UH::CurrentResult())) *answer = remaining > 0;
}

// The shower's trace: a hit on a puppet keeps its location, so the splash lands where the water stops, and
// loses its actor and component, so the Blueprint's cast of the hit fails and no bucket behind it fills.
void OnTracePost(void*, void*, void*) {
    const ScanRow& row = g_scan[kShowerRow];
    UH::CallerFrame frame;
    if (!g_layoutOk || !OwnCall(row, frame)) return;
    uint8_t* hit = frame.locals + row.outOffset;
    int32_t index = -1, serial = 0;
    std::memcpy(&index, hit + g_layout.actor, sizeof(index));
    std::memcpy(&serial, hit + g_layout.actor + sizeof(index), sizeof(serial));
    void* actor = R::ResolveWeakObject(index, serial);
    if (!actor || !IsPuppetBodyFn(nullptr, actor)) return;
    if (CL::ClearHitActor(hit, g_layout)) NoteRefused(row.counter, actor);
}

sg::Verdict OnSetRotationPre(void*, void*, const uint8_t*) {
    if (!InGruntScope()) return sg::Verdict::Run;
    NoteRefused(kGrunt, nullptr);
    return sg::Verdict::Cancel;
}

bool InstallSeam(Seam& seam) {
    const ue_wrap::facts::Resolved resolved = ue_wrap::facts::Resolve(*seam.row);
    seam.fn = resolved.Fn();
    return seam.fn && (seam.post ? UH::InstallPostHook(resolved, seam.post, /*armed=*/false)
                                 : UH::InstallPreHook(resolved, seam.pre, /*armed=*/false));
}

// One scan row looked at again: its ubergraph, the pinned calls of its native that write its out local, and the
// local's offset. A row that does not match the pinned count refuses nothing, said once per resolve.
void ResolveRow(ScanRow& row, void* cls) {
    row.ready = false;
    row.resolvedCls = cls;
    row.ubergraph = nullptr;
    row.outOffset = -1;
    row.endCount = 0;
    if (!cls) return;
    void* ubergraph = R::FindFunction(cls, row.ubergraphName);
    void* native = NativeFn(g_seams[row.seam]);
    std::vector<SC::CallSite> sites;
    if (!ubergraph || !native || !SC::EnumerateCalls(ubergraph, sites)) {
        UE_LOGE("[PI] %ls: %ls, its native or its decode did not resolve -- that scan refuses nothing in this world",
                row.cls, row.ubergraphName);
        return;
    }
    int found = 0;
    for (const SC::CallSite& site : sites) {
        if (!SC::SameCallee(site, native) || !SC::CallHasLocal(site, row.local)) continue;
        if (found < kMaxEnds) row.ends[found] = site.end;
        ++found;
    }
    if (found != row.pinned) {
        UE_LOGE("[PI] %ls: %d call(s) write %ls, %d are pinned -- that scan refuses nothing in this world",
                row.cls, found, row.local, row.pinned);
        return;
    }
    const int32_t offset = R::FindPropertyOffset(ubergraph, row.local);
    if (offset < 0) {
        UE_LOGE("[PI] %ls: %ls has no local %ls -- that scan refuses nothing in this world", row.cls,
                row.ubergraphName, row.local);
        return;
    }
    if (row.seam == kTrace && !g_layoutOk) {
        g_layoutOk = CL::ResolveHitLayout(g_layout);
        if (!g_layoutOk) {
            UE_LOGE("[PI] the hit result's layout did not resolve -- the shower refuses nothing in this world");
            return;
        }
    }
    row.ubergraph = ubergraph;
    row.outOffset = offset;
    row.endCount = found;
    row.ready = true;
    UE_LOGI("[PI] %ls scan ready: %d call(s) of the native write %ls (offset %d)", row.cls, found, row.local, offset);
}

}  // namespace

void InstallPump(int generation, int puppets) {
    for (Seam& seam : g_seams) {
        if (seam.installed || seam.triedGen == generation) continue;
        seam.triedGen = generation;
        if (!InstallSeam(seam)) {
            UE_LOGE("[PI] the %ls seam did not install -- what reads through it is not refused in this world",
                    seam.row->Name());
            continue;
        }
        seam.installed = true;
        if (seam.post) UH::SetArmed(seam.fn, seam.post, puppets > 0);
    }
}

void Rearm(int puppets) {
    for (Seam& seam : g_seams)
        if (seam.installed && seam.post) UH::SetArmed(seam.fn, seam.post, puppets > 0);
}

bool Resolve(uint64_t unlistVersion) {
    const bool versionMoved = unlistVersion != g_scanVer;
    bool looked = false;
    for (ScanRow& row : g_scan) {
        void* cls = OI::ClassByName(row.cls);
        if (!versionMoved && cls == row.resolvedCls) continue;
        looked = true;
        ResolveRow(row, cls);
    }
    g_scanVer = unlistVersion;
    return looked;
}

void ArmGruntWindow(bool on) {
    Seam& seam = g_seams[kSetRotation];
    if (!seam.installed || g_gruntArmed == on) return;
    if (UH::SetArmed(seam.fn, seam.pre, on)) g_gruntArmed = on;
}

bool GruntWindowArmed() { return g_gruntArmed; }

void Clear() {
    for (ScanRow& row : g_scan) {
        row.ready = false;
        row.resolvedCls = nullptr;
        row.ubergraph = nullptr;
        row.endCount = 0;
    }
    g_scanVer = kNoVersion;
    ArmGruntWindow(false);
}

}  // namespace coop::puppet_identity::scan
