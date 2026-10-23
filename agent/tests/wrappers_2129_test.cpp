#include "../../src/votv-coop/src/ue_wrap/devices/lightswitch.cpp"
#include "../../src/votv-coop/src/coop/props/prop_spawn_authoring.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/cooking_food.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace test {
struct Obj { ue_wrap::reflection::FName name; const wchar_t* text; void* owner=nullptr; };
Obj objs[]={{{1,0},L"prop_workbench_C"},{{2,0},L"ExecuteUbergraph_prop_workbench"},
    {{3,0},L"lib_C"},{{4,0},L"replaceProp"},{{5,0},L"update"},{{6,0},L"tool_spawn_C"},
    {{7,0},L"ExecuteUbergraph_tool_spawn"},{{8,0},L"GameplayStatics"},
    {{9,0},L"PlaySound2D"},{{10,0},L"use"},{{13,0},L"prop_sleepingbag_C"},
    {{14,0},L"ExecuteUbergraph_prop_sleepingbag"},{{15,0},L"sleepingbagWrap_C"},
    {{16,0},L"fold"},{{17,0},L"gen"},{{18,0},L"prop_bunkbedSleepable_C"},
    {{19,0},L"putMattressOn"},{{20,0},L"bed_bunkMattress_C"},{{21,0},L"ExecuteUbergraph_bed_bunkMattress"},
    {{22,0},L"prop_food_mre_C"},{{23,0},L"spawn"},
    {{24,0},L"prop_foodBox_C"},{{25,0},L"ExecuteUbergraph_prop_foodBox"},
    {{26,0},L"prop_cookingFood_C"},{{27,0},L"prop_cookingFood_cookietray_C"},
    {{28,0},L"prop_cookingFood_shrimp_C"},{{29,0},L"prop_cookingFood_shrimp1_C"},{{30,0},L"cookItem"},
    {{31,0},L"prop_cf_egg_C"},{{32,0},L"prop_cfood_breadmold_C"},
    {{33,0},L"prop_cfood_pizzad_mush_C"},{{34,0},L"prop_cfood_pizzad_pepp_C"},
    {{35,0},L"prop_cfood_pizzad_pine_C"},{{36,0},L"prop_cfood_pizzad_shri_C"},
    {{37,0},L"prop_fishSteak_C"},{{38,0},L"prop_food_potato_C"},
    {{39,0},L"prop_food_potato_h_C"},{{40,0},L"prop_food_potato_q_C"},
    {{41,0},L"prop_food_banana_C"},{{42,0},L"ExecuteUbergraph_prop_food_banana"},
    {{43,0},L"prop_food_coffeeCup_C"},{{44,0},L"ExecuteUbergraph_prop_food_coffeeCup"},
    {{45,0},L"prop_bbq_C"},{{46,0},L"ExecuteUbergraph_prop_bbq"}};
Obj sw{{11,0},L"switch"}, other{{12,0},L"other"};
void* caller=nullptr;bool body=false, locationOk=true, callOk=true, nested=false, attenuationOk=false;
int clicks=0,uses=0;float vol=0,pitch=0;
bool cookingSource=false,lidOutput=false,foodOutput=false,mreOutput=false;
ue_wrap::ufunction_hook::PreNativeCallback hook=nullptr;
struct Sound { void* cue=&other;float volume=.75f,pitch=1.2f; } sound;
}
namespace ue_wrap::reflection {
const FName& NameOf(void* p) { return static_cast<test::Obj*>(p)->name; }
std::wstring ToString(const FName& n) { for(auto& o:test::objs) if(o.name.ComparisonIndex==n.ComparisonIndex)return o.text;return L"other"; }
bool NameEquals(const FName& n,const wchar_t* s) { return !_wcsicmp(ToString(n).c_str(),s); }
void* OuterOf(void* p) { return p ? static_cast<test::Obj*>(p)->owner : nullptr; }
void* ClassOf(void* p) { return p==&test::sw && test::cookingSource?&test::objs[23]:p; }
void* SuperStructOf(void* p) {
    for(int i=24;i<38;++i)if(i!=27 && p==&test::objs[i])return &test::objs[23];
    return nullptr;
}
bool IsLive(void* p) { return p!=nullptr; }
bool IsLiveByIndex(void* p,int32_t i) { return p && NameOf(p).ComparisonIndex==i; }
int32_t InternalIndexOf(void* p) { return p ? NameOf(p).ComparisonIndex : -1; }
int32_t SlotSerial(int32_t) { return 1; }
int32_t AllocateSlotSerial(int32_t) { return 1; }
void* ObjectAt(int32_t i) { for(auto& o:test::objs) if(o.name.ComparisonIndex==i) return &o;return nullptr; }
void* FindClass(const wchar_t* s) { for(auto& o:test::objs) if(!_wcsicmp(o.text,s))return &o;return nullptr; }
void* FindFunction(void*,const wchar_t* s) { return FindClass(s); }
int32_t FindPropertyOffset(void*,const wchar_t*) { return -1; }
bool FindBoolProperty(void*,const wchar_t*,int32_t&,uint8_t&) { return false; }
int32_t InstanceOffset::Of(void*) { std::abort(); }
std::wstring ClassNameOf(void*) { std::abort(); }
void* FindDispatchFunctionCached(void*,const wchar_t*) { return nullptr; }
int32_t FindParamOffset(void*,const wchar_t* n) {
    if(!std::wcscmp(n,L"Sound"))return 0;
    if(!std::wcscmp(n,L"VolumeMultiplier"))return 8;
    if(!std::wcscmp(n,L"PitchMultiplier"))return 12;
    return -1;
}
int32_t FunctionFrameSize(void*) { return 0; }
std::vector<ParamInfo> FunctionParams(void*) { return {}; }
bool IsDescendantOfAny(void* p,void* const* b,size_t n,int) { for(size_t i=0;i<n;++i)if(p==b[i])return true;return false; }
bool CallFunction(void* obj,void*,void*) {
    ++test::uses;
    CHECK(test::hook);
    auto bytes=reinterpret_cast<const uint8_t*>(&test::sound);
    CHECK(test::hook(nullptr,&test::objs[3],bytes)==script_gate::Verdict::Run);
    CHECK(test::hook(nullptr,obj,bytes)==script_gate::Verdict::Cancel);
    if(test::nested) { test::nested=false;lightswitch::CallUse(&test::other);CHECK(lightswitch::g_replayedSwitch==obj); }
    return test::callOk;
}
}
namespace ue_wrap::engine {
void* SpawnSoundAttenuation(const SoundAttenuationConfig& c) {
    CHECK(c.attenuate && c.spatialize && c.extents[0]+c.falloffDistance<=1500.f);
    return test::attenuationOk ? &test::objs[7] : nullptr;
}
bool TryGetActorLocation(void*,FVector& out) { out={10,20,30};return test::locationOk; }
void PlaySoundAtLocation(void* obj,void* cue,const FVector& p,void* att,float vol,float pitch) {
    CHECK(obj==lightswitch::g_replayedSwitch && cue==test::sound.cue && att && p.X==10);
    ++test::clicks;test::vol=vol;test::pitch=pitch;
}
}
namespace ue_wrap::ufunction_hook {
bool InstallPreHook(void*,PreNativeCallback cb,bool armed) { CHECK(armed);test::hook=cb;return true; }
bool InstallPreHook(const facts::Resolved&,PreNativeCallback cb,bool armed) { CHECK(armed);test::hook=cb;return true; }
CallerFrame CurrentCallerFrame() { return {test::caller,nullptr}; }
}
namespace coop::props::prop_food_sync {bool IsLocalConsumedOutput(void* actor,void* fn){return test::foodOutput && actor==&test::sw && fn==test::caller;}}
namespace coop::prop_action_sync {bool IsLocalMreOutput(void* actor,void* fn){return test::mreOutput && actor==&test::sw && fn==test::caller;}}
namespace ue_wrap::script_gate {
bool Watch(void*,int,PreFn,PostFn) { return true; }
bool Watch(const facts::Resolved&,int,PreFn,PostFn) { return true; }
bool IsBodyActive(void*,void*) { return test::body; }
}
namespace ue_wrap::fname_utils {
reflection::FName StringToFName(const std::wstring& s) { for(auto& o:test::objs)if(s==o.text)return o.name;return {}; }
}
namespace ue_wrap::world_identity { uint32_t Generation(){return 1;} void* CurrentWorld(){return nullptr;} void* WorldOf(void*){return nullptr;} }
namespace ue_wrap::save_record {
std::wstring ReadFNameAt(const void*,int32_t){std::abort();}
bool ReadFloatGroup(const void*,size_t,std::vector<float>&){std::abort();}
bool ReplaceFloatGroup(void*,size_t,const std::vector<float>&){std::abort();}
}
namespace ue_wrap::object_index { void* ClassByName(const wchar_t*){return nullptr;} }
namespace ue_wrap::game_thread { bool IsDefinitelyOffGameThread() { return false; } }
namespace ue_wrap::hot_path { unsigned long CurrentThreadId(){return 0;} unsigned long long CallerRva(const void*){return 0;} }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace coop::hand_item {void* LocalHandActor(){return &test::sw;}}
namespace coop::bucket_sync {bool IsMaterialBirth(){return false;}}
namespace coop::kitchen_sync {bool IsLocalLidOutput(void* source){return test::lidOutput && source==&test::sw;}}
int main() {
    using namespace test;using namespace ue_wrap;
    objs[1].owner=&objs[0];objs[3].owner=&objs[2];objs[4].owner=&objs[0];objs[6].owner=&objs[5];
    using coop::prop_spawn_authoring::BirthIsPlayerAuthored;
    caller=&objs[1];CHECK(BirthIsPlayerAuthored());
    caller=&objs[4];CHECK(!BirthIsPlayerAuthored()); // same class, different function
    objs[1].owner=&objs[2];caller=&objs[1];CHECK(!BirthIsPlayerAuthored()); // same name, foreign class
    objs[1].owner=&objs[0];caller=&objs[3];CHECK(!BirthIsPlayerAuthored());
    objs[11].owner=&objs[10];caller=&objs[11];CHECK(BirthIsPlayerAuthored());
    objs[13].owner=&objs[12];caller=&objs[13];CHECK(BirthIsPlayerAuthored());
    objs[14].owner=&objs[12];caller=&objs[14];CHECK(!BirthIsPlayerAuthored()); // bed begin-play generation
    objs[13].owner=&objs[0];caller=&objs[13];CHECK(!BirthIsPlayerAuthored()); // another class's fold
    objs[11].owner=&objs[0];caller=&objs[11];CHECK(!BirthIsPlayerAuthored());
    objs[16].owner=&objs[15];caller=&objs[16];CHECK(BirthIsPlayerAuthored());
    objs[18].owner=&objs[17];caller=&objs[18];CHECK(BirthIsPlayerAuthored());
    objs[16].owner=&objs[0];caller=&objs[16];CHECK(!BirthIsPlayerAuthored());
    objs[18].owner=&objs[0];caller=&objs[18];CHECK(!BirthIsPlayerAuthored());
    objs[20].owner=&objs[19];caller=&objs[20];
    CHECK(!BirthIsPlayerAuthored(&sw));mreOutput=true;
    CHECK(BirthIsPlayerAuthored(&sw)); // exact local-hand MRE spawn source
    CHECK(!BirthIsPlayerAuthored());CHECK(!BirthIsPlayerAuthored(&other)); // world/loader source excluded
    objs[20].owner=&objs[0];CHECK(!BirthIsPlayerAuthored(&sw)); // foreign spawn body excluded
    objs[14].owner=&objs[19];caller=&objs[14];CHECK(!BirthIsPlayerAuthored(&sw)); // MRE class, other verb
    mreOutput=false;
    objs[22].owner=&objs[21];caller=&objs[22];
    CHECK(!BirthIsPlayerAuthored(&sw));foodOutput=true; // a held source alone has no output provenance
    CHECK(BirthIsPlayerAuthored(&sw)); // hand box's verified empty-wrapper spawn
    CHECK(!BirthIsPlayerAuthored(&other)&&!BirthIsPlayerAuthored());
    objs[22].owner=&objs[19];CHECK(!BirthIsPlayerAuthored(&sw)); // wrong class
    for(int i=38;i<42;i+=2){objs[i+1].owner=&objs[i];caller=&objs[i+1];
        CHECK(BirthIsPlayerAuthored(&sw));CHECK(!BirthIsPlayerAuthored(&other)&&!BirthIsPlayerAuthored());
        objs[i+1].owner=&objs[0];CHECK(!BirthIsPlayerAuthored(&sw));}
    foodOutput=false;
    caller=&objs[27];
    cookingSource=true;
    for(int i=23;i<27;++i){objs[27].owner=&objs[i];CHECK(BirthIsPlayerAuthored(&sw));CHECK(!BirthIsPlayerAuthored(&other)&&!BirthIsPlayerAuthored());}
    for(int i=28;i<38;++i){objs[27].owner=&objs[i];CHECK(BirthIsPlayerAuthored(&sw));CHECK(!BirthIsPlayerAuthored(&other));}
    cookingSource=false;CHECK(!BirthIsPlayerAuthored(&sw));cookingSource=true;
    objs[27].owner=&objs[2];CHECK(!BirthIsPlayerAuthored(&sw));
    caller=nullptr;CHECK(!BirthIsPlayerAuthored());
    objs[43].owner=&objs[42];caller=&objs[43];CHECK(!BirthIsPlayerAuthored(&sw));
    lidOutput=true;CHECK(BirthIsPlayerAuthored(&sw));CHECK(!BirthIsPlayerAuthored(&other));
    objs[43].owner=&objs[0];CHECK(!BirthIsPlayerAuthored(&sw));lidOutput=false;caller=nullptr;
    coop::prop_spawn_authoring::g_toolUberName=objs[6].name;
    caller=&objs[6];CHECK(BirthIsPlayerAuthored());
    coop::prop_spawn_authoring::g_menuSpawnFn=&objs[4];body=true;caller=nullptr;CHECK(BirthIsPlayerAuthored());body=false;
    lightswitch::g_useFn=&objs[9];
    CHECK(lightswitch::CallUse(&sw));CHECK(uses==1 && clicks==0); // transient attenuation failure
    attenuationOk=true;nested=true;
    CHECK(lightswitch::CallUse(&sw));CHECK(uses==3 && clicks==2 && !lightswitch::g_replayedSwitch);
    CHECK(vol==sound.volume && pitch==sound.pitch);
    CHECK(hook(nullptr,&sw,reinterpret_cast<const uint8_t*>(&sound))==script_gate::Verdict::Run); // local click untouched
    locationOk=false;CHECK(lightswitch::CallUse(&sw));CHECK(clicks==2);
    locationOk=true;callOk=false;CHECK(!lightswitch::CallUse(&sw));CHECK(!lightswitch::g_replayedSwitch);
    CHECK(!lightswitch::CallUse(nullptr));
    puts("PASS: real replay/ParamFrame, source filter, distance config, nested/fault cleanup, craft provenance and ambient rejection");
}

namespace ue_wrap::facts {
Resolved Resolve(const Function& row){
    void* cls=reflection::FindClass(row.Class());
    void* fn=cls?reflection::FindFunction(cls,row.Name()):nullptr;
    return fn?Resolved(fn,&row):Resolved{};
}
}
