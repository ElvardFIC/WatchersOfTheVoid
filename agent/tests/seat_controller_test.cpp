#define main PreviousChairMain
#include "chair_sync_test.cpp"
#undef main
namespace test {
void ReadyDriver(bool component=true){
 Reset();driverCount=1;driverAtvs[0]=&objects[2];driverSlots[0]=1;
 if(component){objects[2].component=&objects[35];objects[35].location={11,22,33};objects[35].rotation={5,60,8};}
 else {objects[2].location={10,20,30};objects[2].rotation={2,60,3};}
}
void PendingPose(){
 for(bool noTarget:{false,true}){Reset();auto p=Packet(40);if(noTarget)coop::element::target=false;else coop::element::body=false;
  S::OnAnchor(1,p);S::DriveRemotes();CHECK(S::g_anchor[1].on&&!S::g_anchor[1].ready&&!objects[4].moves);
  coop::element::body=coop::element::target=true;S::DriveRemotes();CHECK(objects[4].rotation.Yaw==40);
 }
}
void SeatDistance(){
 for(float x:{451.f,1.e30f}){Reset();auto p=Packet(10,x);S::OnAnchor(1,p);S::DriveRemotes();CHECK(!S::g_anchor[1].on&&!objects[4].moves);}
 Reset();auto p=Packet(10,100);S::OnAnchor(1,p);S::DriveRemotes();CHECK(objects[4].location.X==100);
}
void ChairWorld(){Reset();objects[1].world=reinterpret_cast<void*>(999);auto p=Packet();S::OnAnchor(1,p);CHECK(!S::g_anchor[1].on);
 Reset();p=Packet();onKey=[]{objects[1].world=reinterpret_cast<void*>(999);};S::OnAnchor(1,p);CHECK(!S::g_anchor[1].on);}
void OccupancyRead(){
 for(int mode=0;mode<3;++mode){Reset();auto p=Packet();onSeat=[mode]{if(mode==0)seat=&objects[1];else if(mode==1)++objects[0].serial;
   else coop::players::Registry::Get().local=&objects[2];};
  S::OnAnchor(1,p);CHECK(!S::g_anchor[1].on);
 }
}
void ScopeRestore(){
 Reset();seat=&objects[1];S::Tick();seat=nullptr;auto p=Packet();S::OnAnchor(1,p);S::DriveRemotes();
 const int local=objects[0].clears,remote=objects[4].clears;++sessionSerial;S::Install(&session);
 CHECK(objects[0].clears>local&&objects[4].clears>remote&&!objects[0].sitting);
 Reset();p=Packet();S::OnAnchor(1,p);S::DriveRemotes();onClear=[]{S::Install(&other);};
 ++sessionSerial;S::Install(&session);CHECK(S::g_scope.session==&other);
}
void RigIdentity(){
 for(int mode=0;mode<4;++mode){ReadyDriver();onComponentLocation=[mode]{if(mode==0)++objects[2].serial;
   else if(mode==1)objects[2].cls=&objects[62];else if(mode==2)++generation;else objects[2].world=reinterpret_cast<void*>(999);};
  S::DriveAtvDrivers();CHECK(!objects[4].moves);
 }
}
void SeatComponent(){
 for(int mode=0;mode<3;++mode){ReadyDriver();onComponentLocation=[mode]{if(mode==0)objects[2].component=&objects[36];
   else if(mode==1)++objects[35].serial;else objects[35].cls=&objects[62];};S::DriveAtvDrivers();CHECK(!objects[4].moves);}
}
void DriverFinite(){
 for(int axis=0;axis<6;++axis){ReadyDriver();const float bad=std::numeric_limits<float>::quiet_NaN();
  switch(axis){case 0:objects[35].location.X=bad;break;case 1:objects[35].location.Y=bad;break;case 2:objects[35].location.Z=bad;break;
   case 3:objects[35].rotation.Pitch=bad;break;case 4:objects[35].rotation.Yaw=bad;break;case 5:objects[35].rotation.Roll=bad;break;}
  S::DriveAtvDrivers();CHECK(!objects[4].moves);
 }
 for(bool rotation:{false,true}){ReadyDriver();if(rotation)componentRotationOk=false;else componentLocationOk=false;S::DriveAtvDrivers();CHECK(!objects[4].moves);}
 ReadyDriver();S::DriveAtvDrivers();CHECK(objects[4].location.X==11&&objects[4].rotation.Roll==8&&coop::players::Registry::Get().puppets[1].viewRefreshes==1);
}
void FallbackRotation(){ReadyDriver(false);readRotation=false;S::DriveAtvDrivers();CHECK(!objects[4].moves);}
void FallbackForward(){
 for(auto f:{ue_wrap::FVector{},ue_wrap::FVector{std::numeric_limits<float>::infinity(),0,0},ue_wrap::FVector{3,0,0}}){
  ReadyDriver(false);forward=f;S::DriveAtvDrivers();CHECK(!objects[4].moves);
 }
 ReadyDriver(false);S::DriveAtvDrivers();CHECK(objects[4].location.X==50&&objects[4].location.Z==75);
}
void DriverCompletion(){
 ReadyDriver();people[1].playerNo=0;S::DriveAtvDrivers();CHECK(!objects[4].moves);
 ReadyDriver();moveOk=false;S::DriveAtvDrivers();CHECK(!objects[4].rotations&&!objects[4].shifts);
 ReadyDriver();rotationOk=false;S::DriveAtvDrivers();CHECK(!objects[4].shifts);
 for(int mode=0;mode<5;++mode){ReadyDriver();onMove=[mode]{if(mode==0)++people[1].playerNo;else if(mode==1)++session.peerEpoch;
   else if(mode==2)driverSlots[0]=2;else if(mode==3)++objects[4].serial;else S::OnDisconnect();};
  S::DriveAtvDrivers();CHECK(!objects[4].rotations&&!objects[4].shifts);
 }
}
void SeatPriority(){
 Reset();auto p=Packet(50);S::OnAnchor(1,p);S::DriveRemotes();const int rotations=objects[1].rotations;
 driverCount=1;driverAtvs[0]=&objects[2];driverSlots[0]=1;objects[2].component=&objects[35];objects[35].rotation.Yaw=90;
 S::DriveRemotes();CHECK(!S::g_anchor[1].on&&objects[1].rotations==rotations);
 S::DriveAtvDrivers();CHECK(objects[4].rotation.Yaw==90);
}
}
int main(int argc,char** argv){using namespace test;
 struct Group{const char* name;void(*run)();}groups[]={{"1 pending pose",PendingPose},{"2 bounded anchor",SeatDistance},{"3 chair world",ChairWorld},
 {"4 occupancy read",OccupancyRead},{"5 scope restore",ScopeRestore},{"19 rig identity",RigIdentity},{"20 seat component",SeatComponent},
 {"21 finite driver",DriverFinite},{"22 fallback rotation",FallbackRotation},{"23 fallback forward",FallbackForward},
 {"24 driver completion",DriverCompletion},{"25 seat priority",SeatPriority}};
 bool ran=false;for(const auto& group:groups)if(argc<2||std::atoi(argv[1])==std::atoi(group.name)){group.run();std::printf("PASS seat %s\n",group.name);ran=true;}CHECK(ran);return 0;
}
