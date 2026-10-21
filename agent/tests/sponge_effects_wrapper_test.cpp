#include "../../src/votv-coop/src/ue_wrap/actors/sponge_effects.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Index {Sponge,Player,Bubbles,SpongeClass,PlayerClass,ParticleClass,Particle,Package,Library,HitStruct,Spawned,Other,Count};
struct Hit {ue_wrap::FVector location{100,200,300},normal{0,0,1},impact{9,9,9},impactNormal{1,0,0};};
struct Obj {int index=0,serial=1;bool live=true;void* player=nullptr;void* effect=nullptr;Hit hit;float soap=100;} objects[Count];
uint32_t world=1;bool fields=true,stock=true,spawnOk=true,valid=true;
int surfaces=0,bursts=0;ue_wrap::FRotator rotation{};
std::function<void()> onLookup,onCall,onStock,onFunction;
std::map<std::wstring,std::wstring> functions;
using Bytes=std::vector<uint8_t>;std::map<const ue_wrap::ParamFrame*,std::map<std::wstring,Bytes>> frames;
void* At(Index index){return &objects[index];}
void Callback(std::function<void()>& value){auto f=std::move(value);value={};if(f)f();}
void* Fn(const wchar_t* name){auto& value=functions[name];value=name;return &value;}
template<class T> T Get(const ue_wrap::ParamFrame& f,const wchar_t* name){T out{};CHECK(f.GetRaw(name,&out,sizeof(out)));return out;}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t index){return index>=0 && index<test::Count && p==&test::objects[index] && IsLive(p);}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->index:-1;}
int32_t SlotSerial(int32_t index){return test::objects[index].serial;}
int32_t AllocateSlotSerial(int32_t index){return SlotSerial(index);}
void* ClassOf(void* p){if(p==test::At(test::Sponge))return test::At(test::SpongeClass);if(p==test::At(test::Player))return test::At(test::PlayerClass);return p;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){return count==1 && cls==bases[0];}
int32_t InstanceOffset::Of(void*){if(!test::fields)return -1;if(!wcscmp(prop_,L"player"))return offsetof(test::Obj,player);
    if(!wcscmp(prop_,L"hitResult"))return offsetof(test::Obj,hit);CHECK(!wcscmp(prop_,L"eff_bubbles"));return offsetof(test::Obj,effect);}
void* PropertyInnerStruct(void* cls,const wchar_t* name){CHECK(cls==test::At(test::PlayerClass) && !wcscmp(name,L"hitResult"));return test::At(test::HitStruct);}
int32_t StructSize(void* p){CHECK(p==test::At(test::HitStruct));return sizeof(test::Hit);}
int32_t FindPropertyOffset(void* p,const wchar_t* name){CHECK(p==test::At(test::HitStruct));auto result=!wcscmp(name,L"Location")?offsetof(test::Hit,location):offsetof(test::Hit,normal);
    CHECK(!wcscmp(name,L"Location") || !wcscmp(name,L"Normal"));test::Callback(test::onLookup);return int32_t(result);}
const FName& NameOf(void* p){static FName names[test::Count]{};const auto index=static_cast<test::Obj*>(p)->index;names[index].ComparisonIndex=index;return names[index];}
bool NameEquals(const FName& name,const wchar_t* text){return (name.ComparisonIndex==test::Particle && !wcscmp(text,L"eff_cleanBubbles")) ||
    (name.ComparisonIndex==test::Package && !wcscmp(text,L"/Game/particles/eff_cleanBubbles"));}
void* OuterOf(void* p){return p==test::At(test::Particle)?test::At(test::Package):nullptr;}
void* FindClassDefaultObject(const wchar_t* name){CHECK(!wcscmp(name,L"GameplayStatics"));return test::At(test::Library);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){test::Callback(test::onFunction);return test::Fn(name);}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){if(!wcscmp(name,L"prop_sponge_C"))return test::At(test::SpongeClass);CHECK(!wcscmp(name,L"ParticleSystem"));return test::At(test::ParticleClass);}
size_t ForEachInstance(void* cls,void(*fn)(void*,void*,int32_t),void* context){CHECK(cls==test::At(test::ParticleClass));fn(context,test::At(test::Particle),test::Particle);return 1;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return test::At(test::Player);}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::engine {void* GetWorldContext(){return test::At(test::Player);}}
namespace ue_wrap::building_material {
bool ReadStock(void* p,Stock& out){CHECK(p==test::At(test::Sponge));test::Callback(test::onStock);out={};out.kind=StockKind::Sponge;out.soapAmount=test::objects[test::Sponge].soap;return test::stock;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){test::frames[this].clear();}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t size){auto& bytes=test::frames[this][name];bytes.resize(size);std::memcpy(bytes.data(),value,size);return true;}
bool ParamFrame::GetRaw(const wchar_t* name,void* value,int32_t size)const{const auto& fields=test::frames[this];const auto it=fields.find(name);if(it==fields.end() || it->second.size()!=size)return false;std::memcpy(value,it->second.data(),size);return true;}
bool Call(void* object,ParamFrame& frame){using namespace test;const auto name=*static_cast<std::wstring*>(frame.function());
    if(name==L"SpawnEmitterAtLocation") {
        CHECK(object==At(Library) && Get<void*>(frame,L"WorldContextObject")==At(Player) && Get<void*>(frame,L"EmitterTemplate")==At(Particle));
        const auto point=Get<FVector>(frame,L"Location");CHECK(point.X==100 && point.Y==200 && point.Z==300);
        CHECK(Get<FVector>(frame,L"Scale").X==1 && Get<bool>(frame,L"bAutoDestroy") && Get<bool>(frame,L"bAutoActivate") && Get<uint8_t>(frame,L"PoolingMethod")==0);
        rotation=Get<FRotator>(frame,L"Rotation");if(!spawnOk)return false;++surfaces;
        void* result=At(Spawned);frame.Set<void*>(L"ReturnValue",result);
    } else {CHECK(name==L"Activate" && object==At(Bubbles) && !Get<bool>(frame,L"bReset"));++bursts;}
    Callback(onCall);return true;
}
}
int main() {
    using namespace test;namespace FX=ue_wrap::sponge_effects;using ue_wrap::FVector;
    for(int i=0;i<Count;++i)objects[i].index=i;objects[Sponge].player=At(Player);objects[Sponge].effect=At(Bubbles);
    FX::Use use{};CHECK(FX::ReadUse(At(Sponge),At(Player),use) && use.soap==100 && use.location.X==100 && use.normal.Z==1);
    CHECK(objects[Sponge].soap==100);CHECK(!FX::IsSponge(At(Other)) && !FX::ReadUse(At(Sponge),At(Other),use));
    objects[Sponge].soap=0;CHECK(!FX::ReadUse(At(Sponge),At(Player),use));objects[Sponge].soap=100;
    stock=false;CHECK(!FX::ReadUse(At(Sponge),At(Player),use));stock=true;
    fields=false;CHECK(!FX::ReadUse(At(Sponge),At(Player),use));fields=true;
    onLookup=[]{++world;};CHECK(!FX::ReadUse(At(Sponge),At(Player),use));
    onLookup=[]{++objects[Player].serial;};CHECK(!FX::ReadUse(At(Sponge),At(Player),use));
    onStock=[]{++objects[Sponge].serial;};CHECK(!FX::ReadUse(At(Sponge),At(Player),use));
    CHECK(!FX::ValidSurface({NAN,0,0},{0,0,1}) && !FX::ValidSurface({100,200,300},{0,0,0}) && !FX::ValidSurface({1.1e7f,0,0},{0,0,1}));
    const auto current=[&]{return valid;};const FVector point{100,200,300};
    CHECK(FX::Surface(point,{0,0,1},current) && surfaces==1 && std::fabs(rotation.Pitch-90)<.001f && rotation.Roll==0);
    CHECK(FX::Surface(point,{0,-1,0},current) && std::fabs(rotation.Yaw+90)<.001f && rotation.Pitch==0);
    spawnOk=false;CHECK(FX::Surface(point,{0,0,1},current));spawnOk=true; // dispatched attempt is consumed
    valid=false;auto count=surfaces;CHECK(!FX::Surface(point,{0,0,1},current) && surfaces==count);valid=true;
    onFunction=[&]{valid=false;};CHECK(!FX::Surface(point,{0,0,1},current));valid=true;
    onFunction=[]{++world;};CHECK(!FX::Surface(point,{0,0,1},current));
    onCall=[&]{valid=false;};CHECK(FX::Surface(point,{0,0,1},current));valid=true;
    CHECK(FX::Burst(At(Sponge)) && bursts==1 && objects[Sponge].soap==100);
    objects[Bubbles].live=false;CHECK(!FX::Burst(At(Sponge)));objects[Bubbles].live=true;
    onFunction=[&]{valid=false;};CHECK(!FX::Burst(At(Sponge),current));valid=true;
    onFunction=[]{++world;};CHECK(!FX::Burst(At(Sponge)));
    onFunction=[]{++objects[Bubbles].serial;};CHECK(!FX::Burst(At(Sponge)));
    onFunction=[]{objects[Sponge].effect=At(Other);};CHECK(!FX::Burst(At(Sponge)));objects[Sponge].effect=At(Bubbles);
    onCall=[]{++world;};CHECK(FX::Burst(At(Sponge)) && objects[Sponge].soap==100);
    puts("PASS sponge wrapper: exact native trace location/normal, source/soap/lifetime checks, particle geometry, auto cleanup, no-reset burst and no cleaning/stock calls");
}
