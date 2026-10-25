#include "../../src/votv-coop/src/ue_wrap/desk/coords_panel.cpp"
#include "../../src/votv-coop/src/ue_wrap/desk/coords_visual.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace CP=ue_wrap::coords_panel;
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;};
Obj cls{1},fn{2},scalar{3},material[8];
struct Widget:Obj {float view[2]{},c0[2]{},c1[2]{},c2[2]{};int selected=0;uint8_t direction=0,canPing=0;
 ue_wrap::field_io::TArrayView edges{},lines{};void* circle=nullptr;void* triangle=nullptr;
} widget,other;
struct Desk:Obj {float outer=0,inner=0,ping=0;bool active=false;int stage=7;} desk;
void* refs[6];Obj* objects[32]{};void* shown=&widget;
uint32_t world=1;
int paints=0,alphas=0,texts=0,failAt=0;bool callOk=true,textOk=true,paramsOk=true;
std::function<void()> callback;
void Callback(){auto cb=std::move(callback);callback={};if(cb)cb();}
void Reset(){
 ++world;cls={1};fn={2};scalar={3};widget={};other={};desk={};
 widget.id=4;widget.cls=&cls;other.id=5;other.cls=&cls;desk.id=6;desk.cls=&cls;
 for(int i=0;i<8;++i){material[i]={10+i};material[i].cls=&cls;objects[10+i]=&material[i];if(i<6)refs[i]=&material[i];}
 objects[1]=&cls;objects[2]=&fn;objects[3]=&scalar;objects[4]=&widget;objects[5]=&other;objects[6]=&desk;
 widget.edges={reinterpret_cast<uint8_t*>(refs),3,3};widget.lines={reinterpret_cast<uint8_t*>(refs+3),3,3};
 widget.circle=&material[6];widget.triangle=&material[7];other.edges=widget.edges;other.lines=widget.lines;other.circle=widget.circle;other.triangle=widget.triangle;
 shown=&widget;paints=alphas=texts=failAt=0;callOk=textOk=paramsOk=true;callback={};
 CP::g_required=false;CP::g_nextResolve={};CP::g_uiCoordsClassIdentity.Reset();
}
}
namespace ue_wrap::reflection {
void* ClassOf(void* a){return a?static_cast<test::Obj*>(a)->cls:nullptr;}
bool IsLive(void* a){return a && static_cast<test::Obj*>(a)->live;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i>0 && i<32 && test::objects[i]?test::objects[i]->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* a,int32_t i){return a && IsLive(a) && InternalIndexOf(a)==i;}
void* ObjectAt(int32_t i){return i>0 && i<32?test::objects[i]:nullptr;}
const FName& NameOf(void*){static FName n{1,0};return n;}
bool NameEquals(const FName&,const wchar_t* s){return std::wstring(s)==L"ui_coordinates_C";}
void* SuperStructOf(void*){return nullptr;}
void* FindClass(const wchar_t*){return &test::cls;}
void* FindFunction(void*,const wchar_t*){return &test::fn;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){return std::wstring(n)==L"SetScalarParameterValue"?&test::scalar:&test::fn;}
int32_t FindPropertyOffset(void*,const wchar_t* n){using W=test::Widget;std::wstring s=n;
 if(s==L"viewCoordinate")return offsetof(W,view);if(s==L"Coordinate_0")return offsetof(W,c0);
 if(s==L"Coordinate_1")return offsetof(W,c1);if(s==L"Coordinate_2")return offsetof(W,c2);
 if(s==L"selected")return offsetof(W,selected);if(s==L"Direction")return offsetof(W,direction);return -1;
}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){off=offsetof(test::Widget,canPing);mask=1;return true;}
int32_t InstanceOffset::Of(void*){using W=test::Widget;using D=test::Desk;std::wstring s=prop_;
 if(s==L"coord_ping_outer")return offsetof(D,outer);if(s==L"coord_ping_inner")return offsetof(D,inner);if(s==L"coord_ping_ping")return offsetof(D,ping);
 if(s==L"dynmat_linesEdges")return offsetof(W,edges);if(s==L"dynmat_linesInner")return offsetof(W,lines);if(s==L"dynmat_ping")return offsetof(W,circle);if(s==L"dynmat_triangle")return offsetof(W,triangle);return -1;
}
int32_t FunctionFrameSize(void* f){return f==&test::scalar?12:0;}
std::vector<ParamInfo> FunctionParams(void*){return test::paramsOk?std::vector<ParamInfo>{{L"ParameterName",0,8,0},{L"Value",8,4,0}}:std::vector<ParamInfo>{{L"Value",8,4,0}};}
bool CallFunction(void*,void* f,void* params){
 if(f==&test::scalar){++test::alphas;float v;std::memcpy(&v,static_cast<uint8_t*>(params)+8,4);CHECK(v>=0 && v<=1);}
 else ++test::paints;
 test::Callback();return test::callOk && test::alphas!=test::failAt;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* WorldOf(void* a){return a==&test::desk?reinterpret_cast<void*>(uintptr_t(test::world)):nullptr;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}}
namespace ue_wrap::console_desk {
bool EnsureResolved(){return true;}void* Instance(){return &test::desk;}void* AtlasUiCoordsSlot(){return test::shown;}
bool CallUpdateCoordCoords(){++test::texts;return test::textOk;}
}
namespace ue_wrap::desk_ping {bool ReadPinging(void*,bool& out){out=test::desk.active;return true;}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){return {9,0};}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){using namespace test;
 Reset();CHECK(CP::Instance()==&widget && CP::PresentationTarget()==widget.triangle);shown=&other;CHECK(CP::Instance()==&other);shown=&widget;
 CP::DishAim a;a.c0X=20;a.c1Y=40;a.c2X=80;a.selected=2;widget.view[0]=999;
 failAt=-1;CHECK(CP::WriteDishCommitted(a));CHECK(paints==1 && texts==1 && widget.c0[0]==20 && widget.view[0]==999);
 CP::PingVisual v{.25f,.5f,.75f,true};CHECK(CP::WritePingVisual(v));CHECK(alphas==7 && desk.outer==.25f && desk.inner==.5f && desk.ping==.75f && !desk.active && desk.stage==7);
 CP::PingVisual read;CHECK(CP::ReadPingVisual(read) && read.outer==.25f && !read.active);
 Reset();desk.active=true;CHECK(!CP::WritePingVisual(v) && alphas==0 && desk.outer==0);
 for(int invalid=0;invalid<5;++invalid){Reset();
  if(invalid==0)widget.edges.num=2;if(invalid==1)widget.lines.max=2;if(invalid==2)widget.circle=nullptr;
  if(invalid==3)refs[2]=nullptr;if(invalid==4)widget.triangle=nullptr;
  CHECK(!CP::PresentationTarget());CHECK(!CP::WriteDishCommitted(a) && paints==0);
 }
 Reset();failAt=-1;callOk=false;CHECK(!CP::WriteDishCommitted(a) && texts==0);
 Reset();failAt=-1;textOk=false;CHECK(!CP::WriteDishCommitted(a));
 Reset();failAt=3;CHECK(!CP::WritePingVisual(v) && alphas==3);
 Reset();paramsOk=false;++scalar.serial;CHECK(!CP::WritePingVisual(v) && alphas==0);
 for(int change=0;change<4;++change){Reset();failAt=-1;callback=[change]{if(change==0)++world;if(change==1)++widget.serial;if(change==2)shown=&other;if(change==3)widget.live=false;};
  CHECK(!CP::WriteDishCommitted(a) && texts==0);}
 for(int change=0;change<4;++change){Reset();failAt=-1;callback=[change]{if(change==0)++world;if(change==1)++desk.serial;if(change==2)shown=&other;if(change==3)material[1].live=false;};
  CHECK(!CP::WritePingVisual(v) && alphas==1);}
 Reset();CHECK(CP::Instance()==&widget);++cls.serial;CP::g_offCoordinate0=-1;CHECK(CP::Instance()==&widget && CP::g_offCoordinate0>=0);
 puts("PASS: actual coordinate wrappers + ParamFrame, live widget/class, readiness, geometry, seven alpha materials, active-FSM exclusion, failure and callback lifetime");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
}
