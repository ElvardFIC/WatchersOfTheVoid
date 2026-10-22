#include <algorithm>
#include <cstdint>
#define private public
#include "coop/element/npc.h"
#undef private
#include "../../src/votv-coop/src/coop/creatures/npc_pose_drive.cpp"
#include <cassert>
#include <cstdio>
namespace test {int actor=1,writes=0,landings=0;bool canWrite=true,canLand=false;float bodyYaw=0;ue_wrap::FRotator rotation{};ue_wrap::FVector position{};}
namespace ue_wrap::reflection {bool IsLiveByIndex(void* p,int32_t){return p==&test::actor;}}
namespace ue_wrap::engine {
bool SetActorLocation(void*,const FVector& p){++test::writes;if(!test::canWrite)return false;test::position=p;return true;}
bool SetActorRotation(void*,const FRotator& r){if(!test::canWrite)return false;test::rotation=r;return true;}
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
namespace ue_wrap::fossilhound {void DriveBodyYaw(void*,float yaw){test::bodyYaw=yaw;}}
namespace ue_wrap::wisp {bool DriveWispLanding(void*){++test::landings;return test::canLand;}}
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
 // Identical re-sends produce no transform writes; pitch-only changes still apply.
 int before=test::writes;npc.SetTargetNpcPose(p);npc.Tick();assert(test::writes==before);
 p.pitch=15;npc.SetTargetNpcPose(p);npc.window_.Open(NowMs()-75,75);npc.Tick();
 assert(test::rotation.Pitch==15&&test::writes==before+1);
 // A failed final transform must retry without another packet or an open LERP window.
 p.x=4000;test::canWrite=false;npc.SetTargetNpcPose(p);npc.Tick();assert(npc.dirty_);
 assert(test::position.X==2000);test::canWrite=true;npc.Tick();assert(test::position.X==4000&&!npc.dirty_);
 before=test::writes;npc.Tick();assert(test::writes==before);
 p.stateBits|=coop::net::kEntityPoseBitHasBodyYaw;p.bodyYaw=73;npc.SetTargetNpcPose(p);
 npc.window_.Open(NowMs()-75,75);npc.Tick();assert(test::bodyYaw==73);
 // A grounded wisp retries its presentation independently of pose dirtiness and stops on success.
 npc.MarkWispMirror();npc.Tick();assert(test::landings==1);
 test::canLand=true;npc.Tick();npc.Tick();assert(test::landings==2);
 static_assert(coop::net::kNpcPoseDatagramMax<=1400);
 static_assert(coop::net::kMaxNpcBatchEntries==26);
 puts("PASS actual NPC orientation: first pose, teleport, shortest arcs, stationary turn and MTU");
}
