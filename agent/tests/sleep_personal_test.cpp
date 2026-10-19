#include <cassert>
#include <cstdio>
#include <cstdarg>
#include "../../src/votv-coop/src/coop/player/sleep_sync.cpp"

namespace coop::text {std::string ToUtf8(const std::wstring&){return "Host";}}
namespace l10n {
const char* T(const char* text){return text;}
int Fmt(char* out,size_t size,const char*,...){return sprintf_s(out,size,"Host is asleep");}
}

int main(){
    namespace S=coop::sleep_sync;namespace L=ue_wrap::sleep;using namespace coop::net;
    Session session;
    auto reset=[&](Role role){S::OnDisconnect();session={};session.mode=role;
        L::sleeping=L::dreaming=false;L::probability=.37f;L::dilation=1;L::need=10;L::wakeCalls=0;
        L::bed=L"bed_C";S::Install(&session);S::Tick();};
    auto receive=[&](SleepStatePayload p,uint8_t sender){S::OnReliable(reinterpret_cast<const uint8_t*>(&p),sizeof(p),sender);};
    for(auto role:{Role::Host,Role::Client}){
        reset(role);L::sleeping=true;L::dilation=role==Role::Host?20.f:1.f;S::Tick();
        assert(L::probability==.37f && L::need==10 && !L::wakeCalls);
        assert(S::LocalInBed());
        // Native nightmare entry changes only this peer's reported state.
        // No dream scene, wake or probability override is replayed.
        L::sleeping=false;L::dreaming=true;S::Tick();
        assert(S::g_localState==SleepPeerState::Dreaming && L::probability==.37f);
        S::OnDreamScene({},0);S::OnDreamScene({},2);S::Tick();
        assert(L::dreaming && !L::wakeCalls && L::need==10 && L::probability==.37f);
        L::dreaming=false;S::Tick();assert(!S::LocalInBed());
        S::OnDisconnect();assert(L::probability==.37f);
    }
    reset(Role::Client);L::sleeping=true;S::Tick();
    assert(session.sent.size()==1 && session.sent[0].op==uint8_t(SleepOp::ClientRest) &&
        session.sent[0].state==uint8_t(SleepPeerState::Resting) && StringFromWireKey(session.sent[0].bed)==L"bed_C");
    L::dreaming=true;L::sleeping=false;L::bed.clear();S::Tick();
    assert(session.sent.back().state==uint8_t(SleepPeerState::Dreaming) &&
        StringFromWireKey(session.sent.back().bed)==L"bed_C");
    receive({uint8_t(SleepOp::HostAsleep),1,0,0,{}},2);assert(!S::HostAsleepNow());
    receive({uint8_t(SleepOp::HostAsleep),1,0,0,{}},0);assert(S::HostAsleepNow() && L::probability==.37f);
    assert(!L::wakeCalls && L::dilation==1);
    receive({uint8_t(SleepOp::PeerRest),uint8_t(SleepPeerState::Dreaming),2,0,{}},0);
    assert(S::PeerState(2)==SleepPeerState::Dreaming);
    receive({uint8_t(SleepOp::PeerRest),uint8_t(SleepPeerState::Awake),2,0,{}},2);
    assert(S::PeerState(2)==SleepPeerState::Dreaming);
    reset(Role::Host);session.ready[1]=true;
    receive({uint8_t(SleepOp::ClientRest),uint8_t(SleepPeerState::Resting),0,0,{}},1);
    assert(S::PeerState(1)==SleepPeerState::Resting);
    receive({uint8_t(SleepOp::HostAsleep),1,0,0,{}},1);assert(!S::g_hostSleeping);
    S::OnDisconnectForSlot(1);assert(S::PeerState(1)==SleepPeerState::Awake);
    puts("PASS personal native nightmare probability/entry, unchanged wake/rest state, real 36-byte rest reports, sender authority and peer cleanup");
}
