#include "../../src/votv-coop/src/ue_wrap/actors/save_record.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/cooking_food.cpp"
#include "../../src/votv-coop/src/coop/props/food_save_sync.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <set>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Object {int index=0,serial=1;bool live=true;float progress=12.5f;std::wstring cls=L"prop_cookingFood_shrimp_C";} objects[3];
uint32_t world=1;bool fields=true,allocation=true;std::set<void*> buffers;
std::function<void()> callback;std::wstring key=L"food-key";
bool saveOk=false;int saves=0,loads=0;void* out=nullptr;
ue_wrap::script_gate::PostFn saved=nullptr,loaded=nullptr;
void Callback(){auto f=std::move(callback);callback={};if(f)f();}
}
namespace ue_wrap::reflection {
void* EngineAlloc(size_t n,uint32_t){if(!test::allocation)return nullptr;auto* p=std::malloc(n);CHECK(p);test::buffers.insert(p);return p;}
void EngineFree(void* p){if(p){CHECK(test::buffers.erase(p)==1);std::free(p);}}
bool IsLive(void* p){return p&&static_cast<test::Object*>(p)->live;}
int32_t InternalIndexOf(void* p){return static_cast<test::Object*>(p)->index;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<3&&p==&test::objects[i]&&IsLive(p);}
void* ClassOf(void* p){return p;}
void* SuperStructOf(void* p){return p==&test::objects[2]?nullptr:&test::objects[2];}
const FName& NameOf(void* p){static FName n{};n.ComparisonIndex=static_cast<test::Object*>(p)->index;return n;}
bool NameEquals(const FName& n,const wchar_t* s){return test::objects[n.ComparisonIndex].cls==s;}
std::wstring ClassNameOf(void* p){return static_cast<test::Object*>(p)->cls;}
int32_t InstanceOffset::Of(void*){test::Callback();return test::fields?offsetof(test::Object,progress):-1;}
int32_t FindPropertyOffset(void*,const wchar_t*){return offsetof(test::Object,serial);}
int32_t FindParamOffset(void*,const wchar_t*){return 16;}
int32_t FunctionFrameSize(void*){return 272;}
std::wstring ToString(const FName&){return test::key;}
void* FindClass(const wchar_t*){std::abort();}
void* FindFunction(void*,const wchar_t*){std::abort();}
void* FindDispatchFunction(void*,const wchar_t*,void**){std::abort();}
bool IsDescendantOfAny(void*,void* const*,size_t,int){std::abort();}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){}
int32_t ParamFrame::OffsetOf(const wchar_t*)const{std::abort();}
bool Call(void*,ParamFrame&){std::abort();}
}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){std::abort();}}
namespace ue_wrap::fstring_utils {bool MintFString(const std::wstring&,void*){std::abort();}}
namespace ue_wrap::signal_dynamic {
bool ReadStruct(const void*,Row&,bool){std::abort();}
bool WriteStructLive(void*,const Row&){std::abort();}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* id,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
    :ident_(id),feature_(feature),cls_(cls),fn_(fn),mode_(mode){}
}
namespace ue_wrap::script_gate {
uint8_t* OutParamPtr(const Call&,int32_t){return static_cast<uint8_t*>(test::out);}
bool WatchName(const facts::Function& row,int,PreFn pre,PostFn post){CHECK(!pre);
 if(!wcscmp(row.Name(),L"getData")){++test::saves;test::saved=post;return test::saveOk;}
 ++test::loads;test::loaded=post;return true;
}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){using namespace test;namespace C=ue_wrap::cooking_food;namespace SR=ue_wrap::save_record;
 for(int i=0;i<3;++i)objects[i].index=i;objects[2].cls=L"prop_cookingFood_C";
 alignas(16) uint8_t record[SR::kSaveRecordBytes]{};
 CHECK(SR::ReplaceFloatGroup(record,0,{88}));CHECK(SR::ReplaceFloatGroup(record,1,{350,.5f,100}));
 const auto outer=SR::ReadArr(record,0x58);const auto base=SR::ReadArr(outer.data,0).data;
 CHECK(C::IsCookingFood(&objects[0])&&C::SaveNative(&objects[0],record));
 const auto grown=SR::ReadArr(record,0x58);CHECK(grown.num==3&&SR::ReadArr(grown.data,0).data==base);
 std::vector<float> values;CHECK(SR::ReadFloatGroup(record,2,values)&&values==std::vector<float>{12.5f});
 objects[1].progress=0;CHECK(C::LoadNative(&objects[1],record)&&objects[1].progress==12.5f);
 const auto count=buffers.size();for(int i=0;i<1000;++i)CHECK(C::SaveNative(&objects[0],record)&&C::LoadNative(&objects[1],record));
 CHECK(buffers.size()==count);
 allocation=false;objects[0].progress=20;CHECK(!C::SaveNative(&objects[0],record));allocation=true;
 CHECK(SR::ReadFloatGroup(record,2,values)&&values[0]==12.5f);
 for(const auto& bad:std::vector<std::vector<float>>{{},{1,2},{-.5f},{NAN},{INFINITY},{1'000'001}}){
  CHECK(SR::ReplaceFloatGroup(record,2,bad));CHECK(!C::LoadNative(&objects[1],record)&&objects[1].progress==12.5f);
 }
 CHECK(SR::ReplaceFloatGroup(record,2,{7}));fields=false;CHECK(!C::LoadNative(&objects[1],record));fields=true;
 callback=[]{++world;};CHECK(!C::LoadNative(&objects[1],record)&&objects[1].progress==12.5f);
 callback=[]{++objects[1].serial;};CHECK(!C::LoadNative(&objects[1],record)&&objects[1].progress==12.5f);
 SR::SaveRecord profile;profile.className=objects[0].cls;profile.key=key;profile.floats={{1},{2,3}};profile.ints={{4}};
 CHECK(C::RefreshHeldRecord(&objects[0],profile)&&profile.floats[2][0]==20&&profile.floats[0][0]==1&&profile.ints[0][0]==4);
 key=L"other";CHECK(!C::RefreshHeldRecord(&objects[0],profile));key=profile.key;
 namespace S=coop::props::food_save_sync;S::Install();CHECK(saves==1&&loads==1);saveOk=true;S::Install();CHECK(saves==2&&loads==1);
 ue_wrap::script_gate::Call call{};call.object=&objects[0];call.function=&objects[0];out=record;saved(call);
 call.object=&objects[1];loaded(call);CHECK(objects[1].progress==20);
 const auto all=SR::ReadArr(record,0x58);for(int i=0;i<all.num;++i)ue_wrap::reflection::EngineFree(const_cast<uint8_t*>(SR::ReadArr(all.data+i*16,0).data));
 ue_wrap::reflection::EngineFree(const_cast<uint8_t*>(all.data));CHECK(buffers.empty());
 std::memset(record,0,sizeof(record));CHECK(C::LoadNative(&objects[1],record)&&objects[1].progress==0);
 puts("PASS cooking native persistence: family, progress, old saves, allocation failure, no leaks, exact held identity, watch retry, serial/world cancellation");
}
