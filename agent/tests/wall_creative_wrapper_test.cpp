#include "../../src/votv-coop/src/ue_wrap/actors/wall_creative.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id {Wall,Tool,ToolClass,Function,Library,AllowedFn,BuildFn,Foreign,Count};
struct Obj {int index=0,serial=1;bool live=true;} objects[Count];
uint32_t world=1;bool outline=true,allowed=true,callOK=true,fields=true,library=true,retire=false;
int builds=0;
std::function<void()> callback;
struct Frame {void* context=nullptr;bool cheat=false;};
std::map<const ue_wrap::ParamFrame*,Frame> frames;
void* At(Id id){return &objects[id];}
}
namespace ue_wrap::reflection {
bool IsLive(void* object){return object && static_cast<test::Obj*>(object)->live;}
int32_t InternalIndexOf(void* object){return object?static_cast<test::Obj*>(object)->index:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* object,int32_t i){return i>=0 && i<test::Count && object==test::At(test::Id(i)) && IsLive(object);}
void* ClassOf(void* object){return object==test::At(test::Tool)?test::At(test::ToolClass):object;}
const FName& NameOf(void* object){static FName names[test::Count]{};const auto id=InternalIndexOf(object);names[id].ComparisonIndex=id;return names[id];}
bool NameEquals(const FName& name,const wchar_t* value){return (name.ComparisonIndex==test::Function && !wcscmp(value,L"addWall")) ||
    (name.ComparisonIndex==test::ToolClass && !wcscmp(value,L"prop_wallbuilder_C")) ||
    (name.ComparisonIndex==test::Library && !wcscmp(value,L"Default__lib_C"));}
void* OuterOf(void* object){return object==test::At(test::Function)?test::At(test::ToolClass):test::At(test::Foreign);}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){return count==1 && cls==bases[0];}
void* FindClassDefaultObject(const wchar_t* name){CHECK(!wcscmp(name,L"lib_C"));return test::library?test::At(test::Library):nullptr;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){if(!test::fields)return nullptr;
    if(!wcscmp(name,L"isCustom"))return test::At(test::AllowedFn);CHECK(!wcscmp(name,L"buildWall"));return test::At(test::BuildFn);}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){CHECK(!wcscmp(name,L"lib_C"));return test::library?test::At(test::Library):nullptr;}
size_t ForEachInstance(void*,InstanceFn fn,void* context){if(test::library)fn(context,test::At(test::Library),test::Library);return test::library?1:0;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::placed_save {Kind Type(void* actor){return actor==test::At(test::Wall) && test::outline?Kind::WallOutline:Kind::Wall;}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){test::frames[this]={};}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t size){
    if(!wcscmp(name,L"__WorldContext")){CHECK(size==sizeof(void*));test::frames[this].context=*static_cast<void* const*>(value);}
    else {CHECK(!wcscmp(name,L"isCheat") && size==sizeof(bool));test::frames[this].cheat=*static_cast<const bool*>(value);}return true;
}
bool ParamFrame::GetRaw(const wchar_t* name,void* value,int32_t size)const {CHECK(!wcscmp(name,L"isCustom") && size==sizeof(bool));*static_cast<bool*>(value)=test::allowed;return true;}
bool Call(void* object,ParamFrame& frame){
    if(frame.function()==test::At(test::AllowedFn)){CHECK(object==test::At(test::Library) && test::frames[&frame].context==test::At(test::Wall));}
    else {CHECK(object==test::At(test::Wall) && test::frames[&frame].cheat);++test::builds;if(test::retire)test::objects[test::Wall].live=false;}
    auto callback=std::move(test::callback);test::callback={};if(callback)callback();return test::callOK;
}
}
int main(){
    using namespace test;namespace W=ue_wrap::wall_creative;
    for(int i=0;i<Count;++i)objects[i].index=i;
    CHECK(W::IsBuilderCall(At(Tool),At(Function)));CHECK(!W::IsBuilderCall(At(Foreign),At(Function)));
    CHECK(!W::IsBuilderCall(At(Tool),At(Foreign)));bool enabled=false;
    CHECK(W::Allowed(At(Wall),enabled) && enabled);allowed=false;CHECK(W::Allowed(At(Wall),enabled) && !enabled);
    callOK=false;enabled=true;CHECK(!W::Allowed(At(Wall),enabled) && enabled);callOK=true;
    fields=false;CHECK(!W::Allowed(At(Wall),enabled));fields=true;
    callback=[]{++world;};CHECK(!W::Allowed(At(Wall),enabled));
    auto current=[]{return true;};fields=false;CHECK(W::Finish(At(Wall),current)==W::Build::Retry && builds==0);fields=true;
    CHECK(W::Finish(At(Wall),[]{return false;})==W::Build::Retry && builds==0);
    retire=true;CHECK(W::Finish(At(Wall),current)==W::Build::Dispatched && builds==1 && !objects[Wall].live);
    objects[Wall].live=true;retire=false;callOK=false;CHECK(W::Finish(At(Wall),current)==W::Build::Failed && builds==2);
    puts("PASS creative wrapper: exact addWall owner/source, native custom-content query, actual isCheat build argument, missing metadata retry, native retirement/fault and world/current fences");
}
