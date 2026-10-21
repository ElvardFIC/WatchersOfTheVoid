#include "support_2135.h"
#include "../../src/votv-coop/src/coop/interactables/grill_hand_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <limits>
namespace test {
namespace S=coop::grill_hand_sync;
using Packet=coop::net::GrillHandPayload;
using Role=coop::net::Role;
uint16_t people[4]{10,11,12,13};
uint32_t generation=1,mirrorGeneration=1;
void* local=nullptr;void* mirror=&actor;
bool grill=true,readable=true,paintable=true,sound=false;
int paints=0,stops=0;
ue_wrap::grill::State state{true,true,true,4,.25f,.08f};
}
namespace coop::hand_item {
void* LocalHandActor(){return test::local;}
uint32_t Generation(uint8_t){return test::generation;}
uint32_t MirrorGeneration(uint8_t){return test::mirrorGeneration;}
void* MirrorActorForSlot(uint8_t){return test::mirror;}
bool IsMirrorActor(void* a){return a==&test::actor || a==&test::ui;}
}
namespace coop::roster_ledger {const Row& Get(int slot){static Row rows[4];rows[slot].playerNo=test::people[slot];return rows[slot];}}
namespace ue_wrap::grill {
bool IsGrill(void* a){return a && test::grill;}
bool Valid(const State& s){return s.coals>=0 && s.coals<=16 && std::isfinite(s.burn) && s.burn>=0 && s.burn<=1.01f &&
    std::isfinite(s.volume) && s.volume>=0 && s.volume<=1;}
bool Read(void*,State& out){out=test::state;test::Callback();return test::readable;}
bool Apply(void*,const State& s,const std::function<bool()>& current){CHECK(current());++test::paints;
    test::state=s;test::sound=s.cooking;test::Callback();return test::paintable && current();}
bool StopSound(void*,const std::function<bool()>& current){if(current && !current())return false;
    ++test::stops;test::sound=false;test::Callback();return true;}
}
namespace test {
void Start(coop::net::Session& session,Role role){
    S::OnDisconnect();++world;session={};session.mode=role;
    coop::players::Registry::Get().peer=role==Role::Host?0:2;
    local=nullptr;mirror=&actor;generation=mirrorGeneration=1;grill=readable=paintable=true;
    state={true,true,true,4,.25f,.08f};actor.live=ui.live=true;paints=stops=0;sound=false;callback={};
    coop::element::reach=true;coop::element::callback={};S::Install(&session);
}
Packet Request(uint32_t seq=1){Packet p{};p.slot=1;p.playerNo=people[1];p.generation=1;p.sequence=seq;
    p.flags=7;p.coals=4;p.burn=.25f;p.volume=.08f;return p;}
Packet Decode(const coop::net::Session::Sent& sent){CHECK(sent.kind==coop::net::ReliableKind::GrillHand);
    Packet p{};CHECK(sent.data.size()==sizeof(p));std::memcpy(&p,sent.data.data(),sizeof(p));return p;}
}
int main(){
    using namespace test;coop::net::Session session;
    CHECK(coop::net::LaneForKind(coop::net::ReliableKind::GrillHand)==coop::net::Lane::Normal);
    CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::GrillHand));
    Start(session,Role::Host);local=&ui;S::Tick();CHECK(session.sent.size()==3);
    auto outgoing=Decode(session.sent.front());CHECK(outgoing.flags==7 && outgoing.coals==4 && outgoing.burn==.25f);
    session.sent.clear();now+=100;S::Tick();CHECK(session.sent.empty());
    state.lid=false;state.volume=.03f;now+=100;S::Tick();CHECK(session.sent.size()==3 && Decode(session.sent.back()).flags==5);
    Start(session,Role::Client);local=&ui;generation=0;S::Tick();CHECK(session.sent.empty());
    generation=1;now+=100;session.sendOk=false;S::Tick();CHECK(session.sent.empty());
    session.sendOk=true;S::Tick();CHECK(session.sent.size()==1 && session.sent.front().slot==0);
    auto owner=Decode(session.sent.back());CHECK(owner.slot==2 && owner.playerNo==people[2]);
    callback=[]{++generation;};now+=100;S::Tick();CHECK(session.sent.size()==1); // native read lost its source
    Start(session,Role::Host);auto p=Request();mirrorGeneration=0;S::Receive(p,1);S::Tick();CHECK(!paints && session.sent.empty());
    mirrorGeneration=1;coop::element::reach=false;S::Tick();CHECK(!paints && session.sent.empty());
    coop::element::reach=true;S::Tick();CHECK(paints==1 && sound && session.sent.size()==2);
    auto canonical=Decode(session.sent.front());CHECK(canonical.slot==1 && canonical.sequence==1);
    S::Receive(p,1);S::Tick();CHECK(paints==1); // replay cannot restart presentation
    p.sequence=2;p.flags=0;p.coals=3;S::Receive(p,1);S::Tick();CHECK(paints==2 && !sound && state.coals==3);
    Start(session,Role::Client);S::Receive(canonical,0);S::Tick();CHECK(sound && session.sent.empty());
    mirrorGeneration=2;S::Tick();CHECK(!sound && stops==1); // stow/replacement cleans the old loop
    canonical.generation=2;canonical.sequence++;S::Receive(canonical,0);S::Tick();CHECK(sound && paints==2);
    ++people[1];S::Tick();CHECK(!sound && !S::g_rows[1].packet.sequence);
    S::Receive(canonical,0);CHECK(!S::g_rows[1].packet.sequence);canonical.playerNo=people[1];canonical.sequence=1;
    S::Receive(canonical,0);S::Tick();CHECK(sound);S::OnDisconnect();CHECK(!sound);
    Start(session,Role::Host);p=Request();S::Receive(p,1);S::Tick();CHECK(sound);
    now+=3001;session.sent.clear();S::QueueConnect(2);S::Tick();CHECK(!sound);
    for(const auto& sent:session.sent)CHECK(!(Decode(sent).flags&4));const auto stopped=stops;
    S::Tick();CHECK(stops==stopped);p.sequence++;S::Receive(p,1);S::Tick();CHECK(sound);
    Start(session,Role::Host);p=Request();paintable=false;S::Receive(p,1);S::Tick();CHECK(session.sent.empty());
    paintable=true;now+=100;S::Tick();CHECK(sound && !session.sent.empty());
    Start(session,Role::Host);p=Request();grill=false;S::Receive(p,1);S::Tick();CHECK(!paints && session.sent.empty());
    grill=true;now+=3001;S::Tick();CHECK(!paints && session.sent.empty());
    Start(session,Role::Host);p=Request();session.ready=false;S::Receive(p,1);CHECK(!S::g_rows[1].packet.sequence);
    session.ready=true;auto invalid=p;invalid.playerNo++;S::Receive(invalid,1);S::Receive(p,2);CHECK(!S::g_rows[1].packet.sequence);
    for(int cause=0;cause<6;++cause){invalid=p;if(cause==0)invalid.flags=8;if(cause==1)invalid.volume=NAN;
        if(cause==2)invalid.burn=-1;if(cause==3)invalid.coals=17;if(cause==4)invalid.generation=0;if(cause==5)invalid.sequence=0;
        S::Receive(invalid,1);CHECK(!S::g_rows[1].packet.sequence);}
    S::Receive(p,1);callback=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session);
    Start(session,Role::Host);p=Request();S::Receive(p,1);coop::element::callback=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session && !paints);
    Start(session,Role::Host);p=Request();auto newer=p;newer.sequence++;newer.flags=0;
    callback=[&]{S::Receive(newer,1);};S::Receive(p,1);S::Tick();CHECK(!S::g_rows[1].admitted);
    now+=100;S::Tick();CHECK(!sound && S::g_rows[1].admitted && S::g_rows[1].packet.sequence==2);
    Start(session,Role::Host);p=Request();bool replaced=false;
    session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !replaced){replaced=true;++people[2];S::QueueConnect(2);}return true;};
    S::Receive(p,1);S::Tick();CHECK(replaced && (S::g_rows[1].pending&(1u<<2)));
    session.allowSend={};S::Tick();CHECK(!(S::g_rows[1].pending&(1u<<2)));
    const auto oldWorldStops=stops;++world;S::Tick();CHECK(!S::g_rows[1].packet.sequence && stops==oldWorldStops);
    Start(session,Role::Host);p=Request(50);S::Receive(p,1);S::Tick();CHECK(sound);
    S::OnPeerLeft(1);CHECK(!sound);p.sequence=1;S::Receive(p,1);S::Tick();CHECK(sound && S::g_rows[1].packet.sequence==1);
    mirror=nullptr;S::Tick();CHECK(!sound);mirror=&actor;
    Start(session,Role::Host);p=Request();auto nextGeneration=p;nextGeneration.generation=2;
    callback=[&]{S::OnPeerLeft(1);S::Receive(nextGeneration,1);};S::Receive(p,1);S::Tick();
    CHECK(S::g_rows[1].packet.generation==2 && !S::g_rows[1].admitted);
    mirrorGeneration=2;now+=100;S::Tick();CHECK(S::g_rows[1].admitted);
    Start(session,Role::Host);p=Request();replaced=false;
    session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !replaced){replaced=true;S::OnPeerLeft(1);
        mirrorGeneration=2;S::Receive(nextGeneration,1);S::Apply(now);}return true;};
    S::Receive(p,1);S::Tick();CHECK(S::g_rows[1].admitted && (S::g_rows[1].pending&(1u<<2)));session.allowSend={};
    Start(session,Role::Host);p=Request();S::Receive(p,1);S::Tick();coop::net::Session replacement,requested;
    callback=[&]{S::Install(&replacement);S::Receive(p,1);S::Tick();};S::Install(&requested);
    CHECK(S::g_session==&replacement && S::g_rows[1].packet.sequence==1 && sound);
    S::OnDisconnect();
    worldUnavailable=true;Start(session,Role::Host);p=Request();S::Receive(p,1);S::Tick();CHECK(sound);
    const auto previousStops=stops;++world;S::Tick();CHECK(stops==previousStops && !S::g_rows[1].packet.sequence);worldUnavailable=false;
    puts("PASS grill hand: exact generation/person, host reach/readiness validation, owner retry, canonical lid/fuel/burn/loop, mirror wait/replacement, expiry, stale join and reentrant packet/recipient/world fences");
}
