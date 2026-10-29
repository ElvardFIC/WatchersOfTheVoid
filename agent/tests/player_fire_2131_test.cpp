// Real native-fire wrapper, canRagdoll accessors and real zero-argument ParamFrame.
#include "../../src/votv-coop/src/ue_wrap/engine/engine_mainplayer.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Pawn { float fuel=50; uint8_t flags=0xFF; bool live=true; } pawn;
int fn=1,cls=2,calls=0,serial=1;
bool verb=true,fields=true,callOk=true,clearFuel=true,clearFlag=true,kill=false;
int frameSize=0;
}
namespace ue_wrap::reflection {
bool IsLive(void* p) { return p && static_cast<test::Pawn*>(p)->live; }
void* ClassOf(void*) { return &test::cls; }
void* FindDispatchFunctionCached(void*,const wchar_t* n) {
    CHECK(std::wstring(n)==L"extinguishFire");return test::verb ? &test::fn : nullptr;
}
int32_t FindPropertyOffset(void*,const wchar_t* n) { CHECK(std::wstring(n)==L"burningTime");return test::fields ? offsetof(test::Pawn,fuel) : -1; }
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask) {
    off=offsetof(test::Pawn,flags);mask=std::wstring(n)==L"canRagdoll" ? 0x20 : 0x04;return test::fields;
}
int32_t InternalIndexOf(void*) { return 1; }
int32_t SlotSerial(int32_t) { return test::serial; }
int32_t AllocateSlotSerial(int32_t) { return test::serial; }
void* ObjectAt(int32_t) { return &test::fn; }
int32_t FunctionFrameSize(void*) { return test::frameSize; }
std::vector<ParamInfo> FunctionParams(void*) { return {}; }
bool CallFunction(void* p,void* fn,void* params) {
    CHECK(fn==&test::fn && params==nullptr);++test::calls;
    auto& a=*static_cast<test::Pawn*>(p);
    if (!test::callOk) return false;
    if (test::clearFuel) a.fuel=0;
    if (test::clearFlag) a.flags &= ~0x04;
    if (test::kill) a.live=false;
    return true;
}
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
// Unexercised neighbours in the same real TU must never silently act as mocks.
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*) { std::abort(); }
std::vector<StructFieldInfo> EnumerateStructFields(void*) { std::abort(); }
void EngineFree(void*) { std::abort(); }
int32_t InstanceOffset::Of(void*) { std::abort(); }
void* PropertyInnerStruct(void*,const wchar_t*) { std::abort(); }
bool IsLiveByIndex(void*,int32_t) { std::abort(); }
void* ResolveWeakObject(int32_t,int32_t) { std::abort(); }
void* FindObject(const wchar_t*,const wchar_t*) { std::abort(); }
void* FindClass(const wchar_t*) { std::abort(); }
void* FindFunction(void*,const wchar_t*) { std::abort(); }
}
namespace ue_wrap::world_identity {
void* CurrentWorld() { std::abort(); }
void* WorldOf(void*) { std::abort(); }
}
namespace ue_wrap::engine { FVector GetComponentLocation(void*) { std::abort(); } }
namespace ue_wrap::physics_gun { void* HeldActor(void*) { std::abort(); } }
namespace ue_wrap::hit_result { bool WriteField(void*,const wchar_t*,void*,void*,const FVector&) { std::abort(); } }
namespace ue_wrap::reflected_offset {
#define UNUSED_OFFSET(n) int32_t n() { std::abort(); }
UNUSED_OFFSET(MainPlayer_heavyGrab)
UNUSED_OFFSET(MainPlayer_droppedItem)
UNUSED_OFFSET(MainPlayer_grabHandle)
UNUSED_OFFSET(MainPlayer_grabTimeline)
UNUSED_OFFSET(MainPlayer_grabbing_component)
UNUSED_OFFSET(MainPlayer_grabbing_actor)
UNUSED_OFFSET(MainPlayer_grabsHeavy)
UNUSED_OFFSET(MainPlayer_grabLen)
UNUSED_OFFSET(MainPlayer_Camera)
UNUSED_OFFSET(MainPlayer_armLength)
UNUSED_OFFSET(MainPlayer_Heavy)
UNUSED_OFFSET(MainPlayer_holding_actor)
UNUSED_OFFSET(MainPlayer_lookAtActor)
UNUSED_OFFSET(MainPlayer_lookAtComponent)
UNUSED_OFFSET(MainPlayer_lookAtBoundsReplace)
UNUSED_OFFSET(MainPlayer_lookAtVerify)
UNUSED_OFFSET(MainPlayer_lookAtState)
UNUSED_OFFSET(MainPlayer_releaseEToUse)
UNUSED_OFFSET(MainPlayer_actionIndex)
UNUSED_OFFSET(MainPlayer_isRagdoll)
UNUSED_OFFSET(MainPlayer_dead)
#undef UNUSED_OFFSET
}
int main() {
    using namespace test;using namespace ue_wrap::engine;
    CHECK(!ExtinguishMainPlayerFire(nullptr));
    pawn.live=false;CHECK(!ExtinguishMainPlayerFire(&pawn));pawn.live=true;
    verb=false;CHECK(!ExtinguishMainPlayerFire(&pawn));verb=true;
    fields=false;CHECK(!ExtinguishMainPlayerFire(&pawn));fields=true;CHECK(calls==0);
    callOk=false;CHECK(!ExtinguishMainPlayerFire(&pawn));callOk=true;
    clearFuel=false;CHECK(!ExtinguishMainPlayerFire(&pawn));clearFuel=true;
    pawn.flags=0xFF;clearFlag=false;CHECK(!ExtinguishMainPlayerFire(&pawn));clearFlag=true;
    CHECK(ExtinguishMainPlayerFire(&pawn));CHECK(pawn.fuel==0 && pawn.flags==0xFB);
    CHECK(ExtinguishMainPlayerFire(&pawn)); // already extinguished is harmless
    kill=true;CHECK(!ExtinguishMainPlayerFire(&pawn));kill=false;pawn.live=true;
    ++serial;frameSize=-1;CHECK(!ExtinguishMainPlayerFire(&pawn));
    ++serial;frameSize=0;CHECK(ExtinguishMainPlayerFire(&pawn));
    pawn.flags=0xFF;CHECK(SetMainPlayerCanRagdoll(&pawn,false));CHECK(pawn.flags==0xDF);
    bool allowed=true;CHECK(ReadMainPlayerCanRagdoll(&pawn,allowed) && !allowed);
    CHECK(SetMainPlayerCanRagdoll(&pawn,true));CHECK(pawn.flags==0xFF);
    std::puts("PASS: real fire wrapper/ParamFrame, missing/dead targets, failed dispatch/readback, idempotence, bool-bit preservation");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
const FName& NameOf(void*){static const FName name{};return name;}
}
