#include "../../src/votv-coop/src/ue_wrap/engine/engine_ragdoll_skin.cpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Array {void** data;int32_t num,max;};
struct Joint {ue_wrap::reflection::FName bone1{2,0},bone2{1,0};ue_wrap::FVector pos1{0,0,12},pos2{0,0,0.1f};};
struct Obj {int id,serial=1;bool live=true;void* skeleton=nullptr;void* physicsAsset=nullptr;void* animClass=nullptr;
 ue_wrap::reflection::FName bone{};Array bodies{},joints{};Joint joint{};} objects[12];
Obj& source=objects[0];Obj& target=objects[1];Obj& mesh=objects[2];Obj& skeleton=objects[3];Obj& physics=objects[4];
void* setups[]={&objects[5],&objects[6],&objects[7]};
void* joints[]={&objects[10],&objects[11]};
Obj& customPhysics=objects[8];Obj& customSkeleton=objects[9];
int fallbackLoads=0;bool recyclePhysics=false;
bool missingBone=false, recycle=false;int materials=2;void* installedPhysics=nullptr;
int socketBone=0,socketCalls=0,failSocketAt=0;bool checkPivot=false,pivotObserved=false,invalidScale=false;
std::wstring failFunction;std::vector<std::wstring> calls;
void Reset(){for(int i=0;i<12;++i){objects[i]={};objects[i].id=i;}
 mesh.skeleton=&skeleton;physics.bodies={setups,3,3};missingBone=recycle=false;materials=2;installedPhysics=nullptr;
 physics.joints={joints,2,2};customPhysics.bodies={setups,3,3};customPhysics.joints={joints,2,2};
 for(int i=5;i<8;++i)objects[i].bone={i-4,0};fallbackLoads=0;recyclePhysics=false;
 socketBone=socketCalls=failSocketAt=0;checkPivot=pivotObserved=invalidScale=false;
 failFunction.clear();calls.clear();ue_wrap::engine::g_playerPhysics.Reset();ue_wrap::engine::g_playerSkeleton.Reset();}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
int32_t InternalIndexOf(void* a){return static_cast<test::Obj*>(a)->id;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<12&&a==&test::objects[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return i>=0&&i<12?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}void* ClassOf(void* a){return a;}
int32_t FindPropertyOffset(void*,const wchar_t* n){std::wstring s=n;if(s==L"Skeleton")return offsetof(test::Obj,skeleton);
 if(s==L"PhysicsAsset")return offsetof(test::Obj,physicsAsset);
 if(s==L"AnimClass")return offsetof(test::Obj,animClass);
 if(s==L"ConstraintSetup")return offsetof(test::Obj,joints);
 if(s==L"DefaultInstance")return offsetof(test::Obj,joint);
 if(s==L"ConstraintBone1")return offsetof(test::Joint,bone1);if(s==L"ConstraintBone2")return offsetof(test::Joint,bone2);
 if(s==L"Pos1")return offsetof(test::Joint,pos1);if(s==L"Pos2")return offsetof(test::Joint,pos2);
 if(s==L"SkeletalBodySetups")return offsetof(test::Obj,bodies);if(s==L"BoneName")return offsetof(test::Obj,bone);return -1;}
bool NameEquals(const FName& name,const wchar_t* expected){return name.ComparisonIndex==1&&std::wstring(expected)==L"pelvis";}
void* FindDispatchFunctionCached(void*,const wchar_t* n){return const_cast<wchar_t*>(n);}
void* PropertyInnerStruct(void*,const wchar_t*){return &test::customSkeleton;}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::puppet {void* GetComponentSkeletalMeshAsset(void*){return &test::mesh;}}
namespace ue_wrap::asset_load {void* LoadObjectByPath(const wchar_t* p){++test::fallbackLoads;return std::wstring(p).find(L"_Skeleton")!=std::wstring::npos?&test::skeleton:&test::physics;}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f):fn_(f),buf_(128){}
bool ParamFrame::SetRaw(const wchar_t* n,const void* data,int32_t){std::wstring name=n;
 if(name==L"NewPhysicsAsset")std::memcpy(&test::installedPhysics,data,sizeof(void*));
 if(name==L"InSocketName")test::socketBone=static_cast<const reflection::FName*>(data)->ComparisonIndex;
 if(name==L"bForceReInit")CHECK(*static_cast<const bool*>(data));
 if(name==L"NewType")CHECK(*static_cast<const uint8_t*>(data)==0);
 if(name==L"bNewSimulate")CHECK(!*static_cast<const bool*>(data));
 if(name==L"InAnimationMode")CHECK(*static_cast<const uint8_t*>(data)==1);
 if(name==L"PhysicsBlendWeight")CHECK(*static_cast<const float*>(data)==0.f);return valid();}
bool ParamFrame::GetRaw(const wchar_t*,void* dst,int32_t size)const{std::memcpy(dst,buf_.data(),size);return true;}
bool Call(void*,ParamFrame& f){using namespace test;std::wstring name=static_cast<wchar_t*>(f.function());calls.push_back(name);
 if(name==failFunction)return false;
 if(name==L"GetBoneIndex"){int index=missingBone?-1:1;std::memcpy(f.data(),&index,sizeof(index));}
 if(name==L"GetNumMaterials")std::memcpy(f.data(),&materials,sizeof(materials));
 if(name==L"K2_GetComponentToWorld"){FTransform value{};value.SX=2;std::memcpy(f.data(),&value,sizeof(value));}
 if(name==L"GetSocketTransform"){if(++socketCalls==failSocketAt)return false;
  FTransform value{};value.SX=value.SY=value.SZ=invalidScale?0.f:100.f;
  value.TZ=socketBone==2?100.f:90.f;std::memcpy(f.data(),&value,sizeof(value));}
 if(name==L"SetPhysicsAsset" && checkPivot && installedPhysics==&physics){CHECK(std::fabs(objects[10].joint.pos1.Z)<0.0001f);pivotObserved=true;}
 if(name==L"GetMaterial"){void* material=&mesh;std::memcpy(f.data(),&material,sizeof(material));if(recycle)++target.serial;if(recyclePhysics)++customPhysics.serial;}
 return true;
}
}
namespace ue_wrap::engine {
bool SetSkeletalMesh(void*,void* mesh){CHECK(mesh==&test::mesh);test::calls.push_back(L"mesh");return true;}
bool SetComponentMaterial(void*,int,void*){test::calls.push_back(L"material");return true;}
bool SetAnimClass(void*,void* cls){CHECK(cls==test::source.animClass);test::calls.push_back(L"animClass");return test::failFunction!=L"animClass";}
bool SetAnimTickAlways(void*){return true;}
}
int main(){using namespace test;using ue_wrap::engine::ConfigurePlayerRagdollMesh;
 Reset();CHECK(ConfigurePlayerRagdollMesh(&source,&target));CHECK(installedPhysics==&physics);
 CHECK(std::find(calls.begin(),calls.end(),L"SetSimulatePhysics")<std::find(calls.begin(),calls.end(),L"mesh"));
 CHECK(std::find(calls.begin(),calls.end(),L"SetPhysicsAsset")<std::find(calls.begin(),calls.end(),L"SetAllBodiesSimulatePhysics"));
 CHECK(std::find(calls.begin(),calls.end(),L"mesh")<std::find(calls.begin(),calls.end(),L"SetPhysicsAsset"));
 CHECK(std::find(calls.begin(),calls.end(),L"K2_SetWorldTransform")<std::find(calls.begin(),calls.end(),L"SetPhysicsAsset"));
 CHECK(calls.back()==L"SetAllBodiesPhysicsBlendWeight");
 Reset();source.animClass=&customSkeleton;
 CHECK(ConfigurePlayerRagdollMesh(&source,&target));
 CHECK(std::find(calls.begin(),calls.end(),L"animClass")==calls.end());
 Reset();source.animClass=&customSkeleton;failFunction=L"animClass";
 CHECK(ConfigurePlayerRagdollMesh(&source,&target));
 for(int failure=0;failure<5;++failure){Reset();if(failure==0)mesh.skeleton=&physics;if(failure==1)missingBone=true;
  if(failure==2)physics.bodies.num=2;if(failure==3)materials=65;if(failure==4)recycle=true;
  CHECK(!ConfigurePlayerRagdollMesh(&source,&target));CHECK(std::find(calls.begin(),calls.end(),L"mesh")==calls.end());}
 Reset();failFunction=L"SetPhysicsAsset";CHECK(!ConfigurePlayerRagdollMesh(&source,&target));
 CHECK(std::find(calls.begin(),calls.end(),L"SetAllBodiesPhysicsBlendWeight")==calls.end());
 Reset();mesh.skeleton=&customSkeleton;mesh.physicsAsset=&customPhysics;
 CHECK(ConfigurePlayerRagdollMesh(&source,&target));CHECK(installedPhysics==&customPhysics);CHECK(fallbackLoads==0);
 // An incomplete native asset still uses the complete common-rig fallback.
 Reset();mesh.physicsAsset=&customPhysics;customPhysics.bodies.num=2;
 CHECK(ConfigurePlayerRagdollMesh(&source,&target));CHECK(installedPhysics==&physics);
 // A foreign rig cannot use Kerfur physics when its own asset is invalid.
 for(int failure=0;failure<7;++failure){Reset();mesh.skeleton=&customSkeleton;mesh.physicsAsset=&customPhysics;
  if(failure==0)customPhysics.joints.num=0;if(failure==1)customPhysics.joints.max=1;
  if(failure==2)objects[10].live=false;if(failure==3)objects[5].bone={7,0};
  if(failure==4)objects[6].bone=objects[5].bone;if(failure==5)missingBone=true;
  if(failure==6)recyclePhysics=true;
  CHECK(!ConfigurePlayerRagdollMesh(&source,&target));CHECK(std::find(calls.begin(),calls.end(),L"mesh")==calls.end());}
 Reset();CHECK(ConfigurePlayerRagdollMesh(&source,&target));checkPivot=true;
 CHECK(ue_wrap::engine::RebuildPlayerRagdollPhysics(&target,&physics));CHECK(pivotObserved);
 CHECK(objects[10].joint.pos1.Z==12 && objects[11].joint.pos1.Z==12); // Shared NPC templates restored.
 failFunction=L"SetPhysicsAsset";CHECK(!ue_wrap::engine::RebuildPlayerRagdollPhysics(&target,&physics));
 CHECK(objects[10].joint.pos1.Z==12 && objects[11].joint.pos1.Z==12);
 failFunction=L"GetSocketTransform";CHECK(!ue_wrap::engine::RebuildPlayerRagdollPhysics(&target,&physics));
 CHECK(objects[10].joint.pos1.Z==12);
 failFunction.clear();socketCalls=0;failSocketAt=3;CHECK(!ue_wrap::engine::RebuildPlayerRagdollPhysics(&target,&physics));
 CHECK(objects[10].joint.pos1.Z==12 && objects[11].joint.pos1.Z==12); // A later read failure restores earlier edits.
 failSocketAt=0;invalidScale=true;CHECK(!ue_wrap::engine::RebuildPlayerRagdollPhysics(&target,&physics));
 CHECK(objects[10].joint.pos1.Z==12);
 Reset();CHECK(ConfigurePlayerRagdollMesh(&source,&target));checkPivot=true;
 CHECK(ue_wrap::engine::RebuildPlayerRagdollPhysics(&target,&customPhysics));
 CHECK(!pivotObserved && objects[10].joint.pos1.Z==12); // Foreign authored rigs are not retargeted.
 puts("PASS native custom rig selection, common fallback, bones/joints/pelvis, stale physics and appearance/failure guards");
}
