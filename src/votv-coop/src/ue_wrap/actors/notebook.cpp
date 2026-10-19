// ue_wrap/actors/notebook.cpp -- see ue_wrap/actors/notebook.h.

#include "ue_wrap/actors/notebook.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/ftext_utils.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"

#include <utility>

namespace ue_wrap::notebook {
namespace {

namespace R = ue_wrap::reflection;

// A class index answers a class that is not loaded in one lookup, so these tests are cheap enough for
// the paths that run them per record or per spawned prop.
bool IsInstanceOf(void* obj, const wchar_t* baseName) {
    if (!obj || !R::IsLive(obj)) return false;
    void* base = object_index::ClassByName(baseName);
    void* cls = R::ClassOf(obj);
    if (!base || !cls) return false;
    void* bases[] = {base};
    return R::IsDescendantOfAny(cls, bases, 1);
}

// A property's place in its class, resolved once: a lookup walks the class's property chain, and these
// run on a path a keystroke reaches. Keyed by the class and its listing id, so a class unloaded and
// loaded again is resolved again. Game thread.
struct Slot {
    void* cls = nullptr;
    uint64_t listing = 0;
    int32_t off = -1;
    uint8_t mask = 0;   // a bool property's bit
};

bool Resolved(const Slot& s, void* cls) {
    return s.off >= 0 && s.cls == cls && s.listing == object_index::ClassListingId(cls);
}

int32_t OffsetOf(Slot& s, void* cls, const wchar_t* name) {
    if (!cls) return -1;
    if (Resolved(s, cls)) return s.off;
    const int32_t off = R::FindPropertyOffset(cls, name);
    if (off >= 0) s = Slot{cls, object_index::ClassListingId(cls), off, 0};
    return off;
}

Slot g_text, g_clipboard, g_box, g_paperDraw, g_uneditable;

// The object a pointer field of `owner` names, null while it is null, dead or the field is unknown.
void* ReadObjectField(void* owner, Slot& slot, const wchar_t* field) {
    const int32_t off = OffsetOf(slot, R::ClassOf(owner), field);
    if (off < 0) return nullptr;
    void* obj = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(owner) + off);
    return obj && R::IsLive(obj) ? obj : nullptr;
}

}  // namespace

bool IsNotebook(void* actor) { return IsInstanceOf(actor, L"prop_notebook_C"); }
bool IsNotebookSheet(void* actor) { return IsInstanceOf(actor, L"prop_notebook_paper_C"); }
bool IsDrawablePaper(void* actor) { return IsInstanceOf(actor, L"prop_paper_drawable_C"); }
bool IsEditorWidget(void* widget) { return IsInstanceOf(widget, L"ui_notebook_C"); }
bool IsPaperDrawWidget(void* widget) { return IsInstanceOf(widget, L"ui_paperDraw_C"); }

void* NotebookOf(void* editorWidget) {
    if (!IsEditorWidget(editorWidget)) return nullptr;
    void* prop = ReadObjectField(editorWidget, g_clipboard, L"clipboard");
    return IsNotebook(prop) ? prop : nullptr;
}

void* PaperOf(void* drawWidget) {
    if (!IsPaperDrawWidget(drawWidget)) return nullptr;
    void* paper = ReadObjectField(drawWidget, g_paperDraw, L"paperDraw");
    return IsDrawablePaper(paper) ? paper : nullptr;
}

bool ReadPages(void* notebook, std::vector<std::wstring>& out) {
    if (!IsNotebook(notebook)) return false;
    const int32_t off = OffsetOf(g_text, R::ClassOf(notebook), L"text");
    if (off < 0) return false;
    out = field_io::ReadFStringArrayField(notebook, off);
    return true;
}

bool WritePages(void* notebook, const std::vector<std::wstring>& pages) {
    if (!IsNotebook(notebook)) return false;
    const int32_t off = OffsetOf(g_text, R::ClassOf(notebook), L"text");
    return off >= 0 && field_io::WriteFStringArrayField(notebook, off, pages);
}

bool SetEditorText(void* editorWidget, const std::wstring& text) {
    if (!IsEditorWidget(editorWidget)) return false;
    void* box = ReadObjectField(editorWidget, g_box, L"MLET_text");
    void* fn = box ? R::FindDispatchFunctionCached(R::ClassOf(box), L"SetText") : nullptr;
    if (!fn) return false;
    uint8_t ftext[ftext_utils::kFTextSize]{};
    if (!ftext_utils::MintFText(text.c_str(), ftext)) return false;
    ParamFrame frame(fn);
    return frame.valid() && frame.SetRaw(L"InText", ftext, sizeof(ftext)) && Call(box, frame);
}

bool IsReadOnly(void* editorWidget) {
    if (!IsEditorWidget(editorWidget)) return false;
    void* cls = R::ClassOf(editorWidget);
    if (!Resolved(g_uneditable, cls)) {
        int32_t off = -1;
        uint8_t mask = 0;
        if (!R::FindBoolProperty(cls, L"uneditable", off, mask)) return false;
        g_uneditable = Slot{cls, object_index::ClassListingId(cls), off, mask};
    }
    return (*(reinterpret_cast<const uint8_t*>(editorWidget) + g_uneditable.off) & g_uneditable.mask) != 0;
}

bool CommitKeystroke(void* editorWidget, const std::wstring& text) {
    if (!IsEditorWidget(editorWidget)) return false;
    void* fn = R::FindDispatchFunctionCached(
        R::ClassOf(editorWidget),
        L"BndEvt__MultiLineEditableText_86_K2Node_ComponentBoundEvent_5_OnMultiLineEditableTextCommittedEvent__"
        L"DelegateSignature");
    if (!fn) return false;
    uint8_t ftext[ftext_utils::kFTextSize]{};
    if (!ftext_utils::MintFText(text.c_str(), ftext)) return false;
    ParamFrame frame(fn);
    return frame.valid() && frame.SetRaw(L"Text", ftext, sizeof(ftext)) && frame.Set<uint8_t>(L"CommitMethod", 0) &&
           Call(editorWidget, frame);
}

bool ReloadEditor(void* editorWidget) {
    void* notebook = NotebookOf(editorWidget);
    if (!notebook) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(editorWidget), L"init");
    if (!fn) return false;
    ParamFrame frame(fn);
    return frame.valid() && frame.Set<void*>(L"clipboard", notebook) &&
           frame.Set<bool>(L"uneditable", IsReadOnly(editorWidget)) && Call(editorWidget, frame);
}

bool CallQuit(void* editorWidget, bool skipEdit) {
    if (!IsEditorWidget(editorWidget)) return false;
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(editorWidget), L"quit");
    if (!fn) return false;
    ParamFrame frame(fn);
    return frame.valid() && frame.Set<bool>(L"skipEdit", skipEdit) && Call(editorWidget, frame);
}

}  // namespace ue_wrap::notebook
