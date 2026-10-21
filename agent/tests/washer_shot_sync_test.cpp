#define main PriorWaterPourMain
#include "water_pour_sync_test.cpp"
#undef main
namespace test {
void WasherStart(coop::net::Session& session,Role role){Start(session,role);shot=washer=true;bucket=pour=false;
    native.soap=0;native.kind=FX::Kind::Washer;native.rotation={12,34,5};native.velocity={5000,10,20};
    S::g_washerTick.Set(&ui);active={{&ui,&ui}};
}
Request ShotPacket(){auto p=Packet();p.kind=1;p.soap=0;p.pitch=12;p.yaw=34;p.roll=5;p.vx=5000;return p;}
}
int main(){
    using namespace test;coop::net::Session session;
    WasherStart(session,Role::Client);CHECK(S::ObserveBirth(&ui,&actor));
    S::AfterSource({});CHECK(reads==0 && retires==0);native.power=0;CaptureFinished();CHECK(!S::g_local.front().captured);
    native.power=1;CaptureFinished();CHECK(S::g_local.front().captured && retires==1);
    Pump();CHECK(session.sent.size()==1);auto p=Decode<Request>(session.sent.front());
    CHECK(p.kind==1 && p.power==1 && p.soap==0 && p.vx==5000 && p.yaw==34);
    WasherStart(session,Role::Host);p=ShotPacket();S::Receive(p,1);Pump();
    CHECK(authors==1 && created.kind==FX::Kind::Washer && created.power==1 && created.rotation.Yaw==34);
    CHECK(sounds==1 && surfaces==0);unsigned launches=0,replies=0;
    for(const auto& sent:session.sent){if(sent.kind==coop::net::ReliableKind::WaterSplash){
        const auto cue=Decode<Splash>(sent);CHECK(cue.kind==2 && sent.slot!=1 && cue.ax==100);++launches;
    }else if(sent.kind==coop::net::ReliableKind::WaterPour){CHECK(Decode<Reply>(sent).code==1);++replies;}}
    CHECK(launches==2 && replies==1);S::Receive(p,1);Pump();CHECK(authors==1 && sounds==1);
    // The exact native hit delegate and graph remain host-owned, including the old Delay.
    WasherStart(session,Role::Client);auto call=HitCall();CHECK(S::BeforeCollision(call)==V::Cancel);
    int32_t entry=982;call.locals=reinterpret_cast<uint8_t*>(&entry);CHECK(S::BeforeGraph(call)==V::Run);
    for(int value:{15,1282,0}){entry=value;CHECK(S::BeforeGraph(call)==V::Cancel);}
    CHECK(S::BeforeParentHit(call)==V::Cancel);entry=3875;CHECK(S::BeforeParentGraph(call)==V::Run);
    for(int value:{4087,3332,15,4641}){entry=value;CHECK(S::BeforeParentGraph(call)==V::Cancel);}
    shot=false;CHECK(S::BeforeParentHit(call)==V::Run && S::BeforeParentGraph(call)==V::Run);
    WasherStart(session,Role::Host);call=HitCall();CHECK(S::BeforeCollision(call)==V::Run);actor.live=false;S::AfterCollision(call);
    CHECK(S::g_cues.size()==1 && S::g_cues.front().packet.kind==1);Pump();CHECK(surfaces==0 && sounds==0);
    auto splash=Decode<Splash>(session.sent.front());WasherStart(session,Role::Client);S::ReceiveSplash(splash,0);Pump();CHECK(surfaces==1 && sounds==1);
    // Local host shots retain their post-native launch cue through field readiness.
    WasherStart(session,Role::Host);S::ObserveHostBirth(&ui,&actor);CHECK(S::g_nativeShots.size()==1);S::CaptureHost();CHECK(reads==0);
    active.clear();readable=false;S::AfterSource({});CHECK(S::g_nativeShots.size()==1);readable=true;Pump();
    CHECK(S::g_nativeShots.empty() && sounds==0 && session.sent.size()==3);
    for(const auto& sent:session.sent)CHECK(Decode<Splash>(sent).kind==2);
    // Launch sound is separate from the native birth emitter, expires and is never join history.
    WasherStart(session,Role::Client);Splash launch{};launch.sequence=1;launch.kind=2;launch.x=launch.ax=100;launch.nz=1;
    S::ReceiveSplash(launch,0);Pump();CHECK(surfaces==0 && sounds==1 && S::g_cues.empty());
    ++launch.sequence;soundReady=false;S::ReceiveSplash(launch,0);Pump();CHECK(S::g_cues.size()==1);
    now+=1501;Pump();CHECK(S::g_cues.empty() && sounds==1);
    // Remove only the owner's predicted native initial particle. Canonical births keep theirs.
    WasherStart(session,Role::Client);S::g_shotBegin.Set(&ui);S::g_shotGraph.Set(&actor);nativeCaller=&actor;
    active.push_back({&ui,&actor});S::AfterLaunchEmitter(nullptr,&actor,&ui);CHECK(retiredEffects==1);
    coopDispatch=true;S::AfterLaunchEmitter(nullptr,&actor,&ui);CHECK(retiredEffects==1);coopDispatch=false;
    nativeCaller=&ui;S::AfterLaunchEmitter(nullptr,&actor,&ui);CHECK(retiredEffects==1);nativeCaller=&actor;
    active={{&ui,&actor}};S::AfterLaunchEmitter(nullptr,&actor,&ui);CHECK(retiredEffects==1);
    // Native source refusal protects mirrors, unowned copies and unavailable suppression.
    WasherStart(session,Role::Client);call={};call.object=&ui;call.function=&ui;call.tag=S::kTag+2;
    sourceMirror=true;CHECK(S::BeforeSource(call)==V::Cancel);sourceMirror=false;
    hand=nullptr;owned=false;CHECK(S::BeforeSource(call)==V::Cancel);hand=&ui;owned=true;
    S::g_launchHookReady=false;CHECK(S::BeforeSource(call)==V::Cancel);S::g_launchHookReady=true;CHECK(S::BeforeSource(call)==V::Run);
    ue_wrap::script_gate::watchLive=false;CHECK(S::BeforeSource(call)==V::Cancel);ue_wrap::script_gate::watchLive=true;
    announced=false;CHECK(S::BeforeSource(call)==V::Cancel);announced=true;
    WasherStart(session,Role::Host);sourceMirror=true;CHECK(S::BeforeSource(call)==V::Cancel);
    WasherStart(session,Role::Host);p=ShotPacket();p.power=.5f;S::Receive(p,1);p=ShotPacket();p.soap=12;S::Receive(p,1);CHECK(S::g_host.empty());
    WasherStart(session,Role::Host);p=ShotPacket();washer=false;S::Receive(p,1);Pump();CHECK(authors==0 && Decode<Reply>(session.sent.back()).code==2);
    WasherStart(session,Role::Host);positionReady=false;
    for(uint32_t i=1;i<=17;++i){p=ShotPacket();p.sequence=i;S::Receive(p,1);}Pump();CHECK(positionReads==8 && authors==0);
    Pump();CHECK(positionReads==16 && positionTurns.at(8)==9);Pump();CHECK(positionReads==24 && positionTurns.at(16)==0);
    WasherStart(session,Role::Host);blockedMirrors=1u<<1;
    for(uint32_t i=1;i<=16;++i){p=ShotPacket();p.sequence=i;S::Receive(p,1);}
    p=ShotPacket();p.slot=2;p.playerNo=roster[2].playerNo;S::Receive(p,2);Pump();Pump();Pump();CHECK(authors==1);
    puts("PASS washer shots: exact native source/delegate/latent authority, post-finish speed/power/rotation, host launch/impact cues, owner exclusion, local-host field retry and predicted-emitter scope");
}
