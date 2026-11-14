// ue_wrap/devices/atv.cpp -- see ue_wrap/devices/atv.h. Engine access for the ATV/quadbike
// (AATV_C). Offsets are resolved from the live class through reflection, with the Alpha
// 0.9.0-n values kept as logged fallbacks. Transform reads and writes go through
// ue_wrap::engine at the actor level: the root Mesh IS the actor root, so the actor transform
// is the physics body transform.

#include "ue_wrap/devices/atv.h"

#include "ue_wrap/actors/save_record.h" // the loaded world save's object records (Fstruct_save)
#include "ue_wrap/core/call.h"      // ParamFrame / Call -- the GameplayStatics spawn (SpawnMirror)
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/fname_utils.h"  // StringToFName -- SpawnMirror's key
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/world/media_player.h"
#include "ue_wrap/engine/engine_component.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/world_singleton.h" // gamemode, for the key map and the save slot
#include "ue_wrap/core/types.h"     // FTransform (SpawnMirror)
#include "ue_wrap/core/trace.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace ue_wrap::atv {
namespace {

namespace R = reflection;

std::atomic<bool> g_resolved{false};
int32_t g_keyOff      = -1;       // Key      (Alpha 0.9.0-n: 0x0618, FName)
int32_t g_playerOff   = -1;       // Player   (0x05B0, AmainPlayer_C*)
int32_t g_isDrivenOff = -1;       // isDriven (0x05F7, bool)
int32_t g_fuelOff     = -1;       // fuel     (0x05D4, float)
int32_t g_healthOff   = -1;       // health   (0x05E4, float)
int32_t g_brakeOff    = -1;       // Brake    (0x05D9, bool)
int32_t g_brakeByte   = -1;
uint8_t g_brakeMask   = 0;
int32_t g_lightsByte  = -1;
uint8_t g_lightsMask  = 0;
int32_t g_turboByte   = -1;
uint8_t g_turboMask   = 0;
int32_t g_radioCompOff = -2;   // ATV_C.radio, the ChildActorComponent
int32_t g_mapCompOff   = -2;   // ATV_C.digitalMap
int32_t g_childOff     = -2;   // ChildActorComponent.ChildActor
int32_t g_mediaOff     = -2;   // the radio actor's mediaPlayer
int32_t g_hasMapByte   = -2;
uint8_t g_hasMapMask   = 0;
bool    g_hasMapSaid   = false;
int32_t g_moduleOff[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
const wchar_t* const kModuleNames[8] = {
    L"module_aircontrol", L"module_belt", L"module_bigLights", L"module_container",
    L"module_floaties", L"module_front", L"module_radio", L"module_solar"
};
void* g_honkFn = nullptr;
void* g_setBrakeFn = nullptr;   // applies the brake bool to the constraint
void* g_loadLightsFn = nullptr; // applies the lights bool to the lamps
void* g_setKeyFn = nullptr;
int32_t g_skipByte = -1;        // skipSave, else ignoreSave
uint8_t g_skipMask = 0;
int32_t g_trapOff     = -1;       // trap     (a bool bit: the byte, with its mask below)
uint8_t g_trapMask    = 0;
int32_t g_tpOff       = -1;       // tp       (a UBillboardComponent*: where `explode` places its blast)

int32_t ResolveOff(void* cls, const wchar_t* name, int32_t fallback) {
    int32_t o = R::FindPropertyOffset(cls, name);
    if (o < 0) {
        UE_LOGW("atv: %ls offset not found -- using fallback 0x%04X", name, fallback);
        o = fallback;
    }
    return o;
}

// ATV_C as the object index holds it, looked up where it is used -- one lookup that walks nothing --
// and never kept: a Blueprint class can come back as a new object after a world change (127 did in
// a same-process rejoin, ATV_C not among them), and one still loading answers null here. The field
// offsets above are its layout, the same in every world, so they are resolved once.
void* AtvClass() { return object_index::ClassByName(L"ATV_C"); }

}  // namespace

bool EnsureResolved() {
    if (g_resolved.load(std::memory_order_acquire)) return true;

    void* cls = AtvClass();
    if (!cls) return false;

    g_keyOff      = ResolveOff(cls, L"Key",      0x0618);
    g_playerOff   = ResolveOff(cls, L"Player",   0x05B0);
    g_isDrivenOff = ResolveOff(cls, L"isDriven", 0x05F7);
    g_fuelOff     = ResolveOff(cls, L"fuel",     0x05D4);
    g_healthOff   = ResolveOff(cls, L"health",   0x05E4);
    g_brakeOff    = ResolveOff(cls, L"Brake",    0x05D9);
    // The handbrake flag is `brake`. `Brake` is the old guessed name; a whole-byte fallback
    // would clobber the neighbours in that byte, so it is only used when neither name resolves.
    if (!R::FindBoolProperty(cls, L"brake", g_brakeByte, g_brakeMask) &&
        !R::FindBoolProperty(cls, L"Brake", g_brakeByte, g_brakeMask)) {
        g_brakeByte = g_brakeOff;
        g_brakeMask = 0xFF;
    }
    R::FindBoolProperty(cls, L"lights", g_lightsByte, g_lightsMask);
    R::FindBoolProperty(cls, L"turbo", g_turboByte, g_turboMask);
    // `radio` is the radio actor and `hasRadio` is the upgrade. Playing is the radio's
    // MediaPlayer, resolved when a scooter is asked. `hasMap` is the map upgrade.
    if (!R::FindBoolProperty(cls, L"hasMap", g_hasMapByte, g_hasMapMask) && !g_hasMapSaid) {
        g_hasMapSaid = true;
        g_hasMapByte = -1;
        UE_LOGW("atv: hasMap did not resolve -- the map upgrade is not streamed");
    }
    g_radioCompOff = R::FindPropertyOffset(cls, L"radio");
    g_mapCompOff   = R::FindPropertyOffset(cls, L"digitalMap");
    for (int i = 0; i < 8; ++i) g_moduleOff[i] = R::FindPropertyOffset(cls, kModuleNames[i]);
    g_honkFn = R::FindFunction(cls, L"InpActEvt_atv_honk_K2Node_InputActionEvent");
    g_setBrakeFn = R::FindFunction(cls, L"setBrake");
    g_loadLightsFn = R::FindFunction(cls, L"loadLights");
    // setKey is often declared on the save parent, and FindFunction does not climb.
    g_setKeyFn = R::FindDispatchFunction(cls, L"setKey", nullptr);
    if (!R::FindBoolProperty(cls, L"skipSave", g_skipByte, g_skipMask))
        R::FindBoolProperty(cls, L"ignoreSave", g_skipByte, g_skipMask);
    // No fallback: a miss leaves the trap host-only, as it was before it travelled. The flag is a bit packed with its
    // neighbours (inWater, floater), so its byte and mask are both resolved.
    if (!R::FindBoolProperty(cls, L"trap", g_trapOff, g_trapMask) || g_trapOff < 0 || g_trapMask == 0) {
        g_trapOff = -1;
        UE_LOGW("atv: trap offset not found -- the story event's trap stays host-only");
    }
    // No fallback: a miss leaves a client's ATV explosion unrequested, since the host has no place to put it.
    g_tpOff = R::FindPropertyOffset(cls, L"tp");
    if (g_tpOff < 0) UE_LOGW("atv: tp offset not found -- a client's ATV explosion cannot be requested");

    g_resolved.store(true, std::memory_order_release);
    UE_LOGI("atv: resolved ATV_C=%p Key@0x%04X Player@0x%04X isDriven@0x%04X fuel@0x%04X "
            "health@0x%04X brake@%d/%02X lights@%d/%02X turbo@%d/%02X hasMap@%d/%02X "
            "radioComp@%d mapComp@%d honk=%p setBrake=%p loadLights=%p",
            cls, g_keyOff, g_playerOff, g_isDrivenOff, g_fuelOff, g_healthOff,
            g_brakeByte, static_cast<unsigned>(g_brakeMask),
            g_lightsByte, static_cast<unsigned>(g_lightsMask),
            g_turboByte, static_cast<unsigned>(g_turboMask),
            g_hasMapByte, static_cast<unsigned>(g_hasMapMask),
            g_radioCompOff, g_mapCompOff, g_honkFn, g_setBrakeFn, g_loadLightsFn);
    return true;
}

bool IsAtv(void* obj) {
    if (!obj) return false;
    void* atvCls = AtvClass();
    if (!atvCls) return false;
    void* cls = R::ClassOf(obj);
    return cls && R::IsDescendantOfAny(cls, &atvCls, 1);
}

size_t ForEachAtv(void (*fn)(void* ctx, void* actor, int32_t idx), void* ctx) {
    if (!fn) return 0;
    // The classes first: the instance walk does not run from inside the class walk.
    std::vector<void*> classes;
    object_index::ForEachClass([](void* c, void* cls, void* anyInstance) {
        if (IsAtv(anyInstance)) static_cast<std::vector<void*>*>(c)->push_back(cls);
    }, &classes);
    struct Walk { void (*fn)(void*, void*, int32_t); void* ctx; size_t loading; } walk{fn, ctx, 0};
    for (void* cls : classes) {
        object_index::ForEachInstance(cls, [](void* p, void* obj, int32_t idx) {
            Walk* w = static_cast<Walk*>(p);
            const auto flags = R::SlotFlags(idx);
            if (flags & R::slot_flags::Dying) return;   // leaving: not counted
            if (flags & R::slot_flags::NotYetReadable) { ++w->loading; return; }
            if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
            w->fn(w->ctx, obj, idx);
        }, &walk);
    }
    return walk.loading;
}

std::wstring GetKeyString(void* atv) {
    if (!atv || g_keyOff < 0) return std::wstring();
    const R::FName& key = *reinterpret_cast<const R::FName*>(
        reinterpret_cast<const char*>(atv) + g_keyOff);
    return R::ToString(key);
}

struct CountCtx { void* atv; void* world; int n; };

void CountOne(void* ctx, void* obj, int32_t) {
    auto* c = static_cast<CountCtx*>(ctx);
    if (!obj || !R::IsLive(obj)) return;
    if (c->world && world_identity::WorldOf(obj) != c->world) return;
    const std::wstring nm = R::ToString(R::NameOf(obj));
    if (nm.rfind(L"Default__", 0) == 0) return;
    ++c->n;
}

struct ClassPick { void* atv; std::vector<void*>* out; };

void CollectAtvClass(void* ctx, void* cls, void*) {
    auto* p = static_cast<ClassPick*>(ctx);
    if (!cls || !p->atv || !R::IsDescendantOfAny(cls, &p->atv, 1)) return;
    p->out->push_back(cls);
}

int CountLiveSettled() {
    if (!object_index::IsSeeded() || object_index::Backlog() > 0) return -1;
    void* world = world_identity::CurrentWorld();
    if (!world) return -1;
    void* cls = AtvClass();
    if (!cls) return -1;
    // The class set moves when a class gains its first instance or loses its last, which is
    // exactly when the list of scooter classes can change. A second scooter of a class already
    // listed does not move it, and the instance walk below still sees that birth. Asking every
    // class in the world on the ticks spent waiting for the save scooter to sit still is the
    // part that does not have to repeat.
    static uint64_t sVer = ~uint64_t{0};
    static void* sRoot = nullptr;
    static std::vector<void*> sClasses;
    const uint64_t ver = object_index::ClassSetVersion();
    if (ver != sVer || cls != sRoot) {
        sClasses.clear();
        ClassPick pick{ cls, &sClasses };
        object_index::ForEachClass(&CollectAtvClass, &pick);
        sVer = ver;
        sRoot = cls;
    }
    CountCtx c{ cls, world, 0 };
    for (void* atvCls : sClasses)
        object_index::ForEachInstance(atvCls, &CountOne, &c);
    return c.n;
}

bool GetRootTransform(void* atv, FVector& loc, FRotator& rot) {
    return atv && engine::TryGetActorLocation(atv, loc) && engine::TryGetActorRotation(atv, rot);
}

void* GetOccupantPlayer(void* atv) {
    if (!atv || g_playerOff < 0) return nullptr;
    return *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(atv) + g_playerOff);
}

bool IsDriven(void* atv) {
    if (!atv || g_isDrivenOff < 0) return false;
    return *reinterpret_cast<const bool*>(reinterpret_cast<const char*>(atv) + g_isDrivenOff);
}

float GetFuel(void* atv) {
    if (!atv || g_fuelOff < 0) return 0.f;
    return *reinterpret_cast<const float*>(reinterpret_cast<const char*>(atv) + g_fuelOff);
}

float GetHealth(void* atv) {
    if (!atv || g_healthOff < 0) return 0.f;
    return *reinterpret_cast<const float*>(reinterpret_cast<const char*>(atv) + g_healthOff);
}

bool ReadFlag(void* atv, int32_t byte, uint8_t mask) {
    if (!atv || byte < 0 || mask == 0) return false;
    const uint8_t b = *reinterpret_cast<const uint8_t*>(reinterpret_cast<const char*>(atv) + byte);
    return (b & mask) != 0;
}

void WriteFlag(void* atv, int32_t byte, uint8_t mask, bool on) {
    if (!atv || byte < 0 || mask == 0) return;
    uint8_t& b = *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(atv) + byte);
    if (on) b = static_cast<uint8_t>(b | mask);
    else    b = static_cast<uint8_t>(b & ~mask);
}

void SetIncludedInSave(void* atv, bool included) {
    if (!atv || !EnsureResolved()) return;
    WriteFlag(atv, g_skipByte, g_skipMask, !included);
}

bool WriteSaveKey(void* atv, const std::wstring& key) {
    if (!atv || key.empty() || key == L"None" || !EnsureResolved() || !g_setKeyFn) return false;
    const R::FName minted = fname_utils::StringToFName(key);
    if (minted.ComparisonIndex == 0) return false;
    ParamFrame f(g_setKeyFn);
    const bool wrote = f.SetRaw(L"Key", &minted, static_cast<int32_t>(sizeof(minted)))
                    || f.SetRaw(L"key", &minted, static_cast<int32_t>(sizeof(minted)));
    if (!wrote || !Call(atv, f)) return false;
    const std::wstring now = GetKeyString(atv);
    return now == key;
}

struct FindCtx { void* atv; void* world; std::wstring key; void* skip; void* found; };

void FindOne(void* ctx, void* obj, int32_t) {
    auto* c = static_cast<FindCtx*>(ctx);
    if (c->found || !obj || obj == c->skip || !R::IsLive(obj)) return;
    if (c->world && world_identity::WorldOf(obj) != c->world) return;
    const std::wstring nm = R::ToString(R::NameOf(obj));
    if (nm.rfind(L"Default__", 0) == 0) return;
    if (GetKeyString(obj) == c->key) c->found = obj;
}

void FindClass(void* ctx, void* cls, void*) {
    auto* c = static_cast<FindCtx*>(ctx);
    if (c->found || !cls || !c->atv || !R::IsDescendantOfAny(cls, &c->atv, 1)) return;
    object_index::ForEachInstance(cls, &FindOne, ctx);
}

void* FindLiveIn(const std::wstring& key, void* skip) {
    if (key.empty() || key == L"None" || !object_index::IsSeeded()) return nullptr;
    void* world = world_identity::CurrentWorld();
    void* cls = AtvClass();
    if (!world || !cls) return nullptr;
    FindCtx c{ cls, world, key, skip, nullptr };
    object_index::ForEachClass(&FindClass, &c);
    return c.found;
}

void* FindLiveByKey(const std::wstring& key) { return FindLiveIn(key, nullptr); }

void* FindOtherLiveByKey(const std::wstring& key, void* self) { return FindLiveIn(key, self); }

// The game's key map (mainGamemode_C::getObjectFromKey), including an entry that is not a live
// actor yet. A stale entry after a destroy is possible; the caller treats any other object as
// "this key is taken", which refuses coopatv1 rather than sharing it. Null when the map cannot
// be asked or does not hold the key.
void* KeyMapObject(const wchar_t* key) {
    if (!key || !*key || std::wstring(key) == L"None") return nullptr;
    void* gm = world_singleton::Gamemode();
    if (!gm) return nullptr;
    void* fn = R::FindFunction(R::ClassOf(gm), L"getObjectFromKey");
    if (!fn) return nullptr;
    ParamFrame f(fn);
    if (!f.valid()) return nullptr;
    const R::FName n = fname_utils::StringToFName(key);
    if (n.ComparisonIndex == 0) return nullptr;
    if (!f.SetRaw(L"ItemToFind", &n, static_cast<int32_t>(sizeof(n)))) return nullptr;
    if (!Call(gm, f)) return nullptr;
    void* out = f.Get<void*>(L"Output");
    return out;
}

// One walk of the loaded save. The scooter count and a key lookup share it, so a tick that
// asks both does not walk twice. A slot that cannot be read leaves the cache cold.
struct SaveScan {
    uint32_t gen = ~uint32_t{0};
    bool counted = false;
    int atvRecords = -1;
    int32_t arrNum = -1;   // objectsData length of the cached walk; -1 has not been read
    std::wstring key;
    bool keyKnown = false;
    bool named = false;
};

SaveScan& SaveScanCache() {
    static SaveScan s;
    return s;
}

bool ReadSaveScan(const wchar_t* key) {
    SaveScan& s = SaveScanCache();
    const uint32_t gen = world_identity::Generation();
    if (gen != s.gen) {
        s = SaveScan{};
        s.gen = gen;
    }
    const bool wantKey = key && *key && std::wstring(key) != L"None";
    void* gm = world_singleton::Gamemode();
    if (!gm || !R::IsLive(gm)) return false;
    void* slot = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(gm) + profile::off::AmainGamemode_saveSlot);
    if (!slot || !R::IsLive(slot)) return false;
    namespace SR = save_record;
    const SR::Arr arr = SR::ReadArr(slot, static_cast<int32_t>(profile::off::UsaveSlot_objectsData));
    const int32_t n = (arr.data && arr.num > 0) ? arr.num : 0;
    // The world generation does not change when the slot's array is filled or a scooter record
    // is appended. A walk that saw an empty list would otherwise answer "no scooter" for the
    // rest of the level, and the companion watch would close before the save twin existed.
    if (s.arrNum != n) {
        s.counted = false;
        s.keyKnown = false;
        s.named = false;
        s.arrNum = n;
    }
    const bool needCount = !s.counted;
    const bool needKey = wantKey && (!s.keyKnown || s.key != key);
    if (!needCount && !needKey) return true;
    if (needCount) { s.counted = true; s.atvRecords = 0; }
    if (needKey) { s.keyKnown = true; s.key = key; s.named = false; }
    if (!arr.data || arr.num <= 0) return true;
    void* atvCls = AtvClass();
    // counted is already set. A missing class would store zero scooters for this array length
    // and the watch would treat the save as empty. Leave the count cold and try next tick.
    if (needCount && !atvCls) {
        s.counted = false;
        if (needKey) { s.keyKnown = false; s.named = false; }
        return false;
    }
    // Fstruct_save::key. The same byte save_record and the save-identity walk read.
    constexpr int32_t kSaveKey = 0x40;
    for (int32_t i = 0; i < arr.num; ++i) {
        const uint8_t* e = arr.data + static_cast<size_t>(i) * static_cast<size_t>(SR::kSaveStride);
        if (needCount) {
            void* cls = nullptr;
            std::memcpy(&cls, e, sizeof(cls));
            if (atvCls && cls && R::IsLive(cls) && R::IsDescendantOfAny(cls, &atvCls, 1))
                ++s.atvRecords;
        }
        if (!needKey) continue;
        R::FName fn{};
        std::memcpy(&fn, e + kSaveKey, sizeof(fn));
        if (fn.ComparisonIndex == 0 || fn.ComparisonIndex > 0x0FFFFFFF) continue;
        if (R::NameEquals(fn, key)) s.named = true;
    }
    return true;
}

bool LoadedSaveNamesKey(const wchar_t* key) {
    if (!key || !*key || std::wstring(key) == L"None") return false;
    if (!ReadSaveScan(key)) return false;
    return SaveScanCache().named;
}

int LoadedSaveAtvCount() {
    if (!ReadSaveScan(nullptr)) return -1;
    return SaveScanCache().atvRecords;
}

// Live actor, key map (even when that object is not live yet), or the loaded save's records.
bool OtherOwnsSaveKey(void* self, const wchar_t* key) {
    if (!key || !*key || std::wstring(key) == L"None") return false;
    if (FindOtherLiveByKey(key, self)) return true;
    void* mapped = KeyMapObject(key);
    if (mapped && mapped != self) return true;
    return LoadedSaveNamesKey(key);
}

std::wstring EnsureOwnSaveKey(void* atv) {
    if (!atv || !EnsureResolved()) return std::wstring();
    SetIncludedInSave(atv, true);
    std::wstring key = GetKeyString(atv);
    if (!key.empty() && key != L"None") {
        if (!OtherOwnsSaveKey(atv, key.c_str())) {
            UE_LOGI("atv: companion keeps its own save key '%ls'", key.c_str());
            return key;
        }
        // Already written, and another scooter owns it -- including one the save names that is
        // not a live actor yet. Move off it, or hand back empty so the caller removes this body.
        UE_LOGW("atv: companion key '%ls' is already owned by another scooter", key.c_str());
        if (key != L"coopatv2" && !OtherOwnsSaveKey(atv, L"coopatv2") && WriteSaveKey(atv, L"coopatv2")) {
            const std::wstring moved = GetKeyString(atv);
            UE_LOGI("atv: companion save key moved to '%ls'", moved.c_str());
            return moved;
        }
        return std::wstring();
    }
    // coopatv1 is the companion's own record. If some other scooter already holds it, coopatv2
    // is the next free one. Writing the taken key would fold both scooters into one save slot.
    const wchar_t* chosen = L"coopatv1";
    if (OtherOwnsSaveKey(atv, chosen)) chosen = L"coopatv2";
    if (OtherOwnsSaveKey(atv, chosen)) {
        UE_LOGW("atv: setKey(%ls) refused -- another scooter already owns that save key", chosen);
        return std::wstring();
    }
    if (!WriteSaveKey(atv, chosen)) {
        UE_LOGW("atv: setKey(%ls) failed -- a companion with Key None shares the other scooter's save record",
                chosen);
        return GetKeyString(atv);
    }
    key = GetKeyString(atv);
    UE_LOGI("atv: companion save key is '%ls'", key.c_str());
    return key;
}

bool CompShown(void* comp) {
    if (!comp || !R::IsLive(comp)) return false;
    const uint8_t hidden = *reinterpret_cast<const uint8_t*>(
        reinterpret_cast<const uint8_t*>(comp) + ue_wrap::profile::off::USceneComponent_HiddenFlagsByte);
    return (hidden & 0x04) == 0;
}

void CallApply(void* atv, void* fn) {
    if (!atv || !fn) return;
    ParamFrame f(fn);
    if (f.valid()) Call(atv, f);
}

bool GetBrake(void* atv) { return ReadFlag(atv, g_brakeByte, g_brakeMask); }
bool GetLights(void* atv) { return ReadFlag(atv, g_lightsByte, g_lightsMask); }
bool GetTurbo(void* atv) { return ReadFlag(atv, g_turboByte, g_turboMask); }
void* ComponentAt(void* atv, int32_t off) {
    if (!atv || off < 0) return nullptr;
    void* c = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(atv) + off);
    return (c && R::IsLive(c)) ? c : nullptr;
}

void* ChildActorOf(void* component) {
    if (!component) return nullptr;
    if (g_childOff == -2) g_childOff = R::FindPropertyOffset(R::ClassOf(component), L"ChildActor");
    if (g_childOff < 0) return nullptr;
    void* child = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(component) + g_childOff);
    return (child && R::IsLive(child)) ? child : nullptr;
}

void* RadioActor(void* atv) {
    if (!EnsureResolved()) return nullptr;
    return ChildActorOf(ComponentAt(atv, g_radioCompOff));
}

void* RadioMedia(void* atv) {
    void* radio = RadioActor(atv);
    if (!radio) return nullptr;
    if (g_mediaOff == -2) g_mediaOff = R::FindPropertyOffset(R::ClassOf(radio), L"mediaPlayer");
    if (g_mediaOff < 0) return nullptr;
    void* player = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(radio) + g_mediaOff);
    return (player && R::IsLive(player)) ? player : nullptr;
}

bool GetRadio(void* atv) {
    void* player = RadioMedia(atv);
    return player && ue_wrap::media_player::IsPlaying(player);
}

std::wstring RadioUrl(void* atv) {
    void* player = RadioMedia(atv);
    return player ? ue_wrap::media_player::Url(player) : std::wstring();
}

bool GetHasMap(void* atv) { return ReadFlag(atv, g_hasMapByte, g_hasMapMask); }

void SetHasMap(void* atv, bool on) {
    if (!atv || !EnsureResolved() || g_hasMapByte < 0) return;
    WriteFlag(atv, g_hasMapByte, g_hasMapMask, on);
    void* map = ChildActorOf(ComponentAt(atv, g_mapCompOff));
    if (map) ue_wrap::engine::SetActorHiddenInGame(map, !on);
}
void SetBrake(void* atv, bool on) {
    if (g_brakeMask == 0 || GetBrake(atv) == on) return;
    WriteFlag(atv, g_brakeByte, g_brakeMask, on);
    CallApply(atv, g_setBrakeFn);  // the bool alone leaves the constraint where it was
}
void SetLights(void* atv, bool on) {
    if (g_lightsMask == 0 || GetLights(atv) == on) return;
    WriteFlag(atv, g_lightsByte, g_lightsMask, on);
    CallApply(atv, g_loadLightsFn);
}
void SetTurbo(void* atv, bool on) { WriteFlag(atv, g_turboByte, g_turboMask, on); }
void SetRadio(void* atv, bool on) {
    void* player = RadioMedia(atv);
    if (!player) return;
    ue_wrap::media_player::Drive(player, nullptr, on);
}

void DriveRadio(void* atv, const wchar_t* url, bool playing) {
    void* player = RadioMedia(atv);
    if (!player) return;
    ue_wrap::media_player::Drive(player, url, playing);
}

uint8_t ReadModuleMask(void* atv) {
    if (!atv) return 0;
    uint8_t mask = 0;
    auto* base = reinterpret_cast<uint8_t*>(atv);
    for (int i = 0; i < 8; ++i) {
        if (g_moduleOff[i] < 0) continue;
        void* comp = *reinterpret_cast<void**>(base + g_moduleOff[i]);
        if (CompShown(comp)) mask = static_cast<uint8_t>(mask | (1u << i));
    }
    return mask;
}

void ApplyModuleMask(void* atv, uint8_t mask) {
    if (!atv) return;
    auto* base = reinterpret_cast<uint8_t*>(atv);
    for (int i = 0; i < 8; ++i) {
        if (g_moduleOff[i] < 0) continue;
        void* comp = *reinterpret_cast<void**>(base + g_moduleOff[i]);
        if (!comp || !R::IsLive(comp)) continue;
        const bool show = (mask & (1u << i)) != 0;
        if (CompShown(comp) == show) continue;
        engine::SetComponentVisible(comp, show, true);
    }
}

void PlayHonk(void* atv) {
    if (!atv || !g_honkFn) return;
    ParamFrame f(g_honkFn);
    if (f.valid()) Call(atv, f);
}

bool EjectWheel(void* atv, int index) {
    if (!atv || index < 0 || index > 3 || !EnsureResolved()) return false;
    void* cls = AtvClass();
    void* fnGet = cls ? R::FindFunction(cls, L"getTire") : nullptr;
    void* fnDmg = cls ? R::FindFunction(cls, L"damageWheel") : nullptr;
    if (!fnGet || !fnDmg) return false;
    ParamFrame tire(fnGet);
    if (!tire.valid()) return false;
    tire.Set(L"index", index);
    if (!Call(atv, tire)) return false;
    void* component = tire.Get<void*>(L"ReturnValue");
    if (!component) return false;
    ParamFrame dmg(fnDmg);
    if (!dmg.valid()) return false;
    dmg.Set(L"index", index);
    dmg.Set(L"damage", 200.f);
    dmg.Set(L"component", component);
    return Call(atv, dmg);
}

bool ReadTrap(void* atv, bool& out) {
    if (!atv || g_trapOff < 0) return false;
    out = (*(reinterpret_cast<const uint8_t*>(atv) + g_trapOff) & g_trapMask) != 0;
    return true;
}

bool WriteTrap(void* atv, bool v) {
    if (!atv || g_trapOff < 0) return false;
    uint8_t& byte = *(reinterpret_cast<uint8_t*>(atv) + g_trapOff);
    byte = v ? static_cast<uint8_t>(byte | g_trapMask) : static_cast<uint8_t>(byte & ~g_trapMask);
    return true;
}

bool ReadTpLocation(void* atv, FVector& out) {
    out = FVector{};
    if (!atv || g_tpOff < 0) return false;
    void* tp = *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(atv) + g_tpOff);
    return tp && engine::TryGetComponentLocation(tp, out);
}

bool TeleportRig(void* atv, const FVector& loc, const FRotator& rot) {
    const FVector target=loc;const FRotator facing=rot;
    for(float v:{target.X,target.Y,target.Z,facing.Pitch,facing.Yaw,facing.Roll})if(!std::isfinite(v))return false;
    if (!atv || !EnsureResolved() || !R::IsLive(atv) || !IsAtv(atv)) return false;
    CachedObjRef actor,cls,function;actor.Set(atv);cls.Set(R::ClassOf(atv));
    const auto generation=world_identity::Generation();void* world=world_identity::CurrentWorld();
    // The memoised lookup, on the rig's own class: a warp comes at up to 20 Hz and a hit renders no
    // name, and the answer is held by slot and serial, so a class a world change replaced is asked
    // again and a miss is held as a miss while its class lives.
    void* const fn = R::FindDispatchFunctionCached(cls.Get(), L"teleportVehicle");
    if (!fn) {
        static bool sWarned = false;
        if (!sWarned) {
            sWarned = true;
            UE_LOGE("atv: teleportVehicle unresolved -- the rig can never be warped; "
                    "a mirror that drifts past the threshold will not be re-placed");
        }
        return false;
    }
    function.Set(fn);
    const auto current=[&]{return actor.Get() && cls.Get() && function.Get() && world &&
        generation==world_identity::Generation() && world==world_identity::CurrentWorld() &&
        world_identity::WorldOf(atv)==world && R::ClassOf(atv)==cls.Get() &&
        R::FindDispatchFunctionCached(cls.Get(),L"teleportVehicle")==function.Get();};
    if(!current())return false;
    ParamFrame f(function.Get());
    if (!f.valid()) return false;
    if (!f.Set<FVector>(L"NewLocation", target) || !f.Set<FRotator>(L"NewRotation", facing) || !current()) return false;
    return Call(atv, f) && current();
}

const char* SpawnFailText(SpawnFail why) {
    switch (why) {
        case SpawnFail::None:          return "no failure";
        case SpawnFail::Unresolved:    return "the ATV wrapper, the spawn functions or the world are unresolved";
        case SpawnFail::NoKey:         return "the class name or the key is empty";
        case SpawnFail::ClassNotFound: return "the class is not loaded";
        case SpawnFail::NotAnAtv:      return "the class is not an ATV subclass";
        case SpawnFail::NoOffset:      return "the class has no key property";
        case SpawnFail::NameNone:      return "the key did not convert to a name";
        case SpawnFail::BeginFailed:   return "the deferred spawn failed";
        case SpawnFail::FinishFailed:  return "finishing the spawn failed";
        case SpawnFail::KeyMismatch:   return "the mirror was born under another key";
        case SpawnFail::InvalidPose:   return "the requested pose is non-finite";
        case SpawnFail::Retired:       return "captured spawn dependencies were retired";
        case SpawnFail::PlacementFailed:return "the whole rig could not be placed";
    }
    return "unknown";
}

// The key is written as a raw FName into the deferred actor, before its finish: the construction script
// runs in the finish and, finding a key, keeps it and registers it; finding none, it mints a random one.
// A raw write because ATV_C's own setKey is an empty stub in the Blueprint (its entry is a bare return),
// so the props lane's setKey call (coop/props/prop_fresh_spawn.cpp, the deferred key write) writes nothing on
// an ATV. The offset and the FName are resolved before anything is spawned; a miss spawns nothing,
// never a mirror with a random key. A failure sets `why` and logs nothing: the caller says it once per key.
void* SpawnMirror(const std::wstring& className, const FVector& loc, const FRotator& rot,
                  const std::wstring& key, SpawnFail* why, std::wstring* bornKey) {
    SpawnFail local = SpawnFail::None;
    SpawnFail& fail = why ? *why : local;
    fail = SpawnFail::None;
    const std::wstring wantedClass=className,wantedKey=key;
    const FVector target=loc;const FRotator facing=rot;
    for(float v:{target.X,target.Y,target.Z,facing.Pitch,facing.Yaw,facing.Roll})
        if(!std::isfinite(v)){fail=SpawnFail::InvalidPose;return nullptr;}
    if (!EnsureResolved()) { fail = SpawnFail::Unresolved; return nullptr; }
    if (wantedClass.empty() || wantedKey.empty()) { fail = SpawnFail::NoKey; return nullptr; }
    const auto generation=world_identity::Generation();void* const world=world_identity::CurrentWorld();
    CachedObjRef context,contextClass;context.Set(engine::GetWorldContext());
    contextClass.Set(context.Get()?R::ClassOf(context.Get()):nullptr);
    if(!world || !context.Get() || !contextClass.Get()){fail=SpawnFail::Unresolved;return nullptr;}
    const auto worldCurrent=[&]{return generation==world_identity::Generation() && world==world_identity::CurrentWorld() &&
        context.Get() && contextClass.Get() && R::ClassOf(context.Get())==contextClass.Get() && engine::GetWorldContext()==context.Get();};
    void* actorClass = R::FindClass(wantedClass.c_str());
    if (!actorClass) { fail = SpawnFail::ClassNotFound; return nullptr; }
    // Trust boundary: the className arrives over the wire -- only spawn ATV-lineage classes.
    void* atvCls = AtvClass();
    if (!atvCls || !R::IsDescendantOfAny(actorClass, &atvCls, 1)) { fail = SpawnFail::NotAnAtv; return nullptr; }
    CachedObjRef requestedClass,baseClass;requestedClass.Set(actorClass);baseClass.Set(atvCls);
    const int32_t keyOff = R::FindPropertyOffset(actorClass, L"key");
    if (keyOff < 0) { fail = SpawnFail::NoOffset; return nullptr; }
    const auto classCurrent=[&]{return requestedClass.Get() && baseClass.Get() &&
        requestedClass.Is(R::FindClass(wantedClass.c_str())) && baseClass.Is(AtvClass()) &&
        R::FindPropertyOffset(requestedClass.Get(),L"key")==keyOff;};
    const R::FName keyName = fname_utils::StringToFName(wantedKey);
    if (keyName.ComparisonIndex == 0) { fail = SpawnFail::NameNone; return nullptr; }
    if(!worldCurrent() || !classCurrent()){fail=SpawnFail::Retired;return nullptr;}
    // Spawning is infrequent. Resolve a fresh checked service tuple rather than
    // retaining raw class/CDO/function pointers across joins and reloads.
    CachedObjRef serviceClass,service,beginFn,finishFn;
    serviceClass.Set(R::FindClass(L"GameplayStatics"));service.Set(R::FindClassDefaultObject(L"GameplayStatics"));
    if(serviceClass.Get()){
        beginFn.Set(R::FindFunction(serviceClass.Get(),L"BeginDeferredActorSpawnFromClass"));
        finishFn.Set(R::FindFunction(serviceClass.Get(),L"FinishSpawningActor"));
    }
    if(!serviceClass.Get() || !service.Get() || !beginFn.Get() || !finishFn.Get()){fail=SpawnFail::Unresolved;return nullptr;}
    const auto current=[&]{return worldCurrent() && classCurrent() && serviceClass.Get() && service.Get() && beginFn.Get() && finishFn.Get() &&
        serviceClass.Is(R::FindClass(L"GameplayStatics")) && service.Is(R::FindClassDefaultObject(L"GameplayStatics")) &&
        R::ClassOf(service.Get())==serviceClass.Get() &&
        beginFn.Is(R::FindFunction(serviceClass.Get(),L"BeginDeferredActorSpawnFromClass")) &&
        finishFn.Is(R::FindFunction(serviceClass.Get(),L"FinishSpawningActor"));};

    FTransform xform{};
    engine::RotatorToQuat(facing.Pitch, facing.Yaw, facing.Roll, xform.RotX, xform.RotY, xform.RotZ, xform.RotW);
    xform.TX = target.X; xform.TY = target.Y; xform.TZ = target.Z;

    void* spawned = nullptr;
    {
        ParamFrame begin(beginFn.Get());
        if (!begin.valid()) { fail = SpawnFail::BeginFailed; return nullptr; }
        if (!begin.Set<void*>(L"WorldContextObject", context.Get()) ||
            !begin.Set<void*>(L"ActorClass", actorClass) ||
            !begin.SetRaw(L"SpawnTransform", &xform, sizeof(xform)) ||
            !begin.Set<uint8_t>(L"CollisionHandlingOverride", uint8_t{1}) ||  // AlwaysSpawn
            !begin.Set<void*>(L"Owner", nullptr)) { fail = SpawnFail::BeginFailed; return nullptr; }
        if(!current()){fail=SpawnFail::Retired;return nullptr;}
        if (!Call(service.Get(), begin)) { fail = SpawnFail::BeginFailed; return nullptr; }
        if(!begin.GetRaw(L"ReturnValue",&spawned,sizeof(spawned))){fail=SpawnFail::BeginFailed;return nullptr;}
    }
    if (!spawned || !R::IsLive(spawned)) { fail = SpawnFail::BeginFailed; return nullptr; }
    CachedObjRef body,bodyClass;body.Set(spawned);bodyClass.Set(R::ClassOf(spawned));
    // A foreign return is never a licence to overwrite its key or destroy it.
    if(!bodyClass.Is(requestedClass.Raw()) || world_identity::WorldOf(spawned)!=world){fail=SpawnFail::BeginFailed;return nullptr;}
    const auto owns=[&]{return body.Get() && bodyClass.Get() && bodyClass.Is(requestedClass.Raw()) &&
        R::ClassOf(spawned)==bodyClass.Get() && generation==world_identity::Generation() &&
        world_identity::CurrentWorld()==world && world_identity::WorldOf(spawned)==world;};
    const auto discard=[&]{if(owns())DestroyMirror(spawned);};
    if(!current() || !owns()){discard();fail=SpawnFail::Retired;return nullptr;}
    // While the actor is still deferred: its construction script runs in the finish.
    *reinterpret_cast<R::FName*>(reinterpret_cast<char*>(spawned) + keyOff) = keyName;
    {
        ParamFrame finish(finishFn.Get());
        if (!finish.valid()) {
            // The BeginDeferred actor was created but never finished -- UE does NOT GC it (it lingers
            // in the world). Destroy the orphan, exactly as npc_mirror does on the same failure.
            discard();
            fail = SpawnFail::FinishFailed;
            return nullptr;
        }
        void* finished = nullptr;
        if(!finish.Set<void*>(L"Actor",spawned) || !finish.SetRaw(L"SpawnTransform",&xform,sizeof(xform))){discard();fail=SpawnFail::FinishFailed;return nullptr;}
        if(!current() || !owns()){discard();fail=SpawnFail::Retired;return nullptr;}
        if (!Call(service.Get(), finish) ||
            !finish.GetRaw(L"ReturnValue", &finished, sizeof(finished)) || finished != spawned) {
            discard();
            fail = SpawnFail::FinishFailed;
            return nullptr;
        }
    }
    // FinishSpawning moves the actor root. The wheels are separate bodies, and only the game's
    // own teleport places them with it. Without this the body can sit beside the save ATV while
    // the wheels stay where the class default left them.
    if(!current() || !owns()){discard();fail=SpawnFail::Retired;return nullptr;}
    if (!TeleportRig(spawned,target,facing)){discard();fail=SpawnFail::PlacementFailed;return nullptr;}
    if(!current() || !owns()){discard();fail=SpawnFail::Retired;return nullptr;}
    // The construction script's keeping of a preset key is read back, not assumed: a mirror that stands
    // under another key, a case-different one included (the index and the prune file key exactly), would
    // be filed by the caller under the host's and be pruned as a stranger by the next pass. The same rig
    // destroy as the failures above.
    const R::FName actual=*reinterpret_cast<const R::FName*>(reinterpret_cast<const char*>(spawned)+keyOff);
    const std::wstring born=R::ToString(actual);
    if(!current() || !owns()){discard();fail=SpawnFail::Retired;return nullptr;}
    const R::FName finalKey=*reinterpret_cast<const R::FName*>(reinterpret_cast<const char*>(spawned)+keyOff);
    if (born != wantedKey || actual.ComparisonIndex!=finalKey.ComparisonIndex || actual.Number!=finalKey.Number) {
        if (bornKey) *bornKey = born;
        discard();
        fail = SpawnFail::KeyMismatch;
        return nullptr;
    }
    return spawned;  // physics LEFT ON -- a native idle ATV (grabbable). atv_sync owns lifetime/sync.
}

bool Dismount(void* atv) {
    if (!atv || !R::IsLive(atv)) return false;
    CachedObjRef live; live.Set(atv);
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(atv), L"dismount");
    ParamFrame f(fn);
    return f.valid() && Call(atv, f) && live.Get() && !IsDriven(atv);
}

bool FindCompanionLocation(void* atv, FVector& location) {
    if(!atv || !R::IsLive(atv) || !IsAtv(atv))return false;
    CachedObjRef source,cls;source.Set(atv);cls.Set(R::ClassOf(atv));
    const auto generation=world_identity::Generation();void* world=world_identity::CurrentWorld();
    const auto current=[&]{return source.Get() && cls.Get() && world && generation==world_identity::Generation() &&
        world==world_identity::CurrentWorld() && world_identity::WorldOf(atv)==world && R::ClassOf(atv)==cls.Get();};
    if(!current())return false;
    FVector loc{}, center{}, half{}; FRotator rot{};
    if (!GetRootTransform(atv, loc, rot) || !current() || !engine::GetActorBounds(atv, true, center, half) || !current() ||
        !std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll) ||
        std::abs(NormalizeAxis(rot.Pitch)) > 10.f || std::abs(NormalizeAxis(rot.Roll)) > 10.f)
        return false;
    for (float v : {loc.X, loc.Y, loc.Z, center.X, center.Y, center.Z, half.X, half.Y, half.Z})
        if (!std::isfinite(v)) return false;
    if (half.X < 25.f || half.Y < 25.f || half.Z < 15.f ||
        half.X > 250.f || half.Y > 250.f || half.Z > 200.f) return false;
    FVector fwd = engine::GetActorForwardVector(atv);
    if(!current() || !std::isfinite(fwd.Z))return false;
    const float length = std::sqrt(fwd.X * fwd.X + fwd.Y * fwd.Y);
    if (!std::isfinite(length) || length < 0.01f) return false;
    fwd = {fwd.X / length, fwd.Y / length, 0.f};
    const FVector directions[] = {{-fwd.Y, fwd.X, 0.f}, {fwd.Y, -fwd.X, 0.f},
                                   fwd, {-fwd.X, -fwd.Y, 0.f}};
    const float bottom = center.Z - half.Z;
    for (float extra : {25.f, 65.f}) for (const auto& direction : directions) {
        const float distance = 2.f * (std::abs(direction.X) * half.X + std::abs(direction.Y) * half.Y) + extra;
        if (distance > 350.f) continue;
        FVector target{center.X + direction.X * distance, center.Y + direction.Y * distance, center.Z};
        // Require support under the centre and all four corners, near this floor.
        // No trace into the basement, or a spawn suspended over a garage pit.
        float lowest = bottom + 40.f, highest = bottom - 60.f;
        bool supported = true;
        const FVector offsets[] = {{}, {-half.X*.8f,-half.Y*.8f,0}, {-half.X*.8f,half.Y*.8f,0},
                                   {half.X*.8f,-half.Y*.8f,0}, {half.X*.8f,half.Y*.8f,0}};
        for (const auto& offset : offsets) {
            FVector point{};
            const bool hit=trace::FindFloor(atv, {target.X+offset.X,target.Y+offset.Y,bottom+40.f},
                                               {target.X+offset.X,target.Y+offset.Y,bottom-60.f}, point);
            if(!current())return false;
            if (!hit) {
                supported = false; break;
            }
            lowest = (std::min)(lowest, point.Z);
            highest = (std::max)(highest, point.Z);
        }
        if (!supported || highest - lowest > 15.f) continue;
        target.Z = highest + half.Z + 4.f;
        // Sweep the whole body from the original bay, so a clear destination on
        // the other side of a wall is not an admissible parking spot.
        FVector start = center;
        start.Z = (std::max)(center.Z + 4.f, target.Z);
        FVector end = target; end.Z = start.Z;
        const int swept=trace::BoxBlocked(atv, start, end, half);
        if(!current())return false;
        if(swept!=0)continue;
        const int parked=trace::BoxBlocked(atv, end, {target.X,target.Y,target.Z-1.f}, half);
        if(!current())return false;
        if(parked!=0)continue;
        FVector settled{};FRotator facing{};
        if(!GetRootTransform(atv,settled,facing) || !current())return false;
        for(float v:{settled.X,settled.Y,settled.Z,facing.Pitch,facing.Yaw,facing.Roll})if(!std::isfinite(v))return false;
        if(std::abs(settled.X-loc.X)>1.f || std::abs(settled.Y-loc.Y)>1.f || std::abs(settled.Z-loc.Z)>1.f ||
           std::abs(NormalizeAxis(facing.Pitch-rot.Pitch))>1.f || std::abs(NormalizeAxis(facing.Yaw-rot.Yaw))>1.f ||
           std::abs(NormalizeAxis(facing.Roll-rot.Roll))>1.f)return false;
        location = {loc.X + target.X - center.X, loc.Y + target.Y - center.Y, loc.Z + target.Z - center.Z};
        return true;
    }
    return false;
}

bool DestroyMirror(void* atv) {
    if (!atv || !R::IsLive(atv)) return false;
    // A driver this peer's controller holds in it is handed back first, through the ATV's own dismount,
    // which re-possesses and shows the player as a driver's own leaving does: the rig possesses the ATV
    // while driven, so destroying it seated would leave the player hidden and unpossessed. MTA's vehicle
    // removes its driver and passengers before its game vehicle goes
    // (reference/mtasa-blue/Client/mods/deathmatch/logic/CClientVehicle.cpp:3075-3094). A dismount that
    // cannot run is said: the destroy still goes ahead, and the player is left as the seat held them.
    if (engine::GetController(atv) && !Dismount(atv))
        UE_LOGW("atv: DestroyMirror could not run the ATV's own dismount -- the local driver stays "
                "unpossessed and hidden after the ATV goes");
    // K2_DestroyActor is declared on Actor, not on ATV_C, so it is resolved there.
    return engine::DestroyActor(atv);
}

void* SpawnMirror(const std::wstring& className,const FVector& loc,const FRotator& rot) {
    std::wstring key;
    for(int i=1;i<=2;++i) {
        const auto candidate=L"coopatv"+std::to_wstring(i);
        if(!FindOtherLiveByKey(candidate,nullptr) && !LoadedSaveNamesKey(candidate.c_str())) {key=candidate;break;}
    }
    if(key.empty())return nullptr;
    void* actor=SpawnMirror(className,loc,rot,key);
    if(actor)SetIncludedInSave(actor,true);
    return actor;
}

}  // namespace ue_wrap::atv
