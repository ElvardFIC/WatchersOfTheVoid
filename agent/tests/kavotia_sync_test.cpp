#include "../../src/votv-coop/src/coop/creatures/kavotia_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include "../../src/votv-coop/src/coop/net/client_authorship.h"
#include <cassert>
#include <cstdio>
#include <cwchar>
#include <limits>
#include <set>

namespace test {
struct Audio {float pitch = 1; uint8_t bits = 0x80;} audio, audio2;
struct Effect {uint8_t bits = 0x80;} windup, windup2;
struct Movement {float speed = 200;} movement;
int mesh = 0, mesh2 = 0, types[7]{};
struct Actor {void* mesh; void* audio; void* effect; void* movement; void* animation=nullptr;} actor{&mesh,&audio,&windup,&movement}, replacement{&mesh2,&audio2,&windup2,&movement};
struct Anim {void* actor; void* mesh; uint8_t bits = 0x80; ue_wrap::FVector target{10,20,30};} anim{&actor,&mesh}, anim2{&replacement,&mesh2};
struct Slot {void* object; int type; int serial = 1; bool live = true;};
std::vector<Slot> slots = {{&actor,0},{&replacement,0},{&anim,1},{&anim2,1},{&audio,2},{&audio2,2},
    {&windup,3},{&windup2,3},{&movement,4},{&mesh,5},{&mesh2,5}};
uint32_t world = 1;
bool watchOk = true, metadata = true, paramOk = true, callOk = true;
const wchar_t* missing = nullptr;
int watches = 0, plays = 0, stops = 0, activates = 0, deactivates = 0, pitches = 0;
int frameSize = 4, entryOffset = 0;
void (*callHook)() = nullptr;
void (*paramHook)() = nullptr;
struct Watch {std::wstring name; int tag; ue_wrap::script_gate::PreFn pre;};
std::vector<Watch> authority;
ue_wrap::script_gate::PostFn after = nullptr;
ue_wrap::script_gate::PostFn actorTick = nullptr;
bool Miss(const wchar_t* name) {return !metadata || (missing && !wcscmp(missing,name));}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return &test::world;}
void* WorldOf(void*){return &test::world;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<std::size(test::slots);++i)if(test::slots[i].object==p)return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<std::size(test::slots) && test::slots[i].object==p && test::slots[i].live;}
bool IsLive(void* p){return IsLiveByIndex(p,InternalIndexOf(p));}
int32_t SlotSerial(int32_t i){return i>=0 && i<std::size(test::slots)?test::slots[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){const auto i=InternalIndexOf(p);return i>=0?&test::types[test::slots[i].type]:nullptr;}
void* SuperStructOf(void*){return nullptr;}
const FName& NameOf(void* cls){static FName n{};for(int i=0;i<7;++i)if(cls==&test::types[i])n.ComparisonIndex=i;return n;}
bool NameEquals(const FName& n,const wchar_t* name){return
    (n.ComparisonIndex==0 && !wcscmp(name,L"kavotia_C")) ||
    (n.ComparisonIndex==1 && !wcscmp(name,L"kavotia_Skeleton_AnimBlueprint_C"));}
int32_t FindPropertyOffset(void* cls,const wchar_t* name){
    if(test::Miss(name))return -1;
    if(cls==&test::types[0]) {
        if(!wcscmp(name,L"Mesh"))return offsetof(test::Actor,mesh);
        if(!wcscmp(name,L"charging"))return offsetof(test::Actor,audio);
        if(!wcscmp(name,L"eff_tentacleBallShootWindup"))return offsetof(test::Actor,effect);
        if(!wcscmp(name,L"CharacterMovement"))return offsetof(test::Actor,movement);
        if(!wcscmp(name,L"anim"))return offsetof(test::Actor,animation);
    }
    if(cls==&test::types[1]) {
        if(!wcscmp(name,L"kavotia"))return offsetof(test::Anim,actor);
        if(!wcscmp(name,L"mesh"))return offsetof(test::Anim,mesh);
        if(!wcscmp(name,L"shootTarget"))return offsetof(test::Anim,target);
    }
    if(cls==&test::types[2] && !wcscmp(name,L"PitchMultiplier"))return offsetof(test::Audio,pitch);
    if(cls==&test::types[4] && !wcscmp(name,L"MaxWalkSpeed"))return offsetof(test::Movement,speed);
    return -1;
}
bool FindBoolProperty(void* cls,const wchar_t* name,int32_t& off,uint8_t& mask){
    if(test::Miss(name))return false;
    if(cls==&test::types[1] && (!wcscmp(name,L"isLaying") || !wcscmp(name,L"isShooting"))) {
        off=offsetof(test::Anim,bits);mask=!wcscmp(name,L"isLaying")?1:2;return true;
    }
    if(!wcscmp(name,L"bIsActive") && (cls==&test::types[2] || cls==&test::types[3])) {
        off=cls==&test::types[2]?offsetof(test::Audio,bits):offsetof(test::Effect,bits);mask=0x20;return true;
    }
    return false;
}
int32_t FindParamOffset(void*,const wchar_t* name){assert(!wcscmp(name,L"EntryPoint"));return test::entryOffset;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return test::Miss(name)?nullptr:const_cast<wchar_t*>(name);}
}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* cls,const wchar_t* name,int tag,PreFn pre,PostFn post){
    ++test::watches;
    if(pre){assert(!wcscmp(cls,L"kavotia_C") && !post);if(test::watchOk)test::authority.push_back({name,tag,pre});}
    else if(!wcscmp(cls,L"kavotia_C")){assert(!wcscmp(name,L"ReceiveTick"));test::actorTick=post;}
    else {assert(!wcscmp(cls,L"kavotia_Skeleton_AnimBlueprint_C") && !wcscmp(name,L"BlueprintUpdateAnimation"));test::after=post;}
    return test::watchOk;
}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(16){}
bool ParamFrame::SetRaw(const wchar_t*,const void* value,int32_t size){
    assert(size<=16);std::memcpy(buf_.data(),value,size);
    if(test::paramHook)test::paramHook();
    return test::paramOk;
}
bool Call(void* object,ParamFrame& frame){
    if(!test::callOk)return false;
    const auto* name=static_cast<const wchar_t*>(frame.function());
    if(!wcscmp(name,L"SetPitchMultiplier")){++test::pitches;static_cast<test::Audio*>(object)->pitch=*static_cast<float*>(frame.data());}
    else if(!wcscmp(name,L"Play")){++test::plays;static_cast<test::Audio*>(object)->bits|=0x20;}
    else if(!wcscmp(name,L"Stop")){++test::stops;static_cast<test::Audio*>(object)->bits&=~0x20;}
    else if(!wcscmp(name,L"Activate")){++test::activates;static_cast<test::Effect*>(object)->bits|=0x20;}
    else if(!wcscmp(name,L"Deactivate")){++test::deactivates;static_cast<test::Effect*>(object)->bits&=~0x20;}
    else assert(false);
    if(test::callHook)test::callHook();
    return true;
}
}

int main(){
    using namespace test;namespace K=coop::kavotia_sync;namespace N=coop::net;
    auto& registry=coop::element::Registry::Get();
    N::Session session;registry.id=42;registry.row.actor=&actor;registry.row.mirror=false;
    auto update=[&](Anim& animation=anim){ue_wrap::script_gate::Call call{};call.object=&animation;after(call);};
    auto reset=[&](N::Role role){
        K::OnDisconnect();session=N::Session{};session.mode=role;
        registry.id=42;registry.present=true;registry.row={&actor,role==N::Role::Client,true};registry.extra.clear();
        anim={&actor,&mesh};actor={&mesh,&audio,&windup,&movement,&anim};audio={};windup={};movement={};
        for(auto& slot:slots)slot.live=true;
        metadata=paramOk=callOk=true;missing=nullptr;callHook=paramHook=nullptr;
        plays=stops=activates=deactivates=pitches=0;
        K::Install(&session);
    };
    assert(N::AuthorshipOf(N::ReliableKind::KavotiaState)==N::ClientAuthorship::Host);
    assert(N::LaneForKind(N::ReliableKind::KavotiaState)==N::Lane::Normal);
    assert(!N::IsClientRelayableReliableKind(N::ReliableKind::KavotiaState));
    assert(!N::IsPreWorldSendableKind(N::ReliableKind::KavotiaState));
    watchOk=false;K::Install(&session);assert(watches==6);watchOk=true;K::Install(&session);K::Install(&session);assert(watches==12);
    assert(authority.size()==4);
    reset(N::Role::Client);
    int32_t entry=3961;ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&entry;call.locals=reinterpret_cast<uint8_t*>(&entry);
    for(const auto& w:authority){call.tag=w.tag;
        assert(w.pre(call)==(w.tag==0?ue_wrap::script_gate::Verdict::Run:ue_wrap::script_gate::Verdict::Cancel));
        for(auto blocked:{15,244,393,2018,4950,5286,5860,6787,7324,7733}){
            entry=blocked;assert(w.pre(call)==ue_wrap::script_gate::Verdict::Cancel);
        }
        entry=3961;
    }
    call.tag=0;entry=3961;frameSize=3;assert(authority[0].pre(call)==ue_wrap::script_gate::Verdict::Cancel);frameSize=4;
    entryOffset=-1;assert(authority[0].pre(call)==ue_wrap::script_gate::Verdict::Cancel);entryOffset=0;
    registry.present=false;entry=393;assert(authority[0].pre(call)==ue_wrap::script_gate::Verdict::Cancel);registry.present=true;
    session.linked=false;assert(authority[0].pre(call)==ue_wrap::script_gate::Verdict::Run);session.linked=true;
    session.mode=N::Role::Host;assert(authority[0].pre(call)==ue_wrap::script_gate::Verdict::Run);
    puts("PASS Kavotia authority: native setup retained, pre-bind clients and latent move/shot entries refused, malformed frames and offline/host fallthrough");

    reset(N::Role::Host);anim.bits=0x83;audio.bits=windup.bits=0xa0;audio.pitch=1.25f;
    ue_wrap::script_gate::Call hostTick{};hostTick.object=&actor;actorTick(hostTick);
    assert(K::g_host.size()==1);K::Tick();assert(session.kavotiaDelivered.size()==1);
    const auto initial=session.kavotiaDelivered.back().packet;
    assert(initial.flags==15 && initial.targetX==10 && initial.targetY==20 && initial.targetZ==30 && initial.chargePitch==1.25f);
    K::Tick();assert(session.kavotiaDelivered.size()==1);
    K::g_host[0].sent=K::Now()-501;K::g_host[0].next=0;K::Tick();assert(session.kavotiaDelivered.size()==2);
    K::QueueConnect(2);session.readyMask=6;K::Tick();assert(session.kavotiaDelivered.size()==4);
    session.failMask=4;K::g_host[0].next=0;anim.target.X=50;update();K::Tick();assert(K::g_out.size()==1);
    K::g_host[0].next=0;anim.target.X=60;update();K::Tick();assert(K::g_out.size()==1 && K::g_out[0].packet.targetX==60);
    session.failMask=0;K::Tick();assert(K::g_out.empty() && session.kavotiaDelivered.back().packet.targetX==60);
    K::g_sequence=0xffffffff;K::QueueConnect(2);K::Tick();assert(session.kavotiaDelivered.back().packet.sequence==1);
    K::g_host[0].sampled=K::Now()-1501;K::Tick();assert(K::g_host.empty());
    registry.row.mirror=true;update();assert(K::g_host.empty());registry.row.mirror=false;
    missing=L"isShooting";update();assert(K::g_host.empty());missing=nullptr;
    audio.pitch=std::numeric_limits<float>::quiet_NaN();update();assert(K::g_host.empty());audio.pitch=1;
    anim.target.X=N::kMaxCoord+1;update();assert(K::g_host.empty());anim.target.X=10;
    anim.mesh=&mesh2;update();assert(K::g_host.empty());anim.mesh=&mesh;update();assert(K::g_host.size()==1);
    ++slots[0].serial;K::Tick();assert(K::g_host.empty());
    puts("PASS Kavotia host: exact native flags/aim/pitch, throttle and stationary join heartbeat, independent retry with latest-state coalescing, wrap and live identity");

    reset(N::Role::Client);N::KavotiaStatePayload p{42,1,100,200,300,1.5f,15};
    K::Receive(p,1);assert(K::g_client.empty());
    for(float bad:{-.1f,2.1f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto invalid=p;invalid.chargePitch=bad;K::Receive(invalid,0);assert(K::g_client.empty());
    }
    for(auto field:{0,1,2}){auto invalid=p;if(field==0)invalid.targetX=std::numeric_limits<float>::quiet_NaN();
        if(field==1)invalid.targetY=N::kMaxCoord+1;if(field==2)invalid.targetZ=std::numeric_limits<float>::infinity();
        K::Receive(invalid,0);assert(K::g_client.empty());}
    auto invalid=p;invalid.flags=16;K::Receive(invalid,0);assert(K::g_client.empty());
    invalid=p;invalid.eid=0;K::Receive(invalid,0);invalid.eid=coop::element::kHostRangeSize;K::Receive(invalid,0);assert(K::g_client.empty());
    invalid=p;invalid.sequence=0;K::Receive(invalid,0);assert(K::g_client.empty());
    registry.present=false;K::Receive(p,0);K::Tick();assert(plays==0 && K::g_client.size()==1);
    registry.present=true;update();K::Tick();assert(anim.bits==0x83 && anim.target.X==100 && movement.speed==450);
    assert(plays==1 && activates==1 && pitches==1 && audio.pitch==1.5f);
    K::Tick();update();assert(plays==1 && activates==1 && pitches==1);
    K::Receive(p,0);assert(K::g_client[0].packet.sequence==1);
    ++p.sequence;p.chargePitch=1.75f;K::Receive(p,0);K::Tick();assert(plays==1 && activates==1 && pitches==2);
    ++p.sequence;p.flags=0;K::Receive(p,0);update();K::Tick();assert(stops==1 && deactivates==1 && anim.bits==0x80 && movement.speed==200);
    ++p.sequence;p.flags=15;K::Receive(p,0);K::Tick();assert(plays==2 && activates==2);
    K::g_client[0].received=K::Now()-1501;update();K::Tick();assert(stops==2 && deactivates==2 && K::g_client.empty() && anim.bits==0x80);
    K::g_seen=0xffffffff;p.sequence=1;K::Receive(p,0);assert(K::g_seen==1);
    puts("PASS Kavotia receiver: host-only finite/range validation, delayed binding, bit-preserving stance/aim, charge loop and windup edges, no duplicate restart, silent expiry and sequence wrap");

    reset(N::Role::Client);p={42,1,1,2,3,1.5f,15};K::Receive(p,0);
    missing=L"Play";K::Tick();assert(plays==0 && pitches==1);missing=nullptr;K::Tick();assert(plays==1 && pitches==1 && activates==1);
    ++p.sequence;p.flags=0;K::Receive(p,0);missing=L"Deactivate";K::Tick();assert(stops==1 && deactivates==0);missing=nullptr;
    K::Tick();assert(stops==1 && deactivates==1);
    ++p.sequence;p.flags=15;K::Receive(p,0);paramOk=false;K::Tick();assert(plays==1);paramOk=true;K::Tick();assert(plays==2);
    actor.audio=&audio2;actor.effect=&windup2;K::Tick();assert(plays==3 && activates==3);
    ++slots[0].serial;K::Tick();assert(K::g_client.empty());update();assert(anim.bits==0x80);
    puts("PASS Kavotia presentation recovery: missing metadata/failed parameter retry preserves completed operations, component replacement and recycled actor retirement");

    for(int mode=0;mode<6;++mode){
        reset(N::Role::Client);p={42,1,1,2,3,1.5f,15};K::Receive(p,0);
        if(mode==0)callHook=[]{K::OnDisconnect();};
        if(mode==1)callHook=[]{++world;};
        if(mode==2)callHook=[]{coop::element::Registry::Get().row.mirror=false;};
        if(mode==3)callHook=[]{++slots[0].serial;};
        if(mode==4)paramHook=[] {K::OnDisconnect();};
        if(mode==5)callHook=[] {K::Tick();};
        K::Tick();assert(mode==5?plays==1:plays==0);
    }
    reset(N::Role::Client);p={42,1,1,2,3,1.5f,15};K::Receive(p,0);
    callHook=[]{callHook=nullptr;K::Receive({42,2,20,30,40,.5f,0},0);};
    K::Tick();assert(K::g_client.size()==1 && K::g_client[0].packet.sequence==2);
    K::Tick();update();assert(stops>=1 && deactivates>=1 && anim.bits==0x80 && anim.target.X==20);
    reset(N::Role::Host);update();session.sendHook=[]{K::OnDisconnect();};K::Tick();assert(K::g_out.empty());
    reset(N::Role::Host);session.readyMask=6;session.failMask=4;update();K::Tick();assert(K::g_out.size()==1);
    K::OnPeerLeft(2);assert(K::g_out.empty());session.failMask=0;K::QueueConnect(2);K::Tick();assert(session.kavotiaDelivered.size()==3);
    puts("PASS Kavotia lifecycle: reentrant reflection/receive/Tick, disconnect/world/authority changes, send teardown and recycled peer slots");

    reset(N::Role::Host);session.readyMask=6;session.failMask=4;
    Actor pack[40]{};
    for(uint32_t i=0;i<40;++i){
        slots.push_back({&pack[i],0});registry.extra.push_back({100+i,{&pack[i],false,true}});
        K::Host row{};row.packet={100+i,0,1,2,3,1,0};row.sampled=K::Now();row.actor.Set(&pack[i]);K::g_host.push_back(row);
    }
    for(int i=0;i<10;++i){const auto before=session.kavotiaDelivered.size();K::Tick();assert(session.kavotiaDelivered.size()-before<=4 && K::g_out.size()<=32);}
    assert(K::g_out.size()==32 && session.kavotiaDelivered.size()==40);
    session.failMask=0;K::Tick();assert(K::g_out.empty());
    K::QueueConnect(2);for(int i=0;i<10;++i)K::Tick();
    std::set<uint32_t> joined;
    for(const auto& sent:session.kavotiaDelivered)if(sent.slot==2)joined.insert(sent.packet.eid);
    assert(joined.size()==40 && K::g_out.empty());
    for(auto& row:K::g_host)row.sampled=K::Now()-1501;
    K::Tick();assert(K::g_host.empty());
    slots.resize(11);registry.extra.clear();

    reset(N::Role::Client);registry.present=false;
    for(uint32_t i=1;i<=128;++i)K::Receive({100+i,i,0,0,0,1,0},0);
    assert(K::g_client.size()==128 && K::g_seen==128);
    K::Receive({300,129,0,0,0,1,0},0);assert(K::g_client.size()==128 && K::g_seen==128);
    for(auto& row:K::g_client)row.received=K::Now()-3001;
    K::Tick();assert(K::g_client.empty());
    K::Receive({300,129,0,0,0,1,0},0);assert(K::g_client.size()==1 && K::g_seen==129);
    puts("PASS Kavotia capacity: four publications/tick, 32 pending per peer, fast-peer progress, full-pack join repair and bounded stale client reclamation");

    reset(N::Role::Client);p={42,1,1,2,3,1.5f,15};K::Receive(p,0);K::Tick();assert(plays==1);
    actor.effect=nullptr;missing=L"SetPitchMultiplier";K::g_client[0].received=K::Now()-1501;
    K::Tick();assert(stops==1 && K::g_client.size()==1);
    actor.effect=&windup;missing=nullptr;K::Tick();assert(K::g_client.empty() && deactivates==1);
    puts("PASS Kavotia independent cleanup: missing particle/pitch metadata cannot strand the stale charge loop");
}
