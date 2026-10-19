#include <algorithm>
#include <cstdint>
#define private public
#include "coop/element/npc.h"
#undef private
#include "../../src/votv-coop/src/coop/creatures/npc_pose_drive.cpp"
#include <cassert>
#include <cstdio>
namespace test {int actor=1;ue_wrap::FRotator rotation{};ue_wrap::FVector position{};}
namespace ue_wrap::reflection {bool IsLiveByIndex(void* p,int32_t){return p==&test::actor;}}
namespace ue_wrap::engine {
bool SetActorLocation(void*,const FVector& p){test::position=p;return true;}
bool SetActorRotation(void*,const FRotator& r){test::rotation=r;return true;}
}
namespace ue_wrap::puppet {
void DriveCharacterMovement(void*,const FVector&,bool){}
void DriveKerfurLookAt(void*,const FVector&){}
void DriveKerfurBodyYaw(void*,float){}
}
namespace ue_wrap::kerfur {
void DriveKerfurState(void*,uint8_t,bool){}
bool DriveKerfurFloppy(void*,bool,uint8_t){return true;}
}
namespace ue_wrap::wisp {bool DriveWispLanding(void*){return true;}}
namespace ue_wrap::sit_pose {void DriveSitting(void*,bool){}}
int main(){
 using namespace coop::element;
 Npc npc;npc.actor=&test::actor;
 coop::net::EntityPoseSnapshot p{};p.pitch=90;p.roll=45;p.yaw=25;
 npc.SetTargetNpcPose(p);npc.Tick();
 assert(test::rotation.Pitch==90&&test::rotation.Roll==45&&test::rotation.Yaw==25);
 p.pitch=179;p.roll=-179;p.x=2000;npc.SetTargetNpcPose(p);npc.Tick();
 assert(test::rotation.Pitch==179&&test::rotation.Roll==-179&&test::position.X==2000);
 p.pitch=-179;p.roll=179;npc.SetTargetNpcPose(p);
 assert(std::fabs(npc.errorPitch_-2)<0.001f&&std::fabs(npc.errorRoll_+2)<0.001f);
 npc.window_.Open(NowMs()-37,75);npc.Tick();
 assert(test::rotation.Pitch>179&&test::rotation.Pitch<181);
 assert(test::rotation.Roll<-179&&test::rotation.Roll>-181);
 npc.window_.Open(NowMs()-75,75);npc.Tick();
 assert(test::rotation.Pitch==-179&&test::rotation.Roll==179);
 // A stationary NPC must still apply a rotation-only update.
 p.pitch=0;p.roll=0;npc.SetTargetNpcPose(p);npc.window_.Open(NowMs()-75,75);npc.Tick();
 assert(test::rotation.Pitch==0&&test::rotation.Roll==0);
 static_assert(coop::net::kNpcPoseDatagramMax<=1400);
 static_assert(coop::net::kMaxNpcBatchEntries==26);
 puts("PASS actual NPC orientation: first pose, teleport, shortest arcs, stationary turn and MTU");
}
