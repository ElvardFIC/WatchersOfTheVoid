// ue_wrap/desk/sat_console.cpp -- see ue_wrap/desk/sat_console.h.

#include "ue_wrap/desk/sat_console.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/ftext_utils.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/desk/console_desk.h"  // Instance, AtlasWidget

#include <chrono>
#include <array>
#include <algorithm>

namespace ue_wrap::sat_console {
namespace {

namespace R = reflection;
using field_io::FStringView;
using field_io::TArrayView;
using field_io::WriteFStringField;

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Warn once after several incomplete passes, but keep retrying delayed metadata.
constexpr int kMaxPostClassAttempts = 5;

// The terminal class, found again in a world that loaded it anew; its Blueprint functions are asked
// through the memoised dispatch lookup at each call for the same reason. The offsets are the class
// layout's and outlive a reload.
CachedObjRef g_termCls;
int32_t g_offProcessing = -1;
uint8_t g_maskProcessing = 0;
int32_t g_offPanel = -1, g_offUsed = -1, g_offActiveDish = -1;
int32_t g_offName = -1, g_offCommand = -1, g_offTextBox = -1, g_offLog = -1;
int32_t g_offEnterC = -1;  // enterCommand's c
int32_t g_offWriteB = -1, g_offWriteType = -1;  // writeToLog's B and bracketsType

int32_t g_offAtlasTerm = -1;  // ui_consolesAtlas_C.umg_console
int32_t g_offDeskPanels = -1;  // analogDScreenTest_C.consoles

void*   g_fnSetText = nullptr;  // EditableTextBox.SetText
void*   g_wblCdo = nullptr;
void*   g_fnCreate = nullptr;  // WidgetBlueprintLibrary.Create

bool     g_resolved = false;
bool     g_resolving = false;
int      g_attempts = 0;
uint64_t g_nextTryMs = 0;
std::array<CachedObjRef,11> g_layoutRefs;
uint64_t g_layoutRevision=0;
bool BindingsCurrent(){
    const wchar_t* names[]{kTerminalClass,L"ui_consolesAtlas_C",L"analogDScreenTest_C",L"EditableTextBox"};
    for(size_t i=0;i<4;++i)if(R::FindClass(names[i])!=g_layoutRefs[i].Raw())return false;
    if(R::FindFunction(g_layoutRefs[0].Raw(),kEnterCommand)!=g_layoutRefs[4].Raw() ||
       R::FindFunction(g_layoutRefs[0].Raw(),kWriteToLog)!=g_layoutRefs[5].Raw() ||
       R::FindFunction(g_layoutRefs[0].Raw(),kInit)!=g_layoutRefs[6].Raw())return false;
    return R::FindFunction(g_layoutRefs[3].Raw(),L"SetText")==g_layoutRefs[7].Raw() &&
        R::FindClassDefaultObject(L"WidgetBlueprintLibrary")==g_layoutRefs[8].Raw() &&
        R::ClassOf(g_layoutRefs[8].Get())==g_layoutRefs[9].Raw() &&
        R::FindFunction(g_layoutRefs[9].Raw(),L"Create")==g_layoutRefs[10].Raw();
}

bool LayoutCurrent() {
    if(!g_resolved || !std::all_of(g_layoutRefs.begin(),g_layoutRefs.end(),[](const CachedObjRef& ref){return ref.Get()!=nullptr;}))return false;
    if(!BindingsCurrent())return false;
    return g_resolved && std::all_of(g_layoutRefs.begin(),g_layoutRefs.end(),
        [](const CachedObjRef& ref){return ref.Get()!=nullptr;}) && BindingsCurrent();
}

void ResetLayout() {
    ++g_layoutRevision;
    g_resolved=false;g_termCls.Reset();g_layoutRefs={};
    g_offProcessing=g_offPanel=g_offUsed=g_offActiveDish=-1;g_maskProcessing=0;
    g_offName=g_offCommand=g_offTextBox=g_offLog=-1;
    g_offEnterC=g_offWriteB=g_offWriteType=g_offAtlasTerm=g_offDeskPanels=-1;
    g_fnSetText=g_wblCdo=g_fnCreate=nullptr;
}

bool ReadString(const void* base,int32_t offset,std::wstring& out,size_t limit) {
    if(!base || offset<0)return false;
    const auto view=*reinterpret_cast<const FStringView*>(static_cast<const uint8_t*>(base)+offset);
    if(view.num<0 || view.max<view.num || size_t(view.num)>limit+1)return false;
    if(!view.num){out.clear();return true;}
    if(!view.data || view.data[view.num-1]!=L'\0')return false;
    out.assign(view.data,size_t(view.num-1));return true;
}

bool Complete() {
    return g_offProcessing >= 0 && g_maskProcessing && g_offPanel >= 0 && g_offUsed >= 0 &&
           g_offActiveDish >= 0 && g_offName >= 0 && g_offCommand >= 0 && g_offTextBox >= 0 && g_offLog >= 0 &&
           g_offEnterC >= 0 && g_offWriteB >= 0 &&
           g_offWriteType >= 0 && g_offAtlasTerm >= 0 && g_offDeskPanels >= 0 && g_fnSetText && g_wblCdo &&
           g_fnCreate;
}

template <class T>
T* At(void* base, int32_t off) {
    return reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off);
}

void* ReadObject(void* base, int32_t off) {
    if (!base || off < 0) return nullptr;
    void* o = *At<void*>(base, off);
    return (o && R::IsLive(o)) ? o : nullptr;
}

bool WriteObject(void* base, int32_t off, void* value) {
    if (!base || off < 0) return false;
    *At<void*>(base, off) = value;
    return true;
}

void* TerminalClass() {
    void* cls = g_termCls.Get();
    if (!cls) {
        cls = R::FindClass(kTerminalClass);
        g_termCls.Set(cls);
    }
    return cls;
}

bool IsTerminal(void* obj) {
    return obj && R::IsLive(obj) && R::ClassOf(obj) == TerminalClass();
}
struct Lease {
    CachedObjRef object,cls;uint32_t world=world_identity::Generation();uint64_t layout=g_layoutRevision;
    explicit Lease(void* value){object.Set(value);cls.Set(object.Get()?R::ClassOf(object.Get()):nullptr);}
    bool Current()const{return object.Get() && cls.Get() && R::ClassOf(object.Get())==cls.Get() &&
        world==world_identity::Generation() && LayoutCurrent() && layout==g_layoutRevision;}
};
bool ParamBounds(size_t dependency,int32_t offset,size_t bytes){
    if(!LayoutCurrent() || offset<0)return false;
    const auto function=g_layoutRefs[dependency];const auto revision=g_layoutRevision;
    const auto size=R::FunctionFrameSize(function.Get());
    return size>=0 && offset<=size && bytes<=size_t(size-offset) && function.Get() && LayoutCurrent() && revision==g_layoutRevision;
}

void* TerminalFn(void* term, const wchar_t* name) { return R::FindDispatchFunctionCached(R::ClassOf(term), name); }

}  // namespace

bool EnsureResolved() {
    if (LayoutCurrent()) return true;
    if(g_resolving)return false;
    if(g_resolved){ResetLayout();g_nextTryMs=0;}
    const uint64_t now = NowMs();
    if (now < g_nextTryMs) return false;
    g_nextTryMs = now + 1000;
    g_resolving=true;
    struct Resolving {~Resolving(){g_resolving=false;}} resolving;
    ResetLayout();

    void* term = R::FindClass(kTerminalClass);
    void* atlas = R::FindClass(L"ui_consolesAtlas_C");
    void* desk = R::FindClass(L"analogDScreenTest_C");
    void* box = R::FindClass(L"EditableTextBox");
    if (!term || !atlas || !desk || !box) return false;  // the world is still loading
    const std::array<void*,4> classes{term,atlas,desk,box};
    for(size_t i=0;i<classes.size();++i)g_layoutRefs[i].Set(classes[i]);

    g_termCls.Set(term);
    R::FindBoolProperty(term, L"processing", g_offProcessing, g_maskProcessing);
    g_offPanel      = R::FindPropertyOffset(term, L"panel");
    g_offUsed       = R::FindPropertyOffset(term, L"used");
    g_offActiveDish = R::FindPropertyOffset(term, L"activeDish");
    g_offName       = R::FindPropertyOffset(term, L"name");
    g_offCommand    = R::FindPropertyOffset(term, L"command");
    g_offTextBox    = R::FindPropertyOffset(term, L"EditableTextBox");
    g_offLog        = R::FindPropertyOffset(term, L"consoleLine");
    void* fnEnter = R::FindFunction(term, kEnterCommand);
    void* fnWrite = R::FindFunction(term, kWriteToLog);
    void* fnInit  = R::FindFunction(term, kInit);
    g_layoutRefs[4].Set(fnEnter);g_layoutRefs[5].Set(fnWrite);g_layoutRefs[6].Set(fnInit);
    if (fnEnter) g_offEnterC = R::FindParamOffset(fnEnter, L"c");
    if (fnWrite) {
        g_offWriteB    = R::FindParamOffset(fnWrite, L"B");
        g_offWriteType = R::FindParamOffset(fnWrite, L"bracketsType");
    }
    const bool haveInit = fnInit && R::FindParamOffset(fnInit, L"name") >= 0 && R::FindParamOffset(fnInit, L"dish") >= 0;
    g_offAtlasTerm  = R::FindPropertyOffset(atlas, L"umg_console");
    g_offDeskPanels = R::FindPropertyOffset(desk, L"consoles");
    g_fnSetText = R::FindFunction(box, L"SetText");
    g_wblCdo = R::FindClassDefaultObject(L"WidgetBlueprintLibrary");
    if (g_wblCdo) g_fnCreate = R::FindFunction(R::ClassOf(g_wblCdo), L"Create");
    g_layoutRefs[7].Set(g_fnSetText);g_layoutRefs[8].Set(g_wblCdo);
    g_layoutRefs[9].Set(g_wblCdo?R::ClassOf(g_wblCdo):nullptr);g_layoutRefs[10].Set(g_fnCreate);

    // No offset fallbacks: a member this wrapper cannot find means the class is not the one it was
    // written against, and a guessed offset would write into whatever lives there now.
    if (!Complete() || !haveInit || !std::all_of(g_layoutRefs.begin(),g_layoutRefs.end(),[](const CachedObjRef& ref){return ref.Get()!=nullptr;})) {
        if (++g_attempts == kMaxPostClassAttempts) {
            UE_LOGW("sat_console: resolution incomplete after %d passes (processing=%d panel=%d used=%d "
                    "activeDish=%d name=%d command=%d textBox=%d init=%d c=%d B=%d "
                    "type=%d atlasTerm=%d panels=%d setText=%p create=%p) -- will retry",
                    g_attempts, g_offProcessing, g_offPanel, g_offUsed, g_offActiveDish,
                    g_offName, g_offCommand, g_offTextBox, haveInit ? 1 : 0, g_offEnterC, g_offWriteB,
                    g_offWriteType, g_offAtlasTerm, g_offDeskPanels, g_fnSetText, g_fnCreate);
        }
        return false;
    }
    g_resolved = true;
    g_attempts=0;
    UE_LOGI("sat_console: resolved (processing=0x%X/%02X command=0x%X c=0x%X B=0x%X)", g_offProcessing,
            g_maskProcessing, g_offCommand, g_offEnterC, g_offWriteB);
    return true;
}

void* LocalTerminal() {
    if (!EnsureResolved()) return nullptr;
    void* atlas = console_desk::AtlasWidget();
    Lease owner(atlas);const auto offset=g_offAtlasTerm;if(!owner.Current() || owner.cls.Get()!=g_layoutRefs[1].Get())return nullptr;
    void* term = ReadObject(atlas, g_offAtlasTerm);
    return owner.Current() && offset==g_offAtlasTerm && console_desk::AtlasWidget()==atlas && IsTerminal(term) ? term : nullptr;
}

void* CreateTerminal(void* worldContext) {
    if (!worldContext || !EnsureResolved()) return nullptr;
    Lease context(worldContext);const auto factory=g_layoutRefs[10],library=g_layoutRefs[8],terminalClass=g_termCls;
    const auto valid=[&]{return context.Current() && factory.Is(g_fnCreate) && library.Is(g_wblCdo) && terminalClass.Is(g_termCls.Get());};
    if(!valid())return nullptr;
    ParamFrame f(g_fnCreate);
    void* cls = TerminalClass();
    if (!cls || !f.valid() || !f.Set(L"WorldContextObject", worldContext) || !f.Set(L"WidgetType", cls))
        return nullptr;
    void* noPlayer = nullptr;  // the game instance owns it: no world object is its outer
    if (!f.Set(L"OwningPlayer", noPlayer) || !valid() || !Call(g_wblCdo, f)) return nullptr;
    void* made=nullptr;if(!f.GetRaw(L"ReturnValue",&made,sizeof(made)))return nullptr;
    if(!valid()){
        if(made && R::IsLive(made) && terminalClass.Get() && R::ClassOf(made)==terminalClass.Get())R::MarkPendingKill(made);
        return nullptr;
    }
    return IsTerminal(made) ? made : nullptr;
}

void DiscardTerminal(void* term) {
    if (IsTerminal(term)) R::MarkPendingKill(term);
}

bool ReadEnterCommandLine(const uint8_t* locals, std::wstring& out) {
    if (!locals || !ParamBounds(4,g_offEnterC,sizeof(FStringView))) return false;
    return ReadString(locals,g_offEnterC,out,65536);
}

bool ReadWriteToLogArgs(const uint8_t* locals, std::wstring& text, uint8_t& bracketsType) {
    if (!locals || !ParamBounds(5,g_offWriteB,sizeof(FStringView)) || !ParamBounds(5,g_offWriteType,1))return false;
    std::wstring value;if(!ReadString(locals,g_offWriteB,value,8192))return false;
    const auto type=locals[g_offWriteType];if(type>3)return false;
    text=std::move(value);bracketsType=type;
    return true;
}

bool ReadProcessing(void* term, bool& out) {
    if (!LayoutCurrent() || !IsTerminal(term)) return false;
    out = (*At<uint8_t>(term, g_offProcessing) & g_maskProcessing) != 0;
    return true;
}

bool WriteProcessing(void* term, bool busy) {
    if (!LayoutCurrent() || !IsTerminal(term)) return false;
    uint8_t& b = *At<uint8_t>(term, g_offProcessing);
    b = busy ? static_cast<uint8_t>(b | g_maskProcessing) : static_cast<uint8_t>(b & ~g_maskProcessing);
    return true;
}

bool WritePanel(void* term, void* desk) { return LayoutCurrent() && IsTerminal(term) && WriteObject(term, g_offPanel, desk); }
void* ReadUsed(void* term) { return (LayoutCurrent() && IsTerminal(term)) ? ReadObject(term, g_offUsed) : nullptr; }
bool WriteUsed(void* term, void* panel) { return LayoutCurrent() && IsTerminal(term) && WriteObject(term, g_offUsed, panel); }

void* ReadActiveDish(void* term) {
    return (LayoutCurrent() && IsTerminal(term)) ? ReadObject(term, g_offActiveDish) : nullptr;
}

bool ReadName(void* term, std::wstring& out) {
    if (!LayoutCurrent() || !IsTerminal(term)) return false;
    return ReadString(term,g_offName,out,65536);
}

bool ReadLog(void* term, std::wstring& out) {
    if (!LayoutCurrent() || !IsTerminal(term)) return false;
    return ReadString(term,g_offLog,out,1024*1024);
}

bool CallInit(void* term, const std::wstring& name, bool hide, void* dish) {
    if (!LayoutCurrent() || !IsTerminal(term) || name.size()>65536) return false;
    Lease owner(term);CachedObjRef selectedDish;selectedDish.Set(dish);
    const auto valid=[&]{return owner.Current() && (!dish || selectedDish.Is(dish));};
    CachedObjRef function;function.Set(TerminalFn(term,kInit));
    ParamFrame f(function.Get());
    if (!f.valid() || !valid() || !function.Is(f.function())) return false;
    // The frame's string may point at our buffer for the call: init copies it (ToLower) before it
    // keeps anything.
    FStringView v{const_cast<wchar_t*>(name.c_str()), static_cast<int32_t>(name.size() + 1),
                  static_cast<int32_t>(name.size() + 1)};
    const uint8_t h = hide ? 1 : 0;
    if (!f.SetRaw(L"name", &v, sizeof(v)) || !f.SetRaw(L"hide", &h, sizeof(h)) || !f.Set(L"dish", dish))
        return false;
    return valid() && function.Get() && Call(term, f) && valid() && function.Get();
}

bool RunCommand(void* term, const std::wstring& line) {
    if (!LayoutCurrent() || !IsTerminal(term) || line.size()>65536) return false;
    Lease owner(term);const auto offset=g_offCommand;
    if (!owner.Current() || !WriteFStringField(term, offset, line) || !owner.Current()) return false;
    CachedObjRef function;function.Set(TerminalFn(term,kEnterCommand));
    ParamFrame f(function.Get());
    if (!f.valid() || !owner.Current() || !function.Is(f.function())) return false;
    // enterCommand copies c into its locals and the ubergraph frame before it keeps anything.
    FStringView v{const_cast<wchar_t*>(line.c_str()), static_cast<int32_t>(line.size() + 1),
                  static_cast<int32_t>(line.size() + 1)};
    if (!f.SetRaw(L"c", &v, sizeof(v))) return false;
    std::wstring written;
    return owner.Current() && function.Get() && ReadString(term,offset,written,65536) && written==line &&
        Call(term, f) && owner.Current() && function.Get();
}

bool CallWriteToLog(void* term, const std::wstring& text, uint8_t bracketsType) {
    if (!LayoutCurrent() || !IsTerminal(term) || text.size()>8192 || bracketsType>3) return false;
    Lease owner(term);CachedObjRef function;function.Set(TerminalFn(term,kWriteToLog));
    ParamFrame f(function.Get());
    if (!f.valid() || !owner.Current() || !function.Is(f.function())) return false;
    // writeToLog concatenates B into a new string before it keeps anything.
    FStringView v{const_cast<wchar_t*>(text.c_str()), static_cast<int32_t>(text.size() + 1),
                  static_cast<int32_t>(text.size() + 1)};
    if (!f.SetRaw(L"B", &v, sizeof(v)) || !f.SetRaw(L"bracketsType", &bracketsType, sizeof(bracketsType)))
        return false;
    return owner.Current() && function.Get() && Call(term, f) && owner.Current() && function.Get();
}

bool ConsumeInput(void* term) {
    return ConsumeInputWhile(term, [] { return true; });
}

bool ConsumeInputWhile(void* term, const std::function<bool()>& current) {
    if (!current || !current() || !LayoutCurrent() || !IsTerminal(term)) return false;
    CachedObjRef terminal, terminalClass, textBox, textBoxClass, setText;
    terminal.Set(term);
    void* cls = R::ClassOf(term);
    terminalClass.Set(cls);
    const int32_t commandOffset = g_offCommand, boxOffset = g_offTextBox;
    if (commandOffset < 0 || boxOffset < 0) return false;
    void* box = *At<void*>(term, boxOffset);
    if (box) {
        if (!R::IsLive(box) || !g_fnSetText || !R::IsLive(g_fnSetText)) return false;
        textBox.Set(box);
        textBoxClass.Set(R::ClassOf(box));
        setText.Set(g_fnSetText);
    }
    const auto valid = [&] {
        return current() && terminal.Is(term) && terminalClass.Is(cls) && R::ClassOf(term) == cls &&
            LayoutCurrent() && g_offCommand == commandOffset && g_offTextBox == boxOffset &&
            *At<void*>(term, boxOffset) == box && (!box || (textBox.Is(box) && textBoxClass.Get() && R::ClassOf(box)==textBoxClass.Get() && setText.Is(g_fnSetText)));
    };
    if (!valid()) return false;
    if (!box) {
        // The command itself is the complete input when there is no text box.
        return WriteFStringField(term, commandOffset, std::wstring()) && valid();
    }
    // Minting empty FText may dispatch. Finish every fallible preparation before
    // changing command, and never use a box that its terminal replaced meanwhile.
    uint8_t empty[ftext_utils::kFTextSize];
    if (!ftext_utils::EmptyFText(empty) || !valid()) return false;
    ParamFrame f(g_fnSetText);
    if (!valid() || !f.valid() || !f.SetRaw(L"InText", empty, sizeof(empty)) || !valid()) return false;
    if (!WriteFStringField(term, commandOffset, std::wstring()) || !valid()) return false;
    const bool consumed = Call(box, f);
    return valid() && consumed;
}

bool ClearLog(void* term) {
    if(!LayoutCurrent() || !IsTerminal(term))return false;
    Lease owner(term);const auto log=g_offLog;
    const int logText=R::FindPropertyOffset(R::ClassOf(term),L"logText");
    if(logText<0 || !owner.Current() || !WriteFStringField(term,log,L"") || !owner.Current())return false;
    if(!WriteFStringField(term,logText,L"") || !owner.Current())return false;
    return CallWriteToLog(term,L"",0);
}

std::wstring PanelName(void* panel) {
    if (!panel || !R::IsLive(panel)) return {};
    return R::ToString(R::NameOf(panel));
}

void* FindPanelByName(const std::wstring& name) {
    if (name.empty() || !EnsureResolved()) return nullptr;
    void* desk = console_desk::Instance();
    Lease owner(desk);if (!owner.Current() || owner.cls.Get()!=g_layoutRefs[2].Get()) return nullptr;
    const auto* arr = At<TArrayView>(desk, g_offDeskPanels);
    const auto snapshot=*arr;
    if (!snapshot.data || snapshot.num <= 0 || snapshot.max<snapshot.num || snapshot.num>4096) return nullptr;
    void* const* elems = reinterpret_cast<void* const*>(snapshot.data);
    for (int32_t i = 0; i < snapshot.num; ++i) {
        if(!owner.Current() || arr->data!=snapshot.data || arr->num!=snapshot.num || arr->max!=snapshot.max)return nullptr;
        void* p = elems[i];
        CachedObjRef candidate;candidate.Set(p);
        if (candidate.Get() && R::ToString(R::NameOf(p)) == name)
            return owner.Current() && candidate.Get() && arr->data==snapshot.data && arr->num==snapshot.num && arr->max==snapshot.max?p:nullptr;
    }
    return nullptr;
}

}  // namespace ue_wrap::sat_console
