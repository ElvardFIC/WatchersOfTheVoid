#include "../../src/votv-coop/src/ue_wrap/actors/building_material.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;ue_wrap::reflection::FName name{};
 float height=35,interp=0;int32_t soap=2,cement=5,maxUnits=20;uint8_t flags=0x83;void* contents=nullptr;} objs[8];
bool fields=true,callOk=true,kill=false;int paints=0,ticks=0;
bool visible=false;int visibilityWrites=0;
std::wstring key=L"trowel_1";
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
void* ClassOf(void* a){return static_cast<test::Obj*>(a)->cls;}
std::wstring ClassNameOf(void* a){if(ClassOf(a)!=&test::objs[1])return L"other_C";
 switch(test::objs[1].name.ComparisonIndex){case 1:return L"prop_bucket_C";case 2:return L"prop_wallfixer_C";
 case 3:return L"prop_concreteBucket_C";case 4:return L"prop_cementBag_C";case 5:return L"prop_sponge_C";default:return L"other_C";}}
void* SuperStructOf(void* a){return a==&test::objs[1]?&test::objs[2]:nullptr;}
const FName& NameOf(void* a){return static_cast<test::Obj*>(a)->name;}
bool NameEquals(const FName& n,const wchar_t* s){return (n.ComparisonIndex==1&&!wcscmp(s,L"prop_bucket_C")) ||
 (n.ComparisonIndex==2&&!wcscmp(s,L"prop_wallfixer_C")) ||
 (n.ComparisonIndex==3&&!wcscmp(s,L"prop_concreteBucket_C")) ||
 (n.ComparisonIndex==4&&!wcscmp(s,L"prop_cementBag_C")) ||
 (n.ComparisonIndex==5&&!wcscmp(s,L"prop_sponge_C"));}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<8&&a==&test::objs[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return test::objs[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return &test::objs[i];}
int32_t FindPropertyOffset(void*,const wchar_t* n){if(!test::fields)return -1;
 if(!wcscmp(n,L"height")||!wcscmp(n,L"power"))return offsetof(test::Obj,height);
 if(!wcscmp(n,L"height_interp"))return offsetof(test::Obj,interp);
 if(!wcscmp(n,L"soap"))return offsetof(test::Obj,soap);
 if(!wcscmp(n,L"maxUnits"))return offsetof(test::Obj,maxUnits);
 if(!wcscmp(n,L"StaticMesh1"))return offsetof(test::Obj,contents);
 if(!wcscmp(n,L"key"))return offsetof(test::Obj,name);
 if(!wcscmp(n,L"concrete")||!wcscmp(n,L"units"))return offsetof(test::Obj,cement);return -1;}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,flags);mask=!wcscmp(n,L"empty")?1:2;return test::fields;}
void* FindDispatchFunctionCached(void* cls,const wchar_t* n){
 if(!wcscmp(n,L"calcPoints"))return &test::objs[4];
 if(!wcscmp(n,L"ReceiveTick")){CHECK(cls==&test::objs[2]);return &test::objs[5];}
 CHECK(!wcscmp(n,L"upd")||!wcscmp(n,L"updConcrete")||!wcscmp(n,L"updStage"));return &test::objs[3];}
int32_t FunctionFrameSize(void*){return sizeof(float);}
std::vector<ParamInfo> FunctionParams(void*){ParamInfo p{};p.name=L"DeltaSeconds";p.offset=0;return {p};}
bool CallFunction(void* a,void* f,void* raw){
 CHECK(a==&test::objs[0]);
 if(f==&test::objs[3])++test::paints;
 if(f==&test::objs[5]){CHECK(*static_cast<float*>(raw)==.016f);++test::ticks;}
 if(test::kill)test::objs[0].live=false;return test::callOk;
}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::save_record {std::wstring ReadFNameAt(const void*,int32_t){return test::key;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::engine {bool SetComponentVisible(void* c,bool on,bool propagate){
 CHECK(c==&test::objs[6]&&!propagate);test::visible=on;++test::visibilityWrites;return true;}}
int main(){using namespace test;namespace B=ue_wrap::building_material;
 for(int i=0;i<8;++i)objs[i].id=i;objs[0].cls=&objs[1];objs[1].name.ComparisonIndex=1;
 B::Water water{};CHECK(B::ReadWater(&objs[0],water)&&water.height==35&&water.soap==2&&water.alcohol);
 CHECK(B::RepairBucketEmpty(&objs[0])&&objs[0].flags==0x82); // preserve alcohol and unrelated packed bits
 CHECK(B::RepaintBucket(&objs[0])&&objs[0].interp==35&&paints==1);
 CHECK(B::TickBucketContents(&objs[0],.016f)&&ticks==1);
 objs[0].height=0;CHECK(B::RepairBucketEmpty(&objs[0])&&objs[0].flags==0x83);
 objs[0].height=std::numeric_limits<float>::quiet_NaN();CHECK(!B::RepairBucketEmpty(&objs[0]));objs[0].height=15;
 kill=true;CHECK(!B::RepaintBucket(&objs[0])&&paints==1);kill=false;objs[0].live=true;
 objs[1].name.ComparisonIndex=2;int32_t units=0;CHECK(B::ReadConcrete(&objs[0],units)&&units==5);
 CHECK(B::SetRemoteConcrete(&objs[0],0)&&objs[0].cement==0&&paints==2);
 CHECK(!B::SetRemoteConcrete(&objs[0],6)&&!B::SetRemoteConcrete(&objs[0],-1));
 ue_wrap::save_record::SaveRecord saved;saved.className=L"prop_wallfixer_C";saved.key=key;
 saved.ints={{5}};saved.strings={{L"preserve"}};
 const int paintsBefore=paints;CHECK(B::RefreshTrowelRecord(&objs[0],saved)&&saved.ints==std::vector<std::vector<int32_t>>{{0}});
 CHECK(saved.strings[0][0]==L"preserve"&&objs[0].cement==0&&paints==paintsBefore);
 objs[0].cement=3;CHECK(B::RefreshTrowelRecord(&objs[0],saved)&&saved.ints[0][0]==3);
 for(int cause=0;cause<6;++cause){auto snapshot=saved;
  if(cause==0)snapshot.key=L"another_trowel";if(cause==1)snapshot.className=L"prop_bucket_C";
  if(cause==2)objs[0].cement=6;if(cause==3)fields=false;if(cause==4)snapshot.key=L"None";if(cause==5)objs[0].live=false;
  CHECK(!B::RefreshTrowelRecord(&objs[0],snapshot)&&snapshot.ints==saved.ints);
  objs[0].cement=3;fields=true;objs[0].live=true;
 }
 CHECK(!B::RefreshTrowelRecord(nullptr,saved));
 fields=false;CHECK(!B::SetRemoteConcrete(&objs[0],5));fields=true;callOk=false;CHECK(!B::SetRemoteConcrete(&objs[0],3));
 CHECK(!B::ReadWater(&objs[0],water));
 objs[1].name.ComparisonIndex=1;callOk=true;objs[0].flags=0x81;
 CHECK(B::SetRemoteBucketDisplay(&objs[0],24,true,true)&&objs[0].height==24&&objs[0].interp==24&&objs[0].soap==1&&objs[0].flags==0x82);
 CHECK(B::SetRemoteBucketDisplay(&objs[0],0,false,false)&&objs[0].height==0&&objs[0].soap==0&&objs[0].flags==0x81);
 for(float bad:{-1.f,39.f,std::numeric_limits<float>::quiet_NaN()})CHECK(!B::SetRemoteBucketDisplay(&objs[0],bad,true,true)&&objs[0].height==0);
 fields=false;CHECK(!B::SetRemoteBucketDisplay(&objs[0],12,true,true)&&objs[0].height==0);fields=true;
 kill=true;const auto before=paints;CHECK(!B::SetRemoteBucketDisplay(&objs[0],12,true,true)&&paints==before);
 kill=false;objs[0].live=true;B::Stock stock{};
 objs[1].name.ComparisonIndex=3;objs[0].cement=20;objs[0].flags=0x82;
 CHECK(B::ReadStock(&objs[0],stock)&&stock.kind==B::StockKind::ConcreteBucket&&stock.units==20&&stock.dry);
 objs[0].cement=-3;objs[0].flags=0x80;CHECK(B::ReadStock(&objs[0],stock)&&stock.units==-3&&!stock.dry);
 objs[1].name.ComparisonIndex=4;CHECK(B::ReadStock(&objs[0],stock)&&stock.kind==B::StockKind::CementBag&&!stock.dry);
 objs[1].name.ComparisonIndex=2;CHECK(B::ReadStock(&objs[0],stock)&&stock.kind==B::StockKind::Trowel);
 fields=false;CHECK(!B::ReadStock(&objs[0],stock));fields=true;
 objs[1].name.ComparisonIndex=1;CHECK(!B::ReadStock(&objs[0],stock));
 objs[1].name.ComparisonIndex=5;objs[0].height=.75f;
 CHECK(B::ReadStock(&objs[0],stock)&&stock.kind==B::StockKind::Sponge&&stock.wetness==.75f);
 objs[0].height=std::numeric_limits<float>::quiet_NaN();CHECK(!B::ReadStock(&objs[0],stock));
 objs[1].name.ComparisonIndex=3;objs[0].contents=&objs[6];objs[0].flags=0x81;
 CHECK(B::SetRemoteConcreteBucketDisplay(&objs[0],12,true)&&objs[0].cement==12&&objs[0].flags==0x83&&visible);
 CHECK(B::SetRemoteConcreteBucketDisplay(&objs[0],0,false)&&objs[0].cement==0&&objs[0].flags==0x81&&!visible);
 CHECK(B::SetRemoteConcreteBucketDisplay(&objs[0],20,false)&&visible);
 CHECK(!B::SetRemoteConcreteBucketDisplay(&objs[0],21,true)&&objs[0].cement==20);
 fields=false;CHECK(!B::SetRemoteConcreteBucketDisplay(&objs[0],10,true)&&objs[0].cement==20);fields=true;
 objs[6].live=false;CHECK(!B::SetRemoteConcreteBucketDisplay(&objs[0],10,true)&&objs[0].cement==20);objs[6].live=true;
 const int visibilityBefore=visibilityWrites;kill=true;
 CHECK(!B::SetRemoteConcreteBucketDisplay(&objs[0],10,true)&&visibilityWrites==visibilityBefore);kill=false;objs[0].live=true;
 puts("PASS concrete bucket hand display: fill/dry/empty/refill, packed flags, missing metadata/component and destruction during repaint");
 puts("PASS real bucket display setter: level/soap/alcohol, packed-bit preservation, invalid input/metadata and lifetime guards");
 puts("PASS real building wrapper/ParamFrame: packed empty/alcohol flags, water repaint, preserved container tick, trowel bounds/classes/call failure/lifetime");
 return 0;
}
