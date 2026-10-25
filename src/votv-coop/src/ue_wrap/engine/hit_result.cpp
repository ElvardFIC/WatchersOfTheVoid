// ue_wrap/engine/hit_result.cpp -- see ue_wrap/engine/hit_result.h.

#include "ue_wrap/engine/hit_result.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

namespace ue_wrap::hit_result {
namespace {

namespace R = reflection;

// GameplayStatics is a native class: the same object for the process, as every native class was in
// the class-lifetime census, so its CDO, its function and the struct the function returns are kept.
void* g_cdo = nullptr;
void* g_makeFn = nullptr;
void* g_hitStruct = nullptr;  // FHitResult's UScriptStruct, the return value's type

// FHitResult's members the reader takes, by the struct's own reflection, resolved once.
struct Layout {
    bool    tried = false, ok = false;
    int32_t size = 0, blockByte = -1, actor = -1, component = -1, point = -1, normal = -1;
    uint8_t blockMask = 0;
};

const Layout& Members() {
    static Layout l;
    static CachedObjRef type;
    if (l.tried && type.Is(g_hitStruct)) return l;
    l={};type.Set(g_hitStruct);
    l.tried = true;
    l.size = R::StructSize(g_hitStruct);
    l.actor = R::FindPropertyOffset(g_hitStruct, L"Actor");
    l.component = R::FindPropertyOffset(g_hitStruct, L"Component");
    l.point = R::FindPropertyOffset(g_hitStruct, L"ImpactPoint");
    l.normal = R::FindPropertyOffset(g_hitStruct, L"ImpactNormal");
    const bool block = R::FindBoolProperty(g_hitStruct, L"bBlockingHit", l.blockByte, l.blockMask);
    const auto fits = [&](int32_t off, int32_t n) { return off >= 0 && off<=l.size && n<=l.size-off; };
    l.ok = type.Get() && block && l.blockMask && fits(l.blockByte, 1) && fits(l.actor, 8) && fits(l.component, 8) && fits(l.point, 12) &&
           fits(l.normal, 12);
    if(!l.ok)l.tried=false;
    if (!l.ok)
        UE_LOGW("hit_result: FHitResult's members did not resolve (size %d, blocking %d, actor %d, component %d, "
                "point %d, normal %d)", l.size, l.blockByte, l.actor, l.component, l.point, l.normal);
    return l;
}

enum class Member : uint8_t { Actor, Component };

// The object a hit's weak pointer member resolves to through the object array, as the engine's own BreakHitResult
// reads it; null when it is stale or the member did not resolve. The offsets are the ones Members() holds.
void* Weak(const uint8_t* hit, Member m) {
    const Layout& l = Members();
    const int32_t off = m == Member::Actor ? l.actor : l.component;
    if (!l.ok || off < 0) return nullptr;
    int32_t idx = 0, serial = 0;
    std::memcpy(&idx, hit + off, sizeof(idx));
    std::memcpy(&serial, hit + off + sizeof(idx), sizeof(serial));
    return R::ResolveWeakObject(idx, serial);
}

// Does the built hit's weak pointer `m` resolve to `object`?
bool Names(const uint8_t* hit, Member m, void* object) { return Weak(hit, m) == object; }

bool Resolve() {
    if (!g_cdo) g_cdo = R::FindClassDefaultObject(L"GameplayStatics");
    if (!g_makeFn) {
        if (void* cls = R::FindClass(L"GameplayStatics")) g_makeFn = R::FindFunction(cls, L"MakeHitResult");
    }
    if (!g_hitStruct && g_makeFn) g_hitStruct = R::PropertyInnerStruct(g_makeFn, L"ReturnValue");
    return g_cdo && g_makeFn && g_hitStruct;
}

// MakeHitResult's value for a blocking hit on `component` of `actor` at `location`, `size` bytes into `hit`. False
// when the call fails or the hit does not name what it was built on.
bool Build(void* actor, void* component, const FVector& location, uint8_t* hit, int32_t size,const std::function<bool()>& current) {
    if(!current())return false;
    ParamFrame f(g_makeFn);
    if (!current() || !f.valid() || !f.Set<bool>(L"bBlockingHit", true) || !current() || !f.Set<void*>(L"HitActor", actor) || !current() ||
        !f.Set<void*>(L"HitComponent", component) || !current())
        return false;  // ParamFrame logs a name it does not know
    if(!f.Set<FVector>(L"Location", location) || !current() || !f.Set<FVector>(L"ImpactPoint", location) || !current())return false;
    if (!Call(g_cdo, f)) {
        UE_LOGW("hit_result: not built: the MakeHitResult call faulted");
        return false;
    }
    // Weak pointers and names only: the bytes are the whole value, with nothing to release.
    if (!current() || !f.GetRaw(L"ReturnValue", hit, size) || !current()) return false;
    // A hit that does not name what it was built on would send the call somewhere else.
    if (!Names(hit, Member::Component, component) || !Names(hit, Member::Actor, actor) || !current()) {
        UE_LOGW("hit_result: not built: the hit does not resolve back (component=%d actor=%d)",
                Names(hit, Member::Component, component) ? 1 : 0, Names(hit, Member::Actor, actor) ? 1 : 0);
        return false;
    }
    const auto& layout=Members();
    if(!layout.ok || !(hit[layout.blockByte]&layout.blockMask) || !current())return false;
    return true;
}

struct HitSource {
    CachedObjRef actor,type,component,componentType,make,makeOwner,cdo,hitStruct;
    uint32_t world=world_identity::Generation();
    explicit HitSource(void* a,void* c) {
        if(!a || !c || !R::IsLive(a) || !R::IsLive(c))return;
        actor.Set(a);type.Set(R::ClassOf(a));component.Set(c);componentType.Set(R::ClassOf(c));
    }
    bool ObjectsCurrent()const {
        return world==world_identity::Generation() && actor.Get() && type.Get() && component.Get() && componentType.Get() &&
            R::ClassOf(actor.Raw())==type.Raw() && R::ClassOf(component.Raw())==componentType.Raw();
    }
    void CaptureMaker(){make.Set(g_makeFn);makeOwner.Set(g_makeFn?R::OuterOf(g_makeFn):nullptr);cdo.Set(g_cdo);hitStruct.Set(g_hitStruct);}
    bool Current()const {
        return ObjectsCurrent() && make.Get() && makeOwner.Get() && cdo.Get() && hitStruct.Get() &&
            make.Raw()==g_makeFn && cdo.Raw()==g_cdo && hitStruct.Raw()==g_hitStruct && R::OuterOf(g_makeFn)==makeOwner.Raw();
    }
};
bool Finite(const FVector& p){return std::isfinite(p.X) && std::isfinite(p.Y) && std::isfinite(p.Z);}

}  // namespace

bool Write(ParamFrame& frame, const wchar_t* param, void* actor, void* component, const FVector& location) {
    return WriteWhile(frame,param,actor,component,location,[]{return true;});
}
bool WriteWhile(ParamFrame& frame,const wchar_t* param,void* actor,void* component,const FVector& location,const std::function<bool()>& caller) {
    const auto point=location;
    if(!caller || !caller() || !param || !*param || !Finite(point))return false;
    HitSource source(actor,component);
    CachedObjRef function,owner;function.Set(frame.function());owner.Set(frame.function()?R::OuterOf(frame.function()):nullptr);
    if (!source.ObjectsCurrent() || !Resolve() || !frame.valid()) {
        UE_LOGW("hit_result: not built (GameplayStatics CDO=%d MakeHitResult=%d HitResult struct=%d frame=%d)",
                g_cdo ? 1 : 0, g_makeFn ? 1 : 0, g_hitStruct ? 1 : 0, frame.valid() ? 1 : 0);
        return false;
    }
    source.CaptureMaker();
    const auto current=[&]{return caller() && source.Current() && function.Get() && owner.Get() &&
        frame.function()==function.Raw() && R::OuterOf(frame.function())==owner.Raw();};
    if(!current())return false;
    const int32_t size = R::FindParamSize(g_makeFn, L"ReturnValue");
    if(!current())return false;
    const int32_t into = R::FindParamSize(frame.function(), param);
    if (!current() || R::PropertyInnerStruct(frame.function(),param)!=g_hitStruct || !current() || size <= 0 || size != into) {
        UE_LOGW("hit_result: not built: MakeHitResult returns %d bytes and '%ls' takes %d", size, param, into);
        return false;
    }
    std::vector<uint8_t> hit(static_cast<size_t>(size));
    return Build(actor, component, point, hit.data(), size,current) && current() &&
        R::PropertyInnerStruct(frame.function(),param)==g_hitStruct && current() && frame.SetRaw(param, hit.data(), size) && current();
}

bool WriteField(void* object, const wchar_t* field, void* actor, void* component, const FVector& location) {
    return WriteFieldWhile(object,field,actor,component,location,[]{return true;});
}
bool WriteFieldWhile(void* object,const wchar_t* field,void* actor,void* component,const FVector& location,const std::function<bool()>& caller) {
    const auto point=location;
    if(!caller || !caller() || !field || !*field || !object || !R::IsLive(object) || !Finite(point))return false;
    HitSource source(actor,component);CachedObjRef destination,type;destination.Set(object);type.Set(R::ClassOf(object));
    if(!source.ObjectsCurrent() || !type.Get() || !Resolve())return false;
    source.CaptureMaker();
    void* cls=type.Raw();
    const auto current=[&]{return caller() && source.Current() && destination.Get() && type.Get() && R::ClassOf(object)==cls;};
    if(!current())return false;
    const int32_t off = R::FindPropertyOffset(cls, field);
    if(!current())return false;
    void* inner = R::PropertyInnerStruct(cls, field);
    if(!current())return false;
    const int32_t size = R::FindParamSize(g_makeFn, L"ReturnValue");
    if(!current())return false;
    const auto objectSize=R::StructSize(cls);
    if (!current() || off<0 || off>objectSize || inner!=g_hitStruct || size<=0 || size>objectSize-off || size!=R::StructSize(inner) || !current()) {
        UE_LOGW("hit_result: not built: MakeHitResult returns %d bytes and '%ls' holds %d", size, field,
                inner ? R::StructSize(inner) : -1);
        return false;
    }
    std::vector<uint8_t> hit(static_cast<size_t>(size));
    if (!Build(actor, component, point, hit.data(), size,current) || !current() ||
        R::FindPropertyOffset(cls,field)!=off || R::PropertyInnerStruct(cls,field)!=inner ||
        R::StructSize(cls)<off || size>R::StructSize(cls)-off || !current()) return false;
    std::memcpy(reinterpret_cast<uint8_t*>(object) + off, hit.data(), hit.size());
    return true;
}

bool Readable() { return Resolve() && Members().ok; }

bool Parse(const void* hit, int32_t bytes, Hit* out) {
    if (!hit || !out || !Resolve()) return false;
    const Layout& l = Members();
    if (!l.ok || bytes < l.size) return false;
    const auto* b = static_cast<const uint8_t*>(hit);
    *out = Hit{};
    out->blocking = (b[l.blockByte] & l.blockMask) != 0;
    out->actor = Weak(b, Member::Actor);
    out->component = Weak(b, Member::Component);
    std::memcpy(&out->location, b + l.point, sizeof(FVector));
    std::memcpy(&out->normal, b + l.normal, sizeof(FVector));
    return true;
}

bool ReadField(void* object, const wchar_t* field, Hit* out) {
    void* cls = object && R::IsLive(object) ? R::ClassOf(object) : nullptr;
    if (!cls || !out || !Resolve()) return false;
    const int32_t off = R::FindPropertyOffset(cls, field);
    void* inner = R::PropertyInnerStruct(cls, field);
    if (off < 0 || inner != g_hitStruct) return false;
    return Parse(reinterpret_cast<const uint8_t*>(object) + off, R::StructSize(inner), out);
}

void* Component(const void* hit) {
    if (!hit || !Resolve()) return nullptr;
    return Weak(static_cast<const uint8_t*>(hit), Member::Component);
}

}  // namespace ue_wrap::hit_result
