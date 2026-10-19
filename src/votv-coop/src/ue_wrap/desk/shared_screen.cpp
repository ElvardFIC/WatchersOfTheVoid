#include "ue_wrap/desk/shared_screen.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/ftext_utils.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/world/world_singleton.h"
#include <cmath>
#include <cstring>
#include <map>

namespace ue_wrap::shared_screen {
namespace {
namespace R = reflection;
void* Field(void* object, const wchar_t* name) {
    if (!object || !R::IsLive(object)) return nullptr;
    const int off = R::FindPropertyOffset(R::ClassOf(object), name);
    if (off < 0) return nullptr;
    void* value = nullptr;
    std::memcpy(&value, static_cast<uint8_t*>(object) + off, sizeof(value));
    return value && R::IsLive(value) ? value : nullptr;
}
void* Fn(void* object, const wchar_t* name) {
    return R::FindDispatchFunctionCached(R::ClassOf(object), name);
}
template<class T> bool Get(void* object, const wchar_t* name, T& value) {
    ParamFrame frame(Fn(object, name));
    return frame.valid() && Call(object, frame) && frame.GetRaw(L"ReturnValue", &value, sizeof(value));
}
template<class T> bool Set(void* object, const wchar_t* name, const wchar_t* param, T value) {
    ParamFrame frame(Fn(object, name));
    return frame.valid() && frame.Set<T>(param, value) && Call(object, frame);
}
bool Is(void* object, const wchar_t* name) {
    void* cls = R::ClassOf(object);
    while (cls) {
        if (R::NameEquals(R::NameOf(cls), name)) return true;
        cls = R::SuperStructOf(cls);
    }
    return false;
}
void Walk(void* widget, const std::string& path, int depth, std::map<std::string, void*>& out) {
    if (!widget || !R::IsLive(widget) || depth > 20 || out.size() >= 512) return;
    out.emplace(path, widget);
    uint8_t visibility = 0;
    if (Get(widget, L"GetVisibility", visibility) && (visibility == 1 || visibility == 2)) return;
    if (Is(widget, L"UserWidget")) {
        void* tree = nullptr;
        std::memcpy(&tree, static_cast<uint8_t*>(widget) + profile::off::UUserWidget_WidgetTree, sizeof(tree));
        if (tree && R::IsLive(tree)) {
            void* root = nullptr;
            std::memcpy(&root, static_cast<uint8_t*>(tree) + profile::off::UWidgetTree_RootWidget, sizeof(root));
            Walk(root, path + "/r", depth + 1, out);
        }
    }
    int32_t count = 0;
    if (!Get(widget, L"GetChildrenCount", count) || count < 0 || count > 512) return;
    int32_t active = -1;
    const bool switcher = Is(widget, L"WidgetSwitcher") && Get(widget, L"GetActiveWidgetIndex", active);
    for (int32_t i = 0; i < count; ++i) {
        if (switcher && i != active) continue;
        ParamFrame frame(Fn(widget, L"GetChildAt"));
        void* child = nullptr;
        if (frame.Set<int32_t>(L"Index", i) && Call(widget, frame) &&
            frame.GetRaw(L"ReturnValue", &child, sizeof(child)))
            Walk(child, path + "/" + std::to_string(i), depth + 1, out);
    }
}
std::map<std::string, void*> Widgets() {
    std::map<std::string, void*> widgets;
    Walk(Field(world_singleton::Gamemode(), L"laptop"), "pc", 0, widgets);
    return widgets;
}
bool ReadRow(void* widget, Row& row) {
    if (!Get(widget, L"GetVisibility", row.visibility)) return false;
    if (Is(widget, L"WidgetSwitcher") && Get(widget, L"GetActiveWidgetIndex", row.tab)) row.flags |= 1;
    if (Is(widget, L"ScrollBox") && Get(widget, L"GetScrollOffset", row.scroll)) row.flags |= 2;
    // Only presentation text is mirrored. Input handlers and their gameplay
    // effects stay on the operator; observers never receive button presses.
    if (Is(widget, L"TextBlock")) {
        const int off = R::FindPropertyOffset(R::ClassOf(widget), L"Text");
        if (off >= 0) {
            row.text = ftext_utils::FTextToString(static_cast<uint8_t*>(widget) + off);
            if (row.text.size() <= 4096) row.flags |= 4;
            else row.text.clear();
        }
    }
    return row.visibility <= 4 && (!(row.flags & 2) || std::isfinite(row.scroll));
}
}
bool Read(std::vector<Row>& rows) {
    rows.clear();
    for (const auto& [path, widget] : Widgets()) {
        Row row; row.path = path;
        if (!ReadRow(widget, row)) return false;
        rows.push_back(std::move(row));
    }
    return !rows.empty();
}
bool Apply(const std::vector<Row>& rows) {
    auto widgets = Widgets();
    if (widgets.empty()) return false;
    bool ok = true;
    for (const Row& row : rows) {
        const auto it = widgets.find(row.path);
        if (it == widgets.end()) { ok = false; continue; }
        void* widget = it->second;
        Row current;
        if (!ReadRow(widget, current)) { ok = false; continue; }
        if (row.visibility != current.visibility)
            ok = Set(widget, L"SetVisibility", L"InVisibility", row.visibility) && ok;
        if ((row.flags & current.flags & 1) && row.tab != current.tab) {
            int32_t count = 0;
            if (Get(widget, L"GetChildrenCount", count) && row.tab >= 0 && row.tab < count)
                ok = Set(widget, L"SetActiveWidgetIndex", L"Index", row.tab) && ok;
            else ok = false;
        }
        if ((row.flags & current.flags & 2) && std::fabs(row.scroll - current.scroll) > 0.5f)
            ok = Set(widget, L"SetScrollOffset", L"NewScrollOffset", row.scroll) && ok;
        if ((row.flags & current.flags & 4) && row.text != current.text) {
            uint8_t text[ftext_utils::kFTextSize]{};
            ParamFrame frame(Fn(widget, L"SetText"));
            ok = (ftext_utils::MintFText(row.text.c_str(), text) &&
                frame.SetRaw(L"InText", text, sizeof(text)) && Call(widget, frame)) && ok;
        }
    }
    return ok;
}
}
