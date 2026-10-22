#include "../../src/votv-coop/src/ue_wrap/engine/engine_playerragdoll.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id{Player,Body,Mesh,Source,Asset,ActorClass,SkinClass,SceneClass,PrimitiveClass,
    NumBones,BoneName,BoneIndex,SocketLocation,SocketRotation,Linear,Angular,SetLinear,SetAngular,
    Attach,Position,Collision,AllSim,Sim,Blend,AnySim,Physics,Arm,Controller,AttachParent,ControlRotation,WorldRotation,AllLinear,Count};
struct Obj{int id=0,serial=1;bool live=true;std::array<uint8_t,4096> storage{};} objects[Count];
struct Params{void* parent=nullptr;ue_wrap::reflection::FName bone{};int32_t index=0;
    uint8_t locationRule=0,rotationRule=0,scaleRule=0,collision=0;bool weld=false,simulate=false,sweep=false,teleport=false,skip=false;
    float weight=0;ue_wrap::FVector vector{};ue_wrap::FRotator rotation{};bool result=false;};
void* At(Id id){return &objects[id];}
ue_wrap::FVector pelvis{0,0,10},origin{},initial{0,0,-25};
float lowest=0;int positions=0,configurations=0,returnErrors=0;bool attached=false,attachOK=true,configuredOK=true;
std::function<void()> onAttach;std::wstring missingParam;
ue_wrap::FRotator armRotation{},controlRotation{-35,385,0};void* armParent=nullptr;
int cameraWrites=0;std::function<void()> onRotation;
std::map<int,ue_wrap::FVector> velocities;bool bodySim=true;int velocityWrites=0;
ue_wrap::ufunction_hook::PreNativeCallback velocityHook=nullptr;bool velocityArmed=false;
void Put(void* obj,size_t off,void* value){std::memcpy(static_cast<uint8_t*>(obj)+off,&value,sizeof(value));}
void Reset(){for(int i=0;i<Count;++i){objects[i].id=i;++objects[i].serial;objects[i].live=true;}
    Put(At(Player),0xc40,At(Body));Put(At(Body),0x230,At(Mesh));Put(At(Mesh),0x300,At(Asset));
    Put(At(Player),0x600,At(Arm));armParent=At(Mesh);armRotation={};controlRotation={-35,385,0};cameraWrites=0;onRotation={};
    pelvis={0,0,10};origin={};lowest=0;positions=configurations=returnErrors=0;attached=false;attachOK=configuredOK=true;onAttach={};missingParam.clear();
    velocities.clear();bodySim=true;velocityWrites=0;}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0 && i<test::Count?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return ObjectAt(i)==p && IsLive(p);}void* ClassOf(void*){return test::At(test::ActorClass);}
int32_t FindPropertyOffset(void*,const wchar_t* name){if(!wcscmp(name,L"lag"))return test::missingParam==L"lag"?-1:0x600;
    return !wcscmp(name,L"PhysicsAssetOverride")?0x300:-1;}
void* FindClass(const wchar_t* name){using namespace test;if(!wcscmp(name,L"SkinnedMeshComponent"))return At(SkinClass);
    if(!wcscmp(name,L"SceneComponent"))return At(SceneClass);if(!wcscmp(name,L"PrimitiveComponent"))return At(PrimitiveClass);return At(ActorClass);}
void* FindFunction(void*,const wchar_t* name){using namespace test;
    const std::pair<const wchar_t*,Id> rows[]={{L"GetNumBones",NumBones},{L"GetBoneName",BoneName},{L"GetBoneIndex",BoneIndex},
        {L"GetSocketLocation",SocketLocation},{L"GetSocketRotation",SocketRotation},{L"GetPhysicsLinearVelocity",Linear},
        {L"GetPhysicsAngularVelocityInDegrees",Angular},{L"SetPhysicsLinearVelocity",SetLinear},
        {L"SetPhysicsAngularVelocityInDegrees",SetAngular},{L"K2_AttachToComponent",Attach},
        {L"K2_SetWorldLocation",Position},{L"SetCollisionEnabled",Collision},{L"SetAllBodiesSimulatePhysics",AllSim},
        {L"SetSimulatePhysics",Sim},{L"SetAllBodiesPhysicsBlendWeight",Blend},{L"IsAnySimulatingPhysics",AnySim},{L"SetPhysicsAsset",Physics},
        {L"GetAttachParent",AttachParent},{L"GetControlRotation",ControlRotation},{L"K2_SetWorldRotation",WorldRotation}};
    if(!wcscmp(name,L"SetAllPhysicsLinearVelocity"))return At(AllLinear);
    for(const auto& row:rows)if(!wcscmp(name,row.first))return At(row.second);return nullptr;
}
void* FindDispatchFunctionCached(void* cls,const wchar_t* name){return FindFunction(cls,name);}
std::wstring ToString(const FName& name){return name.ComparisonIndex==1?L"pelvis":name.ComparisonIndex==2?L"chest":L"None";}
int32_t FunctionFrameSize(void*){return sizeof(test::Params);}
std::vector<ParamInfo> FunctionParams(void* fn){using namespace test;std::vector<ParamInfo> p;
    const auto add=[&](const wchar_t* n,int off){if(missingParam==n)return;ParamInfo x{};x.name=n;x.offset=off;p.push_back(x);};
#define P(n,f) add(L##n,offsetof(Params,f))
    if(fn==At(Attach)){P("Parent",parent);P("SocketName",bone);P("LocationRule",locationRule);P("RotationRule",rotationRule);
        P("ScaleRule",scaleRule);P("bWeldSimulatedBodies",weld);return p;} // Actor's method is void.
    P("BoneIndex",index);P("BoneName",bone);P("InSocketName",bone);P("NewLocation",vector);P("bSweep",sweep);P("bTeleport",teleport);
    P("NewType",collision);P("bNewSimulate",simulate);P("bSimulate",simulate);P("PhysicsBlendWeight",weight);P("bSkipCustomPhysicsType",skip);
    P("NewVel",vector);P("NewAngVel",vector);P("bAddToCurrent",weld);
    P("NewPhysicsAsset",parent);P("bForceReInit",teleport);
    P("NewRotation",rotation);
    if(fn==At(NumBones)||fn==At(BoneIndex))P("ReturnValue",index);else if(fn==At(BoneName))P("ReturnValue",bone);
    else if(fn==At(SocketLocation)||fn==At(Linear)||fn==At(Angular))P("ReturnValue",vector);
    else if(fn==At(SocketRotation)||fn==At(ControlRotation))P("ReturnValue",rotation);
    else if(fn==At(AttachParent))P("ReturnValue",parent);else if(fn==At(AnySim))P("ReturnValue",result);
#undef P
    return p;
}
bool CallFunction(void* object,void* fn,void* raw){using namespace test;auto& p=*static_cast<Params*>(raw);
    if(fn==At(SetLinear) && velocityArmed && velocityHook &&
       velocityHook(object,object,static_cast<const uint8_t*>(raw))==ue_wrap::script_gate::Verdict::Cancel)return true;
    if(fn==At(NumBones))p.index=2;if(fn==At(BoneName))p.bone={p.index+1,0};
    if(fn==At(BoneIndex))p.index=p.bone.ComparisonIndex>0?p.bone.ComparisonIndex-1:-1;
    if(fn==At(SocketLocation))p.vector=pelvis;if(fn==At(Linear))p.vector=velocities.contains(p.bone.ComparisonIndex)?velocities[p.bone.ComparisonIndex]:initial;if(fn==At(Angular))p.vector={};
    if(fn==At(Sim))bodySim=p.simulate;
    if(fn==At(AllLinear)){CHECK(bodySim && !p.weld);for(int bone:{1,2,3})velocities[bone]=p.vector;++velocityWrites;}
    if(fn==At(SetLinear) && bodySim){auto& v=velocities[p.bone.ComparisonIndex?p.bone.ComparisonIndex:1];if(!p.weld)v={};
        v.X+=p.vector.X;v.Y+=p.vector.Y;v.Z+=p.vector.Z;++velocityWrites;}
    if(fn==At(Attach)){CHECK(p.parent==At(Mesh) && p.bone.ComparisonIndex==1 && !p.weld);attached=attachOK;
        auto callback=std::move(onAttach);onAttach={};if(callback)callback();}
    if(fn==At(Position)){CHECK(!p.sweep && p.teleport);++positions;const auto delta=p.vector.Z-origin.Z;pelvis.Z+=delta;lowest+=delta;origin=p.vector;}
    if(fn==At(AttachParent))p.parent=armParent;
    if(fn==At(ControlRotation))p.rotation=controlRotation;
    if(fn==At(WorldRotation)){CHECK(!p.sweep && p.teleport);armRotation=p.rotation;++cameraWrites;
        auto callback=std::move(onRotation);onRotation={};if(callback)callback();}
    if(fn==At(AnySim))p.result=true;return true;
}
}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
    :ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
Resolved Resolve(const Function& row){return Resolved(reflection::FindFunction(nullptr,row.Name()),&row);}
}
namespace ue_wrap::ufunction_hook {
bool InstallPreHook(const facts::Resolved& fn,PreNativeCallback cb,bool armed){test::velocityHook=cb;test::velocityArmed=armed;return bool(fn);}
bool SetArmed(void*,PreNativeCallback cb,bool armed){test::velocityHook=cb;test::velocityArmed=armed;return true;}
}
namespace ue_wrap::world_identity{void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::log{void Write(Level,const char* fmt,...){if(std::string(fmt).find("unknown param")!=std::string::npos)++test::returnErrors;}}
namespace ue_wrap::puppet{void* GetMeshPlayerVisibleComponent(void*){return test::At(test::Source);}void* GetComponentSkeletalMeshAsset(void*){return test::At(test::Asset);}}
namespace ue_wrap::engine{
bool RebuildPlayerRagdollPhysics(void*,void*){test::armRotation={90,23,-67};test::velocities.clear();return true;}
void* GetController(void*){return test::At(test::Controller);}
void* BeginDeferredSpawn(void*,const FVector&,const FRotator&){return nullptr;}
bool FinishDeferredSpawn(void*,const FVector&,const FRotator&){return false;}
bool DestroyActor(void*){return true;}
bool GetAttachParentActor(void*,void*& parent){parent=test::attached?test::At(test::Body):nullptr;return true;}
bool ConfigurePlayerRagdollMesh(void*,void*){++test::configurations;if(!test::configuredOK)return false;
    test::armRotation={90,23,-67};test::origin={};test::pelvis={0,0,100};test::lowest=0;test::velocities.clear();test::bodySim=false;return true;}
bool GetBoneWorldLocationByName(void*,const wchar_t*,FVector& out){out=test::pelvis;return true;}
FVector GetComponentLocation(void*){return test::origin;}
}
int main(int argc,char** argv){using namespace test;Reset();
    if(argc>1 && std::string(argv[1])=="momentum"){
        initial={};velocities[2]={-100,0,0};
        CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        for(int bone:{1,2,3}){Params native{};native.bone={bone,0};native.vector={600,0,0};native.weld=true;
            ue_wrap::reflection::CallFunction(At(Mesh),At(SetLinear),&native);}
        std::this_thread::sleep_for(std::chrono::milliseconds(35));CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        CHECK(std::abs(velocities[1].X-600)<0.001f && std::abs(velocities[2].X-500)<0.001f && velocities[3].X==600);
        const int writes=velocityWrites;CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)) && velocityWrites==writes);
        Reset();initial={};velocities[1]={600,100,-20};velocities[2]={500,100,-20};
        CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        std::this_thread::sleep_for(std::chrono::milliseconds(35));CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        CHECK(velocities[3].X==600 && velocities[3].Y==100 && velocities[3].Z==-20 && velocities[2].X==500);
        Reset();initial={};CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        std::this_thread::sleep_for(std::chrono::milliseconds(35));CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        for(int bone:{1,2,3}){Params native{};native.bone={bone,0};native.vector={0,-450,0};native.weld=true;
            ue_wrap::reflection::CallFunction(At(Mesh),At(SetLinear),&native);}
        CHECK(velocities[1].Y==-450 && velocities[2].Y==-450 && velocities[3].Y==-450 && !velocityArmed);
        Reset();initial={};velocities[1]={600,0,0};velocities[2]={500,0,0};
        CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        const auto command=[](int bone,float x,bool add){Params native{};native.bone={bone,0};native.vector={x,0,0};native.weld=add;
            ue_wrap::reflection::CallFunction(At(Mesh),At(SetLinear),&native);};
        command(0,25,true);command(3,50,true);command(3,40,false);command(3,3,true);command(2,30,false);command(2,2,true);
        std::this_thread::sleep_for(std::chrono::milliseconds(35));CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        CHECK(velocities[1].X==625 && velocities[2].X==32 && velocities[3].X==43);
        Reset();initial={};CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        Params stale{};stale.bone={1,0};stale.vector={600,0,0};stale.weld=true;
        ue_wrap::reflection::CallFunction(At(Mesh),At(SetLinear),&stale);
        ++objects[Mesh].serial;const int oldWrites=velocityWrites;
        CHECK(!ue_wrap::engine::FinishRagdollVelocity(At(Mesh)) && velocityWrites==oldWrites && !velocityArmed);
        Reset();CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        Put(At(Player),0xc40,nullptr);CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)) && !velocityArmed);
        puts("PASS native delayed momentum reaches every body once and retains the game's chest kick");return 0;
    }
    if(argc>1 && std::string(argv[1])=="camera"){
        const auto preservesView=[] {CHECK(std::abs(armRotation.Pitch-controlRotation.Pitch)<0.001f &&
            std::abs(armRotation.Yaw-controlRotation.Yaw)<0.001f && std::abs(armRotation.Roll-controlRotation.Roll)<0.001f);};
        CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));CHECK(cameraWrites==1);preservesView();
        armRotation={90,23,-67};CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));preservesView();
        std::this_thread::sleep_for(std::chrono::milliseconds(35));CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));preservesView();
        const int writes=cameraWrites;armRotation={-20,42,75};CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
        CHECK(cameraWrites==writes && armRotation.Roll==75); // Native head motion resumes after warmup.
        for(const auto& absent:{L"lag",L"ReturnValue",L"NewRotation",L"bSweep",L"bTeleport"}){
            Reset();missingParam=absent;CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));CHECK(cameraWrites==0);}
        Reset();controlRotation.Pitch=NAN;CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));CHECK(cameraWrites==0);
        Reset();armParent=At(Source);CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));CHECK(cameraWrites==0);
        Reset();CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));const int captured=cameraWrites;
        armParent=At(Source);CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));CHECK(cameraWrites==captured);
        for(auto id:{Player,Mesh,Arm}){Reset();ue_wrap::engine::RagdollView view;CHECK(view.Capture(At(Player),At(Mesh)));
            ++objects[id].serial;CHECK(!view.Restore() && cameraWrites==0);}
        puts("PASS native camera: entry pitch/yaw, evaluated-pose rebase, free head follow, recovery/recycle/metadata guards");return 0;
    }
    if(argc>1 && std::string(argv[1])=="attach"){
        CHECK(ue_wrap::engine::AttachActorToRagdollBody(At(Player),At(Body)) && attached && !returnErrors);
        attachOK=false;CHECK(!ue_wrap::engine::AttachActorToRagdollBody(At(Player),At(Body)));
        for(auto id:{Player,Body,Mesh}){Reset();onAttach=[id]{++objects[id].serial;};
            CHECK(!ue_wrap::engine::AttachActorToRagdollBody(At(Player),At(Body)));}
        Reset();missingParam=L"Parent";CHECK(!ue_wrap::engine::AttachActorToRagdollBody(At(Player),At(Body)) && !attached);
        puts("PASS actual ParamFrame: void Actor attachment, pelvis socket and parent readback");return 0;
    }
    CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)) && configurations==1);
    CHECK(lowest==0 && pelvis.Z==100 && positions==0); // Never lower full-size legs to a plushie's pelvis.
    std::this_thread::sleep_for(std::chrono::milliseconds(35));CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
    CHECK(configurations==1 && lowest==0);CHECK(ue_wrap::engine::RefreshLocalRagdollModel(At(Player)) && configurations==1);
    Put(At(Player),0xc40,nullptr);CHECK(!ue_wrap::engine::RefreshLocalRagdollModel(At(Player)));
    puts("PASS native local model entry: source geometry retained, no floor penetration, one staged configuration and recovery");
}
