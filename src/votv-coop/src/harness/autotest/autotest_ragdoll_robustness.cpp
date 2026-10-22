// Opt-in isolated actual-game acceptance: contact surfaces, installed rigs and
// remote sleep/sit/ragdoll transitions. No normal-game behavior is changed here.
#include "harness/autotest.h"
#include "coop/player/local_body.h"
#include "coop/player/client_model.h"
#include "coop/player/remote_player.h"
#include "coop/player/players_registry.h"
#include "coop/config/config.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/actors/sleep_display.h"
#include "ue_wrap/actors/sit_pose.h"
#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/trace.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"
#include <atomic>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace harness::autotest {
namespace {
namespace E = ue_wrap::engine;
namespace R = ue_wrap::reflection;
using ue_wrap::FVector;
using ue_wrap::FRotator;
UE_FACT_FN(kSurfaceOpenLevel,"ragdoll_surface_probe",L"GameplayStatics",L"OpenLevel",Declared);
void LogTravel(void*,void* source,void*) {
    const auto frame=ue_wrap::ufunction_hook::CurrentCallerFrame();
    R::FName level{};
    const int offset=frame.function?R::FindPropertyOffset(frame.function,L"K2Node_CustomEvent_LevelName"):-1;
    if(frame.locals && offset>=0)std::memcpy(&level,frame.locals+offset,sizeof(level));
    bool rag=false,dead=false;void* player=coop::players::Registry::Get().Local();
    if(player)E::ReadMainPlayerRagdollState(player,rag,dead);
    UE_LOGW("ragdoll_surface_travel: level='%ls' rag=%d dead=%d source='%ls' script='%ls' offset=%d",
        R::ToString(level).c_str(),rag,dead,
        source&&R::IsLive(source)?R::ClassNameOf(source).c_str():L"<none>",
        frame.function?R::ToString(R::NameOf(frame.function)).c_str():L"<native>",frame.codeOffset);
}

template<class F> bool Game(F action) {
    auto done = std::make_shared<std::atomic<bool>>(false);
    ue_wrap::game_thread::Post([action, done] { action(); done->store(true); });
    for (int ms = 0; ms < 8000 && !done->load(); ms += 5) ::Sleep(5);
    return done->load();
}
void* Field(void* object, const wchar_t* name) {
    if (!object || !R::IsLive(object)) return nullptr;
    const int offset = R::FindPropertyOffset(R::ClassOf(object), name);
    void* out = nullptr;
    if (offset >= 0) std::memcpy(&out, static_cast<uint8_t*>(object)+offset, sizeof(out));
    return out && R::IsLive(out) ? out : nullptr;
}
void* Fn(void* object, const wchar_t* name) { return R::FindDispatchFunctionCached(R::ClassOf(object), name); }
bool Finite(const FVector& p) { return std::isfinite(p.X) && std::isfinite(p.Y) && std::isfinite(p.Z); }
void* ExpectedMesh(const std::string& skin) {
    return coop::client_model::IsNativeSkin(skin)?coop::local_body::NativeBodyMesh():coop::client_model::GetSkinMesh(skin);
}
bool MovePlayer(void* pawn, const FVector& position) {
    // The game's camera ray traces from lastLoc. Raw test teleports across a
    // solid platform look like wall clipping and legitimately enter Backrooms.
    // Use the native teleport verb, which updates that history as well.
    ue_wrap::FTransform transform;
    transform.TX=position.X;transform.TY=position.Y;transform.TZ=position.Z;
    transform.SX=transform.SY=transform.SZ=0.f; // this verb stores movement velocity in Scale
    ue_wrap::ParamFrame frame(Fn(pawn,L"teleportWObackrooms"));
    return frame.Set<ue_wrap::FTransform>(L"NewTransform",transform) &&
        frame.Set<bool>(L"useRotation",false) && frame.Set<bool>(L"trueRotation",false) && ue_wrap::Call(pawn,frame);
}
struct Check {
    ue_wrap::CachedObjRef pawn, platform, platformMesh;
    coop::RemotePlayer mirror;
    FVector home{}, centre{}, entry{}, last{};
    float rise = -1.e9f, speed = 0.f, slide = 0.f, platformMoved = 0.f;
    int samples = 0, matched = 0, contacts = 0;
    bool setup = false, entered = false, recovered = false, stable = true;
};
bool MakePlatform(const std::shared_ptr<Check>& s, float pitch) {
    void* cls = R::FindClass(L"StaticMeshActor");
    void* cube = ue_wrap::asset_load::LoadObjectByPath(L"/Engine/BasicShapes/Cube.Cube");
    if (!cls || !cube) return false;
    // Keep the original safe XY and place the contact fixture above scenery.
    s->centre = {s->home.X, s->home.Y, s->home.Z + 250.f};
    void* actor = E::BeginDeferredSpawn(cls, s->centre, {pitch,0,0});
    if (!actor) return false;
    s->platform.Set(actor);
    void* mesh = Field(actor, L"StaticMeshComponent");
    if (!mesh || !E::FinishDeferredSpawn(actor, s->centre, {pitch,0,0})) return false;
    s->platformMesh.Set(mesh);
    ue_wrap::ParamFrame mobility(Fn(mesh,L"SetMobility"));
    ue_wrap::ParamFrame asset(Fn(mesh,L"SetStaticMesh"));
    ue_wrap::ParamFrame collision(Fn(mesh,L"SetCollisionEnabled"));
    ue_wrap::ParamFrame response(Fn(mesh,L"SetCollisionResponseToAllChannels"));
    if (!mobility.Set<uint8_t>(L"NewMobility",2) || !ue_wrap::Call(mesh,mobility) ||
        !asset.Set<void*>(L"NewMesh",cube) || !ue_wrap::Call(mesh,asset) ||
        !collision.Set<uint8_t>(L"NewType",3) || !ue_wrap::Call(mesh,collision) ||
        !response.Set<uint8_t>(L"NewResponse",2) || !ue_wrap::Call(mesh,response) ||
        !E::SetActorScale3D(actor,{20.f,20.f,.4f})) return false;
    ue_wrap::trace::Hit hit;
    const FVector start{s->centre.X,s->centre.Y,s->centre.Z+800.f};
    const FVector end{s->centre.X,s->centre.Y,s->centre.Z-800.f};
    if (!ue_wrap::trace::LineHitStatDyn(s->pawn.Get(),start,end,&hit) || !hit.blocked || hit.actor!=actor) return false;
    s->entry = {hit.point.X,hit.point.Y,hit.point.Z+E::GetActorCharacterHalfHeight(s->pawn.Get())+5.f};
    UE_LOGI("ragdoll_surface_setup: slope=%.1f normal=(%.3f,%.3f,%.3f) actor=%p home=(%.1f,%.1f,%.1f) entry=(%.1f,%.1f,%.1f)",pitch,hit.normal.X,hit.normal.Y,hit.normal.Z,actor,s->home.X,s->home.Y,s->home.Z,s->entry.X,s->entry.Y,s->entry.Z);
    return MovePlayer(s->pawn.Get(),s->entry);
}
void Sample(const std::shared_ptr<Check>& s, bool moving, int tick) {
    void* pawn=s->pawn.Get();
    if (!pawn || !E::RefreshLocalRagdollModel(pawn)) return;
    if (moving && s->platformMesh.Get()) {
        ue_wrap::ParamFrame move(Fn(s->platformMesh.Get(),L"K2_SetWorldLocation"));
        move.Set<FVector>(L"NewLocation",{s->centre.X+tick*5.f,s->centre.Y,s->centre.Z});
        move.Set<bool>(L"bSweep",false); move.Set<bool>(L"bTeleport",false);
        s->stable = ue_wrap::Call(s->platformMesh.Get(),move) && s->stable;
        FVector actual{};
        s->stable = E::TryGetActorLocation(s->platform.Get(),actual) && std::fabs(actual.X-s->centre.X-tick*5.f)<1.f && s->stable;
        s->platformMoved=(std::max)(s->platformMoved,std::fabs(actual.X-s->centre.X));
    }
    FVector at{},linear{},angular{}; FRotator rot{};
    std::vector<E::ArticulatedBone> bones;
    void* mesh=E::GetLocalRagdollBodyMesh(pawn);
    if (!E::ReadLocalRagdollPelvisPhysics(pawn,at,rot,linear,angular) || !E::ReadArticulatedPose(mesh,bones)) return;
    ++s->samples; s->last=at;
    if (s->samples <= 2) UE_LOGI("ragdoll_surface_sample: at=(%.1f,%.1f,%.1f) entryZ=%.1f velocity=(%.1f,%.1f,%.1f)",at.X,at.Y,at.Z,s->entry.Z,linear.X,linear.Y,linear.Z);
    s->stable = s->stable && Finite(at) && Finite(linear) && Finite(angular);
    s->rise=(std::max)(s->rise,at.Z-s->entry.Z);
    s->speed=(std::max)(s->speed,std::sqrt(linear.X*linear.X+linear.Y*linear.Y+linear.Z*linear.Z));
    s->slide=(std::max)(s->slide,std::hypot(at.X-s->entry.X,at.Y-s->entry.Y));
    ue_wrap::trace::Hit below;
    if (ue_wrap::trace::LineHitStatDyn(pawn,{at.X,at.Y,at.Z+100.f},{at.X,at.Y,at.Z-220.f},&below) &&
        (s->platform.Get()?below.actor==s->platform.Get():below.blocked&&below.normal.Z>.5f)) ++s->contacts;
    void* standing=ue_wrap::puppet::GetMeshPlayerVisibleComponent(pawn);
    if (ue_wrap::puppet::GetComponentSkeletalMeshAsset(standing)==ue_wrap::puppet::GetComponentSkeletalMeshAsset(mesh)) ++s->matched;
}
void RemoteTransitions(const std::shared_ptr<Check>& s, const char* skin) {
    auto result=std::make_shared<int>(0);
    if (!Game([s,result] {
        coop::net::PoseSnapshot pose{};pose.x=s->home.X;pose.y=s->home.Y;pose.z=s->home.Z;
        pose.stateBits=coop::net::kStateBitSleeping|coop::net::kStateBitRagdoll;
        s->mirror.SetTargetPose(pose);s->mirror.Tick();
        if (!s->mirror.RagdollBody() && ue_wrap::sleep_display::Mesh(s->mirror.GetActor())) *result|=1;
        pose.stateBits=coop::net::kStateBitSitting;
        s->mirror.SetTargetPose(pose);s->mirror.Tick();
        if (!s->mirror.RagdollBody() && !ue_wrap::sleep_display::Mesh(s->mirror.GetActor()) && ue_wrap::sit_pose::ReadIsOnAtv(s->mirror.GetActor())) *result|=2;
        pose.stateBits=coop::net::kStateBitRagdoll|coop::net::kStateBitSitting;
        s->mirror.SetTargetPose(pose);s->mirror.Tick();
        if(s->mirror.RagdollBody())*result|=4;
    })) return;
    ::Sleep(60);
    if (!Game([s,result] {
        s->mirror.Tick();
        coop::net::PoseSnapshot pose{};pose.x=s->home.X;pose.y=s->home.Y;pose.z=s->home.Z;
        pose.stateBits=coop::net::kStateBitSleeping;
        s->mirror.SetTargetPose(pose);s->mirror.Tick();
        if (!s->mirror.RagdollBody() && ue_wrap::sleep_display::Mesh(s->mirror.GetActor())) *result|=8;
        pose.stateBits=0;s->mirror.SetTargetPose(pose);s->mirror.Tick();
        if (!s->mirror.RagdollBody() && !ue_wrap::sleep_display::Mesh(s->mirror.GetActor())) *result|=16;
    })) return;
    UE_LOGI("ragdoll_transition_case: %s skin=%s remote sleep->sit->ragdoll->sleep->stand checks=%d/31",*result==31?"PASS":"FAIL",skin,*result);
}
}

void RunRagdollRobustness(void* local, bool catalogOnly) {
    Game([]{ue_wrap::ufunction_hook::InstallPostHook(ue_wrap::facts::Resolve(kSurfaceOpenLevel),&LogTravel);});
    struct Surface {const char* name;float pitch;bool moving;};
    const Surface surfaces[]={{"flat",0,false},{"slope25",25,false},{"slope35",35,false},{"moving",0,true}};
    std::vector<std::string> skins={"sci_v1sc","walter_v1sc","rvi_scientist_v1sc","luther_v1sc","kerfur_omega_h","kerfur_omega_m"};
    const bool modelsOnly=catalogOnly || coop::config::ReadEnv("VOTVCOOP_RAGDOLL_MODELS_ONLY")=="1";
    const auto configured=modelsOnly?coop::config::ReadEnv("VOTVCOOP_RAGDOLL_TEST_SKINS"):std::string{};
    if(!configured.empty()) {
        skins.clear();std::stringstream input(configured);std::string skin;
        while(std::getline(input,skin,','))if(!skin.empty())skins.push_back(skin);
    }
    int passed=0,total=0;
    for(const auto& skinName:skins) {
        const char* skin=skinName.c_str();
        coop::local_body::RequestSkin(skin);::Sleep(1200);
        for(const auto& surface:surfaces) {
            if(modelsOnly && surface.pitch!=0.f)continue;
            if(modelsOnly && surface.moving)continue;
            auto s=std::make_shared<Check>();
            if(!Game([s,local,surface,skinName,modelsOnly] {
                s->pawn.Set(local);
                if (!s->pawn.Get() || !E::TryGetActorLocation(local,s->home) || coop::local_body::LocalSkinName()!=skinName) return;
                void* expected=ExpectedMesh(skinName);
                void* actual=ue_wrap::puppet::GetComponentSkeletalMeshAsset(ue_wrap::puppet::GetMeshPlayerVisibleComponent(local));
                if(!expected || actual!=expected) {
                    UE_LOGW("ragdoll_surface_model: requested '%s' is not the applied native mesh",skinName.c_str()); return;
                }
                if(modelsOnly) {
                    // Catalog/appearance acceptance on the native ground; the
                    // separate matrix owns artificial contact fixtures.
                    s->entry=s->home;ue_wrap::trace::Hit floor;
                    s->setup=ue_wrap::trace::LineHitStatDyn(local,s->home,{s->home.X,s->home.Y,s->home.Z-350.f},&floor)&&floor.blocked&&floor.normal.Z>.5f;
                } else s->setup=MakePlatform(s,surface.pitch);
            })) return;
            ::Sleep(150);
            if(!Game([s] {if(s->setup)s->entered=E::SetMainPlayerRagdollMode(s->pawn.Get(),true,false,false);}))return;
            for(int tick=0;tick<36 && s->entered;++tick) {::Sleep(50);if(!Game([s,surface,tick]{Sample(s,surface.moving,tick);}))return;}
            if(!Game([s]{if(s->pawn.Get())E::ForceMainPlayerWakeup(s->pawn.Get());}))return;
            ::Sleep(500);
            if(!Game([s]{bool rag=true,dead=true;void* pawn=s->pawn.Get();
                s->recovered=pawn&&E::ReadMainPlayerRagdollState(pawn,rag,dead)&&!rag&&!dead&&!E::GetLocalRagdollBodyMesh(pawn);
                if(s->platform.Get())E::DestroyActor(s->platform.Get());
            }))return;
            // Let destruction remove the fixture's PhysX shape before moving
            // across where it was. A pending collider can still answer traces.
            ::Sleep(150);
            if(!Game([s]{if(void* pawn=s->pawn.Get())MovePlayer(pawn,s->home);}))return;
            const bool ok=s->setup&&s->entered&&s->recovered&&s->stable&&s->samples>=15&&s->matched>=10&&s->contacts>=10&&s->rise<180.f&&s->speed<3000.f&&(!surface.moving||s->platformMoved>140.f);
            ++total;if(ok)++passed;
            UE_LOGI("ragdoll_surface_case: %s skin=%s case=%s setup=%d entered=%d recovered=%d samples=%d selected=%d contact=%d rise=%.1f speed=%.1f slide=%.1f platform-move=%.1f",
                ok?"PASS":"FAIL",skin,surface.name,s->setup,s->entered,s->recovered,s->samples,s->matched,s->contacts,s->rise,s->speed,s->slide,s->platformMoved);
            if(surface.moving || modelsOnly) { if(!Game([s,skinName]{s->mirror.Spawn(skinName,1);}))return; RemoteTransitions(s,skin); }
            if(!Game([s]{s->mirror.Destroy();coop::players::Registry::Get().UnregisterPuppet(1);}))return;
            ::Sleep(300);
        }
    }
    const int expected=int(skins.size())*(modelsOnly?1:4);
    UE_LOGI("ragdoll_%s_acceptance: %s %d/%d",modelsOnly?"catalog":"surface",passed==total&&total==expected?"PASS":"FAIL",passed,total);
}
}
