#define CONTAINER_APPLY_HARNESS
#define main inventory_baseline_main
#include "inventory_apply_test.cpp"
#undef main
#include "../../src/votv-coop/src/coop/props/container_apply_scope.h"
#include "coop/props/container_seed_retry.h"
#include "coop/props/container_write_policy.h"
#include "coop/props/container_birth.h"
#include "ue_wrap/actors/container_view.h"
#ifndef CONTAINER_PARK_SOURCE
#define CONTAINER_PARK_SOURCE "../../src/votv-coop/src/coop/props/container_park.cpp"
#endif
#include CONTAINER_PARK_SOURCE
#include <atomic>

namespace wc {
coop::net::Session session;
uint32_t serial=1;uint64_t now=1000;
bool live=true,birth=false;int volume=0,names=0,views=0,completed=0,corrections=0;
std::vector<int> sends;
std::function<void()> onJudge,onVolume,onNames,onView;
coop::props::container_write_policy::Decision decision=coop::props::container_write_policy::Decision::Accept;
struct Packet {uint32_t eid=7;uint64_t base=1;std::vector<SR::SaveRecord> records;};
std::map<uint8_t,Packet> packets;uint8_t next=0;
std::map<uint32_t,uint64_t> published;
std::vector<uint8_t> Blob(int amount) {auto r=test::Rich().inventory;r[0].ints={{amount}};uint8_t id=++next;packets[id]={7,1,r};return {id};}
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
namespace coop::blob_chunks {
bool SendBlobToSlotWhile(net::Session* s,int slot,net::ReliableKind,uint32_t,const std::vector<uint8_t>&,const std::function<bool()>& current) {
    if(!current())return false;const bool ok=s->sendOK;if(ok)wc::sends.push_back(slot);
    test::Callback(s->duringSend);return ok&&current();
}}
namespace coop::props::container_write_policy {
Decision SpendArrival(uint32_t,uint8_t,uint64_t,net::Session&,Source,bool){return Decision::Accept;}
Decision Accept(uint32_t,uint64_t,uint8_t,uint64_t,net::Session&,Source){test::Callback(wc::onJudge);return wc::decision;}
void NotePublished(uint32_t eid,uint64_t hash){wc::published[eid]=hash;}
}
namespace coop::props::container_birth {bool Awaited(void*,uint8_t){return wc::birth;}void Complete(void*){++wc::completed;wc::birth=false;}}

namespace coop::props::container_contents_sync {
namespace {
namespace R=ue_wrap::reflection;namespace SR=ue_wrap::save_record;namespace ci=ue_wrap::container_inventory;
namespace wp=coop::props::container_write_policy;namespace cb=coop::props::container_birth;namespace pk=coop::props::container_park;
namespace cw {
constexpr size_t kMaxRecords=128;
bool ParseHeader(const std::vector<uint8_t>& blob,size_t& off,uint32_t& eid,uint64_t& base) {if(blob.size()!=1||!wc::packets.count(blob[0]))return false;auto& p=wc::packets.at(blob[0]);off=1;eid=p.eid;base=p.base;return true;}
bool ParseRecords(const std::vector<uint8_t>& blob,size_t,std::vector<SR::SaveRecord>& out,const char**){out=wc::packets.at(blob[0]).records;return true;}
uint64_t ContentHash(uint32_t,const std::vector<SR::SaveRecord>& records){return records.empty()?1000:uint64_t(records[0].ints[0][0])+1000;}
}
std::atomic<net::Session*> g_session{nullptr};uint64_t g_applyEpoch=0;
std::map<uint32_t,uint64_t> g_appliedHash,g_baseHash,g_sentHash;
std::map<uint32_t,detail::AppliedBinding> g_appliedObject;
std::set<uint32_t> g_dirty,g_retry;
ContainerSeedRetries g_seedRetry;std::array<uint32_t,net::kMaxPeers> g_seedScans{};
uint32_t g_nextSeq=0;
bool IsHost(){return g_session.load()&&g_session.load()->role()==net::Role::Host;}
uint64_t NowMs(){return wc::now;}
void* LivePropActor(uint32_t eid){return wc::live&&eid==7?&test::objects[2]:nullptr;}
void* ContainerClass(){return &test::objects[12];}
bool IsContainerActor(void* a){return ci::IsContainer(a);}
void QueueConnectBroadcastForSlot(int){std::abort();}
bool BroadcastContainer(net::Session* s,uint32_t,void*,int slot,bool){++wc::corrections;if(s->sendOK)wc::sends.push_back(slot);return s->sendOK;}
enum class SliceRead {Ready,Unreadable,OverCap};
constexpr uint64_t kUnsentWarnMs=5000;
bool RecordIsNestedContainer(const SR::SaveRecord& r){return ci::IsContainerRecord(r);}
void NeuterNestedIndex(SR::SaveRecord& r){ci::ClearInventoryIndex(r);}
struct Assembler {bool OnChunk(const net::BlobChunkPayload& p,uint8_t,std::vector<uint8_t>& out){out={uint8_t(p.blobSeq)};return true;}} g_asm;
#ifndef CONTAINER_APPLY_FUNCTIONS
#define CONTAINER_APPLY_FUNCTIONS "container_apply_production.inc"
#endif
#include CONTAINER_APPLY_FUNCTIONS
}
}
namespace S=coop::props::container_contents_sync;
namespace pk=coop::props::container_park;
namespace wc {
void Reset(coop::net::Role role=coop::net::Role::Host) {
    test::Reset();session={};session.mode=role;session.peers[1]=session.peers[2]=true;session.generations.fill(1);
    serial=1;now=1000;live=true;birth=false;volume=names=views=completed=corrections=0;sends.clear();packets.clear();next=0;published.clear();
    onJudge=onVolume=onNames=onView={};decision=coop::props::container_write_policy::Decision::Accept;
    test::Set(&test::objects[3],0,int32_t(1));test::Set(&test::objects[3],4,uint8_t(0x20));
    S::g_session=&session;++S::g_applyEpoch;S::g_appliedHash.clear();S::g_appliedObject.clear();S::g_baseHash.clear();S::g_sentHash.clear();S::g_dirty.clear();
    S::g_seedRetry.Clear();S::g_seedScans={};pk::Reset();
}
S::Ingest Apply(const std::vector<uint8_t>& blob,uint8_t author=1){uint32_t eid=0;return S::ParseAndApply(blob,eid,author,coop::props::container_write_policy::Source::Arrival);}
void Receive(const std::vector<uint8_t>& blob,uint8_t author=1){coop::net::BlobChunkPayload p{};p.blobSeq=blob[0];S::OnContentsChunk(p,author);}
SR::SaveRecord Item(){auto a=SR::ReadArr(test::stack+16,0);CHECK(a.num==1);SR::SaveRecord r;SR::ReadSaveRecord(a.data,r);return r;}
bool Replay(const std::vector<uint8_t>& blob,uint8_t author){return Apply(blob,author)!=S::Ingest::Park;}
int replayed=0;
bool ReplaceDuringReplay(const std::vector<uint8_t>& blob,uint8_t author){++replayed;auto original=blob;pk::Admit(7,author,{99},now);CHECK(blob==original);return true;}
bool ResetDuringReplay(const std::vector<uint8_t>& blob,uint8_t){auto old=blob;pk::Reset();pk::Admit(8,0,{42},now);CHECK(blob==old);return true;}
bool NestedSweep(const std::vector<uint8_t>&,uint8_t){++replayed;pk::Sweep(NestedSweep,now);return false;}
}
int main() {
    using namespace wc;using O=S::Ingest;
    Reset();auto blob=Blob(7);CHECK(Apply(blob)==O::Applied&&Item().ints[0][0]==7&&published[7]==1007&&sends==std::vector<int>{2});
    CHECK(S::IsWorldContainerInventory(&test::objects[3])&&S::InventoryOf(&test::objects[2])==&test::objects[3]&&S::OwnerOf(&test::objects[3])==&test::objects[2]&&S::GObjStackSlot(&test::objects[3])==test::stack+16);
    int allocations=test::allocations;CHECK(Apply(blob)==O::Handled&&test::allocations==allocations);
    // Each allocation failure must retain the old container and its replay debt.
    for(int at=1;at<=allocations;++at){Reset();blob=Blob(7);auto before=SR::ReadArr(test::stack+16,0);test::failAt=at;Receive(blob);
        CHECK(pk::Size()==1&&SR::ReadArr(test::stack+16,0).data==before.data&&S::g_appliedHash.empty()&&published.empty()&&sends.empty());
        test::failAt=0;pk::Sweep(Replay,now);CHECK(pk::Size()==0&&Item().ints[0][0]==7&&published[7]==1007);}
    for(int mode=0;mode<4;++mode){Reset();blob=Blob(7);if(mode==0)test::classesReady=false;if(mode==1)test::namesReady=false;if(mode==2)test::stringsReady=false;if(mode==3)test::signalsReady=false;
        CHECK(Apply(blob)==O::Park&&S::g_appliedHash.empty()&&sends.empty());if(mode==0)CHECK(test::allocations==0);}
    Reset();blob=Blob(7);test::Set(&test::objects[3],4,uint8_t(0x24));CHECK(Apply(blob)==O::Handled&&sends.empty());
    Reset();blob=Blob(7);test::Set(&test::objects[3],0,int32_t(0));CHECK(Apply(blob)==O::Park&&sends.empty()&&SR::ReadArr(test::stack,0).data==test::oldItem);
    for(int mode=0;mode<3;++mode){Reset();if(mode==0){int32_t bad=-1;std::memcpy(test::stack+24,&bad,4);}if(mode==1){int32_t max=0;std::memcpy(test::stack+28,&max,4);}if(mode==2){void* ptr=nullptr;std::memcpy(test::stack+16,&ptr,8);}std::vector<SR::SaveRecord> out;CHECK(S::ReadContents(&test::objects[3],out)==S::SliceRead::Unreadable&&out.empty());int count=100;CHECK(!C::ReadContents(&test::objects[3],out,128,&count)&&count==-1&&out.empty());}
    puts("PASS actual container apply/codec: every nested allocation and construction failure retains live contents and replay; one shared masked personal/world boundary");

    for(int change=0;change<11;++change){Reset();blob=Blob(7);test::onNative=[change]{
        if(change==0)++test::world;if(change==1)++serial;if(change==2)++test::objects[2].serial;if(change==3)++test::objects[3].serial;
        if(change==4)++S::g_applyEpoch;if(change==5)session.mode=coop::net::Role::Client;if(change==6)++session.generations[1];
        if(change==7)test::Set(&test::objects[0],0,static_cast<void*>(&test::objects[4]));
        if(change==8)test::Set(&test::objects[2],0,static_cast<void*>(&test::objects[5]));
        if(change==9)test::Set(&test::objects[3],0,int32_t(0));if(change==10)S::detail::ApplySource::Changed(7);
    };CHECK(Apply(blob)==O::Handled&&S::g_appliedHash.empty()&&published.empty()&&sends.empty());}
    Reset();blob=Blob(7);test::onNative=[]{SR::WriteArrHeader(test::stack+16,0,nullptr,0);};CHECK(Apply(blob)==O::Handled&&SR::ReadArr(test::stack+16,0).num==0);
    Reset();blob=Blob(7);auto newer=Blob(9);test::onNative=[&]{CHECK(Apply(newer)==O::Applied);};CHECK(Apply(blob)==O::Handled&&Item().ints[0][0]==9&&published[7]==1009&&sends.size()==1);
    Reset();blob=Blob(7);onJudge=[&]{auto fresh=Blob(9);CHECK(Apply(fresh)==O::Applied);};CHECK(Apply(blob)==O::Handled&&Item().ints[0][0]==9);
    Reset();blob=Blob(7);test::onNative=[]{decision=coop::props::container_write_policy::Decision::StaleBase;auto bad=Blob(9);CHECK(Apply(bad)==O::Handled);decision=coop::props::container_write_policy::Decision::Accept;};CHECK(Apply(blob)==O::Applied&&Item().ints[0][0]==7);
    Reset();blob=Blob(7);onNames=[]{++test::world;};CHECK(Apply(blob)==O::Handled&&volume==0&&views==0&&published.empty()&&sends.empty());
    Reset();blob=Blob(7);birth=true;SR::WriteArrHeader(test::stack+16,0,nullptr,0);onView=[]{++test::world;};CHECK(Apply(blob)==O::Handled&&completed==1&&S::g_dirty.count(7));
    Reset();blob=Blob(7);CHECK(Apply(blob)==O::Applied);++test::objects[2].serial;SR::WriteArrHeader(test::stack+16,0,test::foreignItem,1);CHECK(Apply(blob)==O::Applied&&Item().ints[0][0]==7);
    Reset();blob=Blob(7);CHECK(Apply(blob)==O::Applied);++test::objects[3].serial;SR::WriteArrHeader(test::stack+16,0,test::foreignItem,1);CHECK(Apply(blob)==O::Applied&&Item().ints[0][0]==7);
    Reset();blob=Blob(7);CHECK(Apply(blob)==O::Applied);SR::WriteArrHeader(test::stack+16,0,test::foreignItem,1);CHECK(Apply(blob)==O::Applied&&Item().ints[0][0]==7);
    puts("PASS callback scopes: newer input/local edits, sender replacement, session/world/actor/class/component/store guards, quiet stale derivation and commit-bound birth completion");

    Reset();blob=Blob(7);session.sendOK=false;CHECK(Apply(blob)==O::Applied&&S::g_seedRetry.Size()==1);session.sendOK=true;S::RetrySeeds(&session);CHECK(S::g_seedRetry.Size()==0&&sends==std::vector<int>{2});
    Reset();blob=Blob(7);session.sendOK=false;CHECK(Apply(blob)==O::Applied);++session.generations[2];session.sendOK=true;S::RetrySeeds(&session);CHECK(sends.empty()&&S::g_seedRetry.Size()==0);
    Reset();blob=Blob(7);session.peers[3]=true;session.duringSend=[]{++serial;};CHECK(Apply(blob)==O::Applied&&sends==std::vector<int>{2});
    puts("PASS exact-recipient relay debt, accepted-author exclusion, occupant retirement and interrupted relay cancellation");

    Reset();replayed=0;pk::Admit(7,1,{1,2,3},now);auto version=pk::Revision(7);pk::Sweep(ReplaceDuringReplay,now);CHECK(pk::Size()==1&&pk::Revision(7)!=version&&replayed==1);
    pk::Retire(7,version);CHECK(pk::Size()==1);pk::Sweep(ResetDuringReplay,now);CHECK(pk::Size()==1&&pk::Revision(8));
    pk::Reset();pk::Admit(7,0,{1},now);replayed=0;pk::Sweep(NestedSweep,now-1);CHECK(pk::Size()==1&&replayed==1);
    pk::Reset();for(uint32_t i=0;i<8;++i)pk::Admit(100+i,1,{1},now+i);pk::Admit(7,2,{1},now);pk::Admit(7,1,{2},now+20);CHECK(pk::Size()==8);
    Reset(coop::net::Role::Client);auto old=Blob(7);pk::Admit(7,0,std::move(old),now);auto fresh=Blob(9);Receive(fresh,0);CHECK(pk::Size()==0&&Item().ints[0][0]==9);pk::Sweep(Replay,now);CHECK(Item().ints[0][0]==9);
    Reset(coop::net::Role::Client);old=Blob(7);pk::Admit(7,0,std::move(old),now);fresh=Blob(9);test::onNative=[&]{wc::live=false;auto last=Blob(11);Receive(last,0);};Receive(fresh,0);CHECK(pk::Size()==1);wc::live=true;pk::Sweep(Replay,now);CHECK(Item().ints[0][0]==11);
    CHECK(pk::RunSelftest());
    puts("PASS real park replay: owned callback input, replacement/reset/newer arrivals, no nested sweep, author cap, TTL clock rollback and old-row retirement");
    Reset();
}
