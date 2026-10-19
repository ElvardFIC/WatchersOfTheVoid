#include "ue_wrap/devices/laptop_ui.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/store_catalog.h"
#include "ue_wrap/world/world_singleton.h"
#include <algorithm>
#include <cstring>

namespace ue_wrap::laptop_ui {
namespace {
namespace R = reflection;
namespace SC = store_catalog;
constexpr int kStride = 0x50; // 0.9.0n Fstruct_store array alignment (struct size 0x4D)
constexpr const wchar_t* kSwitchers[] = {L"switcher", L"switcher_media", L"switcher_mediaViewer"};
struct Layout {
    CachedObjRef cls, gmCls;
    int laptop = -1, cart = -1, slots = -1;
    std::array<int, 3> switchers{-1, -1, -1};
} g;
template<class T> T At(void* object, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
}
void* Ptr(void* object, int offset) {
    void* p = object && offset >= 0 ? At<void*>(object, offset) : nullptr;
    return p && R::IsLive(p) ? p : nullptr;
}
void* Widget() {
    void* gm = world_singleton::Gamemode();
    if (!gm) return nullptr;
    // Dispatch cache belongs to reflection; offsets are class-owned, not actor-owned.
    void* gmCls = R::ClassOf(gm);
    if (g.gmCls.Get() != gmCls || g.laptop < 0) {
        g.gmCls.Set(gmCls); g.laptop = R::FindPropertyOffset(gmCls, L"laptop");
    }
    void* w = Ptr(gm, g.laptop);
    if (!w || R::ClassNameOf(w) != L"ui_laptop_C") return nullptr;
    void* cls = R::ClassOf(w);
    if (g.cls.Get() != cls) {
        g.cls.Set(cls);
        g.cart = R::FindPropertyOffset(cls, L"cart");
        g.slots = R::FindPropertyOffset(cls, L"slots_cart");
        for (int i = 0; i < 3; ++i) g.switchers[i] = R::FindPropertyOffset(cls, kSwitchers[i]);
    }
    return g.cart >= 0 && g.slots >= 0 ? w : nullptr;
}
bool Cart(void* w, std::vector<std::wstring>& out, bool requireNames = true) {
    SC::Layout layout;
    if (!SC::ReadLayout(layout) || layout.name < 0 || layout.name + sizeof(R::FName) > kStride) return false;
    const int n = At<int32_t>(w, g.cart + 8), max = At<int32_t>(w, g.cart + 12);
    const auto* data = At<const uint8_t*>(w, g.cart);
    if (n < 0 || n > 50 || max < n || (n && !data)) return false;
    out.clear(); out.reserve(n);
    for (int i = 0; i < n; ++i) {
        R::FName name{}; std::memcpy(&name, data + i * kStride + layout.name, sizeof(name));
        auto row = R::ToString(name);
        if (requireNames && (row.empty() || row == L"None" || row.size() > 128)) return false;
        out.push_back(std::move(row));
    }
    return true;
}
bool SameWorld(uint32_t generation, const CachedObjRef& ref) {
    return generation == world_identity::Generation() && ref.Get();
}
}
bool Valid(const State& s) {
    if (s.tabs[0] < 0 || s.tabs[0] > 11 || s.tabs[1] < 0 || s.tabs[1] > 1 ||
        s.tabs[2] < 0 || s.tabs[2] > 1 || s.cart.size() > 50) return false;
    if (s.cart.empty()) return true;
    SC::Layout layout;
    if (!SC::Ready() || !SC::ReadLayout(layout) || layout.size < 0 || layout.size + 4 > kStride) return false;
    int64_t total = 0;
    for (const auto& name : s.cart) {
        if (name.empty() || name.size() > 128) return false;
        const auto* row = SC::Find(name);
        if (!row || !row->data) return false;
        int32_t size = 0; std::memcpy(&size, row->data + layout.size, sizeof(size));
        if (size < 0 || (total += size) > 50) return false;
    }
    return true;
}
bool Read(State& s) {
    void* w = Widget();
    if (!w) return false;
    const auto generation = world_identity::Generation();
    CachedObjRef ref; ref.Set(w);
    State result;
    if (!Cart(w, result.cart)) return false;
    for (int i = 0; i < 3; ++i) {
        void* sw = Ptr(w, g.switchers[i]);
        if (!sw) return false;
        ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(sw), L"GetActiveWidgetIndex"));
        if (!frame.valid() || !Call(sw, frame) || !SameWorld(generation, ref)) return false;
        result.tabs[i] = frame.Get<int32_t>(L"ReturnValue");
    }
    if (!Valid(result)) return false;
    s = std::move(result); return true;
}
bool Apply(const State& s) {
    if (!Valid(s)) return false;
    void* w = Widget();
    if (!w) return false;
    const auto generation = world_identity::Generation();
    CachedObjRef ref; ref.Set(w);
    std::vector<std::wstring> before;
    // An older refused-order restore could leave unnamed rows. They cannot be
    // published, but valid native slots can still be removed to recover the UI.
    if (!Cart(w, before, false)) return false;
    // Resolve every dependency before removing anything. Cart verbs dereference
    // these widgets, and genStoreCart also reads laptop.gamemode.saveSlot.orders.
    const auto cls = R::ClassOf(w);
    std::array<CachedObjRef, 3> switchers;
    std::vector<ParamFrame> tabFrames;
    for (int i = 0; i < 3; ++i) {
        void* sw = Ptr(w, g.switchers[i]);
        if (!sw) return false;
        switchers[i].Set(sw);
        ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(sw), L"SetActiveWidgetIndex"));
        if (!f.valid() || !f.Set<int32_t>(L"Index", s.tabs[i])) return false;
        tabFrames.push_back(std::move(f));
    }
    if (before != s.cart) {
        for (auto name : {L"vb_cart", L"txt_price", L"txt_cartSize"})
            if (!Ptr(w, R::FindPropertyOffset(cls, name))) return false;
        void* actor = Ptr(w, R::FindPropertyOffset(cls, L"laptop"));
        void* gm = actor ? Ptr(actor, R::FindPropertyOffset(R::ClassOf(actor), L"gamemode")) : nullptr;
        if (!gm || !Ptr(gm, R::FindPropertyOffset(R::ClassOf(gm), L"saveSlot"))) return false;
        const int slots = At<int32_t>(w, g.slots + 8);
        void* data = At<void*>(w, g.slots);
        if (slots != int(before.size()) || (slots && !data)) return false;
        for (int i = 0; i < slots; ++i) if (!Ptr(data, i * int(sizeof(void*)))) return false;
        void* add = R::FindDispatchFunctionCached(cls, L"addStoreCart");
        void* remove = R::FindDispatchFunctionCached(cls, L"removeStoreCart");
        ParamFrame rebuild(R::FindDispatchFunctionCached(cls, L"genStoreCart"));
        SC::Layout layout;
        const int paramSize = add ? R::FindParamSize(add, L"struct_store") : -1;
        if (!remove || !rebuild.valid() || paramSize <= 0 || paramSize > kStride ||
            !SC::ReadLayout(layout) || layout.name < 0 || layout.name + sizeof(R::FName) > paramSize) return false;
        size_t prefix = 0;
        while (prefix < before.size() && prefix < s.cart.size() && before[prefix] == s.cart[prefix]) ++prefix;
        std::vector<ParamFrame> removes, adds;
        for (int i = int(before.size()) - 1; i >= int(prefix); --i) {
            ParamFrame f(remove);
            if (!f.valid() || !f.Set<int32_t>(L"index", i)) return false;
            removes.push_back(std::move(f));
        }
        for (size_t i = prefix; i < s.cart.size(); ++i) {
            const auto* row = SC::Find(s.cart[i]);
            ParamFrame f(add);
            if (!row || !f.valid() || !f.SetRaw(L"struct_store", row->data, paramSize)) return false;
            // The table's name field is None. Native generateStore stamps the
            // key; a mirror must do the same, or the next purchase loses identity.
            std::memcpy(static_cast<uint8_t*>(f.data()) + f.ParamOffset(L"struct_store") + layout.name,
                &row->key, sizeof(row->key));
            adds.push_back(std::move(f));
        }
        for (auto& f : removes) if (!SameWorld(generation, ref) || !Call(w, f)) return false;
        for (auto& f : adds) if (!SameWorld(generation, ref) || !Call(w, f)) return false;
        if (!SameWorld(generation, ref) || !Call(w, rebuild) || !SameWorld(generation, ref)) return false;
    }
    // Calling switcherSwitched would createRC/leaveRC and possess a camera on
    // every spectator. UWidgetSwitcher alone changes the displayed page.
    for (int i = 0; i < 3; ++i) {
        if (!SameWorld(generation, ref) || !switchers[i].Get() || !Call(switchers[i].Get(), tabFrames[i])) return false;
    }
    State actual;
    return SameWorld(generation, ref) && Read(actual) && actual == s;
}
}
