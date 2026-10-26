#include "../../src/votv-coop/src/ue_wrap/actors/llama.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id{Actor,Audio,Light,Visibility,Activate,Graph,OtherFunction,Other,Class=20,AudioClass,LightClass,ForeignClass,Count=64};
struct Obj{int id=0,serial=1;bool live=true,visible=false;uint8_t flags=0xA4;void* cls=nullptr;void* parent=nullptr;void* audio=nullptr;void* light=nullptr;
    ue_wrap::reflection::FName name{};} objects[Count];
Obj& O(void* p){CHECK(p);return *static_cast<Obj*>(p);}void* At(Id i){return &objects[i];}
uint32_t generation=1;uintptr_t world=100;bool nativeOk=true;int shows=0,cues=0,entryOffset=0,entryBytes=4,graphSize=4;
uint8_t activeMask=1;void* visibility=nullptr;std::wstring badParam,fieldName;int badBytes=-1;
std::function<void()> onFrame,onField,onCall;std::vector<void*> instances;
void Fire(std::function<void()>& f){auto callback=std::move(f);f={};if(callback)callback();}
void Reset(){++generation;++world;for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(generation*1000)+i;objects[i].cls=&objects[i];objects[i].name.ComparisonIndex=i;}
    objects[Actor].cls=At(Class);objects[Audio].cls=At(AudioClass);objects[Light].cls=At(LightClass);objects[Other].cls=At(ForeignClass);
    objects[Actor].audio=At(Audio);objects[Actor].light=At(Light);nativeOk=true;shows=cues=0;entryOffset=0;entryBytes=graphSize=4;
    activeMask=1;visibility=At(Visibility);badParam=fieldName=L"";badBytes=-1;onFrame=onField=onCall={};instances={At(Actor),At(Actor),At(Other)};}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}int32_t SlotSerial(int32_t i){return i>=0&&i<test::Count?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}void* ObjectAt(int32_t i){return i>=0&&i<test::Count?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}void* SuperStructOf(void* p){return test::O(p).parent;}
const FName& NameOf(void* p){return test::O(p).name;}std::wstring ToString(const FName&){return L"llama_fixture";}
bool NameEquals(const FName& n,const wchar_t* name){return (n.ComparisonIndex==test::Class&&!wcscmp(name,L"prop_llama_C"))||
    (n.ComparisonIndex==test::AudioClass&&!wcscmp(name,L"AudioComponent"))||(n.ComparisonIndex==test::LightClass&&!wcscmp(name,L"PointLightComponent"));}
int32_t FindPropertyOffset(void*,const wchar_t* name){int offset=!wcscmp(name,L"Audio")?offsetof(test::Obj,audio):!wcscmp(name,L"PointLight")?offsetof(test::Obj,light):-1;
    if(test::fieldName==name)test::Fire(test::onField);return offset;}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& offset,uint8_t& mask){offset=offsetof(test::Obj,flags);mask=!wcscmp(name,L"h")?test::activeMask:4;
    CHECK(!wcscmp(name,L"h")||!wcscmp(name,L"yumpskare"));if(test::fieldName==name)test::Fire(test::onField);return true;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return !wcscmp(name,L"SetVisibility")?test::visibility:!wcscmp(name,L"Activate")?test::At(test::Activate):nullptr;}
int32_t FindParamOffset(void*,const wchar_t* name){CHECK(!wcscmp(name,L"EntryPoint"));return test::entryOffset;}
int32_t FunctionFrameSize(void* p){test::Fire(test::onFrame);return p==test::At(test::Graph)?test::graphSize:p==test::At(test::Activate)?1:2;}
std::vector<ParamInfo> FunctionParams(void* p){std::vector<ParamInfo> params=p==test::At(test::Graph)?std::vector<ParamInfo>{{L"EntryPoint",test::entryOffset,test::entryBytes,0}}:
    p==test::At(test::Activate)?std::vector<ParamInfo>{{L"bReset",0,1,0}}:std::vector<ParamInfo>{{L"bNewVisibility",0,1,0},{L"bPropagateToChildren",1,1,0}};
    for(auto it=params.begin();it!=params.end();)if(it->name==test::badParam){if(test::badBytes<0)it=params.erase(it);else{it->size=test::badBytes;++it;}}else ++it;return params;}
bool CallFunction(void* object,void* function,void* raw){auto* bytes=static_cast<uint8_t*>(raw);
    const bool result=test::nativeOk;
    if(function==test::At(test::Activate)){CHECK(object==test::At(test::Audio)&&!bytes[0]);++test::cues;}
    else {CHECK(function==test::visibility&&object==test::At(test::Light)&&!bytes[1]);++test::shows;test::O(object).visible=bytes[0]!=0;}
    test::Fire(test::onCall);return result;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* name){return !wcscmp(name,L"prop_llama_C")?test::At(test::Class):nullptr;}
size_t ForEachInstance(void*,InstanceFn fn,void* context){for(void* p:test::instances)fn(context,p,test::O(p).id);return test::instances.size();}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace test {
namespace M=ue_wrap::llama;
void Entry(){Reset();int32_t input=1146,out=99;CHECK(M::Entry(At(Graph),reinterpret_cast<uint8_t*>(&input),out)&&out==1146);
    for(int cause=0;cause<5;++cause){Reset();out=99;if(cause==0)entryOffset=2;if(cause==1)graphSize=3;if(cause==2)entryBytes=1;
        if(cause==3)onFrame=[]{++objects[Graph].serial;};if(cause==4)graphSize=70000;
        CHECK(!M::Entry(At(Graph),reinterpret_cast<uint8_t*>(&input),out)&&out==99);}}
void Once(){Reset();bool dispatched=false;onCall=[]{++world;};CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&!dispatched&&!cues);
    Reset();int calls=0;onCall=[&]{++calls;onCall=[]{++world;};};CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&dispatched&&cues==1);
    Reset();nativeOk=false;CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&!dispatched&&!cues);
    Reset();onCall=[]{nativeOk=false;};CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&dispatched&&cues==1);}
void State(){Reset();bool dispatched=false;CHECK(M::Apply(At(Actor),true,false,dispatched,[]{return true;})&&objects[Light].visible&&!(objects[Actor].flags&1)&&!cues);
    CHECK(M::Apply(At(Actor),false,false,dispatched,[]{return true;})&&!objects[Light].visible&&!(objects[Actor].flags&1));
    Reset();CHECK(M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&dispatched&&cues==1);}
void Bits(){Reset();bool original=false;CHECK(M::ParkBegin(At(Actor),original,[]{return true;})&&original&&objects[Actor].flags==0xA0);
    CHECK(M::RestoreBegin(At(Actor),original,[]{return true;})&&objects[Actor].flags==0xA4);
    CHECK(M::ParkBegin(At(Actor),original,[]{return true;}));bool dispatched=false;
    CHECK(M::Apply(At(Actor),true,false,dispatched,[]{return true;})&&objects[Actor].flags==0xA0);
    fieldName=L"h";onField=[]{activeMask=2;};bool out=false;CHECK(!M::Read(At(Actor),out)&&!out);}
void Types(){for(bool light:{false,true}){Reset();objects[light?Light:Audio].cls=At(ForeignClass);bool dispatched=false;
    CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&!cues);}
    Reset();objects[Actor].audio=nullptr;bool dispatched=false;CHECK(M::Apply(At(Actor),true,false,dispatched,[]{return true;}));}
void Functions(){for(const wchar_t* name:{L"bNewVisibility",L"bPropagateToChildren",L"bReset"})for(bool size:{false,true}){
    Reset();badParam=name;if(size)badBytes=4;bool dispatched=false;CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&!cues&&!dispatched);}
    Reset();onFrame=[]{++objects[Visibility].serial;};bool dispatched=false;CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&!shows);
    Reset();onFrame=[]{visibility=At(OtherFunction);};CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&!shows);}
void Binding(){for(int cause=0;cause<5;++cause){Reset();onCall=[cause]{if(cause==0)++objects[Actor].serial;if(cause==1)++objects[LightClass].serial;
    if(cause==2)objects[Actor].audio=At(Other);if(cause==3)++generation;if(cause==4)objects[Actor].light=At(Other);};bool dispatched=false;
    CHECK(!M::Apply(At(Actor),true,true,dispatched,[]{return true;})&&!cues);}}
void Reads(){Reset();bool out=true;fieldName=L"h";onField=[]{++objects[Class].serial;};CHECK(!M::Read(At(Actor),out)&&out);
    Reset();objects[Actor].flags|=1;CHECK(M::Read(At(Actor),out)&&out);
    ue_wrap::CachedObjRef refs[2];refs[1].Set(At(Other));CHECK(!M::Collect(nullptr,2)&&!M::Collect(refs,0));CHECK(M::Collect(refs,1)==1&&refs[1].Is(At(Other)));}
}
int main(){using namespace test;struct Group{int id;void(*run)();};Group groups[]={{5,Entry},{7,Once},{8,State},{9,Bits},{10,Types},{11,Functions},{12,Binding},{13,Reads}};
    for(const auto& g:groups){g.run();std::printf("PASS llama %d\n",g.id);}return 0;}
