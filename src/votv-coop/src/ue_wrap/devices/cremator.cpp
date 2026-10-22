#include "ue_wrap/devices/cremator.h"

#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/engine/world_identity.h"
#include "coop/element/portable_identity.h"
#include "ue_wrap/engine/engine.h"

#include <cstdint>
#include <cstring>
#include <chrono>
#include <cmath>

namespace ue_wrap::cremator {
namespace {
namespace R = reflection;
CachedObjRef g_machineClass;
CachedObjRef g_doorClass;
uint32_t g_generation = 0;
std::chrono::steady_clock::time_point g_nextScan{};
std::vector<void*> g_machines;
std::vector<int32_t> g_machineIndices;

bool IsOf(void* actor, void* base) {
    if (!actor || !base) return false;
    void* cls = R::ClassOf(actor);
    void* bases[] = {base};
    return cls && R::IsDescendantOfAny(cls, bases, 1);
}
}

bool EnsureResolved() {
    if (g_machineClass.Get() && g_doorClass.Get()) return true;
    if (!g_machineClass.Get()) g_machineClass.Set(object_index::ClassByName(L"cremator_C"));
    if (!g_doorClass.Get()) g_doorClass.Set(object_index::ClassByName(L"prop_swinger_crematorDoor_C"));
    return g_machineClass.Get() && g_doorClass.Get();
}

bool IsCremator(void* actor) { EnsureResolved(); return IsOf(actor, g_machineClass.Get()); }

const std::vector<void*>& Instances() {
    EnsureResolved();
    const uint32_t gen = world_identity::Generation();
    const auto now=std::chrono::steady_clock::now();
    if (g_generation == gen) {
        bool live = true;
        for (size_t i = 0; i < g_machines.size(); ++i)
            if (!R::IsLiveByIndex(g_machines[i], g_machineIndices[i])) { live = false; break; }
        if (live && (!g_machines.empty() || now<g_nextScan)) return g_machines;
    }
    g_machines.clear();
    g_machineIndices.clear();
    if (void* machineClass=g_machineClass.Get()) {
        const int32_t n = R::NumObjects();
        for (int32_t i = 0; i < n; ++i) {
            void* a = R::ObjectAt(i);
            if (a && IsOf(a, machineClass) && R::IsLive(a) && !R::NameStartsWith(R::NameOf(a), L"Default__")) {
                g_machines.push_back(a);
                g_machineIndices.push_back(R::InternalIndexOf(a));
            }
        }
    }
    g_generation = gen;
    g_nextScan=now+std::chrono::seconds(1);
    return g_machines;
}

namespace {
int32_t BoolOffset(void* actor, const wchar_t* name) {
    void* cls = actor ? R::ClassOf(actor) : nullptr;
    return cls ? R::FindPropertyOffset(cls, name) : -1;
}
bool ReadBool(void* actor, const wchar_t* name, bool& out) {
    const int32_t off = BoolOffset(actor, name);
    if (off < 0) return false;
    std::memcpy(&out, reinterpret_cast<const uint8_t*>(actor) + off, sizeof(out));
    return true;
}
}

bool CanUseLever(void* actor) {
    bool info = false, looking = false;
    return CanReplayLever(actor) && ReadBool(actor, L"lookingAt_info", info) && info &&
           ReadBool(actor, L"lookingAt_handle", looking) && looking;
}

bool CanReplayLever(void* actor) {
    bool animating = true;
    return IsCremator(actor) && ReadBool(actor, L"anim", animating) && !animating;
}

bool TryReadLeverActive(void* actor, bool& active) {
    return IsCremator(actor) && ReadBool(actor, L"isActive", active);
}


bool CallUseLever(void* actor) {
    if (!IsCremator(actor)) return false;
    void* fn = R::FindFunction(R::ClassOf(actor), L"useLever");
    if (!fn) return false;
    ue_wrap::ParamFrame frame(fn);
    return frame.valid() && ue_wrap::Call(actor, frame);
}


std::wstring LeverWireKey(void* actor) {
    if (!IsCremator(actor)) return {};
    const std::wstring base = coop::element::PortableWireKey(actor);
    return base.empty() ? std::wstring{} : base + L"/lever";
}

void* FindByLeverWireKey(const std::wstring& key) {
    constexpr wchar_t suffix[] = L"/lever";
    if (key.size() <= 6 || key.compare(key.size() - 6, 6, suffix) != 0) return nullptr;
    const std::wstring base = key.substr(0, key.size() - 6);
    void* match = nullptr;
    for (void* actor : Instances()) {
        if (!actor || coop::element::PortableWireKey(actor) != base) continue;
        if (match && match != actor) return nullptr;  // never guess between duplicate identities
        match = actor;
    }
    return match;
}
}
