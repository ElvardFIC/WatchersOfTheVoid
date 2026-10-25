#include "support_2135.h"
#include <set>
#ifndef SPONGE_EFFECT_SOURCE
#define SPONGE_EFFECT_SOURCE "../../src/votv-coop/src/coop/props/sponge_effects_sync.cpp"
#endif
#include SPONGE_EFFECT_SOURCE
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
namespace test {
bool sponge=true,found=true,readable=true,surfaceOk=true,burstOk=true;
uint32_t sessionSerial=1;std::set<uint64_t> inactiveInvocations;
void* localHand=nullptr;void* mirror=nullptr;uint32_t generation=1,mirrorGeneration=1;
float soap=100;int surfaces=0,bursts=0;
std::wstring key=L"sponge";
std::array<coop::roster_ledger::Row,coop::net::kMaxPeers> roster{};
}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace ue_wrap::script_gate {bool IsInvocationActive(uint64_t id){return id && !test::inactiveInvocations.contains(id);}}
namespace ue_wrap::sponge_effects {
bool IsSponge(void* actor){return actor && reflection::IsLive(actor) && test::sponge;}
bool ReadUse(void*,void*,Use& use){use={{100,200,300},{0,0,1},test::soap};test::Callback();return test::readable && test::soap>0;}
bool ReadSoap(void*,float& value){value=test::soap;test::Callback();return test::readable;}
bool Surface(const FVector& location,const FVector& normal,const std::function<bool()>& current){CHECK(location.X==100 && normal.Z==1);test::Callback();if(!current() || !test::surfaceOk)return false;++test::surfaces;return true;}
bool Burst(void*,const std::function<bool()>& current){test::Callback();if((current && !current()) || !test::burstOk)return false;++test::bursts;return true;}
}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::key;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& key){return test::found && key==test::key?&test::actor:nullptr;}}
namespace coop::hand_item {
void* LocalHandActor(){return test::localHand;}void* MirrorActorForSlot(uint8_t){return test::mirror;}
uint32_t Generation(uint8_t){return test::generation;}uint32_t MirrorGeneration(uint8_t){return test::mirrorGeneration;}
bool IsMirrorActor(void* a){return a && a==test::mirror;}
bool IsHandAxisActor(void* a){return a && (a==test::localHand || a==test::mirror);}
}
namespace coop::roster_ledger {const Row& Get(int slot){static Row empty;return slot>=0 && slot<net::kMaxPeers?test::roster[slot]:empty;}}
namespace test {
namespace S=coop::sponge_effects_sync;using Packet=coop::net::SpongeEffectPayload;using Role=coop::net::Role;
void Start(coop::net::Session& session,Role role) {
    S::OnDisconnect();session.mode=role;session.linked=session.ready=session.sendOk=true;session.sent.clear();session.allowSend={};
    session.active=true;session.generations.fill(1);
    inactiveInvocations.clear();++sessionSerial;
    coop::players::Registry::Get().peer=role==Role::Host?0:1;coop::players::Registry::Get().local=&ui;
    for(int slot=0;slot<coop::net::kMaxPeers;++slot)roster[slot].playerNo=uint16_t(slot+10);
    localHand=mirror=nullptr;generation=mirrorGeneration=1;soap=100;sponge=found=readable=surfaceOk=burstOk=true;
    callback={};coop::element::reach=coop::element::segmentReach=true;coop::element::callback={};
    S::Install(&session);S::Reconcile();
}
Packet Request(uint32_t sequence,bool hand=false) {
    Packet p{};p.sequence=sequence;p.slot=1;p.playerNo=11;p.x=100;p.y=200;p.z=300;p.nz=1;
    if(hand)p.generation=2;else coop::net::WireKeyFromString(key,p.key);return p;
}
Packet Decode(const coop::net::Session::Sent& out){CHECK(out.kind==coop::net::ReliableKind::SpongeEffect);Packet p{};std::memcpy(&p,out.data.data(),sizeof(p));return p;}
void NativeUse(ue_wrap::script_gate::Call& call) {CHECK(S::Before(call)==ue_wrap::script_gate::Verdict::Run);soap-=5;S::AfterUse(call);}
}
int main() {
    using namespace test;using V=ue_wrap::script_gate::Verdict;coop::net::Session session;
    CHECK(coop::net::LaneForKind(coop::net::ReliableKind::SpongeEffect)==coop::net::Lane::Normal);
    CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::SpongeEffect));
    Start(session,Role::Client);ue_wrap::script_gate::Call call{};call.object=&actor;call.stack=&ui;call.depth=1;call.invocation=1;call.function=&ui;
    NativeUse(call);CHECK(S::g_events.size()==1 && soap==95);session.sendOk=false;S::Tick();CHECK(S::g_events.size()==1);
    session.sendOk=true;S::Tick();CHECK(S::g_events.empty() && session.sent.size()==1 && surfaces==0 && bursts==0);
    auto request=Decode(session.sent.back());CHECK(request.op==0 && request.slot==1 && request.playerNo==11);
    auto old=soap;CHECK(S::Before(call)==V::Run);S::AfterUse(call);CHECK(S::g_events.empty() && soap==old); // no native debit, no event
    soap=0;CHECK(S::Before(call)==V::Run);S::AfterUse(call);CHECK(S::g_frames.empty() && S::g_events.empty());soap=100;
    mirror=&actor;CHECK(S::Before(call)==V::Cancel);mirror=nullptr;
    localHand=&actor;generation=0;NativeUse(call);S::Tick();CHECK(S::g_events.empty() && session.sent.size()==1);
    generation=3;S::Tick();CHECK(S::g_events.empty() && session.sent.size()==1);
    NativeUse(call);localHand=nullptr;S::Tick();CHECK(S::g_events.empty()); // stow before send does not hit another hand
    NativeUse(call);now+=1600;S::Tick();CHECK(S::g_events.empty());
    Start(session,Role::Host);auto beforeSurface=surfaces,beforeBurst=bursts;
    S::Receive(request,1);S::Receive(request,1);CHECK(S::g_events.size()==1);session.sendOk=false;S::Tick();
    CHECK(surfaces==beforeSurface+1 && bursts==beforeBurst+1 && S::g_events.size()==1 && soap==100);
    session.sendOk=true;S::Tick();CHECK(S::g_events.empty() && surfaces==beforeSurface+1 && bursts==beforeBurst+1);
    for(const auto& out:session.sent)CHECK(out.slot!=1 && Decode(out).op==1);auto canonical=Decode(session.sent.back());
    request.sequence++;coop::element::segmentReach=false;S::Receive(request,1);S::Tick();CHECK(surfaces==beforeSurface+1);
    coop::element::segmentReach=true;request.sequence++;coop::element::reach=false;S::Receive(request,1);S::Tick();CHECK(surfaces==beforeSurface+1);coop::element::reach=true;
    auto bad=request;bad.sequence++;bad.playerNo++;S::Receive(bad,1);CHECK(S::g_events.empty());
    bad=request;bad.sequence++;bad.nx=NAN;S::Receive(bad,1);CHECK(S::g_events.empty());
    bad=request;bad.sequence++;bad.key.len=32;S::Receive(bad,1);CHECK(S::g_events.empty());
    bad=request;bad.sequence++;bad.generation=1;S::Receive(bad,1);CHECK(S::g_events.empty());
    // Hand requests wait for their matching mirror; both body and point reach
    // are checked before a host can play or relay a transient effect.
    request=Request(100,true);mirror=nullptr;generation=1;S::Receive(request,1);S::Tick();CHECK(S::g_events.size()==1);
    mirror=&actor;generation=mirrorGeneration=2;S::Tick();CHECK(S::g_events.empty() && surfaces==beforeSurface+2);
    request.sequence++;S::Receive(request,1);roster[1].playerNo=50;S::Tick();CHECK(S::g_events.empty() && surfaces==beforeSurface+2);
    Start(session,Role::Client);coop::players::Registry::Get().peer=2;
    S::Receive(canonical,2);CHECK(S::g_events.empty());S::Receive(canonical,0);S::Receive(canonical,0);CHECK(S::g_events.size()==1);
    burstOk=false;beforeSurface=surfaces;beforeBurst=bursts;S::Tick();CHECK(surfaces==beforeSurface+1 && bursts==beforeBurst && S::g_events.size()==1);
    S::Tick();CHECK(surfaces==beforeSurface+1);burstOk=true;S::Tick();CHECK(bursts==beforeBurst+1 && S::g_events.empty());
    canonical.sequence++;surfaceOk=false;S::Receive(canonical,0);S::Tick();beforeBurst=bursts;
    surfaceOk=true;S::Tick();CHECK(bursts==beforeBurst && S::g_events.empty()); // surface retry never duplicates the hand burst
    canonical.sequence++;S::Receive(canonical,0);callback=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session && S::g_events.empty());
    Start(session,Role::Host);request=Request(1);S::Receive(request,1);coop::element::callback=[]{S::OnDisconnect();};
    beforeSurface=surfaces;S::Tick();CHECK(!S::g_session && surfaces==beforeSurface);
    Start(session,Role::Host);NativeUse(call);CHECK(S::g_events.size()==1);S::Tick();CHECK(S::g_events.empty()); // host native effect is not played twice
    session.sent.clear();for(int i=0;i<4;++i)NativeUse(call);auto oldest=S::g_sequence+1;
    session.allowSend=[&](int slot,coop::net::ReliableKind,const void* data,int){Packet p{};std::memcpy(&p,data,sizeof(p));return !(slot==2 && p.sequence==oldest);};
    S::Tick();for(const auto& out:session.sent)CHECK(out.slot!=2);session.allowSend={};S::Tick();uint32_t last=0;int count=0;
    for(const auto& out:session.sent)if(out.slot==2){const auto p=Decode(out);CHECK(p.sequence>last);last=p.sequence;++count;}CHECK(count==4);
    // An already canonical retry stays before a newly authorized hand event.
    Start(session,Role::Host);request=Request(1,true);generation=1;mirror=nullptr;S::Receive(request,1);NativeUse(call);
    session.allowSend=[](int slot,coop::net::ReliableKind,const void*,int){return slot!=2;};S::Tick();
    mirror=&ui;generation=mirrorGeneration=2;session.allowSend={};S::Tick();last=0;count=0;
    for(const auto& out:session.sent)if(out.slot==2){auto p=Decode(out);CHECK(p.sequence>last);last=p.sequence;++count;}CHECK(count==2);
    // A native event enqueued reentrantly during presentation cannot overtake
    // the retained canonical event on a backpressured recipient.
    Start(session,Role::Host);request=Request(1);S::Receive(request,1);callback=[&]{NativeUse(call);};
    session.allowSend=[](int slot,coop::net::ReliableKind,const void*,int){return slot!=2;};S::Tick();
    session.allowSend={};S::Tick();last=0;count=0;
    for(const auto& out:session.sent)if(out.slot==2){auto p=Decode(out);CHECK(p.sequence>last);last=p.sequence;++count;}CHECK(count==2);
    // A dead actor's POST still removes its capture without reading the corpse.
    CHECK(S::Before(call)==V::Run);actor.live=false;S::AfterUse(call);CHECK(S::g_frames.empty());actor.live=true;
    // PRE/POST and wrapper callbacks cannot reassign a captured use to a new person/hand.
    Start(session,Role::Client);CHECK(S::Before(call)==V::Run);soap-=5;++roster[1].playerNo;S::AfterUse(call);CHECK(S::g_events.empty());
    Start(session,Role::Client);localHand=&actor;CHECK(S::Before(call)==V::Run);soap-=5;++generation;S::AfterUse(call);CHECK(S::g_events.empty());
    Start(session,Role::Client);callback=[]{++roster[1].playerNo;};CHECK(S::Before(call)==V::Run);CHECK(S::g_frames.empty());
    Start(session,Role::Client);CHECK(S::Before(call)==V::Run);soap-=5;callback=[]{++roster[1].playerNo;};S::AfterUse(call);CHECK(S::g_events.empty());
    // A blocked transient is discarded when its recipient changes; it is not join history.
    Start(session,Role::Host);NativeUse(call);session.allowSend=[](int slot,coop::net::ReliableKind,const void*,int){return slot!=2;};S::Tick();
    CHECK(S::g_events.size()==1);++roster[2].playerNo;session.allowSend={};session.sent.clear();S::Tick();CHECK(S::g_events.empty() && session.sent.empty());
    // Sending may synchronously clear the queue or replace the whole session.
    Start(session,Role::Host);NativeUse(call);NativeUse(call);
    session.allowSend=[](int,coop::net::ReliableKind,const void*,int){S::OnDisconnect();return false;};S::Tick();CHECK(!S::g_session && S::g_events.empty());
    Start(session,Role::Host);NativeUse(call);NativeUse(call);
    session.allowSend=[](int,coop::net::ReliableKind,const void*,int){S::OnPeerLeft(0);return true;};S::Tick();CHECK(S::g_events.empty());
    Start(session,Role::Host);NativeUse(call);
    session.allowSend=[](int slot,coop::net::ReliableKind,const void*,int){++roster[slot].playerNo;return false;};S::Tick();CHECK(S::g_events.empty());
    Start(session,Role::Host);request=Request(1);S::Receive(request,1);
    callback=[]{++roster[1].playerNo;};beforeBurst=bursts;S::Tick();CHECK(bursts==beforeBurst && S::g_events.empty());
    Start(session,Role::Host);request=Request(1);S::Receive(request,1);
    coop::element::callback=[]{++roster[1].playerNo;};beforeSurface=surfaces;S::Tick();CHECK(surfaces==beforeSurface && S::g_events.empty());
    Start(session,Role::Client);canonical.sequence++;S::Receive(canonical,0);
    ++world;S::Tick();CHECK(S::Current() && S::g_events.empty() && S::g_world==world);S::Install(&session);CHECK(S::g_events.empty());S::OnDisconnect();
    puts("PASS sponge effects: native-debit capture, hand/world identity, reach/point checks, mirror refusal, canonical transient output, partial/FIFO retry, no stock replay and teardown");
    return 0;
}
