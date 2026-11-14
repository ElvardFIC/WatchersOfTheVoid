#define EVENT_SLOT_NO_MAIN
#include "event_slot_canonical_test.cpp"

namespace test {
constexpr const char* boundaries[] = {
    "client initial digest", "client scalar read", "client content read", "client final digest",
    "client transport completion", "client baseline completion", "client refusal bookkeeping",
    "host initial digest", "host previous scalar", "host previous content", "host apply completion",
    "host response continuation", "client sweep digest", "client overdue continuation", "laptop settled digest"
};
constexpr const char* causes[] = {"world", "session", "actor", "class", "peer"};
void Client(coop::net::Session& session) {
    Host(session);session.mode=coop::net::Role::Client;S::Install(&session);
}
void ClaimPacket(const S::Slot& value,bool trailing=false) {
    incoming={S::kOpClaim,static_cast<uint8_t>(box)};
    std::vector<uint8_t> row;CHECK(S::PackOne(0,value,row));
    incoming.insert(incoming.end(),row.begin(),row.end());
    if(trailing)incoming.push_back(42);
}
void RunBoundary(int stage,int cause) {
    coop::net::Session session;
    const bool host=stage>=7 && stage<=11;
    if(host)Host(session);else Client(session);
    actorWorldKnown=false; // Exercise the frame guard independently of CachedObjRef's world stamp.
    const int peer=host?1:0;
    bool retired=false;
    const auto retire=[&] {
        retired=true;
        if(cause==0)++world;
        if(cause==1)++serial;
        if(cause==2)++actor.serial;
        if(cause==3)++cls.serial;
        if(cause==4)++session.generations[peer];
    };
    if(stage==0 || stage==3 || stage==7 || stage==11 || stage>=12){
        digestTrigger=stage==3?3:stage==11?4:stage==13?2:1;onDigest=retire;
    }
    if(stage==1 || stage==8)onScalar=retire;
    if(stage==2 || stage==9)onContent=retire;
    if(stage==4 || stage==6)onSend=retire;
    if(stage==5)onQuad=retire;
    if(stage==6)session.allowed[0]=false;
    if(stage==10)onWrite=retire;
    if(stage<=6){
        const auto kind=stage==5?F::DeviceKind::Laptop:box;
        S::ClientClaim(&session,kind,0,&actor);
        CHECK(retired && S::g_shadow.empty() && S::g_awaiting.empty());
        CHECK(attempts==((stage==4 || stage==6)?1:0));
    }else if(host){
        const S::PeerFrame source{&session,peer};
        S::HostClaim(source,box,0,&actor,Value({L"accepted-row"}));
        CHECK(retired && !attempts);
        CHECK(writes==(stage>=10?1:0));
        CHECK(!S::g_hostClaiming && !S::g_hostPublishing);
    }else if(stage==12 || stage==13){
        S::g_haveCanonical[static_cast<uint8_t>(box)]=true;S::g_shadow[Key()]=99;
        if(stage==13)S::g_awaiting[Key()]={0,S::kMaxClaimTries};
        S::SweepKind(&session,box,false);
        CHECK(retired && !attempts && S::g_shadow.at(Key())==99 && !S::g_sweeping);
    }else{
        laptopReady=true;S::g_shadow[Key(F::DeviceKind::Laptop)]=native.c.data.size();
        S::g_haveCanonical[static_cast<uint8_t>(F::DeviceKind::Laptop)]=true;
        CHECK(!S::LaptopOccupancySettled() && retired);
    }
    std::printf("PASS FC%02d: %s / %s\n",stage*5+cause+1,boundaries[stage],causes[cause]);
    S::OnDisconnect();
}
}

int main(){using namespace test;
    for(int stage=0;stage<15;++stage)for(int cause=0;cause<5;++cause)RunBoundary(stage,cause);
    coop::net::Session session;
    Client(session);S::ClientClaim(&session,box,0,&actor);
    CHECK(attempts==1 && S::g_shadow.at(Key())==2 && S::g_awaiting.count(Key()));
    Client(session);session.allowed[0]=false;S::ClientClaim(&session,box,0,&actor);
    CHECK(attempts==1 && S::g_retry.count(Key()) && S::g_shadow.empty() && S::g_awaiting.empty());
    Client(session);onSend=[&]{S::StageCanonical(box,0,Value({L"host-answer"}),0,123);};
    S::ClientClaim(&session,box,0,&actor);CHECK(attempts==1 && S::g_awaiting.empty() && S::g_shadow.empty() && !S::g_retry.count(Key()));
    Client(session);onSend=[]{native.c.data.push_back(L"new-local-row");};
    S::ClientClaim(&session,box,0,&actor);CHECK(S::g_shadow.at(Key())==2 && native.c.data.size()==3);
    S::g_haveCanonical[static_cast<uint8_t>(box)]=true;S::SweepKind(&session,box,false);
    CHECK(attempts==2 && S::g_shadow.at(Key())==3);
    Client(session);onRead=[]{native.c.data.push_back(L"changed-read");};
    S::ClientClaim(&session,box,0,&actor);CHECK(!attempts && S::g_retry.count(Key()));
    Client(session);onQuad=[]{native.c.data.push_back(L"changed-baseline");};
    S::ClientClaim(&session,F::DeviceKind::Laptop,0,&actor);CHECK(!attempts && S::g_retry.count(Key(F::DeviceKind::Laptop)));
    Client(session);onSend=[&]{S::ClientClaim(&session,box,0,&actor);};
    S::ClientClaim(&session,box,0,&actor);CHECK(attempts==1 && S::g_retry.count(Key()) && S::g_awaiting.empty());
    for(bool host:{false,true}){
        if(host)Host(session);else Client(session);
        onRead=[&]{S::OnPeerGone(host?1:0);};
        if(host)S::HostClaim(S::PeerFrame{&session,1},box,0,&actor,Value({L"new"}));
        else S::ClientClaim(&session,box,0,&actor);
        CHECK(!writes && !attempts);
    }
    Host(session);const auto value=Value({L"accepted"});
    ClaimPacket(value);coop::net::BlobChunkPayload chunk{};S::OnChunk(chunk,1);
    CHECK(writes==1 && attempts==2 && native.c.data==value.c.data);
    Host(session);ClaimPacket(value,true);S::OnChunk(chunk,1);CHECK(!writes && !attempts);
    Host(session);ClaimPacket(value);session.ready[1]=false;S::OnChunk(chunk,1);CHECK(!writes && !reads);
    Host(session);readReady=false;S::HostClaim(S::PeerFrame{&session,1},box,0,&actor,value);CHECK(!writes && !attempts);
    Host(session);partialWrite=true;writeReady=false;
    S::HostClaim(S::PeerFrame{&session,1},box,0,&actor,value);
    CHECK(writes==1 && attempts==2 && native.c.data==value.c.data);
    S::SweepKind(&session,box,true);CHECK(writes==1 && attempts==2);
    Host(session);onWrite=[&]{S::HostClaim(S::PeerFrame{&session,2},box,0,&actor,Value({L"nested"}));};
    S::HostClaim(S::PeerFrame{&session,1},box,0,&actor,value);
    CHECK(writes==1 && native.c.data==value.c.data);
    Client(session);laptopReady=true;S::g_shadow[Key(F::DeviceKind::Laptop)]=native.c.data.size();
    S::g_haveCanonical[static_cast<uint8_t>(F::DeviceKind::Laptop)]=true;
    CHECK(S::LaptopOccupancySettled());S::g_awaiting[Key(F::DeviceKind::Laptop)]={1,0};CHECK(!S::LaptopOccupancySettled());
    Host(session);
    auto large=Value({L""});std::vector<uint8_t> packed;CHECK(S::PackOne(0,large,packed));
    large.c.data[0].assign(S::kMaxSlotBytes-packed.size(),L'x');
    CHECK(S::PackOne(0,large,packed) && packed.size()==S::kMaxSlotBytes);
    ClaimPacket(large);S::OnChunk(chunk,1);CHECK(writes==1 && attempts==2);
    Host(session);large.c.data[0].push_back(L'x');
    incoming={S::kOpClaim,static_cast<uint8_t>(box),0};S::PackSlot(incoming,large);
    CHECK(incoming.size()<=coop::blob_chunks::MaxBlobBytes());
    S::OnChunk(chunk,1);CHECK(!writes && !attempts);
    puts("PASS M10: maximum publishable claim accepted, one byte over refused before native writes");
    puts("PASS claim integration: success/refusal, early canonical, newer state, partial read/baseline, reentry, departure, parser and uncertain-write correction");
}
