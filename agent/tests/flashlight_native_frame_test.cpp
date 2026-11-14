// Actual light setters and ParamFrame; only reflection/native dispatch are doubled.
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/sdk_profile.h"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "flashlight_native_production.inc"
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Object { int index, serial=1; bool live=true; } objects[]{{0},{1},{2},{3},{4},{5}};
int calls=0, frameSize=8;
bool dispatchOK=true, overflow=false;
std::wstring missing;
void* function=nullptr;
std::vector<uint8_t> bytes;
void Reset() {
    for (auto& object:objects) { ++object.serial; object.live=true; }
    calls=0; frameSize=8; dispatchOK=true; overflow=false; missing.clear(); bytes.clear();
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p) { return p && static_cast<test::Object*>(p)->live; }
int32_t InternalIndexOf(void* p) { return p?static_cast<test::Object*>(p)->index:-1; }
void* ObjectAt(int32_t i) { return i>=0 && i<6?&test::objects[i]:nullptr; }
int32_t SlotSerial(int32_t i) { return i>=0 && i<6?test::objects[i].serial:0; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
void* FindClass(const wchar_t*) { return &test::objects[1]; }
void* FindFunction(void*,const wchar_t* name) {
    const wchar_t* names[]{L"SetIntensity",L"SetVisibility",L"SetOuterConeAngle",L"SetInnerConeAngle"};
    for (int i=0;i<4;++i) if (!std::wcscmp(name,names[i])) return &test::objects[i+2];
    return nullptr;
}
int32_t FunctionFrameSize(void*) { return test::frameSize; }
std::vector<ParamInfo> FunctionParams(void* fn) {
    std::vector<ParamInfo> params;
    auto add=[&](const wchar_t* name,int offset) {
        if (test::missing==name) return;
        ParamInfo p{}; p.name=name; p.offset=test::overflow?test::frameSize:offset; params.push_back(p);
    };
    if (fn==&test::objects[2]) add(L"NewIntensity",0);
    if (fn==&test::objects[3]) { add(L"bNewVisibility",0); add(L"bPropagateToChildren",1); }
    if (fn==&test::objects[4]) add(L"NewOuterConeAngle",0);
    if (fn==&test::objects[5]) add(L"NewInnerConeAngle",0);
    return params;
}
bool CallFunction(void*,void* fn,void* data) {
    ++test::calls; test::function=fn;
    test::bytes.assign(static_cast<uint8_t*>(data),static_cast<uint8_t*>(data)+test::frameSize);
    return test::dispatchOK;
}
const FName& NameOf(void*) { static FName name{}; return name; }
std::wstring ToString(const FName&) { return L"test light setter"; }
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
int main() {
    using namespace test;
    using namespace ue_wrap::engine;
    const auto run=[](int setter) {
        if (setter==0) return SetLightIntensity(&objects[0],7.5f);
        if (setter==1) return SetSceneComponentVisibility(&objects[0],true,true);
        if (setter==2) return SetSpotLightOuterConeAngle(&objects[0],45.f);
        return SetSpotLightInnerConeAngle(&objects[0],20.f);
    };
    const wchar_t* required[]{L"NewIntensity",L"bNewVisibility",L"NewOuterConeAngle",L"NewInnerConeAngle"};
    for (int setter=0;setter<4;++setter) {
        Reset(); CHECK(run(setter) && calls==1 && function==&objects[setter+2]);
        if (setter==1) CHECK(bytes[0]==1 && bytes[1]==1);
        else { float value=0; std::memcpy(&value,bytes.data(),4); CHECK(value==(setter==0?7.5f:setter==2?45.f:20.f)); }
        Reset(); missing=required[setter]; CHECK(!run(setter) && calls==0);
        Reset(); overflow=true; CHECK(!run(setter) && calls==0);
        Reset(); frameSize=-1; CHECK(!run(setter) && calls==0);
        Reset(); frameSize=0; CHECK(!run(setter) && calls==0);
        Reset(); dispatchOK=false; CHECK(!run(setter) && calls==1);
        Reset(); objects[0].live=false; CHECK(!run(setter) && calls==0);
    }
    Reset(); missing=L"bPropagateToChildren";
    CHECK(!SetSceneComponentVisibility(&objects[0],true,true) && calls==0);
    CHECK(!SetLightIntensity(nullptr,5) && calls==0);
    CHECK(!SetSceneComponentVisibility(nullptr,true,false) && calls==0);
    CHECK(!SetSpotLightOuterConeAngle(nullptr,45) && calls==0);
    CHECK(!SetSpotLightInnerConeAngle(nullptr,20) && calls==0);
    std::puts("PASS actual flashlight setters/ParamFrame: exact bytes, missing and overflowing parameters, invalid frames, dead targets and native failure");
}
