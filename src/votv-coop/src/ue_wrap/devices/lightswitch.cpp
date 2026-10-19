// ue_wrap/devices/lightswitch.cpp -- see ue_wrap/devices/lightswitch.h. Engine access for VOTV
// light groups (Atrigger_lightRoot_C). Offsets resolved from the live class via
// reflection (version-portable); the Alpha 0.9.0-n values are logged fallbacks.

#include "ue_wrap/devices/lightswitch.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"

#include <atomic>
#include <cstdint>

namespace ue_wrap::lightswitch {
namespace {

namespace R = reflection;

std::atomic<bool> g_resolved{false};

void*   g_rootCls    = nullptr;  // trigger_lightRoot_C UClass
int32_t g_keyOff     = -1;       // AtriggerBase_C::Key       (Alpha 0.9.0-n: 0x0260)
int32_t g_isActiveOff = -1;      // trigger_lightRoot_C::IsActive (0x02B8)
void*   g_runTriggerFn = nullptr;// runTrigger(owner, index) -- index 1/2 are the ABSOLUTE setters
void*   g_setActiveFn = nullptr; // setActive(bool) -- the group's breaker verb
int32_t g_breakerOff  = -1;      // trigger_lightRoot_C::active, the breaker (distinct from IsActive)

constexpr int32_t kKeyOffFallback      = 0x0260;
constexpr int32_t kIsActiveOffFallback = 0x02B8;

// --- The light SWITCH (Alightswitch_C) ---
std::atomic<bool> g_swResolved{false};
void*   g_swCls   = nullptr;  // lightswitch_C UClass
int32_t g_swKeyOff = -1;      // AtriggerBase_C::Key (shared base offset, 0x0260)
int32_t g_swAOff  = -1;       // Alightswitch_C::A (the flip bool, 0x02A0)
void*   g_useFn   = nullptr;  // use()
int32_t g_objectsOff = -1;    // triggerBase_C::objects (TArray<UObject*>): objects[0] is the switch's group
constexpr int32_t kSwitchAOffFallback = 0x02A0;

// Native use() plays a UI sound even when replayed across the map. Limit this
// replacement to the synchronous replay; local presses retain their native cue.
void* g_replayedSwitch = nullptr;
int32_t g_soundOff = -1, g_volumeOff = -1, g_pitchOff = -1;
void* g_clickAttenuation = nullptr; // SpawnSoundAttenuation roots it against GC.
bool g_clickHookReady = false;

script_gate::Verdict OnReplaySound(void*, void* source, const uint8_t* params) {
    if (!g_replayedSwitch || source != g_replayedSwitch || !params)
        return script_gate::Verdict::Run;
    FVector at{};
    void* cue = *reinterpret_cast<void* const*>(params + g_soundOff);
    if (cue && g_clickAttenuation && engine::TryGetActorLocation(source, at)) {
        engine::PlaySoundAtLocation(source, cue, at, g_clickAttenuation,
            *reinterpret_cast<const float*>(params + g_volumeOff),
            *reinterpret_cast<const float*>(params + g_pitchOff));
    }
    // Failure to obtain a position must not fall back to a map-wide UI click.
    return script_gate::Verdict::Cancel;
}

void EnsureReplaySound() {
    if (g_clickHookReady && g_clickAttenuation) return;
    void* cls = R::FindClass(L"GameplayStatics");
    void* fn = cls ? R::FindFunction(cls, L"PlaySound2D") : nullptr;
    if (!fn) return;
    g_soundOff = R::FindParamOffset(fn, L"Sound");
    g_volumeOff = R::FindParamOffset(fn, L"VolumeMultiplier");
    g_pitchOff = R::FindParamOffset(fn, L"PitchMultiplier");
    if (g_soundOff < 0 || g_volumeOff < 0 || g_pitchOff < 0) return;
    if (!g_clickAttenuation) {
        engine::SoundAttenuationConfig cfg{};
        cfg.distanceAlgorithm = 0; // Linear fade from 1 m to silence at 15 m.
        cfg.extents[0] = 100.f;
        cfg.falloffDistance = 1400.f;
        g_clickAttenuation = engine::SpawnSoundAttenuation(cfg);
    }
    g_clickHookReady = ufunction_hook::InstallPreHook(fn, &OnReplaySound, true);
}

struct ReplaySwitchScope {
    void* previous;
    explicit ReplaySwitchScope(void* sw) : previous(g_replayedSwitch) { g_replayedSwitch = sw; }
    ~ReplaySwitchScope() { g_replayedSwitch = previous; }
};

}  // namespace

bool EnsureResolved() {
    if (g_resolved.load(std::memory_order_acquire)) return true;

    void* rootCls = R::FindClass(L"trigger_lightRoot_C");
    if (!rootCls) return false;

    // Key is declared on the trigger base, which the property lookup climbs to.
    int32_t keyOff = R::FindPropertyOffset(rootCls, L"Key");
    if (keyOff < 0) {
        UE_LOGW("lightswitch: reflected Key offset not found -- using fallback 0x%04X", kKeyOffFallback);
        keyOff = kKeyOffFallback;
    }
    int32_t isActiveOff = R::FindPropertyOffset(rootCls, L"IsActive");
    if (isActiveOff < 0) {
        UE_LOGW("lightswitch: reflected IsActive offset not found -- using fallback 0x%04X", kIsActiveOffFallback);
        isActiveOff = kIsActiveOffFallback;
    }
    void* setActiveFn = R::FindFunction(rootCls, L"SetActive");
    if (!setActiveFn) {
        UE_LOGW("lightswitch: SetActive UFunction not found -- not ready");
        return false;
    }
    // `active` (the breaker) and `IsActive` (the live state) are two separate FBoolProperties, and
    // FName lookup ignores case, so these two names resolve two fields. Read only: the breaker is
    // set through the group's own setActive.
    g_breakerOff  = R::FindPropertyOffset(rootCls, L"Active");
    g_setActiveFn = setActiveFn;
    void* runTriggerFn = R::FindFunction(rootCls, L"runTrigger");
    if (!runTriggerFn) UE_LOGW("lightswitch: runTrigger UFunction not found -- absolute group apply disabled");
    g_runTriggerFn = runTriggerFn;

    g_rootCls     = rootCls;
    g_keyOff      = keyOff;
    g_isActiveOff = isActiveOff;
    g_resolved.store(true, std::memory_order_release);
    UE_LOGI("lightswitch: resolved trigger_lightRoot_C=%p Key@0x%04X IsActive@0x%04X SetActive=%p",
            rootCls, keyOff, isActiveOff, setActiveFn);
    return true;
}


bool IsLightRoot(void* obj) {
    if (!obj || !g_rootCls) return false;
    void* cls = R::ClassOf(obj);
    if (!cls) return false;
    void* bases[1] = { g_rootCls };
    return R::IsDescendantOfAny(cls, bases, 1);
}

std::wstring GetKeyString(void* root) {
    if (!root || g_keyOff < 0) return std::wstring();
    const R::FName& key = *reinterpret_cast<const R::FName*>(
        reinterpret_cast<const char*>(root) + g_keyOff);
    return R::ToString(key);
}

bool TryReadActive(void* root, bool& on) {
    if (!root || g_isActiveOff < 0) return false;
    on = *reinterpret_cast<const bool*>(
        reinterpret_cast<const char*>(root) + g_isActiveOff);
    return true;
}

// --- The light SWITCH (Alightswitch_C) ---

bool EnsureSwitchResolved() {
    if (g_swResolved.load(std::memory_order_acquire)) return true;
    void* cls = R::FindClass(L"lightswitch_C");
    if (!cls) return false;
    // Key and objects are declared on the trigger base, which the property lookup climbs to.
    int32_t keyOff = R::FindPropertyOffset(cls, L"Key");
    if (keyOff < 0) keyOff = kKeyOffFallback;
    int32_t aOff = R::FindPropertyOffset(cls, L"A");
    if (aOff < 0) {
        UE_LOGW("lightswitch: switch A offset not found -- using fallback 0x%04X", kSwitchAOffFallback);
        aOff = kSwitchAOffFallback;
    }
    void* useFn = R::FindFunction(cls, L"use");
    if (!useFn) { UE_LOGW("lightswitch: switch use() UFunction not found -- not ready"); return false; }
    g_objectsOff = R::FindPropertyOffset(cls, L"objects");
    g_swCls = cls; g_swKeyOff = keyOff; g_swAOff = aOff; g_useFn = useFn;
    g_swResolved.store(true, std::memory_order_release);
    UE_LOGI("lightswitch: resolved switch lightswitch_C=%p Key@0x%04X A@0x%04X objects@0x%04X use=%p",
            cls, keyOff, aOff, g_objectsOff, useFn);
    return true;
}

bool IsLightSwitch(void* obj) {
    if (!obj || !g_swCls) return false;
    void* cls = R::ClassOf(obj);
    if (!cls) return false;
    void* bases[1] = { g_swCls };
    return R::IsDescendantOfAny(cls, bases, 1);
}

std::wstring GetSwitchKeyString(void* sw) {
    if (!sw || g_swKeyOff < 0) return std::wstring();
    const R::FName& key = *reinterpret_cast<const R::FName*>(
        reinterpret_cast<const char*>(sw) + g_swKeyOff);
    return R::ToString(key);
}

bool TryReadSwitchA(void* sw, bool& on) {
    if (!sw || g_swAOff < 0) return false;
    on = *reinterpret_cast<const bool*>(reinterpret_cast<const char*>(sw) + g_swAOff);
    return true;
}

bool CallUse(void* sw) {
    if (!sw || !g_useFn) return false;
    ParamFrame f(g_useFn);
    if (!f.valid()) return false;
    EnsureReplaySound();
    ReplaySwitchScope scope(sw);
    return Call(sw, f);
}

// --- The GROUP as a synced entity -----------------------------------------

void* SwitchRoot(void* sw) {
    if (!sw || g_objectsOff < 0 || !EnsureResolved()) return nullptr;
    const auto* arr = reinterpret_cast<const field_io::TArrayView*>(reinterpret_cast<const char*>(sw) + g_objectsOff);
    if (!arr->data || arr->num <= 0) return nullptr;
    void* first = *reinterpret_cast<void* const*>(arr->data);
    return (first && R::IsLive(first) && IsLightRoot(first)) ? first : nullptr;
}

bool CallRunTrigger(void* root, int32_t index) {
    if (!root || !g_runTriggerFn) return false;
    ParamFrame f(g_runTriggerFn);
    if (!f.valid()) return false;
    f.Set<void*>(L"owner", root);   // never read by the BP on any index path; a valid object beats null
    f.Set<int32_t>(L"index", index);
    return Call(root, f);
}

bool ApplyGroupState(void* root, bool on) { return CallRunTrigger(root, on ? 1 : 2); }

bool TryReadBreaker(void* root, bool& open) {
    if (!root || g_breakerOff < 0) return false;
    open = *reinterpret_cast<const bool*>(reinterpret_cast<const char*>(root) + g_breakerOff);
    return true;
}

bool CallSetBreaker(void* root, bool open) {
    if (!root || !g_setActiveFn) return false;
    ParamFrame f(g_setActiveFn);
    if (!f.valid()) return false;
    f.Set<bool>(L"active", open);
    return Call(root, f);
}

namespace {
CachedObjRef g_lampClass;
int32_t g_lampActive = -1;
uint8_t g_lampMask = 0;
}
bool EnsureLampResolved() {
    void* cls = object_index::ClassByName(L"ceilingLamp_C");
    if (!cls) return false;
    if (g_lampClass.Is(cls)) return true;
    g_lampClass.Reset();
    if (!R::FindBoolProperty(cls, L"isActive", g_lampActive, g_lampMask) ||
        g_lampActive < 0 || !g_lampMask) return false;
    g_lampClass.Set(cls);
    return true;
}
bool IsCeilingLamp(void* actor) {
    if (!actor || !EnsureLampResolved()) return false;
    void* base = g_lampClass.Get();
    void* cls = R::ClassOf(actor);
    return base && cls && (cls == base || R::IsDescendantOfAny(cls, &base, 1));
}
bool TryReadLampActive(void* actor, bool& on) {
    if (!R::IsLive(actor) || !IsCeilingLamp(actor)) return false;
    on = (static_cast<const uint8_t*>(actor)[g_lampActive] & g_lampMask) != 0;
    return true;
}
bool ApplyLampActive(void* actor, bool on) {
    if (!R::IsLive(actor) || !IsCeilingLamp(actor)) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(actor), L"setActive");
    if (!fn) return false;
    ParamFrame f(fn);
    if (!f.valid() || !f.Set<bool>(L"isActive", on)) return false;
    CachedObjRef identity; identity.Set(actor);
    return Call(actor, f) && identity.Get() == actor;
}

namespace {
CachedObjRef g_ambientClass, g_ambientComponentClass;
int32_t g_bottomLight = -1, g_ambientVisible = -1;
uint8_t g_ambientVisibleMask = 0;
}
bool EnsureAmbientResolved() {
    void* cls = object_index::ClassByName(L"ambientLight_C");
    if (!cls) return false;
    if (g_ambientClass.Is(cls)) return true;
    g_ambientClass.Reset();
    g_bottomLight = R::FindPropertyOffset(cls, L"bottomLight");
    if (g_bottomLight < 0) return false;
    g_ambientClass.Set(cls);
    return true;
}
bool IsAmbientLight(void* actor) {
    if (!actor || !EnsureAmbientResolved()) return false;
    void* base = g_ambientClass.Get();
    void* cls = R::ClassOf(actor);
    return base && cls && (cls == base || R::IsDescendantOfAny(cls, &base, 1));
}
bool TryReadAmbientActive(void* actor, bool& on) {
    if (!R::IsLive(actor) || !IsAmbientLight(actor)) return false;
    void* component = *reinterpret_cast<void* const*>(static_cast<const char*>(actor) + g_bottomLight);
    if (!R::IsLive(component)) return false;
    void* cls = R::ClassOf(component);
    if (!cls) return false;
    if (!g_ambientComponentClass.Is(cls)) {
        g_ambientComponentClass.Reset();
        if (!R::FindBoolProperty(cls, L"bVisible", g_ambientVisible, g_ambientVisibleMask) ||
            g_ambientVisible < 0 || !g_ambientVisibleMask) return false;
        g_ambientComponentClass.Set(cls);
    }
    on = (static_cast<const uint8_t*>(component)[g_ambientVisible] & g_ambientVisibleMask) != 0;
    return true;
}
bool ApplyAmbientActive(void* actor, bool on) {
    if (!R::IsLive(actor) || !IsAmbientLight(actor)) return false;
    // Do not invoke a partially constructed actor's component setter.
    bool current = false;
    if (!TryReadAmbientActive(actor, current)) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(actor), L"setActive");
    if (!fn) return false;
    ParamFrame f(fn);
    if (!f.valid() || !f.Set<bool>(L"active", on)) return false;
    CachedObjRef identity; identity.Set(actor);
    return Call(actor, f) && identity.Get() == actor;
}

}  // namespace ue_wrap::lightswitch
