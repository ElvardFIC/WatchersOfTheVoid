// Production native scalar codec + drying wrapper + save/load seams. Only UE boundaries are doubled.
#include "../../src/votv-coop/src/ue_wrap/actors/save_record.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/wall_drying.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/material_saved_state.cpp"
#include "../../src/votv-coop/src/ue_wrap/devices/grill_saved_state.cpp"
#include "../../src/votv-coop/src/coop/world/material_save_sync.cpp"
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <limits>
#include <set>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Actor {int index=0,serial=1;bool live=true;float elapsed=0,duration=360,dryTimer=0,soap=0,power=.75f,burn=0;uint8_t active=0xA0;std::wstring cls=L"customWall_wetConcrete_C";} actors[2];
uint32_t world=1;bool fields=true,allocation=true,saveWatch=true,loadWatch=true;
int allocs=0,frees=0,saveRegistrations=0,loadRegistrations=0,frameSize=272,parameter=16,failIn=0;
std::set<void*> buffers;std::function<void()> inOffset;
void* out=nullptr;
ue_wrap::script_gate::PostFn saved=nullptr,loaded=nullptr;
void Callback(){auto fn=std::move(inOffset);inOffset={};if(fn)fn();}
}
namespace ue_wrap::reflection {
void* EngineAlloc(size_t n,uint32_t){if(!test::allocation||(test::failIn&&!--test::failIn))return nullptr;void* p=std::malloc(n);CHECK(p);test::buffers.insert(p);++test::allocs;return p;}
void EngineFree(void* p){if(!p)return;CHECK(test::buffers.erase(p)==1);++test::frees;std::free(p);}
bool IsLive(void* a){return a&&static_cast<test::Actor*>(a)->live;}
int32_t InternalIndexOf(void* a){return static_cast<test::Actor*>(a)->index;}
int32_t SlotSerial(int32_t i){return test::actors[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<2&&a==&test::actors[i]&&IsLive(a);}
void* ClassOf(void* a){return a;}
std::wstring ClassNameOf(void* a){return static_cast<test::Actor*>(a)->cls;}
int32_t InstanceOffset::Of(void*){test::Callback();if(!test::fields)return -1;
 if(!wcscmp(prop_,L"a"))return offsetof(test::Actor,burn);
 if(!wcscmp(prop_,L"dryTime"))return offsetof(test::Actor,elapsed);
 if(!wcscmp(prop_,L"dryTimer"))return offsetof(test::Actor,dryTimer);
 if(!wcscmp(prop_,L"soapAmount"))return offsetof(test::Actor,soap);
 return offsetof(test::Actor,duration);}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask){CHECK(!wcscmp(name,L"active"));
 off=offsetof(test::Actor,active);mask=4;return test::fields;}
int32_t FindParamOffset(void*,const wchar_t*){return test::parameter;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
void* FindClass(const wchar_t*){std::abort();}
void* FindFunction(void*,const wchar_t*){std::abort();}
void* FindDispatchFunction(void*,const wchar_t*,void**){std::abort();}
void* SuperStructOf(void*){return nullptr;}
const FName& NameOf(void*){static FName n{};return n;}
std::wstring ToString(const FName&){std::abort();}
bool IsDescendantOfAny(void*,void* const*,size_t,int){std::abort();}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){}
int32_t ParamFrame::OffsetOf(const wchar_t*)const{std::abort();}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){std::abort();}
bool ParamFrame::GetRaw(const wchar_t*,void*,int32_t)const{std::abort();}
bool Call(void*,ParamFrame&){std::abort();}
}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){std::abort();}}
namespace ue_wrap::fstring_utils {bool MintFString(const std::wstring&,void*){std::abort();}}
namespace ue_wrap::signal_dynamic {
bool ReadStruct(const void*,Row&,bool){std::abort();}
bool WriteStructLive(void*,const Row&){std::abort();}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){std::abort();}}
namespace ue_wrap::grill {
bool IsGrill(void* a){return a && reflection::IsLive(a) && reflection::ClassNameOf(a)==L"prop_bbq_C";}
bool ReadStock(void* actor,Stock& out,const std::function<bool()>& current){if(!IsGrill(actor) || !test::fields || (current && !current()))return false;out={7,true};return true;}
}
namespace ue_wrap::component_calls {bool CallParamlessNamed(void* a,const wchar_t* verb){CHECK(grill::IsGrill(a) && !wcscmp(verb,L"upd"));test::Callback();return true;}}
namespace ue_wrap::building_material {
bool ReadStock(void* a,Stock& s){auto& actor=*static_cast<test::Actor*>(a);s={};
 if(actor.cls==L"prop_concreteBucket_C"){s.kind=StockKind::ConcreteBucket;s.dryTimer=actor.dryTimer;return true;}
 s.kind=StockKind::Sponge;s.wetness=actor.power;s.soapAmount=actor.soap;return true;
}
}
namespace ue_wrap::script_gate {
uint8_t* OutParamPtr(const Call&,int32_t){return static_cast<uint8_t*>(test::out);}
bool WatchClassName(const wchar_t* cls,const wchar_t* fn,int,PreFn pre,PostFn post){
 CHECK(!pre);if(wcscmp(cls,L"customWall_C"))return true;
 if(!wcscmp(fn,L"getData")){++test::saveRegistrations;test::saved=post;return test::saveWatch;}
 CHECK(!wcscmp(fn,L"loadData"));++test::loadRegistrations;test::loaded=post;return test::loadWatch;
}
}
int main(){using namespace test;namespace SR=ue_wrap::save_record;namespace D=ue_wrap::wall_drying;namespace S=coop::material_save_sync;
 actors[0].index=0;actors[1].index=1;
 alignas(16) uint8_t record[SR::kSaveStride]{};
 auto* groups=static_cast<uint8_t*>(ue_wrap::reflection::EngineAlloc(2*SR::kMxStride));std::memset(groups,0,2*SR::kMxStride);
 SR::WriteArrHeader(record,0x58,groups,2);CHECK(SR::ReplaceFloatGroup(record,0,{120,200}));
 CHECK(SR::ReplaceFloatGroup(record,1,{777}));const auto other=SR::ReadArr(groups+SR::kMxStride,0).data;
 actors[0].elapsed=72;CHECK(D::SaveNative(&actors[0],record));std::vector<float> values;
 CHECK(SR::ReadFloatGroup(record,0,values)&&values==std::vector<float>({120,200,72,360}));
 CHECK(SR::ReadArr(groups+SR::kMxStride,0).data==other);CHECK(D::LoadNative(&actors[1],record));
 CHECK(actors[1].elapsed==72&&actors[1].duration==360);
 const auto live=buffers.size();for(int i=0;i<1000;++i){CHECK(D::SaveNative(&actors[0],record)&&D::LoadNative(&actors[1],record));}
 CHECK(buffers.size()==live&&allocs-frees==live);
 const auto previous=SR::ReadArr(groups,0).data;allocation=false;actors[0].elapsed=100;
 CHECK(!D::SaveNative(&actors[0],record)&&SR::ReadArr(groups,0).data==previous);allocation=true;
 CHECK(SR::ReadFloatGroup(record,0,values)&&values[2]==72);
 CHECK(SR::ReplaceFloatGroup(record,0,{120,200}));actors[1].elapsed=19;actors[1].duration=400;
 CHECK(D::LoadNative(&actors[1],record)&&actors[1].elapsed==19&&actors[1].duration==400);
 for(const auto& bad:std::vector<std::vector<float>>{{120,200,1},{120,200,-1,360},{120,200,1,0},{120,200,1,1e7f},{120,200,NAN,360}}){
  CHECK(SR::ReplaceFloatGroup(record,0,bad));CHECK(!D::LoadNative(&actors[1],record));CHECK(actors[1].elapsed==19&&actors[1].duration==400);
 }
 CHECK(SR::ReplaceFloatGroup(record,0,{120,200,72,360}));fields=false;CHECK(!D::LoadNative(&actors[1],record));fields=true;
 inOffset=[]{++world;};CHECK(!D::LoadNative(&actors[1],record)&&actors[1].elapsed==19);
 inOffset=[]{++actors[1].serial;};CHECK(!D::LoadNative(&actors[1],record)&&actors[1].elapsed==19);
 actors[1].cls=L"customWall_C";CHECK(!D::LoadNative(&actors[1],record));actors[1].cls=L"customWall_wetConcrete_C";
 saveWatch=false;S::Install();CHECK(saveRegistrations==1&&loadRegistrations==1);saveWatch=true;
 S::Install();CHECK(saveRegistrations==2&&loadRegistrations==1);S::Install();CHECK(saveRegistrations==2);
 ue_wrap::script_gate::Call c{};c.object=&actors[0];c.function=&actors[0];out=record;
 actors[0].elapsed=88;saved(c);actors[1].elapsed=0;c.object=&actors[1];loaded(c);CHECK(actors[1].elapsed==88);
 frameSize=20;actors[1].elapsed=9;loaded(c);CHECK(actors[1].elapsed==9);frameSize=272;
 alignas(16) uint8_t frame[272]{};std::memcpy(frame+16,record,SR::kSaveRecordBytes);out=nullptr;c.locals=frame;
 loaded(c);CHECK(actors[1].elapsed==88); // by-value/ProcessEvent parameters
 c.object=&actors[0];actors[0].elapsed=99;saved(c);std::memcpy(record,frame+16,SR::kSaveRecordBytes);
 CHECK(SR::ReadFloatGroup(record,0,values)&&values[2]==99);
 CHECK(!SR::ReadFloatGroup(nullptr,0,values)&&!SR::ReplaceFloatGroup(record,64,{1}));
 for(int i=0;i<2;++i)ue_wrap::reflection::EngineFree(const_cast<uint8_t*>(SR::ReadArr(groups+i*SR::kMxStride,0).data));
 ue_wrap::reflection::EngineFree(groups);CHECK(buffers.empty());
 // Concrete's native record has only base floats. Append without losing or
 // duplicating ownership of the base group's child array, including allocation failure.
 std::memset(record,0,sizeof(record));CHECK(SR::ReplaceFloatGroup(record,0,{55,66}));
 const auto baseOuter=SR::ReadArr(record,0x58).data;const auto baseChild=SR::ReadArr(baseOuter,0).data;
 actors[0].cls=L"prop_concreteBucket_C";actors[0].dryTimer=234;failIn=2;
 CHECK(!ue_wrap::material_saved_state::SaveNative(&actors[0],record));
 CHECK(SR::ReadArr(record,0x58).data==baseOuter&&SR::ReadArr(baseOuter,0).data==baseChild);
 CHECK(ue_wrap::material_saved_state::SaveNative(&actors[0],record));
 auto all=SR::ReadArr(record,0x58);CHECK(all.num==2&&SR::ReadArr(all.data,0).data==baseChild);
 actors[1].cls=L"prop_concreteBucket_C";actors[1].dryTimer=0;
 CHECK(ue_wrap::material_saved_state::LoadNative(&actors[1],record)&&actors[1].dryTimer==234);
 CHECK(SR::ReplaceFloatGroup(record,1,{-1}));CHECK(!ue_wrap::material_saved_state::LoadNative(&actors[1],record)&&actors[1].dryTimer==234);
 actors[0].cls=L"prop_sponge_p_C";actors[0].soap=23;CHECK(SR::ReplaceFloatGroup(record,1,{.75f}));
 CHECK(ue_wrap::material_saved_state::SaveNative(&actors[0],record));
 CHECK(SR::ReadFloatGroup(record,1,values)&&values==std::vector<float>({.75f,23}));
 actors[1].cls=L"prop_sponge_C";actors[1].soap=0;CHECK(ue_wrap::material_saved_state::LoadNative(&actors[1],record)&&actors[1].soap==23);
 CHECK(SR::ReplaceFloatGroup(record,1,{.75f}));actors[1].soap=3;
 CHECK(ue_wrap::material_saved_state::LoadNative(&actors[1],record)&&actors[1].soap==3); // old native save
 all=SR::ReadArr(record,0x58);for(int i=0;i<all.num;++i)ue_wrap::reflection::EngineFree(const_cast<uint8_t*>(SR::ReadArr(all.data+i*SR::kMxStride,0).data));
 ue_wrap::reflection::EngineFree(const_cast<uint8_t*>(all.data));CHECK(buffers.empty());
 puts("PASS material extensions: appended concrete drying timer, sponge soap/base fields, scalar/outer allocation rollback, child ownership transfer, old native records and restored stock");
 puts("PASS wet walls: actual native record save/load, old saves, elapsed/threshold, in-place group ownership, 1000-cycle allocation balance, refusal, malformed metadata, world/serial fences, pre-session seams and out/by-value frames");
 return 0;
}
