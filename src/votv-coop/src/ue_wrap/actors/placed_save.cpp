#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/actors/wall_build.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/world/keyed_objects.h"
#include <cmath>
#include <cstring>

namespace ue_wrap::placed_save {
namespace {
namespace R = reflection;
namespace SR = save_record;
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
    CachedObjRef ref; ref.Set(actor);
    return SR::CaptureRecordVia(actor,
        R::FindDispatchFunctionCached(R::ClassOf(actor), L"getData"), record) && ref.Get();
}
bool SetKey(void* actor, const std::wstring& key) {
    if (Type(actor) == Kind::None || Empty(key) || !Name(key)) return false;
    R::InstanceOffset field(L"key");
    const int32_t off = field.Of(actor);
    if (off < 0) return false;
    SR::WriteFNameField(static_cast<uint8_t*>(actor) + off, key);
    // setKey alone does not register the actor in mainGamemode.keyObj_*.
    CachedObjRef ref; ref.Set(actor);
    return Verb(actor, L"getKey") && ref.Get() && SR::ReadFNameAt(actor, off) == key;
}
bool Apply(void* actor, const SR::SaveRecord& record) {
    if (Type(actor) != ClassKind(record.className) || R::ClassNameOf(actor) != record.className) return false;
    CachedObjRef ref; ref.Set(actor);
    // Wall loadData calls init, which derives brick count/collision from scale.
    // actor_save.loadData restores transform too, but establish it before any
    // descendant initialization; the other families retain their native order.
    if (IsWall(ClassKind(record.className))) {
        if (!engine::SetActorScale3D(actor, {record.xform[7], record.xform[8], record.xform[9]}) || !ref.Get()) return false;
    }
    if (!SR::ApplyRecordVia(actor, R::FindDispatchFunctionCached(R::ClassOf(actor), L"loadData"), record)) return false;
    if (!ref.Get()) return false;
    if (ClassKind(record.className) == Kind::WallOutline &&
        !wall_build::RestoreMaterial(actor, record.strings[0][0].c_str())) return false;
    return ref.Get() && SetKey(actor, record.key);
}
void* Spawn(const SR::SaveRecord& record) {
    void* cls = object_index::ClassByName(record.className.c_str());
    if (!cls || ClassKind(record.className) == Kind::None) return nullptr;
    const FVector pos{record.xform[4], record.xform[5], record.xform[6]};
    const auto rot = engine::QuatToRotator(record.xform[0], record.xform[1], record.xform[2], record.xform[3]);
    void* a = engine::BeginDeferredSpawn(cls, pos, rot);
    if (!a) return nullptr;
    CachedObjRef ref; ref.Set(a);
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
bool Link(void* actor, const SR::SaveRecord& record) {
    const Kind kind = Type(actor);
    if (kind == Kind::None) return false;
    CachedObjRef ref; ref.Set(actor);
    if (IsWall(kind)) return true; // no hose/pot links or processKeys work
    if (kind == Kind::Faucet) {
        const auto& key = record.names[0][0];
        if (!Empty(key)) {
            void* hose = keyed_objects::Resolve(key.c_str());
            if (Type(hose) != Kind::Hose) return false;
        }
    } else if (kind == Kind::Hose) {
        void* faucet = keyed_objects::Resolve(record.names[0][0].c_str());
        if (Type(faucet) != Kind::Faucet) return false;
        CachedObjRef faucetRef; faucetRef.Set(faucet);
        void* sprinkler = nullptr;
        if (!Empty(record.names[0][1])) {
            sprinkler = keyed_objects::Resolve(record.names[0][1].c_str());
            if (!sprinkler || !R::IsLive(sprinkler) || !Descends(sprinkler, L"prop_sprinkler_C")) return false;
        }
        // processKeys restores hose->faucet but not the reverse link; native placement writes both.
        if (!ref.Get() || !faucetRef.Get()) return false;
        if (!PutObject(faucet, L"hose", actor) || (sprinkler && !PutObject(sprinkler, L"hose", actor))) return false;
    } else if (!Empty(record.names[1][1])) {
        void* pot = keyed_objects::Resolve(record.names[1][1].c_str());
        if (!pot || !R::IsLive(pot) || !Descends(pot, L"prop_farmPot_C")) return false;
    }
    if (!ref.Get()) return false;
    if (!Verb(actor, L"processKeys") || !ref.Get()) return false;
    return kind != Kind::Faucet || Verb(actor, L"upd");
}
}
