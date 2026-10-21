// Real queue, receiver and serializer; only engine/transport entry points are doubled.
#include "../../src/votv-coop/src/coop/props/prop_save_data.cpp"
#include "../../src/votv-coop/src/coop/items/save_record_wire.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#define CHECK(x) do { if(!(x)) {std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
namespace S=coop::prop_save_data;
using Record=ue_wrap::save_record::SaveRecord;
namespace test {
int actor=0,calls=0,depth=0,notes=0,lookups=0,captures=0;
bool nested=false,success=true,host=true,live=true,covered=true,allowOther=false;
bool captureOK=false,sendOK=true;
int serial=1;
uint32_t world=1;
std::wstring liveKey=L"item";
bool safe=false,unlocked=false;int sentLock=-1;
bool grill=false;float burn=.75f;
std::wstring onlyLive;
std::function<void(const Record&)> duringApply;
std::function<void()> duringCapture;
std::function<void()> duringNoteApply,duringNotePublish;
std::function<void()> duringSend,duringRefresh,duringLock;
std::vector<uint8_t> incoming;
std::vector<int> sent,noted;
coop::net::Session session;
Record Make(const std::wstring& key=L"item",int value=1) {
    Record r;r.key=key;r.ints={{value}};r.strings={{std::wstring(256,L'x')}};return r;
}
void Occupancy() {
    std::map<uint8_t,S::SenderUse> expected;
    for (const auto& [key,row]:S::g_parked) {++expected[row.senderSlot].count;expected[row.senderSlot].bytes+=row.bytes;}
    for (const auto& [slot,use]:S::g_parkUse) {CHECK(use.count==expected[slot].count);CHECK(use.bytes==expected[slot].bytes);}
    for (const auto& [slot,use]:expected) {CHECK(S::g_parkUse.count(slot));CHECK(S::g_parkUse.at(slot).count==use.count);}
}
void Reset() {
    serial=1;liveKey=L"item";
    world=1;duringSend=duringRefresh=duringLock={};CHECK(S::g_recordOrder.empty());
    session.linked=session.ready=true;session.blockedSlot=-1;
    CHECK(S::g_applying.empty());CHECK(S::g_publishing.empty());CHECK(!S::g_driving);S::OnDisconnect();
    calls=depth=notes=lookups=captures=0;nested=false;success=host=live=covered=sendOK=true;
    allowOther=captureOK=false;duringCapture={};duringNoteApply={};duringNotePublish={};
    safe=unlocked=false;sentLock=-1;
    grill=false;burn=.75f;
    onlyLive.clear();duringApply={};incoming.clear();sent.clear();noted.clear();
}
void Deliver(const std::wstring& key,int value,uint8_t sender,bool intent) {
    host=intent;incoming=S::BuildBody(key,Make(key,value));
    S::OnChunk(session,{},sender,intent);
}
enum Mode {Birth,Drain,Canonical,Intent};
void Run(Mode mode) {
    if (mode==Birth || mode==Drain) {
        S::Park(L"item",Make(),1,100);
        if (mode==Birth) S::ApplyParked(&actor,L"item");else S::Drive();
    } else Deliver(L"item",1,mode==Intent?1:0,mode==Intent);
}
}
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::grill_saved_state {
bool Supported(void*){return test::grill;}
bool RefreshHeldRecord(void*,save_record::SaveRecord& record){record.floats.resize(2);record.floats[1]={43217,1,test::burn,1};auto cb=std::move(test::duringRefresh);test::duringRefresh={};if(cb)cb();return true;}
bool PreserveHostRecord(void* actor,save_record::SaveRecord& record){return RefreshHeldRecord(actor,record);}
}
namespace ue_wrap::safe {
bool IsSafe(void*){return test::safe;}
bool PreserveHostLock(void*,save_record::SaveRecord& r){
 auto callback=std::move(test::duringLock);test::duringLock={};if(callback)callback();
 if(!test::safe)return false;
 const auto bit=uint8_t(!test::unlocked);
 if(r.bools.size()>1&&!r.bools[1].empty()&&r.bools[1][0]==bit)return false;
 r.bools.resize(2);r.bools[1]={bit};return true;
}
bool PreserveUnlocked(void*,save_record::SaveRecord& r){
 auto callback=std::move(test::duringLock);test::duringLock={};if(callback)callback();
 if(!test::safe||!test::unlocked)return false;
 if(r.bools.size()>1&&!r.bools[1].empty()&&!r.bools[1][0])return false;
 r.bools.resize(2);r.bools[1]={0};return true;
}
}
namespace ue_wrap::reflection {
void* ClassOf(void*) { return &test::actor; }
bool IsLive(void*){return test::live;}
bool IsLiveByIndex(void*,int){return test::live;}
int32_t InternalIndexOf(void*){return 1;}int32_t SlotSerial(int32_t){return test::serial;}int32_t AllocateSlotSerial(int32_t){return test::serial;}
const FName& NameOf(void*){static FName n{};return n;}
void* SuperStructOf(void*){return nullptr;}
std::wstring ToString(const FName&){return L"prop_test_C";}
void* FindClass(const wchar_t*) { return nullptr; }
bool IsDescendantOfAny(void*,void* const*,size_t,int) { return false; }
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}uint32_t Generation(){return test::world;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::liveKey;}}
namespace ue_wrap::save_record {
bool OverridesGetData(void*) { return test::covered; }
bool CaptureRecord(void*,SaveRecord& rec) {
    ++test::captures;auto callback=std::move(test::duringCapture);test::duringCapture={};
    if (callback) callback();rec=test::Make();return test::captureOK;
}
bool ApplyRecord(void*,const SaveRecord& rec,void*(*)(const wchar_t*)) {
    ++test::calls;
    // Reject borrowed queue storage before callbacks invalidate it (a safe mutation oracle).
    auto row=S::g_parked.find(rec.key);
    if (row!=S::g_parked.end()) CHECK(&rec!=&row->second.rec);
    if (++test::depth>1 && !test::allowOther) {--test::depth;return false;}
    auto callback=std::move(test::duringApply);test::duringApply={};
    if (callback) callback(rec);
    CHECK(rec.ints.at(0).at(0)==1 || rec.ints.at(0).at(0)==2);
    --test::depth;return test::success;
}
void ResetCodecCache() {}
}
namespace coop::prop_element_tracker {
bool SessionIsHost() { return test::host; }
void* FindLiveActorByKey(const std::wstring& key) {
    ++test::lookups;return test::live && (test::onlyLive.empty() || key==test::onlyLive)?&test::actor:nullptr;
}
}
namespace coop::dev::food_clock_probe {
void NoteRefused(void*) {}
void NotePublish(void*,const std::wstring&,const ue_wrap::save_record::SaveRecord&) {
    auto fn=std::move(test::duringNotePublish);test::duringNotePublish={};if(fn)fn();
}
void NoteApply(void*,const std::wstring&,const Record& r) {
    auto fn=std::move(test::duringNoteApply);test::duringNoteApply={};if(fn)fn();
    ++test::notes;test::noted.push_back(r.ints.at(0).at(0));
}
}
namespace coop::blob_chunks {
bool SendBlob(net::Session*,net::ReliableKind,uint32_t,const std::vector<uint8_t>& bytes) {
    std::wstring key;Record rec;CHECK(S::ParseBody(bytes,key,rec));test::sent.push_back(rec.ints.at(0).at(0));
    auto callback=std::move(test::duringSend);test::duringSend={};if(callback)callback();
    if(rec.bools.size()>1&&!rec.bools[1].empty())test::sentLock=rec.bools[1][0];return test::sendOK;
}
bool SendBlobToSlot(net::Session* s,int,net::ReliableKind kind,uint32_t seq,const std::vector<uint8_t>& b) {return SendBlob(s,kind,seq,b);}
bool SendBlobWhile(net::Session* s,net::ReliableKind kind,uint32_t seq,const std::vector<uint8_t>& b,const std::function<bool()>& current){
    if(!current())return false;const bool sent=SendBlob(s,kind,seq,b);return current() && sent;}
bool SendBlobToSlotWhile(net::Session* s,int,net::ReliableKind kind,uint32_t seq,const std::vector<uint8_t>& b,const std::function<bool()>& current){return SendBlobWhile(s,kind,seq,b,current);}
bool Assembler::OnChunk(const net::BlobChunkPayload&,uint8_t,std::vector<uint8_t>& out) {out=std::move(test::incoming);return !out.empty();}
void Assembler::Sweep(std::chrono::steady_clock::time_point,std::chrono::seconds) {}
void Assembler::Clear() {}
void Assembler::ClearSlot(uint8_t) {}
}
namespace coop::signal_wire {
std::vector<uint8_t> Serialize(const ue_wrap::signal_dynamic::Row&) { std::abort(); }
bool Deserialize(const std::vector<uint8_t>&,ue_wrap::signal_dynamic::Row&) { std::abort(); }
}
int main() {
    using namespace test;
    Reset();safe=unlocked=true;auto stale=Make();stale.bools={{1},{1}};
    incoming=S::BuildBody(L"item",stale);
    duringApply=[](const Record& r){CHECK(r.bools[1][0]==0);};S::OnChunk(session,{},1,true);
    CHECK(sentLock==0&&calls==1); // normalize before canonical broadcast AND native apply
    Reset();safe=true;auto early=stale;early.bools[1][0]=0;incoming=S::BuildBody(L"item",early);
    duringApply=[](const Record& r){CHECK(r.bools[1][0]==1);};S::OnChunk(session,{},1,true);
    CHECK(sentLock==1&&calls==1); // a record cannot bypass the native host unlock/loot
    Reset();safe=true;S::ExpectRecordFor(&actor,L"item",1);incoming=S::BuildBody(L"item",early);
    duringApply=[](const Record& r){CHECK(r.bools[1][0]==0);};S::OnChunk(session,{},1,true);
    CHECK(sentLock==0&&calls==1); // a newly dropped, already unlocked safe keeps its saved state
    Reset();safe=unlocked=true;S::g_session=&session;S::Park(L"item",Record(stale),1,100);
    duringApply=[](const Record& r){CHECK(r.bools[1][0]==0);};CHECK(S::ApplyParked(&actor,L"item"));CHECK(sentLock==0);
    Reset();safe=unlocked=true;host=false;S::Park(L"item",Record(stale),0,100);
    duringApply=[](const Record& r){CHECK(r.bools[1][0]==1);};CHECK(S::ApplyParked(&actor,L"item"));CHECK(sent.empty()); // client accepts host correction
    for (Mode mode:{Birth,Drain,Canonical,Intent}) {
        for (bool ok:{true,false}) {
            Reset();success=ok;
            duringApply=[](const Record&){nested=S::ApplyParked(&actor,L"item");};Run(mode);
            CHECK(!nested);CHECK(calls==1);CHECK(S::g_parked.empty()==ok);Occupancy();
            CHECK(S::g_applying.empty());
            if (!ok) CHECK(S::g_parked.at(L"item").failures==1);

            // New same-key data cannot run recursively or inherit the old row's failures.
            Reset();success=ok;
            duringApply=[mode](const Record& old){
                Deliver(L"item",2,mode==Intent?2:0,mode==Intent);
                CHECK(old.ints[0][0]==1);CHECK(!S::ApplyParked(&actor,L"item"));
            };Run(mode);
            CHECK(calls==1);CHECK(S::g_parked.count(L"item"));CHECK(S::g_parked.at(L"item").rec.ints[0][0]==2);
            CHECK(S::g_parked.at(L"item").failures==0);Occupancy();
            if (mode==Intent) CHECK(sent==std::vector<int>({1,2}));
            success=true;CHECK(S::ApplyParked(&actor,L"item"));CHECK(S::g_parked.empty());Occupancy();

            // Disconnect resets sequence to 1: the epoch prevents retirement of a new-world row.
            Reset();success=ok;
            duringApply=[](const Record&){S::OnDisconnect();S::Park(L"item",Make(L"item",2),3,201);};Run(mode);
            CHECK(calls==1);CHECK(notes==0);CHECK(S::g_appliedTotal==0);
            CHECK(S::g_parked.at(L"item").rec.ints[0][0]==2);
            CHECK(S::g_parked.at(L"item").failures==0);Occupancy();

            Reset();success=ok;
            duringApply=[mode](const Record&){S::OnPeerGone(mode==Canonical?0:1);};Run(mode);
            CHECK(calls==1);CHECK(S::g_parked.empty());Occupancy();
        }
    }
    Reset();allowOther=true;
    S::Park(L"other",Make(L"other",2),2,40);
    duringApply=[](const Record&){CHECK(S::ApplyParked(&actor,L"other"));};Run(Birth);
    CHECK(calls==2);CHECK(S::g_parked.empty());Occupancy();

    Reset();duringApply=[](const Record&){S::Drive();};Run(Drain);
    CHECK(calls==1);CHECK(S::g_applyBudget==S::kAppliesPerFrame-1);

    Reset();success=false;Run(Drain);
    for (int i=0;i<10;++i) S::Drive();
    CHECK(calls==S::kMaxApplyFailures);CHECK(S::g_parked.at(L"item").failures==S::kMaxApplyFailures);
    success=true;CHECK(S::ApplyParked(&actor,L"item")); // explicit fresh birth gets an attempt

    Reset();success=false;
    for (int i=0;i<20;++i) Deliver(L"k"+std::to_wstring(i),1,0,false);
    CHECK(calls==S::kAppliesPerFrame);CHECK(S::g_applyBudget==0);CHECK(S::g_parked.size()==20);Occupancy();
    Reset();for(int i=0;i<20;++i) Deliver(L"k"+std::to_wstring(i),1,1,true);
    CHECK(calls==20);CHECK(S::g_parked.empty()); // host intents retain immediate, rate-gated acceptance

    Reset();onlyLive=L"zzzz";
    for (int i=0;i<150;++i) S::Park(L"missing"+std::to_wstring(i),Make(),1,100);
    S::Park(onlyLive,Make(onlyLive),2,70);
    for (int i=0;i<3;++i) {const int before=lookups;S::Drive();CHECK(lookups-before<=64);}
    CHECK(calls==1);CHECK(!S::g_parked.count(onlyLive));Occupancy();

    Reset();duringApply=[](const Record&){throw std::runtime_error("native boundary double");};
    try {Run(Drain);CHECK(false);} catch (const std::runtime_error&) {}
    CHECK(S::g_applying.empty());CHECK(!S::g_driving);depth=0;
    CHECK(S::ApplyParked(&actor,L"item"));Occupancy();

    Reset();live=false;
    for (int i=0;i<2050;++i) S::Park(L"k"+std::to_wstring(i),Make(),1,10);
    CHECK(S::g_parked.size()==2048);CHECK(!S::g_parked.count(L"k0"));Occupancy();
    S::OnPeerGone(1);CHECK(S::g_parkUse.empty());CHECK(S::g_parked.empty());
    for (int i=0;i<130;++i) S::Park(L"k"+std::to_wstring(i),Make(),1,32768);
    CHECK(S::g_parked.size()==128);Occupancy();
    Reset();covered=false;Run(Intent);CHECK(calls==0);CHECK(sent.empty());CHECK(S::g_parked.empty());

    // Native capture is another callback boundary: retries must not retain an invalid iterator.
    for (bool leave:{false,true}) {
        Reset();captureOK=true;sendOK=false;
        CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));CHECK(S::g_retry.size()==1);
        sendOK=true;sent.clear();
        duringCapture=[leave]{if (leave) S::OnPeerGone(1);else S::OnDisconnect();};
        S::DriveRetries();CHECK(sent.empty());CHECK(S::g_retry.empty());
    }
    Reset();captureOK=true;sendOK=false;
    CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));
    duringCapture=[]{
        S::OnPeerGone(1);
        CHECK(!S::SendBody(&session,2,L"item",Make()));
    };
    S::DriveRetries();CHECK(S::g_retry.size()==1);CHECK(S::g_retry.at({L"item",2}).peerSlot==2);
    CHECK(S::g_retry.at({L"item",2}).tries==1);
    sendOK=true;S::g_retry.at({L"item",2}).nextTryMs=0;S::DriveRetries();CHECK(S::g_retry.empty());

    Reset();captureOK=true;sendOK=false;
    CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));
    duringCapture=[]{CHECK(!S::SendBody(&session,2,L"item",Make()));sendOK=true;};
    S::DriveRetries();CHECK(S::g_retry.empty()); // both independent recipients succeed this frame

    Reset();captureOK=true;
    duringCapture=[]{CHECK(!S::Publish(&session,&actor,L"item"));};
    CHECK(S::Publish(&session,&actor,L"item"));CHECK(captures==1);CHECK(sent.size()==1);
    Reset();captureOK=true;
    duringApply=[](const Record&){CHECK(!S::Publish(&session,&actor,L"item"));};Run(Birth);
    CHECK(captures==0);CHECK(sent.empty());

    Reset();duringNoteApply=[]{S::Park(L"item",Make(L"item",2),2,77);CHECK(!S::ApplyParked(&actor,L"item"));};
    Run(Birth);CHECK(calls==1);CHECK(S::g_parked.count(L"item"));CHECK(noted==std::vector<int>({1}));Occupancy();
    Reset();duringNoteApply=[]{S::OnDisconnect();S::Park(L"item",Make(L"item",2),2,77);};
    Run(Drain);CHECK(S::g_parked.count(L"item"));CHECK(S::g_appliedTotal==0);Occupancy();
    Reset();captureOK=true;duringNotePublish=[]{S::OnDisconnect();};
    CHECK(!S::Publish(&session,&actor,L"item"));CHECK(sent.empty());

    Reset();captureOK=true;sendOK=false;
    CHECK(!S::Publish(&session,&actor,L"item"));
    for (int i=0;i<S::kMaxRetryTries;++i) {
        S::g_retry.at({L"item",-1}).nextTryMs=0;S::DriveRetries();
        CHECK(S::g_retry.at({L"item",-1}).tries==i+1);
    }
    S::g_retry.at({L"item",-1}).nextTryMs=0;S::DriveRetries();
    CHECK(S::g_retry.empty());CHECK(captures==S::kMaxRetryTries+1);
    Reset();
    CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));
    CHECK(S::g_retry.size()==1&&sent.empty());captureOK=true;
    S::DriveRetries();CHECK(S::g_retry.empty()&&sent.size()==1);
    Reset();duringCapture=[]{S::OnDisconnect();};
    CHECK(!S::Publish(&session,&actor,L"item"));CHECK(S::g_retry.empty());
    Reset();duringCapture=[]{live=false;};
    CHECK(!S::Publish(&session,&actor,L"item"));CHECK(S::g_retry.empty());
    Reset();CHECK(!S::Publish(&session,&actor,L"item"));
    for(int i=0;i<S::kMaxRetryTries+1;++i){S::g_retry.at({L"item",-1}).nextTryMs=0;S::DriveRetries();}
    CHECK(S::g_retry.empty()&&sent.empty());
    for(bool captureFailure:{false,true}) {
        Reset();onlyLive=L"not-indexed";captureOK=!captureFailure;sendOK=captureFailure;
        CHECK(!S::Publish(&session,&actor,L"item"));CHECK(S::g_retry.size()==1);
        sent.clear();captureOK=sendOK=true;S::DriveRetries();CHECK(S::g_retry.empty()&&sent.size()==1);
    }
    for(int invalid=0;invalid<3;++invalid) {
        Reset();onlyLive=L"not-indexed";CHECK(!S::Publish(&session,&actor,L"item"));
        if(invalid==0)++serial;
        if(invalid==1)liveKey=L"changed";
        if(invalid==2)live=false;
        captureOK=true;S::DriveRetries();CHECK(S::g_retry.empty()&&sent.empty());
    }
    Reset();captureOK=true;sendOK=false;
    CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));
    CHECK(!S::PublishToSlot(&session,2,&actor,L"item"));
    CHECK(!S::Publish(&session,&actor,L"item"));
    CHECK(S::g_retry.size()==3);
    S::g_retry.at({L"item",1}).tries=3;
    CHECK(!S::PublishToSlot(&session,2,&actor,L"item"));
    CHECK(S::g_retry.at({L"item",1}).tries==3&&S::g_retry.at({L"item",2}).tries==0);
    S::OnPeerGone(1);CHECK(S::g_retry.size()==2);
    CHECK(S::g_retry.count({L"item",2})&&S::g_retry.count({L"item",-1}));
    sent.clear();sendOK=true;S::DriveRetries();CHECK(S::g_retry.empty()&&sent.size()==2);
    Reset();captureOK=true;sendOK=false;
    CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));sendOK=true;
    duringCapture=[]{S::QueuePublishRetry(L"item",1);};
    S::DriveRetries();CHECK(S::g_retry.count({L"item",1})); // newer same-target work survives old success
    S::g_retry.at({L"item",1}).nextTryMs=0;S::DriveRetries();CHECK(S::g_retry.empty());
    for(int slot:{-1,1}) {
        Reset();captureOK=true;sendOK=false;
        if(slot<0)CHECK(!S::Publish(&session,&actor,L"item"));
        else CHECK(!S::PublishToSlot(&session,slot,&actor,L"item"));
        sent.clear();const int before=captures;session.ready=false;
        for(int i=0;i<S::kMaxRetryTries+3;++i)S::DriveRetries();
        CHECK(S::g_retry.at({L"item",slot}).tries==0&&captures==before&&sent.empty());
        session.ready=true;session.linked=false;S::DriveRetries();
        CHECK(S::g_retry.at({L"item",slot}).tries==0);
        session.linked=true;sendOK=true;S::DriveRetries();CHECK(S::g_retry.empty()&&sent.size()==1);
    }
    Reset();captureOK=true;sendOK=false;
    CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));
    CHECK(!S::PublishToSlot(&session,2,&actor,L"item"));
    session.blockedSlot=1;sent.clear();sendOK=true;S::DriveRetries();
    CHECK(S::g_retry.size()==1&&S::g_retry.at({L"item",1}).tries==0&&sent.size()==1);
    session.blockedSlot=-1;S::ExpectRecordFor(&actor,L"item",1);
    for(int i=0;i<S::kMaxRetryTries+3;++i)S::DriveRetries();
    CHECK(S::g_retry.at({L"item",1}).tries==0&&sent.size()==1);
    S::g_awaiting.erase(L"item");S::DriveRetries();CHECK(S::g_retry.empty()&&sent.size()==2);
    Reset();captureOK=true;sendOK=false;
    CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));
    CHECK(!S::PublishToSlot(&session,2,&actor,L"item"));
    sendOK=true;CHECK(S::PublishToSlot(&session,1,&actor,L"item"));
    CHECK(!S::g_retry.count({L"item",1})&&S::g_retry.count({L"item",2}));
    sent.clear();S::DriveRetries();CHECK(S::g_retry.empty()&&sent.size()==1);
    Reset();CHECK(!S::Publish(&session,&actor,L"item"));captureOK=true;
    CHECK(S::Publish(&session,&actor,L"item"));CHECK(S::g_retry.empty());
    sent.clear();S::DriveRetries();CHECK(sent.empty());
    Reset();CHECK(!S::PublishToSlot(&session,1,&actor,L"item"));captureOK=true;
    duringCapture=[]{S::QueuePublishRetry(L"item",1);};
    CHECK(S::PublishToSlot(&session,1,&actor,L"item"));CHECK(S::g_retry.count({L"item",1}));
    puts("PASS: direct successful publication clears only its older matching retry, preserving replacement work");
    Reset();grill=true;duringApply=[](const Record& rec){CHECK(rec.floats.size()==2 && rec.floats[1][2]==burn);};Run(Intent);CHECK(calls==1);
    Reset();grill=true;S::ExpectRecordFor(&actor,L"item",1);duringApply=[](const Record& rec){CHECK(rec.floats.empty());};Run(Intent);CHECK(calls==1);
    Reset();live=false;Deliver(L"item",1,1,true);CHECK(S::g_parked.at(L"item").preserveGrillRuntime);
    live=grill=true;burn=.45f;duringApply=[](const Record& rec){CHECK(rec.floats[1][2]==.45f);};CHECK(S::ApplyParked(&actor,L"item"));
    Reset();live=false;S::ExpectRecordFor(&actor,L"item",1);Deliver(L"item",1,1,true);CHECK(!S::g_parked.at(L"item").preserveGrillRuntime);
    live=grill=true;duringApply=[](const Record& rec){CHECK(rec.floats.empty());};CHECK(S::ApplyParked(&actor,L"item"));
    puts("PASS: host grill burn/active survives ordinary and parked client records; exact initial inventory birth retains its source runtime");
    // New same-key input can arrive during normalization, capture or transport.
    for(bool inGrill:{false,true}){
        Reset();grill=inGrill;auto& callback=inGrill?duringRefresh:duringLock;
        callback=[]{Deliver(L"item",2,2,true);};Run(Intent);
        CHECK(calls==1 && sent==std::vector<int>({2}) && noted==std::vector<int>({2}) && S::g_parked.empty());
    }
    Reset();duringSend=[]{Deliver(L"item",2,2,true);};Run(Intent);
    CHECK(calls==1 && sent==std::vector<int>({1,2}) && noted==std::vector<int>({2}) && S::g_parked.empty() && S::g_retry.empty());
    Reset();grill=true;S::g_session=&session;S::Park(L"item",Make(),1,100);S::g_parked.at(L"item").preserveGrillRuntime=true;
    duringRefresh=[]{Deliver(L"item",2,2,true);};CHECK(!S::ApplyParked(&actor,L"item"));
    CHECK(!calls && S::g_parked.at(L"item").rec.ints[0][0]==2 && !S::g_parked.at(L"item").failures);
    CHECK(S::ApplyParked(&actor,L"item") && calls==1 && noted==std::vector<int>({2}));
    Reset();captureOK=true;duringCapture=[]{Deliver(L"item",2,2,true);};
    CHECK(!S::Publish(&session,&actor,L"item") && sent==std::vector<int>({2}) && calls==1 && S::g_retry.empty());
    Reset();captureOK=true;onlyLive=L"item";duringCapture=[]{Deliver(L"other",2,2,true);};
    CHECK(S::Publish(&session,&actor,L"item") && sent==std::vector<int>({2,1})); // unrelated key remains independent
    for(int cause=0;cause<5;++cause){
        Reset();captureOK=true;sendOK=false;duringSend=[cause]{if(cause==0)S::OnDisconnect();if(cause==1)++world;
            if(cause==2)S::OnPeerGone(1);if(cause==3)++serial;if(cause==4)liveKey=L"new_key";};
        CHECK(!S::PublishToSlot(&session,1,&actor,L"item") && S::g_retry.empty());
    }
    Reset();duringSend=[]{S::OnDisconnect();Deliver(L"item",2,2,true);};Run(Intent);
    CHECK(calls==1 && noted==std::vector<int>({2}) && S::g_parked.empty());
    puts("PASS: same-key normalization/capture/send supersession, independent other keys, native pre-apply cancellation and stale-transfer retry refusal");
    Reset();S::ExpectRecordFor(&actor,L"item",1);CHECK(!S::PrepareForMutation(&actor,L"item"));
    Deliver(L"item",1,1,true);CHECK(S::PrepareForMutation(&actor,L"item"));
    Reset();S::Park(L"item",Make(),1,100);success=false;CHECK(!S::PrepareForMutation(&actor,L"item"));
    success=true;CHECK(S::PrepareForMutation(&actor,L"item") && S::g_parked.empty());
    puts("PASS: readiness/ownership waits preserve retry budget without blocking ready peers");
    puts("PASS: independent per-recipient/broadcast retries, attempt counts and departure cleanup");
    puts("PASS: unindexed live-source retry for capture/send failures; dead/recycled/rekeyed sources refused");
    puts("PASS: failed capture retries, recovery, invalidated capture exclusion and bounded failure");
    puts("PASS: item record reentry, replacement, teardown, ordering, bounded work, fairness and retry lifecycle");
}

namespace ue_wrap::food {bool IsFood(void*){return false;}}
namespace coop::props::prop_food_sync {bool PublishSpawnState(coop::net::Session&,void*,int){std::abort();}}
