// ue_wrap/devices/coffee.cpp -- see ue_wrap/devices/coffee.h.

#include "ue_wrap/devices/coffee.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"

namespace ue_wrap::coffee {
namespace {

namespace R = reflection;

// A class as this world holds it, looked up by name again once its slot or serial no longer holds the
// kept one (a Blueprint class can be a new object in a new world).
struct Cls {
    const wchar_t* name;
    ue_wrap::CachedObjRef ref;
};
Cls g_vending{L"prop_coffeevending_C", {}};
Cls g_maker{L"prop_coffeeMaker_C", {}};
Cls g_bag{L"prop_coffeeBag_C", {}};

void* ClassFor(Cls& c) {
    if (void* k = c.ref.Get()) return k;
    void* k = ue_wrap::object_index::ClassByName(c.name);
    if (k) c.ref.Set(k);
    return c.ref.Get();
}

// A Blueprint bool is an FBoolProperty: its byte offset and the bit inside that byte, since bools can pack
// several to a byte and a raw byte read would read a neighbour's bit as this flag's. Found once per class
// object, again when the maker's class is a new object (a new world).
struct BoolField {
    const wchar_t* name;
    ue_wrap::CachedObjRef cls;
    int32_t off = -1;
    uint8_t mask = 0;
};
BoolField g_hasCup{L"hasCup"};
BoolField g_hasCoffee{L"hasCoffee"};
BoolField g_coffeeReady{L"coffeeReady"};
R::InstanceOffset g_amounts{L"amounts"};

bool ReadBool(void* obj, void* cls, BoolField& f, bool& out) {
    ue_wrap::CachedObjRef actorRef, classRef;
    actorRef.Set(obj); classRef.Set(cls);
    if (!f.cls.Is(cls)) {
        f.cls.Set(cls);
        f.off = -1;
        f.mask = 0;
    }
    if (f.off < 0) {
        if (!R::FindBoolProperty(cls, f.name, f.off, f.mask)) f.off = -1;
    }
    if (!actorRef.Get() || !classRef.Get() || !f.cls.Is(cls) || f.off < 0 || !f.mask) return false;
    out = (*(reinterpret_cast<const uint8_t*>(obj) + f.off) & f.mask) != 0;
    return true;
}

template <class T>
bool ReadField(void* obj, R::InstanceOffset& off, T& out) {
    ue_wrap::CachedObjRef ref; ref.Set(obj);
    const int32_t o = off.Of(obj);
    if (o < 0 || !ref.Get()) return false;
    out = *reinterpret_cast<const T*>(reinterpret_cast<const char*>(obj) + o);
    return true;
}

}  // namespace

Device DeviceOf(void* actor) {
    ue_wrap::CachedObjRef actorRef, classRef;
    actorRef.Set(actor);
    void* cls = actor ? R::ClassOf(actor) : nullptr;
    if (!cls) return Device::None;
    classRef.Set(cls);
    const auto current = [&] { return actorRef.Get() && classRef.Get() && R::ClassOf(actor) == cls; };
    void* vending = ClassFor(g_vending);
    if (!current()) return Device::None;
    if (cls == vending) return Device::Vending;
    void* maker = ClassFor(g_maker);
    if (!current()) return Device::None;
    if (cls == maker) return Device::Maker;
    return Device::None;
}

bool IsBag(void* actor) {
    ue_wrap::CachedObjRef ref, classRef; ref.Set(actor);
    void* cls = actor ? R::ClassOf(actor) : nullptr;
    if (!cls) return false;
    classRef.Set(cls);
    void* bag = ClassFor(g_bag);
    return ref.Get() && classRef.Get() && R::ClassOf(actor) == cls && cls == bag;
}

bool ReadMakerFlags(void* maker, MakerFlags& out) {
    ue_wrap::CachedObjRef ref; ref.Set(maker);
    if (DeviceOf(maker) != Device::Maker || !ref.Get()) return false;
    void* const cls = R::ClassOf(maker);
    ue_wrap::CachedObjRef classRef; classRef.Set(cls);
    MakerFlags f;
    if (!ReadBool(maker, cls, g_hasCup, f.hasCup) || !ReadBool(maker, cls, g_hasCoffee, f.hasCoffee) ||
        !ReadBool(maker, cls, g_coffeeReady, f.coffeeReady) || !ref.Get() || !classRef.Get() ||
        R::ClassOf(maker) != cls) return false;
    out = f;
    return true;
}

bool ReadBagAmounts(void* bag, int32_t& out) {
    ue_wrap::CachedObjRef ref; ref.Set(bag);
    int32_t amount = 0;
    if (!IsBag(bag) || !ref.Get() || !ReadField(bag, g_amounts, amount) || !ref.Get()) return false;
    out = amount;
    return true;
}

}  // namespace ue_wrap::coffee
