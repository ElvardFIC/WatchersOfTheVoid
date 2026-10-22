// Opt-in native entry/recovery acceptance, including the completed wakeup timeline.
#include "harness/autotest.h"
#include "coop/player/local_body.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace harness::autotest {
namespace {
namespace E=ue_wrap::engine;
namespace R=ue_wrap::reflection;
using ue_wrap::FVector;
using ue_wrap::FRotator;
template<class F> bool Game(F action) {
    auto done=std::make_shared<std::atomic<bool>>(false);
    ue_wrap::game_thread::Post([done,action]{action();done->store(true);});
    for(int ms=0;ms<8000 && !done->load();ms+=5)::Sleep(5);
    return done->load();
}
void* Component(void* player,const wchar_t* name) {
    if(!player || !R::IsLive(player))return nullptr;
    void* out=nullptr;const int offset=R::FindPropertyOffset(R::ClassOf(player),name);
    if(offset>=0)std::memcpy(&out,static_cast<uint8_t*>(player)+offset,sizeof(out));
    return out && R::IsLive(out)?out:nullptr;
}
bool Flag(void* player,const wchar_t* name,bool fallback) {
    int32_t byte=-1;uint8_t mask=0;
    return player && R::FindBoolProperty(R::ClassOf(player),name,byte,mask)?
        (static_cast<uint8_t*>(player)[byte]&mask)!=0:fallback;
}
float YawError(float a,float b){return std::abs(ue_wrap::NormalizeAxis(a-b));}

// The old 20 ms polling could miss the initial impulse once entry no longer
// pauses physics for 30 ms: floor friction has already acted by the first poll.
// Observe the real native per-bone command after it runs, before the next solver
// step, instead of changing the floor or adding a guessed velocity to the test.
UE_FACT_FN(kMomentumCommand,"ragdoll_momentum_probe",L"PrimitiveComponent",L"SetPhysicsLinearVelocity",Declared);
struct InitialMomentum {
    ue_wrap::CachedObjRef player,mesh;
    FVector expected{};
    struct Bone {R::FName name{};float projection=0;bool seen=false;};
    std::vector<Bone> bones;
    R::FName pending{};bool havePending=false;
    int boneOffset=-1;
} g_initial;
ue_wrap::script_gate::Verdict BeforeMomentum(void* component,void* source,const uint8_t* params) {
    g_initial.havePending=false;
    void* player=g_initial.player.Get();
    if(!player || source!=player || component!=E::GetLocalRagdollBodyMesh(player) || !params)return ue_wrap::script_gate::Verdict::Run;
    if(g_initial.mesh.Get()!=component) {
        g_initial.mesh.Set(component);g_initial.bones.clear();std::vector<E::ArticulatedBone> rig;
        if(!E::ReadArticulatedPose(component,rig))return ue_wrap::script_gate::Verdict::Run;
        for(const auto& bone:rig){InitialMomentum::Bone entry;
            ue_wrap::ParamFrame label(R::FindDispatchFunctionCached(R::ClassOf(component),L"GetBoneName"));
            if(label.Set<int32_t>(L"BoneIndex",bone.index) && ue_wrap::Call(component,label) &&
                label.GetRaw(L"ReturnValue",&entry.name,sizeof(entry.name)))g_initial.bones.push_back(entry);
        }
    }
    std::memcpy(&g_initial.pending,params+g_initial.boneOffset,sizeof(g_initial.pending));
    g_initial.havePending=true;return ue_wrap::script_gate::Verdict::Run;
}
void AfterMomentum(void* component,void* source,void*) {
    if(!g_initial.havePending || source!=g_initial.player.Get() || component!=g_initial.mesh.Get())return;
    g_initial.havePending=false;
    for(auto& bone:g_initial.bones)if(bone.name.ComparisonIndex==g_initial.pending.ComparisonIndex && bone.name.Number==g_initial.pending.Number){
        FVector velocity{};ue_wrap::ParamFrame read(R::FindDispatchFunctionCached(R::ClassOf(component),L"GetPhysicsLinearVelocity"));
        const auto& v=g_initial.expected;const float speed=std::sqrt(v.X*v.X+v.Y*v.Y+v.Z*v.Z);
        if(speed>0 && read.Set<R::FName>(L"BoneName",bone.name) && ue_wrap::Call(component,read) &&
            read.GetRaw(L"ReturnValue",&velocity,sizeof(velocity))){
            bone.projection=(velocity.X*v.X+velocity.Y*v.Y+velocity.Z*v.Z)/speed;bone.seen=true;
        }
    }
}
}

void BeginNativeRagdollMomentumProbe(void* player,const FVector& expected) {
    g_initial={};g_initial.player.Set(player);g_initial.expected=expected;
    const auto fn=ue_wrap::facts::Resolve(kMomentumCommand);
    ue_wrap::ParamFrame frame(fn.Fn());g_initial.boneOffset=frame.ParamOffset(L"BoneName");
    if(g_initial.boneOffset<0 || g_initial.boneOffset+int(sizeof(R::FName))>frame.FrameSize()){g_initial={};return;}
    ue_wrap::ufunction_hook::InstallPreHook(fn,&BeforeMomentum,true);
    ue_wrap::ufunction_hook::InstallPostHook(fn,&AfterMomentum,true);
}
bool ReadNativeRagdollMomentumProbe(float& mean,float& pelvis,int& count) {
    if(g_initial.bones.empty())return false;
    mean=0;count=0;pelvis=-1.e9f;
    for(const auto& bone:g_initial.bones){if(!bone.seen)return false;mean+=bone.projection;++count;
        if(R::NameEquals(bone.name,L"pelvis"))pelvis=bone.projection;}
    mean/=count;return true;
}
void EndNativeRagdollMomentumProbe() {
    g_initial={};const auto fn=ue_wrap::facts::Resolve(kMomentumCommand);
    ue_wrap::ufunction_hook::SetArmed(fn.Fn(),&BeforeMomentum,false);
    ue_wrap::ufunction_hook::SetArmed(fn.Fn(),&AfterMomentum,false);
}

void RunRagdollTransitionAcceptance(void* local) {
    struct Case {const char* name;FRotator look;bool crouch,force,turnAfter;};
    const Case cases[]={{"normal",{0,-90,0},false,false,false},{"down-wrapped",{-35,385,0},false,false,true},
        {"crouched",{30,-170,0},true,false,false},{"forced",{25,65,0},false,true,true}};
    int passed=0,total=0;
    for(const char* skin:{"sci_v1sc","kerfur_omega_h"}){
        coop::local_body::RequestSkin(skin);::Sleep(1500);
        for(const auto& item:cases){
            struct Check {ue_wrap::CachedObjRef pawn;bool entered=false,instant=false,recovered=false,turned=false;
                float wanted=0,finishError=999,drift=0,stableYaw=0;int stable=0;};
            auto s=std::make_shared<Check>();
            if(!Game([s,local,item]{s->pawn.Set(local);void* p=s->pawn.Get();if(!p)return;
                E::RefreshLocalRagdollModel(p);
                ue_wrap::ParamFrame crouch(R::FindDispatchFunctionCached(R::ClassOf(p),item.crouch?L"Crouch":L"UnCrouch"));
                if(crouch.Set<bool>(L"bClientSimulation",false))ue_wrap::Call(p,crouch);
                E::SetControlRotation(E::GetController(p),item.look);
            }))return;
            ::Sleep(300);
            if(!Game([s,item]{void* p=s->pawn.Get();if(!p)return;
                E::SetControlRotation(E::GetController(p),item.look);E::SetComponentWorldRotation(Component(p,L"lag"),item.look);
                s->entered=E::SetMainPlayerRagdollMode(p,true,false,false);
                void* mesh=E::GetLocalRagdollBodyMesh(p);
                if(!mesh)return;
                ue_wrap::ParamFrame sim(R::FindDispatchFunctionCached(R::ClassOf(mesh),L"IsAnySimulatingPhysics"));
                s->instant=mesh && ue_wrap::puppet::GetComponentSkeletalMeshAsset(mesh)==
                    ue_wrap::puppet::GetComponentSkeletalMeshAsset(ue_wrap::puppet::GetMeshPlayerVisibleComponent(p)) &&
                    sim.valid() && ue_wrap::Call(mesh,sim) && sim.Get<bool>(L"ReturnValue");
            }))return;
            ::Sleep(1100);
            if(!Game([s,item]{void* p=s->pawn.Get();if(!p)return;
                s->wanted=E::GetComponentWorldRotation(Component(p,L"lag")).Yaw;
                if(item.force)E::ForceMainPlayerWakeup(p);else E::SetMainPlayerRagdollMode(p,false,false,false);
            }))return;
            for(int sample=0;sample<150;++sample){
                ::Sleep(25);
                if(!Game([s,item]{void* p=s->pawn.Get();bool rag=true,dead=true;
                    if(!p || !E::ReadMainPlayerRagdollState(p,rag,dead) || rag || dead || Flag(p,L"isWakingUp",true))return;
                    const float yaw=E::GetComponentWorldRotation(Component(p,L"lag")).Yaw;
                    if(!s->stable){s->finishError=YawError(yaw,s->wanted);s->stableYaw=yaw;s->recovered=true;}
                    else s->drift=(std::max)(s->drift,YawError(yaw,s->stableYaw));
                    ++s->stable;
                    if(item.turnAfter && s->stable==20){
                        s->stableYaw=ue_wrap::NormalizeAxis(yaw+37.f);
                        E::SetControlRotation(E::GetController(p),{0,s->stableYaw,0});s->turned=true;
                    }
                }))return;
            }
            const bool ok=s->entered&&s->instant&&s->recovered&&s->stable>65&&s->finishError<0.3f&&s->drift<0.3f&&(!item.turnAfter||s->turned);
            ++total;if(ok)++passed;
            UE_LOGI("ragdoll_transition_case: %s skin=%s case=%s instant=%d recovered=%d stable=%d finish-yaw=%.3f drift=%.3f turned=%d",
                ok?"PASS":"FAIL",skin,item.name,s->instant,s->recovered,s->stable,s->finishError,s->drift,s->turned);
        }
    }
    UE_LOGI("ragdoll_transition_acceptance: %s %d/%d",passed==8&&total==8?"PASS":"FAIL",passed,total);
}
}
