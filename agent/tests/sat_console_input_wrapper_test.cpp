// Exercise the production input wrapper, parameter frames, FText mint and empty
// FString field write; only engine reflection/allocation is doubled.
#ifndef SAT_CONSOLE_WRAPPER_SOURCE
#define SAT_CONSOLE_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/desk/sat_console.cpp"
#endif
#include SAT_CONSOLE_WRAPPER_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/field_io.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/fstring_utils.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/ftext_utils.cpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL wrapper line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)

namespace SC = ue_wrap::sat_console;
namespace R = ue_wrap::reflection;
namespace test {
struct Obj {
    int id=0, serial=1;
    bool live=true;
    void* cls=nullptr;
    ue_wrap::field_io::FStringView command{};
    void* box=nullptr;
    int cleared=0;
};
std::array<Obj,16> objects;
Obj& term=objects[1]; Obj& box=objects[2]; Obj& replacement=objects[3];
Obj& termClass=objects[4]; Obj& boxClass=objects[5]; Obj& setText=objects[6];
Obj& textLibrary=objects[7]; Obj& mintText=objects[8]; Obj& otherClass=objects[9];
Obj& otherFunction=objects[10]; Obj& otherTerm=objects[11];
uint32_t world=0;
bool current=true, mintOk=true, callOk=true, paramsOk=true;
bool metadataReady=false,missingEnter=false,missingWrite=false,missingInit=false;
int frameBytes=ue_wrap::ftext_utils::kFTextSize;
int mints=0, calls=0, frees=0;
void* shown=nullptr;
std::function<void()> onMint, onFrame, onParams, onSetText, onFree;

void Fire(std::function<void()>& hook) { auto copy=std::move(hook); hook={}; if(copy)copy(); }
void Reset() {
    for(auto& obj:objects)if(obj.command.data)std::free(obj.command.data);
    ++world;
    for(int i=0;i<int(objects.size());++i){objects[i]={};objects[i].id=i;objects[i].serial=int(world)*100;}
    term.cls=&termClass; otherTerm.cls=&termClass;
    box.cls=replacement.cls=&boxClass; term.box=&box;textLibrary.cls=&otherClass;
    term.command.data=static_cast<wchar_t*>(std::malloc(8*sizeof(wchar_t)));
    CHECK(term.command.data);std::memcpy(term.command.data,L"sv.ping",8*sizeof(wchar_t));
    term.command.num=term.command.max=8;
    current=mintOk=callOk=paramsOk=true;frameBytes=ue_wrap::ftext_utils::kFTextSize;
    mints=calls=frees=0;shown=&term;onMint=onFrame=onParams=onSetText=onFree={};
    SC::g_termCls.Set(&termClass);SC::g_resolved=true;
    for(auto& ref:SC::g_layoutRefs)ref.Set(&termClass);
    SC::g_nextTryMs=0;SC::g_attempts=0;SC::g_resolving=false;
    metadataReady=missingEnter=missingWrite=missingInit=false;
    SC::g_offCommand=int(offsetof(Obj,command));SC::g_offTextBox=int(offsetof(Obj,box));SC::g_fnSetText=&setText;
    ue_wrap::ftext_utils::g_cached=false;ue_wrap::ftext_utils::g_ktlCdo=nullptr;
    ue_wrap::ftext_utils::g_convStrToTextFn=nullptr;ue_wrap::ftext_utils::g_inputParam=nullptr;
}
bool Consume() {
    const auto sourceWorld=world;
    return SC::ConsumeInputWhile(&term,[sourceWorld]{return current && world==sourceWorld && shown==&term;});
}
void Untouched() { CHECK(term.command.num==8 && term.command.data && std::wstring(term.command.data)==L"sv.ping");CHECK(calls==0 && frees==0); }
}

namespace ue_wrap::reflection {
void* ObjectAt(int32_t i){return i>=0 && i<int(test::objects.size())?&test::objects[i]:nullptr;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){auto* p=static_cast<test::Obj*>(ObjectAt(i));return p?p->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return p && ObjectAt(i)==p && test::objects[i].live;}
bool IsLive(void* p){return p && IsLiveByIndex(p,InternalIndexOf(p));}
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
void* FindClass(const wchar_t*){return &test::termClass;}
void* FindClassDefaultObject(const wchar_t*){return &test::textLibrary;}
void* FindFunction(void*,const wchar_t* name){
    if((test::missingEnter&&!std::wcscmp(name,L"enterCommand")) ||
       (test::missingWrite&&!std::wcscmp(name,L"writeToLog")) ||
       (test::missingInit&&!std::wcscmp(name,L"init")))return nullptr;
    return !std::wcscmp(name,L"Conv_StringToText")?&test::mintText:&test::setText;
}
void* FindDispatchFunctionCached(void*,const wchar_t*){return &test::setText;}
int32_t FindPropertyOffset(void*,const wchar_t*){return test::metadataReady?0:-1;}
bool FindBoolProperty(void*,const wchar_t*,int32_t& offset,uint8_t& mask){if(!test::metadataReady)return false;offset=0;mask=1;return true;}
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
bool MarkPendingKill(void* p){static_cast<test::Obj*>(p)->live=false;return true;}
const FName& NameOf(void*){static FName name{};return name;}
std::wstring ToString(const FName&){return L"test";}
void* EngineAlloc(size_t bytes,uint32_t){return std::malloc(bytes);}
void EngineFree(void* p){if(p){++test::frees;std::free(p);test::Fire(test::onFree);}}
int32_t FunctionFrameSize(void* p){
    if(p==&test::mintText)return 16+ftext_utils::kFTextSize;
    test::Fire(test::onFrame);return test::frameBytes;
}
std::vector<ParamInfo> FunctionParams(void* p){
    if(p==&test::mintText)return {{L"InString",0,16,0},{L"ReturnValue",16,ftext_utils::kFTextSize,0}};
    test::Fire(test::onParams);
    return {{test::paramsOk?L"InText":L"Unknown",0,ftext_utils::kFTextSize,0}};
}
bool CallFunction(void* target,void* function,void* data){
    using namespace test;
    if(function==&mintText){
        CHECK(target==&textLibrary);auto* input=static_cast<FString*>(data);
        CHECK(input->Num==1 && input->Data && !input->Data[0]);++mints;Fire(onMint);
        if(!mintOk)return false;
        std::memset(static_cast<uint8_t*>(data)+16,0x5A,ftext_utils::kFTextSize);return true;
    }
    CHECK(function==&setText && target==&box);++calls;
    for(int i=0;i<ftext_utils::kFTextSize;++i)CHECK(static_cast<uint8_t*>(data)[i]==0x5A);
    if(callOk)++static_cast<Obj*>(target)->cleared;
    Fire(onSetText);return callOk;
}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return nullptr;} // these widgets belong to the game instance
}
namespace ue_wrap::console_desk {void* Instance(){return nullptr;}void* AtlasWidget(){return nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}

int main(){using namespace test;
    Reset();CHECK(Consume());CHECK(mints==1 && calls==1 && box.cleared==1 && frees==1);CHECK(!term.command.data && !term.command.num);
    Reset();CHECK(SC::ConsumeInput(&term));CHECK(calls==1 && box.cleared==1);
    Reset();term.box=nullptr;CHECK(Consume());CHECK(mints==0 && calls==0 && frees==1 && !term.command.data);
    Reset();CHECK(!SC::ConsumeInputWhile(&term,{}));Untouched();CHECK(mints==0);
    Reset();current=false;CHECK(!Consume());Untouched();CHECK(mints==0);
    Reset();box.live=false;CHECK(!Consume());Untouched();CHECK(mints==0);
    Reset();mintOk=false;CHECK(!Consume());Untouched();CHECK(mints==1);
    Reset();paramsOk=false;CHECK(!Consume());Untouched();CHECK(mints==1);
    Reset();frameBytes=ue_wrap::ftext_utils::kFTextSize-1;CHECK(!Consume());Untouched();CHECK(mints==1);
    Reset();callOk=false;CHECK(!Consume());CHECK(calls==1 && !box.cleared && !term.command.data && frees==1);

    // Minting FText and inspecting SetText's reflected frame can retire either
    // object, its class/function, or the exact field that owns the text box.
    for(int stage=0;stage<3;++stage)for(int change=0;change<13;++change){
        Reset();const auto retire=[change]{switch(change){
            case 0:current=false;break;case 1:++world;break;case 2:shown=&otherTerm;break;
            case 3:++term.serial;break;case 4:term.live=false;break;
            case 5:++box.serial;break;case 6:box.live=false;break;
            case 7:term.box=&replacement;break;case 8:term.box=nullptr;break;
            case 9:++termClass.serial;break;case 10:term.cls=&otherClass;break;
            case 11:++setText.serial;break;case 12:SC::g_fnSetText=&otherFunction;break;
        }};
        if(stage==0)onMint=retire;else if(stage==1)onFrame=retire;else onParams=retire;
        const bool consumed=Consume();
        if(consumed)std::fprintf(stderr,"unexpected consumption at stage=%d change=%d\n",stage,change);
        CHECK(!consumed);Untouched();CHECK(mints==1 && box.cleared==0 && replacement.cleared==0);
    }
    Reset();onFree=[]{current=false;};CHECK(!Consume());CHECK(calls==0 && frees==1 && !term.command.data);
    for(int change=0;change<5;++change){Reset();onSetText=[change]{
        if(change==0)current=false;if(change==1)++term.serial;if(change==2)++box.serial;
        if(change==3)term.box=&replacement;if(change==4)++world;
    };CHECK(!Consume());CHECK(calls==1 && frees==1 && box.cleared==1 && !replacement.cleared);}
    Reset();SC::g_resolved=false;CHECK(!Consume());Untouched();CHECK(!mints);
    Reset();SC::g_offCommand=-1;CHECK(!Consume());Untouched();CHECK(!mints);
    Reset();SC::g_offTextBox=-1;CHECK(!Consume());Untouched();CHECK(!mints);
    Reset();SC::g_resolved=false;
    for(int i=0;i<8;++i){SC::g_nextTryMs=0;CHECK(!SC::EnsureResolved());}
    metadataReady=true;SC::g_nextTryMs=0;CHECK(SC::EnsureResolved());
    puts("PASS console delayed metadata recovers after eight incomplete attempts");
    // A new attempt cannot borrow a parameter offset from an incomplete predecessor.
    for(bool write:{false,true}){
        Reset();SC::g_resolved=false;metadataReady=true;missingInit=true;CHECK(!SC::EnsureResolved());
        missingInit=false;if(write)missingWrite=true;else missingEnter=true;
        SC::g_nextTryMs=0;CHECK(!SC::EnsureResolved());CHECK(write?SC::g_offWriteB<0:SC::g_offEnterC<0);
        missingWrite=missingEnter=false;SC::g_nextTryMs=0;CHECK(SC::EnsureResolved());
    }
    puts("PASS console incomplete resolutions never reuse previous parameter offsets");
    Reset();SC::g_resolved=false;metadataReady=true;CHECK(SC::EnsureResolved());
    ++setText.serial;CHECK(!SC::LayoutCurrent());CHECK(SC::EnsureResolved());CHECK(SC::LayoutCurrent());
    ++termClass.serial;CHECK(!SC::LayoutCurrent());CHECK(SC::EnsureResolved());CHECK(SC::LayoutCurrent());
    puts("PASS console replaced class/function metadata re-resolves before use");
    Reset();SC::g_offEnterC=0;SC::g_offWriteB=0;SC::g_offWriteType=sizeof(ue_wrap::field_io::FStringView);
    struct Args {ue_wrap::field_io::FStringView value;uint8_t type=2;};
    wchar_t good[]=L"help";Args args{{good,5,5}};std::wstring text;uint8_t type=0;
    const auto* raw=reinterpret_cast<const uint8_t*>(&args);
    CHECK(SC::ReadEnterCommandLine(raw,text)&&text==L"help");
    CHECK(SC::ReadWriteToLogArgs(raw,text,type)&&text==L"help"&&type==2);
    for(int mode=0;mode<5;++mode){args.value={good,5,5};
        if(mode==0)args.value.num=-1;if(mode==1)args.value.max=2;
        if(mode==2)args.value.data=nullptr;if(mode==3)args.value.num=70000;
        if(mode==4){args.value.num=4;args.value.max=4;}
        CHECK(!SC::ReadEnterCommandLine(raw,text));CHECK(!SC::ReadWriteToLogArgs(raw,text,type));
    }
    args.value={nullptr,0,0};CHECK(SC::ReadEnterCommandLine(raw,text)&&text.empty());
    args.value={good,9000,9000};CHECK(!SC::ReadWriteToLogArgs(raw,text,type));
    Reset();puts("PASS bounded console FString headers reject malformed storage and oversized output");
    puts("PASS production console input wrapper: real ParamFrame/FText/FString, no-box success, preparation/errors and callback/serial/field-owner guards");
}
