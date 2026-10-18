#include "../../src/votv-coop/src/ue_wrap/devices/cremator.cpp"
#include <cassert>
#include <cwchar>
#include <cstdio>
namespace test {
struct Obj {int id,serial=1;bool live=true;void* cls=nullptr;void* timeline=nullptr;uint8_t bits=0;};
Obj machine{0},timeline{1},cls{2},doorClass{3};Obj* objects[]={&machine,&timeline,&cls,&doorClass};
bool recycle=false,missingDoor=true;float position=-1;int stops=0,scans=0;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return static_cast<test::Obj*>(p)->id;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<4&&p==test::objects[i]&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<4?test::objects[i]->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
bool IsDescendantOfAny(void* p,void* const* bases,size_t n,int){for(size_t i=0;i<n;++i)if(p==bases[i])return true;return false;}
int32_t FindPropertyOffset(void*,const wchar_t* n){return !wcscmp(n,L"handleLimeline")?offsetof(test::Obj,timeline):-1;}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,bits);mask=!wcscmp(n,L"isActive")?4:2;return true;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){return const_cast<wchar_t*>(n);}
void* FindFunction(void*,const wchar_t*){assert(false && "paint must not call gameplay verbs");return nullptr;}
const FName& NameOf(void*){static FName n{1,0};return n;}
bool NameStartsWith(const FName&,const wchar_t*){return false;}
int32_t NumObjects(){++test::scans;return 1;}
void* ObjectAt(int32_t){return &test::machine;}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n){return !wcscmp(n,L"cremator_C")?&test::cls:
 (test::missingDoor?nullptr:&test::doorClass);}
}
namespace coop::element {std::wstring PortableWireKey(void*){return L"cremator";}}
namespace ue_wrap::world_identity {
void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}uint32_t Generation(){return 1;}
}
namespace ue_wrap::component_calls {bool CallParamlessNamed(void*,const wchar_t* n){assert(!wcscmp(n,L"Stop"));++test::stops;return true;}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(128){}
bool ParamFrame::SetRaw(const wchar_t* n,const void* p,int32_t size){
 if(!wcscmp(n,L"NewPosition"))std::memcpy(buf_.data(),p,size);
 if(!wcscmp(n,L"bFireEvents"))assert(!*static_cast<const bool*>(p));
 if(!wcscmp(n,L"bFireUpdate"))assert(*static_cast<const bool*>(p));return valid();
}
bool ParamFrame::GetRaw(const wchar_t*,void* p,int32_t size)const{std::memcpy(p,buf_.data(),size);return true;}
bool Call(void*,ParamFrame& f){auto* n=static_cast<wchar_t*>(f.function());
 if(!wcscmp(n,L"GetTimelineLength")){float value=1;std::memcpy(f.data(),&value,sizeof(value));}
 else {assert(!wcscmp(n,L"SetPlaybackPosition"));std::memcpy(&test::position,f.data(),sizeof(float));
  if(test::recycle)++test::machine.serial;}
 return true;
}
}
int main(){using namespace test;namespace C=ue_wrap::cremator;
 machine.cls=timeline.cls=&cls;machine.timeline=&timeline;machine.bits=3;
 assert(C::IsCremator(&machine)); // Missing door class must not disable the lever.
 assert(C::ApplyLeverState(&machine,true));assert(machine.bits==5&&position==1&&stops==1);
 assert(C::ApplyLeverState(&machine,false));assert(machine.bits==1&&position==0);
 recycle=true;assert(!C::ApplyLeverState(&machine,true));assert(machine.bits==1);
 recycle=false;machine.timeline=nullptr;assert(!C::ApplyLeverState(&machine,true));
 C::Instances();const int first=scans;C::Instances();assert(scans==first); // Cached census, no per-door scans.
 puts("PASS cremator paint-only lever endpoints, masked flags, missing door class, stale actor and cached census");
}
