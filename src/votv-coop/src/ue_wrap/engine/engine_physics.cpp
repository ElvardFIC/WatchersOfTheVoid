// Native rigid-body calls use bounded reflected frames and incarnation-bound metadata.
#include "ue_wrap/engine/engine_physics.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace ue_wrap::engine {
namespace {
namespace P = profile;
namespace R = reflection;
constexpr int32_t kFrameBytes = 64;
constexpr size_t kMaxLayouts = 128, kMaxMembers = 256, kMaxFields = 1024, kMaxAncestors = 32;
struct Busy {
    bool& flag;
    explicit Busy(bool& f) : flag(f) { flag = true; }
    ~Busy() { flag = false; }
    Busy(const Busy&) = delete;
};
struct Lifetime {
    CachedObjRef object, cls;
    uint32_t world = world_identity::Generation();
    explicit Lifetime(void* p) {
        if (!p || !R::IsLive(p)) return;
        object.Set(p);
        if (object.Get()) cls.Set(R::ClassOf(p));
    }
    bool Current() const {
        return world == world_identity::Generation() && object.Get() && cls.Get() &&
               R::ClassOf(object.Raw()) == cls.Raw();
    }
};
CachedObjRef g_primitiveClass;
bool Primitive(CachedObjRef& out) {
    if (!g_primitiveClass.Get()) {
        void* cls = R::FindClass(P::name::PrimitiveComponentClass);
        if (!cls || !R::IsLive(cls)) return false;
        g_primitiveClass.Set(cls);
    }
    out = g_primitiveClass;
    return out.Get() != nullptr;
}
struct Component : Lifetime {
    CachedObjRef primitive;
    bool ready = false;
    explicit Component(void* p) : Lifetime(p) {
        if (!Lifetime::Current() || !Primitive(primitive) || !Lifetime::Current()) return;
        void* base = primitive.Get();
        ready = base && R::IsDescendantOfAny(cls.Raw(), &base, 1);
    }
    bool Current() const { return ready && Lifetime::Current() && primitive.Get(); }
};
bool Span(int32_t off, int32_t size, int32_t frame) {
    return off >= 0 && size > 0 && frame >= size && off <= frame - size;
}
bool Finite(const FVector& v) { return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z); }
struct Field { int32_t off = -1, size = 0; uint8_t mask = 0; };
struct Layout {
    CachedObjRef fn, owner, primitive;
    int32_t frame = 0;
    std::array<Field, 3> fields{};
    bool Current() const {
        return fn.Get() && owner.Get() && primitive.Get() && R::OuterOf(fn.Raw()) == owner.Raw();
    }
};
enum class Shape { Simulate, Linear, Angular, Query, Center };
struct Thunk {
    const wchar_t* name;
    Shape shape;
    Layout layout;
    bool resolving = false;
};
Thunk g_setSimulate{P::name::SetSimulatePhysicsFn, Shape::Simulate};
Thunk g_setLinVel{P::name::SetPhysicsLinearVelocityFn, Shape::Linear};
Thunk g_setAngVel{P::name::SetPhysicsAngularVelocityInDegreesFn, Shape::Angular};
Thunk g_isSimulating{P::name::IsSimulatingPhysicsFn, Shape::Query};
Thunk g_getCenterOfMass{P::name::GetCenterOfMassFn, Shape::Center};

template<class Current>
bool Resolve(Thunk& thunk, const CachedObjRef& primitive, Current current, Layout& out) {
    if (!current() || !primitive.Get() || thunk.resolving) return false;
    if (thunk.layout.Current() && thunk.layout.primitive.Is(primitive.Raw())) {
        out = thunk.layout;
        return current();
    }
    Busy busy(thunk.resolving);
    Layout next;
    next.primitive = primitive;
    const uint32_t world = world_identity::Generation();
    void* declaring = nullptr;
    void* fn = R::FindDispatchFunction(primitive.Raw(), thunk.name, &declaring);
    if (!current() || !primitive.Get() || !fn || !R::IsLive(fn)) return false;
    next.fn.Set(fn);
    void* owner = R::OuterOf(fn);
    if (!owner || !R::IsLive(owner) || (declaring && owner != declaring)) return false;
    next.owner.Set(owner);
    const auto valid = [&] { return world == world_identity::Generation() && current() && next.Current(); };
    if (!valid()) return false;
    next.frame = R::FunctionFrameSize(fn);
    if (!valid() || next.frame <= 0 || next.frame > kFrameBytes) return false;
    auto field = [&](int index, const wchar_t* name, int32_t bytes, bool bit = false) {
        if (!valid()) return false;
        Field f;
        f.size = R::FindParamSize(fn, name);
        if (!valid() || f.size != bytes) return false;
        if (bit) {
            if (!R::FindBoolProperty(fn, name, f.off, f.mask) || !f.mask ||
                (f.mask != 0xff && (f.mask & (f.mask - 1)))) return false;
        } else f.off = R::FindParamOffset(fn, name);
        if (!valid() || !Span(f.off, bytes, next.frame)) return false;
        next.fields[index] = f;
        return true;
    };
    int count = 2;
    bool ok = false;
    switch (thunk.shape) {
    case Shape::Simulate:
        count = 1; ok = field(0, L"bSimulate", 1, true); break;
    case Shape::Linear:
    case Shape::Angular:
        count = 3;
        ok = field(0, thunk.shape == Shape::Linear ? L"NewVel" : L"NewAngVel", sizeof(FVector)) &&
             field(1, L"BoneName", sizeof(R::FName)) && field(2, L"bAddToCurrent", 1, true);
        break;
    case Shape::Query:
        ok = field(0, L"BoneName", sizeof(R::FName)) && field(1, L"ReturnValue", 1, true); break;
    case Shape::Center:
        ok = field(0, L"BoneName", sizeof(R::FName)) && field(1, L"ReturnValue", sizeof(FVector)); break;
    }
    if (!ok || !valid()) return false;
    for (int i = 0; i < count; ++i) for (int j = i + 1; j < count; ++j) {
        const auto& a = next.fields[i]; const auto& b = next.fields[j];
        if (a.off < b.off + b.size && b.off < a.off + a.size &&
            !(a.mask && b.mask && a.off == b.off && !(a.mask & b.mask))) return false;
    }
    thunk.layout = next;
    out = next;
    return true;
}
using Frame = std::array<unsigned char, kFrameBytes>;
template<class T> void Put(Frame& frame, const Field& f, const T& value) {
    std::memcpy(frame.data() + f.off, &value, sizeof(value));
}
void Bit(Frame& frame, const Field& f, bool value) {
    auto& byte = frame[f.off];
    byte = value ? static_cast<uint8_t>(byte | f.mask) : static_cast<uint8_t>(byte & ~f.mask);
}
template<class Current, class Fill, class Read>
bool Invoke(Thunk& thunk, void* component, Current caller, Fill fill, Read read) {
    Component body(component);
    auto current = [&] { return body.Current() && caller(); };
    Layout layout;
    if (!current() || !Resolve(thunk, body.primitive, current, layout)) return false;
    alignas(16) Frame frame{};
    fill(frame, layout);
    if (!current() || !layout.Current()) return false;
    const bool ok = R::CallFunction(component, layout.fn.Raw(), frame.data());
    if (!ok || !current() || !layout.Current()) return false;
    return read(frame, layout);
}
template<class Current>
bool SimulateWhile(void* component, bool simulate, Current current) {
    return Invoke(g_setSimulate, component, current,
        [&](Frame& f, const Layout& l) { Bit(f, l.fields[0], simulate); },
        [](const Frame&, const Layout&) { return true; });
}
template<class Current>
bool QueryWhile(void* component, Current current, bool& out) {
    bool value = false;
    const bool ok = Invoke(g_isSimulating, component, current,
        [](Frame& f, const Layout& l) { Put(f, l.fields[0], R::FName{0,0}); },
        [&](const Frame& f, const Layout& l) { value = (f[l.fields[1].off] & l.fields[1].mask) != 0; return true; });
    if (ok) out = value;
    return ok;
}
bool SetVelocity(Thunk& thunk, void* component, float x, float y, float z) {
    const FVector value{x,y,z};
    if (!Finite(value)) return false;
    return Invoke(thunk, component, [] { return true; },
        [&](Frame& f, const Layout& l) {
            Put(f, l.fields[0], value); Put(f, l.fields[1], R::FName{0,0}); Bit(f, l.fields[2], false);
        }, [](const Frame&, const Layout&) { return true; });
}
struct ClassMembers {
    CachedObjRef cls;
    std::vector<CachedObjRef> chain;
    std::vector<int32_t> offsets;
    bool Current() const {
        if (!cls.Get()) return false;
        for (size_t i = 0; i < chain.size(); ++i) {
            if (!chain[i].Get()) return false;
            if (i + 1 < chain.size() && R::SuperStructOf(chain[i].Raw()) != chain[i+1].Raw()) return false;
        }
        return true;
    }
};
std::deque<ClassMembers> g_componentMembers;
bool g_stopping = false;
template<class Current>
bool ComponentMembers(void* cls, Current current, std::vector<int32_t>& out) {
    for (const auto& cached : g_componentMembers)
        if (cached.cls.Is(cls) && cached.Current()) { out = cached.offsets; return current(); }
    ClassMembers next;
    next.cls.Set(cls);
    const int32_t size = R::StructSize(cls);
    if (!current() || !next.cls.Get() || size <= 0) return false;
    size_t fieldsSeen = 0;
    void* c = cls;
    while (c) {
        if (next.chain.size() >= kMaxAncestors || !R::IsLive(c)) return false;
        for (const auto& seen : next.chain) if (seen.Raw() == c) return false;
        CachedObjRef ref; ref.Set(c); next.chain.push_back(ref);
        const auto n = R::ToString(R::NameOf(c));
        if (!current() || !next.Current()) return false;
        if (n.size() < 2 || n.compare(n.size()-2, 2, L"_C") != 0) break;
        const auto fields = R::EnumerateStructFields(c);
        if (!current() || !next.Current() || fields.size() > kMaxFields - fieldsSeen) return false;
        fieldsSeen += fields.size();
        for (const auto& f : fields) {
            if (!(f.flags & P::cpf::InstancedReference) || f.size != sizeof(void*)) continue;
            if (!Span(f.offset, sizeof(void*), size)) return false;
            int32_t off = -1; std::wstring type;
            if (!R::PropertyOffsetAndClass(c, f.name.c_str(), off, type) || !current() || !next.Current()) return false;
            if (type != L"ObjectProperty" || off != f.offset) continue;
            if (std::find(next.offsets.begin(), next.offsets.end(), off) != next.offsets.end()) continue;
            if (next.offsets.size() >= kMaxMembers) return false;
            next.offsets.push_back(off);
        }
        c = R::SuperStructOf(c);
        if (!current() || !next.Current()) return false;
    }
    if (!current() || !next.Current()) return false;
    out = next.offsets;
    std::erase_if(g_componentMembers, [](const ClassMembers& row) { return !row.Current(); });
    if (g_componentMembers.size() >= kMaxLayouts) g_componentMembers.pop_front();
    g_componentMembers.push_back(std::move(next));
    return true;
}
void* Member(void* actor, int32_t off) {
    void* result = nullptr;
    std::memcpy(&result, static_cast<const unsigned char*>(actor) + off, sizeof(result));
    return result;
}
} // namespace

bool IsPrimitiveComponent(void* component) { return Component(component).Current(); }
bool SetComponentSimulatePhysics(void* component, bool simulate) {
    return SimulateWhile(component, simulate, [] { return true; });
}
bool SetComponentLinearVelocity(void* component, float x, float y, float z) {
    return SetVelocity(g_setLinVel, component, x, y, z);
}
bool SetComponentAngularVelocity(void* component, float x, float y, float z) {
    return SetVelocity(g_setAngVel, component, x, y, z);
}
bool IsComponentSimulatingPhysics(void* component) {
    bool result = false;
    return QueryWhile(component, [] { return true; }, result) && result;
}
bool GetComponentCenterOfMass(void* component, FVector& out) {
    FVector value{};
    const bool ok = Invoke(g_getCenterOfMass, component, [] { return true; },
        [](Frame& f, const Layout& l) { Put(f, l.fields[0], R::FName{0,0}); },
        [&](const Frame& f, const Layout& l) {
            std::memcpy(&value, f.data() + l.fields[1].off, sizeof(value)); return Finite(value);
        });
    if (ok) out = value;
    return ok;
}
int StopActorSimulating(void* actor) {
    if (g_stopping) return 0;
    Busy busy(g_stopping);
    Lifetime owner(actor);
    if (!owner.Current()) return 0;
    std::vector<int32_t> offsets;
    if (!ComponentMembers(owner.cls.Raw(), [&] { return owner.Current(); }, offsets)) return 0;
    int stopped = 0;
    for (int32_t off : offsets) {
        if (!owner.Current()) break;
        void* comp = Member(actor, off);
        Component component(comp);
        auto current = [&] {
            return owner.Current() && component.Current() && Member(actor, off) == comp && R::OuterOf(comp) == actor;
        };
        if (!current()) continue;
        bool simulates = false;
        if (!QueryWhile(comp, current, simulates) || !current() || !simulates) continue;
        if (SimulateWhile(comp, false, current)) ++stopped;
    }
    return stopped;
}
} // namespace ue_wrap::engine
