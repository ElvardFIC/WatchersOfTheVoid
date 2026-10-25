#include "support_2135.h"
#ifndef WATOY_SYNC_SOURCE
#define WATOY_SYNC_SOURCE "../../src/votv-coop/src/coop/props/watoy_sync.cpp"
#endif
#include WATOY_SYNC_SOURCE
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <limits>
#include <set>
namespace test {
bool found=true,toy=true,writable=true,readable=true;
void* localHand=nullptr;
void* mirror=nullptr;
void* componentOwner=nullptr;
uint32_t handGeneration=1,mirrorGeneration=1;
int activations=0,applies=0,cues=0,claims=0,releases=0;
ue_wrap::watoy::State state{},heard{},handHeard{};
std::wstring key=L"watoy";
std::array<coop::roster_ledger::Row,coop::net::kMaxPeers> roster{};
uint32_t sessionSerial=1;
std::set<uint64_t> invocations;
std::function<void()> toyCallback;
}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace ue_wrap::script_gate {bool IsInvocationActive(uint64_t id){return test::invocations.contains(id);}}
namespace ue_wrap::watoy {
bool IsToy(void* a){const bool result=a && reflection::IsLive(a) && test::toy;auto callback=std::move(test::toyCallback);test::toyCallback={};if(callback)callback();return result;}
size_t Collect(CachedObjRef* out,size_t cap){if(!cap || !test::found)return 0;out[0].Set(&test::actor);return 1;}
bool Read(void*,State& s){s=test::state;test::Callback();return test::readable;}
bool Activate(void*,const std::function<bool()>& current){if(current && !current())return false;++test::activations;test::state.active=true;test::Callback();return !current || current();}
bool ApplyAudio(void* a,const State& s,const std::function<bool()>& current){if(current && !current())return false;++test::applies;test::heard=s;if(a==&test::ui)test::handHeard=s;test::Callback();return test::writable && (!current || current());}
bool Apply(View& view,void* actor,const State& state,const std::function<bool()>& current){view.actor.Set(actor);return ApplyAudio(actor,state) && current();}
void Clear(View& view){auto actor=view.actor.Get();view={};if(actor)ApplyAudio(actor,{});}
bool PlayStartCue(){++test::cues;test::Callback();return true;}
void* ZeroGravityOwner(void*){return test::componentOwner;}
}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::key;}void* GetStaticMesh(void*){return &test::ui;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& key){return test::found && key==test::key?&test::actor:nullptr;}}
namespace coop::prop_drive_host {void Claim(void*,const char*){++test::claims;}void Release(void*){++test::releases;}}
namespace coop::hand_item {
void* LocalHandActor(){return test::localHand;}
void* MirrorActorForSlot(uint8_t){return test::mirror;}
uint32_t Generation(uint8_t){return test::handGeneration;}
uint32_t MirrorGeneration(uint8_t){return test::mirrorGeneration;}
bool IsMirrorActor(void* a){return a && a==test::mirror;}
bool IsHandAxisActor(void* a){return a && (a==test::localHand || a==test::mirror);}
}
namespace coop::roster_ledger {const Row& Get(int slot){static Row empty;return slot>=0 && slot<net::kMaxPeers?test::roster[slot]:empty;}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return &test::ui;}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* id,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
    :ident_(id),feature_(feature),cls_(cls),fn_(fn),mode_(mode){}
Resolved ResolveOn(const Function&,void*){return {};}
}
namespace ue_wrap::ufunction_hook {bool InstallPreHook(const facts::Resolved&,PreNativeCallback,bool){return true;}}
namespace test {
namespace S=coop::watoy_sync;using Packet=coop::net::WatoyPayload;
void Start(coop::net::Session& session,coop::net::Role role) {
    S::OnDisconnect();session.mode=role;session.linked=session.ready=session.sendOk=true;session.sent.clear();session.allowSend={};
    coop::players::Registry::Get().peer=role==coop::net::Role::Host?0:1;
    found=toy=writable=readable=true;localHand=mirror=componentOwner=nullptr;handGeneration=mirrorGeneration=1;
    for(int slot=0;slot<coop::net::kMaxPeers;++slot)roster[slot].playerNo=uint16_t(slot+10);
    state={};heard={};S::Install(&session);S::ReconcileOccupants();
    coop::element::reach=true;callback=toyCallback={};coop::element::callback={};
}
Packet World(uint32_t sequence,uint8_t op=0) {
    Packet p{};coop::net::WireKeyFromString(key,p.key);p.sequence=sequence;p.op=op;
    if(op==0){p.active=1;p.volume=.6f;p.pitch=2.f;}return p;
}
Packet Hand(uint32_t sequence,uint32_t generation=1,uint8_t slot=2,uint8_t op=3) {
    Packet p{};p.sequence=sequence;p.op=op;p.slot=slot;p.generation=generation;p.playerNo=roster[slot].playerNo;
    if(op==3){p.active=1;p.volume=.7f;p.pitch=3.f;}return p;
}
Packet Decode(const coop::net::Session::Sent& sent){Packet p{};CHECK(sent.kind==coop::net::ReliableKind::Watoy);std::memcpy(&p,sent.data.data(),sizeof(p));return p;}
}
#ifndef WATOY_FIXTURE_ONLY
int main() {
    using namespace test;using coop::net::Role;using V=ue_wrap::script_gate::Verdict;coop::net::Session session;
    CHECK(coop::net::LaneForKind(coop::net::ReliableKind::Watoy)==coop::net::Lane::Normal);
    CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::Watoy));
    Start(session,Role::Host);S::Tick();CHECK(session.sent.size()==3 && !Decode(session.sent[0]).active);
    state={true,.2f,1.2f};now+=100;S::Tick();CHECK(claims==1 && session.sent.size()==6);
    auto active=Decode(session.sent.back());CHECK(active.active && active.volume==.2f && active.pitch==1.2f);
    state.active=false;now+=100;S::Tick();CHECK(releases==1);
    auto request=World(1,1);S::Receive(request,1);CHECK(activations==1 && state.active && S::g_out.size()==1);
    S::Receive(request,1);CHECK(activations==1);request.sequence++;S::Receive(request,1);CHECK(activations==1); // active is idempotent
    auto native=activations;request.sequence++;coop::element::reach=false;S::Receive(request,1);CHECK(activations==native);
    coop::element::reach=true;request.sequence++;session.ready=false;S::Receive(request,1);CHECK(activations==native);session.ready=true;
    localHand=&actor;request.sequence++;S::Receive(request,1);CHECK(activations==native);localHand=nullptr;
    session.sendOk=false;S::Tick();CHECK(!S::g_out.empty() && S::g_worldRows.at(key).pending);
    session.sendOk=true;S::Tick();CHECK(S::g_out.empty() && !S::g_worldRows.at(key).pending);
    auto sent=session.sent.size();S::QueueConnect(2);S::Tick();CHECK(session.sent.size()==sent+1 && Decode(session.sent.back()).op==0);
    // A peer cannot forge a state, another owner's hand, an inactive sample's
    // scalar bounds, or a canonical world cue.
    auto forged=active;forged.sequence=100;S::Receive(forged,1);CHECK(activations==native);
    auto h=Hand(100);S::Receive(h,1);CHECK(!S::g_hands[2].state.sequence);
    h=Hand(100,1,1);h.volume=NAN;S::Receive(h,1);CHECK(!S::g_hands[1].state.sequence);
    h=Hand(100,1,1);S::Receive(h,1);CHECK(S::g_hands[1].state.sequence && !S::g_hands[1].authorized);
    now+=100;S::Tick();CHECK(!S::g_hands[1].authorized);mirror=&ui;
    now+=100;S::Tick();CHECK(S::g_hands[1].authorized && heard.active && heard.pitch==3);
    S::QueueConnect(3);S::Tick();CHECK(session.sent.back().slot==3);
    auto previous=S::g_hands[1].state;roster[1].playerNo=100;S::Tick();CHECK(!S::g_hands[1].state.sequence);
    S::Receive(previous,1);CHECK(!S::g_hands[1].state.sequence); // no missed-leave inheritance
    // World and foreign-hand producer bodies stay native only on their author.
    ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&ui;call.invocation=1;invocations.insert(1);
    mirror=&actor;CHECK(S::BeforeActivate(call)==V::Cancel && S::BeforeTick(call)==V::Cancel);
    componentOwner=&actor;CHECK(S::BeforePhysics(&ui,&actor,nullptr)==V::Cancel);
    CHECK(S::BeforePhysics(&actor,&actor,nullptr)==V::Run);componentOwner=nullptr;CHECK(S::BeforePhysics(&ui,&actor,nullptr)==V::Run);
    mirror=nullptr;CHECK(S::BeforeTick(call)==V::Run);
    state.active=false;auto queued=S::g_out.size();CHECK(S::BeforeActivate(call)==V::Run);state.active=true;S::AfterActivate(call);
    CHECK(S::g_out.size()==queued+1);CHECK(S::BeforeActivate(call)==V::Run);S::AfterActivate(call);CHECK(S::g_out.size()==queued+1);
    Start(session,Role::Client);applies=0;S::Receive(active,2);CHECK(S::g_worldRows.empty());
    auto bad=active;bad.key.len=32;S::Receive(bad,0);CHECK(S::g_worldRows.empty());
    bad=active;bad.pitch=std::numeric_limits<float>::infinity();S::Receive(bad,0);CHECK(S::g_worldRows.empty());
    bad=active;bad.active=2;S::Receive(bad,0);CHECK(S::g_worldRows.empty());
    S::Receive(active,0);found=false;S::Tick();CHECK(applies==0);found=true;writable=false;now+=100;S::Tick();CHECK(applies==1);
    writable=true;now+=100;S::Tick();CHECK(applies==2 && heard.active && heard.volume==.2f);
    auto newer=active;newer.sequence++;newer.active=0;S::Receive(newer,0);S::Receive(active,0);S::Tick();CHECK(!heard.active);
    CHECK(S::BeforeActivate(call)==V::Cancel && S::g_out.size()==1);CHECK(S::BeforeTick(call)==V::Cancel);
    session.sendOk=false;S::Tick();CHECK(S::g_out.size()==1);session.sendOk=true;S::Tick();CHECK(S::g_out.empty());
    localHand=&actor;CHECK(S::BeforeTick(call)==V::Run);CHECK(S::BeforeActivate(call)==V::Run);S::AfterActivate(call);localHand=nullptr;
    auto pulse=World(100,2);const auto beforeCues=cues;S::Receive(pulse,0);S::Receive(pulse,0);CHECK(S::g_out.size()==1);
    found=false;S::Tick();CHECK(cues==beforeCues);found=true;S::Tick();CHECK(cues==beforeCues+1 && S::g_out.empty());
    pulse.sequence++;found=false;S::Receive(pulse,0);now+=1600;S::Tick();found=true;S::Tick();CHECK(cues==beforeCues+1);
    h=Hand(200,2);handGeneration=1;mirrorGeneration=1;mirror=nullptr;S::Receive(h,0);S::Tick();auto beforeApplies=applies;
    mirror=&ui;mirrorGeneration=1;now+=100;S::Tick();CHECK(applies==beforeApplies+1); // world row only
    mirrorGeneration=handGeneration=2;now+=100;S::Tick();CHECK(handHeard.pitch==3 && S::g_hands[2].actor.Is(&ui));
    mirror=nullptr;now+=100;S::Tick();CHECK(!S::g_handViews[2].actor.Get() && !handHeard.active);
    // State may arrive before the replacement mirror. Retire the old view while
    // retaining the newer sample until that exact hand generation exists.
    mirror=&ui;now+=100;S::Tick();CHECK(S::g_handViews[2].actor.Is(&ui) && handHeard.active);
    auto replacement=h;replacement.sequence++;replacement.generation=3;S::Receive(replacement,0);
    now+=100;S::Tick();CHECK(!S::g_handViews[2].actor.Get() && !handHeard.active);
    mirrorGeneration=handGeneration=3;now+=100;S::Tick();CHECK(S::g_handViews[2].actor.Is(&ui) && handHeard.active);
    h=replacement;
    auto old=h;old.sequence++;old.generation=1;S::Receive(old,0);CHECK(S::g_hands[2].state.generation==3);
    old=h;old.sequence++;old.playerNo++;S::Receive(old,0);CHECK(S::g_hands[2].state.sequence==h.sequence);
    roster[2].playerNo++;S::Tick();CHECK(!S::g_hands[2].state.sequence);
    Start(session,Role::Host);mirror=nullptr;h=Hand(1,2,1,4);handGeneration=1;
    auto cueCount=cues;S::Receive(h,1);S::Tick();CHECK(cues==cueCount && S::g_out.size()==1);
    mirror=&ui;mirrorGeneration=handGeneration=2;S::Tick();CHECK(cues==cueCount+1 && S::g_out.empty());
    for(const auto& out:session.sent)if(Decode(out).op==4)CHECK(out.slot!=1);
    // A level sample accepted while a cue is transport-blocked cannot make that
    // older cue look like a replay: the two owner streams have separate cursors.
    h=Hand(10,2,1);S::Receive(h,1);h=Hand(9,2,1,4);S::Receive(h,1);CHECK(S::g_out.size()==1);S::Tick();CHECK(S::g_out.empty());
    Start(session,Role::Client);localHand=&actor;handGeneration=0;state.active=false;
    CHECK(S::BeforeActivate(call)==V::Run);state.active=true;S::AfterActivate(call);CHECK(S::g_out.size()==1);
    S::Tick();CHECK(S::g_out.size()==1);handGeneration=4;S::Tick();CHECK(S::g_out.empty());
    bool announcedCue=false;for(const auto& out:session.sent){auto p=Decode(out);if(p.op==4){CHECK(p.generation==4);announcedCue=true;}}CHECK(announcedCue);
    Start(session,Role::Client);S::Receive(active,0);S::Tick();CHECK(S::g_worldViews.size()==1);
    found=false;now+=10001;S::Tick();CHECK(S::g_worldRows.empty() && S::g_worldViews.empty());found=true;
    S::Receive(active,0);S::Tick();
    // Reentrant calls invalidate both captured identities and pending work.
    callback=[&]{S::OnDisconnect();};now+=100;S::Tick();CHECK(!S::g_session && S::g_worldRows.empty());
    Start(session,Role::Host);state.active=false;request=World(1,1);coop::element::callback=[&]{S::OnDisconnect();};native=activations;
    S::Receive(request,1);CHECK(!S::g_session && activations==native);
    Start(session,Role::Client);S::Receive(active,0);++world;S::Tick();CHECK(S::Current() && S::g_worldRows.empty());S::Install(&session);CHECK(S::g_worldRows.empty());
    // Reliable backpressure cannot let a newer live cue overtake an older one.
    Start(session,Role::Host);state.active=false;
    for(int i=0;i<5;++i)S::QueueCue(&actor);auto oldest=S::g_sequence+1;
    session.allowSend=[&](int slot,coop::net::ReliableKind,const void* data,int){Packet p{};std::memcpy(&p,data,sizeof(p));return !(slot==2 && p.op==2 && p.sequence==oldest);};
    S::Tick();for(const auto& out:session.sent)if(out.slot==2)CHECK(Decode(out).op!=2);
    session.allowSend={};S::Tick();int heardCount=0;uint32_t last=0;
    for(const auto& out:session.sent)if(out.slot==2){auto p=Decode(out);if(p.op==2){CHECK(p.sequence>last);last=p.sequence;++heardCount;}}
    CHECK(heardCount==5);S::OnDisconnect();
    Start(session,Role::Host);mirror=nullptr;handGeneration=1;h=Hand(1,2,1,4);S::Receive(h,1);S::QueueCue(&actor);
    session.allowSend=[](int slot,coop::net::ReliableKind,const void*,int){return slot!=2;};S::Tick();
    mirror=&ui;handGeneration=mirrorGeneration=2;session.allowSend={};S::Tick();last=0;heardCount=0;
    for(const auto& out:session.sent)if(out.slot==2){auto p=Decode(out);if(p.op==2 || p.op==4){CHECK(p.sequence>last);last=p.sequence;++heardCount;}}
    CHECK(heardCount==2);S::OnDisconnect();
    Start(session,Role::Host);found=false;request=World(1,1);native=activations;S::Receive(request,1);
    CHECK(S::g_out.size()==1);S::Tick();CHECK(activations==native && S::g_out.size()==1);
    found=true;readable=false;now+=100;S::Tick();CHECK(activations==native && S::g_out.size()==1);
    readable=true;now+=100;S::Tick();CHECK(activations==native+1);S::Tick();CHECK(S::g_out.empty());
    Start(session,Role::Host);found=false;request=World(1,1);S::Receive(request,1);S::OnPeerLeft(1);
    found=true;native=activations;now+=100;S::Tick();CHECK(activations==native && S::g_out.empty());
    Start(session,Role::Host);found=false;request=World(1,1);S::Receive(request,1);
    now+=3001;found=true;native=activations;S::Tick();CHECK(activations==native && S::g_out.empty());
    Start(session,Role::Host);found=false;request=World(1,1);S::Receive(request,1);
    roster[1].playerNo++;found=true;native=activations;now+=100;S::Tick();CHECK(activations==native && S::g_out.empty());
    Start(session,Role::Host);request=World(1,1);native=activations;
    coop::element::callback=[]{roster[1].playerNo++;};S::Receive(request,1);CHECK(activations==native);
    puts("PASS watoy: canonical world activation/motion/audio, late actor/field retry, join, native producer guards, owner hand readiness/occupancy, retry FIFO and teardown");
}
#endif
