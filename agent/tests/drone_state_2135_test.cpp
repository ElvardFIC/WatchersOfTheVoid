#include "support_2135.h"
#include "../../src/votv-coop/src/coop/interactables/drone_sync.cpp"
#include <limits>
namespace S=coop::drone_sync;
namespace test {inline bool leave=false,found=true,fieldReady=true;inline int writes=0,suppress=0;inline float pitch=1.f,appliedPitch=0.f;}
namespace ue_wrap::drone {
bool EnsureResolved(){return true;}void* Find(){return test::found?&test::actor:nullptr;}
bool IsActive(void*){return false;}bool ReadPose(void*,FVector& p,FRotator& r){p={};r={};return true;}
bool DriveMirror(void*,const FVector&,const FRotator&){return true;}
void SuppressTick(void*){++test::suppress;}void RestoreTick(void*){}
uint8_t ReadFxBits(void*){return 0;}bool ReadDustAnchor(void*,FVector&){return false;}
void ApplyDustMirror(void*,bool,const FVector&){}void PlayArrivalCue(void*){}void SetSignalLight(void*,bool){}
float ReadRotorPitch(void*){return test::pitch;}void ApplyRotorPitch(void*,float value){test::appliedPitch=value;}
void WriteGateFields(void*,bool,bool){}void RepointContainer(void*){}
bool ReadDeparturePreference(void*,bool& v){if(!test::fieldReady)return false;v=test::leave;return true;}
bool WriteDeparturePreference(void*,bool v){if(!test::fieldReady)return false;test::leave=v;++test::writes;return true;}
}
coop::net::Session session;
void Reset(){S::OnDisconnect();session={};test::leave=false;test::found=test::fieldReady=true;test::writes=0;test::pitch=1.f;test::appliedPitch=0.f;S::Install(&session);}
coop::net::DroneStatePayload Sent(){coop::net::DroneStatePayload p{};std::memcpy(&p,session.sent.back().data.data(),sizeof(p));return p;}
void Tick(){S::g_nextCheckMs=0;S::Tick();}
int main(){using namespace test;using namespace ue_wrap::drone;
 Reset();leave=true;Tick();CHECK((Sent().stateBits&(kLeaveAfter5min|kDepartureValid))==(kLeaveAfter5min|kDepartureValid));Tick();CHECK(session.sent.size()==1);
 leave=false;Tick();CHECK((Sent().stateBits&kLeaveAfter5min)==0);CHECK(session.sent.size()==2);
 S::g_lastSendMs=S::NowMs()-2100;Tick();CHECK(session.sent.size()==3); // idle seed recovery
 Reset();session.sendOk=false;Tick();CHECK(!S::g_haveSent);session.sendOk=true;Tick();CHECK(S::g_haveSent);
 Reset();fieldReady=false;Tick();CHECK((Sent().stateBits&kDepartureValid)==0);fieldReady=true;Tick();CHECK(Sent().stateBits&kDepartureValid);
 Reset();leave=true;S::QueueConnectBroadcastForSlot(1);CHECK(Sent().adopt==1&&(Sent().stateBits&kLeaveAfter5min));
 auto p=Sent();Reset();session.mode=coop::net::Role::Client;found=false;S::OnReliable(p);CHECK(writes==0);found=true;Tick();CHECK(leave&&writes==1);
 p.stateBits=kDepartureValid;S::OnReliable(p);CHECK(!leave);p.stateBits=kLeaveAfter5min;S::OnReliable(p);CHECK(!leave); // valid-bit guards missing reflection
 p.stateBits=kDepartureValid|kLeaveAfter5min;p.x=std::numeric_limits<float>::quiet_NaN();S::OnReliable(p);CHECK(!leave);
 S::OnDisconnect();p.x=0;S::OnReliable(p);CHECK(!leave);
 Reset();Tick();CHECK(Sent().rotorPitch==1.f);pitch=1.8f;Tick();CHECK(session.sent.size()==2&&Sent().rotorPitch==1.8f);
 p=Sent();Reset();session.mode=coop::net::Role::Client;found=false;S::OnReliable(p);CHECK(appliedPitch==0.f);
 found=true;Tick();CHECK(appliedPitch==1.8f);
 p.rotorPitch=std::numeric_limits<float>::quiet_NaN();S::OnReliable(p);CHECK(appliedPitch==1.8f);
 p.rotorPitch=5.f;S::OnReliable(p);CHECK(appliedPitch==1.8f);
 puts("PASS actual drone snapshots: rotor pitch, invalid packets, preferences, failed sends, idle recovery, late actor and disconnect");
}
