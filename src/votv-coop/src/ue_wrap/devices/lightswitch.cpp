// ue_wrap/devices/lightswitch.cpp -- see ue_wrap/devices/lightswitch.h. Engine access for VOTV
// light groups (Atrigger_lightRoot_C). Offsets resolved from the live class via
// reflection, with live class/function identity checks and no guessed offsets.

#include "ue_wrap/devices/lightswitch.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/engine/engine.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace ue_wrap::lightswitch {
namespace {

namespace R = reflection;
UE_FACT_FN(kRowPlaySound2D, "lightswitch", L"GameplayStatics", L"PlaySound2D", Declared);

std::atomic<bool> g_resolved{false};

CachedObjRef g_rootClass, g_runTrigger, g_setActive;
uint64_t g_rootRevision=0;
int32_t g_keyOff     = -1;       // AtriggerBase_C::Key       (Alpha 0.9.0-n: 0x0260)
int32_t g_isActiveOff = -1;      // trigger_lightRoot_C::IsActive (0x02B8)
int32_t g_breakerOff  = -1;      // trigger_lightRoot_C::active, the breaker (distinct from IsActive)
uint8_t g_isActiveMask = 0, g_breakerMask = 0;

// --- The light SWITCH (Alightswitch_C) ---
std::atomic<bool> g_swResolved{false};
CachedObjRef g_switchClass, g_use;
uint64_t g_switchRevision=0;
int32_t g_swKeyOff = -1;      // AtriggerBase_C::Key (shared base offset, 0x0260)
int32_t g_swAOff  = -1;       // Alightswitch_C::A (the flip bool, 0x02A0)
int32_t g_objectsOff = -1;    // triggerBase_C::objects (TArray<UObject*>): objects[0] is the switch's group
uint8_t g_swAMask = 0;

bool RootLayoutCurrent() {
    return g_resolved.load(std::memory_order_acquire) && g_rootClass.Get() && g_runTrigger.Get() && g_setActive.Get() &&
        g_rootClass.Is(R::FindClass(L"trigger_lightRoot_C")) &&
        g_runTrigger.Is(R::FindFunction(g_rootClass.Get(),L"runTrigger")) &&
        g_setActive.Is(R::FindFunction(g_rootClass.Get(),L"SetActive"));
}
bool SwitchLayoutCurrent() {
    return g_swResolved.load(std::memory_order_acquire) && g_switchClass.Get() && g_use.Get() &&
        g_switchClass.Is(R::FindClass(L"lightswitch_C")) && g_use.Is(R::FindFunction(g_switchClass.Get(),L"use"));
}
struct NativeScope {
    CachedObjRef actor,cls,function;
    uint32_t world=world_identity::Generation();
    bool hasFunction=false;
    const uint64_t* revision=nullptr;uint64_t layout=0;
    explicit NativeScope(void* object,void* fn=nullptr,const uint64_t* version=nullptr)
        : hasFunction(fn!=nullptr),revision(version),layout(version?*version:0) {
        actor.Set(object);cls.Set(actor.Get()?R::ClassOf(actor.Get()):nullptr);function.Set(fn);
    }
    bool Current() const {
        return actor.Get() && cls.Get() && R::ClassOf(actor.Get())==cls.Get() &&
            world==world_identity::Generation() && (!hasFunction || function.Get()) && (!revision || *revision==layout);
    }
};

// Native use() plays a UI sound even when replayed across the map. Limit this
// replacement to the synchronous replay; local presses retain their native cue.
void* g_replayedSwitch = nullptr;
const NativeScope* g_replayIdentity = nullptr;
int32_t g_soundOff = -1, g_volumeOff = -1, g_pitchOff = -1;
struct SoundResource {
    CachedObjRef object,cls;
    void Set(void* value){object.Set(value);cls.Set(object.Get()?R::ClassOf(object.Get()):nullptr);}
    bool Current()const{return object.Get() && cls.Get() && R::ClassOf(object.Get())==cls.Get();}
};
SoundResource g_clickAttenuation,g_soundFunction;
bool g_clickHookReady = false;
bool g_buildingAttenuation = false;
uint64_t g_soundRevision = 0;
int32_t g_soundFrameSize = -1;

bool SoundSpan(int32_t offset,size_t bytes,int32_t frame) {
    return offset>=0 && offset<=frame && bytes<=static_cast<size_t>(frame-offset);
}

script_gate::Verdict OnReplaySound(void*, void* source, const uint8_t* params) {
    if (!g_replayedSwitch || source != g_replayedSwitch)
        return script_gate::Verdict::Run;
    const auto* identity=g_replayIdentity;
    const auto function=g_soundFunction,attenuation=g_clickAttenuation;
    const auto revision=g_soundRevision;
    const auto current=[&]{return identity && identity==g_replayIdentity && source==g_replayedSwitch && identity->Current() &&
        revision==g_soundRevision && function.Current() && attenuation.Current() &&
        ufunction_hook::CurrentNativeFunction()==function.object.Get();};
    if(!params || !g_clickHookReady || !current() ||
       !SoundSpan(g_soundOff,sizeof(void*),g_soundFrameSize) ||
       !SoundSpan(g_volumeOff,sizeof(float),g_soundFrameSize) || !SoundSpan(g_pitchOff,sizeof(float),g_soundFrameSize))
        return script_gate::Verdict::Cancel;
    void* cue=nullptr;float volume=0,pitch=0;
    std::memcpy(&cue,params+g_soundOff,sizeof(cue));
    std::memcpy(&volume,params+g_volumeOff,sizeof(volume));
    std::memcpy(&pitch,params+g_pitchOff,sizeof(pitch));
    if(!cue || !R::IsLive(cue) || !std::isfinite(volume) || volume<0 || !std::isfinite(pitch) || pitch<=0)
        return script_gate::Verdict::Cancel;
    SoundResource sound;sound.Set(cue);
    FVector at{};
    if (sound.Current() && engine::TryGetActorLocation(source, at) &&
        std::isfinite(at.X) && std::isfinite(at.Y) && std::isfinite(at.Z) && current() && sound.Current()) {
        engine::PlaySoundAtLocation(source,sound.object.Get(),at,attenuation.object.Get(),volume,pitch);
    }
    // Failure to obtain a position must not fall back to a map-wide UI click.
    return script_gate::Verdict::Cancel;
}

bool EnsureReplaySound() {
    const auto resolved = facts::Resolve(kRowPlaySound2D);
    void* fn = resolved.Fn();
    if (!fn) return false;
    if(!g_clickHookReady || !g_soundFunction.Current() || !g_soundFunction.object.Is(fn)){
        SoundResource function;function.Set(fn);
        if(!function.Current())return false;
        const int32_t frame=R::FunctionFrameSize(fn),sound=R::FindParamOffset(fn,L"Sound"),
            volume=R::FindParamOffset(fn,L"VolumeMultiplier"),pitch=R::FindParamOffset(fn,L"PitchMultiplier");
        if(!SoundSpan(sound,sizeof(void*),frame) || !SoundSpan(volume,sizeof(float),frame) ||
           !SoundSpan(pitch,sizeof(float),frame) || !function.Current() ||
           !ufunction_hook::InstallPreHook(resolved,&OnReplaySound,true) || !function.Current())return false;
        g_soundFunction=function;g_soundFrameSize=frame;g_soundOff=sound;g_volumeOff=volume;g_pitchOff=pitch;
        g_clickHookReady=true;++g_soundRevision;
    }
    if (!g_clickAttenuation.Current() && !g_buildingAttenuation) {
        g_buildingAttenuation=true;
        struct Building {~Building(){g_buildingAttenuation=false;}} building;
        engine::SoundAttenuationConfig cfg{};
        cfg.distanceAlgorithm = 0; // Linear fade from 1 m to silence at 15 m.
        cfg.extents[0] = 100.f;
        cfg.falloffDistance = 1400.f;
        SoundResource candidate;candidate.Set(engine::SpawnSoundAttenuation(cfg));
        if(!g_clickAttenuation.Current() && candidate.Current())g_clickAttenuation=candidate;
    }
    // Visual replay may proceed with a live hook while attenuation is loading:
    // the hook cancels its UI sound until a spatial replacement is available.
    return g_clickHookReady && g_soundFunction.Current() && g_soundFunction.object.Is(fn);
}

struct ReplaySwitchScope {
    void* previous;
    const NativeScope* previousIdentity;
    ReplaySwitchScope(void* sw,const NativeScope& identity) : previous(g_replayedSwitch),previousIdentity(g_replayIdentity) {
        g_replayedSwitch = sw;g_replayIdentity=&identity;
    }
    ~ReplaySwitchScope() { g_replayedSwitch = previous;g_replayIdentity=previousIdentity; }
};

}  // namespace

bool EnsureResolved() {
    if (RootLayoutCurrent()) return true;
    ++g_rootRevision;
    g_resolved.store(false,std::memory_order_release);
    g_rootClass.Reset();g_runTrigger.Reset();g_setActive.Reset();
    g_keyOff=g_isActiveOff=g_breakerOff=-1;g_isActiveMask=g_breakerMask=0;
    void* rootCls = R::FindClass(L"trigger_lightRoot_C");
    if (!rootCls) return false;
    g_rootClass.Set(rootCls); // predicates depend on class, not layout readiness
    const int32_t keyOff=R::FindPropertyOffset(rootCls,L"Key");
    int32_t activeOff=-1,breakerOff=-1;uint8_t activeMask=0,breakerMask=0;
    if(keyOff<0 || !R::FindBoolProperty(rootCls,L"IsActive",activeOff,activeMask) || activeOff<0 || !activeMask ||
       !R::FindBoolProperty(rootCls,L"Active",breakerOff,breakerMask) || breakerOff<0 || !breakerMask)return false;
    void* setActiveFn=R::FindFunction(rootCls,L"SetActive");
    void* runTriggerFn = R::FindFunction(rootCls, L"runTrigger");
    if(!setActiveFn || !runTriggerFn)return false;
    g_setActive.Set(setActiveFn);g_runTrigger.Set(runTriggerFn);
    if(!g_rootClass.Get() || !g_setActive.Get() || !g_runTrigger.Get())return false;
    g_keyOff      = keyOff;
    g_isActiveOff = activeOff;g_isActiveMask=activeMask;
    g_breakerOff=breakerOff;g_breakerMask=breakerMask;
    g_resolved.store(true, std::memory_order_release);
    return RootLayoutCurrent();
}


bool IsLightRoot(void* obj) {
    void* base=g_rootClass.Get();
    if (!obj || !base || !R::IsLive(obj)) return false;
    void* cls = R::ClassOf(obj);
    if (!cls) return false;
    void* bases[1] = { base };
    return R::IsDescendantOfAny(cls, bases, 1);
}

std::wstring GetKeyString(void* root) {
    if (!EnsureResolved() || !IsLightRoot(root)) return {};
    NativeScope scope(root,nullptr,&g_rootRevision);if(!scope.Current())return {};
    const R::FName key = *reinterpret_cast<const R::FName*>(
        reinterpret_cast<const char*>(root) + g_keyOff);
    const auto text=R::ToString(key);
    return scope.Current() && RootLayoutCurrent()?text:std::wstring{};
}

bool TryReadActive(void* root, bool& on) {
    if (!EnsureResolved() || !IsLightRoot(root)) return false;
    NativeScope scope(root,nullptr,&g_rootRevision);if(!scope.Current())return false;
    on = (static_cast<const uint8_t*>(root)[g_isActiveOff] & g_isActiveMask)!=0;
    return true;
}

// --- The light SWITCH (Alightswitch_C) ---

bool EnsureSwitchResolved() {
    if (SwitchLayoutCurrent()) return true;
    ++g_switchRevision;
    g_swResolved.store(false,std::memory_order_release);
    g_switchClass.Reset();g_use.Reset();
    g_swKeyOff=g_swAOff=g_objectsOff=-1;g_swAMask=0;
    void* cls = R::FindClass(L"lightswitch_C");
    if (!cls) return false;
    g_switchClass.Set(cls);
    const int32_t keyOff=R::FindPropertyOffset(cls,L"Key"),objectsOff=R::FindPropertyOffset(cls,L"objects");
    int32_t aOff=-1;uint8_t aMask=0;
    if(keyOff<0 || objectsOff<0 || !R::FindBoolProperty(cls,L"A",aOff,aMask) || aOff<0 || !aMask)return false;
    void* useFn = R::FindFunction(cls, L"use");
    if (!useFn || !g_switchClass.Get())return false;
    g_use.Set(useFn);if(!g_use.Get())return false;
    g_objectsOff=objectsOff;g_swKeyOff=keyOff;g_swAOff=aOff;g_swAMask=aMask;
    g_swResolved.store(true, std::memory_order_release);
    return SwitchLayoutCurrent();
}

bool IsLightSwitch(void* obj) {
    void* base=g_switchClass.Get();
    if (!obj || !base || !R::IsLive(obj)) return false;
    void* cls = R::ClassOf(obj);
    if (!cls) return false;
    void* bases[1] = { base };
    return R::IsDescendantOfAny(cls, bases, 1);
}

std::wstring GetSwitchKeyString(void* sw) {
    if (!EnsureSwitchResolved() || !IsLightSwitch(sw)) return {};
    NativeScope scope(sw,nullptr,&g_switchRevision);if(!scope.Current())return {};
    const R::FName key = *reinterpret_cast<const R::FName*>(
        reinterpret_cast<const char*>(sw) + g_swKeyOff);
    const auto text=R::ToString(key);
    return scope.Current() && SwitchLayoutCurrent()?text:std::wstring{};
}

bool TryReadSwitchA(void* sw, bool& on) {
    if (!EnsureSwitchResolved() || !IsLightSwitch(sw)) return false;
    NativeScope scope(sw,nullptr,&g_switchRevision);if(!scope.Current())return false;
    on = (static_cast<const uint8_t*>(sw)[g_swAOff] & g_swAMask)!=0;
    return true;
}

bool CallUse(void* sw) {
    if (!EnsureSwitchResolved() || !IsLightSwitch(sw)) return false;
    NativeScope identity(sw,g_use.Get(),&g_switchRevision);
    ParamFrame f(identity.function.Get());
    if (!f.valid()) return false;
    if(!EnsureReplaySound() || !identity.Current() || !SwitchLayoutCurrent())return false;
    ReplaySwitchScope scope(sw,identity);
    return Call(sw, f) && identity.Current() && SwitchLayoutCurrent();
}

// --- The GROUP as a synced entity -----------------------------------------

void* SwitchRoot(void* sw) {
    if (!EnsureSwitchResolved() || !IsLightSwitch(sw) || !EnsureResolved())return nullptr;
    NativeScope scope(sw,nullptr,&g_switchRevision);if(!scope.Current())return nullptr;
    const auto* arr = reinterpret_cast<const field_io::TArrayView*>(reinterpret_cast<const char*>(sw) + g_objectsOff);
    const auto snapshot=*arr;
    if(!snapshot.data || snapshot.num<=0 || snapshot.max<snapshot.num || snapshot.num>4096 || snapshot.max>4096)return nullptr;
    void* first=nullptr;std::memcpy(&first,snapshot.data,sizeof(first));
    if(!first || !R::IsLive(first))return nullptr;
    NativeScope target(first,nullptr,&g_rootRevision);
    const bool root=IsLightRoot(first);
    return root && scope.Current() && target.Current() && RootLayoutCurrent() && SwitchLayoutCurrent() &&
        arr->data==snapshot.data && arr->num==snapshot.num && arr->max==snapshot.max &&
        *reinterpret_cast<void* const*>(arr->data)==first?first:nullptr;
}

bool CallRunTrigger(void* root, int32_t index) {
    if (!EnsureResolved() || !IsLightRoot(root))return false;
    NativeScope scope(root,g_runTrigger.Get(),&g_rootRevision);
    ParamFrame f(scope.function.Get());
    if(!f.valid() || !f.Set<void*>(L"owner",root) || !f.Set<int32_t>(L"index",index) ||
       !scope.Current() || !RootLayoutCurrent())return false;
    return Call(root,f) && scope.Current() && RootLayoutCurrent();
}

bool ApplyGroupState(void* root, bool on) { return CallRunTrigger(root, on ? 1 : 2); }

bool TryReadBreaker(void* root, bool& open) {
    if (!EnsureResolved() || !IsLightRoot(root))return false;
    NativeScope scope(root,nullptr,&g_rootRevision);if(!scope.Current())return false;
    open=(static_cast<const uint8_t*>(root)[g_breakerOff] & g_breakerMask)!=0;
    return true;
}

bool CallSetBreaker(void* root, bool open) {
    if (!EnsureResolved() || !IsLightRoot(root))return false;
    NativeScope scope(root,g_setActive.Get(),&g_rootRevision);
    ParamFrame f(scope.function.Get());
    if(!f.valid() || !f.Set<bool>(L"active",open) || !scope.Current() || !RootLayoutCurrent())return false;
    return Call(root,f) && scope.Current() && RootLayoutCurrent();
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
    if (!R::IsLive(actor))return false;
    NativeScope identity(actor);
    if(!identity.Current() || !IsCeilingLamp(actor))return false;
    const auto base=g_lampClass;
    void* fn = R::FindDispatchFunctionCached(identity.cls.Get(), L"setActive");
    if (!fn) return false;
    identity.function.Set(fn);identity.hasFunction=true;
    const auto current=[&]{return identity.Current() && base.Is(g_lampClass.Get()) &&
        base.Is(object_index::ClassByName(L"ceilingLamp_C")) &&
        R::FindDispatchFunctionCached(identity.cls.Get(),L"setActive")==identity.function.Get();};
    ParamFrame f(identity.function.Get());
    if (!f.valid() || !f.Set<bool>(L"isActive", on) || !current()) return false;
    return Call(actor, f) && current();
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
    if (!R::IsLive(actor))return false;
    NativeScope identity(actor);
    if(!identity.Current() || !IsAmbientLight(actor))return false;
    const auto base=g_ambientClass;const auto offset=g_bottomLight;
    void* component = *reinterpret_cast<void* const*>(static_cast<const char*>(actor) + g_bottomLight);
    if (!R::IsLive(component)) return false;
    NativeScope bottom(component);
    void* cls = bottom.cls.Get();
    if (!cls) return false;
    if (!g_ambientComponentClass.Is(cls)) {
        g_ambientComponentClass.Reset();
        if (!R::FindBoolProperty(cls, L"bVisible", g_ambientVisible, g_ambientVisibleMask) ||
            g_ambientVisible < 0 || !g_ambientVisibleMask) return false;
        g_ambientComponentClass.Set(cls);
    }
    const auto bound=[&]{return identity.Current() && bottom.Current() && base.Is(g_ambientClass.Get()) &&
        base.Is(object_index::ClassByName(L"ambientLight_C")) && g_bottomLight==offset &&
        *reinterpret_cast<void* const*>(static_cast<const char*>(actor)+offset)==component;};
    if(!bound())return false;
    const bool value = (static_cast<const uint8_t*>(component)[g_ambientVisible] & g_ambientVisibleMask) != 0;
    if(!bound())return false;
    on=value;
    return true;
}
bool ApplyAmbientActive(void* actor, bool on) {
    if (!R::IsLive(actor))return false;
    NativeScope identity(actor);
    if(!identity.Current() || !IsAmbientLight(actor))return false;
    const auto base=g_ambientClass;const auto offset=g_bottomLight;
    void* component=*reinterpret_cast<void* const*>(static_cast<const char*>(actor)+offset);
    if(!R::IsLive(component))return false;
    NativeScope bottom(component);
    // Do not invoke a partially constructed actor's component setter.
    bool current = false;
    if (!TryReadAmbientActive(actor, current)) return false;
    void* fn = R::FindDispatchFunctionCached(identity.cls.Get(), L"setActive");
    if (!fn) return false;
    identity.function.Set(fn);identity.hasFunction=true;
    const auto bound=[&]{return bottom.Current() &&
        *reinterpret_cast<void* const*>(static_cast<const char*>(actor)+offset)==component;};
    const auto ready=[&]{return identity.Current() && base.Is(g_ambientClass.Get()) &&
        base.Is(object_index::ClassByName(L"ambientLight_C")) && g_bottomLight==offset && bound() &&
        R::FindDispatchFunctionCached(identity.cls.Get(),L"setActive")==identity.function.Get();};
    ParamFrame f(identity.function.Get());
    if (!f.valid() || !f.Set<bool>(L"active", on) || !ready()) return false;
    return Call(actor, f) && ready();
}

}  // namespace ue_wrap::lightswitch
