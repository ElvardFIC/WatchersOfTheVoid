// ue_wrap/actors/notebook.h -- the notebook props and the editor widgets that write them.
//
// prop_notebook_C keeps its pages in `text` (TArray<FString>) and the open page in `page`; the
// torn sheet prop_notebook_paper_C is its child. The editor is ui_notebook_C, one widget per
// process: its `clipboard` names the notebook it edits and its MLET_text holds the open page, which
// reaches `text` only when the page is committed or the editor quits. prop_paper_drawable_C is
// drawn on through ui_paperDraw_C, whose `paperDraw` names the paper.
//
// Engine-wrapper layer (principle 7): class tests, field reads and writes, the widget's own
// calls. No network and no coop state. Game thread.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ue_wrap::notebook {

// prop_notebook_C or below, the torn sheet included.
bool IsNotebook(void* actor);
// prop_notebook_paper_C or below: a page torn out of a notebook.
bool IsNotebookSheet(void* actor);
// prop_paper_drawable_C or below.
bool IsDrawablePaper(void* actor);

// ui_notebook_C / ui_paperDraw_C, the editor widgets.
bool IsEditorWidget(void* widget);
bool IsPaperDrawWidget(void* widget);

// The live notebook an editor widget is open on, else null (a closed editor has none: quit clears it).
void* NotebookOf(void* editorWidget);
// The live drawable paper a draw widget is open on, else null.
void* PaperOf(void* drawWidget);

// The notebook's pages and its open page index. False when the class or the field does not resolve.
bool ReadPages(void* notebook, std::vector<std::wstring>& out);
bool WritePages(void* notebook, const std::vector<std::wstring>& pages);
bool ReadPage(void* notebook, int32_t& page);

// The editor's own state: the text in its box, and whether it opened the notebook read-only.
bool ReadEditorText(void* editorWidget, std::wstring& out);
bool SetEditorText(void* editorWidget, const std::wstring& text);
bool IsReadOnly(void* editorWidget);

// What the widget's own commit does: the box's text becomes the open page of the notebook's `text`.
// `pagesOut` receives the pages as they stand afterwards. False for a read-only editor, an open page
// outside `text`, or a field that does not resolve (`pagesOut` is then untouched).
bool CommitEditorPage(void* editorWidget, std::vector<std::wstring>& pagesOut);

// The widget's own committed-text handler: the bound event of MLET_text's OnTextCommitted, which the box
// raises when a typed page is committed. It cuts the text to 32 lines, writes it into the box and into the
// open page of the notebook's `text`, and runs the graph, so the graph's watchers see it. A keystroke itself
// raises only OnTextChanged, whose handler scrolls. The handler's name carries cooked numbers; false when it
// does not resolve.
bool CommitKeystroke(void* editorWidget, const std::wstring& text);

// ui_notebook_C::init(clipboard, uneditable) again on the notebook the editor is open on: the widget's own
// page load. It reloads `notebookText` from the notebook's `text` and runs clampPages, which sets the box
// from the open page. For an editor whose notebook's `text` was written under it.
bool ReloadEditor(void* editorWidget);

// ui_notebook_C::quit(skipEdit), the editor's own close. Its body writes the box into the open page
// unless skipEdit, hides the widget and clears `clipboard`.
bool CallQuit(void* editorWidget, bool skipEdit);

}  // namespace ue_wrap::notebook
