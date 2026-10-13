#include "../../src/votv-coop/src/ue_wrap/engine/engine_ragdoll_skin.cpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Array {void** data;int32_t num,max;};
struct Obj {int id,serial=1;bool live=true;void* skeleton=nullptr;ue_wrap::reflection::FName bone{};Array bodies{};} objects[8];
Obj& source=objects[0];Obj& target=objects[1];Obj& mesh=objects[2];Obj& skeleton=objects[3];Obj& physics=objects[4];
void* setups[]={&objects[5],&objects[6],&objects[7]};
bool missingBone=false, recycle=false;int materials=2;void* installedPhysics=nullptr;
std::wstring failFunction;std::vector<std::wstring> calls;
void Reset(){for(int i=0;i<8;++i){objects[i]={};objects[i].id=i;}
 mesh.skeleton=&skeleton;physics.bodies={setups,3,3};missingBone=recycle=false;materials=2;installedPhysics=nullptr;
 failFunction.clear();calls.clear();ue_wrap::engine::g_playerPhysics.Reset();ue_wrap::engine::g_playerSkeleton.Reset();}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
int32_t InternalIndexOf(void* a){return static_cast<test::Obj*>(a)->id;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<8&&a==&test::objects[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return i>=0&&i<8?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}void* ClassOf(void* a){return a;}
int32_t FindPropertyOffset(void*,const wchar_t* n){std::wstring s=n;if(s==L"Skeleton")return offsetof(test::Obj,skeleton);
 if(s==L"SkeletalBodySetups")return offsetof(test::Obj,bodies);if(s==L"BoneName")return offsetof(test::Obj,bone);return -1;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){return const_cast<wchar_t*>(n);}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::puppet {void* GetComponentSkeletalMeshAsset(void*){return &test::mesh;}}
namespace ue_wrap::asset_load {void* LoadObjectByPath(const wchar_t* p){return std::wstring(p).find(L"_Skeleton")!=std::wstring::npos?&test::skeleton:&test::physics;}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f):fn_(f),buf_(128){}
bool ParamFrame::SetRaw(const wchar_t* n,const void* data,int32_t){std::wstring name=n;
 if(name==L"NewPhysicsAsset")std::memcpy(&test::installedPhysics,data,sizeof(void*));
 if(name==L"bForceReInit")CHECK(*static_cast<const bool*>(data));
 if(name==L"InAnimationMode")CHECK(*static_cast<const uint8_t*>(data)==1);
 if(name==L"PhysicsBlendWeight")CHECK(*static_cast<const float*>(data)==1.f);return valid();}
bool ParamFrame::GetRaw(const wchar_t*,void* dst,int32_t size)const{std::memcpy(dst,buf_.data(),size);return true;}
bool Call(void*,ParamFrame& f){using namespace test;std::wstring name=static_cast<wchar_t*>(f.function());calls.push_back(name);
 if(name==failFunction)return false;
 if(name==L"GetBoneIndex"){int index=missingBone?-1:1;std::memcpy(f.data(),&index,sizeof(index));}
 if(name==L"GetNumMaterials")std::memcpy(f.data(),&materials,sizeof(materials));
 if(name==L"K2_GetComponentToWorld"){FTransform value{};value.SX=2;std::memcpy(f.data(),&value,sizeof(value));}
 if(name==L"GetMaterial"){void* material=&mesh;std::memcpy(f.data(),&material,sizeof(material));if(recycle)++target.serial;}
 return true;
}
}
namespace ue_wrap::engine {
bool SetSkeletalMesh(void*,void* mesh){CHECK(mesh==&test::mesh);test::calls.push_back(L"mesh");return true;}
bool SetComponentMaterial(void*,int,void*){test::calls.push_back(L"material");return true;}
}
int main(){using namespace test;using ue_wrap::engine::ConfigurePlayerRagdollMesh;
 Reset();CHECK(ConfigurePlayerRagdollMesh(&source,&target));CHECK(installedPhysics==&physics);
 CHECK(std::find(calls.begin(),calls.end(),L"mesh")<std::find(calls.begin(),calls.end(),L"SetPhysicsAsset"));
 CHECK(calls.back()==L"SetAllBodiesPhysicsBlendWeight");
 for(int failure=0;failure<5;++failure){Reset();if(failure==0)mesh.skeleton=&physics;if(failure==1)missingBone=true;
  if(failure==2)physics.bodies.num=2;if(failure==3)materials=65;if(failure==4)recycle=true;
  CHECK(!ConfigurePlayerRagdollMesh(&source,&target));CHECK(std::find(calls.begin(),calls.end(),L"mesh")==calls.end());}
 Reset();failFunction=L"SetPhysicsAsset";CHECK(!ConfigurePlayerRagdollMesh(&source,&target));
 CHECK(std::find(calls.begin(),calls.end(),L"SetAllBodiesPhysicsBlendWeight")==calls.end());
 puts("PASS selected mesh/full physics, incompatible and missing bones, appearance limits, recycled target and failure propagation");
}
