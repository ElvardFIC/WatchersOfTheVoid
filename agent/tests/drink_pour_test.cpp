#define DRINK_SPILL_FIXTURE_ONLY
#include "drink_spill_test.cpp"
namespace pour_test {
using namespace spill_test;
using Pour=coop::net::DrinkPourPayload;
void NativePour(){test::activeInvocations={1};ue_wrap::script_gate::Call call{};call.object=&test::actors[0];call.invocation=1;
    CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Run);D::ObservePourSound(nullptr,&test::actors[0],nullptr);
}
Pour Request(uint32_t seq=1){Pour p{};p.sequence=seq;p.generation=7;p.slot=1;p.playerNo=11;p.x=20;return p;}
Pour NativePacket(){auto p=Request();p.slot=0;p.playerNo=10;p.generation=0;p.op=1;return p;}
}
int main(){using namespace spill_test;using namespace test;using coop::net::Role;using pour_test::NativePour;coop::net::Session session;
    spill_test::Reset(session,Role::Client);D::ObservePourSound(nullptr,&actors[0],nullptr);CHECK(D::g_pourLocal.empty());
    NativePour();D::ObservePourSound(nullptr,&actors[0],nullptr);CHECK(D::g_pourLocal.size()==1);D::Tick();CHECK(session.pours.empty());
    activeInvocations.clear();actors[0].live=false;hand=nullptr;generation=99;D::Tick();CHECK(session.pours.size()==1);
    const auto request=session.pours[0].payload;CHECK(request.generation==7 && request.playerNo==11 && request.x==20 && !request.op);
    spill_test::Reset(session,Role::Host);D::ReceivePour(request,1);D::Tick();CHECK(pourSounds==1 && authors==0 && D::g_flights.empty() && D::g_records.empty() && session.pours.size()==3);
    const auto canonical=session.pours[0].payload;CHECK(canonical.op==1 && session.pours[0].slot==1);
    D::ReceivePour(request,1);D::Tick();CHECK(pourSounds==1 && session.pours.size()==4);D::g_pourRequests.clear();D::ReceivePour(request,1);D::Tick();CHECK(pourSounds==1);
    spill_test::Reset(session,Role::Client);NativePour();activeInvocations.clear();D::Tick();auto wrong=canonical;wrong.x+=1;
    D::ReceivePour(wrong,0);CHECK(D::g_pourLocal.size()==1);D::ReceivePour(canonical,2);CHECK(D::g_pourLocal.size()==1);
    D::ReceivePour(canonical,0);D::Tick();CHECK(D::g_pourLocal.empty() && !pourSounds);
    spill_test::Reset(session,Role::Client);local=2;coop::players::Registry::Get().peer=2;D::ReceivePour(canonical,0);D::Tick();CHECK(pourSounds==1);
    D::ReceivePour(canonical,0);D::Tick();CHECK(pourSounds==1);
    // Canonical spatial audio can arrive after its source left or before roster.
    // A retired person's sequence must not mask the next person in that slot.
    spill_test::Reset(session,Role::Client);local=2;coop::players::Registry::Get().peer=2;++people[1].playerNo;
    auto retiredSource=canonical;retiredSource.sequence=1000;D::ReceivePour(retiredSource,0);D::Tick();CHECK(pourSounds==1);
    auto successor=canonical;successor.playerNo=people[1].playerNo;successor.sequence=1;D::ReceivePour(successor,0);D::Tick();CHECK(pourSounds==2);
    D::ReceivePour(retiredSource,0);D::Tick();CHECK(pourSounds==2);
    // Host-native source sound was already heard by the host and is transient.
    spill_test::Reset(session,Role::Host);NativePour();activeInvocations.clear();D::Tick();CHECK(!pourSounds && session.pours.size()==3);
    auto native=session.pours[0].payload;session.pours.clear();D::SendAllToSlot(2);D::Tick();CHECK(session.pours.empty());
    // Independent recipient and local tracks preserve order across cue kinds.
    spill_test::Reset(session,Role::Host);auto next=native;next.sequence=2;next.kind=2;
    CHECK(D::QueuePour(native,false) && D::QueuePour(next,false));session.allowPour=[](int slot,const auto& p){return slot!=2 || p.kind!=0;};D::Tick();
    CHECK(session.pours.size()==4);for(const auto& sent:session.pours)CHECK(sent.slot!=2);
    session.allowPour={};D::Tick();CHECK(session.pours.size()==6 && session.pours[4].payload.kind==0 && session.pours[5].payload.kind==2);
    spill_test::Reset(session,Role::Client);blockedPourKind=0;D::ReceivePour(native,0);D::ReceivePour(next,0);D::Tick();CHECK(!pourSounds);
    blockedPourKind=-1;D::Tick();CHECK(pourOrder==std::vector<uint8_t>({0,2}));
    // Native, source and occupant interruptions cannot publish an old capture.
    for(int cause=0;cause<4;++cause){spill_test::Reset(session,Role::Client);onPourRead=[&,cause]{if(cause==0)D::OnDisconnect();if(cause==1)++world;
        if(cause==2)++actors[0].serial;if(cause==3)session.mode=Role::Host;};NativePour();CHECK(D::g_pourLocal.empty());}
    for(int cause=0;cause<7;++cause){spill_test::Reset(session,Role::Host);onReach=[&,cause]{if(cause==0)D::OnDisconnect();if(cause==1)++world;
        if(cause==2)D::OnPeerLeft(1);if(cause==3)++people[1].playerNo;if(cause==4)++actors[0].serial;if(cause==5)session.mode=Role::Client;if(cause==6)session.ready[1]=false;};
        D::ReceivePour(request,1);D::Tick();CHECK(!pourSounds);}
    spill_test::Reset(session,Role::Host);onReach=[&]{D::OnPeerLeft(1);D::ReceivePour(request,1);};D::ReceivePour(request,1);D::Tick();CHECK(!pourSounds);
    D::Tick();CHECK(pourSounds==1);
    spill_test::Reset(session,Role::Host);onReach=[]{++D::g_peerEpoch[1];};D::ReceivePour(request,1);D::Tick();CHECK(!pourSounds);
    spill_test::Reset(session,Role::Client);NativePour();activeInvocations.clear();++people[1].playerNo;D::Tick();CHECK(session.pours.empty() && D::g_pourLocal.empty());
    // Request readiness/ack backpressure and retained sequence history are separate.
    spill_test::Reset(session,Role::Host);sourceReady=false;D::ReceivePour(request,1);D::Tick();CHECK(!pourSounds && D::g_pourRequests.size()==1);
    sourceReady=true;session.allowPour=[](int,const auto&){return false;};D::Tick();CHECK(pourSounds==1 && session.pours.empty());
    session.allowPour={};D::Tick();CHECK(pourSounds==1 && session.pours.size()==3);
    spill_test::Reset(session,Role::Host);sourceReady=false;D::ReceivePour(request,1);D::g_pourRequests.begin()->second.expires=0;D::Tick();
    CHECK(!pourSounds && session.pours.size()==1 && session.pours[0].payload.op==2);
    spill_test::Reset(session,Role::Client);NativePour();activeInvocations.clear();auto refused=request;refused.op=2;D::ReceivePour(refused,0);CHECK(D::g_pourLocal.empty() && !pourSounds);
    // An older pending origin blocks only its own later cue, until terminal.
    spill_test::Reset(session,Role::Host);auto older=request;older.generation=8;auto later=request;later.sequence=2;
    D::ReceivePour(older,1);D::ReceivePour(later,1);D::Tick();CHECK(!pourSounds);D::g_pourRequests.begin()->second.expires=0;D::Tick();D::Tick();CHECK(pourSounds==1);
    // A peer epoch fences a recipient even if its player number is unchanged.
    spill_test::Reset(session,Role::Host);D::QueuePour(native,false);++D::g_peerEpoch[2];D::Tick();
    CHECK(session.pours.size()==2);for(const auto& sent:session.pours)CHECK(sent.slot!=2);
    for(int cause=0;cause<4;++cause){spill_test::Reset(session,Role::Client);D::ReceivePour(native,0);onPourPlay=[&,cause]{if(cause==0)D::OnDisconnect();
        if(cause==1)++world;if(cause==2)session.mode=Role::Host;if(cause==3)D::g_pours.clear();};D::Tick();CHECK(D::g_pours.empty() || !D::Current());}
    for(int cause=0;cause<8;++cause){spill_test::Reset(session,Role::Host);auto bad=request;uint8_t sender=1;
        if(cause==0)sender=2;if(cause==1)bad.op=1;if(cause==2)bad.kind=3;if(cause==3)bad.reserved[0]=1;if(cause==4)bad.x=NAN;
        if(cause==5)bad.playerNo=12;if(cause==6)bad.sequence=0;if(cause==7){bad.kind=2;coop::net::WireKeyFromString(L"world_coffee",bad.source);}
        D::ReceivePour(bad,sender);D::Tick();CHECK(!pourSounds && D::g_pourRequests.empty());}
    spill_test::Reset(session,Role::Client);pourReady=false;ue_wrap::script_gate::Call call{};call.object=&actors[0];call.invocation=1;
    CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Run);
    NativeSpill();activeInvocations.clear();D::Tick();CHECK(session.launches.size()==1);
    spill_test::Reset(session,Role::Client);call.object=&actors[0];call.invocation=1;CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Run);
    activeInvocations.clear();D::ObservePourSound(nullptr,&actors[0],nullptr);CHECK(D::g_pourLocal.empty());
    puts("PASS drink pour: native invocation/point capture, host source proof, exact ACK, owner exclusion, independent FIFO recipients and local audio, replay/expiry, same-person epochs and native/send lifecycle");return 0;
}
