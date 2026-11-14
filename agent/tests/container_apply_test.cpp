#define CONTAINER_APPLY_HARNESS
#define main inventory_baseline_main
#include "inventory_apply_test.cpp"
#undef main
#include "../../src/votv-coop/src/coop/props/container_apply_scope.h"
#include "coop/props/container_seed_retry.h"
#include "coop/props/container_birth.h"
#include "ue_wrap/actors/container_view.h"
#ifndef CONTAINER_PARK_SOURCE
#define CONTAINER_PARK_SOURCE "../../src/votv-coop/src/coop/props/container_park.cpp"
#endif
#include CONTAINER_PARK_SOURCE
#include <atomic>


namespace wc {
coop::net::Session session;
uint32_t serial=1;bool live=true;int volume=0,names=0,views=0,completed=0;
std::function<void()> onVolume,onNames,onView,onFlags;
}
namespace coop::net::session_serial {uint32_t Current(){return wc::serial;}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n) {
    if(!wcscmp(n,L"propInventory_C"))return &test::objects[13];
    if(!wcscmp(n,L"prop_container_C"))return &test::objects[12];
    return reflection::FindClass(n);
}}
namespace ue_wrap::reflection {void* FindDispatchFunctionCached(void*,const wchar_t* name){return &test::objects[!wcscmp(name,L"updateVolumesAndMass")?26:27];}}
namespace ue_wrap::component_calls {bool CallParamless(void*,void* fn){if(fn==&test::objects[26]){++wc::volume;test::Callback(wc::onVolume);}else{++wc::names;test::Callback(wc::onNames);}return true;}}
namespace ue_wrap::container_view {bool Refresh(void*){++wc::views;test::Callback(wc::onView);return true;}}
namespace coop::props::container_contents_sync {
namespace {
namespace R=ue_wrap::reflection;namespace SR=ue_wrap::save_record;namespace ci=ue_wrap::container_inventory;
namespace cw {constexpr uint8_t kKnownFlags=3;}
namespace ca {bool ApplyFlags(uint32_t,void*,uint8_t){test::Callback(wc::onFlags);return true;}}
struct Seen {uint64_t records;uint8_t flags;};
constexpr uint8_t kFlagsUnapplied=255;
std::atomic<net::Session*> g_session{nullptr};uint64_t g_applyEpoch=0;
std::map<uint32_t,Seen> g_applied;
std::map<uint32_t,detail::AppliedBinding> g_appliedObject;
std::set<uint32_t> g_dirty;
void Touch(uint32_t) {}
namespace internal {void MarkDirty(uint32_t eid){g_dirty.insert(eid);}}
bool IsHost(){return g_session.load()&&g_session.load()->role()==net::Role::Host;}
void* LivePropActor(uint32_t eid){return wc::live&&eid==7?&test::objects[2]:nullptr;}
#ifndef CONTAINER_APPLY_FUNCTIONS
#define CONTAINER_APPLY_FUNCTIONS "container_apply_production.inc"
#endif
#include CONTAINER_APPLY_FUNCTIONS
}
}
namespace S=coop::props::container_contents_sync;
namespace pk=coop::props::container_park;
namespace wc {
void Reset() {
    test::Reset();session={};session.mode=coop::net::Role::Client;session.peers[1]=session.peers[2]=true;session.generations.fill(1);
    serial=1;live=true;volume=names=views=completed=0;onVolume=onNames=onView=onFlags={};
    test::Set(&test::objects[3],0,int32_t(1));test::Set(&test::objects[3],4,uint8_t(0x20));
    S::g_session=&session;++S::g_applyEpoch;S::g_applied.clear();S::g_appliedObject.clear();S::g_dirty.clear();pk::Reset();
}
S::Ingest Apply(int amount,uint8_t author=0) {
    auto recs=test::Rich().inventory;recs[0].ints={{amount}};
    auto epoch=S::g_applyEpoch;
    S::detail::ApplySource source(&session,7,author,[=]{return S::g_applyEpoch==epoch;});
    source.Claim();
    return S::ApplyContents(7,recs,uint64_t(amount)+1000,uint8_t{1},&source,[]{++completed;});
}
SR::SaveRecord Item(){auto a=SR::ReadArr(test::stack+16,0);CHECK(a.num==1);SR::SaveRecord r;SR::ReadSaveRecord(a.data,r);return r;}
int replayed=0;
bool ReplaceDuringReplay(const std::vector<uint8_t>& blob,uint8_t author){++replayed;auto original=blob;pk::Admit(7,author,{99},1000);CHECK(blob==original);return true;}
bool ResetDuringReplay(const std::vector<uint8_t>& blob,uint8_t){auto old=blob;pk::Reset();pk::Admit(8,0,{42},1000);CHECK(blob==old);return true;}
bool NestedSweep(const std::vector<uint8_t>&,uint8_t){++replayed;pk::Sweep(NestedSweep,1000);return false;}
}
int main() {
    using namespace wc;using O=S::Ingest;
    Reset();CHECK(Apply(7)==O::Applied&&Item().ints[0][0]==7&&completed==1);
    int allocations=test::allocations;CHECK(Apply(7)==O::Handled&&test::allocations==allocations&&completed==1);
    for(int at=1;at<=allocations;++at){Reset();auto before=SR::ReadArr(test::stack+16,0);test::failAt=at;
        CHECK(Apply(7)==O::Park&&SR::ReadArr(test::stack+16,0).data==before.data&&S::g_applied.empty()&&completed==0);
        test::failAt=0;CHECK(Apply(7)==O::Applied&&Item().ints[0][0]==7);}
    for(int mode=0;mode<4;++mode){Reset();if(mode==0)test::classesReady=false;if(mode==1)test::namesReady=false;if(mode==2)test::stringsReady=false;if(mode==3)test::signalsReady=false;
        CHECK(Apply(7)==O::Park&&S::g_applied.empty()&&completed==0);if(mode==0)CHECK(test::allocations==0);}
    Reset();test::Set(&test::objects[3],4,uint8_t(0x24));CHECK(Apply(7)==O::Handled&&completed==0);
    Reset();test::Set(&test::objects[3],0,int32_t(0));CHECK(Apply(7)==O::Park&&completed==0&&SR::ReadArr(test::stack,0).data==test::oldItem);
    for(int mode=0;mode<3;++mode){Reset();if(mode==0){int32_t bad=-1;std::memcpy(test::stack+24,&bad,4);}if(mode==1){int32_t max=0;std::memcpy(test::stack+28,&max,4);}if(mode==2){void* ptr=nullptr;std::memcpy(test::stack+16,&ptr,8);}std::vector<SR::SaveRecord> out;int count=100;CHECK(!C::ReadContents(&test::objects[3],out,128,&count)&&count==-1&&out.empty());}
    puts("PASS actual container apply/codec: nested allocation and readiness failures retain live contents; masked personal boundary and checked headers");
    for(int change=0;change<11;++change){Reset();test::onNative=[change]{
        if(change==0)++test::world;if(change==1)++serial;if(change==2)++test::objects[2].serial;if(change==3)++test::objects[3].serial;
        if(change==4)++S::g_applyEpoch;if(change==5)session.mode=coop::net::Role::Host;if(change==6)++session.generations[0];
        if(change==7)test::Set(&test::objects[0],0,static_cast<void*>(&test::objects[4]));
        if(change==8)test::Set(&test::objects[2],0,static_cast<void*>(&test::objects[5]));
        if(change==9)test::Set(&test::objects[3],0,int32_t(0));if(change==10)S::detail::ApplySource::Changed(7);
    };CHECK(Apply(7)==O::Handled&&S::g_applied.empty()&&completed==0);}
    Reset();test::onNative=[]{SR::WriteArrHeader(test::stack+16,0,nullptr,0);};CHECK(Apply(7)==O::Handled&&SR::ReadArr(test::stack+16,0).num==0);
    Reset();test::onNative=[]{CHECK(Apply(9)==O::Applied);};CHECK(Apply(7)==O::Handled&&Item().ints[0][0]==9&&completed==1);
    Reset();onNames=[]{++test::world;};CHECK(Apply(7)==O::Handled&&volume==0&&views==0&&completed==1);
    Reset();onView=[]{++test::world;};CHECK(Apply(7)==O::Handled&&completed==1);
    for(int incarnation=0;incarnation<3;++incarnation){Reset();CHECK(Apply(7)==O::Applied);if(incarnation<2)++test::objects[2+incarnation].serial;SR::WriteArrHeader(test::stack+16,0,test::foreignItem,1);CHECK(Apply(7)==O::Applied&&Item().ints[0][0]==7);}
    Reset();onFlags=[]{++test::world;};CHECK(Apply(7)==O::Handled&&completed==1&&names==0&&views==0);
    puts("PASS actual apply lifetime: newer input, sender/session/world/actor/component/store retirement, checked dedup binding, commit before callbacks and arming cancellation");
    Reset();replayed=0;pk::Admit(7,1,{1,2,3},1000);auto version=pk::Revision(7);pk::Sweep(ReplaceDuringReplay,1000);CHECK(pk::Size()==1&&pk::Revision(7)!=version&&replayed==1);
    pk::Retire(7,version);CHECK(pk::Size()==1);pk::Sweep(ResetDuringReplay,1000);CHECK(pk::Size()==1&&pk::Revision(8));
    pk::Reset();pk::Admit(7,0,{1},1000);replayed=0;pk::Sweep(NestedSweep,999);CHECK(pk::Size()==1&&replayed==1);
    CHECK(pk::RunSelftest());
    coop::props::ContainerSeedRetries retries;retries.Queue(7,31,2);int sent=0;
    retries.Drain([&](uint32_t eid,int slot,uint32_t gen){CHECK(eid==7&&slot==31&&gen==2);++sent;return false;});CHECK(retries.Size()==1);
    retries.Drain([&](uint32_t,int,uint32_t){return true;});CHECK(retries.Size()==0&&sent==1);
    puts("PASS park replacement/reset/reentry and exact-recipient seed refusal/retry at slot 31");
    Reset();
}
