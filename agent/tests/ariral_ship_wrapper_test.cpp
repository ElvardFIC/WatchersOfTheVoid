#include "../../src/votv-coop/src/ue_wrap/world/ariral_ship.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {
    int id=0,serial=1;bool live=true;uint8_t bits=2;float alpha=.5f;
    void *mesh=nullptr,*light=nullptr,*radar=nullptr,*air=nullptr,*timeline=nullptr,*gamemode=nullptr;
    ue_wrap::reflection::FName name{};std::wstring label;std::vector<std::wstring> params;
} objs[100];
uint32_t world=1;bool fields=true,names=true,callOK=true,current=true;
float scalar=-1,intensity=-1;int stops=0,triggers=0,eventCalls=0,emitters=0;
std::vector<void*> senders;std::function<void()> callback,nameCallback;
void Callback(std::function<void()>& source){auto fn=std::move(source);source={};if(fn)fn();}
int Offset(int fn,const wchar_t* name){auto& p=objs[fn].params;for(size_t i=0;i<p.size();++i)if(p[i]==name)return int(i*16);return -1;}
template<class T>T Read(void* frame,int fn,const wchar_t* name){T out{};int off=Offset(fn,name);CHECK(off>=0);std::memcpy(&out,static_cast<uint8_t*>(frame)+off,sizeof(T));return out;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return static_cast<test::Obj*>(p)->id<7?CurrentWorld():nullptr;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
void* ObjectAt(int32_t i){return i>=0&&i<100?&test::objs[i]:nullptr;}
int32_t SlotSerial(int32_t i){return i>=0&&i<100?test::objs[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}
void* ClassOf(void* p){return &test::objs[p==&test::objs[0]?1:90];}
const FName& NameOf(void* p){return static_cast<test::Obj*>(p)->name;}
bool NameEquals(const FName& n,const wchar_t* text){return test::objs[n.ComparisonIndex].label==text;}
std::wstring ToString(const FName& n){return test::objs[n.ComparisonIndex].label;}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,bits);mask=4;return test::fields;}
int32_t FindPropertyOffset(void*,const wchar_t* name) {
    if(!test::fields)return -1;
    if(!wcscmp(name,L"StaticMesh"))return offsetof(test::Obj,mesh);
    if(!wcscmp(name,L"PointLight"))return offsetof(test::Obj,light);
    if(!wcscmp(name,L"radarPoint"))return offsetof(test::Obj,radar);
    if(!wcscmp(name,L"air"))return offsetof(test::Obj,air);
    if(!wcscmp(name,L"a"))return offsetof(test::Obj,timeline);
    if(!wcscmp(name,L"gamemode"))return offsetof(test::Obj,gamemode);
    if(!wcscmp(name,L"a_a_2C6617ED4975EC563C73D5B0DBE0638B"))return offsetof(test::Obj,alpha);
    return -1;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name){for(int i=20;i<=25;++i)if(test::objs[i].label==name)return &test::objs[i];return nullptr;}
void* FindClassDefaultObject(const wchar_t* cls){return &test::objs[!wcscmp(cls,L"lib_C")?9:10];}
int32_t FunctionFrameSize(void* p){return int(static_cast<test::Obj*>(p)->params.size()*16);}
std::vector<ParamInfo> FunctionParams(void* p){std::vector<ParamInfo> out;auto& names=static_cast<test::Obj*>(p)->params;for(size_t i=0;i<names.size();++i){ParamInfo param{};param.name=names[i];param.offset=int(i*16);out.push_back(param);}return out;}
bool CallFunction(void* actor,void* function,void* frame) {
    using namespace test;const int fn=static_cast<Obj*>(function)->id;
    if(!callOK)return false;
    if(fn==20){CHECK(actor==&objs[2]&&Read<FName>(frame,fn,L"ParameterName").ComparisonIndex==77);scalar=Read<float>(frame,fn,L"ParameterValue");}
    if(fn==21){CHECK(actor==&objs[3]);intensity=Read<float>(frame,fn,L"NewIntensity");}
    if(fn==22){CHECK(actor==&objs[5]);++stops;}
    if(fn==23){CHECK(actor==&objs[0]&&!Read<void*>(frame,fn,L"owner"));int index=Read<int32_t>(frame,fn,L"index");CHECK(index==0||index==1);triggers+=index==0?1:10;}
    if(fn==24) {
        CHECK(actor==&objs[9]&&Read<void*>(frame,fn,L"__WorldContext")==&objs[0]&&!Read<bool>(frame,fn,L"deactivateAmbientTrack"));
        ++eventCalls;bool active=Read<bool>(frame,fn,L"isEventActive");
        if(active)senders.push_back(&objs[0]);else senders.erase(std::remove(senders.begin(),senders.end(),&objs[0]),senders.end());
    }
    if(fn==25) {
        CHECK(actor==&objs[10]&&Read<void*>(frame,fn,L"EmitterTemplate")==&objs[7]&&Read<void*>(frame,fn,L"AttachToComponent")==&objs[6]);
        CHECK(!Read<FName>(frame,fn,L"AttachPointName").ComparisonIndex);
        auto location=Read<FVector>(frame,fn,L"Location"),scale=Read<FVector>(frame,fn,L"Scale");auto rotation=Read<FRotator>(frame,fn,L"Rotation");
        CHECK(location.X==0&&location.Y==0&&location.Z==0&&rotation.Pitch==0&&rotation.Yaw==0&&rotation.Roll==0&&scale.X==1&&scale.Y==1&&scale.Z==1);
        CHECK(!Read<uint8_t>(frame,fn,L"LocationType")&&!Read<uint8_t>(frame,fn,L"PoolingMethod")&&Read<bool>(frame,fn,L"bAutoDestroy")&&Read<bool>(frame,fn,L"bAutoActivateSystem"));
        ++emitters;void* result=&objs[12];std::memcpy(static_cast<uint8_t*>(frame)+Offset(fn,L"ReturnValue"),&result,sizeof(result));
    }
    Callback(callback);return true;
}
}
namespace ue_wrap::engine {bool TryGetActorLocation(void*,FVector& out){out={100,200,300};return true;}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring& s){CHECK(s==L"alpha");test::Callback(test::nameCallback);return {test::names?77:0,0};}}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* cls){return &test::objs[!wcscmp(cls,L"ParticleSystem")?8:1];}
size_t ForEachInstance(void* cls,InstanceFn fn,void* ctx){int i=cls==&test::objs[8]?7:0;fn(ctx,&test::objs[i],i);return 1;}
}
namespace ue_wrap::active_events {bool ReadSenders(Senders& out){out={test::senders.data(),int32_t(test::senders.size())};return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main() {
    using namespace test;namespace A=ue_wrap::ariral_ship;
    for(int i=0;i<100;++i){objs[i].id=i;objs[i].name.ComparisonIndex=i;}
    objs[0].label=L"arirShipAppear_C_0";objs[1].label=L"arirShipAppear_C";objs[7].label=L"eff_arirShipAir";
    objs[0].mesh=&objs[2];objs[0].light=&objs[3];objs[0].radar=&objs[4];objs[0].timeline=&objs[5];objs[0].air=&objs[6];
    objs[0].gamemode=&objs[14];
    objs[20].label=L"SetScalarParameterValueOnMaterials";objs[20].params={L"ParameterName",L"ParameterValue"};
    objs[21].label=L"SetIntensity";objs[21].params={L"NewIntensity"};objs[22].label=L"Stop";
    objs[23].label=L"runTrigger";objs[23].params={L"owner",L"index"};
    objs[24].label=L"setEvent";objs[24].params={L"isEventActive",L"deactivateAmbientTrack",L"__WorldContext"};
    objs[25].label=L"SpawnEmitterAttached";objs[25].params={L"EmitterTemplate",L"AttachToComponent",L"AttachPointName",L"Location",L"Rotation",L"Scale",L"LocationType",L"bAutoDestroy",L"PoolingMethod",L"bAutoActivateSystem",L"ReturnValue"};
    const auto valid=[]{return current;};auto* a=&objs[0];A::Appearance state;
    CHECK(A::Actors().size()==1&&A::Read(a,state)&&state.name==a->label&&state.alpha==.5f);
    state.visible=true;CHECK(A::Paint(a,state,valid));CHECK(stops==1&&a->bits==6&&objs[4].bits==6&&scalar==.5f&&std::abs(intensity-97.65625f)<.001f);
    state.visible=false;state.alpha=0;CHECK(A::Paint(a,state,valid)&&a->bits==2&&objs[4].bits==2&&scalar==0&&intensity==0);
    names=false;state.alpha=1;CHECK(!A::Paint(a,state,valid)&&a->alpha==0);names=true;
    objs[5].live=false;CHECK(!A::Paint(a,state,valid)&&a->alpha==0);objs[5].live=true;
    callOK=false;CHECK(!A::Paint(a,state,valid)&&a->alpha==0);callOK=true;
    nameCallback=[]{current=false;};CHECK(!A::Paint(a,state,valid)&&a->alpha==0);current=true;
    callback=[]{current=false;};CHECK(!A::Paint(a,state,valid)&&a->alpha==0);current=true;
    nameCallback=[]{objs[0].mesh=&objs[13];};CHECK(!A::Paint(a,state,valid)&&a->alpha==0);objs[0].mesh=&objs[2];
    CHECK(A::PhotoSubscription(a,true,valid)&&A::PhotoSubscription(a,false,valid)&&triggers==11);
    a->gamemode=nullptr;CHECK(!A::PhotoSubscription(a,true,valid)&&triggers==11);a->gamemode=&objs[14];
    callback=[]{objs[0].gamemode=&objs[15];};CHECK(!A::PhotoSubscription(a,true,valid));a->gamemode=&objs[14];
    senders={&objs[11]};CHECK(A::Activity(a,true,valid)&&eventCalls==1&&senders.size()==2);
    CHECK(A::Activity(a,true,valid)&&eventCalls==1);CHECK(A::Activity(a,false,valid)&&eventCalls==2&&senders.size()==1&&senders[0]==&objs[11]);
    CHECK(A::Activity(a,false,valid)&&eventCalls==2);CHECK(A::Air(a,valid)&&emitters==1);
    current=false;CHECK(!A::Air(a,valid)&&!A::PhotoSubscription(a,true,valid)&&!A::Activity(a,true,valid));current=true;
    callback=[]{++world;};CHECK(!A::Paint(a,state,valid));
    ++objs[1].serial;fields=false;CHECK(!A::Read(a,state));
    puts("PASS real wrapper/ParamFrame: packed flags, stopped timeline, alpha/light formula, readiness/lifetime guards, native listener parameters, isolated event senders and air emitter defaults");
}
