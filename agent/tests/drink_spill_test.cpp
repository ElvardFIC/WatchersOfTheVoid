#define DRINK_SPILL_ACTUAL_TEST
#define main PriorGrimeMain
#include "grime_join_test.cpp"
#undef main
#include "../../src/votv-coop/src/coop/props/drink_spill_sync.cpp"
namespace spill_test {
namespace D=coop::drink_spill_sync;
namespace N=ue_wrap::drink_spill;
using Packet=coop::net::DrinkSpillPayload;
using LaunchPacket=coop::net::DrinkLaunchPayload;
void* hand=&test::actors[0];void* mirror=&test::actors[0];void* driven=nullptr;
void* heldWorld=nullptr;
uint32_t generation=7,mirrorGeneration=7;uint8_t local=1,kind=0;
bool sourceReady=true,readReady=true,authorReady=true,authorOK=true,announced=true,isMirror=false,reach=true,segment=true,existingReady=true;
bool coopDispatch=false,flightReady=true,retiredProof=false;
std::wstring proofClass;
bool handAxis=false;
bool scorchSource=false,privateScorchSource=false;std::function<void()> onScorchSource;
bool pourReady=true,pourReadReady=true,pourPlayReady=true;int pourSounds=0;std::vector<uint8_t> pourOrder;
int blockedPourKind=-1;
std::function<void()> onPourRead,onPourPlay;
int authors=0,sounds=0,destroys=0,freezes=0;std::function<void()> onRead,onAuthor,onSound,onReach;
void Run(std::function<void()>& fn){auto f=std::move(fn);fn={};if(f)f();}

}
namespace ue_wrap::prop_explosion {bool IsGameplayBlast(void* actor){spill_test::Run(spill_test::onScorchSource);
    return spill_test::scorchSource && actor==&test::actors[0] && test::actors[0].live;}
bool IsPrivatePcBlast(void* actor){return spill_test::privateScorchSource && actor==&test::actors[0] && test::actors[0].live;}}
namespace coop::hand_item {
void* LocalHandActor(){return spill_test::hand;}void* MirrorActorForSlot(uint8_t){return spill_test::mirror;}
uint32_t Generation(uint8_t){return spill_test::generation;}uint32_t MirrorGeneration(uint8_t){return spill_test::mirrorGeneration;}
bool IsMirrorActor(void*){return spill_test::isMirror;}
bool IsHandAxisActor(void*){return spill_test::handAxis;}
bool RecentGenerationClass(uint8_t,uint32_t generation,std::wstring& cls){using namespace spill_test;
    cls=!proofClass.empty()?proofClass:kind==3?L"prop_gasBottle_C":kind==2?L"prop_food_coffeeCup_C":kind==1?L"prop_snack_wine_C":L"prop_snack_beer_C";return retiredProof && generation==7;}
}
namespace coop::gravity_motion {void* Target(){return spill_test::heldWorld;}}
namespace ue_wrap::engine {bool ReadMainPlayerGrabState(void*,MainPlayerGrabState&){return false;}}
namespace coop::local_streams {void* LastHeldActor(){return spill_test::heldWorld;}uint16_t CurrentHoldGen(){return 7;}}
namespace coop::remote_prop {void* DrivenActor(int){return spill_test::driven;}uint16_t DrivenHoldGeneration(int){return 7;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring&){return spill_test::mirror;}}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*){return spill_test::announced;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return L"drink";}}
namespace ue_wrap::reflection {bool InCoopDispatch(){return spill_test::coopDispatch;}
int32_t FindParamOffset(void*,const wchar_t*){return 0;}int32_t FunctionFrameSize(void*){return 4;}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return &test::actors[0];}}
namespace ue_wrap::facts {Resolved ResolveOn(const Function& row,void* cls){return Resolved(cls,&row);}}
namespace ue_wrap::ufunction_hook {bool InstallPostHook(const facts::Resolved&,PostNativeCallback,bool){return true;}}
namespace ue_wrap::ufunction_hook {CallerFrame CurrentCallerFrame(){return {};}}
namespace ue_wrap::script_gate {bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){return true;}
bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return spill_test::flightReady;}}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(coop::net::Session&,uint8_t,float){return {};}
IntentSubject IntentTarget::Authorize(void* actor)const{spill_test::Run(spill_test::onReach);return {spill_test::reach?IntentOutcome::Ok:IntentOutcome::OutOfReach,actor};}
IntentSubject IntentTarget::AuthorizeSegment(const ue_wrap::FVector&,const ue_wrap::FVector&)const{return {spill_test::segment?IntentOutcome::Ok:IntentOutcome::OutOfReach,nullptr};}
}
namespace ue_wrap::engine {bool DestroyActor(void* actor){++spill_test::destroys;static_cast<test::Actor*>(actor)->live=false;return true;}}
namespace ue_wrap::drink_spill {
bool PourReady(void*){return spill_test::pourReady;}
bool ReadPourCue(void*,const ufunction_hook::CallerFrame&,uint8_t& kind,FVector& point,const std::function<bool()>& current){
    kind=spill_test::kind;point={20,0,0};spill_test::Run(spill_test::onPourRead);return spill_test::pourReady && spill_test::pourReadReady && current();}
bool PourSound(uint8_t kind,const FVector&,const std::function<bool()>& current){spill_test::Run(spill_test::onPourPlay);
    if(!spill_test::pourPlayReady || spill_test::blockedPourKind==kind || !current())return false;++spill_test::pourSounds;spill_test::pourOrder.push_back(kind);return true;}
bool Valid(const State& s){return s.kind<=4 && std::isfinite(s.location.X) && s.process>=0 && s.process<1.e6f;}
bool IsProjectile(void* actor){return actor==&test::actors[1];}
bool ProjectileKind(void* actor,uint8_t& kind){kind=spill_test::kind;return kind<=3 && IsProjectile(actor);}
bool SourceClassKind(const std::wstring& cls,uint8_t& kind){
    if(cls==L"prop_gasBottle_C")kind=3;else if(cls==L"prop_food_coffeeCup_C")kind=2;else if(cls==L"prop_snack_wine_C")kind=1;else if(cls==L"prop_snack_beer_C")kind=0;else return false;
    return spill_test::sourceReady;}
bool Valid(const Launch& s){return s.kind<=3 && std::isfinite(s.location.X) && std::isfinite(s.velocity.X);}
bool ReadLaunch(void* actor,Launch& out,const std::function<bool()>& current){out.location=static_cast<test::Actor*>(actor)->location;out.velocity={1000,0,0};out.kind=spill_test::kind;
    if(out.kind==2){out.scale={.75f,.75f,.75f};out.size=5;}
    if(out.kind==3){out.scale={1,1,1};out.size=5;}
    spill_test::Run(spill_test::onRead);return spill_test::readReady && current();}
bool FreezeLaunch(void*,const std::function<bool()>& current){++spill_test::freezes;return current();}
Spawn AuthorLaunch(const Launch& state,const std::function<bool()>& current){State s{};s.kind=state.kind;s.location=state.location;s.process=100;return Author(s,false,current);}
bool SourceKind(void* actor,uint8_t& kind){kind=spill_test::kind;return kind<=2 && actor==&test::actors[0] && spill_test::sourceReady;}
bool BreakSourceKind(void* actor,uint8_t& kind){kind=spill_test::kind;return kind<=3 && actor==&test::actors[0] && spill_test::sourceReady;}
bool DecalKind(void* actor,uint8_t& kind){kind=spill_test::kind;if(!actor)return false;const auto id=static_cast<test::Actor*>(actor)->id;return id==2 || id==4 || id>=100;}
bool Read(void* actor,State& out,const std::function<bool()>& current){auto& a=*static_cast<test::Actor*>(actor);
    out.location=a.location;out.process=a.process;out.scale={.25f,.25f,.25f};out.kind=spill_test::kind;
    spill_test::Run(spill_test::onRead);return spill_test::readReady && current();}
Spawn Author(const State& state,bool canonical,const std::function<bool()>& current){
    if(!spill_test::authorReady || !current())return {};auto* actor=&test::actors[100+(++spill_test::authors)];
    actor->location=state.location;actor->process=canonical?state.process:100;spill_test::Run(spill_test::onAuthor);
    return {spill_test::authorOK && current()?actor:nullptr,true};}
bool ImpactSound(const ue_wrap::FVector&,const std::function<bool()>& current){spill_test::Run(spill_test::onSound);if(!current())return false;++spill_test::sounds;return true;}
bool ApplyExisting(void* actor,const State& state,const std::function<bool()>& current){if(!spill_test::existingReady || !current())return false;
    static_cast<test::Actor*>(actor)->process=state.process;return true;}
}
namespace spill_test {
void Reset(coop::net::Session& session,coop::net::Role role){
    D::OnDisconnect();test::Reset(session,0);session.mode=role;local=role==coop::net::Role::Host?0:1;
    coop::players::Registry::Get().peer=local;
    hand=mirror=&test::actors[0];driven=heldWorld=nullptr;generation=mirrorGeneration=7;kind=0;
    scorchSource=privateScorchSource=false;onScorchSource={};
    sourceReady=readReady=authorReady=authorOK=announced=reach=segment=existingReady=true;isMirror=coopDispatch=retiredProof=handAxis=false;flightReady=true;
    pourReady=pourReadReady=pourPlayReady=true;blockedPourKind=-1;pourSounds=0;pourOrder.clear();onPourRead=onPourPlay={};
    authors=sounds=destroys=freezes=0;proofClass.clear();onRead=onAuthor=onSound=onReach={};D::g_hook=D::g_watch=true;D::Install(&session);CHECK(D::Reconcile());
    D::IndexComplete(test::world);
    test::activeInvocations={1};test::actors[2].process=100;
}
void NativeSpill(){test::activeInvocations={1};ue_wrap::script_gate::Call call{};call.object=&test::actors[0];call.invocation=1;
    CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Run);
    D::ObserveFinish(nullptr,&test::actors[0],&test::actors[1]);
    if(D::Host())D::ObserveFinish(nullptr,&test::actors[1],&test::actors[2]);test::activeInvocations.clear();
}
LaunchPacket Request(){LaunchPacket p{};p.sequence=1;p.slot=1;p.playerNo=11;p.generation=7;p.x=20;p.sx=p.sy=p.sz=.25f;p.size=1;p.vx=1000;return p;}
}
#ifndef DRINK_SPILL_FIXTURE_ONLY
int main(){using namespace spill_test;using namespace test;using coop::net::Role;coop::net::Session session;
    spill_test::Reset(session,Role::Host);NativeSpill();D::Tick();CHECK(D::g_records.size()==1 && session.spills.size()==3 && authors==0 && sounds==0);
    const auto birth=session.spills[0].payload;CHECK(birth.id && birth.nonce && birth.op==1);const auto dynamic=D::KeyFor(&actors[2]);CHECK(dynamic.starts_with(L"ds_") && dynamic.size()<=31);
    const auto staticKey=S::PosKey(&actors[3]);actors[3].location=actors[2].location;S::Floor()[S::PosKey(&actors[3])]=0;
    S::HubPassBegin(nullptr,true);S::HubMatch(nullptr,&actors[2]);S::HubMatch(nullptr,&actors[3]);S::HubPassComplete(nullptr,true,world);
    CHECK(actors[2].process==100 && S::PosKey(&actors[2])==dynamic && actors[3].process==0);
    // New same-place birth has a new key and cannot inherit a wiped predecessor.
    S::Floor()[dynamic]=0;auto another=birth;actors[4].location=actors[2].location;actors[4].process=100;
    const auto id=D::Register(&actors[4],another);CHECK(id!=birth.id && D::KeyFor(&actors[4])!=dynamic && actors[4].process==100);
    spill_test::Reset(session,Role::Client);D::Receive(birth,0);D::Tick();CHECK(authors==1 && sounds==1 && D::g_records.size()==1);
    D::Receive(birth,0);D::Tick();CHECK(authors==1 && sounds==1);
    spill_test::Reset(session,Role::Client);auto join=birth;join.op=2;D::Receive(join,0);D::Tick();CHECK(authors==1 && sounds==0);
    spill_test::Reset(session,Role::Client);NativeSpill();CHECK(D::g_local.size()==1 && D::g_flights.empty());
    D::Tick();CHECK(D::g_local.size()==1 && session.launches.size()==1 && !actors[1].live && actors[2].live);
    const auto request=session.launches[0].payload;CHECK(request.op==0 && request.playerNo==11 && request.generation==7 && request.size==1);
    spill_test::Reset(session,Role::Host);D::ReceiveLaunch(request,1);D::Tick();CHECK(authors==1 && D::g_records.empty() && sounds==0 && D::g_flights.size()==1);
    const auto accepted=session.launches[0].payload;CHECK(accepted.op==1);D::ReceiveLaunch(request,1);D::Tick();CHECK(authors==1 && session.launches.size()==2);
    D::g_requests.clear();D::ReceiveLaunch(request,1);D::Tick();CHECK(authors==1);
    D::ObserveFinish(nullptr,&actors[101],&actors[2]);D::Tick();CHECK(D::g_records.size()==1 && session.spills.size()==3 && sounds==0);
    auto canonical=D::g_records.begin()->second.packet;
    spill_test::Reset(session,Role::Client);NativeSpill();D::Tick();D::ReceiveLaunch(accepted,0);CHECK(D::g_local.empty());D::Receive(canonical,0);D::Tick();CHECK(authors==1 && sounds==1);
    // Native metadata may become ready later; an attempted failure remains terminal.
    spill_test::Reset(session,Role::Host);authorReady=false;D::ReceiveLaunch(Request(),1);D::Tick();CHECK(!authors && D::g_requests.size()==1);
    authorReady=true;D::Tick();D::Tick();CHECK(authors==1 && D::g_flights.size()==1);
    spill_test::Reset(session,Role::Host);authorOK=false;D::ReceiveLaunch(Request(),1);D::Tick();D::Tick();CHECK(authors==1 && D::g_requests.begin()->second.done && session.launches[0].payload.op==3);
    spill_test::Reset(session,Role::Client);authorOK=false;D::Receive(birth,0);D::Tick();D::Tick();CHECK(authors==1 && D::g_records.begin()->second.failed);
    // Late join and refused sends sample current cleaning, without historical impact audio.
    spill_test::Reset(session,Role::Host);NativeSpill();session.allowSpill=[](int,const auto&){return false;};D::Tick();CHECK(session.spills.empty());
    actors[2].process=25;session.allowSpill={};D::Tick();CHECK(session.spills.size()==3 && session.spills[0].payload.process==25);
    session.spills.clear();D::SendAllToSlot(2);D::Tick();CHECK(session.spills.size()==1 && session.spills[0].payload.op==2);
    actors[2].live=false;D::OnRemoved(&actors[2]);D::Tick();CHECK(D::g_records.begin()->second.retired && session.spills.back().payload.op==3);
    spill_test::Reset(session,Role::Client);D::Receive(birth,0);D::Tick();auto retired=birth;retired.op=3;D::Receive(retired,0);D::Receive(birth,0);D::Tick();CHECK(authors==1 && D::g_records.begin()->second.retired);
    for(int cause=0;cause<5;++cause){spill_test::Reset(session,Role::Host);onReach=[&,cause]{if(cause==0)D::OnDisconnect();if(cause==1)++world;
        if(cause==2)D::OnPeerLeft(1);if(cause==3)++people[1].playerNo;if(cause==4)++actors[0].serial;};D::ReceiveLaunch(Request(),1);D::Tick();CHECK(!authors);}
    spill_test::Reset(session,Role::Host);reach=false;D::ReceiveLaunch(Request(),1);D::Tick();CHECK(!authors);
    spill_test::Reset(session,Role::Host);kind=1;D::ReceiveLaunch(Request(),1);D::Tick();CHECK(!authors);
    spill_test::Reset(session,Role::Client);announced=false;ue_wrap::script_gate::Call call{};call.object=&actors[0];call.invocation=1;CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Cancel);
    announced=true;isMirror=true;CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Cancel);
    // A native host candidate cannot inherit an old static-cell floor before capture.
    spill_test::Reset(session,Role::Host);S::Floor()[S::PosKey(&actors[2])]=0;NativeSpill();
    S::HubPassBegin(nullptr,true);S::HubMatch(nullptr,&actors[2]);S::HubPassComplete(nullptr,true,world);
    CHECK(actors[2].process==100 && D::ExcludeFromStaticIndex(&actors[2]));D::Tick();CHECK(actors[2].process==100 && !D::KeyFor(&actors[2]).empty());
    // Saved liquid decals enroll quietly and are adopted by a joiner's existing actor.
    spill_test::Reset(session,Role::Host);actors[2].process=35;S::Floor()[S::PosKey(&actors[2])]=0;D::ObserveExisting(&actors[2]);
    S::HubPassBegin(nullptr,true);S::HubMatch(nullptr,&actors[2]);S::HubPassComplete(nullptr,true,world);
    CHECK(actors[2].process==35);D::Tick();
    CHECK(D::g_records.size()==1 && session.spills[0].payload.op==2 && sounds==0);auto saved=session.spills[0].payload;
    spill_test::Reset(session,Role::Client);D::g_baselineComplete=false;D::Receive(saved,0);D::Tick();CHECK(!authors);
    actors[2].location={saved.x,saved.y,saved.z};D::ObserveExisting(&actors[2]);D::IndexComplete(world);D::Tick();
    CHECK(authors==0 && actors[2].process==35 && D::g_records.begin()->second.actor.Get()==&actors[2]);
    D::OnDisconnect();CHECK(actors[2].live && S::Floor().empty());
    // A joiner's saved decal is isolated from old static-cell cleaning until claimed.
    spill_test::Reset(session,Role::Client);S::Floor()[S::PosKey(&actors[2])]=0;
    S::HubPassBegin(nullptr,true);S::HubMatch(nullptr,&actors[2]);S::HubPassComplete(nullptr,true,world);CHECK(actors[2].process==100);
    D::Receive(saved,0);D::Tick();CHECK(!authors && actors[2].process==35 && !D::ExcludeFromStaticIndex(&actors[2]));
    // A newer join value lowers an existing mirror and a replay cannot raise it.
    saved.process=15;D::Receive(saved,0);D::Tick();CHECK(actors[2].process==15);
    saved.process=35;D::Receive(saved,0);D::Tick();CHECK(actors[2].process==15 && !authors);
    // A retirement also ends a saved actor whose native repaint is still waiting.
    spill_test::Reset(session,Role::Client);D::ObserveExisting(&actors[2]);existingReady=false;D::Receive(saved,0);D::Tick();
    CHECK(!authors && D::g_records.begin()->second.adopted.Get()==&actors[2]);saved.op=3;D::Receive(saved,0);CHECK(!actors[2].live);
    // Accepted host physics continues when its owner leaves; pending proof is dropped.
    spill_test::Reset(session,Role::Host);D::ReceiveLaunch(Request(),1);D::Tick();D::OnPeerLeft(1);CHECK(actors[101].live && D::g_requests.empty() && D::g_flights.size()==1);
    spill_test::Reset(session,Role::Client);NativeSpill();D::OnDisconnect();CHECK(!actors[1].live);
    // A scan inside FinishSpawningActor and a repeated native callback share one birth.
    spill_test::Reset(session,Role::Host);D::ObserveExisting(&actors[2]);NativeSpill();NativeSpill();D::Tick();CHECK(D::g_records.size()==1 && session.spills.size()==3);
    CHECK(session.spills[0].payload.op==1);CHECK(D::Register(&actors[2],birth)==D::g_records.begin()->first && D::g_records.size()==1);
    // Host break/fire factories use the same measured flight seam, even during
    // a canonical mod-authored break; this observer never cancels native break.
    spill_test::Reset(session,Role::Host);ue_wrap::script_gate::Call broken{};broken.object=&actors[0];broken.invocation=1;broken.fromOurCode=true;
    coopDispatch=true;CHECK(D::ObserveHostBreak(broken)==ue_wrap::script_gate::Verdict::Run);
    D::ObserveFinish(nullptr,&actors[0],&actors[1]);D::ObserveFinish(nullptr,&actors[1],&actors[2]);D::Tick();CHECK(D::g_records.size()==1 && session.spills[0].payload.op==1);
    spill_test::Reset(session,Role::Client);CHECK(D::ObserveHostBreak(broken)==ue_wrap::script_gate::Verdict::Run);
    D::ObserveFinish(nullptr,&actors[0],&actors[1]);CHECK(D::g_flights.empty());
    // Expired or dead candidates also consume the native cleanup budget.
    spill_test::Reset(session,Role::Client);for(int i=0;i<30;++i){D::Candidate row{};row.actor.Set(&actors[i]);row.expires=1;D::g_candidates.push_back(row);}
    D::Capture();CHECK(destroys==8 && D::g_candidates.size()==22);
    // Recycled projectile storage cannot be destroyed by an older local request.
    spill_test::Reset(session,Role::Client);NativeSpill();++actors[1].serial;D::RetireLocal(1);CHECK(actors[1].live);
    spill_test::Reset(session,Role::Client);D::ObserveExisting(&actors[2]);++actors[2].serial;D::ObserveExisting(&actors[2]);D::Capture();
    CHECK(D::ExcludeFromStaticIndex(&actors[2]) && D::g_existing.size()==1);
    // A late retirement of an old generation cannot unbind a newly adopted decal.
    spill_test::Reset(session,Role::Client);saved.op=2;saved.id=1;actors[2].location={saved.x,saved.y,saved.z};D::ObserveExisting(&actors[2]);D::Receive(saved,0);D::Tick();
    ++actors[2].serial;D::ObserveExisting(&actors[2]);auto successor=saved;successor.id=2;D::Receive(successor,0);D::Tick();
    auto old=saved;old.op=3;D::Receive(old,0);CHECK(actors[2].live && D::KeyFor(&actors[2])==D::DynamicKey(successor.nonce,2));
    // Two matching saved decals are claimed at most once each.
    spill_test::Reset(session,Role::Client);actors[2].location=actors[4].location={saved.x,saved.y,saved.z};
    D::ObserveExisting(&actors[2]);D::ObserveExisting(&actors[4]);D::Receive(saved,0);D::Receive(successor,0);D::Tick();
    CHECK(!authors && D::g_records.at(1).actor.Get()!=D::g_records.at(2).actor.Get() && D::g_existing.empty());
    // Cleaning/join updates during construction do not restart the native factory.
    spill_test::Reset(session,Role::Client);auto newer=birth;newer.process=12;onAuthor=[&]{D::Receive(newer,0);};D::Receive(birth,0);D::Tick();
    CHECK(authors==1 && D::g_records.begin()->second.actor.Get());D::Tick();CHECK(authors==1 && actors[101].process==12);
    spill_test::Reset(session,Role::Client);authorOK=false;onAuthor=[&]{D::Receive(newer,0);};D::Receive(birth,0);D::Tick();D::Tick();
    CHECK(authors==1 && D::g_records.begin()->second.failed);
    spill_test::Reset(session,Role::Client);D::Receive(birth,0);auto malformed=birth;malformed.x+=10;D::Receive(malformed,0);
    CHECK(D::g_records.begin()->second.packet.x==birth.x);
    // Initial host adoption replaces a different local save; an identified newer
    // minimum still wins, and a live birth cannot steal an old saved decal.
    spill_test::Reset(session,Role::Client);actors[2].location={saved.x,saved.y,saved.z};actors[2].process=5;
    D::ObserveExisting(&actors[2]);D::Receive(saved,0);D::Tick();CHECK(!authors && actors[2].process==saved.process);
    spill_test::Reset(session,Role::Client);actors[2].location={saved.x,saved.y,saved.z};D::ObserveExisting(&actors[2]);
    S::Floor()[D::DynamicKey(saved.nonce,saved.id)]=12;D::Receive(saved,0);D::Tick();CHECK(!authors && actors[2].process==12);
    spill_test::Reset(session,Role::Client);actors[2].location={birth.x,birth.y,birth.z};actors[2].process=5;D::ObserveExisting(&actors[2]);
    D::Receive(birth,0);D::Tick();CHECK(authors==1 && D::g_records.begin()->second.actor.Get()!=&actors[2] && actors[101].process==birth.process);
    spill_test::Reset(session,Role::Client);actors[2].location={birth.x,birth.y,birth.z};D::ObserveExisting(&actors[2]);D::Receive(birth,0);
    auto joining=birth;joining.op=2;D::Receive(joining,0);D::Tick();CHECK(authors==1 && D::g_records.begin()->second.actor.Get()!=&actors[2]);
    // Completed history does not spend the pending work budget of a fresh birth.
    spill_test::Reset(session,Role::Host);for(uint32_t id=1;id<=1000;++id)D::g_records[id]={};D::g_id=1000;
    NativeSpill();D::Tick();CHECK(session.spills.size()==3 && D::g_pendingRecords.empty());
    spill_test::Reset(session,Role::Host);onReach=[&]{D::OnPeerLeft(1);D::ReceiveLaunch(Request(),1);};D::ReceiveLaunch(Request(),1);D::Tick();CHECK(authors==0);
    D::Tick();CHECK(authors==1);
    spill_test::Reset(session,Role::Host);onReach=[]{++D::g_peerEpoch[1];};D::ReceiveLaunch(Request(),1);D::Tick();CHECK(!authors);
    // Client gates only beer/wine, permit native BeginPlay and refuse every impact entry.
    spill_test::Reset(session,Role::Client);ue_wrap::script_gate::Call flight{};flight.object=&actors[1];flight.tag=D::kTag+3;
    CHECK(D::BeforeFlight(flight)==ue_wrap::script_gate::Verdict::Cancel);flight.tag=D::kTag+4;flight.function=&actors[0];int32_t entry=15;flight.locals=reinterpret_cast<uint8_t*>(&entry);
    CHECK(D::BeforeFlight(flight)==ue_wrap::script_gate::Verdict::Run);entry=130;CHECK(D::BeforeFlight(flight)==ue_wrap::script_gate::Verdict::Cancel);
    flight.locals=nullptr;CHECK(D::BeforeFlight(flight)==ue_wrap::script_gate::Verdict::Cancel);flight.object=&actors[3];CHECK(D::BeforeFlight(flight)==ue_wrap::script_gate::Verdict::Run);
    flightReady=false;call.object=&actors[0];call.invocation=1;CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Cancel);
    // A final consumed hand generation can launch without a surviving mirror, only with its class proof.
    spill_test::Reset(session,Role::Host);mirror=nullptr;retiredProof=true;D::ReceiveLaunch(Request(),1);D::Tick();CHECK(authors==1);
    spill_test::Reset(session,Role::Host);mirror=nullptr;D::ReceiveLaunch(Request(),1);D::Tick();CHECK(!authors);
    spill_test::Reset(session,Role::Host);mirror=nullptr;retiredProof=true;segment=false;D::ReceiveLaunch(Request(),1);D::Tick();CHECK(!authors);
    // Host ignores client-chosen impacts entirely; acknowledgements must echo the launch exactly.
    spill_test::Reset(session,Role::Host);D::Receive(birth,1);D::Tick();CHECK(!authors && D::g_records.empty());
    spill_test::Reset(session,Role::Client);NativeSpill();D::Tick();auto wrong=accepted;wrong.vx+=1;D::ReceiveLaunch(wrong,0);CHECK(D::g_local.size()==1);
    D::ReceiveLaunch(accepted,2);CHECK(D::g_local.size()==1);D::ReceiveLaunch(accepted,0);CHECK(D::g_local.empty());
    spill_test::Reset(session,Role::Client);NativeSpill();activeInvocations={1};D::Tick();CHECK(session.launches.empty() && actors[1].live);activeInvocations.clear();actors[0].live=false;hand=nullptr;D::Tick();CHECK(session.launches.size()==1);
    spill_test::Reset(session,Role::Host);onAuthor=[]{D::OnPeerLeft(1);D::ReceiveLaunch(Request(),1);};D::ReceiveLaunch(Request(),1);D::Tick();
    CHECK(authors==1 && !D::g_requests.begin()->second.done && D::g_flights.empty());D::Tick();CHECK(authors==2 && D::g_flights.size()==1);
    spill_test::Reset(session,Role::Host);onAuthor=[&]{session.ready[1]=false;};D::ReceiveLaunch(Request(),1);D::Tick();CHECK(authors==1 && D::g_requests.empty());
    spill_test::Reset(session,Role::Client);NativeSpill();generation=99;D::Tick();CHECK(session.launches[0].payload.generation==7);
    for(int cause=0;cause<5;++cause){spill_test::Reset(session,Role::Client);NativeSpill();spill_test::onRead=[&,cause]{if(cause==0)D::OnDisconnect();if(cause==1)++world;
        if(cause==2)++actors[1].serial;if(cause==3)++people[1].playerNo;if(cause==4)session.mode=Role::Host;};D::Tick();CHECK(session.launches.empty());}
    for(int cause=0;cause<7;++cause){spill_test::Reset(session,Role::Host);auto bad=Request();if(cause==0)bad.sx=1;if(cause==1)bad.size=2;
        if(cause==2)bad.op=1;if(cause==3)bad.reserved[0]=1;if(cause==4)bad.sequence=0;if(cause==5)bad.playerNo=12;if(cause==6)bad.source.len=32;
        D::ReceiveLaunch(bad,1);D::Tick();CHECK(!authors);}
    // A refused acknowledgement stays owed without spawning again, and resamples the recipient.
    spill_test::Reset(session,Role::Host);session.allowLaunch=[](int,const auto&){return false;};D::ReceiveLaunch(Request(),1);D::Tick();CHECK(authors==1 && session.launches.empty());
    session.allowLaunch={};D::Tick();CHECK(authors==1 && session.launches.size()==1);
    // Slow first entries cannot monopolize local launch capture.
    spill_test::Reset(session,Role::Client);readReady=false;
    for(uint32_t i=1;i<=12;++i){D::Local row{};row.actor.Set(&actors[1]);row.packet=Request();row.packet.sequence=i;row.expires=D::Now()+3000;D::g_local[i]=row;}
    D::DrainLocal();CHECK(D::g_localCursor==8);D::DrainLocal();CHECK(D::g_localCursor==4);
    spill_test::Reset(session,Role::Client);D::g_finishFunction.Reset();call.object=&actors[0];call.invocation=1;CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Cancel);
    spill_test::Reset(session,Role::Host);D::g_finishFunction.Reset();D::ReceiveLaunch(Request(),1);D::Tick();CHECK(!authors && D::g_requests.size()==1);
    D::Install(&session);D::Tick();CHECK(authors==1);
    // Coffee uses its own native size/scale and consumes a whole cup in one tick.
    spill_test::Reset(session,Role::Client);kind=2;NativeSpill();actors[0].live=false;hand=nullptr;D::Tick();
    CHECK(session.launches.size()==1 && session.launches[0].payload.kind==2 && session.launches[0].payload.size==5 && session.launches[0].payload.sx==.75f);
    const auto coffee=session.launches[0].payload;
    spill_test::Reset(session,Role::Host);kind=2;mirror=nullptr;retiredProof=true;D::ReceiveLaunch(coffee,1);D::Tick();CHECK(authors==1 && D::g_flights.size()==1);
    D::ObserveFinish(nullptr,&actors[101],&actors[2]);D::Tick();CHECK(D::g_records.size()==1 && session.spills[0].payload.kind==2);
    for(int cause=0;cause<4;++cause){spill_test::Reset(session,Role::Host);kind=2;auto bad=coffee;
        if(cause==0)bad.size=1;if(cause==1)bad.sx=.49f;if(cause==2)bad.sy=.9f;if(cause==3)coop::net::WireKeyFromString(L"world_coffee",bad.source);
        D::ReceiveLaunch(bad,1);D::Tick();CHECK(!authors);}
    spill_test::Reset(session,Role::Host);kind=2;driven=&actors[0];call.object=&actors[0];call.invocation=1;CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Run);
    kind=0;CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Cancel);
    spill_test::Reset(session,Role::Client);kind=2;hand=nullptr;heldWorld=&actors[0];call.object=&actors[0];call.invocation=1;
    CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Cancel);kind=0;CHECK(D::BeforeSource(call)==ue_wrap::script_gate::Verdict::Run);
    puts("PASS drink spills: exact source/flight/decal capture, host request authority, unique cleaning generations, canonical births/join/retirement, host flight collision and exact owner launch acknowledgements, partial construction and replay/identity/readiness fences");
}
#endif
