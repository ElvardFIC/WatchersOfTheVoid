#include "ue_wrap/devices/cremator.h"

#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/engine/world_identity.h"
#include "coop/element/portable_identity.h"
#include "ue_wrap/engine/engine.h"

#include <cstdint>
#include <cstring>

namespace ue_wrap::cremator {
namespace {
namespace R = reflection;
void* g_machineClass = nullptr;
void* g_doorClass = nullptr;
uint32_t g_generation = 0;
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
    if (g_machineClass && g_doorClass) return true;
    g_machineClass = R::FindClass(L"cremator_C");
    g_doorClass = R::FindClass(L"prop_swinger_crematorDoor_C");
    return g_machineClass && g_doorClass;
}

bool IsCrematorDoor(void* actor) { return EnsureResolved() && IsOf(actor, g_doorClass); }
bool IsCremator(void* actor) { return EnsureResolved() && IsOf(actor, g_machineClass); }

const std::vector<void*>& Instances() {
    EnsureResolved();
    const uint32_t gen = world_identity::Generation();
    if (g_generation == gen) {
        bool live = true;
        for (size_t i = 0; i < g_machines.size(); ++i)
            if (!R::IsLiveByIndex(g_machines[i], g_machineIndices[i])) { live = false; break; }
        if (live && !g_machines.empty()) return g_machines;
    }
    g_machines.clear();
    g_machineIndices.clear();
    if (g_machineClass) {
        const int32_t n = R::NumObjects();
        for (int32_t i = 0; i < n; ++i) {
            void* a = R::ObjectAt(i);
            if (a && IsCremator(a) && R::IsLive(a) && !R::NameStartsWith(R::NameOf(a), L"Default__")) {
                g_machines.push_back(a);
                g_machineIndices.push_back(R::InternalIndexOf(a));
            }
        }
    }
    g_generation = gen;
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

bool IsLeverActive(void* actor) {
    bool active = false;
    return TryReadLeverActive(actor, active) && active;
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
