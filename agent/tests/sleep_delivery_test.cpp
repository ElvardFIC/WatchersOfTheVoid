#define main sleep_baseline_main
#include "sleep_personal_test.cpp"
#undef main
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::sleep_sync;
namespace L=ue_wrap::sleep;
using namespace coop::net;
namespace {
Session session;
SleepStatePayload Rest(SleepPeerState state=SleepPeerState::Resting){SleepStatePayload p{};p.op=uint8_t(SleepOp::ClientRest);p.state=uint8_t(state);return p;}
void Receive(SleepStatePayload p,int from=1){S::OnReliable(reinterpret_cast<const uint8_t*>(&p),sizeof(p),uint8_t(from));}
void Reset(Role role=Role::Host){
    S::OnDisconnect();session={};session.mode=role;sleepTestNow=1000;
    ++ue_wrap::world_identity::testGeneration;++session_serial::testSerial;
    L::sleeping=L::dreaming=false;L::bed=L"bed_C";
    S::Install(&session);
}
void Join(int slot){session.ready[slot]=true;S::QueueConnectBroadcastForSlot(slot);}
void ClearSent(){session.sent.clear();session.destinations.clear();}
bool Sent(int to,SleepOp op,int slot,int state){
    for(size_t i=0;i<session.sent.size();++i){const auto& p=session.sent[i];
        if(session.destinations[i]==to && p.op==uint8_t(op) && p.slot==slot && p.state==state)return true;}
    return false;
}
void Owe(SleepPeerState state){
    auto& pending=S::g_pending[1];pending={};pending.state=state;
    for(int i=0;i<5;++i)pending.pace.Offer(S::kRestBudget,NowMs());
    CHECK(pending.pace.Owed());
}
}
int main(){
    Reset();Join(1);ClearSent();session.sendOk=false;L::sleeping=true;S::Tick();
    CHECK(S::g_hostDebt[1] && S::g_restDebt[1][0]);
    const auto attempts=session.attempts;S::Tick();S::Tick();CHECK(session.attempts==attempts);
    session.sendOk=true;sleepTestNow+=100;S::Tick();
    CHECK(Sent(1,SleepOp::HostAsleep,0,1) && Sent(1,SleepOp::PeerRest,0,int(SleepPeerState::Resting)));
    CHECK(!S::g_hostDebt[1] && !S::g_restDebt[1][0]);
    // A newer wake supersedes refused sleep levels, without replaying stale sleep.
    session.sendOk=false;L::sleeping=false;S::Tick();L::sleeping=true;S::Tick();L::sleeping=false;S::Tick();
    ClearSent();session.sendOk=true;sleepTestNow+=100;S::Tick();
    CHECK(Sent(1,SleepOp::HostAsleep,0,0) && Sent(1,SleepOp::PeerRest,0,0));
    for(const auto& p:session.sent)CHECK(p.state==0);
    std::puts("PASS host sleep/peer delivery: refused edges remain owed, newest state supersedes and retries are bounded");

    Reset();Join(1);Join(2);ClearSent();session.sendOk=false;Receive(Rest());
    CHECK(S::g_restDebt[2][1]);session.sendOk=true;sleepTestNow+=100;S::PayOwed(session);
    CHECK(Sent(2,SleepOp::PeerRest,1,int(SleepPeerState::Resting)) && !S::g_restDebt[2][1]);
    // Explicit awake rows heal a client's old mirror, including a missing leave.
    ClearSent();session.sendOk=false;Join(3);CHECK(S::g_hostDebt[3]);
    session.sendOk=true;sleepTestNow+=100;S::PayOwed(session);
    CHECK(Sent(3,SleepOp::HostAsleep,0,0));
    for(int slot:{0,2,3})CHECK(Sent(3,SleepOp::PeerRest,slot,0));
    CHECK(Sent(3,SleepOp::PeerRest,1,int(SleepPeerState::Resting)));
    std::puts("PASS peer relay and complete join repair: exact-recipient retry includes awake rows and current host flag");

    Reset();Join(1);Join(2);Receive(Rest());L::sleeping=true;CHECK(S::AllPlayersResting()==false);
    Receive(Rest(),2);CHECK(S::AllPlayersResting());
    ++session.generations[1];CHECK(!S::AllPlayersResting());
    ClearSent();S::Tick();CHECK(S::PeerState(1)==SleepPeerState::Awake);
    CHECK(Sent(2,SleepOp::PeerRest,1,0));Receive(Rest());CHECK(S::AllPlayersResting());
    session.ready[1]=false;session.joining[1]=true;S::Tick();session.ready[1]=true;S::Tick();
    CHECK(S::PeerState(1)==SleepPeerState::Awake && !S::AllPlayersResting());
    Receive(Rest());Owe(SleepPeerState::Awake);CHECK(!S::AllPlayersResting());
    S::g_pending[1].pace.Withdraw();CHECK(S::AllPlayersResting());
    L::dreaming=true;CHECK(!S::AllPlayersResting());L::dreaming=false;
    Reset();Join(1);L::sleeping=true;
    for(auto state:{SleepPeerState::Resting,SleepPeerState::Awake,SleepPeerState::Dreaming,SleepPeerState::Resting,SleepPeerState::Awake})Receive(Rest(state));
    CHECK(S::PeerState(1)==SleepPeerState::Resting && S::g_pending[1].pace.Owed() && !S::AllPlayersResting());
    sleepTestNow+=1000;S::PayOwed(session);CHECK(S::PeerState(1)==SleepPeerState::Awake);
    std::puts("PASS unanimous sleep: replacement/readiness needs a new rest report; paced wake and host dream veto acceleration immediately");

    Reset();Join(1);Owe(SleepPeerState::Resting);Session foreign;foreign.mode=Role::Host;
    S::PayOwed(foreign);CHECK(S::g_pending[1].pace.Owed());
    session.ready[1]=false;sleepTestNow+=5000;S::PayOwed(session);
    CHECK(S::PeerState(1)==SleepPeerState::Awake && !S::g_pending[1].pace.Owed());
    session.ready[1]=true;S::Tick();Owe(SleepPeerState::Resting);S::OnDisconnect();
    CHECK(!S::g_pending[1].pace.Owed());S::Install(&session);sleepTestNow+=5000;S::PayOwed(session);
    CHECK(S::PeerState(1)==SleepPeerState::Awake);
    std::puts("PASS paced reports: foreign sessions cannot consume debt; unready/departed/session-ended reports never return");

    Reset();Join(1);session.active=false;Receive(Rest());CHECK(S::PeerState(1)==SleepPeerState::Awake);
    session.active=true;session.linked=false;Receive(Rest());CHECK(S::PeerState(1)==SleepPeerState::Awake);
    session.linked=true;auto p=Rest();p._pad=1;Receive(p);CHECK(S::PeerState(1)==SleepPeerState::Awake);
    S::OnReliable(nullptr,sizeof(p),1);CHECK(S::PeerState(1)==SleepPeerState::Awake);
    Reset(Role::Client);p={};p.op=uint8_t(SleepOp::PeerRest);p.slot=2;p.bed.len=1;p.bed.data[0]='b';
    Receive(p,0);CHECK(S::g_bed[2].len==0);
    std::puts("PASS active wire authority: stopped/disconnected input, null payload, padding and awake-with-bed are refused on both roles");

    for(int change=0;change<3;++change){
        Reset();Join(1);Receive(Rest());Owe(SleepPeerState::Awake);
        if(change==0)++ue_wrap::world_identity::testGeneration;
        if(change==1)++session_serial::testSerial;
        if(change==2)session.mode=Role::Client;
        S::Tick();CHECK(S::PeerState(1)==SleepPeerState::Awake && !S::g_pending[1].pace.Owed());
    }
    // Reentry cannot clear a newer debt or continue a send into the new world.
    Reset();Join(1);Join(2);ClearSent();S::g_hostSleeping=true;
    session.duringSend=[]{S::g_hostSleeping=false;S::SendHostAsleep(&session,1);};
    S::SendHostAsleep(&session,1);CHECK(S::g_hostDebt[1]);
    ClearSent();S::PayOwed(session);CHECK(Sent(1,SleepOp::HostAsleep,0,0));
    session.duringSend=[]{++ue_wrap::world_identity::testGeneration;};ClearSent();
    S::SendHostAsleep(&session,-1);CHECK(session.sent.size()==1);
    std::puts("PASS lifetime: world/serial/role resets all sleep state, reentrant changes retain newer debt and old sends stop");

    Reset(Role::Client);S::Tick();CHECK(session.sent.size()==1 && session.sent[0].state==0);
    ClearSent();session.duringSend=[]{L::sleeping=true;S::Tick();};S::g_haveSent=false;S::Tick();
    CHECK(S::g_sentState==SleepPeerState::Resting);
    Reset(Role::Client);L::bed.clear();
    session.duringSend=[]{L::sleeping=true;S::g_localState=SleepPeerState::Resting;S::g_localSleeping=true;};
    S::Tick();CHECK(S::g_sentState==SleepPeerState::Awake);S::Tick();
    CHECK(session.sent.size()==2 && session.sent.back().state==uint8_t(SleepPeerState::Resting));
    Reset(Role::Client);session.duringSend=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_haveSent);
    std::puts("PASS client publication: initial awake is reported; latch records exact payload, newer nested send wins and disconnect cannot mark sent");
    Reset();L::sleeping=true;
    for(int slot=1;slot<kMaxPeers;++slot){session.ready[slot]=true;Receive(Rest(),slot);}
    CHECK(S::AllPlayersResting());Receive(Rest(SleepPeerState::Awake),kMaxPeers-1);CHECK(!S::AllPlayersResting());
    ClearSent();S::QueueConnectBroadcastForSlot(kMaxPeers-1);
    CHECK(Sent(kMaxPeers-1,SleepOp::PeerRest,kMaxPeers-1,0));
    std::puts("PASS 32-seat sleep votes and full snapshot repair, including the last recipient/subject slot");
}
