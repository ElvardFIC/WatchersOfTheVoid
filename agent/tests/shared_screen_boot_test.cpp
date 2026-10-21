#include "../../src/votv-coop/src/ue_wrap/desk/shared_screen.cpp"
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
namespace test {
enum Kind { Panel, User, Switcher, Text, Scroll };
struct Obj {
    alignas(8) uint8_t native[0x200]{};
    Kind kind = Panel;
    bool live = true;
    void* laptop = nullptr;
    void* rootswitch = nullptr;
    uint8_t visibility = 0;
    int index = 0;
    float scroll = 0;
    std::wstring text;
    std::vector<void*> children;
} gm, widget, tree, root, boot, desktop, app, page, text, scroll;
int rootWrites = 0;
std::wstring minted;
void Init() {
    gm.laptop = &widget; widget.kind = User; widget.rootswitch = &root;
    void* p = &tree; std::memcpy(widget.native + ue_wrap::profile::off::UUserWidget_WidgetTree, &p, sizeof(p));
    p = &root; std::memcpy(tree.native + ue_wrap::profile::off::UWidgetTree_RootWidget, &p, sizeof(p));
    root.kind = app.kind = Switcher; root.children = {&boot, &desktop};
    desktop.children = {&app}; app.children = {&page}; page.children = {&text, &scroll};
    text.kind = Text; scroll.kind = Scroll;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
void* ClassOf(void* p) { return p; }
void* SuperStructOf(void*) { return nullptr; }
const FName& NameOf(void* p) {
    static FName names[] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}};
    return names[static_cast<test::Obj*>(p)->kind];
}
bool NameEquals(const FName& n, const wchar_t* want) {
    const wchar_t* names[] = {L"PanelWidget", L"UserWidget", L"WidgetSwitcher", L"TextBlock", L"ScrollBox"};
    return std::wstring(want) == names[n.ComparisonIndex];
}
int32_t FindPropertyOffset(void*, const wchar_t* name) {
    if (std::wstring(name) == L"laptop") return offsetof(test::Obj, laptop);
    if (std::wstring(name) == L"rootswitch") return offsetof(test::Obj, rootswitch);
    if (std::wstring(name) == L"Text") return offsetof(test::Obj, text);
    return -1;
}
void* FindDispatchFunctionCached(void*, const wchar_t* name) { return const_cast<wchar_t*>(name); }
}
namespace ue_wrap::world_singleton { void* Gamemode() { return &test::gm; } }
namespace ue_wrap::ftext_utils {
std::wstring FTextToString(const void* p) { return *static_cast<const std::wstring*>(p); }
bool MintFText(const wchar_t* s, void*) { test::minted = s; return true; }
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f) : fn_(f), buf_(128) {}
bool ParamFrame::SetRaw(const wchar_t*, const void* data, int32_t size) { std::memcpy(buf_.data(), data, size); return valid(); }
bool ParamFrame::GetRaw(const wchar_t*, void* out, int32_t size) const { std::memcpy(out, buf_.data(), size); return true; }
bool Call(void* object, ParamFrame& f) {
    auto& obj = *static_cast<test::Obj*>(object);
    const std::wstring name(static_cast<wchar_t*>(f.function()));
    auto result = [&](auto value) { std::memcpy(f.data(), &value, sizeof(value)); return true; };
    if (name == L"GetVisibility") return result(obj.visibility);
    if (name == L"GetChildrenCount") return result(int32_t(obj.children.size()));
    if (name == L"GetActiveWidgetIndex") return result(obj.index);
    if (name == L"GetScrollOffset") return result(obj.scroll);
    if (name == L"GetChildAt") {
        int index = 0; std::memcpy(&index, f.data(), sizeof(index));
        return result(index >= 0 && index < int(obj.children.size()) ? obj.children[index] : nullptr);
    }
    if (object == &test::root || object == &test::boot || object == &test::widget) ++test::rootWrites;
    if (name == L"SetActiveWidgetIndex") std::memcpy(&obj.index, f.data(), sizeof(obj.index));
    else if (name == L"SetScrollOffset") std::memcpy(&obj.scroll, f.data(), sizeof(obj.scroll));
    else if (name == L"SetVisibility") std::memcpy(&obj.visibility, f.data(), sizeof(obj.visibility));
    else if (name == L"SetText") obj.text = test::minted;
    else CHECK(false);
    return true;
}
}
int main() {
    using namespace test; namespace UI = ue_wrap::shared_screen; Init();
    // The same wrapper paints host and client observers. Capture while the
    // native rootswitch shows boot, then finish native startup before repaint.
    for (int peer = 0; peer < 2; ++peer) {
        root.index = 0; text.text = L"mail"; scroll.scroll = 42; rootWrites = 0;
        std::vector<UI::Row> rows; CHECK(UI::Read(rows));
        root.index = 1; text.text = L"changed"; scroll.scroll = 0;
        const bool applied = UI::Apply(rows);
        CHECK(root.index == 1 && rootWrites == 0); CHECK(applied);
        CHECK(text.text == L"mail" && scroll.scroll == 42);
        // Old observer snapshots cannot expose a desktop over a later boot or
        // power cycle either; native power owns the entire outer switcher.
        root.index = 0; CHECK(UI::Apply(rows)); CHECK(root.index == 0 && rootWrites == 0);
        root.index = 1; CHECK(UI::Apply(rows)); CHECK(root.index == 1 && rootWrites == 0);
        app.index = -1; CHECK(!UI::Apply(rows)); // the selected page appears after the tab write
        CHECK(UI::Apply(rows)); CHECK(app.index == 0 && root.index == 1 && rootWrites == 0);
    }
    std::vector<UI::Row> rows;
    root.children.pop_back(); CHECK(!UI::Read(rows)); CHECK(!UI::Apply({}));
    root.children.push_back(&desktop); CHECK(UI::Read(rows));
    widget.rootswitch = nullptr; CHECK(!UI::Read(rows)); CHECK(!UI::Apply({}));
    puts("PASS shared screen preserves native boot/power on both observers while mirroring desktop text/scroll");
}
