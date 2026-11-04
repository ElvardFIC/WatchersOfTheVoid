#ifndef EMOTE_SYNC_SOURCE
#define EMOTE_SYNC_SOURCE "../../src/votv-coop/src/coop/player/emote_sync.cpp"
#endif
#include EMOTE_SYNC_SOURCE
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include "../../src/votv-coop/src/coop/net/client_authorship.h"
#include <cassert>
#include <cstdio>
#include <limits>
namespace test {
uint64_t now=1000;uint32_t world=1,serial=1;bool assets=true;
std::array<coop::roster_ledger::Row,32> people{};
ue_wrap::emote_wheel::State source{},painted{};int paints=0,stops=0;
std::function<void()> callback;
int body=1,other=2;
int item=3,handPlacements=0;uint32_t handGeneration=99;
void Callback(){auto once=std::move(callback);callback={};if(once)once();}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void*){return 0;}int32_t AllocateSlotSerial(int32_t){return 1;}int32_t SlotSerial(int32_t){return 1;}
bool IsLiveByIndex(void* p,int32_t){return p!=nullptr;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* WorldOf(void*){return nullptr;}void* CurrentWorld(){return nullptr;}}
namespace coop::active_drive {uint64_t NowMs(){return test::now;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace coop::roster_ledger {const Row& Get(int i){static Row none;return i>=0&&i<32?test::people[i]:none;}}
namespace coop::hand_item {
void* LocalHandActor(){return nullptr;}uint32_t Generation(uint8_t){return 0;}
void* MirrorActorForSlot(uint8_t){return &test::item;}uint32_t MirrorGeneration(uint8_t){return test::handGeneration;}
}
namespace ue_wrap::emote_wheel {
void Observe(void*){}State Sample(){return test::source;}void ResetObservation(){}
void Stop(Playback& p){if(p.actor.Get())++test::stops;p={};test::Callback();}
bool Apply(void* actor,const State& state,Playback& p,const std::function<bool()>& current){
    ++test::paints;test::Callback();if(!current() || !test::assets)return false;
    p.actor.Set(actor);p.state=state;test::painted=state;return true;
}
Hand CaptureHand(void*,void*,uint32_t){return {};}
bool PlaceHand(void*,void*,const Hand&,const std::function<bool()>& current){++test::handPlacements;test::Callback();return current();}
}
int main(){
    namespace S=coop::emote_sync;namespace N=coop::net;namespace E=ue_wrap::emote_wheel;
    using namespace test;auto& registry=coop::players::Registry::Get();
    auto reset=[&](N::Session& session,bool client=false){
        S::OnDisconnect();people={};people[0].playerNo=1;people[31].playerNo=32;people[1].playerNo=2;
        registry.peer=client?1:0;registry.local=&body;registry.puppets={};registry.puppets[31].body=&other;registry.puppets[0].body=&other;
        source={};session.mode=client?N::Role::Client:N::Role::Host;session.ready[1]=session.ready[31]=true;
        now+=5000;S::Install(&session);
    };
    assert(N::LaneForKind(N::ReliableKind::EmoteState)==N::Lane::Default);
    assert(!N::IsClientRelayableReliableKind(N::ReliableKind::EmoteState));
    assert(!N::IsPreWorldSendableKind(N::ReliableKind::EmoteState));
    assert(N::AuthorshipOf(N::ReliableKind::EmoteState)==N::ClientAuthorship::Client);
    uint16_t loop=0,variant=0;
    for(size_t i=0;i<E::kPartCount;++i){if(E::kParts[i].loop&&E::kParts[i].sound&&!loop)loop=uint16_t(i+1);
        if(std::wstring(E::kParts[i].animation).find(L"_v2")!=std::wstring::npos&&!variant)variant=uint16_t(i+1);}
    assert(loop&&variant);
    N::Session host;reset(host);
    N::EmoteStatePayload p{};p.slot=31;p.playerNo=32;p.sequence=10;p.part=loop;p.episode=8;p.music=loop;p.musicEpisode=9;p.position=1;p.musicPosition=1;
    S::Receive(p,31);S::Tick();assert(painted.part==loop && painted.episode==8 && painted.musicEpisode==9);
    assert(painted.position>=1.05f);assert(!host.sent.empty());
    auto row=S::g_rows[31];
    for(auto bad:{0,1,30,32,255}){auto packet=p;packet.sequence++;S::Receive(packet,uint8_t(bad));assert(S::g_rows[31].revision==row.revision);}
    for(int bad=0;bad<8;++bad){auto packet=p;packet.sequence++;
        if(bad==0)packet.playerNo=2;if(bad==1)packet.part=65535;if(bad==2)packet.position=std::numeric_limits<float>::quiet_NaN();
        if(bad==3)packet.position=-1;if(bad==4)packet.reserved=1;if(bad==5)packet.episode=0;
        if(bad==6)packet.musicPosition=std::numeric_limits<float>::infinity();if(bad==7)packet.music=1;
        S::Receive(packet,31);assert(S::g_rows[31].revision==row.revision);
    }
    auto changed=p;changed.sequence++;changed.part=variant;changed.position=.1f;changed.episode=10;
    S::Receive(changed,31);now+=50;S::Tick();assert(painted.part==variant && painted.episode==10 && painted.music==loop);
    const auto latest=S::g_rows[31].revision;S::Receive(p,31);assert(S::g_rows[31].revision==latest);
    host.sent.clear();host.sendOK=false;changed.sequence++;S::Receive(changed,31);now+=50;S::Tick();assert(host.sent.empty());
    host.sendOK=true;now+=50;S::Tick();assert(!host.sent.empty());
    host.ready[1]=false;host.sent.clear();now+=50;S::Tick();assert(host.sent.empty());
    host.ready[1]=true;now+=50;S::Tick();assert(!host.sent.empty() && host.sent.back().packet.part==variant);
    // A host without the optional assets still retains and forwards the exact state.
    assets=false;host.sent.clear();changed.sequence++;S::Receive(changed,31);now+=50;S::Tick();assert(!host.sent.empty());assets=true;
    const auto before=stops;registry.puppets[31].eligible=false;now+=50;S::Tick();assert(stops>before);
    registry.puppets[31].eligible=true;now+=50;S::Tick();assert(painted.part==variant);
    auto stop=changed;stop.sequence++;S::PutState(stop,{});S::Receive(stop,31);now+=50;S::Tick();assert(!S::g_rows[31].playback.actor.Get());
    changed.sequence+=2;S::Receive(changed,31);now+=50;S::Tick();S::OnPeerLeft(31);assert(!S::g_rows[31].packet.sequence && !S::g_rows[31].playback.actor.Get());
    S::Receive(changed,31);++host.generation[31];now+=50;S::Tick();assert(!S::g_rows[31].packet.sequence);
    changed.sequence++;S::Receive(changed,31);now+=4000;S::Tick();assert(!S::Active(S::g_rows[31].packet));
    // World, stop count, session serial and role replacement retire all retained work.
    for(int boundary=0;boundary<4;++boundary){changed.sequence++;S::Receive(changed,31);
        if(boundary==0)++world;if(boundary==1)++host.stops;if(boundary==2)++serial;if(boundary==3)host.mode=N::Role::Client;
        now+=50;S::Tick();assert(!S::g_rows[31].packet.sequence);
    }
    N::Session client;reset(client,true);source={7,8,.25f,.25f,loop,loop};client.sendOK=false;S::Tick();assert(S::g_localPending);
    client.sendOK=true;now+=50;S::Tick();assert(!S::g_localPending && !client.sent.empty() && client.sent.back().packet.part==loop);
    source={};now+=50;S::Tick();assert(!client.sent.back().packet.part && !client.sent.back().packet.music);
    auto canonical=p;canonical.slot=0;canonical.playerNo=1;canonical.sequence=100;
    S::Receive(canonical,31);assert(!S::g_rows[0].packet.sequence);
    S::Receive(canonical,0);now+=50;S::Tick();assert(S::g_rows[0].playback.actor.Get());
    source={7,8,.5f,.5f,loop,loop};client.callback=[]{S::OnDisconnect();};now+=50;S::Tick();assert(!S::g_session && !S::g_rows[0].packet.sequence);
    reset(host);changed.sequence=50;S::Receive(changed,31);callback=[]{S::OnDisconnect();};S::Tick();assert(!S::g_session && !S::g_rows[31].packet.sequence);
    // Bone placement is display-only and tied to the exact current hand generation.
    reset(host);registry.puppets[0].body=nullptr;
    auto held=p;held.sequence=50;held.handGeneration=99;held.handBone=2;held.handPose[0]=4;held.handPose[6]=1;
    S::Receive(held,31);S::Tick();assert(S::DriveHand(&other,&item)==S::HandDrive::Applied && handPlacements==1);
    ++handGeneration;assert(S::DriveHand(&other,&item)==S::HandDrive::Fallback && handPlacements==1);--handGeneration;
    auto malformed=held;malformed.sequence++;malformed.handPose[6]=0;const auto revision=S::g_rows[31].revision;
    S::Receive(malformed,31);assert(S::g_rows[31].revision==revision);
    malformed=held;malformed.sequence++;malformed.handPose[0]=301;S::Receive(malformed,31);assert(S::g_rows[31].revision==revision);
    callback=[]{S::OnDisconnect();};assert(S::DriveHand(&other,&item)==S::HandDrive::Retired);
    // A listening solo host captures its current emote before the first join.
    host.linked=false;reset(host);source={90,91,.2f,.2f,loop,loop};S::Tick();
    assert(S::g_rows[0].packet.part==loop);const auto soloEpoch=S::g_epoch;
    host.linked=true;now+=50;S::Tick();assert(S::g_epoch==soloEpoch && S::g_rows[0].packet.episode==90);
    // All 31 recipients make progress even with 32 active sources.
    reset(host);for(int i=0;i<32;++i){people[i].playerNo=uint16_t(i+1);host.ready[i]=true;}
    S::Reconcile();for(int i=0;i<32;++i){auto packet=p;packet.slot=uint8_t(i);packet.playerNo=uint16_t(i+1);S::Canonical(packet,now);}
    for(int i=0;i<20;++i){now+=50;S::Tick();}
    std::array<bool,32> received{};for(const auto& sent:host.sent)received[sent.slot]=true;
    for(int i=1;i<32;++i)assert(received[i]);
    std::puts("PASS EmoteWheel ownership, variants, independent music, malformed state, replay, retry, mixed assets, joins, interruption, expiry, teardown and 32-peer delivery");
}
