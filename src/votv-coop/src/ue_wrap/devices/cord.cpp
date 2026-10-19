#include "ue_wrap/devices/cord.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include <cmath>
#include <cstring>

namespace ue_wrap::cord {
namespace {
namespace R = reflection;
CachedObjRef g_classes[2];
bool Is(void* a, int i) {
    if (!a || !R::IsLive(a)) return false;
    void* cls = g_classes[i].Get();
    if (!cls) {
        cls = object_index::ClassByName(i ? L"cordSocket_C" : L"cord_C");
        if (cls) g_classes[i].Set(cls);
    }
    return cls && R::IsDescendantOfAny(R::ClassOf(a), &cls, 1);
}
void* Obj(void* a, R::InstanceOffset& field) {
    if (!a) return nullptr;
    const int32_t off = field.Of(a);
    void* p = nullptr;
    if (off >= 0) std::memcpy(&p, static_cast<uint8_t*>(a) + off, sizeof(p));
    return p && R::IsLive(p) ? p : nullptr;
}
bool Bit(void* a, const wchar_t* name, bool& out) {
    if (!a) return false;
    int32_t off = -1; uint8_t mask = 0;
    if (!R::FindBoolProperty(R::ClassOf(a), name, off, mask) || off < 0 || !mask) return false;
    out = (static_cast<uint8_t*>(a)[off] & mask) != 0;
    return true;
}
bool Location(void* component, FVector& out) {
    if (!component) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(component), L"K2_GetComponentLocation");
    if (!fn) return false;
    ParamFrame f(fn);
    if (!f.valid() || !Call(component, f)) return false;
    out = f.Get<FVector>(L"ReturnValue");
    return std::isfinite(out.X) && std::isfinite(out.Y) && std::isfinite(out.Z);
}
bool SetLocation(void* component, const FVector& location) {
    if (!component || !R::IsLive(component)) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(component), L"K2_SetWorldLocation");
    if (!fn) return false;
    ParamFrame f(fn);
    return f.valid() && f.Set<FVector>(L"NewLocation", location) &&
        f.Set<bool>(L"bSweep", false) && f.Set<bool>(L"bTeleport", true) && Call(component, f);
}
}
bool IsCord(void* a) { return Is(a, 0); }
bool IsSocket(void* a) { return Is(a, 1); }
void* Parent(void* a) { static R::InstanceOffset f{L"parent"}; return Obj(a, f); }
void* Socket(void* a) { static R::InstanceOffset f{L"socket"}; return Obj(a, f); }
bool ReadHeadPose(void* a, FVector& location, FRotator& rotation) {
    static R::InstanceOffset f{L"head"};
    void* head = Obj(a, f);
    if (!IsCord(a) || !Location(head, location)) return false;
    rotation = engine::GetComponentWorldRotation(head);
    return std::isfinite(rotation.Pitch) && std::isfinite(rotation.Yaw) && std::isfinite(rotation.Roll);
}
bool WriteHeadPose(void* a, const FVector& location, const FRotator& rotation) {
    static R::InstanceOffset f{L"head"};
    if (!IsCord(a)) return false;
    CachedObjRef actor, head; actor.Set(a); head.Set(Obj(a, f));
    return head.Get() && SetLocation(head.Get(), location) && actor.Get() && head.Get() &&
        engine::SetComponentWorldRotation(head.Get(), rotation);
}
void* Occupant(void* a) { static R::InstanceOffset f{L"cord"}; return Obj(a, f); }
bool Dragging(void* a) { bool b = false; return Bit(a, L"drag", b) && b; }
bool Powered(void* a, bool& out) { return Bit(a, L"powered", out); }
bool Disconnect(void* a) {
    return IsCord(a) && component_calls::CallParamlessNamed(a, L"disconnect");
}
bool Connect(void* a, void* socket) {
    if (!IsCord(a) || !IsSocket(socket)) return false;
    CachedObjRef cordRef, socketRef; cordRef.Set(a); socketRef.Set(socket);
    const auto world = world_identity::Generation();
    auto current = [&] { return world == world_identity::Generation() && cordRef.Get() && socketRef.Get(); };
    if (Socket(a) == socket && Occupant(socket) == a) return true;
    void* occupant = Occupant(socket);
    if (occupant && occupant != a) return false;
    // cord_sync guards the delayed reattach resume against an already-reconnected cord.
    if (Socket(a) && !Disconnect(a)) return false;
    if (!current()) return false;
    // Power/disconnect callbacks can replace either actor or connect another plug.
    // Never continue this older operation over the callback's new connection.
    occupant = Occupant(socket);
    if (Socket(a) || (occupant && occupant != a)) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(a), L"connect");
    if (!fn) return false;
    ParamFrame f(fn);
    // The native load=true bypasses occupancy; network calls always use the regular verb.
    return f.valid() && f.Set<void*>(L"socket", socket) && f.Set<bool>(L"load", false) &&
        Call(a, f) && current() && Socket(a) == socket && Occupant(socket) == a;
}
bool SetPower(void* socket, bool powered) {
    bool old = false;
    if (!IsSocket(socket) || !Powered(socket, old)) return false;
    if (old == powered) return true;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(socket), L"setActiveTrigger");
    if (!fn) return false;
    ParamFrame f(fn);
    return f.valid() && f.Set<bool>(L"active", powered) && Call(socket, f);
}
bool InCableReach(void* a, void* socket) {
    static R::InstanceOffset root{L"root"}, dir{L"dir"}, length{L"cordMaxLength"};
    if (!IsCord(a) || !IsSocket(socket)) return false;
    CachedObjRef cordRef, socketRef, rootRef, dirRef;
    cordRef.Set(a); socketRef.Set(socket); rootRef.Set(Obj(a, root)); dirRef.Set(Obj(socket, dir));
    const auto world = world_identity::Generation();
    auto current = [&] {
        return world == world_identity::Generation() && cordRef.Get() && socketRef.Get() &&
            rootRef.Get() && dirRef.Get() && Obj(a, root) == rootRef.Get() && Obj(socket, dir) == dirRef.Get();
    };
    FVector from{}, to{}; float limit = 0;
    const int32_t off = length.Of(a);
    if (off < 0 || !current() || !Location(rootRef.Get(), from) || !current() ||
        !Location(dirRef.Get(), to) || !current()) return false;
    std::memcpy(&limit, static_cast<uint8_t*>(a) + off, sizeof(limit));
    if (!std::isfinite(limit) || limit <= 0) return false;
    const double x = double(from.X)-to.X, y = double(from.Y)-to.Y, z = double(from.Z)-to.Z;
    // Native ReceiveTick disconnects at cordMaxLength * 1.2 + 100.
    const double reach = double(limit) * 1.2 + 100.;
    return x*x+y*y+z*z < reach*reach;
}
}
