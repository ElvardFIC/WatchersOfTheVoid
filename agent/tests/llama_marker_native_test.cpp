#include "../../src/votv-coop/src/ue_wrap/actors/llama_marker.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/placed_save_record.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL marker native %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace M=ue_wrap::llama_marker;namespace S=ue_wrap::save_record;namespace R=ue_wrap::reflection;
enum Id {Actor,Other,MarkerClass=50,SaveClass,ForeignClass,SaveType,EngineClass,GetKey=100,GetData,LoadData,GetTransform,SetTransform,Count=128};
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;void* super=nullptr;void* outer=nullptr;R::FName name{};
    uintptr_t world=0;std::wstring key=L"None";ue_wrap::FTransform transform{};} objects[Count];
Obj& O(void* p){CHECK(p);return *static_cast<Obj*>(p);}void* At(int id){return &objects[id];}
uintptr_t world=100;uint32_t generation=1;std::map<std::wstring,void*> keys;std::wstring generated=L"native_marker_key";
int fail=-1,missing=-1,wrongSize=-1,beginCount=0,finishCount=0,destroyCount=0,keyCalls=0,dataCalls=0,loadCalls=0,transformCalls=0;
bool fields=true,registry=true,loadAccepted=true,finishOK=true,destroyOK=true,setAccepted=true,writeOK=true,wrongLoadKey=false,extraRecord=false;
std::wstring badParam;void* beginResult=nullptr;std::function<void(int)> onCall;std::function<void()> onBegin,onFinish;
S::SaveRecord nativeRecord,written;
void Fire(int fn){auto action=std::move(onCall);onCall={};if(action)action(fn);}
void Fire(std::function<void()>& f){auto action=std::move(f);f={};if(action)action();}
S::SaveRecord Record(const wchar_t* key=L"marker_key"){S::SaveRecord r;r.className=L"llamaSoul_C";r.key=key;return r;}
void Reset(){++world;++generation;for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(generation*1000)+i;objects[i].cls=At(i);objects[i].name.ComparisonIndex=i;}
    objects[Actor].cls=At(MarkerClass);objects[Other].cls=At(ForeignClass);objects[Actor].world=objects[Other].world=world;objects[MarkerClass].super=At(SaveClass);
    for(int i=GetKey;i<=SetTransform;++i)objects[i].outer=At(i<=LoadData?SaveClass:EngineClass);
    keys.clear();generated=L"native_marker_key";fail=missing=wrongSize=-1;beginCount=finishCount=destroyCount=keyCalls=dataCalls=loadCalls=transformCalls=0;
    fields=registry=loadAccepted=finishOK=destroyOK=setAccepted=writeOK=true;wrongLoadKey=extraRecord=false;badParam=L"";beginResult=At(Actor);onCall={};onBegin=onFinish={};
    nativeRecord=written={};M::g_cleanup.reset();}
std::vector<R::ParamInfo> Params(int fn){std::vector<R::ParamInfo> fields;
    if(fn==GetKey)fields={{L"key",0,8,0}};
    if(fn==GetData)fields={{L"data",0,256,0}};
    if(fn==LoadData)fields={{L"data",0,256,0},{L"return",256,1,0}};
    if(fn==GetTransform)fields={{L"ReturnValue",0,48,0}};
    if(fn==SetTransform)fields={{L"NewTransform",0,48,0},{L"bSweep",48,1,0},{L"SweepHitResult",56,136,0},{L"bTeleport",192,1,0},{L"ReturnValue",193,1,0}};
    for(auto it=fields.begin();it!=fields.end();)if(it->name==badParam){if(wrongSize<0)it=fields.erase(it);else{it->size=wrongSize;++it;}}else ++it;
    return fields;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}
void* ObjectAt(int32_t id){return id>=0&&id<test::Count?test::At(id):nullptr;}
int32_t SlotSerial(int32_t id){return id>=0&&id<test::Count?test::objects[id].serial:0;}int32_t AllocateSlotSerial(int32_t id){return SlotSerial(id);}
bool IsLiveByIndex(void* p,int32_t id){return p&&id>=0&&id<test::Count&&p==test::At(id)&&IsLive(p);}
void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}void* SuperStructOf(void* p){return test::O(p).super;}void* OuterOf(void* p){return test::O(p).outer;}
const FName& NameOf(void* p){return test::O(p).name;}
bool NameEquals(const FName& name,const wchar_t* text){const int id=name.ComparisonIndex;
    return (id==test::MarkerClass&&!wcscmp(text,L"llamaSoul_C"))||(id==test::SaveClass&&!wcscmp(text,L"actor_save_C"))||(id==test::SaveType&&!wcscmp(text,L"struct_save"))||
        (id==test::GetKey&&!wcscmp(text,L"getKey"))||(id==test::GetData&&!wcscmp(text,L"getData"))||(id==test::LoadData&&!wcscmp(text,L"loadData"))||
        (id==test::GetTransform&&!wcscmp(text,L"GetTransform"))||(id==test::SetTransform&&!wcscmp(text,L"K2_SetActorTransform"));}
std::wstring ToString(const FName&){return L"marker_fixture";}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& kind){if(!test::fields||wcscmp(name,L"key"))return false;offset=offsetof(test::Obj,key);kind=L"NameProperty";return true;}
int32_t StructSize(void* p){return p==test::At(test::SaveType)?248:sizeof(test::Obj);}
void* PropertyInnerStruct(void*,const wchar_t* name){return !wcscmp(name,L"data")?test::At(test::SaveType):nullptr;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){const wchar_t* names[]={L"getKey",L"getData",L"loadData",L"GetTransform",L"K2_SetActorTransform"};
    for(int i=0;i<5;++i)if(!wcscmp(names[i],name))return test::missing==test::GetKey+i?nullptr:test::At(test::GetKey+i);return nullptr;}
int32_t FunctionFrameSize(void*){return 512;}std::vector<ParamInfo> FunctionParams(void* fn){return test::Params(test::O(fn).id);}
bool CallFunction(void* p,void* function,void* raw){using namespace test;const int fn=O(function).id;auto& actor=O(p);auto* bytes=static_cast<uint8_t*>(raw);const bool result=fn!=fail;
    if(fn==GetKey){++keyCalls;if(actor.key.empty()||actor.key==L"None")actor.key=generated;keys[actor.key]=p;}
    else if(fn==GetData){++dataCalls;nativeRecord=Record(actor.key.c_str());const auto& t=actor.transform;
        nativeRecord.xform={t.RotX,t.RotY,t.RotZ,t.RotW,t.TX,t.TY,t.TZ,t.SX,t.SY,t.SZ};if(extraRecord)nativeRecord.ints={{1}};}
    else if(fn==LoadData){++loadCalls;actor.key=wrongLoadKey?L"wrong_key":written.key;bytes[256]=loadAccepted;}
    else if(fn==GetTransform)std::memcpy(bytes,&actor.transform,48);
    else if(fn==SetTransform){++transformCalls;CHECK(!bytes[48]&&bytes[192]);if(setAccepted)std::memcpy(&actor.transform,bytes,48);bytes[193]=setAccepted;}
    else CHECK(false);Fire(fn);return result;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}void* WorldOf(void* p){return p?reinterpret_cast<void*>(test::O(p).world):nullptr;}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* name){return !wcscmp(name,L"llamaSoul_C")?test::At(test::MarkerClass):nullptr;}}
namespace ue_wrap::keyed_objects {bool Available(){return test::registry;}void* Resolve(const wchar_t* key){auto it=test::keys.find(key);return it!=test::keys.end()&&reflection::IsLive(it->second)?it->second:nullptr;}}
namespace ue_wrap::save_record {
std::wstring ReadFNameAt(const void* base,int32_t offset){return *reinterpret_cast<const std::wstring*>(static_cast<const uint8_t*>(base)+offset);}
void WriteFNameField(void* dst,const std::wstring& key){*static_cast<std::wstring*>(dst)=key;}
void ReadSaveRecord(const void*,SaveRecord& out){out=test::nativeRecord;}
bool WriteSaveRecordChecked(uint8_t*,const SaveRecord& record,ClassResolver resolve){CHECK(resolve&&resolve(record.className.c_str())==test::At(test::MarkerClass));test::written=record;return test::writeOK;}
}
namespace ue_wrap::engine {
FRotator QuatToRotator(float,float,float,float){return {};}
void* BeginDeferredSpawn(void*,const FVector&,const FRotator&){++test::beginCount;test::Fire(test::onBegin);return test::beginResult;}
bool FinishDeferredSpawn(void*,const FVector&,const FRotator&){++test::finishCount;test::Fire(test::onFinish);return test::finishOK;}
bool DestroyActor(void* p){++test::destroyCount;if(test::destroyOK)test::O(p).live=false;return test::destroyOK;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace test {
void Group(int id){Reset();S::SaveRecord out=Record(L"untouched");auto record=Record();
    switch(id){
    case 1:CHECK(M::Is(At(Actor))&&!M::Is(At(Other)));CHECK(ue_wrap::placed_save::ClassKind(L"llamaSoul_C")==ue_wrap::placed_save::Kind::Marker);
        CHECK(ue_wrap::placed_save::ClassKind(L"llamaSoul_child_C")==ue_wrap::placed_save::Kind::None);break;
    case 2:objects[Actor].world=999;CHECK(!M::Capture(At(Actor),out)&&out.key==L"untouched"&&!keyCalls);break;
    case 3:objects[Actor].key=L"native_saved_key";CHECK(M::Capture(At(Actor),out)&&out.key==L"native_saved_key"&&keyCalls==1&&dataCalls==1);break;
    case 4:objects[Actor].key=record.key;keys[record.key]=At(Other);CHECK(!M::Capture(At(Actor),out)&&!keyCalls&&keys[record.key]==At(Other));
        CHECK(!M::SetKey(At(Actor),record.key)&&keys[record.key]==At(Other));break;
    case 5:objects[MarkerClass].super=nullptr;CHECK(!M::Is(At(Actor))&&!M::Capture(At(Actor),out)&&!keyCalls);break;
    case 6:for(const wchar_t* bad:{L"data",L"key"}){Reset();badParam=bad;CHECK(!M::Capture(At(Actor),out)&&!dataCalls);}
        Reset();objects[GetData].outer=At(ForeignClass);CHECK(!M::Capture(At(Actor),out)&&!dataCalls);
        Reset();fail=GetData;CHECK(!M::Capture(At(Actor),out)&&out.key==L"untouched");break;
    case 7:missing=LoadData;CHECK(!M::Apply(At(Actor),record)&&!loadCalls);Reset();badParam=L"return";wrongSize=4;CHECK(!M::Apply(At(Actor),record)&&!loadCalls);
        Reset();loadAccepted=false;CHECK(!M::Apply(At(Actor),record)&&loadCalls==1);break;
    case 8:CHECK(M::Normalize(record,out)&&out.className==L"llamaSoul_C");record.className=L"actor_save_C";CHECK(!M::Normalize(record,out));break;
    case 9:for(int extra=0;extra<11;++extra){record=Record();if(extra==0)record.bools={{1}};if(extra==1)record.floats={{1}};if(extra==2)record.ints={{1}};
        if(extra==3)record.strings={{L"extra"}};if(extra==4)record.signals.resize(1);if(extra==5)record.classes={{L"extra"}};if(extra==6)record.vectors={{{1,2,3}}};
        if(extra==7)record.rotators={{{1,2,3}}};if(extra==8)record.transforms.resize(1);if(extra==9)record.bytes={{1}};if(extra==10)record.names={{L"extra"}};CHECK(!M::Normalize(record,out));}break;
    case 10:for(int axis=0;axis<7;++axis){record=Record();record.xform[axis]=std::numeric_limits<float>::quiet_NaN();CHECK(!M::Normalize(record,out));}
        record=Record();record.xform[4]=2000001;CHECK(!M::Normalize(record,out));record=Record();record.xform[3]=0;CHECK(!M::Normalize(record,out));break;
    case 11:for(float scale:{0.f,-1.f,101.f,std::numeric_limits<float>::infinity()}){record=Record();record.xform[8]=scale;CHECK(!M::Normalize(record,out));}break;
    case 12:generated=L"None";CHECK(!M::Capture(At(Actor),out)&&out.key==L"untouched");Reset();generated=std::wstring(129,L'k');CHECK(!M::Capture(At(Actor),out));break;
    case 13:for(int cause=0;cause<5;++cause){Reset();onCall=[cause](int fn){CHECK(fn==GetKey);if(cause==0)++objects[Actor].serial;if(cause==1)++objects[MarkerClass].serial;
        if(cause==2)++world;if(cause==3)++objects[GetData].serial;if(cause==4)fields=false;};CHECK(!M::Capture(At(Actor),out)&&out.key==L"untouched"&&!dataCalls);}break;
    case 14:generated=std::wstring(128,L'k');CHECK(M::Capture(At(Actor),out)&&out.key==generated&&out.key.size()==128);break;
    case 15:beginResult=At(Other);CHECK(!M::Spawn(record)&&beginCount==1&&!finishCount&&!destroyCount&&objects[Other].live);break;
    case 16:keys[record.key]=At(Other);CHECK(!M::Apply(At(Actor),record)&&!loadCalls&&objects[Actor].key==L"None");break;
    case 17:beginResult=nullptr;CHECK(!M::Spawn(record)&&!finishCount&&!destroyCount);Reset();onFinish=[]{++world;};CHECK(!M::Spawn(record)&&!destroyCount);
        Reset();finishOK=false;destroyOK=false;CHECK(!M::Spawn(record)&&beginCount==1&&M::g_cleanup.has_value());CHECK(!M::Spawn(record)&&beginCount==1);destroyOK=true;M::OnDisconnect();CHECK(!M::g_cleanup);break;
    case 18:record.xform[4]=15;record.xform[8]=2;CHECK(M::Apply(At(Actor),record)&&transformCalls==1&&objects[Actor].transform.TX==15&&objects[Actor].transform.SY==2);
        Reset();record=Record();record.xform[4]=15;setAccepted=false;CHECK(!M::Apply(At(Actor),record)&&!loadCalls);break;
    case 19:CHECK(M::Apply(At(Actor),record)&&objects[Actor].key==record.key&&keys[record.key]==At(Actor));Reset();wrongLoadKey=true;CHECK(!M::Apply(At(Actor),record)&&!keyCalls);
        Reset();objects[Actor].key=record.key;keys[record.key]=At(Actor);CHECK(M::Spawn(record)==At(Actor)&&!beginCount);M::OnDisconnect();CHECK(objects[Actor].live&&!destroyCount);break;
    default:CHECK(false);}
}
}
int main(){for(int id=1;id<=19;++id){test::Group(id);std::printf("PASS llama lifecycle %d\n",id);}return 0;}
