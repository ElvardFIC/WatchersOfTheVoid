// Actual canonical application and pending drain; only Unreal boundaries doubled.
#ifndef EVENT_SLOT_SOURCE
#define EVENT_SLOT_SOURCE "../../src/votv-coop/src/coop/interactables/floppy_slot_sync.cpp"
#endif
#include EVENT_SLOT_SOURCE
namespace coop::disc_record {size_t MaxBodyBytes(){return kTransportBodyBytes;}}
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
std::function<void()> onRead,onWrite,onPrime,onSend;
std::function<void()> onScalar,onContent,onQuad,onDigest;
int digestReads=0,digestTrigger=0;
std::vector<uint8_t> incoming;
bool laptopReady=false,actorWorldKnown=true;
struct Delivery {int slot;uint32_t sequence;std::vector<uint8_t> bytes;};
std::vector<Delivery> delivered;
int attempts=0,reads=0;
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
void Reset(){onRead=onWrite=onPrime=onSend={};S::OnDisconnect();++world;++serial;
    onScalar=onContent=onQuad=onDigest={};digestReads=digestTrigger=0;incoming.clear();
    ++actor.serial;++cls.serial;actor.live=cls.live=true;
    writeReady=readReady=clearReady=actorWorldKnown=true;partialWrite=laptopReady=false;
    writes=clears=primes=sends=attempts=reads=0;delivered.clear();native=Value({L"$srv",L"note"});}
void Host(coop::net::Session& session){Reset();session={};S::Install(&session);}
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
void* WorldOf(void* p){return test::actorWorldKnown && p==&test::actor?CurrentWorld():nullptr;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::floppy_slot {
bool EnsureResolved(DeviceKind){return true;}
std::wstring ObjectDataKey(const std::wstring& value){return value;}
bool ReadScalars(DeviceKind,void*,Scalars& out){++test::reads;out=test::native.st;test::Callback(test::onScalar);test::Callback(test::onRead);return test::readReady;}
bool ReadContent(DeviceKind,void*,Content& out){++test::reads;out=test::native.c;test::Callback(test::onContent);test::Callback(test::onRead);return test::readReady;}
bool WriteSlot(DeviceKind,void*,const Scalars& st,const Content& value){++test::writes;
    if(test::writeReady || test::partialWrite){test::native.st=st;test::native.c=value;}test::Callback(test::onWrite);return test::writeReady;}
bool ClearSlot(DeviceKind,void*){++test::clears;if(test::clearReady)test::native.st.floppyType=-1;
    test::Callback(test::onWrite);return test::clearReady;}
bool ReadDigest(DeviceKind,void*,uint64_t& out,DigestScope){out=test::native.c.data.size();
    if(++test::digestReads==test::digestTrigger)test::Callback(test::onDigest);
    test::Callback(test::onPrime);return true;}
void ResetCache(){}
}
namespace ue_wrap::floppy_disc {void ResetCache(){}}
namespace ue_wrap::serverbox {size_t ReadServers(std::vector<void*>& out){out.push_back(&test::actor);return 1;}}
namespace ue_wrap::laptop {bool EnsureResolved(){return test::laptopReady;}void* Instance(){return test::laptopReady?&test::actor:nullptr;}}
namespace ue_wrap::game_thread {bool IsGameThread(){return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...){}}
namespace coop::text {
std::string ToUtf8Exact(const std::wstring& text){std::string out;for(auto c:text){CHECK(c<128);out.push_back(static_cast<char>(c));}return out;}
bool FromUtf8Strict(const char* data,size_t size,std::wstring* out){out->clear();
    for(size_t i=0;i<size;++i){if(static_cast<unsigned char>(data[i])>=128)return false;out->push_back(data[i]);}return true;}
}
namespace coop::blob_chunks {
void Assembler::Clear(){}void Assembler::ClearSlot(uint8_t){}
void Assembler::Sweep(std::chrono::steady_clock::time_point,std::chrono::seconds){}
bool Assembler::OnChunk(const net::BlobChunkPayload&,uint8_t,std::vector<uint8_t>& out){out=std::move(test::incoming);test::incoming.clear();return !out.empty();}
bool SendBlob(net::Session*,net::ReliableKind,uint32_t,const std::vector<uint8_t>&){++test::sends;return true;}
bool SendBlobToSlot(net::Session*,int,net::ReliableKind,uint32_t,const std::vector<uint8_t>&){++test::sends;return true;}
bool SendBlobWhile(net::Session* s,net::ReliableKind,uint32_t sequence,
                   const std::vector<uint8_t>& blob,const std::function<bool()>& current){
    if(!current())return false;
    ++test::attempts;const bool accepted=s->allowed[0];
    if(accepted){++test::sends;test::delivered.push_back({0,sequence,blob});}
    test::Callback(test::onSend);return current() && accepted;
}
bool SendBlobToSlotWhile(net::Session* s,int slot,net::ReliableKind,uint32_t sequence,
                         const std::vector<uint8_t>& blob,const std::function<bool()>& current){
    if(!current())return false;
    ++test::attempts;
    const bool accepted=s->allowed[slot];
    if(accepted){++test::sends;test::delivered.push_back({slot,sequence,blob});}
    test::Callback(test::onSend);
    return current() && accepted;
}
}
namespace coop::laptop_buffer_sync {void PrimeQuadBaseline(){++test::primes;test::Callback(test::onQuad);}void QueueConnectBroadcastForSlot(int){}}
#ifndef EVENT_SLOT_NO_MAIN
int main(){using namespace test;
    auto first=Value({L""}),second=first;std::vector<uint8_t> entryBytes;
    CHECK(S::PackOne(0,first,entryBytes));
    const size_t max=coop::blob_chunks::MaxBlobBytes();
    first.c.data[0].assign(max/2-entryBytes.size(),L'x');
    second.c.data[0].assign(max-max/2-4-entryBytes.size(),L'y');
    auto blobs=S::PackCanonicalSet(box,{{0,first},{1,second}});
    CHECK(blobs.size()==2); // Four bytes of apparent space cannot fit the eight-byte header.
    for(const auto& blob:blobs)CHECK(blob.size()<=max);
    second.c.data[0].resize(second.c.data[0].size()-4);
    blobs=S::PackCanonicalSet(box,{{0,first},{1,second}});
    CHECK(blobs.size()==1 && blobs[0].size()==max);
    puts("PASS M9: canonical split counts generation, exact transport limit remains valid");
    Reset();auto fullDisc=Value(std::vector<std::wstring>(32,std::wstring(1500,L'x')));
    fullDisc.c.objectData=std::wstring(5000,L'j');
    std::vector<uint8_t> packed;
    CHECK(S::PackOne(0,fullDisc,packed));
    CHECK(packed.size()>32768 && packed.size()+8<=coop::blob_chunks::MaxBlobBytes());
    fullDisc.c.objectData=std::wstring(coop::blob_chunks::MaxBlobBytes(),L'j');
    CHECK(!S::PackOne(0,fullDisc,packed));
    puts("PASS full disc slot: 32 server rows with JSON fit; over-transport slot refused");
    Reset();coop::net::Session joined;joined.mode=coop::net::Role::Client;S::Install(&joined);
    const auto consumed=Value({L"note"});Stage(consumed);writeReady=false;
    Drain();CHECK(S::g_pendingCanonical.count(Key()) && writes==1 && native.c.data.size()==2 && S::g_shadow.empty());
    S::g_haveCanonical[static_cast<uint8_t>(box)]=true;S::g_shadow[Key()]=999;
    S::SweepKind(&joined,box,false);CHECK(!sends); // stale protected row cannot be claimed over a pending canonical
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
    coop::net::Session session;S::g_session.store(&session);++world;
    coop::net::BlobChunkPayload chunk{};S::OnChunk(chunk,0);CHECK(S::g_pendingCanonical.empty());S::OnDisconnect();
    puts("PASS canonical event slots: protected-row consumption retry, equal/clear, newer rows, read/write retirement, laptop generation and session reset");

    Host(session);session.allowed[1]=false;S::g_shadow[Key()]=99;
    native=consumed;S::HostBroadcastOne(&session,box,0,&actor);
    CHECK(attempts==2 && delivered.size()==1 && delivered[0].slot==2);
    CHECK(S::g_retry.count(Key()) && S::g_shadow.at(Key())==99);
    session.allowed[1]=true;S::SweepKind(&session,box,true);
    CHECK(attempts==4 && delivered.size()==3 && !S::g_retry.count(Key()));
    CHECK(S::g_shadow.at(Key())==1 && native.c.data==consumed.c.data && !writes);
    const auto before=attempts;S::SweepKind(&session,box,true);CHECK(attempts==before);
    Host(session);session.allowed[1]=session.allowed[2]=false;
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(S::g_retry.count(Key()) && S::g_shadow.empty());
    session.allowed[1]=session.allowed[2]=true;S::SweepKind(&session,box,true);
    CHECK(delivered.size()==2 && !S::g_retry.count(Key()));
    Host(session);session.peers[31]=session.ready[31]=true;session.generations[31]=31;
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(delivered.size()==3 && delivered.back().slot==31);
    Host(session);session.ready[2]=false;S::HostBroadcastOne(&session,box,0,&actor);
    CHECK(delivered.size()==1 && delivered[0].slot==1);
    puts("PASS SB1/SB2: partial/all refusal retained, retry without a new edge, slot 31, unready recipient and no native consumption replay");

    Host(session);onSend=[]{native.c.data.push_back(L"later");};
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(S::g_shadow.at(Key())==2 && native.c.data.size()==3);
    S::SweepKind(&session,box,true);CHECK(S::g_shadow.at(Key())==3 && delivered.size()==4);
    CHECK(delivered[0].bytes==delivered[1].bytes && delivered[0].bytes!=delivered[2].bytes);
    CHECK(delivered[0].sequence==delivered[1].sequence && delivered[2].sequence!=delivered[0].sequence);
    Host(session);onSend=[&]{native=consumed;S::HostBroadcastOne(&session,box,0,&actor);};
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(S::g_retry.count(Key()) && S::g_shadow.empty() && attempts==2);
    S::SweepKind(&session,box,true);CHECK(S::g_shadow.at(Key())==1 && !S::g_retry.count(Key()));
    Host(session);onRead=[]{native=Value({L"mid-read"});};
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(!attempts && S::g_retry.count(Key()));
    S::SweepKind(&session,box,true);CHECK(attempts==2 && S::g_shadow.at(Key())==1);
    puts("PASS SB3: immutable bytes and captured digest, nested publish and changing native read retain newer work");

    for(int cause=0;cause<5;++cause){
        Host(session);if(cause==0)actorWorldKnown=false;
        onSend=[&,cause]{
            if(cause==0)++world;
            if(cause==1)++serial;
            if(cause==2)++actor.serial;
            if(cause==3)S::OnDisconnect();
            if(cause==4)++cls.serial;
        };
        S::HostBroadcastOne(&session,box,0,&actor);CHECK(attempts==1 && S::g_shadow.empty());
    }
    Host(session);onRead=[]{++actor.serial;};S::HostBroadcastOne(&session,box,0,&actor);
    CHECK(reads==1 && !attempts && S::g_shadow.empty());
    Host(session);onSend=[&]{++session.generations[2];};
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(attempts==1 && S::g_retry.count(Key()));
    S::SweepKind(&session,box,true);CHECK(attempts==3 && !S::g_retry.count(Key()));
    Host(session);onSend=[&]{S::g_nextSweep=0;S::Tick();};
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(attempts==2);
    Host(session);onPrime=[]{S::OnDisconnect();};
    S::HostBroadcastOne(&session,F::DeviceKind::Laptop,0,&actor);
    CHECK(!attempts && S::g_shadow.empty() && !S::g_laptopGenKnown);
    Host(session);native.c.objectData=std::wstring(coop::blob_chunks::MaxBlobBytes(),L'j');
    onPrime=[]{onPrime=[]{onPrime=[]{S::OnDisconnect();};};};
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(!attempts && S::g_shadow.empty());
    puts("PASS SB4: world/session/actor/class/disconnect cancellation, no content read after retirement, peer replacement and nested tick");

    Host(session);laptopReady=true;session.allowed[1]=false;
    S::QueueConnectBroadcastForSlot(1);CHECK(S::g_connectRetry.count(1));
    for(int retry=0;retry<8;++retry){S::g_connectRetry.at(1).next=0;S::DrainConnectRetries(&session);}
    CHECK(S::g_connectRetry.count(1) && !delivered.size() && attempts==9);
    const auto refusedAttempts=attempts;S::DrainConnectRetries(&session);CHECK(attempts==refusedAttempts);
    session.allowed[1]=true;S::g_connectRetry.at(1).next=0;S::DrainConnectRetries(&session);
    CHECK(!S::g_connectRetry.count(1) && delivered.size()==2 && S::g_shadow.empty());
    Host(session);laptopReady=true;session.ready[1]=false;
    S::QueueConnectBroadcastForSlot(1);CHECK(S::g_connectRetry.count(1) && !attempts);
    session.ready[1]=true;S::DrainConnectRetries(&session);CHECK(delivered.size()==2 && S::g_connectRetry.empty());
    Host(session);laptopReady=true;session.allowed[1]=false;S::QueueConnectBroadcastForSlot(1);
    ++session.generations[1];S::DrainConnectRetries(&session);CHECK(S::g_connectRetry.empty());
    Host(session);laptopReady=true;onSend=[]{S::OnDisconnect();};
    S::QueueConnectBroadcastForSlot(1);CHECK(attempts==1 && S::g_connectRetry.empty());
    Host(session);laptopReady=true;onSend=[&]{S::QueueConnectBroadcastForSlot(1);};
    S::QueueConnectBroadcastForSlot(1);CHECK(attempts==1 && S::g_connectRetry.count(1));
    S::DrainConnectRetries(&session);CHECK(attempts==3 && S::g_connectRetry.empty());
    Host(session);readReady=false;laptopReady=true;S::QueueConnectBroadcastForSlot(1);
    CHECK(!attempts && S::g_connectRetry.count(1));readReady=true;
    S::g_connectRetry.at(1).next=0;S::DrainConnectRetries(&session);CHECK(attempts==2 && S::g_connectRetry.empty());
    Host(session);laptopReady=true;session.allowed[1]=false;S::QueueConnectBroadcastForSlot(1);
    S::OnPeerGone(1);CHECK(S::g_connectRetry.empty());
    Host(session);laptopReady=true;onSend=[&]{S::QueueConnectBroadcastForSlot(2);};
    S::HostBroadcastOne(&session,box,0,&actor);
    CHECK(attempts==2 && S::g_connectRetry.count(2));
    S::DrainConnectRetries(&session);CHECK(attempts==4 && S::g_connectRetry.empty());
    Host(session);laptopReady=true;onSend=[&]{S::HostBroadcastOne(&session,box,0,&actor);};
    S::QueueConnectBroadcastForSlot(1);CHECK(attempts==2 && S::g_retry.count(Key()));
    S::SweepKind(&session,box,true);CHECK(attempts==4 && !S::g_retry.count(Key()));
    Host(session);laptopReady=true;onRead=[]{++actor.serial;};
    S::QueueConnectBroadcastForSlot(1);CHECK(!attempts && reads==1 && S::g_connectRetry.count(1));
    Host(session);laptopReady=true;onPrime=[]{native=Value({L"changed-during-join"});};
    S::QueueConnectBroadcastForSlot(1);CHECK(S::g_connectRetry.count(1));
    S::g_connectRetry.at(1).next=0;S::DrainConnectRetries(&session);CHECK(S::g_connectRetry.empty());
    Host(session);S::QueueConnectBroadcastForSlot(1);CHECK(attempts==1 && S::g_connectRetry.count(1));
    for(int retry=0;retry<8;++retry){S::g_connectRetry.at(1).next=0;S::DrainConnectRetries(&session);}
    CHECK(attempts==1);laptopReady=true;S::g_connectRetry.at(1).next=0;S::DrainConnectRetries(&session);
    CHECK(attempts==2 && S::g_connectRetry.empty());
    Host(session);onSend=[]{S::OnPeerGone(2);};
    S::HostBroadcastOne(&session,box,0,&actor);CHECK(attempts==1 && S::g_retry.count(Key()));
    puts("PASS SB5: more than five refusals, paced retry, pre-ready join, same-slot replacement, disconnect, nested request, unreadable rows and peer cleanup");
}
#endif
