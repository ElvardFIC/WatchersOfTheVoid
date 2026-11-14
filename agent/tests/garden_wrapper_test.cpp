#include "../../src/votv-coop/src/ue_wrap/actors/placed_save.cpp"
#define Name RecordName
#define Empty RecordEmpty
#include "../../src/votv-coop/src/ue_wrap/actors/placed_save_record.cpp"
#undef Empty
#undef Name
#include "../../src/votv-coop/src/ue_wrap/actors/wall_drying.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "llama_marker_boundary_stubs.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Obj { int id=0, serial=1, amount=7, material=0; bool live=true; uint8_t flags=0; std::wstring key=L"saved_plant_5"; ue_wrap::reflection::FName name{};
float water=0,fertilizer=0,height=35,maxHeight=35;
int classId=1, generations=0, updates=0; void* faucet=nullptr; void* sprinkler=nullptr; void* hose=nullptr;
std::wstring hoseKey=L"None", sprinklerKey=L"None"; } objs[20];
bool fields=true, callOK=true, validOwner=true, loadOK=true; int harvests=0, loads=0;
uint32_t world=1;
std::map<std::wstring,void*> keys;
bool captureOK=false, processOK=true;
int toggles=0, detaches=0;
ue_wrap::save_record::SaveRecord captured;
void (*processCallback)(void*)=nullptr;
void (*nameCallback)()=nullptr;
const wchar_t* ClassLabel(int i) { return i==9?L"dirthole_C":i==6?L"faucet_C":i==7?L"groundHose_C":i==8?L"prop_sprinkler_C":L"growingPlant_C"; }
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a) { return a?static_cast<test::Obj*>(a)->id:-1; }
void* ObjectAt(int32_t i) { return i>=0&&i<20?&test::objs[i]:nullptr; }
int32_t SlotSerial(int32_t i) { return i>=0&&i<20?test::objs[i].serial:0; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLive(void* a) { return a&&static_cast<test::Obj*>(a)->live; }
bool IsLiveByIndex(void* a,int32_t i) { return a&&a==ObjectAt(i)&&IsLive(a); }
void* ClassOf(void* a) { return a?&test::objs[static_cast<test::Obj*>(a)->classId]:nullptr; }
void* SuperStructOf(void*) { return nullptr; }
std::wstring ClassNameOf(void* a) { return a==&test::objs[4]?L"prop_bucket_C":a==&test::objs[3]?L"prop_fertilizer_C":test::ClassLabel(static_cast<test::Obj*>(a)->classId); }
const FName& NameOf(void* a) { return static_cast<test::Obj*>(a)->name; }
bool NameEquals(const FName& n,const wchar_t* name) { return !wcscmp(name,test::ClassLabel(n.ComparisonIndex))&&(n.ComparisonIndex==1||n.ComparisonIndex==6||n.ComparisonIndex==7||n.ComparisonIndex==8||n.ComparisonIndex==9); }
int32_t FindPropertyOffset(void*,const wchar_t* name) { return test::fields && !wcscmp(name,L"matType") ? offsetof(test::Obj,material) : -1; }
bool NameStartsWith(const FName& n,const wchar_t* name) { return n.ComparisonIndex==1&&!wcscmp(name,L"growingPlant"); }
void* OuterOf(void*) { return &test::objs[test::validOwner?1:5]; }
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask) { off=offsetof(test::Obj,flags);mask=4;return test::fields; }
int32_t InstanceOffset::Of(void*) {
 if(!test::fields)return -1;
 if(!wcscmp(prop_,L"amount"))return offsetof(test::Obj,amount);
 if(!wcscmp(prop_,L"water"))return offsetof(test::Obj,water);
 if(!wcscmp(prop_,L"fertilizer"))return offsetof(test::Obj,fertilizer);
 if(!wcscmp(prop_,L"height"))return offsetof(test::Obj,height);
 if(!wcscmp(prop_,L"maxHeight"))return offsetof(test::Obj,maxHeight);
 if(!wcscmp(prop_,L"faucet"))return offsetof(test::Obj,faucet);
 if(!wcscmp(prop_,L"sprinkler"))return offsetof(test::Obj,sprinkler);
 if(!wcscmp(prop_,L"hose"))return offsetof(test::Obj,hose);
 if(!wcscmp(prop_,L"hose_key"))return offsetof(test::Obj,hoseKey);
 if(!wcscmp(prop_,L"sprinkler_key"))return offsetof(test::Obj,sprinklerKey);
 return offsetof(test::Obj,key);
}
void* FindDispatchFunctionCached(void*,const wchar_t* name) { return &test::objs[!wcscmp(name,L"actionOptionIndex")?2:!wcscmp(name,L"loadData")?3:!wcscmp(name,L"processKeys")?18:!wcscmp(name,L"upd")?17:!wcscmp(name,L"detachSprinkler")?16:4]; }
int32_t FunctionFrameSize(void* f) { return f==&test::objs[2]?24:0; }
std::vector<ParamInfo> FunctionParams(void*) {
    std::vector<ParamInfo> out;const wchar_t* names[]{L"player",L"action",L"lookAtComponent"};
    for(int i=0;i<3;++i){ParamInfo p{};p.name=names[i];p.offset=i*8;out.push_back(p);}return out;
}
bool CallFunction(void* actor,void* fn,void* raw) {
    if(!test::callOK)return false;
    if(fn==&test::objs[18]){
        if(!test::processOK)return false;
        ++static_cast<test::Obj*>(actor)->generations;
        if(test::processCallback)test::processCallback(actor);
    }
    if(fn==&test::objs[17])++static_cast<test::Obj*>(actor)->updates;
    if(fn==&test::objs[16]){
        auto* hose=static_cast<test::Obj*>(actor);auto* end=static_cast<test::Obj*>(hose->sprinkler);
        CHECK(end&&end->hose==actor&&end->hoseKey==L"None"&&hose->sprinklerKey==L"None");
        end->hose=nullptr;hose->sprinkler=nullptr;++test::detaches;
    }
    if(fn==&test::objs[2]){
        auto* p=static_cast<uint8_t*>(raw);void* player=nullptr;void* component=nullptr;
        std::memcpy(&player,p,8);std::memcpy(&component,p+16,8);
        if(static_cast<test::Obj*>(actor)->classId==6){CHECK(!player&&!component&&p[8]==5);++test::toggles;}
        else {CHECK(actor==&test::objs[0]&&player==&test::objs[5]&&!component&&p[8]==4);++test::harvests;}
    }
    return true;
}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* a){return a==&test::objs[0]?CurrentWorld():nullptr;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& out){out={11,22,33};return true;}
bool TryGetActorRotation(void*,FRotator& out){out={};return true;}
FVector GetActorScale3D(void*){return {1,1,1};}
void RotatorToQuat(float,float,float,float& x,float& y,float& z,float& w){x=y=z=0;w=1;}
bool SetActorScale3D(void*,const FVector&){return true;}
bool DestroyActor(void*){return false;}
void* BeginDeferredSpawn(void*,const FVector&,const FRotator&){return nullptr;}
bool FinishDeferredSpawn(void*,const FVector&,const FRotator&){return false;}
bool SetActorTickEnabled(void*,bool){return false;}
void* ReadMainPlayerHitActor(void*){return &test::objs[0];}
FRotator QuatToRotator(float,float,float,float){return {};}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return nullptr;}}
namespace ue_wrap::keyed_objects {void* Resolve(const wchar_t* key){auto it=test::keys.find(key);return it==test::keys.end()?nullptr:it->second;}}
namespace ue_wrap::wall_build {bool RestoreMaterial(void*,const wchar_t*){return true;}}
namespace ue_wrap::save_record {
bool ReadFloatGroup(const void*,size_t,std::vector<float>&){std::abort();}
bool ReplaceFloatGroup(void*,size_t,const std::vector<float>&){std::abort();}
bool CaptureRecordVia(void*,void*,SaveRecord& r){r=test::captured;return test::captureOK;}
std::wstring ReadFNameAt(const void* a,int32_t){return static_cast<const test::Obj*>(a)->key;}
void WriteFNameField(void* field,const std::wstring& name){*static_cast<std::wstring*>(field)=name;if(test::nameCallback)test::nameCallback();}
bool ApplyRecordVia(void* actor,void* fn,const SaveRecord& r,ClassResolver){
    CHECK(fn==&test::objs[3]&&static_cast<test::Obj*>(actor)->amount==0);++test::loads;
    if(!test::loadOK)return false;
    if(r.floats[1][3]>=1.f)static_cast<test::Obj*>(actor)->amount=1;
    return true;
}
}
int main() {
    using namespace test;namespace P=ue_wrap::placed_save;
    for(int i=0;i<6;++i){objs[i].id=i;objs[i].name.ComparisonIndex=i;}
    auto* a=&objs[0];a->flags=2;
    void* collector=&objs[5];
    CHECK(P::CanHarvest(a)&&P::Harvest(a,collector)&&harvests==1); // unrelated packed bit preserved
    a->flags|=4;CHECK(!P::CanHarvest(a)&&!P::Harvest(a,collector)&&harvests==1);a->flags=0;
    fields=false;CHECK(!P::Harvest(a,collector));fields=true;
    validOwner=false;CHECK(!P::Harvest(a,collector));validOwner=true;
    callOK=false;CHECK(!P::Harvest(a,collector));callOK=true;
    CHECK(!P::Harvest(a,nullptr));
    CHECK(P::ExistingKey(a)==L"saved_plant_5");
    ue_wrap::save_record::SaveRecord r;r.className=L"growingPlant_C";r.key=L"saved_plant_5";
    r.names={{},{L"tomato",L"None"}};r.floats={{},{1,0,0,0,0,0}};
    CHECK(P::Apply(a,r)&&a->amount==0&&loads==1);
    r.floats[1][3]=1;CHECK(P::Apply(a,r)&&a->amount==1);
    r.floats[1][3]=0;CHECK(P::Apply(a,r)&&a->amount==0); // ripe -> harvested must clear cached amount
    a->amount=1;loadOK=false;CHECK(!P::Apply(a,r));loadOK=true;
    P::CareUse use;CHECK(P::ReadCareUse(&objs[4],collector,P::CareKind::Water,use)&&use.plant==a&&use.amount==600.f);
    objs[4].height=17.5f;CHECK(P::ReadCareUse(&objs[4],collector,P::CareKind::Water,use)&&use.amount==300.f);
    objs[4].maxHeight=0;CHECK(!P::ReadCareUse(&objs[4],collector,P::CareKind::Water,use));objs[4].maxHeight=35;
    objs[4].height=36;CHECK(!P::ReadCareUse(&objs[4],collector,P::CareKind::Water,use));objs[4].height=17.5f;
    const auto sourceKey=objs[4].key;objs[4].key=L"None";CHECK(!P::ReadCareUse(&objs[4],collector,P::CareKind::Water,use));objs[4].key=sourceKey;
    CHECK(!P::CareWasSpent(&objs[4],P::CareKind::Water));objs[4].height=0;
    CHECK(P::CareWasSpent(&objs[4],P::CareKind::Water)&&!P::ReadCareUse(&objs[4],collector,P::CareKind::Water,use));
    CHECK(P::ReadCareUse(&objs[3],collector,P::CareKind::Fertilizer,use)&&use.amount==600.f);
    a->fertilizer=5;CHECK(!P::ReadCareUse(&objs[3],collector,P::CareKind::Fertilizer,use));
    CHECK(P::ApplyCare(a,P::CareKind::Water,300.f)&&a->water==300.f);
    CHECK(P::ApplyCare(a,P::CareKind::Fertilizer,600.f)&&a->fertilizer==5.f);
    a->fertilizer=-1;CHECK(P::ApplyCare(a,P::CareKind::Fertilizer,600.f)&&a->fertilizer==600.f);
    CHECK(!P::ApplyCare(a,P::CareKind::Water,601.f)&&!P::ApplyCare(a,P::CareKind::Fertilizer,300.f));
    puts("PASS measured care wrapper: bucket ratio/capacity/minimum/key, empty-source proof, fertilizer 600/no-reset and canonical field writes");
    puts("PASS native garden wrapper/ParamFrame: explicit collector, packed discard/metadata/owner guards, saved identity, ripe-to-harvested amount reset");
    objs[9].id=9;objs[9].name.ComparisonIndex=9;a->classId=9;a->material=2;
    CHECK(P::Type(a)==P::Kind::DirtHole && P::SetKey(a,L"mvpl_00000001_00000001"));
    ue_wrap::save_record::SaveRecord hole, normalized;
    CHECK(P::Capture(a,hole) && hole.key==L"mvpl_00000001_00000001" && hole.ints[0][0]==2);
    CHECK(hole.xform[4]==11 && hole.xform[5]==22 && hole.xform[6]==33 && P::Normalize(hole,normalized));
    hole.ints[0][0]=1;CHECK(P::Apply(a,hole) && a->material==1 && P::Link(a,hole));
    hole.ints[0][0]=3;CHECK(!P::Apply(a,hole) && a->material==1);
    ++a->serial;CHECK(P::ExistingKey(a).empty());
    CHECK(P::SetKey(a,L"fresh"));++world;CHECK(P::ExistingKey(a).empty());
    puts("PASS transient hole identity, position/material capture, validated repaint and recycled-slot/world cleanup");
    return 0;
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
}
