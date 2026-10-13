#include "support_2135.h"
#include "../../src/votv-coop/src/coop/world/wall_build_sync.cpp"
namespace test { bool feedOK=true, die=false; int fed=0,changed=0,tracked=0; }
namespace ue_wrap::reflection {
int32_t FunctionFrameSize(void*){return 16;}
int32_t FindParamOffset(void*,const wchar_t*){return offsetof(test::Obj,flags);}
}
namespace test { uint8_t repairResult=0x80; }
namespace test { bool disconnectOnTrack=false; }
namespace ue_wrap::script_gate { uint8_t* OutParamPtr(const Call&,int32_t){return &test::repairResult;} }
namespace coop::placed_save_sync { void MarkChanged(void*){++test::changed;} void TrackWall(void*){++test::tracked;if(test::disconnectOnTrack)coop::wall_build_sync::OnDisconnect();} }
namespace ue_wrap::wall_build {
bool Feed(void* w,void*) {
    using namespace coop::wall_build_sync;
    script_gate::Call c{};c.object=w;c.fromOurCode=true;
    CHECK(BeforeOverlap(c)==script_gate::Verdict::Run);
    ++test::fed;if(test::die)test::ui.live=false;
    test::Callback();return test::feedOK;
}
}
int main() {
    using namespace test;namespace S=coop::wall_build_sync;using V=ue_wrap::script_gate::Verdict;
    coop::net::Session s;s.mode=coop::net::Role::Client;S::Install(&s);
    ue_wrap::script_gate::Call c{};c.object=&actor;
    uint8_t params[16]{};void* item=&ui;std::memcpy(params+8,&item,sizeof(item));c.locals=params;
    CHECK(S::BeforeOverlap(c)==V::Cancel&&S::g_pending.empty());
    CHECK(S::BeforeMutation(c)==V::Cancel);
    uint8_t repairParams[16]{};c.function=&actor;c.locals=repairParams;
    CHECK(S::BeforeRepair(c)==V::Cancel&&repairResult==0x81&&repairParams[offsetof(Obj,flags)]==1);
    CHECK(tracked==0);
    c.locals=params;
    s.mode=coop::net::Role::Host;
    CHECK(S::BeforeOverlap(c)==V::Cancel&&S::g_pending.size()==1);
    CHECK(tracked==1);
    CHECK(S::BeforeMutation(c)==V::Run&&tracked==2);
    CHECK(S::BeforeRepair(c)==V::Run&&tracked==3);
    S::BeforeOverlap(c);CHECK(S::g_pending.size()==1); // duplicate overlap before drain
    feedOK=false;S::Tick();CHECK(fed==1&&S::g_pending.size()==1);
    feedOK=true;S::Tick();CHECK(fed==2&&S::g_pending.empty());
    S::BeforeOverlap(c);++ui.serial;S::Tick();CHECK(fed==2); // recycled material
    S::BeforeOverlap(c);++world;S::Tick();CHECK(fed==2); // world swap
    S::BeforeOverlap(c);S::g_pending[0].until=0;S::Tick();CHECK(fed==2);
    S::BeforeOverlap(c);callback=[]{S::OnDisconnect();};S::Tick();CHECK(S::g_pending.empty());
    CHECK(S::BeforeMutation(c)==V::Run&&S::g_feeding==nullptr);
    S::Install(&s);S::BeforeOverlap(c);const auto previous=fed;s.linked=false;
    S::Tick();CHECK(fed==previous+1&&S::g_pending.empty()); // drain work from before last peer left
    CHECK(S::BeforeOverlap(c)==V::Run); // running host, no peers
    s.mode=coop::net::Role::Client;CHECK(S::BeforeOverlap(c)==V::Cancel); // still loading
    s.mode=coop::net::Role::Host;
    s.linked=true;c.locals=nullptr;CHECK(S::BeforeOverlap(c)==V::Cancel&&S::g_pending.empty());
    CHECK(S::BeforeMutation(c)==V::Run);
    disconnectOnTrack=true;
    CHECK(S::BeforeOverlap(c)==V::Cancel&&S::g_session==nullptr);
    S::Install(&s);CHECK(S::BeforeMutation(c)==V::Cancel);
    S::Install(&s);CHECK(S::BeforeRepair(c)==V::Cancel);
    disconnectOnTrack=false;
    S::Install(&s);actor.live=false;
    CHECK(S::BeforeMirrorOnly(c)==V::Run); // native host debris after kill flag
    s.mode=coop::net::Role::Client;CHECK(S::BeforeMirrorOnly(c)==V::Cancel);
    actor.live=true;
    puts("PASS wall authority: client refusal, host deferral/dedup, retry, serial/world invalidation and disconnect reentry");
}
