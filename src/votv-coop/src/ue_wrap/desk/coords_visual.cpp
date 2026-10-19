#include "ue_wrap/desk/coords_panel.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/desk_ping.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/fname_utils.h"
#include <array>
#include <cmath>
#include <cstring>

namespace ue_wrap::coords_panel {
namespace {
namespace R = reflection;
R::InstanceOffset g_outer{L"coord_ping_outer"}, g_inner{L"coord_ping_inner"}, g_ping{L"coord_ping_ping"};
R::InstanceOffset g_edges{L"dynmat_linesEdges"}, g_lines{L"dynmat_linesInner"};
R::InstanceOffset g_circle{L"dynmat_ping"}, g_triangle{L"dynmat_triangle"};
bool Unit(float v) { return std::isfinite(v) && v >= 0.f && v <= 1.f; }
void* Desk() {
    void* d = console_desk::EnsureResolved() ? console_desk::Instance() : nullptr;
    if (!d || !R::IsLive(d)) return nullptr;
    CachedObjRef ref; ref.Set(d);
    return ref.Get();
}
bool Offsets(void* d, int32_t (&off)[3]) {
    off[0] = g_outer.Of(d); off[1] = g_inner.Of(d); off[2] = g_ping.Of(d);
    return off[0] >= 0 && off[1] >= 0 && off[2] >= 0;
}
void* Object(void* w, R::InstanceOffset& f) {
    const int32_t off = f.Of(w);
    void* obj = nullptr;
    if (off >= 0) std::memcpy(&obj, static_cast<uint8_t*>(w) + off, sizeof(obj));
    return obj && R::IsLive(obj) ? obj : nullptr;
}
bool Materials(void* w, std::array<CachedObjRef, 7>& out) {
    if (!w) return false;
    R::InstanceOffset* fields[] = {&g_edges, &g_lines};
    for (int group = 0; group < 2; ++group) {
        const int32_t off = fields[group]->Of(w);
        if (off < 0) return false;
        field_io::TArrayView a{};
        std::memcpy(&a, static_cast<uint8_t*>(w) + off, sizeof(a));
        if (!a.data || a.num != 3 || a.max < a.num || a.max > 64) return false;
        for (int i = 0; i < 3; ++i) {
            void* obj = nullptr;
            std::memcpy(&obj, a.data + i * sizeof(void*), sizeof(obj));
            if (!obj || !R::IsLive(obj)) return false;
            out[group * 3 + i].Set(obj);
        }
    }
    void* circle = Object(w, g_circle);
    if (!circle) return false;
    out[6].Set(circle);
    return true;
}
}
void* PresentationTarget() {
    if (!Desk()) return nullptr;
    void* w = Instance();
    std::array<CachedObjRef, 7> materials;
    return Materials(w, materials) ? Object(w, g_triangle) : nullptr;
}
bool ReadPingVisual(PingVisual& out) {
    void* d = Desk(); int32_t off[3];
    if (!d || !Offsets(d, off) || !desk_ping::ReadPinging(d, out.active)) return false;
    float values[3];
    for (int i = 0; i < 3; ++i) std::memcpy(&values[i], static_cast<uint8_t*>(d) + off[i], sizeof(float));
    if (!Unit(values[0]) || !Unit(values[1]) || !Unit(values[2])) return false;
    out.outer = values[0]; out.inner = values[1]; out.ping = values[2];
    return true;
}
bool WritePingVisual(const PingVisual& in) {
    if (!Unit(in.outer) || !Unit(in.inner) || !Unit(in.ping)) return false;
    void* d = Desk(); void* w = d ? Instance() : nullptr;
    int32_t off[3]; bool simulating = false;
    std::array<CachedObjRef, 7> materials;
    if (!d || !w || !Offsets(d, off) || !desk_ping::ReadPinging(d, simulating) ||
        simulating || !Materials(w, materials)) return false;
    CachedObjRef desk, widget; desk.Set(d); widget.Set(w);
    const uint32_t generation=world_identity::Generation();
    const auto current = [&] { return generation==world_identity::Generation() && desk.Get() == d && widget.Get() == w && Desk() == d && Instance() == w; };
    const float values[] = {in.outer, in.inner, in.ping};
    // Native process_coords repaints these values even while idle. Mirror the
    // accumulators only; NEVER start/advance the FSM or write its stage/flag.
    for (int i = 0; i < 3; ++i) std::memcpy(static_cast<uint8_t*>(d) + off[i], &values[i], sizeof(float));
    const R::FName alpha = fname_utils::StringToFName(L"alpha");
    if (!alpha.ComparisonIndex || !current()) return false;
    for (int i = 0; i < 7; ++i) {
        void* mat = materials[i].Get();
        if (!mat || !current()) return false;
        void* fn = R::FindDispatchFunctionCached(R::ClassOf(mat), L"SetScalarParameterValue");
        if (!fn) return false;
        ParamFrame frame(fn);
        if (!frame.valid() || !frame.Set<R::FName>(L"ParameterName", alpha) ||
            !frame.Set<float>(L"Value", values[i < 3 ? 0 : i < 6 ? 1 : 2]) ||
            !Call(mat, frame) || !current()) return false;
    }
    return true;
}
}
