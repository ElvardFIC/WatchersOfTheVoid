// Extracted production bodies; fake UE object slots, metadata and native calls.
#define main AtvPreparationMain
#include "atv_spawn_preparation_test.cpp"
#undef main
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace A=ue_wrap::atv;
using F=A::SpawnFail;
using namespace test;
ue_wrap::FVector location{100,200,50};ue_wrap::FRotator facing{};F why;
void* Spawn(){return A::SpawnMirror(L"ATV_C",location,facing,L"coopatv1",&why);}
void Pose(){
 for(int axis=0;axis<6;++axis)for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
  Reset();location={100,200,50};facing={};
  switch(axis){case 0:location.X=bad;break;case 1:location.Y=bad;break;case 2:location.Z=bad;break;
   case 3:facing.Pitch=bad;break;case 4:facing.Yaw=bad;break;case 5:facing.Roll=bad;break;}
  CHECK(!Spawn() && why==F::InvalidPose && !calls[0] && !destroyed);
 }
 location={100,200,50};facing={};
}
void Services(){
 Reset();CHECK(Spawn()==&objects[0]);Reset();serviceClass=&objects[13];service=&objects[14];beginFunction=&objects[10];
 CHECK(Spawn()==&objects[0] && why==F::None && calls[0]==1 && calls[1]==1);
 for(int mode=0;mode<4;++mode){
  Reset();onFrame=[mode](int stage){if(stage)return;onFrame={};
   if(mode==0)++objects[4].serial;else if(mode==1)++objects[5].serial;
   else if(mode==2)beginFunction=&objects[10];else ++objects[8].serial;};
  CHECK(!Spawn() && why==F::Retired && !calls[0]);
 }
}
void ClassKey(){
 for(int mode=0;mode<3;++mode){
  Reset();onName=[mode]{if(mode==0)++objects[2].serial;else if(mode==1)actorClass=baseClass=&objects[11];else ++keyOffset;};
  CHECK(!Spawn() && why==F::Retired && !calls[0]);
 }
 Reset();std::wstring cls=L"ATV_C",key=L"coopatv1";auto loc=location;auto rot=facing;
 onName=[&]{cls=L"Foreign";key=L"changed";loc.X=999;rot.Yaw=90;};
 CHECK(A::SpawnMirror(cls,loc,rot,key,&why)==&objects[0]);
 CHECK(lastTransform.TX==100 && lastTeleport.X==100 && lastFacing.Yaw==0);
 Reset();onKey=[]{objects[0].key={321,0};};
 CHECK(!Spawn() && why==F::KeyMismatch && destroyed==1);
}
void WorldLifetime(){
 for(int mode=0;mode<5;++mode){
  Reset();onName=[mode]{if(mode==0)++generation;else if(mode==1)++objects[6].serial;
   else if(mode==2)context=&objects[12];else if(mode==3)objects[6].cls=&objects[13];else ++worldToken;};
  CHECK(!Spawn() && why==F::Retired && !calls[0]);
 }
 for(int stage=0;stage<2;++stage){
  Reset();onFrame=[stage](int current){if(current==stage){onFrame={};++generation;}};
  CHECK(!Spawn() && why==F::Retired && !calls[stage] && !destroyed);
 }
 Reset();onNative=[](int stage){if(stage==1){onNative={};++generation;}};
 CHECK(!Spawn() && why==F::Retired && !calls[2] && !destroyed);
}
void BeginType(){
 for(int mode=0;mode<2;++mode){
  Reset();beginResult=&objects[1];if(mode==1){objects[1].cls=actorClass;objects[1].world=reinterpret_cast<void*>(999);}
  CHECK(!Spawn() && why==F::BeginFailed && !calls[1] && !calls[2] && !destroyed);
  CHECK(objects[1].key.ComparisonIndex==777);
 }
 Reset();beginReadOk=false;CHECK(!Spawn() && why==F::BeginFailed && !calls[1] && !destroyed);
}
void FinishLifetime(){
 for(int mode=0;mode<2;++mode){
  Reset();onFrame=[mode](int stage){if(stage==1){onFrame={};if(mode==0)++objects[0].serial;else objects[0].cls=&objects[13];}};
  CHECK(!Spawn() && why==F::Retired && !calls[1] && !destroyed);
 }
 Reset();onNative=[](int stage){if(stage==1){onNative={};++objects[0].serial;}};
 CHECK(!Spawn() && why==F::Retired && !calls[2] && !destroyed);
}
void Cleanup(){
 Reset();invalidFunction=1;onFrame=[](int stage){if(stage==1){onFrame={};++objects[0].serial;}};
 CHECK(!Spawn() && why==F::FinishFailed && !destroyed && objects[0].live);
 for(int mode=0;mode<2;++mode){
  Reset();onFrame=[mode](int stage){if(stage==1){onFrame={};++objects[mode?6:4].serial;}};
  CHECK(!Spawn() && why==F::Retired && destroyed==1 && destroyedActor==&objects[0]);
 }
}
void TeleportLifetime(){
 for(int axis=0;axis<6;++axis){
  Reset();auto loc=location;auto rot=facing;const float bad=std::numeric_limits<float>::infinity();
  switch(axis){case 0:loc.X=bad;break;case 1:loc.Y=bad;break;case 2:loc.Z=bad;break;
   case 3:rot.Pitch=bad;break;case 4:rot.Yaw=bad;break;case 5:rot.Roll=bad;break;}
  CHECK(!A::TeleportRig(&objects[0],loc,rot) && !calls[2]);
 }
 for(int mode=0;mode<5;++mode){
  Reset();onFrame=[mode](int stage){if(stage==2){onFrame={};if(mode==0)++objects[0].serial;
   else if(mode==1)++objects[2].serial;else if(mode==2)++objects[9].serial;
   else if(mode==3)++generation;else teleportFunction=&objects[12];}};
  CHECK(!A::TeleportRig(&objects[0],location,facing) && !calls[2]);
 }
 Reset();onNative=[](int stage){if(stage==2){onNative={};++objects[0].serial;}};
 CHECK(!A::TeleportRig(&objects[0],location,facing) && calls[2]==1);
}
void PlacementFail(){
 for(int mode=0;mode<2;++mode){
  Reset();if(mode==0){failFunction=2;failField=L"NewLocation";}else callOk[2]=false;
  CHECK(!Spawn() && why==F::PlacementFailed && destroyed==1);
 }
 Reset();CHECK(Spawn()==&objects[0] && why==F::None && !destroyed);
}
int main(int argc,char** argv){
 const char* mode=argc>1?argv[1]:"all";
 struct Group{const char* name;void(*run)();}groups[]={{"pose",Pose},{"services",Services},{"class-key",ClassKey},
  {"world",WorldLifetime},{"begin-type",BeginType},{"finish",FinishLifetime},{"cleanup",Cleanup},{"teleport",TeleportLifetime},{"placement-fail",PlacementFail}};
 bool ran=false;for(const auto& group:groups)if(!std::strcmp(mode,"all") || !std::strcmp(mode,group.name)){
  group.run();std::printf("PASS ATV spawn lifetime %s\n",group.name);ran=true;
 }
 CHECK(ran);return 0;
}
