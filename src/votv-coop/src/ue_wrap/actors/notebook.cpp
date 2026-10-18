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

// The object a pointer field of `owner` names, null while it is null, dead or the field is unknown.
void* ReadObjectField(void* owner, const wchar_t* field) {
    const int32_t off = R::FindPropertyOffset(R::ClassOf(owner), field);
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
    void* prop = ReadObjectField(editorWidget, L"clipboard");
    return IsNotebook(prop) ? prop : nullptr;
}

void* PaperOf(void* drawWidget) {
    if (!IsPaperDrawWidget(drawWidget)) return nullptr;
    void* paper = ReadObjectField(drawWidget, L"paperDraw");
    return IsDrawablePaper(paper) ? paper : nullptr;
}

bool ReadPages(void* notebook, std::vector<std::wstring>& out) {
    if (!IsNotebook(notebook)) return false;
    const int32_t off = R::FindPropertyOffset(R::ClassOf(notebook), L"text");
    if (off < 0) return false;
    out = field_io::ReadFStringArrayField(notebook, off);
    return true;
}

bool WritePages(void* notebook, const std::vector<std::wstring>& pages) {
    if (!IsNotebook(notebook)) return false;
    const int32_t off = R::FindPropertyOffset(R::ClassOf(notebook), L"text");
    return off >= 0 && field_io::WriteFStringArrayField(notebook, off, pages);
}

bool ReadPage(void* notebook, int32_t& page) {
    if (!IsNotebook(notebook)) return false;
    const int32_t off = R::FindPropertyOffset(R::ClassOf(notebook), L"page");
    if (off < 0) return false;
    page = *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(notebook) + off);
    return true;
}

bool ReadEditorText(void* editorWidget, std::wstring& out) {
    if (!IsEditorWidget(editorWidget)) return false;
    void* box = ReadObjectField(editorWidget, L"MLET_text");
    void* fn = box ? R::FindDispatchFunctionCached(R::ClassOf(box), L"GetText") : nullptr;
    if (!fn) return false;
    ParamFrame frame(fn);
    if (!frame.valid() || !Call(box, frame)) return false;
    uint8_t ftext[ftext_utils::kFTextSize]{};
    if (!frame.GetRaw(L"ReturnValue", ftext, sizeof(ftext))) return false;
    out = ftext_utils::FTextToString(ftext);
    return true;
}

bool IsReadOnly(void* editorWidget) {
    if (!IsEditorWidget(editorWidget)) return false;
    int32_t off = -1;
    uint8_t mask = 0;
    if (!R::FindBoolProperty(R::ClassOf(editorWidget), L"uneditable", off, mask)) return false;
    return (*(reinterpret_cast<const uint8_t*>(editorWidget) + off) & mask) != 0;
}

bool CommitEditorPage(void* editorWidget, std::vector<std::wstring>& pagesOut) {
    void* notebook = NotebookOf(editorWidget);
    if (!notebook || IsReadOnly(editorWidget)) return false;
    std::wstring boxText;
    std::vector<std::wstring> pages;
    int32_t page = -1;
    if (!ReadEditorText(editorWidget, boxText) || !ReadPages(notebook, pages) || !ReadPage(notebook, page))
        return false;
    if (page < 0 || static_cast<size_t>(page) >= pages.size()) return false;
    if (pages[static_cast<size_t>(page)] != boxText) {
        pages[static_cast<size_t>(page)] = boxText;
        if (!WritePages(notebook, pages)) return false;
    }
    pagesOut = std::move(pages);
    return true;
}

}  // namespace ue_wrap::notebook
