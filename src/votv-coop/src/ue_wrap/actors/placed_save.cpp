#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/actors/wall_build.h"
#include "ue_wrap/actors/wall_drying.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/keyed_objects.h"
#include <cmath>
#include <cstring>
#include <map>

namespace ue_wrap::placed_save {
namespace {
namespace R = reflection;
namespace SR = save_record;
// Native holes are transient AActors with no save key/getData. Retain a
// session identity alongside the live actor, without inventing engine fields.
struct HoleKey { CachedObjRef actor; std::wstring key; };
std::map<int32_t, HoleKey> g_holeKeys;
uint32_t g_holeWorld = 0;
void CleanHoleKeys() {
    const auto world = world_identity::Generation();
    if (g_holeWorld != world) { g_holeKeys.clear(); g_holeWorld = world; }
    std::erase_if(g_holeKeys, [](const auto& row) { return !row.second.actor.Get(); });
}
bool Verb(void* actor, const wchar_t* name) {
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(actor), name));
    return f.valid() && Call(actor, f);
}
bool PutObject(void* actor, const wchar_t* name, void* value) {
    R::InstanceOffset field(name);
    const int32_t off = field.Of(actor);
    if (off < 0) return false;
    std::memcpy(static_cast<uint8_t*>(actor) + off, &value, sizeof(value));
    return true;
}
bool ReadObject(void* actor, const wchar_t* name, void*& value) {
    R::InstanceOffset field(name);
    const int32_t off = field.Of(actor);
    if (off < 0) return false;
    std::memcpy(&value, static_cast<uint8_t*>(actor) + off, sizeof(value));
    return true;
}
bool PutName(void* actor, const wchar_t* name, const std::wstring& value) {
    R::InstanceOffset field(name);
    const int32_t off = field.Of(actor);
    if (off < 0) return false;
    SR::WriteFNameField(static_cast<uint8_t*>(actor) + off, value);
    return true;
}
bool Name(const std::wstring& s) {
    if (s.size() > 128) return false;
    for (wchar_t c : s) if (c < 32 || c > 126) return false;
    return true;
}
bool Empty(const std::wstring& s) { return s.empty() || s == L"None"; }
bool Descends(void* a, const wchar_t* name) {
    int depth = 0;
    for (void* c = a ? R::ClassOf(a) : nullptr; c && depth++ < 64; c = R::SuperStructOf(c))
        if (R::NameEquals(R::NameOf(c), name)) return true;
    return false;
}
}
Kind Type(void* actor) {
    if (!actor || !R::IsLive(actor)) return Kind::None;
    void* cls = R::ClassOf(actor);
    if (!cls) return Kind::None;
    const auto& name = R::NameOf(cls);
    if (R::NameEquals(name, L"dirthole_C")) return Kind::DirtHole;
    if (R::NameEquals(name, L"faucet_C")) return Kind::Faucet;
    if (R::NameEquals(name, L"groundHose_C")) return Kind::Hose;
    if (R::NameEquals(name, L"customWall_unfinished_C")) return Kind::WallOutline;
    if (R::NameStartsWith(name, L"customWall")) return ClassKind(R::ClassNameOf(actor));
    if (!R::NameStartsWith(name, L"growingPlant")) return Kind::None;
    return ClassKind(R::ClassNameOf(actor));
}
bool PlacementCaller(Kind kind, void* function) {
    if (!function) return false;
    void* owner = R::OuterOf(function);
    if (!owner) return false;
    const auto& fn = R::NameOf(function);
    const auto& cls = R::NameOf(owner);
    if (kind == Kind::DirtHole)
        return R::NameEquals(cls, L"mainPlayer_C") && R::NameEquals(fn, L"ExecuteUbergraph_mainPlayer");
    if (IsWall(kind))
        return (R::NameEquals(cls, L"prop_wallbuilder_C") && R::NameEquals(fn, L"addWall")) ||
            (kind == Kind::Wall && ((R::NameEquals(cls, L"customWall_unfinished_C") && R::NameEquals(fn, L"buildWall")) ||
                (R::NameEquals(cls, L"customWall_wetConcrete_C") && R::NameEquals(fn, L"ExecuteUbergraph_customWall_wetConcrete"))));
    if (kind == Kind::Faucet)
        return R::NameEquals(cls, L"prop_faucet_C") && R::NameEquals(fn, L"ExecuteUbergraph_prop_faucet");
    if (kind == Kind::Hose)
        return R::NameEquals(cls, L"prop_hose_C") && R::NameEquals(fn, L"ExecuteUbergraph_prop_hose");
    if (kind == Kind::Plant)
        return (R::NameEquals(cls, L"dirthole_C") && R::NameEquals(fn, L"processBuriedItem")) ||
            (R::NameEquals(cls, L"prop_farmPot_C") && R::NameEquals(fn, L"ExecuteUbergraph_prop_farmPot"));
    return false;
}
bool Ready(void* actor) {
    const Kind kind = Type(actor);
    if (kind == Kind::None) return false;
    if (kind != Kind::Hose) return true;
    int32_t off = -1; uint8_t mask = 0;
    return R::FindBoolProperty(R::ClassOf(actor), L"moving", off, mask) && off >= 0 && mask &&
        !(static_cast<uint8_t*>(actor)[off] & mask);
}
bool Capture(void* actor, SR::SaveRecord& record) {
    if (Type(actor) == Kind::None) return false;
    if (Type(actor) == Kind::DirtHole) {
        CachedObjRef ref; ref.Set(actor);
        const auto world = world_identity::Generation();
        FVector pos{}; FRotator rot{};
        if (!engine::TryGetActorLocation(actor, pos) || !ref.Get() ||
            !engine::TryGetActorRotation(actor, rot) || !ref.Get()) return false;
        const auto scale = engine::GetActorScale3D(actor);
        if (!ref.Get() || world != world_identity::Generation()) return false;
        const int off = R::FindPropertyOffset(R::ClassOf(actor), L"matType");
        if (off < 0) return false;
        int32_t material = 0; std::memcpy(&material, static_cast<uint8_t*>(actor) + off, sizeof(material));
        SR::SaveRecord out; out.className = L"dirthole_C"; out.key = ExistingKey(actor);
        engine::RotatorToQuat(rot.Pitch, rot.Yaw, rot.Roll, out.xform[0], out.xform[1], out.xform[2], out.xform[3]);
        out.xform[4] = pos.X; out.xform[5] = pos.Y; out.xform[6] = pos.Z;
        out.xform[7] = scale.X; out.xform[8] = scale.Y; out.xform[9] = scale.Z;
        out.ints = {{material}}; record = std::move(out); return true;
    }
    const auto world = world_identity::Generation();
    CachedObjRef ref; ref.Set(actor);
    if (!SR::CaptureRecordVia(actor,
        R::FindDispatchFunctionCached(R::ClassOf(actor), L"getData"), record) || !ref.Get() || world != world_identity::Generation()) return false;
    const auto kind = Type(actor);
    if (wall_drying::IsWetWall(actor) && !wall_drying::Capture(actor,record)) return false;
    if (kind == Kind::Hose || kind == Kind::Faucet) {
        // Native getData preserves the old saved key when the optional pointer
        // is null. A detached endpoint must not reattach on the next load.
        void* endpoint = nullptr;
        if (!ReadObject(actor, kind == Kind::Hose ? L"sprinkler" : L"hose", endpoint) ||
            record.names.empty() || record.names[0].size() != (kind == Kind::Hose ? 2 : 1)) return false;
        if (!endpoint || !R::IsLive(endpoint)) {
            record.names[0][kind == Kind::Hose ? 1 : 0] = L"None";
            if (!PutName(actor, kind == Kind::Hose ? L"sprinkler_key" : L"hose_key", L"None") ||
                !ref.Get() || world != world_identity::Generation()) return false;
        }
    }
    return true;
}
bool SetKey(void* actor, const std::wstring& key) {
    if (Type(actor) == Kind::None || Empty(key) || !Name(key)) return false;
    if (Type(actor) == Kind::DirtHole) {
        CleanHoleKeys();
        CachedObjRef ref; ref.Set(actor);
        if (g_holeKeys.size() >= 1024 && !g_holeKeys.count(ref.Idx())) return false;
        g_holeKeys[ref.Idx()] = {ref, key}; return true;
    }
    R::InstanceOffset field(L"key");
    const int32_t off = field.Of(actor);
    if (off < 0) return false;
    SR::WriteFNameField(static_cast<uint8_t*>(actor) + off, key);
    // setKey alone does not register the actor in mainGamemode.keyObj_*.
    CachedObjRef ref; ref.Set(actor);
    return Verb(actor, L"getKey") && ref.Get() && SR::ReadFNameAt(actor, off) == key;
}
std::wstring ExistingKey(void* actor) {
    if (Type(actor) == Kind::None) return {};
    if (Type(actor) == Kind::DirtHole) {
        CleanHoleKeys(); CachedObjRef ref; ref.Set(actor);
        const auto it = g_holeKeys.find(ref.Idx());
        return it != g_holeKeys.end() && it->second.actor.Is(actor) ? it->second.key : std::wstring{};
    }
    R::InstanceOffset field(L"key");
    const int32_t off = field.Of(actor);
    return off >= 0 ? SR::ReadFNameAt(actor, off) : std::wstring{};
}
bool CanHarvest(void* actor) {
    if (Type(actor) != Kind::Plant) return false;
    int32_t off = -1; uint8_t mask = 0;
    return R::FindBoolProperty(R::ClassOf(actor), L"discard", off, mask) && off >= 0 && mask &&
        !(static_cast<uint8_t*>(actor)[off] & mask);
}
bool Harvest(void* actor, void* collector) {
    if (!CanHarvest(actor) || !collector || !R::IsLive(collector)) return false;
    const auto world = world_identity::Generation();
    CachedObjRef ref, collectorRef; ref.Set(actor); collectorRef.Set(collector);
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(actor), L"actionOptionIndex");
    void* owner = fn ? R::OuterOf(fn) : nullptr;
    if (!owner || !R::NameEquals(R::NameOf(owner), L"growingPlant_C")) return false;
    ParamFrame frame(fn);
    if (!frame.valid() || !frame.Set<void*>(L"player", collector) || !frame.Set<uint8_t>(L"action", 4) ||
        !frame.Set<void*>(L"lookAtComponent", nullptr) || !ref.Get() || !collectorRef.Get() || world != world_identity::Generation()) return false;
    return Call(actor, frame);
}
bool ReadCareUse(void* tool, void* player, CareKind kind, CareUse& use) {
    if (!tool || !player || !R::IsLive(tool) || !R::IsLive(player) ||
        R::ClassNameOf(tool) != (kind==CareKind::Water?L"prop_bucket_C":L"prop_fertilizer_C")) return false;
    const auto world=world_identity::Generation();
    CachedObjRef source, pawn;source.Set(tool);pawn.Set(player);
    void* plant=engine::ReadMainPlayerHitActor(player);
    if (!source.Get() || !pawn.Get() || world!=world_identity::Generation() || Type(plant)!=Kind::Plant ||
        !world_identity::CurrentWorld() || world_identity::WorldOf(plant)!=world_identity::CurrentWorld()) return false;
    CachedObjRef target;target.Set(plant);
    R::InstanceOffset key(L"key");const int keyOff=key.Of(tool);
    if (keyOff<0) return false;
    CareUse measured;measured.plant=plant;measured.kind=kind;measured.sourceKey=SR::ReadFNameAt(tool,keyOff);
    if (Empty(measured.sourceKey) || !Name(measured.sourceKey)) return false;
    if (kind==CareKind::Water) {
        R::InstanceOffset height(L"height"), maximum(L"maxHeight");
        const int h=height.Of(tool), m=maximum.Of(tool);
        if (h<0 || m<0) return false;
        float water=0,capacity=0;std::memcpy(&water,static_cast<uint8_t*>(tool)+h,4);std::memcpy(&capacity,static_cast<uint8_t*>(tool)+m,4);
        if (!std::isfinite(water)||!std::isfinite(capacity)||water<=5.f||capacity<=0.f||water>capacity) return false;
        measured.amount=water/capacity*600.f;
    } else {
        R::InstanceOffset fertilizer(L"fertilizer");const int off=fertilizer.Of(plant);float stock=0;
        if (off<0) return false;std::memcpy(&stock,static_cast<uint8_t*>(plant)+off,4);
        if (!std::isfinite(stock)||stock>0.f) return false;
        measured.amount=600.f;
    }
    if (!source.Get() || !pawn.Get() || !target.Get() || world!=world_identity::Generation()) return false;
    use=std::move(measured);return true;
}
bool CareWasSpent(void* tool, CareKind kind) {
    if (kind==CareKind::Fertilizer) return !R::IsLive(tool);
    if (!tool||!R::IsLive(tool)||R::ClassNameOf(tool)!=L"prop_bucket_C") return false;
    R::InstanceOffset height(L"height");const int off=height.Of(tool);float water=1;
    if (off<0) return false;std::memcpy(&water,static_cast<uint8_t*>(tool)+off,4);
    return water==0.f;
}
bool ApplyCare(void* plant, CareKind kind, float amount) {
    if (Type(plant)!=Kind::Plant || !std::isfinite(amount)||amount<=0.f||amount>600.f ||
        (kind==CareKind::Fertilizer && amount!=600.f)) return false;
    R::InstanceOffset water(L"water"), fertilizer(L"fertilizer");
    const int off=(kind==CareKind::Water?water:fertilizer).Of(plant);
    if (off<0) return false;
    float current=0;std::memcpy(&current,static_cast<uint8_t*>(plant)+off,4);
    if (!std::isfinite(current)) return false;
    // Fertilizer's native use refuses a second application while stock remains.
    // A stale guest view must not reset a currently fertilized host crop.
    if (kind==CareKind::Fertilizer && current>0.f) return true;
    std::memcpy(static_cast<uint8_t*>(plant)+off,&amount,4);
    return true;
}
bool Apply(void* actor, const SR::SaveRecord& record) {
    if (Type(actor) != ClassKind(record.className) || R::ClassNameOf(actor) != record.className) return false;
    CachedObjRef ref; ref.Set(actor);
    if (Type(actor) == Kind::DirtHole) {
        SR::SaveRecord checked;
        if (!Normalize(record, checked) || !SetKey(actor, record.key)) return false;
        const int off = R::FindPropertyOffset(R::ClassOf(actor), L"matType");
        if (off < 0) return false;
        std::memcpy(static_cast<uint8_t*>(actor) + off, &checked.ints[0][0], sizeof(int32_t));
        return Verb(actor, L"UserConstructionScript") && ref.Get();
    }
    if (Type(actor) == Kind::Plant) {
        R::InstanceOffset amount(L"amount");
        const int32_t off = amount.Of(actor);
        if (off < 0) return false;
        // upd sets amount=1 for ripe bushes, but never clears an older ripe
        // amount when loading the host's harvested fruitGrow=0 state.
        const int32_t empty = 0; std::memcpy(static_cast<uint8_t*>(actor) + off, &empty, sizeof(empty));
    }
    // Wall loadData calls init, which derives brick count/collision from scale.
    // actor_save.loadData restores transform too, but establish it before any
    // descendant initialization; the other families retain their native order.
    if (IsWall(ClassKind(record.className))) {
        if (!engine::SetActorScale3D(actor, {record.xform[7], record.xform[8], record.xform[9]}) || !ref.Get()) return false;
    }
    if (!SR::ApplyRecordVia(actor, R::FindDispatchFunctionCached(R::ClassOf(actor), L"loadData"), record)) return false;
    if (!ref.Get()) return false;
    if (wall_drying::IsWetWall(actor) && !wall_drying::Restore(actor,record)) return false;
    if (ClassKind(record.className) == Kind::WallOutline &&
        !wall_build::RestoreMaterial(actor, record.strings[0][0].c_str())) return false;
    return ref.Get() && SetKey(actor, record.key);
}
bool ToggleFaucet(void* actor) {
    if (Type(actor) != Kind::Faucet) return false;
    const auto world = world_identity::Generation();
    CachedObjRef ref; ref.Set(actor);
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(actor), L"actionOptionIndex"));
    return f.valid() && f.Set<uint8_t>(L"action", 5) && f.Set<void*>(L"player", nullptr) &&
        world == world_identity::Generation() && ref.Get() && Call(actor, f);
}
bool DetachSprinkler(void* actor, const std::wstring& expectedEndpoint) {
    if (Type(actor) != Kind::Hose) return false;
    const auto world = world_identity::Generation();
    CachedObjRef ref; ref.Set(actor);
    void* sprinkler = nullptr;
    if (!ReadObject(actor, L"sprinkler", sprinkler)) return false;
    if (!sprinkler || !R::IsLive(sprinkler)) return true;
    if (!Descends(sprinkler, L"prop_sprinkler_C")) return false;
    CachedObjRef end; end.Set(sprinkler);
    R::InstanceOffset key(L"key"); const auto off = key.Of(sprinkler);
    if (off < 0 || SR::ReadFNameAt(sprinkler, off) != expectedEndpoint) return false;
    const auto current = [&] { return world == world_identity::Generation() && ref.Get() && end.Get(); };
    void* owner = nullptr;
    if (!ReadObject(sprinkler, L"hose", owner) || owner != actor || !current()) return false;
    if (!PutName(sprinkler, L"hose_key", L"None") || !current() ||
        !PutName(actor, L"sprinkler_key", L"None") || !current()) return false;
    return Verb(actor, L"detachSprinkler") && current();
}
void* Spawn(const SR::SaveRecord& record) {
    void* cls = object_index::ClassByName(record.className.c_str());
    if (!cls || ClassKind(record.className) == Kind::None) return nullptr;
    const FVector pos{record.xform[4], record.xform[5], record.xform[6]};
    const auto rot = engine::QuatToRotator(record.xform[0], record.xform[1], record.xform[2], record.xform[3]);
    void* a = engine::BeginDeferredSpawn(cls, pos, rot);
    if (!a) return nullptr;
    CachedObjRef ref; ref.Set(a);
    if (ClassKind(record.className) == Kind::DirtHole) {
        const int off = R::FindPropertyOffset(R::ClassOf(a), L"matType");
        if (off < 0 || record.ints.empty() || record.ints[0].size() != 1) {
            engine::DestroyActor(a); return nullptr;
        }
        std::memcpy(static_cast<uint8_t*>(a) + off, &record.ints[0][0], sizeof(int32_t));
    }
    if (ClassKind(record.className) == Kind::Plant) {
        R::InstanceOffset field(L"plant");
        const int32_t off = field.Of(a);
        if (off < 0) { engine::DestroyActor(a); return nullptr; }
        SR::WriteFNameField(static_cast<uint8_t*>(a) + off, record.names[1][0]);
    }
    if (!engine::FinishDeferredSpawn(a, pos, rot) || !ref.Get() || !Apply(a, record)) {
        if (ref.Get()) engine::DestroyActor(a);
        return nullptr;
    }
    // BeginPlay starts following this machine's mainPlayer. The completed saved
    // hose must not execute that body while its faucet dependency is arriving.
    if (ClassKind(record.className) == Kind::Hose) engine::SetActorTickEnabled(a, false);
    engine::SetActorScale3D(a, {record.xform[7], record.xform[8], record.xform[9]});
    return ref.Get();
}
bool HoseLinksAvailable(void* actor, const SR::SaveRecord& record) {
    if (Type(actor) != Kind::Hose || record.names.empty() || record.names[0].size() != 2) return false;
    const auto world = world_identity::Generation();
    CachedObjRef ref; ref.Set(actor);
    void* faucet = keyed_objects::Resolve(record.names[0][0].c_str());
    if (world != world_identity::Generation() || !ref.Get() || Type(faucet) != Kind::Faucet) return false;
    CachedObjRef faucetRef; faucetRef.Set(faucet);
    void* owner = nullptr;
    if (!ReadObject(faucet, L"hose", owner) || (owner && owner != actor && R::IsLive(owner))) return false;
    if (!Empty(record.names[0][1])) {
        void* sprinkler = keyed_objects::Resolve(record.names[0][1].c_str());
        if (world != world_identity::Generation() || !ref.Get() || !faucetRef.Get() || !sprinkler || !R::IsLive(sprinkler) ||
            !Descends(sprinkler, L"prop_sprinkler_C") || !ReadObject(sprinkler, L"hose", owner) ||
            (owner && owner != actor && R::IsLive(owner))) return false;
    }
    return world == world_identity::Generation() && ref.Get() && faucetRef.Get();
}
bool Link(void* actor, const SR::SaveRecord& record) {
    const Kind kind = Type(actor);
    if (kind == Kind::None) return false;
    CachedObjRef ref; ref.Set(actor);
    if (IsWall(kind) || kind == Kind::DirtHole) return true; // no hose/pot links or processKeys work
    if (kind == Kind::Faucet) {
        const auto& key = record.names[0][0];
        if (!Empty(key)) {
            void* hose = keyed_objects::Resolve(key.c_str());
            if (Type(hose) != Kind::Hose) return false;
        }
    } else if (kind == Kind::Hose) {
        if (!HoseLinksAvailable(actor, record) || !ref.Get()) return false;
        const auto world = world_identity::Generation();
        void* faucet = keyed_objects::Resolve(record.names[0][0].c_str());
        if (world != world_identity::Generation() || !ref.Get() || Type(faucet) != Kind::Faucet) return false;
        CachedObjRef faucetRef; faucetRef.Set(faucet);
        void* sprinkler = nullptr;
        if (!Empty(record.names[0][1])) {
            sprinkler = keyed_objects::Resolve(record.names[0][1].c_str());
            if (world != world_identity::Generation() || !ref.Get() || !faucetRef.Get() ||
                !sprinkler || !R::IsLive(sprinkler) || !Descends(sprinkler, L"prop_sprinkler_C")) return false;
        }
        CachedObjRef sprinklerRef; if (sprinkler) sprinklerRef.Set(sprinkler);
        const auto current = [&] { return world == world_identity::Generation() && ref.Get() && faucetRef.Get() &&
            (!sprinkler || sprinklerRef.Get()); };
        void* oldSprinkler = nullptr;
        if (!ReadObject(actor, L"sprinkler", oldSprinkler) || !current()) return false;
        CachedObjRef oldRef;
        if (oldSprinkler && R::IsLive(oldSprinkler) && Descends(oldSprinkler, L"prop_sprinkler_C")) oldRef.Set(oldSprinkler);
        // loadData changes only the key; processKeys never clears the old
        // pointer on None. Clear the former owner's pointer AND saved key.
        if (oldRef.Get() && oldSprinkler != sprinkler) {
            void* owner = nullptr;
            if (!ReadObject(oldSprinkler, L"hose", owner)) return false;
            if (owner == actor) {
                if (!PutName(oldSprinkler, L"hose_key", L"None") || !current() || !oldRef.Get()) return false;
                if (!PutObject(oldSprinkler, L"hose", nullptr)) return false;
                if (!Verb(oldSprinkler, L"upd") || !current()) return false;
            }
        }
        // Native placement writes both directions. Establish nullable pointers
        // before processKeys/finish so faucet.upd sees the new endpoint too.
        if (!current() || !PutObject(actor, L"faucet", faucet) || !PutObject(actor, L"sprinkler", sprinkler) ||
            !PutObject(faucet, L"hose", actor) || !PutName(faucet, L"hose_key", record.key) || !current()) return false;
        if (sprinkler && (!PutObject(sprinkler, L"hose", actor) || !PutName(sprinkler, L"hose_key", record.key) || !current())) return false;
        if (!Verb(actor, L"processKeys") || !current()) return false;
        return !sprinkler || (Verb(sprinkler, L"upd") && current());
    } else if (!Empty(record.names[1][1])) {
        void* pot = keyed_objects::Resolve(record.names[1][1].c_str());
        if (!pot || !R::IsLive(pot) || !Descends(pot, L"prop_farmPot_C")) return false;
    }
    if (!ref.Get()) return false;
    if (!Verb(actor, L"processKeys") || !ref.Get()) return false;
    return kind != Kind::Faucet || Verb(actor, L"upd");
}
}
