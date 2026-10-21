#define main PriorWaterPourMain
#include "water_pour_sync_test.cpp"
#undef main
using Launch=coop::net::WaterLaunchPayload;
Launch SoundPacket(uint32_t sequence=1,uint8_t kind=0,uint8_t slot=1){
    Launch p{};p.sequence=sequence;p.generation=7;p.kind=kind;p.slot=slot;p.playerNo=test::roster[slot].playerNo;
    p.x=100;p.y=200;p.z=300;return p;
}
int main(){
    using namespace test;coop::net::Session session;uint8_t params[128]{};
    Start(session,Role::Client);CHECK(S::BeforeBucketSoundAt(nullptr,&ui,params)==V::Run && S::g_bucketLocal.size()==1);
    session.sendOk=false;Pump();CHECK(S::g_bucketLocal.size()==1 && session.sent.empty());
    session.sendOk=true;now+=150;Pump();CHECK(S::g_bucketLocal.empty() && session.sent.size()==1 && bucketSounds==0);
    auto request=Decode<Launch>(session.sent.front());CHECK(request.generation==7 && request.playerNo==11 && request.op==0 && request.kind==0);
    Start(session,Role::Host);mirrorGeneration=0;S::ReceiveLaunch(request,1);Pump();CHECK(S::g_bucketHost.size()==1 && bucketSounds==0);
    mirrorGeneration=7;Pump();CHECK(S::g_bucketHost.empty() && bucketSounds==1 && session.sent.size()==2);
    for(const auto& sent:session.sent)CHECK(sent.slot!=1 && Decode<Launch>(sent).op==1);
    auto canonical=Decode<Launch>(session.sent.front());S::ReceiveLaunch(request,1);Pump();CHECK(bucketSounds==1);
    S::g_bucketHost.clear();S::ReceiveLaunch(request,1);Pump();CHECK(bucketSounds==1); // history survives queue retirement
    Start(session,Role::Client);S::ReceiveLaunch(canonical,1);CHECK(S::g_cues.empty());S::ReceiveLaunch(canonical,0);Pump();CHECK(bucketSounds==1 && surfaces==0);
    S::ReceiveLaunch(canonical,0);Pump();CHECK(bucketSounds==1);
    // Native throwing emits one wade and one splash cue for the whole multi-projectile throw.
    Start(session,Role::Client);nativeCue.kind=1;CHECK(S::BeforeBucketSound2D(nullptr,&ui,params)==V::Run && S::g_bucketLocal.empty());
    active={{&actor,&ui}};S::BeforeBucketSound2D(nullptr,&ui,params);nativeCue.kind=2;S::BeforeBucketSound2D(nullptr,&ui,params);
    CHECK(S::g_bucketLocal.size()==2);Pump();CHECK(session.sent.size()==2);
    auto first=Decode<Launch>(session.sent[0]),second=Decode<Launch>(session.sent[1]);
    Start(session,Role::Host);S::ReceiveLaunch(first,1);S::ReceiveLaunch(second,1);Pump();
    CHECK(bucketKinds==std::vector<uint8_t>({1,2}) && session.sent.size()==4);
    // Native host audio already plays locally; only other players get the canonical copy.
    Start(session,Role::Host);S::BeforeBucketSoundAt(nullptr,&ui,params);Pump();CHECK(bucketSounds==0 && session.sent.size()==3);
    // A hand announcement can arrive later; physical holds use their separate key/generation.
    Start(session,Role::Client);generation=0;S::BeforeBucketSoundAt(nullptr,&ui,params);Pump();CHECK(session.sent.empty() && S::g_bucketLocal.size()==1);
    generation=7;Pump();CHECK(session.sent.size()==1);
    Start(session,Role::Client);hand=nullptr;S::BeforeBucketSoundAt(nullptr,&ui,params);Pump();
    auto worldRequest=Decode<Launch>(session.sent.front());CHECK(worldRequest.source.len && worldRequest.generation==19);
    Start(session,Role::Host);S::ReceiveLaunch(worldRequest,1);Pump();CHECK(bucketSounds==1);
    // Class, source/point reach, occupant, generation, rate and expiry validation.
    Start(session,Role::Host);auto p=SoundPacket();bucket=false;S::ReceiveLaunch(p,1);Pump();CHECK(bucketSounds==0);
    Start(session,Role::Host);p=SoundPacket();coop::element::reach=false;S::ReceiveLaunch(p,1);Pump();CHECK(bucketSounds==0);
    Start(session,Role::Host);p=SoundPacket();p.x=1000;S::ReceiveLaunch(p,1);Pump();CHECK(bucketSounds==0);
    Start(session,Role::Host);p=SoundPacket();p.playerNo++;S::ReceiveLaunch(p,1);CHECK(S::g_bucketHost.empty());
    Start(session,Role::Host);p=SoundPacket();mirror=nullptr;S::ReceiveLaunch(p,1);now+=1501;Pump();CHECK(S::g_bucketHost.empty() && bucketSounds==0);
    Start(session,Role::Client);S::BeforeBucketSoundAt(nullptr,&ui,params);hand=nullptr;owned=false;Pump();CHECK(S::g_bucketLocal.empty() && session.sent.empty());
    Start(session,Role::Client);onBucketRead=[]{S::OnDisconnect();};S::BeforeBucketSoundAt(nullptr,&ui,params);CHECK(S::g_bucketLocal.empty());
    Start(session,Role::Client);coopDispatch=true;S::BeforeBucketSoundAt(nullptr,&ui,params);CHECK(S::g_bucketLocal.empty());
    // One unavailable first page cannot starve another peer's ready sound.
    Start(session,Role::Host);blockedMirrors=2;for(uint32_t i=1;i<=16;++i)S::ReceiveLaunch(SoundPacket(i),1);
    S::ReceiveLaunch(SoundPacket(1,0,2),2);Pump();Pump();Pump();CHECK(bucketSounds==1);
    // Bucket and impact audio share the recipient FIFO, despite using different wire kinds.
    Start(session,Role::Host);S::CanonicalBucketSound(SoundPacket(),0,false);ImpactEvent();
    session.allowSend=[](int slot,auto kind,const void*,int){return !(slot==2 && kind==coop::net::ReliableKind::WaterLaunch);};
    Pump();for(const auto& sent:session.sent)CHECK(sent.slot!=2);session.allowSend={};Pump();
    std::vector<coop::net::ReliableKind> order;for(const auto& sent:session.sent)if(sent.slot==2)order.push_back(sent.kind);
    CHECK(order==std::vector<coop::net::ReliableKind>({coop::net::ReliableKind::WaterLaunch,coop::net::ReliableKind::WaterSplash}));
    // Local readiness also preserves order across kinds on a receiving client.
    Start(session,Role::Client);auto launch=SoundPacket();launch.op=1;S::ReceiveLaunch(launch,0);
    Splash impactCue{};impactCue.sequence=2;impactCue.x=100;impactCue.y=200;impactCue.z=300;
    impactCue.nz=1;impactCue.ax=110;S::ReceiveSplash(impactCue,0);
    onBucketSound=[]{soundReady=false;};onSound=[]{soundReady=true;};Pump();
    CHECK(bucketSounds==0 && sounds==0 && session.sent.empty() && surfaces==1);
    soundReady=true;Pump();CHECK(bucketSounds==1 && sounds==1 && S::g_cues.empty());
    // Per-peer epochs also cover same-person departure while a cue is detached for send.
    Start(session,Role::Host);S::CanonicalBucketSound(SoundPacket(),0,false);bool replaced=false;
    session.allowSend=[&](int slot,auto,const void*,int){if(slot==2 && !replaced){replaced=true;S::OnPeerLeft(2);S::CanonicalBucketSound(SoundPacket(2),0,false);return false;}return true;};
    Pump();session.allowSend={};Pump();unsigned count=0;for(const auto& sent:session.sent)if(sent.slot==2)++count;CHECK(count==1);
    puts("PASS bucket source audio: native pre-hook admission, whole-throw cue pair, owner exclusion, source/hold/readiness validation, mixed impact FIFO, retry/expiry and same-person recipient epochs");
}
