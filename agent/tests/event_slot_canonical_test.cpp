// Actual canonical application and pending drain; only Unreal boundaries doubled.
#ifndef EVENT_SLOT_SOURCE
#define EVENT_SLOT_SOURCE "../../src/votv-coop/src/coop/interactables/floppy_slot_sync.cpp"
#endif
#include EVENT_SLOT_SOURCE
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace S=coop::floppy_slot_sync;
namespace F=ue_wrap::floppy_slot;
struct Object {int index=0,serial=1;bool live=true;} actor{0},cls{1};
uint32_t world=1,serial=1;
bool writeReady=true,readReady=true,clearReady=true,partialWrite=false;
int writes=0,clears=0,primes=0,sends=0;
S::Slot native;
std::function<void()> onRead,onWrite,onPrime;
void Callback(std::function<void()>& cb){auto fn=std::move(cb);cb={};if(fn)fn();}
constexpr auto box=F::DeviceKind::ServerBox;
uint32_t Key(F::DeviceKind kind=box){return S::ShadowKey(kind,0);}
void Stage(const S::Slot& value,F::DeviceKind kind=box,uint32_t generation=0){
    const auto key=Key(kind);S::g_pendingCanonical[key]=value;
    S::g_pendingCanonicalRevision[key]=++S::g_pendingRevision;
    if(kind==F::DeviceKind::Laptop)S::g_pendingCanonicalGen[key]=generation;
}
S::Slot Value(std::vector<std::wstring> rows){S::Slot value;value.st.floppyType=2;
    value.st.readWrites=7;value.c.objectData=L"disc-key";value.c.data=std::move(rows);return value;}
void Reset(){onRead=onWrite=onPrime={};S::OnDisconnect();++world;++serial;
    ++actor.serial;++cls.serial;actor.live=cls.live=true;
    writeReady=readReady=clearReady=true;partialWrite=false;writes=clears=primes=sends=0;native=Value({L"$srv",L"note"});}
void Drain(F::DeviceKind kind=box){S::DrainPendingCanonicals(kind,{&actor});}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Object*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return i==0?test::actor.serial:i==1?test::cls.serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return (i==0?p==&test::actor:i==1?p==&test::cls:false) && static_cast<test::Object*>(p)->live;}
bool IsLive(void* p){return p && (p==&test::actor || p==&test::cls) && static_cast<test::Object*>(p)->live;}
void* ClassOf(void*){return &test::cls;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p==&test::actor?CurrentWorld():nullptr;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::floppy_slot {
bool EnsureResolved(DeviceKind){return true;}
std::wstring ObjectDataKey(const std::wstring& value){return value;}
bool ReadScalars(DeviceKind,void*,Scalars& out){out=test::native.st;test::Callback(test::onRead);return test::readReady;}
bool ReadContent(DeviceKind,void*,Content& out){out=test::native.c;test::Callback(test::onRead);return test::readReady;}
bool WriteSlot(DeviceKind,void*,const Scalars& st,const Content& value){++test::writes;
    if(test::writeReady || test::partialWrite){test::native.st=st;test::native.c=value;}test::Callback(test::onWrite);return test::writeReady;}
bool ClearSlot(DeviceKind,void*){++test::clears;if(test::clearReady)test::native.st.floppyType=-1;
    test::Callback(test::onWrite);return test::clearReady;}
bool ReadDigest(DeviceKind,void*,uint64_t& out,DigestScope){out=test::native.c.data.size();test::Callback(test::onPrime);return true;}
void ResetCache(){}
}
namespace ue_wrap::floppy_disc {void ResetCache(){}}
namespace ue_wrap::serverbox {size_t ReadServers(std::vector<void*>& out){out.push_back(&test::actor);return 1;}}
namespace ue_wrap::laptop {bool EnsureResolved(){return false;}void* Instance(){return nullptr;}}
namespace ue_wrap::game_thread {bool IsGameThread(){return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...){}}
namespace coop::text {
std::string ToUtf8Exact(const std::wstring& text){std::string out;for(auto c:text){CHECK(c<128);out.push_back(static_cast<char>(c));}return out;}
bool FromUtf8Strict(const char*,size_t,std::wstring*){std::abort();}
}
namespace coop::blob_chunks {
void Assembler::Clear(){}void Assembler::ClearSlot(uint8_t){}
void Assembler::Sweep(std::chrono::steady_clock::time_point,std::chrono::seconds){}
bool Assembler::OnChunk(const net::BlobChunkPayload&,uint8_t,std::vector<uint8_t>&){return false;}
bool SendBlob(net::Session*,net::ReliableKind,uint32_t,const std::vector<uint8_t>&){++test::sends;return true;}
bool SendBlobToSlot(net::Session*,int,net::ReliableKind,uint32_t,const std::vector<uint8_t>&){++test::sends;return true;}
}
namespace coop::laptop_buffer_sync {void PrimeQuadBaseline(){++test::primes;}void QueueConnectBroadcastForSlot(int){}}
int main(){using namespace test;
    Reset();const auto consumed=Value({L"note"});Stage(consumed);writeReady=false;
    Drain();CHECK(S::g_pendingCanonical.count(Key()) && writes==1 && native.c.data.size()==2 && S::g_shadow.empty());
    S::g_haveCanonical[static_cast<uint8_t>(box)]=true;S::g_shadow[Key()]=999;
    S::SweepKind(nullptr,box,false);CHECK(!sends); // stale protected row cannot be claimed over a pending canonical
    writeReady=true;Drain();CHECK(S::g_pendingCanonical.empty() && writes==2 && native.c.data==consumed.c.data);
    CHECK(S::g_haveCanonical[static_cast<uint8_t>(box)]);
    Stage(consumed);Drain();CHECK(writes==2 && S::g_pendingCanonical.empty());
    Reset();S::Slot empty;empty.st.floppyType=-1;Stage(empty);clearReady=false;Drain();
    CHECK(S::g_pendingCanonical.count(Key()) && clears==1);clearReady=true;Drain();CHECK(S::g_pendingCanonical.empty() && native.st.floppyType<0);
    Reset();Stage(consumed);onRead=[&]{Stage(Value({L"newer"}));};Drain();
    CHECK(!writes && S::g_pendingCanonical.at(Key()).c.data==std::vector<std::wstring>{L"newer"});
    Drain();CHECK(S::g_pendingCanonical.empty() && native.c.data==std::vector<std::wstring>{L"newer"});
    Reset();Stage(consumed);onWrite=[&]{Stage(Value({L"newer"}));Drain();CHECK(writes==1);};Drain();
    CHECK(writes==1 && S::g_pendingCanonical.count(Key()));Drain();CHECK(writes==2 && S::g_pendingCanonical.empty());
    Reset();Stage(consumed);onWrite=[]{S::OnDisconnect();};Drain();CHECK(S::g_pendingCanonical.empty() && !S::g_haveCanonical[static_cast<uint8_t>(box)]);
    for(int cause=0;cause<3;++cause){Reset();Stage(consumed);onRead=[cause]{if(cause==0)++world;if(cause==1)++serial;if(cause==2)++actor.serial;};
        Drain();CHECK(!writes && S::g_pendingCanonical.count(Key()));}
    Reset();auto newDisc=consumed;newDisc.c.objectData=L"new-disc";Stage(newDisc,F::DeviceKind::Laptop,11);writeReady=false;Drain(F::DeviceKind::Laptop);
    CHECK(!S::g_laptopGen && !primes && S::g_pendingCanonical.count(Key(F::DeviceKind::Laptop)));
    writeReady=true;Drain(F::DeviceKind::Laptop);CHECK(S::g_laptopGen==11 && primes==1 && S::g_pendingCanonical.empty());
    Reset();Stage(consumed);writeReady=false;partialWrite=true;Drain();
    CHECK(native.c.data==consumed.c.data && S::g_pendingCanonical.count(Key()) && writes==1);
    writeReady=true;Drain();CHECK(writes==2 && S::g_pendingCanonical.empty() && S::g_pendingWriteDebt.empty());
    Reset();for(unsigned i=0;i<10;++i){const auto key=S::ShadowKey(box,i);S::g_pendingCanonical[key]=consumed;
        S::g_pendingCanonicalRevision[key]=++S::g_pendingRevision;}
    std::vector<void*> devices(10,&actor);writeReady=false;S::DrainPendingCanonicals(box,devices);
    CHECK(writes==8 && S::g_pendingCanonical.size()==10);writeReady=true;
    S::DrainPendingCanonicals(box,devices);CHECK(S::g_pendingCanonical.size()==2 && !S::g_pendingCanonical.count(S::ShadowKey(box,9)));
    S::DrainPendingCanonicals(box,devices);CHECK(S::g_pendingCanonical.empty());
    Reset();Stage(consumed);S::Install(nullptr);CHECK(S::g_pendingCanonical.empty());
    Reset();CHECK(S::StageCanonical(box,0,consumed,0,20));
    CHECK(!S::StageCanonical(box,0,Value({L"$srv",L"note"}),0,19));
    CHECK(!S::StageCanonical(box,0,Value({L"$srv",L"note"}),0,20));
    Drain();CHECK(native.c.data==consumed.c.data);
    CHECK(!S::StageCanonical(box,0,Value({L"$srv",L"note"}),0,18) && S::g_pendingCanonical.empty());
    CHECK(S::StageCanonical(box,1,Value({L"other-device"}),0,10)); // per-device order, not one global latch
    Reset();CHECK(S::StageCanonical(box,0,consumed,0,0xfffffffeu));
    CHECK(S::StageCanonical(box,0,Value({L"wrapped"}),0,0));
    CHECK(!S::StageCanonical(box,0,consumed,0,0xffffffffu));
    CHECK(S::StageCanonical(F::DeviceKind::Laptop,0,consumed,11,1));
    CHECK(!S::StageCanonical(F::DeviceKind::Laptop,0,consumed,9,0));
    CHECK(S::g_pendingCanonicalGen.at(Key(F::DeviceKind::Laptop))==11);
    Reset();Stage(consumed);S::g_boundWorld=world;S::g_boundSerial=serial;
    S::g_session.store(reinterpret_cast<coop::net::Session*>(uintptr_t(1)));++world;
    coop::net::BlobChunkPayload chunk{};S::OnChunk(chunk,0);CHECK(S::g_pendingCanonical.empty());S::OnDisconnect();
    puts("PASS canonical event slots: protected-row consumption retry, equal/clear, newer rows, read/write retirement, laptop generation and session reset");
}
