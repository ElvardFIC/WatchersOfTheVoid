#include "../../src/votv-coop/src/coop/creatures/piramid_sync.cpp"
#include <cassert>
#include <cstdio>
#include <cwchar>
#include <functional>
#include <limits>
#include <stdexcept>

namespace test {
namespace C=coop::piramid_sync;namespace N=coop::net;namespace EL=coop::element;
struct Object {
    void* target=nullptr;void* movement=nullptr;void* arrow=nullptr;
    ue_wrap::FVector location{},look{};ue_wrap::FRotator rotation{};
    uint8_t walking=0,gathering=0,gathered=0;
} objects[32];
int serials[32];bool alive[32];uint32_t world=1;
N::Session session;EL::WorldActor pyramid,otherPyramid;EL::Npc wisp,otherWisp;
bool gameThread=true,collect=true,callOk=true,dispatch=true,tickOk=true;
int replays=0,queries=0,ticks=0,rotations=0;
void* tickActor=nullptr;
std::function<void(void*)> onLocation,onTick,onRotation;
std::function<void()> onCall;
void Reset(bool client=true) {
    C::OnDisconnect();session=N::Session{};session.mode=client?N::Role::Client:N::Role::Host;
    for(int i=0;i<32;++i){objects[i]={};serials[i]=1;alive[i]=true;}
    world=1;gameThread=collect=callOk=dispatch=tickOk=true;replays=queries=ticks=rotations=0;tickActor=nullptr;
    onLocation={};onTick={};onRotation={};onCall={};
    pyramid={};pyramid.id=1;pyramid.actor=&objects[0];pyramid.name="piramid2_C";pyramid.mirror=client;
    otherPyramid={};otherPyramid.id=3;otherPyramid.actor=&objects[2];otherPyramid.name="piramid2_C";otherPyramid.mirror=client;
    wisp={};wisp.id=2;wisp.actor=&objects[1];wisp.name="killerwisp_C";wisp.mirror=client;
    otherWisp={};otherWisp.id=4;otherWisp.actor=&objects[3];otherWisp.name="killerwisp_C";otherWisp.mirror=client;
    EL::WaMirrors().entries={&pyramid,&otherPyramid};EL::NpcMirrors().entries={&wisp,&otherWisp};
    C::Install(&session);C::g_armed=true;C::g_armedAtomic.store(true);
    C::g_fnSeeWisps=&objects[8];C::g_fnCheckIfReached=&objects[9];C::g_fnRandLoc=&objects[10];C::g_fnChangeLook=&objects[11];
    C::g_offWispTarget=offsetof(Object,target);C::g_isWalkingOff=offsetof(Object,walking);C::g_isWalkingMask=1;
    C::g_gatheringOff=offsetof(Object,gathering);C::g_gatheringMask=2;
    C::g_offMovementVector=offsetof(Object,movement);C::g_offArrow=offsetof(Object,arrow);C::g_offRelLook=offsetof(Object,look);
    objects[0].movement=&objects[12];objects[0].arrow=&objects[13];
    C::t_allowCheck={};
}
void Receive(uint32_t p=1,uint32_t w=2){N::PyramidGatherPayload payload{};payload.pyramidEid=p;payload.wispEid=w;C::OnPyramidGather(payload);}
template<class F>void Case(const char* id,F fn){fn();printf("PASS %s\n",id);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<32;++i)if(p==&test::objects[i])return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<32&&p==&test::objects[i]&&test::alive[i];}
bool IsLive(void* p){return IsLiveByIndex(p,InternalIndexOf(p));}
int32_t SlotSerial(int32_t i){return i>=0&&i<32?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p==&test::objects[1]||p==&test::objects[3]?&test::objects[5]:&test::objects[4];}
int32_t FindPropertyOffset(void*,const wchar_t* name){
    if(!wcscmp(name,L"wispTarget"))return offsetof(test::Object,target);
    if(!wcscmp(name,L"movementVector"))return offsetof(test::Object,movement);
    if(!wcscmp(name,L"Arrow"))return offsetof(test::Object,arrow);return offsetof(test::Object,look);
}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask){
    if(!wcscmp(name,L"isWalking")){off=offsetof(test::Object,walking);mask=1;}
    else if(!wcscmp(name,L"gathering")){off=offsetof(test::Object,gathering);mask=2;}
    else {assert(!wcscmp(name,L"gathered"));off=offsetof(test::Object,gathered);mask=4;}return true;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return &test::world;}void* WorldOf(void*){return &test::world;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return &test::objects[4];}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
 :ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
void Function::NoteUse(Use,bool)const{}
Resolved ResolveOn(const Function& row,void*){
    int i=!wcscmp(row.Name(),L"seeWisps")?8:!wcscmp(row.Name(),L"checkIfReached")?9:!wcscmp(row.Name(),L"randLoc")?10:11;
    return Resolved(&test::objects[i],&row);
}
}
namespace ue_wrap::game_thread {
bool IsGameThread(){return test::gameThread;}
bool RegisterInterceptor(const facts::Resolved&,UFunctionInterceptor){return true;}
bool RegisterPostObserver(const facts::Resolved&,ProcessEventObserverFn){return true;}
void UnregisterInterceptor(void*,UFunctionInterceptor){}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void* actor,FVector& out){
    assert(reflection::IsLive(actor));++test::queries;out=static_cast<test::Object*>(actor)->location;
    if(test::onLocation)test::onLocation(actor);return true;
}
bool SetActorTickEnabled(void* actor,bool){assert(reflection::IsLive(actor));++test::ticks;test::tickActor=actor;if(test::onTick)test::onTick(actor);return test::tickOk;}
FRotator GetComponentWorldRotation(void* actor){assert(reflection::IsLive(actor));if(test::onRotation)test::onRotation(actor);return static_cast<test::Object*>(actor)->rotation;}
bool SetComponentWorldRotation(void* actor,const FRotator& rotation){
    assert(reflection::IsLive(actor));++test::rotations;static_cast<test::Object*>(actor)->rotation=rotation;if(test::onRotation)test::onRotation(actor);return true;
}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(1){}
bool Call(void* actor,ParamFrame& frame){
    using namespace test;assert(frame.function()==&objects[9]);
    if(!dispatch||C::CheckSuppress_Interceptor(actor,frame.data()))return false;
    ++replays;
    if(collect){auto* p=static_cast<Object*>(actor);p->gathering|=2;p->walking=0;
        auto* w=static_cast<Object*>(p->target);if(w)w->gathered|=4;}
    if(onCall)onCall();return callOk;
}
}
int main(){using namespace test;
    Case("P01 session/world cancellation",[]{Reset();Receive();++world;C::Tick();assert(!C::g_pending.active&&replays==0);});
    Case("P02 receiver role/thread",[]{Reset(false);Receive();assert(!C::g_pending.active);Reset();gameThread=false;Receive();assert(!C::g_pending.active);gameThread=true;session.active=false;Receive();assert(!C::g_pending.active);});
    Case("P03 exact pyramid mirror",[]{for(int mode=0;mode<2;++mode){Reset();if(mode==0)pyramid.name="other_C";else pyramid.mirror=false;Receive();C::TryReplayPendingGather();assert(replays==0);}Reset();Receive();C::TryReplayPendingGather();assert(replays==1&&ticks==1);});
    Case("P04 canonical host wisp",[]{Reset(false);objects[0].target=&objects[1];objects[0].gathering=2;wisp.mirror=true;C::CheckIfReached_POST(&objects[0],nullptr,nullptr);assert(session.sends.empty());C::g_lastGathering.clear();wisp.mirror=false;wisp.name="wisp_C";C::CheckIfReached_POST(&objects[0],nullptr,nullptr);assert(session.sends.empty());wisp.name="killerwisp_C";wisp.actor=nullptr;assert(!C::FindNpcEidForActor(nullptr));});
    Case("P05 host edge serial",[]{Reset(false);objects[0].target=&objects[1];objects[0].gathering=2;C::CheckIfReached_POST(&objects[0],nullptr,nullptr);assert(session.sends.size()==1);++serials[0];C::CheckIfReached_POST(&objects[0],nullptr,nullptr);assert(session.sends.size()==2);});
    Case("P06 send teardown accounting",[]{Reset(false);objects[0].target=&objects[1];objects[0].gathering=2;session.sendHook=[] {C::OnDisconnect();};C::CheckIfReached_POST(&objects[0],nullptr,nullptr);assert(C::DebugHostRelayCount()==0);});
    Case("P07 pending binding",[]{for(int mode=0;mode<4;++mode){Reset();Receive();if(mode==0)++serials[0];if(mode==1)++serials[1];if(mode==2)pyramid.actor=&objects[2];if(mode==3)wisp.actor=&objects[3];C::TryReplayPendingGather();assert(!C::g_pending.active&&replays==0);}});
    Case("P08 duplicate deadline",[]{Reset();Receive();auto deadline=C::g_pending.deadlineMs;auto revision=C::g_pending.revision;Receive();assert(C::g_pending.deadlineMs==deadline&&C::g_pending.revision==revision);C::TryReplayPendingGather();Receive();C::TryReplayPendingGather();assert(replays==1);++serials[0];++serials[1];objects[0].gathering=0;Receive();C::TryReplayPendingGather();assert(replays==2);});
    Case("P09 finite gather geometry",[]{for(float value:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){Reset();objects[0].location.X=value;Receive();C::TryReplayPendingGather();assert(replays==0);}Reset();objects[1].location.X=10000.25f;Receive();C::TryReplayPendingGather();assert(!replays&&C::g_pending.active);objects[1].location.X=10000.f;C::TryReplayPendingGather();assert(replays==1);});
    Case("P10 location callback receipts",[]{for(int mode=0;mode<4;++mode){Reset();Receive();onLocation=[mode](void* actor){if(mode==0&&actor==&objects[0])++serials[0];if(mode==1&&actor==&objects[1])++serials[1];if(mode==2)++world;if(mode==3)Receive(3,4);};C::TryReplayPendingGather();assert(replays==0);}});
    Case("P11 admitted call once",[]{Reset();Receive();callOk=false;collect=false;C::TryReplayPendingGather();C::TryReplayPendingGather();assert(replays==1&&!C::g_pending.active);Reset();Receive();dispatch=false;C::TryReplayPendingGather();assert(C::g_pending.active);dispatch=true;C::TryReplayPendingGather();assert(replays==1);});
    Case("P12 owned staging cleanup",[]{Reset();Receive();collect=false;C::TryReplayPendingGather();assert(objects[0].target==nullptr&&!objects[0].walking);Reset();Receive();collect=false;onCall=[] {++serials[0];objects[0].target=&objects[3];};C::TryReplayPendingGather();assert(objects[0].target==&objects[3]);});
    Case("P13 gathered wisp tick binding",[]{for(int mode=0;mode<3;++mode){Reset();Receive();onCall=[mode] {if(mode==0)++serials[1];if(mode==1)objects[1].gathered=0;if(mode==2)++world;};C::TryReplayPendingGather();assert(ticks==0);}Reset();Receive();tickOk=false;C::TryReplayPendingGather();assert(C::g_wispTick.active&&replays==1);tickOk=true;C::TryReplayPendingGather();assert(!C::g_wispTick.active&&replays==1&&ticks==2);});
    Case("P14 recursive ticks",[]{Reset();Receive();onCall=[] {C::Tick();};C::g_lastProbeMs=C::NowMs();C::Tick();assert(replays==1);});
    Case("P15 exact replay pass",[]{Reset();Receive();onCall=[] {
        assert(C::BrainSuppress_Interceptor(&objects[0],nullptr));assert(C::CheckSuppress_Interceptor(&objects[2],nullptr));
        assert(C::CheckSuppress_Interceptor(&objects[0],C::t_allowCheck.params));
    };C::TryReplayPendingGather();assert(replays==1);});
    Case("P16 allowance RAII",[]{Reset();Receive();collect=false;onCall=[] {throw std::runtime_error("native fault");};try{C::TryReplayPendingGather();assert(false);}catch(const std::runtime_error&){}
        assert(!C::t_allowCheck.actor.Raw()&&!C::g_pending.active&&!objects[0].walking&&!objects[0].target);});
    Case("P17 restored tick incarnation",[]{Reset();C::RestoreNewClientMirrors();int count=ticks;++serials[0];C::RestoreNewClientMirrors();assert(ticks==count+1);Reset();tickOk=false;C::RestoreNewClientMirrors();assert(C::g_tickRestored.empty());tickOk=true;C::RestoreNewClientMirrors();assert(C::g_tickRestored.size()==2);Reset();onTick=[](void*) {C::OnDisconnect();};C::RestoreNewClientMirrors();assert(C::g_tickRestored.empty());});
    Case("P18 heading component lifetime",[]{Reset();alive[12]=alive[13]=false;float yaw=0;assert(!C::ReadHostHeadingYaw(&objects[0],yaw));C::ApplyMirrorHeadingYaw(&objects[0],45);assert(rotations==0);});
    Case("P19 finite heading",[]{Reset();float yaw=0;objects[12].rotation.Yaw=std::numeric_limits<float>::infinity();assert(!C::ReadHostHeadingYaw(&objects[0],yaw));C::ApplyMirrorHeadingYaw(&objects[0],std::numeric_limits<float>::quiet_NaN());assert(rotations==0);});
    Case("P20 heading callback ownership",[]{for(int mode=0;mode<4;++mode){Reset();onRotation=[mode](void*) {if(mode==0)++serials[0];if(mode==1)++world;if(mode==2)objects[0].movement=&objects[14];if(mode==3)C::OnDisconnect();};C::ApplyMirrorHeadingYaw(&objects[0],45);assert(rotations==1);}});
    Case("P21 finite head look",[]{Reset();float x=0,y=0,z=0;objects[0].look.X=std::numeric_limits<float>::quiet_NaN();assert(!C::ReadHostRelLook(&objects[0],x,y,z));objects[0].look={1,2,3};C::ApplyMirrorRelLook(&objects[0],std::numeric_limits<float>::infinity(),4,5);assert(objects[0].look.X==1);});
    Case("P22 target cache incarnation",[]{Reset(false);objects[0].target=&objects[1];assert(C::ReadHostWispTargetEid(&objects[0])==2);++serials[1];wisp.id=4;assert(C::ReadHostWispTargetEid(&objects[0])==4);wisp.actor=nullptr;assert(C::ReadHostWispTargetEid(&objects[0])==0);});
    Case("P23 join occupancy admission",[]{Reset(false);objects[0].target=&objects[1];objects[0].gathering=2;for(int slot:{-1,0,32})C::QueueConnectBroadcastForSlot(slot);assert(session.sends.empty());session.generations[31]=0;C::QueueConnectBroadcastForSlot(31);assert(session.sends.empty());session.generations[31]=1;C::QueueConnectBroadcastForSlot(31);assert(session.sends.size()==1);});
    Case("P24 join callback snapshots",[]{for(int mode=0;mode<3;++mode){Reset(false);objects[0].target=&objects[1];objects[0].gathering=2;objects[2].target=&objects[3];objects[2].gathering=2;
        session.sendHook=[mode] {if(mode==0)++session.generations[1];if(mode==1)C::OnDisconnect();if(mode==2)EL::WaMirrors().entries.clear();};C::QueueConnectBroadcastForSlot(1);assert(session.sends.size()==1);}});
    puts("PASS 24 Great Pyramid adapter boundaries with native/transport failure injection");
}
