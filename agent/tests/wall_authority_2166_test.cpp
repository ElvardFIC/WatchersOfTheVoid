#include "support_2135.h"
#include "../../src/votv-coop/src/coop/world/wall_build_sync.cpp"
namespace coop::wall_creative_sync {
void Install(net::Session*){}void Tick(){}void OnDisconnect(){}
bool NativeCurrent(void*){return true;}
ue_wrap::script_gate::Verdict BeforeBuild(const ue_wrap::script_gate::Call&){return ue_wrap::script_gate::Verdict::Cancel;}
}
namespace test {
bool feedOK=true, die=false; int fed=0,changed=0,tracked=0;
std::function<void()> queryCallback,trackCallback;
ue_wrap::script_gate::Verdict lastFeed=ue_wrap::script_gate::Verdict::Run;
}
namespace ue_wrap::reflection {
int32_t FunctionFrameSize(void*){return 16;}
int32_t FindParamOffset(void*,const wchar_t*){return offsetof(test::Obj,flags);}
}
namespace test { uint8_t repairResult=0x80; }
namespace test { bool disconnectOnTrack=false; }
namespace ue_wrap::script_gate { uint8_t* OutParamPtr(const Call&,int32_t){return &test::repairResult;} }
namespace coop::placed_save_sync {
void MarkChanged(void*){++test::changed;}
void TrackWall(void*){
 ++test::tracked;if(test::disconnectOnTrack)coop::wall_build_sync::OnDisconnect();
 auto callback=std::move(test::trackCallback);test::trackCallback={};if(callback)callback();
}
}
namespace ue_wrap::wall_build {
bool Feed(void* w,void* material) {
    using namespace coop::wall_build_sync;
    script_gate::Call c{};c.object=w;c.fromOurCode=true;c.function=&test::actor;
    uint8_t params[16]{};std::memcpy(params+8,&material,sizeof(material));c.locals=params;
    auto query=std::move(test::queryCallback);test::queryCallback={};if(query)query();
    test::lastFeed=BeforeOverlap(c);
    if(test::lastFeed==script_gate::Verdict::Cancel)return true;
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
    auto queue=[&](void* wall,void* material,bool ours=false) {
        ue_wrap::script_gate::Call event{};event.object=wall;event.function=&actor;event.fromOurCode=ours;
        uint8_t args[16]{};std::memcpy(args+8,&material,sizeof(material));event.locals=args;
        return S::BeforeOverlap(event);
    };
    auto reset=[&] {
        S::OnDisconnect();s.mode=coop::net::Role::Host;s.active=s.linked=true;
        actor.live=ui.live=true;feedOK=true;callback=queryCallback=trackCallback={};
        fed=0;lastFeed=V::Run;S::Install(&s);
    };
    // An overlap emitted while native code consumes material must not enqueue
    // that same wall/material pair again, including pairs not processed yet.
    reset();queue(&actor,&ui);
    callback=[&]{CHECK(queue(&actor,&ui)==V::Cancel);S::Tick();};
    S::Tick();S::Tick();CHECK(fed==1&&S::g_pending.empty()&&!S::g_draining);
    reset();queue(&actor,&ui);queue(&ui,&actor);
    callback=[&]{CHECK(queue(&ui,&actor)==V::Cancel);};
    S::Tick();S::Tick();CHECK(fed==2&&S::g_pending.empty());
    reset();feedOK=false;queue(&actor,&ui);
    callback=[&]{CHECK(queue(&actor,&ui)==V::Cancel);};
    S::Tick();CHECK(fed==1&&S::g_pending.size()==1);
    feedOK=true;S::Tick();CHECK(fed==2&&S::g_pending.empty());
    puts("PASS material overlap dedup: in-flight and unprocessed pairs stay visible, nested ticks are refused and failed feeds retain exactly one retry");

    // Only the exact material may consume the replay permit. Another overlap
    // raised from enrollment is queued for the next tick.
    reset();Obj extra{3};queue(&actor,&ui);
    trackCallback=[&]{CHECK(queue(&actor,&extra,true)==V::Cancel);};
    S::Tick();CHECK(fed==1&&S::g_pending.size()==1);
    S::Tick();CHECK(fed==2&&S::g_pending.empty());
    for(int reason=0;reason<4;++reason) {
        reset();queue(&actor,&ui);
        queryCallback=[&,reason]{
            if(reason==0)S::OnDisconnect();
            if(reason==1)++world;
            if(reason==2)s.active=false;
            if(reason==3)s.mode=coop::net::Role::Client;
            CHECK(S::BeforeMutation(c)==V::Cancel);
            ue_wrap::script_gate::Call repair{};repair.object=&actor;repair.function=&actor;
            uint8_t repairFrame[16]{};repair.locals=repairFrame;repairResult=0x80;
            CHECK(S::BeforeRepair(repair)==V::Cancel&&repairResult==0x81&&repairFrame[offsetof(Obj,flags)]==1);
            const int published=changed;S::Changed(c);CHECK(changed==published);
        };
        S::Tick();CHECK(fed==0&&lastFeed==V::Cancel&&S::g_pending.empty()&&!S::g_draining);
        CHECK(!S::g_feeding&&!S::g_allowFeed&&!S::g_feedMaterial.Raw());
    }
    reset();queue(&actor,&ui);queue(&ui,&actor);
    callback=[&]{++world;};S::Tick();CHECK(fed==1&&S::g_pending.empty());
    reset();queryCallback=[&]{S::OnDisconnect();S::Install(&s);S::Tick();};queue(&actor,&ui);
    S::Tick();CHECK(fed==0&&lastFeed==V::Cancel&&S::g_pending.empty());
    puts("PASS wall replay context: exact material permit, deferred callback additions, world/session/role shutdown and reconnect cannot resume stale offline spending");

    reset();Obj materials[10];
    for(auto& material:materials){material.id=2;CHECK(queue(&actor,&material)==V::Cancel);}
    S::Tick();CHECK(fed==8&&S::g_pending.size()==2);S::Tick();CHECK(fed==10&&S::g_pending.empty());
    reset();queue(&actor,&ui);queryCallback=[&]{s.linked=false;};
    S::Tick();CHECK(fed==1&&S::g_pending.empty()); // Last-peer departure keeps native host ownership.
    S::OnDisconnect();CHECK(S::BeforeMutation(c)==V::Run&&S::BeforeRepair(c)==V::Run);
    puts("PASS wall authority: client refusal, host deferral/dedup, retry, serial/world invalidation and disconnect reentry");
}
