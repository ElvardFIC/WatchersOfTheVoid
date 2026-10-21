#include "support_2135.h"
#include "../../src/votv-coop/src/coop/props/water_pour_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
namespace test {
namespace S=coop::water_pour_sync;namespace FX=ue_wrap::water_pour;
using Request=coop::net::WaterPourPayload;using Reply=coop::net::WaterPourReplyPayload;using Splash=coop::net::WaterSplashPayload;
using Role=coop::net::Role;using V=ue_wrap::script_gate::Verdict;
std::array<coop::roster_ledger::Row,4> roster{};
void* hand=&ui;void* mirror=&ui;void* driven=&ui;void* held=&ui;
uint32_t generation=7,mirrorGeneration=7;uint16_t holdGeneration=19,remoteHoldGeneration=19;
bool& owned=coop::local_prop_hold::held;
bool coopDispatch=false,bucket=true,pour=true,readable=true,impactReady=true;
bool shot=false,washer=false,sourceMirror=false,hookReady=true;
void* nativeCaller=nullptr;
int retiredEffects=0;
unsigned blockedMirrors=0;int positionReads=0;
std::vector<size_t> positionTurns;
bool attempted=true,authored=true,surfaceReady=true,soundReady=true;
int reads=0,authors=0,retires=0,surfaces=0,sounds=0;
FX::Birth native{{100,200,300},{5,6,7},12};FX::Birth created;
FX::Impact impact{{100,200,300},{0,0,1},{110,210,310}};
std::wstring key=L"bucket";
ue_wrap::FVector sourcePosition{100,200,300};bool positionReady=true;
std::vector<std::pair<void*,void*>> active;
std::function<void()> onRead,onAuthor,onSurface,onSound,onImpact;
std::function<void()> onBucketRead,onBucketSound;
FX::BucketCue nativeCue{0,{100,200,300}};
bool cueReady=true;
int bucketSounds=0;
std::vector<uint8_t> bucketKinds;
void Run(std::function<void()>& f){auto callback=std::move(f);f={};if(callback)callback();}
}
namespace coop::hand_item {
void* LocalHandActor(){return test::hand;}void* MirrorActorForSlot(uint8_t slot){return (test::blockedMirrors&(1u<<slot))?nullptr:test::mirror;}
uint32_t Generation(uint8_t){return test::generation;}uint32_t MirrorGeneration(uint8_t){return test::mirrorGeneration;}
bool IsHandAxisActor(void* a){return a && (a==test::hand || a==test::mirror);}
bool IsMirrorActor(void* a){return a==&test::ui && test::sourceMirror;}
}
namespace coop::local_streams {void* LastHeldActor(){return test::held;}uint16_t CurrentHoldGen(){return test::holdGeneration;}}
namespace coop::remote_prop {void* DrivenActor(int){return test::driven;}uint16_t DrivenHoldGeneration(int){return test::remoteHoldGeneration;}}
namespace coop::prop_lifecycle {void DestroyLocalProp(void* actor,bool deferred){CHECK(actor==&test::actor && deferred);++test::retires;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& key){return key==test::key?&test::ui:nullptr;}}
namespace coop::roster_ledger {const Row& Get(int slot){return test::roster.at(slot);}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::key;}}
namespace ue_wrap::engine {bool TryGetActorLocation(void* actor,FVector& out){CHECK(actor==&test::ui);++test::positionReads;test::positionTurns.push_back(test::S::g_hostCursor);out=test::sourcePosition;test::Callback();return test::positionReady;}
bool DestroyComponent(void* component,void* source){CHECK(component==&test::ui && source==&test::actor);++test::retiredEffects;return true;}}
namespace ue_wrap::building_material {bool IsBucket(void* actor){return actor==&test::ui && test::bucket && reflection::IsLive(actor);}}
namespace ue_wrap::pressure_washer {bool IsWasher(void* actor){return actor==&test::ui && test::washer && reflection::IsLive(actor);}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* name){CHECK(!wcscmp(name,L"GameplayStatics"));return &test::ui;}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode):ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode){}
Resolved ResolveOn(const Function& row,void* cls){return Resolved(cls,&row);}
}
namespace ue_wrap::ufunction_hook {
bool InstallPostHook(const facts::Resolved&,PostNativeCallback,bool){return test::hookReady;}
bool InstallPreHook(const facts::Resolved&,PreNativeCallback,bool){return test::hookReady;}
CallerFrame CurrentCallerFrame(){return {test::nativeCaller,nullptr,-1};}
}
namespace ue_wrap::reflection {
bool InCoopDispatch(){return test::coopDispatch;}
int32_t FindParamOffset(void*,const wchar_t* name){if(!wcscmp(name,L"InputPin5") || !wcscmp(name,L"Hit"))return 8;
    CHECK(!wcscmp(name,L"EntryPoint") || !wcscmp(name,L"otherac") || !wcscmp(name,L"OtherActor"));return 0;}
int32_t FunctionFrameSize(void*){return 32;}
void* PropertyInnerStruct(void*,const wchar_t* name){CHECK(!wcscmp(name,L"InputPin5") || !wcscmp(name,L"Hit"));return &test::ui;}
int32_t StructSize(void*){return 24;}
}
namespace ue_wrap::script_gate {
bool IsObjectBodyActive(void* fn,void* actor){for(auto& pair:test::active)if(pair==std::make_pair(fn,actor))return fn!=nullptr;return false;}
uint8_t* OutParamPtr(const Call&,int32_t){return nullptr;}
}
namespace ue_wrap::water_pour {
bool IsPour(void* actor){return actor==&test::actor && test::pour && reflection::IsLive(actor);}
bool IsShot(void* actor){return actor==&test::actor && test::shot && reflection::IsLive(actor);}
bool IsProjectile(void* actor){return IsPour(actor) || IsShot(actor);}
bool Valid(const Birth& value){return value.power==1 && uint8_t(value.kind)<=1 && (value.kind!=Kind::Washer || value.soap==0) && std::isfinite(value.location.X) && std::isfinite(value.velocity.X) && value.soap>=0 && value.soap<=1.e6f;}
bool Valid(const Impact& value){return std::isfinite(value.location.X) && value.normal.Z==1;}
bool ReadBirth(void*,Birth& out,const std::function<bool()>& current){++test::reads;out=test::native;test::Run(test::onRead);return test::readable && Valid(out) && current();}
bool ReadImpact(void*,void*,void*,const void*,Impact& out){out=test::impact;test::Run(test::onImpact);return test::impactReady;}
Spawn Author(const Birth& value,const std::function<bool()>& current){CHECK(current());++test::authors;test::created=value;
    test::Run(test::onAuthor);return {test::authored && current()?&test::actor:nullptr,test::attempted};}
bool Surface(const Impact& value,const std::function<bool()>& current){CHECK(value.location.X==100);test::Run(test::onSurface);
    if(!test::surfaceReady || !current())return false;++test::surfaces;return true;}
bool Sound(const Impact& value,const std::function<bool()>& current){CHECK(value.audioLocation.X==(value.launch?100:110));test::Run(test::onSound);
    if(!test::soundReady || !current())return false;++test::sounds;return true;}
bool ReadBucketCue(void*,void*,const uint8_t*,bool twoD,BucketCue& out,const std::function<bool()>& current){
    out=test::nativeCue;test::Run(test::onBucketRead);return test::cueReady && current() && (twoD?out.kind>0:out.kind==0);}
bool BucketSound(const BucketCue& cue,const std::function<bool()>& current){test::Run(test::onBucketSound);
    if(!test::soundReady || !current())return false;++test::bucketSounds;test::bucketKinds.push_back(cue.kind);return true;}
}
namespace test {
void Start(coop::net::Session& session,Role role){
    S::OnDisconnect();session={};session.mode=role;coop::players::Registry::Get().peer=role==Role::Host?0:1;
    actor.live=ui.live=true;actor.serial=ui.serial=1;
    for(int slot=0;slot<4;++slot)roster[slot].playerNo=uint16_t(slot+10);
    hand=mirror=driven=held=&ui;generation=mirrorGeneration=7;holdGeneration=remoteHoldGeneration=19;
    owned=bucket=pour=readable=impactReady=attempted=authored=surfaceReady=soundReady=true;coopDispatch=false;
    shot=washer=sourceMirror=false;hookReady=true;retiredEffects=0;nativeCaller=nullptr;native={{100,200,300},{5,6,7},12};
    blockedMirrors=0;positionReads=0;positionTurns.clear();ue_wrap::script_gate::watchLive=true;announced=true;
    onBucketRead=onBucketSound={};nativeCue={0,{100,200,300}};cueReady=true;bucketSounds=0;bucketKinds.clear();
    callback={};onRead=onAuthor=onImpact=onSurface=onSound={};active.clear();key=L"bucket";
    coop::local_prop_hold::onRead={};
    sourcePosition={100,200,300};positionReady=true;
    coop::element::reach=coop::element::segmentReach=true;coop::element::callback={};
    reads=authors=retires=surfaces=sounds=0;S::Install(&session);CHECK(S::Reconcile());
    S::g_tick.Set(&ui);S::g_throw.Set(&actor);active={{&ui,&ui}};
}
Request Packet(uint32_t sequence=1,bool world=false){
    Request p{};p.sequence=sequence;p.playerNo=11;p.slot=1;p.generation=world?19:7;
    p.x=100;p.y=200;p.z=300;p.vx=5;p.vy=6;p.vz=7;p.soap=12;p.power=1;
    if(world)coop::net::WireKeyFromString(key,p.source);return p;
}
template<class T>T Decode(const coop::net::Session::Sent& out){CHECK(out.data.size()==sizeof(T));T p{};std::memcpy(&p,out.data.data(),sizeof(p));return p;}
ue_wrap::script_gate::Call HitCall(){static uint8_t frame[32]{};ue_wrap::script_gate::Call call{};
    call.object=&actor;call.function=&ui;call.locals=frame;call.stack=&ui;call.depth=1;return call;}
void Pump(){active.clear();S::Tick();}
void CaptureFinished(){active.clear();S::AfterSource({});}
void ImpactEvent(){auto call=HitCall();CHECK(S::BeforeCollision(call)==V::Run);S::AfterCollision(call);}
}
int main(){
    using namespace test;coop::net::Session session;
    CHECK(coop::net::LaneForKind(coop::net::ReliableKind::WaterPour)==coop::net::Lane::Normal);
    CHECK(coop::net::LaneForKind(coop::net::ReliableKind::WaterSplash)==coop::net::Lane::Normal);
    CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::WaterPour));
    Start(session,Role::Client);
    CHECK(!S::ObserveBirth(&actor,&actor));active.clear();CHECK(!S::ObserveBirth(&ui,&actor));active={{&ui,&ui}};
    coopDispatch=true;CHECK(!S::ObserveBirth(&ui,&actor));coopDispatch=false;
    pour=false;CHECK(!S::ObserveBirth(&ui,&actor));pour=true;
    hand=nullptr;owned=false;CHECK(!S::ObserveBirth(&ui,&actor));owned=true;hand=&ui;
    CHECK(S::ObserveBirth(&ui,&actor));S::AfterSource({});S::Tick();
    CHECK(reads==0 && retires==0 && session.sent.empty()); // source still assigns velocity
    CaptureFinished();CHECK(reads==1 && retires==1);
    Start(session,Role::Client);active={{&actor,&ui}}; // throwing, not the ReceiveTick source
    CHECK(S::ObserveBirth(&ui,&actor));S::AfterSource({});CHECK(reads==0);
    CaptureFinished();CHECK(reads==1);
    Start(session,Role::Client);
    generation=0;native.velocity={};CHECK(S::ObserveBirth(&ui,&actor) && S::g_local.size()==1 && reads==0);
    CHECK(S::ObserveBirth(&ui,&actor) && S::g_local.size()==1);native.velocity={5,6,7};CaptureFinished();
    CHECK(S::g_local.front().captured && S::g_local.front().request.vx==5 && retires==1);
    actor.live=false;Pump();CHECK(session.sent.empty());generation=7;Pump();CHECK(session.sent.size()==1);
    auto request=Decode<Request>(session.sent.front());CHECK(request.generation==7 && request.soap==12 && !request.source.len);
    S::ReceiveReply({request.sequence,11,1,1},2);CHECK(S::g_local.size()==1);
    S::ReceiveReply({request.sequence,12,1,1},0);CHECK(S::g_local.size()==1);
    S::ReceiveReply({request.sequence,11,1,1},0);CHECK(S::g_local.empty());
    Start(session,Role::Client);CHECK(S::ObserveBirth(&ui,&actor));onRead=[]{++generation;};CaptureFinished();CHECK(!S::g_local.front().captured);
    Pump();CHECK(S::g_local.empty() && session.sent.empty());
    Start(session,Role::Client);CHECK(S::ObserveBirth(&ui,&actor));onRead=[]{++roster[1].playerNo;};Pump();CHECK(session.sent.empty());
    Start(session,Role::Client);hand=nullptr;held=nullptr;CHECK(S::ObserveBirth(&ui,&actor));Pump();CHECK(session.sent.empty());
    held=&ui;session.sendOk=false;Pump();CHECK(session.sent.empty());session.sendOk=true;now+=150;Pump();
    CHECK(session.sent.size()==1 && Decode<Request>(session.sent.back()).source.len && Decode<Request>(session.sent.back()).generation==19);
    Start(session,Role::Client);CHECK(S::ObserveBirth(&ui,&actor));onRead=[]{S::OnDisconnect();};Pump();CHECK(!S::g_session && session.sent.empty());
    Start(session,Role::Client);CHECK(S::ObserveBirth(&ui,&actor));CaptureFinished();now+=1501;Pump();CHECK(S::g_local.empty() && session.sent.empty());
    Start(session,Role::Client);CHECK(S::ObserveBirth(&ui,&actor));const auto beforeRetire=retires;++world;S::Install(&session);CHECK(retires==beforeRetire);
    Start(session,Role::Client);hand=nullptr;coop::local_prop_hold::onRead=[]{S::OnDisconnect();};
    CHECK(S::ObserveBirth(&ui,&actor) && !S::g_session && S::g_local.empty());
    Start(session,Role::Client);hand=nullptr;coop::local_prop_hold::onRead=[]{++holdGeneration;};
    CHECK(S::ObserveBirth(&ui,&actor));Pump();CHECK(S::g_local.empty() && session.sent.empty());
    Start(session,Role::Host);auto p=Packet();mirrorGeneration=0;S::Receive(p,1);Pump();CHECK(!authors && session.sent.empty());
    mirrorGeneration=7;attempted=false;authored=false;Pump();CHECK(authors==1 && !S::g_host.front().code);
    attempted=authored=true;Pump();CHECK(authors==2 && S::g_host.front().code==1 && Decode<Reply>(session.sent.back()).code==1);
    const auto canonical=p;session.sent.clear();S::Receive(p,1);Pump();CHECK(authors==2 && session.sent.size()==1);
    actor.live=false;S::Receive(p,1);Pump();CHECK(authors==2);actor.live=true;
    auto changed=p;changed.soap=13;session.sent.clear();S::Receive(changed,1);Pump();CHECK(session.sent.empty());
    Start(session,Role::Host);S::Receive(Packet(2),1);S::Receive(Packet(1),1);Pump();CHECK(authors==2);
    S::Receive(Packet(1),1);Pump();CHECK(authors==2); // delayed earlier birth is new; its replay is not
    S::g_host.clear();S::Receive(Packet(1),1);Pump();CHECK(authors==2); // retained replay history outlives reply-row eviction
    Start(session,Role::Host);S::Receive(Packet(0xfffffffeu),1);S::Receive(Packet(2),1);S::Receive(Packet(0xffffffffu),1);
    Pump();CHECK(authors==3);S::Receive(Packet(0xffffffffu),1);Pump();CHECK(authors==3);
    Start(session,Role::Host);S::Receive(Packet(300),1);S::Receive(Packet(1),1);S::Receive(Packet(45),1);
    Pump();CHECK(authors==2); // bounded receive history, including exactly the oldest admitted entry
    Start(session,Role::Host);p=Packet();coop::element::reach=false;S::Receive(p,1);Pump();Pump();CHECK(!authors && Decode<Reply>(session.sent.back()).code==2);
    Start(session,Role::Host);p=Packet();coop::element::segmentReach=false;S::Receive(p,1);Pump();Pump();CHECK(!authors && Decode<Reply>(session.sent.back()).code==2);
    Start(session,Role::Host);p=Packet();sourcePosition.X=800;S::Receive(p,1);Pump();Pump();CHECK(!authors && Decode<Reply>(session.sent.back()).code==2);
    Start(session,Role::Host);p=Packet();positionReady=false;S::Receive(p,1);Pump();CHECK(!authors && session.sent.empty());positionReady=true;Pump();CHECK(authors==1);
    Start(session,Role::Host);p=Packet();S::Receive(p,1);callback=[]{++mirrorGeneration;};Pump();CHECK(!authors && session.sent.empty());
    Start(session,Role::Host);p=Packet();S::Receive(p,1);callback=[]{++world;};Pump();CHECK(!authors && session.sent.empty());
    Start(session,Role::Host);p=Packet();attempted=authored=false;
    for(uint32_t sequence=1;sequence<=9;++sequence)S::Receive(Packet(sequence),1);
    Pump();CHECK(authors==8);authored=attempted=true;Pump();CHECK(authors==16 && S::g_host.back().code==1);
    Start(session,Role::Host);mirror=nullptr;
    for(uint32_t sequence=1;sequence<=129;++sequence)S::Receive(Packet(sequence),1);
    CHECK(S::g_host.size()==128);now+=1000;S::Receive(Packet(129),1);CHECK(S::g_host.size()==129);
    Start(session,Role::Host);p=Packet();authored=false;S::Receive(p,1);session.sendOk=false;Pump();CHECK(authors==1 && S::g_host.front().owed);
    session.sendOk=true;Pump();CHECK(authors==1 && Decode<Reply>(session.sent.back()).code==3);
    Start(session,Role::Host);p=Packet(1,true);remoteHoldGeneration=18;S::Receive(p,1);Pump();CHECK(!authors);
    remoteHoldGeneration=19;Pump();CHECK(authors==1);CHECK(created.soap==12 && created.velocity.X==5);
    Start(session,Role::Host);p=Packet();mirror=nullptr;S::Receive(p,1);now+=1501;Pump();CHECK(!authors && Decode<Reply>(session.sent.back()).code==2);
    Start(session,Role::Host);p=Packet();session.ready=false;S::Receive(p,1);CHECK(S::g_host.empty());session.ready=true;
    for(int cause=0;cause<7;++cause){auto bad=p;if(cause==0)bad.playerNo++;if(cause==1)bad.slot=2;if(cause==2)bad.sequence=0;
        if(cause==3)bad.generation=0;if(cause==4)bad.kind=2;if(cause==5)bad.x=NAN;if(cause==6)bad.soap=-1;
        S::Receive(bad,1);CHECK(S::g_host.empty());}
    S::Receive(p,1);coop::element::callback=[]{S::OnPeerLeft(1);++roster[1].playerNo;};Pump();CHECK(!authors && session.sent.empty());
    Start(session,Role::Host);p=Packet();S::Receive(p,1);onAuthor=[&]{S::Receive(p,1);};Pump();CHECK(authors==1);
    Start(session,Role::Host);p=Packet();S::Receive(p,1);onAuthor=[]{S::OnDisconnect();};Pump();CHECK(!S::g_session && session.sent.empty());
    Start(session,Role::Host);p=Packet();S::Receive(p,1);session.allowSend=[](int,auto,const void*,int){S::OnDisconnect();return true;};Pump();CHECK(!S::g_session);
    Start(session,Role::Client);auto call=HitCall();CHECK(S::BeforeCollision(call)==V::Cancel);int32_t entry=1654;call.locals=reinterpret_cast<uint8_t*>(&entry);
    CHECK(S::BeforeGraph(call)==V::Run);for(int value:{15,2901,0}){entry=value;CHECK(S::BeforeGraph(call)==V::Cancel);}
    call.locals=nullptr;CHECK(S::BeforeGraph(call)==V::Cancel);S::OnDisconnect();CHECK(S::BeforeGraph(call)==V::Run);
    Start(session,Role::Host);call=HitCall();CHECK(S::BeforeCollision(call)==V::Run);actor.live=false;S::AfterCollision(call);
    CHECK(S::g_cues.size()==1);Pump();CHECK(session.sent.size()==3 && surfaces==0 && sounds==0);
    auto splash=Decode<Splash>(session.sent.front());CHECK(splash.x==100 && splash.ax==110 && splash.nz==1);
    actor.live=true;S::BeforeCollision(call);S::AfterCollision(call);CHECK(S::g_cues.empty()); // same serial cannot splash twice
    Start(session,Role::Host);call=HitCall();call.fromOurCode=true;S::BeforeCollision(call);S::AfterCollision(call);
    CHECK(S::g_cues.size()==1); // native collision during a host-authoring call still presents
    Start(session,Role::Host);impactReady=false;ImpactEvent();CHECK(S::g_cues.empty());impactReady=true;
    onImpact=[]{S::OnDisconnect();};ImpactEvent();CHECK(!S::g_session && S::g_cues.empty());
    Start(session,Role::Client);S::ReceiveSplash(splash,2);CHECK(S::g_cues.empty());S::ReceiveSplash(splash,0);S::ReceiveSplash(splash,0);CHECK(S::g_cues.size()==1);
    soundReady=false;Pump();CHECK(surfaces==1 && sounds==0 && S::g_cues.size()==1);Pump();CHECK(surfaces==1);
    soundReady=true;Pump();CHECK(sounds==1 && S::g_cues.empty());
    splash.sequence++;surfaceReady=false;S::ReceiveSplash(splash,0);Pump();CHECK(sounds==2 && surfaces==1);
    surfaceReady=true;Pump();CHECK(sounds==2 && surfaces==2);
    splash.sequence++;S::ReceiveSplash(splash,0);now+=1501;Pump();CHECK(surfaces==2 && S::g_cues.empty());
    splash.sequence++;S::ReceiveSplash(splash,0);onSurface=[]{S::OnDisconnect();};Pump();CHECK(!S::g_session && S::g_cues.empty());
    Start(session,Role::Host);ImpactEvent();CHECK(S::g_cues.size()==1);bool replaced=false;
    session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !replaced){replaced=true;++roster[slot].playerNo;}return false;};
    Pump();CHECK(replaced && !(S::g_cues.front().pending&4));session.allowSend={};Pump();for(const auto& out:session.sent)CHECK(out.slot!=2);
    Start(session,Role::Host);ImpactEvent();++actor.serial;ImpactEvent();++actor.serial;ImpactEvent();auto oldest=S::g_cues.front().packet.sequence;
    session.allowSend=[&](int slot,auto,const void* data,int){Splash q{};std::memcpy(&q,data,sizeof(q));return !(slot==2 && q.sequence==oldest);};
    Pump();for(const auto& out:session.sent)CHECK(out.slot!=2);session.allowSend={};Pump();uint32_t last=0;int count=0;
    for(const auto& out:session.sent)if(out.slot==2){auto q=Decode<Splash>(out);CHECK(q.sequence>last);last=q.sequence;++count;}CHECK(count==3);
    Start(session,Role::Host);ImpactEvent();session.allowSend=[](int,auto,const void*,int){S::OnDisconnect();return true;};Pump();CHECK(!S::g_session && S::g_cues.empty());
    puts("PASS water pour controller: exact owner/source body, post-native velocity/soap capture, host hand/hold/reach/readiness, terminal dedup/retry, client collision/latent refusal, native impact, transient FIFO/partial retry and reentrant identity/session/world fences");
    return 0;
}
