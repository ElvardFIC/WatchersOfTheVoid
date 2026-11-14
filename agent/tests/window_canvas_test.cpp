#ifndef WINDOW_CANVAS_SOURCE
#define WINDOW_CANVAS_SOURCE "../../src/votv-coop/src/ue_wrap/devices/window_canvas.cpp"
#endif
#include WINDOW_CANVAS_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

const char* canvasCase="setup";
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s line %d: %s\n",canvasCase,__LINE__,#x);std::exit(1);}}while(0)
namespace canvas_test {
namespace W=ue_wrap::window_canvas;
namespace R=ue_wrap::reflection;
enum Id {Window,WindowClass,ChildClass,Canvas,Canvas2,CanvasClass,Mid,Mid2,MidClass,Parent,ParentClass,
    Sponge,SpongeClass,SpongeCDO,Player,PlayerClass,GM,GMClass,Panels,PanelsClass,Library,LibraryClass,
    DrawFn,OpenFn,SetFn,GetFn,CreateFn,NewClass,VectorStruct,GraphFn,Count};
struct GraphFrame {void* material=nullptr;float size=16;void* thrownMaterial=nullptr;float thrownSize=48;} graphFrame;
ue_wrap::ufunction_hook::CallerFrame caller{};
struct Obj {
    int id=0,serial=0;bool live=true;void* cls=nullptr;void* owner=nullptr;
    void* holding=nullptr;void* canv=nullptr;void* mid=nullptr;void* parent=nullptr;void* panels=nullptr;
    ue_wrap::FVector2D point{12,23};float size=32,strength=1,soap=0,opac=.4f,col=0;uint8_t flags=0;
} objects[Count];
void* At(Id id){return &objects[id];}
uint32_t world=1;
bool current=true,classes=true,canvasReady=true,createOK=true,scalarOK=true,drawOK=true,names=true;
bool vectorType=true,vectorLayout=true;
int opens=0,sets=0,gets=0,creates=0,draws=0;
void* selectedWindow=nullptr;void* factoryResult=nullptr;void* gamemode=nullptr;
std::wstring trigger,missing,wrongSize,wrongOffset,badFrame,missingField;
std::function<void()> callback;
int triggerAt=1;
std::set<void*>& Roots(){static auto* roots=new std::set<void*>();return *roots;}
void Event(const std::wstring& label){if(trigger==label && callback && --triggerAt==0){auto fn=std::move(callback);callback={};fn();}}
std::wstring FnName(void* fn){
    const auto id=static_cast<Obj*>(fn)->id;
    if(id==DrawFn)return L"K2_DrawMaterial";if(id==OpenFn)return L"canvas";
    if(id==SetFn)return L"SetScalarParameterValue";if(id==GetFn)return L"K2_GetScalarParameterValue";
    if(id==GraphFn)return L"ExecuteUbergraph_d_window";
    CHECK(id==CreateFn);return L"CreateDynamicMaterialInstance";
}
std::vector<R::ParamInfo> Params(void* fn){
    const auto id=static_cast<Obj*>(fn)->id;
    if(id==OpenFn)return {};
    if(id==CreateFn)return {{L"WorldContextObject",0,8,0},{L"Parent",8,8,0},{L"ReturnValue",16,8,0}};
    if(id==SetFn || id==GetFn)return {{L"ParameterName",0,8,0},{id==SetFn?L"Value":L"ReturnValue",8,4,0}};
    CHECK(id==DrawFn);
    return {{L"RenderMaterial",0,8,0},{L"ScreenPosition",8,8,0},{L"ScreenSize",16,8,0},
        {L"CoordinatePosition",24,8,0},{L"CoordinateSize",32,8,0},{L"Rotation",40,4,0},{L"PivotPoint",44,8,0}};
}
template<class T>T Arg(void* data,void* fn,const wchar_t* name){
    T value{};for(const auto& p:Params(fn))if(p.name==name){CHECK(sizeof(T)==p.size);std::memcpy(&value,static_cast<uint8_t*>(data)+p.offset,sizeof(T));return value;}
    CHECK(false);return value;
}
template<class T>void Output(void* data,void* fn,T value){
    for(const auto& p:Params(fn))if(p.name==L"ReturnValue"){CHECK(sizeof(T)==p.size);std::memcpy(static_cast<uint8_t*>(data)+p.offset,&value,sizeof(T));return;}
    CHECK(false);
}
W::Dab Dab(){return {12,23,32,.7f,.25f};}
W::DrawResult Draw(){return W::DrawDab(At(Window),Dab(),[]{return current;});}
void Reset(){
    callback={};trigger.clear();triggerAt=1;W::ReleaseBrush();CHECK(Roots().empty());++world;
    for(int i=0;i<Count;++i){const int serial=objects[i].serial+1;objects[i]={};objects[i].id=i;objects[i].serial=serial;objects[i].cls=&objects[i];}
    classes=current=canvasReady=createOK=scalarOK=drawOK=names=vectorType=vectorLayout=true;
    opens=sets=gets=creates=draws=0;missing.clear();wrongSize.clear();wrongOffset.clear();badFrame.clear();missingField.clear();
    selectedWindow=At(WindowClass);factoryResult=At(Mid);gamemode=At(GM);
    objects[Window].cls=At(WindowClass);objects[Window].canv=At(Canvas);
    objects[Canvas].cls=objects[Canvas2].cls=At(CanvasClass);
    objects[Mid].cls=objects[Mid2].cls=At(MidClass);objects[Parent].cls=At(ParentClass);
    objects[Sponge].cls=objects[SpongeCDO].cls=At(SpongeClass);objects[Sponge].mid=At(Mid);objects[SpongeCDO].parent=At(Parent);
    objects[Player].cls=At(PlayerClass);objects[Player].holding=At(Sponge);
    objects[GM].cls=At(GMClass);objects[GM].panels=At(Panels);objects[Panels].cls=At(PanelsClass);
    objects[Library].cls=At(LibraryClass);
    objects[DrawFn].owner=At(CanvasClass);objects[OpenFn].owner=At(WindowClass);
    objects[GraphFn].owner=At(WindowClass);graphFrame={At(Mid),16,At(Mid2),48};caller={At(GraphFn),reinterpret_cast<uint8_t*>(&graphFrame),100};
    objects[SetFn].owner=objects[GetFn].owner=At(MidClass);objects[CreateFn].owner=At(LibraryClass);
    W::g_core={};W::g_sponge={};W::g_nOpac={};W::g_nCol={};
}
bool Select(int argc,char** argv,const char* name){canvasCase=name;return argc<2 || std::string(argv[1])==name;}
}
namespace ue_wrap::reflection {
bool IsLive(void* object){return object && static_cast<canvas_test::Obj*>(object)->live;}
int32_t InternalIndexOf(void* object){return object?static_cast<canvas_test::Obj*>(object)->id:-1;}
int32_t SlotSerial(int32_t id){return canvas_test::objects[id].serial;}
int32_t AllocateSlotSerial(int32_t id){return SlotSerial(id);}
void* ObjectAt(int32_t id){return id>=0 && id<canvas_test::Count?&canvas_test::objects[id]:nullptr;}
bool IsLiveByIndex(void* object,int32_t id){return id>=0 && id<canvas_test::Count && object==&canvas_test::objects[id] && IsLive(object);}
void* ClassOf(void* object){return static_cast<canvas_test::Obj*>(object)->cls;}
void* OuterOf(void* object){return static_cast<canvas_test::Obj*>(object)->owner;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){
    using namespace canvas_test;
    return count==1 && (cls==bases[0] || (cls==At(ChildClass) && bases[0]==selectedWindow));
}
const FName& NameOf(void* object){static FName name{};name.ComparisonIndex=InternalIndexOf(object)+100;return name;}
bool NameEquals(const FName& name,const wchar_t* text){return name.ComparisonIndex==canvas_test::VectorStruct+100 && !wcscmp(text,L"Vector2D");}
std::wstring ToString(const FName&){return L"window_canvas_fixture";}
int32_t FindPropertyOffset(void*,const wchar_t* name){
    using namespace canvas_test;Event(L"offset:"+std::wstring(name));if(missingField==name)return -1;
    if(!wcscmp(name,L"X"))return 0;if(!wcscmp(name,L"Y"))return vectorLayout?4:8;
    if(!wcscmp(name,L"A"))return offsetof(Obj,point);if(!wcscmp(name,L"canv"))return offsetof(Obj,canv);
    if(!wcscmp(name,L"size"))return offsetof(Obj,size);if(!wcscmp(name,L"strength"))return offsetof(Obj,strength);
    if(!wcscmp(name,L"soapAmount"))return offsetof(Obj,soap);if(!wcscmp(name,L"dynmat"))return offsetof(Obj,mid);
    if(!wcscmp(name,L"drawmat"))return offsetof(Obj,parent);if(!wcscmp(name,L"holding_actor"))return offsetof(Obj,holding);
    CHECK(!wcscmp(name,L"analogPanels"));return offsetof(Obj,panels);
}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& offset,uint8_t& mask){
    using namespace canvas_test;Event(L"bool:"+std::wstring(name));if(missingField==name)return false;
    offset=offsetof(Obj,flags);mask=!wcscmp(name,L"cv")?4:8;return true;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name){
    using namespace canvas_test;Event(L"lookup:"+std::wstring(name));if(missing==name)return nullptr;
    if(!wcscmp(name,L"K2_DrawMaterial"))return At(DrawFn);if(!wcscmp(name,L"canvas"))return At(OpenFn);
    if(!wcscmp(name,L"ExecuteUbergraph_d_window"))return At(GraphFn);
    if(!wcscmp(name,L"SetScalarParameterValue"))return At(SetFn);if(!wcscmp(name,L"K2_GetScalarParameterValue"))return At(GetFn);
    CHECK(!wcscmp(name,L"CreateDynamicMaterialInstance"));return At(CreateFn);
}
void* FindClassDefaultObject(const wchar_t* name){
    using namespace canvas_test;Event(L"cdo:"+std::wstring(name));return !wcscmp(name,L"prop_sponge_C")?At(SpongeCDO):At(Library);
}
void* FindObject(const wchar_t*,const wchar_t*){return canvas_test::At(canvas_test::Parent);}
int32_t FindParamSize(void* fn,const wchar_t* name){
    using namespace canvas_test;Event(L"param:"+std::wstring(name));if(missing==name)return -1;if(wrongSize==name)return 1;
    for(const auto& p:Params(fn))if(p.name==name)return p.size;return -1;
}
int32_t FindParamOffset(void* fn,const wchar_t* name){
    using namespace canvas_test;
    for(const auto& p:Params(fn))if(p.name==name)return p.offset+(wrongOffset==name?4:0);return -1;
}
void* PropertyInnerStruct(void*,const wchar_t*){return canvas_test::At(canvas_test::vectorType?canvas_test::VectorStruct:canvas_test::Parent);}
int32_t StructSize(void*){return sizeof(ue_wrap::FVector2D);}
int32_t FunctionFrameSize(void* fn){
    using namespace canvas_test;Event(L"frame:"+FnName(fn));if(badFrame==FnName(fn))return 0;
    if(fn==At(GraphFn))return sizeof(GraphFrame);
    const auto params=Params(fn);return params.empty()?0:params.back().offset+params.back().size;
}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& type){
    using namespace canvas_test;
    if(!wcscmp(name,L"CallFunc_wash_dynmat")){offset=offsetof(GraphFrame,material);type=L"ObjectProperty";}
    else if(!wcscmp(name,L"CallFunc_wash_size")){offset=offsetof(GraphFrame,size);type=L"FloatProperty";}
    else if(!wcscmp(name,L"CallFunc_wash_dynmat_1")){offset=offsetof(GraphFrame,thrownMaterial);type=L"ObjectProperty";}
    else if(!wcscmp(name,L"CallFunc_wash_size_1")){offset=offsetof(GraphFrame,thrownSize);type=L"FloatProperty";}
    else return false;
    return true;
}
std::vector<ParamInfo> FunctionParams(void* fn){return canvas_test::Params(fn);}
bool CallFunction(void* target,void* fn,void* raw){
    using namespace canvas_test;const auto id=static_cast<Obj*>(fn)->id;
    if(id==OpenFn){CHECK(target==At(Window));++opens;objects[Window].canv=canvasReady?At(Canvas):nullptr;objects[Window].flags|=4;Event(L"call:canvas");return true;}
    if(id==SetFn){CHECK(target==At(Mid) || target==At(Mid2));++sets;
        const auto name=Arg<FName>(raw,fn,L"ParameterName");const auto value=Arg<float>(raw,fn,L"Value");
        if(scalarOK){if(name.ComparisonIndex==1)static_cast<Obj*>(target)->opac=value;else {CHECK(name.ComparisonIndex==2);static_cast<Obj*>(target)->col=value;}}
        Event(name.ComparisonIndex==1?L"call:opac":L"call:col");return scalarOK;
    }
    if(id==GetFn){++gets;const auto name=Arg<FName>(raw,fn,L"ParameterName");
        Output(raw,fn,name.ComparisonIndex==1?static_cast<Obj*>(target)->opac:static_cast<Obj*>(target)->col);
        Event(name.ComparisonIndex==1?L"read:opac":L"read:col");return scalarOK;
    }
    if(id==CreateFn){CHECK(target==At(Library) && Arg<void*>(raw,fn,L"WorldContextObject")==At(Window) && Arg<void*>(raw,fn,L"Parent")==At(Parent));
        ++creates;Output(raw,fn,createOK?factoryResult:nullptr);Event(L"call:factory");return createOK;
    }
    CHECK(id==DrawFn && (target==At(Canvas) || target==At(Canvas2)));++draws;
    const auto material=Arg<void*>(raw,fn,L"RenderMaterial");CHECK(material==factoryResult);
    CHECK(static_cast<Obj*>(material)->opac==.7f && static_cast<Obj*>(material)->col==.25f);
    const auto position=Arg<FVector2D>(raw,fn,L"ScreenPosition"),size=Arg<FVector2D>(raw,fn,L"ScreenSize"),
        origin=Arg<FVector2D>(raw,fn,L"CoordinatePosition"),extent=Arg<FVector2D>(raw,fn,L"CoordinateSize"),pivot=Arg<FVector2D>(raw,fn,L"PivotPoint");
    CHECK(position.X==12 && position.Y==23 && size.X==32 && size.Y==32 && origin.X==0 && origin.Y==0 &&
        extent.X==1 && extent.Y==1 && pivot.X==.5f && pivot.Y==.5f && Arg<float>(raw,fn,L"Rotation")==0);
    Event(L"call:draw");return drawOK;
}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){using namespace canvas_test;Event(L"class:"+std::wstring(name));if(!classes)return nullptr;
    if(!wcscmp(name,L"d_window_C"))return selectedWindow;if(!wcscmp(name,L"Canvas"))return At(CanvasClass);
    if(!wcscmp(name,L"prop_sponge_C"))return At(SpongeClass);CHECK(!wcscmp(name,L"MaterialInstanceDynamic"));return At(MidClass);}
}
namespace ue_wrap::fname_utils {
reflection::FName StringToFName(const std::wstring& text){using namespace canvas_test;Event(L"name:"+text);return names?reflection::FName{text==L"opac"?1:2,0}:reflection::FName{};}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return canvas_test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(canvas_test::world));}
void* WorldOf(void*){return nullptr;}
}
namespace ue_wrap::world_singleton {void* Gamemode(){canvas_test::Event(L"gamemode");return canvas_test::gamemode;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::ufunction_hook {CallerFrame CurrentCallerFrame(){return canvas_test::caller;}}
namespace ue_wrap::script_calls {
bool EnumerateCalls(void*,std::vector<CallSite>& out){
    out={CallSite{},CallSite{}};out[0].start=10;out[0].end=100;out[1].start=20;out[1].end=200;
    for(auto& site:out)site.callee=canvas_test::At(canvas_test::DrawFn);return true;
}
bool SameCallee(const CallSite& site,void* fn){return site.callee==fn;}
bool CallHasLocal(const CallSite& site,const wchar_t* name){
    return !wcscmp(name,site.start==10?L"CallFunc_wash_dynmat":L"CallFunc_wash_dynmat_1");
}
}
namespace ue_wrap {
bool GcPin::Pin(void* object){Release();if(!object || canvas_test::Roots().contains(object))return false;obj_=object;canvas_test::Roots().insert(object);canvas_test::Event(L"pin");return true;}
void GcPin::Release(){if(!obj_)return;void* old=obj_;obj_=nullptr;canvas_test::Roots().erase(old);canvas_test::Event(L"unpin");}
}

int main(int argc,char** argv){using namespace canvas_test;using Result=W::DrawResult;
    if(Select(argc,argv,"original-dab")){
        Reset();CHECK(W::EnsureResolved());objects[Window].flags=4;objects[Player].holding=nullptr;
        objects[Sponge].mid=nullptr;objects[Mid].opac=.8f;W::Dab dab{};
        CHECK(W::ReadStrokeDab(At(Window),dab,[]{return current;}) && dab.size==32 && dab.opac==.8f && gets==2);
        caller.codeOffset=200;objects[Mid2].opac=.15f;objects[Mid2].col=.25f;
        CHECK(W::ReadStrokeDab(At(Window),dab,[]{return current;}) && dab.size==48 && dab.opac==.15f && dab.col==.25f);
        const auto before=gets;caller.codeOffset=201;CHECK(!W::ReadStrokeDab(At(Window),dab,[]{return current;}) && gets==before);
        caller.codeOffset=100;caller.function=At(OpenFn);CHECK(!W::ReadStrokeDab(At(Window),dab,[]{return current;}));
        for(int cause=0;cause<4;++cause){Reset();CHECK(W::EnsureResolved());objects[Window].flags=4;dab={-9,-9,-9,-9,-9};
            trigger=L"read:opac";callback=[cause]{if(cause==0)graphFrame.material=At(Mid2);if(cause==1)++world;
                if(cause==2)++objects[GraphFn].serial;if(cause==3)++objects[Mid].serial;};
            CHECK(!W::ReadStrokeDab(At(Window),dab,[]{return current;}) && gets==1 && dab.size==-9);
        }
    }
    if(Select(argc,argv,"geometry")){
        Reset();CHECK(Draw()==Result::Applied && draws==1 && creates==1 && sets==2 && Roots().size()==1);
        CHECK(Draw()==Result::Applied && creates==1 && draws==2);W::ReleaseBrush();CHECK(Roots().empty());
    }
    if(Select(argc,argv,"resolution-retry")){
        Reset();classes=false;CHECK(!W::EnsureResolved());classes=true;missingField=L"canv";CHECK(!W::EnsureResolved());
        missingField.clear();CHECK(W::EnsureResolved());++objects[WindowClass].serial;CHECK(!W::IsWindow(At(Window)));CHECK(W::EnsureResolved());
        selectedWindow=At(NewClass);objects[Window].cls=At(NewClass);CHECK(W::EnsureResolved() && W::IsWindow(At(Window)));
        Reset();trigger=L"offset:A";callback=[]{++objects[WindowClass].serial;};CHECK(!W::EnsureResolved());CHECK(W::EnsureResolved());
        Reset();trigger=L"offset:A";callback=[]{selectedWindow=At(NewClass);objects[Window].cls=At(NewClass);};
        CHECK(!W::EnsureResolved());CHECK(W::EnsureResolved());
    }
    if(Select(argc,argv,"raw-observer")){
        Reset();CHECK(W::EnsureResolved());objects[Window].cls=At(ChildClass);objects[Window].flags=2;
        CHECK(W::IsWindow(At(Window)) && !W::IsStrokeDraw(At(Window)));objects[Window].flags|=4;CHECK(W::IsStrokeDraw(At(Window)));
        float x=-1,y=-1;CHECK(W::ReadStrokeCorner(At(Window),x,y) && x==12 && y==23);
        objects[Window].point.X=NAN;CHECK(!W::ReadStrokeCorner(At(Window),x,y));
        objects[Window].live=false;CHECK(!W::IsStrokeDraw(At(Window)));CHECK(!W::ReadStrokeCorner(At(Player),x,y));
    }
    if(Select(argc,argv,"brush-reads")){
        Reset();float size=-9,opac=-9,col=-9;
        CHECK(W::ReadHeldBrush(At(Player),size,opac,col) && size==32 && opac==.4f && col==0 && gets==2);
        objects[Sponge].mid=nullptr;objects[Sponge].soap=2;CHECK(W::ReadHeldBrush(At(Player),size,opac,col) && opac==.8f);
        objects[Sponge].soap=-4;CHECK(W::ReadHeldBrush(At(Player),size,opac,col) && opac==.4f);
        objects[Sponge].mid=At(Mid);objects[Mid].opac=.8f;CHECK(W::ReadHeldBrush(At(Player),size,opac,col) && opac==.8f);
        objects[Sponge].strength=NAN;CHECK(!W::ReadHeldBrush(At(Player),size,opac,col));
        Reset();objects[Mid].opac=NAN;CHECK(!W::ReadHeldBrush(At(Player),size,opac,col));
        Reset();missingField=L"size";CHECK(!W::ReadHeldBrush(At(Player),size,opac,col));missingField.clear();CHECK(W::ReadHeldBrush(At(Player),size,opac,col));
    }
    if(Select(argc,argv,"brush-read-retirement")){
        for(int cause=0;cause<11;++cause){Reset();float size=-9,opac=-9,col=-9;trigger=L"read:opac";callback=[cause]{
            if(cause==0)++world;if(cause==1)current=false;if(cause==2)++objects[Player].serial;if(cause==3)++objects[Sponge].serial;
            if(cause==4)objects[Player].holding=At(Parent);if(cause==5)objects[Sponge].mid=At(Mid2);
            if(cause==6)++objects[Mid].serial;if(cause==7)++objects[SpongeClass].serial;
            if(cause==8)++objects[Sponge].size;if(cause==9)++objects[Sponge].soap;if(cause==10)++objects[Sponge].strength;};
            CHECK(!W::ReadHeldBrush(At(Player),size,opac,col,[]{return current;}) && gets==1 && size==-9 && opac==-9 && col==-9);}
    }
    if(Select(argc,argv,"signal-gate")){
        Reset();CHECK(!W::IsSignalPlaying());objects[Panels].flags=8;CHECK(W::IsSignalPlaying());objects[Panels].flags=2;CHECK(!W::IsSignalPlaying());
        gamemode=nullptr;CHECK(W::IsSignalPlaying());gamemode=At(GM);missingField=L"analogPanels";CHECK(W::IsSignalPlaying());
        missingField.clear();missingField=L"isPlayingSignal";CHECK(W::IsSignalPlaying());missingField.clear();CHECK(!W::IsSignalPlaying());
        trigger=L"bool:isPlayingSignal";callback=[]{objects[GM].panels=At(Parent);};CHECK(W::IsSignalPlaying());
    }
    if(Select(argc,argv,"draw-preparation")){
        Reset();canvasReady=false;CHECK(Draw()==Result::Retry && draws==0);canvasReady=true;CHECK(Draw()==Result::Applied && draws==1);
        Reset();missing=L"ScreenSize";CHECK(Draw()==Result::Retry && draws==0);missing.clear();CHECK(Draw()==Result::Applied);
        Reset();scalarOK=false;CHECK(Draw()==Result::Retry && draws==0 && sets==1);scalarOK=true;CHECK(Draw()==Result::Applied);
        Reset();createOK=false;CHECK(Draw()==Result::Retry && draws==0 && Roots().empty());createOK=true;CHECK(Draw()==Result::Applied);
        Reset();names=false;CHECK(Draw()==Result::Retry && draws==0);names=true;CHECK(Draw()==Result::Applied);
    }
    if(Select(argc,argv,"draw-bindings")){
        const wchar_t* fields[]={L"WorldContextObject",L"Parent",L"ReturnValue",L"ParameterName",L"Value",L"RenderMaterial",L"ScreenPosition",
            L"ScreenSize",L"CoordinatePosition",L"CoordinateSize",L"Rotation",L"PivotPoint"};
        for(const auto field:fields){Reset();wrongSize=field;CHECK(Draw()==Result::Retry && draws==0);wrongSize.clear();CHECK(Draw()==Result::Applied);}
        Reset();badFrame=L"K2_DrawMaterial";CHECK(Draw()==Result::Retry && draws==0);badFrame.clear();++objects[DrawFn].serial;CHECK(Draw()==Result::Applied);
        Reset();wrongOffset=L"ScreenSize";CHECK(Draw()==Result::Retry && draws==0);wrongOffset.clear();CHECK(Draw()==Result::Applied);
    }
    if(Select(argc,argv,"vector-layout")){
        Reset();vectorType=false;CHECK(Draw()==Result::Retry && draws==0);vectorType=true;CHECK(Draw()==Result::Applied);
        Reset();vectorLayout=false;CHECK(Draw()==Result::Retry && draws==0);vectorLayout=true;CHECK(Draw()==Result::Applied);
        Reset();trigger=L"offset:Y";callback=[]{++objects[VectorStruct].serial;};CHECK(Draw()==Result::Retry && draws==0);
        Reset();trigger=L"bool:isPlayingSignal";triggerAt=2;callback=[]{++objects[VectorStruct].serial;};CHECK(Draw()==Result::Retry && draws==0);
        Reset();trigger=L"bool:isPlayingSignal";triggerAt=2;callback=[]{vectorType=false;};CHECK(Draw()==Result::Retry && draws==0);
    }
    if(Select(argc,argv,"draw-retirement")){
        for(int cause=0;cause<11;++cause){Reset();trigger=L"call:canvas";callback=[cause]{
            if(cause==0)++world;if(cause==1)current=false;if(cause==2)++objects[Window].serial;if(cause==3)++objects[WindowClass].serial;
            if(cause==4)W::ReleaseBrush();if(cause==5)objects[Window].canv=At(Canvas2);if(cause==6)objects[Window].canv=nullptr;
            if(cause==7)++objects[DrawFn].serial;if(cause==8)++objects[Mid].serial;if(cause==9)objects[Window].cls=At(PlayerClass);
            if(cause==10)objects[Window].flags=0;};
            const auto result=Draw();if(cause==5){CHECK(result==Result::Applied && draws==1);}else CHECK(result==Result::Retry && draws==0);}
    }
    if(Select(argc,argv,"scalar-retirement")){
        for(int cause=0;cause<8;++cause){Reset();trigger=L"call:opac";callback=[cause]{
            if(cause==0)++world;if(cause==1)current=false;if(cause==2)++objects[Canvas].serial;if(cause==3)objects[Window].canv=At(Canvas2);
            if(cause==4)++objects[Mid].serial;if(cause==5)W::ReleaseBrush();if(cause==6)++objects[SetFn].serial;
            if(cause==7)objects[Panels].flags=8;};
            CHECK(Draw()==Result::Retry && draws==0);if(cause!=7)CHECK(sets==1);}
    }
    if(Select(argc,argv,"binding-retirement")){
        const wchar_t* stages[]={L"lookup:canvas",L"frame:canvas",L"param:RenderMaterial",L"param:PivotPoint",L"name:opac"};
        for(const auto stage:stages){Reset();trigger=stage;callback=[]{current=false;};CHECK(Draw()==Result::Retry && draws==0);}
        Reset();trigger=L"param:ScreenSize";callback=[]{++objects[DrawFn].serial;};CHECK(Draw()==Result::Retry && draws==0);
        Reset();trigger=L"lookup:canvas";callback=[]{++objects[OpenFn].serial;objects[OpenFn].live=false;};CHECK(Draw()==Result::Retry && draws==0);
        Reset();objects[Window].cls=At(ChildClass);objects[OpenFn].owner=At(ChildClass);
        CHECK(Draw()==Result::Retry && opens==0 && draws==0);
        objects[OpenFn].owner=At(WindowClass);CHECK(Draw()==Result::Applied);
    }
    if(Select(argc,argv,"factory-lifetime")){
        for(const auto stage:{L"call:factory",L"pin"}){Reset();trigger=stage;callback=[]{W::ReleaseBrush();};CHECK(Draw()==Result::Retry && draws==0 && Roots().empty());}
        Reset();factoryResult=At(Parent);CHECK(Draw()==Result::Retry && Roots().empty());
        Reset();CHECK(Draw()==Result::Applied);trigger=L"unpin";callback=[]{factoryResult=At(Mid2);CHECK(Draw()==Result::Applied);};
        W::ReleaseBrush();CHECK(W::g_brush.Raw()==At(Mid2) && Roots().contains(At(Mid2)) && !Roots().contains(At(Mid)));
    }
    if(Select(argc,argv,"native-admission")){
        Reset();drawOK=false;CHECK(Draw()==Result::Attempted && draws==1);
        Reset();trigger=L"call:draw";callback=[]{current=false;};CHECK(Draw()==Result::Attempted && draws==1);
        Reset();trigger=L"call:opac";callback=[]{CHECK(Draw()==Result::Retry);};CHECK(Draw()==Result::Applied && draws==1);
        Reset();auto invalid=Dab();invalid.x=NAN;CHECK(W::DrawDab(At(Window),invalid)==Result::Retry && draws==0);
    }
    callback={};W::ReleaseBrush();CHECK(Roots().empty());
    puts("PASS actual window canvas/ParamFrame: checked bindings, retriable preparation, native admission, source/actor/class/canvas/material/function ownership, guarded brush reads and factory pins, signal readiness and teardown reentry");
}
