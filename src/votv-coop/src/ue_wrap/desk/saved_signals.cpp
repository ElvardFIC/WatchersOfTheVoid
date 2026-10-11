// ue_wrap/desk/saved_signals.cpp -- see header.

#include "ue_wrap/desk/saved_signals.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/world/world_singleton.h"

#include <chrono>
#include <cstring>

namespace ue_wrap::saved_signals {

namespace R = ue_wrap::reflection;
namespace SD = ue_wrap::signal_dynamic;

namespace {

struct TArrayView { uint8_t* data; int32_t num; int32_t max; };

void* g_gamemodeCls = nullptr;
int32_t g_offSavedSignals = -1;  // mainGamemode_C::savedSignals_0, resolved by name
void* g_saveSignalFn = nullptr;
void* g_deleteSignalFn = nullptr;

std::chrono::steady_clock::time_point g_nextResolve{};
bool g_coreResolved = false;

void ResolvePass() {
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextResolve) return;
    g_nextResolve = now + std::chrono::seconds(2);
    if (!g_gamemodeCls) g_gamemodeCls = R::FindClass(L"mainGamemode_C");
    if (!g_gamemodeCls) return;
    if (g_offSavedSignals < 0)
        g_offSavedSignals = R::FindPropertyOffset(g_gamemodeCls, L"savedSignals_0");
    if (!g_saveSignalFn) g_saveSignalFn = R::FindFunction(g_gamemodeCls, L"saveSignal");
    if (!g_deleteSignalFn) g_deleteSignalFn = R::FindFunction(g_gamemodeCls, L"deleteSignal");
    const bool core = g_offSavedSignals >= 0 && g_saveSignalFn && g_deleteSignalFn;
    if (core && !g_coreResolved) {
        g_coreResolved = true;
        UE_LOGI("saved_signals: resolved (savedSignals_0=0x%X saveSignal=yes deleteSignal=yes)",
                g_offSavedSignals);
    }
}

TArrayView* Rows() {
    void* gm = world_singleton::Gamemode();
    if (!gm || g_offSavedSignals < 0) return nullptr;
    return reinterpret_cast<TArrayView*>(reinterpret_cast<uint8_t*>(gm) + g_offSavedSignals);
}

}  // namespace

bool EnsureResolved() {
    ResolvePass();
    return g_coreResolved;
}

int32_t Count() {
    TArrayView* a = Rows();
    if (!a) return -1;
    if (a->num < 0 || a->num > 4096) return -1;  // sanity
    return a->num;
}

bool ReadRow(int32_t index, SD::Row& out, bool withImage) {
    TArrayView* a = Rows();
    if (!a || index < 0 || index >= a->num) return false;
    return SD::ReadStruct(a->data + static_cast<size_t>(index) * SD::kStride, out, withImage);
}

bool ReadRowKey(int32_t index, RowKey& out) {
    TArrayView* a = Rows();
    if (!a || index < 0 || index >= a->num) return false;
    const uint8_t* rp = a->data + static_cast<size_t>(index) * SD::kStride;
    std::memcpy(&out.namePtr, rp + SD::kOff_name, sizeof(out.namePtr));
    std::memcpy(&out.idPtr, rp + SD::kOff_id, sizeof(out.idPtr));
    std::memcpy(&out.date, rp + SD::kOff_date, sizeof(out.date));
    std::memcpy(&out.level, rp + SD::kOff_level, sizeof(out.level));
    out.isCopy = *(rp + SD::kOff_isCopy);
    return true;
}

RowApply ApplySaveSignal(const SD::Row& row) {
    using R = RowApplyResult;
    // The two the game cannot be asked yet: no world, or the verb not found in it.
    void* gm = world_singleton::Gamemode();
    if (!gm) return {R::NotNow, "no gamemode"};
    if (!g_saveSignalFn) return {R::NotNow, "saveSignal not resolved"};
    // Every other failure is final for the row: the verb's layout and the name library are fixed for
    // the process, so what fails here fails on every try.
    ue_wrap::ParamFrame f(g_saveSignalFn);
    if (!f.valid()) return {R::Refused, "saveSignal's parameter frame could not be built"};
    uint8_t sig[SD::kStride];
    if (!SD::BuildParamBytes(row, sig))  // row outlives the call (FStrings alias it)
        return {R::Refused, "a name in the row could not be made an FName"};
    if (!f.SetRaw(L"signal", sig, SD::kStride)) return {R::Refused, "the signal parameter was refused"};
    f.Set<bool>(L"new", false);          // dead param (RE: never read)
    f.Set<bool>(L"checkOnly", false);
    f.Set<float>(L"downloadedAtQuality", row.downloadedAtQuality);
    f.Set<bool>(L"selfQuality", true);   // keep the row's own quality unchanged
    if (!ue_wrap::Call(gm, f)) return {R::Refused, "saveSignal faulted"};
    // succ is false on one path only, the row's own decoded >= size > 0 test (the bytecode's 1060).
    if (!f.Get<bool>(L"succ")) return {R::Refused, "the game refused it: not decoded, or no size"};
    return {R::Applied, ""};
}

bool DeleteSignal(int32_t index) {
    TArrayView* a = Rows();
    if (!a || index < 0 || index >= a->num) return false;
    void* gm = world_singleton::Gamemode();
    if (!gm || !g_deleteSignalFn) return false;
    ue_wrap::ParamFrame f(g_deleteSignalFn);
    if (!f.valid()) return false;
    if (!f.Set<int32_t>(L"IndexToRemove", index)) return false;
    return ue_wrap::Call(gm, f);
}

bool FirstOrdinarySignalName(std::wstring& out) {
    out.clear();
    void* gm = world_singleton::Gamemode();
    if (!gm) return false;
    void* cls = R::ClassOf(gm);
    if (!cls) return false;
    // Plain (non-GUID) member names, so FindPropertyOffset resolves them; the two arrays are filled
    // in parallel, so index i of signalUniques describes name i.
    const int32_t offNames = R::FindPropertyOffset(cls, L"signalNames_default");
    if (offNames < 0) return false;
    const int32_t offUniq = R::FindPropertyOffset(cls, L"signalUniques");
    struct { R::FName* data; int32_t num; int32_t max; } names{};
    std::memcpy(&names, static_cast<uint8_t*>(gm) + offNames, sizeof(names));
    if (names.num <= 0 || !names.data) return false;  // not built yet (pre-world)
    struct { uint8_t* data; int32_t num; int32_t max; } uniq{};  // TArray<bool>, 1 byte/elem
    if (offUniq >= 0) std::memcpy(&uniq, static_cast<uint8_t*>(gm) + offUniq, sizeof(uniq));
    for (int32_t i = 0; i < names.num; ++i) {
        const bool unique = uniq.data && i < uniq.num && uniq.data[i] != 0;
        if (unique) continue;  // a unique signal lands in `specials`, a side effect a drill must not fire
        std::wstring leaf = R::ToString(names.data[i]);
        if (!leaf.empty() && leaf != L"None") { out = std::move(leaf); return true; }
    }
    return false;
}

}  // namespace ue_wrap::saved_signals
