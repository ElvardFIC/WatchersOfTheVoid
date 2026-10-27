// Execute the production readers against relocated fields and packed flags.
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/reflection.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "flashlight_reader_production.inc"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Object {void* light=nullptr;float intensity=.2f,outer=50,inner=0;uint8_t flags=0,mode=1;bool live=true;} pawn,light;
std::wstring missing;
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Object*>(p)->live;}
void* ClassOf(void* p){return p;}
int32_t InstanceOffset::Of(void*) {
    if(test::missing==prop_)return -1;
    if(!std::wcscmp(prop_,L"light_R"))return offsetof(test::Object,light);
    if(!std::wcscmp(prop_,L"Intensity"))return offsetof(test::Object,intensity);
    if(!std::wcscmp(prop_,L"OuterConeAngle"))return offsetof(test::Object,outer);
    if(!std::wcscmp(prop_,L"InnerConeAngle"))return offsetof(test::Object,inner);
    if(!std::wcscmp(prop_,L"flashlightMode"))return offsetof(test::Object,mode);
    return -1;
}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& offset,uint8_t& mask) {
    if(test::missing==name)return false;
    offset=offsetof(test::Object,flags);
    if(!std::wcscmp(name,L"bVisible"))mask=1;
    else if(!std::wcscmp(name,L"flashlight"))mask=2;
    else if(!std::wcscmp(name,L"hasFlashlight"))mask=4;
    else if(!std::wcscmp(name,L"crankFlashlight"))mask=8;
    else return false;
    return true;
}
}
int main(){
    using namespace test;using namespace ue_wrap::engine;
    pawn.light=&light;pawn.flags=0xEE;light.flags=1;
    FlashlightSnapshot snapshot{};MainPlayerFlashlightState state{};
    CHECK(GetMainPlayerLightR(&pawn)==&light);
    CHECK(ReadFlashlightSnapshot(&light,snapshot) && snapshot.visible && snapshot.intensity==.2f && snapshot.outerConeAngle==50);
    CHECK(ReadMainPlayerFlashlightState(&pawn,state) && state.flashlight && state.hasFlashlight && state.crankFlashlight && state.mode==1);
    light.flags=0x10;CHECK(ReadFlashlightSnapshot(&light,snapshot) && !snapshot.visible);
    CHECK(WriteMainPlayerFlashlight(&pawn,false) && pawn.flags==0xEC);
    CHECK(WriteMainPlayerFlashlight(&pawn,true) && pawn.flags==0xEE);
    for(const auto* name:{L"Intensity",L"OuterConeAngle",L"InnerConeAngle",L"bVisible"}){
        missing=name;snapshot={9,8,7,true};CHECK(!ReadFlashlightSnapshot(&light,snapshot));
        CHECK(snapshot.intensity==9 && snapshot.outerConeAngle==8 && snapshot.innerConeAngle==7 && snapshot.visible);
    }
    for(const auto* name:{L"flashlight",L"hasFlashlight",L"crankFlashlight",L"flashlightMode"}){
        missing=name;state={false,false,false,7};CHECK(!ReadMainPlayerFlashlightState(&pawn,state));
        CHECK(!state.flashlight && !state.hasFlashlight && !state.crankFlashlight && state.mode==7);
    }
    missing=L"flashlight";CHECK(!WriteMainPlayerFlashlight(&pawn,false) && pawn.flags==0xEE);
    missing=L"light_R";CHECK(!GetMainPlayerLightR(&pawn));missing.clear();
    light.live=false;CHECK(!GetMainPlayerLightR(&pawn) && !ReadFlashlightSnapshot(&light,snapshot));
    pawn.live=false;CHECK(!ReadMainPlayerFlashlightState(&pawn,state) && !WriteMainPlayerFlashlight(&pawn,true));
    puts("PASS production flashlight readers: relocated fields, packed flags, non-destructive unresolved reads and writes");
}
