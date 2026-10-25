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

// The notebook's pages (`text`, a TArray of FString: no FText is read). False when the class or the field
// does not resolve.
bool ReadPages(void* notebook, std::vector<std::wstring>& out);
bool WritePages(void* notebook, const std::vector<std::wstring>& pages);

// The editor's own state: what it opened the notebook as (read-only or not), and a new text for its box.
bool SetEditorText(void* editorWidget, const std::wstring& text);
bool IsReadOnly(void* editorWidget);

// The widget's own committed-text handler: the bound event of MLET_text's OnTextCommitted, which the box
// raises when a typed page is committed (focus leaving the box, or the game's commit). It cuts the text to 32
// lines, writes it into the box and into the open page of the notebook's `text`, and runs the graph. A keystroke
// itself raises only OnTextChanged, whose handler scrolls. The handler's name carries cooked numbers; false
// when it does not resolve.
bool CommitKeystroke(void* editorWidget, const std::wstring& text);

// ui_notebook_C::init(clipboard, uneditable) again on the notebook the editor is open on: the widget's own
// page load. It reloads `notebookText` from the notebook's `text` and runs clampPages, which sets the box
// from the open page. For an editor whose notebook's `text` was written under it.
bool ReloadEditor(void* editorWidget);

// ui_notebook_C::quit(skipEdit), the editor's own close. Its body writes the box into the open page
// unless skipEdit, hides the widget and clears `clipboard`.
bool CallQuit(void* editorWidget, bool skipEdit);

}  // namespace ue_wrap::notebook
