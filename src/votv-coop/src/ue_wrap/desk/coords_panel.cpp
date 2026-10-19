// ue_wrap/desk/coords_panel.cpp -- see ue_wrap/desk/coords_panel.h. The desk half of the
// instance chain stays in console_desk (AtlasUiCoordsSlot), and the desk's updateCoordCoords
// verb is reached through console_desk::CallUpdateCoordCoords.

#include "ue_wrap/desk/coords_panel.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/desk/console_desk.h"

#include <chrono>
#include <cmath>
#include <cstring>

namespace ue_wrap::coords_panel {
namespace {

namespace R = ue_wrap::reflection;

// ui_coordinates (the coords-panel widget; SINGLETON child of the atlas) --
// the REAL dish-aim state + its repaint verb.
void* g_uiCoordsCls = nullptr;
CachedObjRef g_uiCoordsClassIdentity;
void* g_uiCoordsInst = nullptr;
int32_t g_offViewCoordinate = -1;  // FVector2D
int32_t g_offCoordinate0 = -1;     // FVector2D
int32_t g_offCoordinate1 = -1;     // FVector2D
int32_t g_offCoordinate2 = -1;     // FVector2D
int32_t g_offSelected = -1;
int32_t g_offDirection = -1;       // bool -- the catch-gate toggle
int32_t g_offCanPing = -1;         // canPing's byte, with g_maskCanPing its bit
uint8_t g_maskCanPing = 0;
void* g_updCursorLocationsFn = nullptr;

// The REQUIRED set (cls + the five aim offsets + the repaint verb) resolved --
// the steady-state fast gate. Direction is OPTIONAL at the read site (as in
// the pre-split code) but resolves in the same pass from the same class dump.
bool g_required = false;
std::chrono::steady_clock::time_point g_nextResolve{};

void ResolvePass() {
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextResolve) return;
    g_nextResolve = now + std::chrono::seconds(2);

    if (!g_uiCoordsCls) g_uiCoordsCls = R::FindClass(L"ui_coordinates_C");
    if (g_uiCoordsCls) {
        if (g_offViewCoordinate < 0)
            g_offViewCoordinate = R::FindPropertyOffset(g_uiCoordsCls, L"viewCoordinate");
        if (g_offCoordinate0 < 0)
            g_offCoordinate0 = R::FindPropertyOffset(g_uiCoordsCls, L"Coordinate_0");
        if (g_offCoordinate1 < 0)
            g_offCoordinate1 = R::FindPropertyOffset(g_uiCoordsCls, L"Coordinate_1");
        if (g_offCoordinate2 < 0)
            g_offCoordinate2 = R::FindPropertyOffset(g_uiCoordsCls, L"Coordinate_2");
        if (g_offSelected < 0)
            g_offSelected = R::FindPropertyOffset(g_uiCoordsCls, L"selected");
        if (g_offDirection < 0)
            g_offDirection = R::FindPropertyOffset(g_uiCoordsCls, L"Direction");
        if (g_offCanPing < 0 && !R::FindBoolProperty(g_uiCoordsCls, L"canPing", g_offCanPing, g_maskCanPing))
            g_offCanPing = -1;
        if (!g_updCursorLocationsFn)
            g_updCursorLocationsFn = R::FindFunction(g_uiCoordsCls, L"updCursorLocations");
    }

    if (!g_required && g_uiCoordsCls && g_updCursorLocationsFn &&
        g_offViewCoordinate >= 0 && g_offCoordinate0 >= 0 && g_offCoordinate1 >= 0 &&
        g_offCoordinate2 >= 0 && g_offSelected >= 0) {
        g_required = true;
        UE_LOGI("coords_panel: resolved -- view/c0/sel/dir=0x%X/0x%X/0x%X/0x%X updCursor=%s",
                g_offViewCoordinate, g_offCoordinate0, g_offSelected, g_offDirection,
                g_updCursorLocationsFn ? "yes" : "NO");
    }
}

}  // namespace

void* Instance() {
    // A live old widget may survive reconstruction. Follow the desk's current
    // chain on every lookup instead of treating a live cache as authoritative.
    // Resolve via the desk's OWN field chain (desk.Widget -> atlas.ui_coordinates), which is the
    // game's own access path -- getCoordWidget does exactly this. Resolving instead by
    // FindObjectsByClass(ui_coordinates_C) and taking the first hit grabs the cooked widget-tree
    // TEMPLATE: it loads with the class, reads live, is not Default__-named and is permanently
    // resident, so it wins on lowest index and gets cached forever. The host then writes dot
    // state into an invisible template and the client reads that template's frozen fields, which
    // is a star-map mirror with nothing to stream.
    void* coords = ue_wrap::console_desk::AtlasUiCoordsSlot();
    if (!coords || !R::IsLive(coords)) return nullptr;
    void* cls = R::ClassOf(coords);
    void* base = cls;
    for (int depth=0; base && depth<16; ++depth) {
        if (R::NameEquals(R::NameOf(base), L"ui_coordinates_C")) break;
        base = R::SuperStructOf(base);
    }
    if (!base || !R::NameEquals(R::NameOf(base), L"ui_coordinates_C")) return nullptr;
    if (!g_uiCoordsClassIdentity.Is(base)) {
        g_uiCoordsClassIdentity.Set(base); g_uiCoordsCls=base;
        g_offViewCoordinate=g_offCoordinate0=g_offCoordinate1=g_offCoordinate2=g_offSelected=-1;
        g_offDirection=g_offCanPing=-1; g_maskCanPing=0;
        g_updCursorLocationsFn=nullptr; g_required=false; g_nextResolve={};
        ResolvePass();
    }
    if (!g_required) ResolvePass();
    const bool changed = coords != g_uiCoordsInst;
    g_uiCoordsInst = coords;
    if (changed) UE_LOGI("coords_panel: ui_coordinates LIVE instance resolved via desk.Widget chain (%p)", coords);
    return g_uiCoordsInst;
}

bool ReadDishAim(DishAim& out) {
    void* w = Instance();
    if (!w || g_offViewCoordinate < 0 || g_offCoordinate0 < 0 ||
        g_offCoordinate1 < 0 || g_offCoordinate2 < 0 || g_offSelected < 0)
        return false;
    auto* p = reinterpret_cast<uint8_t*>(w);
    auto rd = [&](int32_t off, float& x, float& y) {
        std::memcpy(&x, p + off + 0, sizeof(float));
        std::memcpy(&y, p + off + 4, sizeof(float));
    };
    rd(g_offViewCoordinate, out.viewX, out.viewY);
    rd(g_offCoordinate0, out.c0X, out.c0Y);
    rd(g_offCoordinate1, out.c1X, out.c1Y);
    rd(g_offCoordinate2, out.c2X, out.c2Y);
    std::memcpy(&out.selected, p + g_offSelected, sizeof(int32_t));
    out.direction = (g_offDirection >= 0 && *(p + g_offDirection)) ? 1 : 0;
    return true;
}

bool ReadCanPing(bool& out) {
    void* w = Instance();
    if (!w || g_offCanPing < 0) return false;
    out = (*(reinterpret_cast<const uint8_t*>(w) + g_offCanPing) & g_maskCanPing) != 0;
    return true;
}

bool AimCanPing(const DishAim& aim) {
    const float side0 = std::hypot(aim.c0X - aim.c1X, aim.c0Y - aim.c1Y);
    const float side1 = std::hypot(aim.c1X - aim.c2X, aim.c1Y - aim.c2Y);
    const float side2 = std::hypot(aim.c2X - aim.c0X, aim.c2Y - aim.c0Y);
    // Heron's area over the half perimeter, the widget's `perim`; its Sqrt and Divide answer 0 where they cannot.
    const float half = (side0 + side1 + side2) / 2.f;
    const float square = half * (half - side0) * (half - side1) * (half - side2);
    const float inradius = half > 0.f && square > 0.f ? std::sqrt(square) / half : 0.f;
    return side0 <= 740.f && side1 <= 740.f && side2 <= 740.f && inradius > 1.f;
}

bool ReadAreaSize(float& width, float& height) {
    void* w = Instance();
    void* fn = (w && g_uiCoordsCls) ? R::FindDispatchFunctionCached(g_uiCoordsCls, L"getAreaSize") : nullptr;
    if (!fn) return false;
    ue_wrap::ParamFrame f(fn);
    if (!f.valid() || !ue_wrap::Call(w, f)) return false;
    float size[2] = {0.f, 0.f};
    if (!f.GetRaw(L"size", size, static_cast<int32_t>(sizeof(size)))) return false;
    width = size[0];
    height = size[1];
    return width > 0.f && height > 0.f;
}

// The LIVE cursor apply (the DeskCursorPose stream, ~60 Hz interpolated).
// INVARIANT -- it writes ONLY viewCoordinate and dispatches NOTHING. It must NEVER call
// updCursorLocations: that is the committed-triangle plus pingDishes setRot loop, N UFunction
// dispatches, and running it at 60 Hz is a per-frame dispatch STORM. The widget's own Tick
// repaints the cursor from viewCoordinate through updMainCursor, so keeping the field fresh
// IS the whole job here; the triangle and dish repaint is WriteDishCommitted, at commit rate.
// The name is the guard: WriteCursorOnly does cursor, only.
bool WriteCursorOnly(float viewX, float viewY) {
    void* w = Instance();
    if (!w || g_offViewCoordinate < 0) return false;
    auto* p = reinterpret_cast<uint8_t*>(w);
    std::memcpy(p + g_offViewCoordinate + 0, &viewX, sizeof(float));
    std::memcpy(p + g_offViewCoordinate + 4, &viewY, sizeof(float));
    return true;  // NO UFunction dispatch -- see the INVARIANT above.
}

// The COMMITTED-coords apply (the discrete triangulation locks). Writes Coordinate_0/1/2,
// selected and direction, then calls updCursorLocations -- repainting the committed triangle
// and rotating the physical pingDishes -- and updateCoordCoords for the az/alt text. It does
// NOT touch viewCoordinate: the live cursor belongs to the DeskCursorPose stream and
// WriteCursorOnly. This runs at COMMIT rate, change-gated, so the per-call dish setRot loop
// is affordable here in a way it is not per frame.
bool WriteDishCommitted(const DishAim& in) {
    void* w = Instance();
    if (!w || g_offCoordinate0 < 0 || g_offCoordinate1 < 0 ||
        g_offCoordinate2 < 0 || g_offSelected < 0)
        return false;
    if (!PresentationTarget()) return false;
    CachedObjRef identity;
    identity.Set(w);
    const uint32_t generation=world_identity::Generation();
    const auto current = [&] { return generation==world_identity::Generation() && identity.Get() == w && Instance() == w; };
    auto* p = reinterpret_cast<uint8_t*>(w);
    auto wr = [&](int32_t off, float x, float y) {
        std::memcpy(p + off + 0, &x, sizeof(float));
        std::memcpy(p + off + 4, &y, sizeof(float));
    };
    wr(g_offCoordinate0, in.c0X, in.c0Y);
    wr(g_offCoordinate1, in.c1X, in.c1Y);
    wr(g_offCoordinate2, in.c2X, in.c2Y);
    std::memcpy(p + g_offSelected, &in.selected, sizeof(int32_t));
    if (g_offDirection >= 0) *(p + g_offDirection) = in.direction ? 1 : 0;
    if (!g_updCursorLocationsFn) return false;
    ue_wrap::ParamFrame f(g_updCursorLocationsFn);
    if (!f.valid() || !ue_wrap::Call(w, f) || !current()) return false;
    // The desk's az/alt text repaint (updateCoordCoords) -- the desk owns its
    // verb table; declines silently on an unresolved desk, as pre-split.
    return ue_wrap::console_desk::CallUpdateCoordCoords() && current();
}

}  // namespace ue_wrap::coords_panel
