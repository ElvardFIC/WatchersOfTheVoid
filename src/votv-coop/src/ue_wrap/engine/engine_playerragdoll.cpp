// ue_wrap/engine/engine_playerragdoll.cpp -- AplayerRagdoll_C engine substrate (principle 7).
//
// playerRagdoll_C supplies the actor and native interaction shell. The selected
// player mesh/materials and full compatible physics asset replace its plushie
// defaults after construction, before any simulation. Native ragdollMode is never
// invoked on a puppet because it can affect the local player's death state.

#include "ue_wrap/engine/engine.h"
#include "ue_wrap/actors/puppet.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/ufunction_hook.h"

#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <chrono>
#include <map>

namespace ue_wrap::engine {
bool RebuildPlayerRagdollPhysics(void* target, void* physics);
namespace {

namespace R = reflection;

// The three fields this file reads raw, from the CXX dump and confirmed by
// autotest_ragdoll_spawn_probe.cpp: AplayerRagdoll_C::Player (AmainPlayer_C*),
// Aragdoll_C::SkeletalMesh (the body mesh), and AmainPlayer_C::ragdollActor -- the NATIVE ragdoll
// the C key or a faint spawns, whose pelvis physics the sender samples.
constexpr size_t kPlayerRagdoll_Player  = 0x0248;
constexpr size_t kAragdoll_SkeletalMesh = 0x0230;
constexpr size_t kMainPlayer_ragdollActor = 0x0C40;

// Cached UClass + sim UFunctions (resolve once; playerRagdoll_C loads with
// gameplay and is always resident per the probe's residency check). Plain void*
// caches -- no atomics/mutex (the incremental-link DLL-corruption rule does not
// structurally trigger), consistent with the engine_mainplayer.cpp precedent.
ue_wrap::CachedObjRef g_ragdollClass;
// Pelvis reads: the sender samples its native ragdoll's pelvis location, rotation and velocity for
// the wire (ReadLocalRagdollPelvisPhysics). The "pelvis" FName is one GNames entry, stable across
// every ragdoll body, so the eight bytes are cached once alongside the functions.
void* g_getSocketRotFn   = nullptr;
uint8_t g_pelvisFName[8] = {};
bool g_havePelvisFName   = false;
// The physics-sync caches, resolved once: SceneComponent::GetSocketLocation plus the four
// UPrimitiveComponent velocity operations.
void* g_getSocketLocFn = nullptr;
void* g_getLinVelFn    = nullptr;  // UPrimitiveComponent::GetPhysicsLinearVelocity
void* g_getAngVelFn    = nullptr;  // ::GetPhysicsAngularVelocityInDegrees
void* g_setLinVelFn    = nullptr;  // ::SetPhysicsLinearVelocity
void* g_setAngVelFn    = nullptr;  // ::SetPhysicsAngularVelocityInDegrees

void* ResolveRagdollClass() {
    if (g_ragdollClass.Alive()) return g_ragdollClass.Raw();
    g_ragdollClass.Set(R::FindClass(L"playerRagdoll_C"));
    return g_ragdollClass.Raw();
}

// Start PhysX gravity simulation on the ragdoll body's skeletal-mesh component.
// This is the step ragdollMode does AFTER spawn that AplayerRagdoll_C::BeginPlay
// does NOT (the bare body stays frozen at the spawn transform). Proven by the SP
// probe: with this the body falls to the floor + settles; without it, rigid.
// Game thread only.
bool StartBodySim(void* comp) {
    CachedObjRef ref; ref.Set(comp);
    if (!ref.Get()) return false;
    void* cls = R::ClassOf(comp);
    // Recreate bodies from the selected-model pose before enabling simulation.
    // Construction uses SetSkeletalMesh's initialized reference pose immediately;
    // a late swap can also arrive here after the component's evaluation tick.
    const int physicsOffset = R::FindPropertyOffset(cls, L"PhysicsAssetOverride");
    void* physics = nullptr;
    if (physicsOffset < 0) return false;
    std::memcpy(&physics, static_cast<uint8_t*>(comp) + physicsOffset, sizeof(physics));
    CachedObjRef physicsRef; physicsRef.Set(physics);
    if (!physicsRef.Get()) return false;
    if (!RebuildPlayerRagdollPhysics(comp, physics) || !ref.Get() || !physicsRef.Get()) return false;
    ParamFrame collision(R::FindDispatchFunctionCached(cls, L"SetCollisionEnabled"));
    if (!collision.Set<uint8_t>(L"NewType", 3) || !Call(comp, collision) || !ref.Get()) return false;
    ParamFrame bodies(R::FindDispatchFunctionCached(cls, L"SetAllBodiesSimulatePhysics"));
    if (!bodies.Set<bool>(L"bNewSimulate", true) || !Call(comp, bodies) || !ref.Get()) return false;
    ParamFrame simulation(R::FindDispatchFunctionCached(cls, L"SetSimulatePhysics"));
    if (!simulation.Set<bool>(L"bSimulate", true) || !Call(comp, simulation) || !ref.Get()) return false;
    ParamFrame blend(R::FindDispatchFunctionCached(cls, L"SetAllBodiesPhysicsBlendWeight"));
    if (!blend.Set<float>(L"PhysicsBlendWeight", 1.f) || !blend.Set<bool>(L"bSkipCustomPhysicsType", false) ||
        !Call(comp, blend) || !ref.Get()) return false;
    // Body simulation and skinning are separate switches. The replacement rig
    // uses a single-node reference pose, so explicitly give physics ownership
    // of the rendered bones and ensure its component can refresh them.
    ParamFrame render(R::FindDispatchFunctionCached(cls, L"SetEnablePhysicsBlending"));
    if(!render.Set<bool>(L"bNewBlendPhysics",true) || !Call(comp,render) || !ref.Get() ||
       !SetComponentTickEnabled(comp,true) || !ref.Get())return false;
    ParamFrame check(R::FindDispatchFunctionCached(cls, L"IsAnySimulatingPhysics"));
    return check.valid() && Call(comp, check) && ref.Get() && check.Get<bool>(L"ReturnValue");
}

// Find the 8-byte FName of the bone named `wantName` on a skeletal-mesh component
// (enumerate bones + match by string -- we can't construct an FName for an arbitrary
// string without the engine's name table). Writes NAME_None (zeros) into outName and
// returns false if not found. Game thread only.
bool FindBoneFName(void* meshComp, const wchar_t* wantName, uint8_t outName[8]) {
    std::memset(outName, 0, 8);
    if (!meshComp || !R::IsLive(meshComp)) return false;
    void* sk = R::FindClass(L"SkinnedMeshComponent");
    if (!sk) return false;
    void* numFn  = R::FindFunction(sk, L"GetNumBones");
    void* nameFn = R::FindFunction(sk, L"GetBoneName");
    if (!numFn || !nameFn) return false;
    int32_t n = 0;
    { ParamFrame f(numFn); if (Call(meshComp, f)) n = f.Get<int32_t>(L"ReturnValue"); }
    for (int32_t i = 0; i < n; ++i) {
        uint8_t name[8] = {};
        ParamFrame nf(nameFn); nf.Set<int32_t>(L"BoneIndex", i);
        if (!Call(meshComp, nf)) continue;
        nf.GetRaw(L"ReturnValue", name, sizeof(name));
        if (R::ToString(*reinterpret_cast<const R::FName*>(name)) == wantName) {
            std::memcpy(outName, name, 8);
            return true;
        }
    }
    return false;
}

// Ensure g_pelvisFName holds the "pelvis" bone FName (resolve once from any ragdoll
// mesh -- the name is one GNames entry, identical across the sender's native ragdoll
// and our spawned mirror body). Returns false until it resolves. Game thread.
bool EnsurePelvisFName(void* mesh) {
    if (g_havePelvisFName) return true;
    g_havePelvisFName = FindBoneFName(mesh, L"pelvis", g_pelvisFName);
    return g_havePelvisFName;
}

// Resolve the SceneComponent::GetSocketLocation + the 4 UPrimitiveComponent velocity
// UFunctions once. Returns false if the engine classes/functions can't be found.
bool ResolvePhysicsFns() {
    if (!g_getSocketLocFn) {
        if (void* sc = R::FindClass(L"SceneComponent")) g_getSocketLocFn = R::FindFunction(sc, L"GetSocketLocation");
    }
    if (!g_getSocketRotFn) {
        if (void* sc = R::FindClass(L"SceneComponent")) g_getSocketRotFn = R::FindFunction(sc, L"GetSocketRotation");
    }
    // Resolve the 4 UPrimitiveComponent velocity ops. Cache the class pointer once so
    // a single still-unresolved fn (e.g. a transient FindFunction miss) re-runs only
    // its own FindFunction, NOT a fresh FindClass GUObjectArray walk every frame (the
    // per-pointer-independent guard pattern the sibling SceneComponent lookups use).
    if (!g_getLinVelFn || !g_getAngVelFn || !g_setLinVelFn || !g_setAngVelFn) {
        static ue_wrap::CachedObjRef sPrimCompCls;
        if (!sPrimCompCls.Alive()) sPrimCompCls.Set(R::FindClass(L"PrimitiveComponent"));
        if (void* sPrimCls = sPrimCompCls.Raw()) {
            if (!g_getLinVelFn) g_getLinVelFn = R::FindFunction(sPrimCls, L"GetPhysicsLinearVelocity");
            if (!g_getAngVelFn) g_getAngVelFn = R::FindFunction(sPrimCls, L"GetPhysicsAngularVelocityInDegrees");
            if (!g_setLinVelFn) g_setLinVelFn = R::FindFunction(sPrimCls, L"SetPhysicsLinearVelocity");
            if (!g_setAngVelFn) g_setAngVelFn = R::FindFunction(sPrimCls, L"SetPhysicsAngularVelocityInDegrees");
        }
    }
    return g_getSocketLocFn && g_getSocketRotFn &&
           g_getLinVelFn && g_getAngVelFn && g_setLinVelFn && g_setAngVelFn;
}

// Resolve a ragdoll actor's body SkeletalMesh (kAragdoll_SkeletalMesh, live-checked). null on miss.
void* RagdollMeshOf(void* ragdollActor) {
    if (!ragdollActor || !R::IsLive(ragdollActor)) return nullptr;
    void* mesh = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ragdollActor) + kAragdoll_SkeletalMesh);
    return (mesh && R::IsLive(mesh)) ? mesh : nullptr;
}

// Native ragdollMode attaches the camera spring arm to the plushie's head and
// installs a fixed (0,-90,0) relative rotation. A selected character's head has
// different axes. Keep the entry look while swapping/evaluating that skeleton,
// then leave the relative attachment alone so the camera follows the real flop.
// The controller still contains the pre-entry look, even after native ragdollMode.
struct RagdollView {
    CachedObjRef player, mesh, arm;
    FRotator entry{};

    void Reset() { player.Reset(); mesh.Reset(); arm.Reset(); }

    bool Attached() const {
        void* component = arm.Get();
        if (!player.Get() || !mesh.Get() || !component) return false;
        ParamFrame parent(R::FindDispatchFunctionCached(R::ClassOf(component), L"GetAttachParent"));
        void* value = nullptr;
        return parent.valid() && Call(component, parent) &&
            parent.GetRaw(L"ReturnValue", &value, sizeof(value)) &&
            player.Get() && mesh.Get() && arm.Get() && value == mesh.Get();
    }

    bool Capture(void* owner, void* bodyMesh, bool beforeAttachment = false) {
        Reset();
        player.Set(owner); mesh.Set(bodyMesh);
        if (!player.Get() || !mesh.Get()) return false;
        const int offset = R::FindPropertyOffset(R::ClassOf(owner), L"lag");
        if (offset < 0) return false;
        void* component = nullptr;
        std::memcpy(&component, static_cast<uint8_t*>(owner) + offset, sizeof(component));
        arm.Set(component);
        if (!arm.Get() || (!beforeAttachment && !Attached())) { Reset(); return false; }
        CachedObjRef controller; controller.Set(GetController(owner));
        if (!controller.Get()) { Reset(); return false; }
        ParamFrame rotation(R::FindDispatchFunctionCached(R::ClassOf(controller.Get()), L"GetControlRotation"));
        if (!rotation.valid() || !Call(controller.Get(), rotation) || !controller.Get() ||
            !rotation.GetRaw(L"ReturnValue", &entry, sizeof(entry)) ||
            !std::isfinite(entry.Pitch) || !std::isfinite(entry.Yaw) || !std::isfinite(entry.Roll) ||
            (!beforeAttachment && !Attached())) { Reset(); return false; }
        return true;
    }

    bool Restore() const {
        // Recovery can reparent the arm before ragdollActor is cleared. Never
        // write to a recovered player's camera or a recycled component slot.
        if (!Attached()) return false;
        void* component = arm.Get();
        ParamFrame rotation(R::FindDispatchFunctionCached(R::ClassOf(component), L"K2_SetWorldRotation"));
        return rotation.Set<FRotator>(L"NewRotation", entry) &&
            rotation.Set<bool>(L"bSweep", false) && rotation.Set<bool>(L"bTeleport", true) &&
            Call(component, rotation) && Attached();
    }
};

UE_FACT_FN(kRagdollLinearVelocity, "ragdoll_momentum", L"PrimitiveComponent", L"SetPhysicsLinearVelocity", Declared);
struct BodyVelocity { R::FName bone{}; FVector linear{}, angular{}; bool linearDone=false; };
struct VelocityCommand { R::FName bone{}; FVector linear{}; bool add=false; };
CachedObjRef g_velocityMesh;
std::vector<BodyVelocity> g_bodyVelocities;
std::vector<VelocityCommand> g_velocityCommands;
size_t g_bodyVelocityIndex=0, g_velocityCommandIndex=0;
void* g_velocityFunction = nullptr;
int g_velocityOffset=-1, g_addOffset=-1, g_boneOffset=-1;
bool g_replayingVelocity = false;
FVector g_baseLinear{};
bool g_baseLinearDone=false;
bool FiniteVelocity(const FVector& v) { return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z); }

script_gate::Verdict HoldRagdollVelocity(void* component, void*, const uint8_t* params) {
    if (!params || g_replayingVelocity || !component || component!=g_velocityMesh.Get()) return script_gate::Verdict::Run;
    R::FName bone{}; FVector velocity{}; bool add=false;
    std::memcpy(&bone,params+g_boneOffset,sizeof(bone));
    std::memcpy(&velocity,params+g_velocityOffset,sizeof(velocity));
    std::memcpy(&add,params+g_addOffset,sizeof(add));
    if (!FiniteVelocity(velocity) || g_velocityCommands.size()>=4096) return script_gate::Verdict::Run;
    g_velocityCommands.push_back({bone,velocity,add});
    // fallImpulse runs after Delay(0), often while SetSkeletalMesh's bodies are
    // being replaced. Preserve that actual native command instead of guessing
    // velocity from a stopped CharacterMovement or adding it a second time.
    return script_gate::Verdict::Cancel;
}

void CancelRagdollVelocity() {
    if (g_velocityFunction) ufunction_hook::SetArmed(g_velocityFunction,&HoldRagdollVelocity,false);
    g_velocityMesh.Reset(); g_bodyVelocities.clear(); g_replayingVelocity=false;
    g_velocityCommands.clear(); g_bodyVelocityIndex=0; g_velocityCommandIndex=0;
    g_baseLinear={}; g_baseLinearDone=false;
}

bool BeginRagdollVelocity(void* mesh) {
    CancelRagdollVelocity();
    CachedObjRef ref; ref.Set(mesh);
    if (!ref.Get() || !ResolvePhysicsFns()) return false;
    const auto fn=facts::Resolve(kRagdollLinearVelocity);
    ParamFrame signature(fn.Fn());
    g_velocityOffset=signature.ParamOffset(L"NewVel"); g_addOffset=signature.ParamOffset(L"bAddToCurrent");
    g_boneOffset=signature.ParamOffset(L"BoneName");
    if (g_velocityOffset<0 || g_addOffset<0 || g_boneOffset<0 ||
        g_velocityOffset+int(sizeof(FVector))>signature.FrameSize() ||
        g_addOffset+int(sizeof(bool))>signature.FrameSize() || g_boneOffset+int(sizeof(R::FName))>signature.FrameSize()) return false;
    void* cls=R::ClassOf(mesh);
    ParamFrame count(R::FindDispatchFunctionCached(cls,L"GetNumBones")); int32_t n=0;
    if (!Call(mesh,count) || !count.GetRaw(L"ReturnValue",&n,sizeof(n)) || n<1 || n>1024) return false;
    for (int32_t i=0;i<n;++i) {
        if (!ref.Get()) {CancelRagdollVelocity();return false;}
        BodyVelocity entry;
        ParamFrame name(R::FindDispatchFunctionCached(cls,L"GetBoneName"));
        ParamFrame linear(g_getLinVelFn), angular(g_getAngVelFn);
        if (!name.Set<int32_t>(L"BoneIndex",i) || !Call(mesh,name) || !name.GetRaw(L"ReturnValue",&entry.bone,sizeof(entry.bone)) ||
            !linear.Set<R::FName>(L"BoneName",entry.bone) || !Call(mesh,linear) || !linear.GetRaw(L"ReturnValue",&entry.linear,sizeof(entry.linear)) ||
            !angular.Set<R::FName>(L"BoneName",entry.bone) || !Call(mesh,angular) || !angular.GetRaw(L"ReturnValue",&entry.angular,sizeof(entry.angular)) ||
            !ref.Get() || !FiniteVelocity(entry.linear) || !FiniteVelocity(entry.angular)) {CancelRagdollVelocity();return false;}
        g_bodyVelocities.push_back(entry);
        if (R::ToString(entry.bone)==L"pelvis") g_baseLinear=entry.linear;
    }
    g_velocityFunction=fn.Fn();
    if (!ufunction_hook::InstallPreHook(fn,&HoldRagdollVelocity,false)) {CancelRagdollVelocity();return false;}
    g_velocityMesh.Set(mesh);
    if (!ufunction_hook::SetArmed(g_velocityFunction,&HoldRagdollVelocity,true)) {CancelRagdollVelocity();return false;}
    return true;
}

bool FinishRagdollVelocity(void* mesh) {
    if (g_velocityMesh.Get()!=mesh) {CancelRagdollVelocity();return false;}
    struct ReplayGuard { ReplayGuard(){g_replayingVelocity=true;} ~ReplayGuard(){g_replayingVelocity=false;} } guard;
    // If fallImpulse already ran on the stock body before the first refresh,
    // its pelvis carries the common translation. Give that to NEW limb bodies
    // too, then restore per-bone differences such as the native chest kick.
    if (!g_baseLinearDone) {
        ParamFrame base(R::FindDispatchFunctionCached(R::ClassOf(mesh),L"SetAllPhysicsLinearVelocity"));
        if (!base.Set<FVector>(L"NewVel",g_baseLinear) || !base.Set<bool>(L"bAddToCurrent",false) ||
            !Call(mesh,base) || !g_velocityMesh.Get()) return false;
        g_baseLinearDone=true;
    }
    while (g_bodyVelocityIndex<g_bodyVelocities.size()) {
        auto& entry=g_bodyVelocities[g_bodyVelocityIndex]; int32_t index=-1;
        ParamFrame bone(R::FindDispatchFunctionCached(R::ClassOf(mesh),L"GetBoneIndex"));
        if (!bone.Set<R::FName>(L"BoneName",entry.bone) || !Call(mesh,bone) || !g_velocityMesh.Get() ||
            !bone.GetRaw(L"ReturnValue",&index,sizeof(index))) return false;
        // The stock rig has combined thighs/lowlegs; those are absent on the
        // selected full rig. Only restore bones the new mesh actually owns.
        if (index<0) {++g_bodyVelocityIndex;continue;}
        if (!entry.linearDone) {
            ParamFrame velocity(g_setLinVelFn);
            if (!velocity.Set<R::FName>(L"BoneName",entry.bone) || !velocity.Set<FVector>(L"NewVel",entry.linear) ||
                !velocity.Set<bool>(L"bAddToCurrent",false) || !Call(mesh,velocity) || !g_velocityMesh.Get()) return false;
            entry.linearDone=true;
        }
        ParamFrame angular(g_setAngVelFn);
        if (!angular.Set<R::FName>(L"BoneName",entry.bone) || !angular.Set<FVector>(L"NewAngVel",entry.angular) ||
            !angular.Set<bool>(L"bAddToCurrent",false) || !Call(mesh,angular) || !g_velocityMesh.Get()) return false;
        ++g_bodyVelocityIndex;
    }
    // Replay exactly in order, retaining additive/absolute semantics and NAME_None
    // (the native default body). A retry resumes after successful commands.
    while (g_velocityCommandIndex<g_velocityCommands.size()) {
        const auto& entry=g_velocityCommands[g_velocityCommandIndex];
        ParamFrame velocity(g_setLinVelFn);
        if (!velocity.Set<R::FName>(L"BoneName",entry.bone) || !velocity.Set<FVector>(L"NewVel",entry.linear) ||
            !velocity.Set<bool>(L"bAddToCurrent",entry.add) || !Call(mesh,velocity) || !g_velocityMesh.Get()) return false;
        ++g_velocityCommandIndex;
    }
    UE_LOGI("ragdoll_momentum: restored native motion (base=%.1f,%.1f,%.1f deferred=%u)",
        g_baseLinear.X,g_baseLinear.Y,g_baseLinear.Z,static_cast<unsigned>(g_velocityCommands.size()));
    CancelRagdollVelocity();
    return true;
}

CachedObjRef g_localRagdollPlayer, g_configuredRagdoll, g_selectedRagdoll, g_entryScript;
std::chrono::steady_clock::time_point g_ragdollReadyAfter{};
RagdollView g_ragdollView;
bool g_ragdollStarted = false, g_entryViewPending = false, g_recoveryAligned = false;

UE_FACT_FN(kRagdollFinishSpawn, "ragdoll_transition", L"GameplayStatics", L"FinishSpawningActor", Declared);
UE_FACT_FN(kRagdollArmRotation, "ragdoll_transition", L"SceneComponent", L"K2_SetRelativeRotation", Declared);
UE_FACT_FN(kRagdollArmAttach, "ragdoll_transition", L"SceneComponent", L"K2_AttachToComponent", Declared);

void* LocalComponent(void* player, const wchar_t* name) {
    if (!player || !R::IsLive(player)) return nullptr;
    const int offset = R::FindPropertyOffset(R::ClassOf(player), name);
    void* component = nullptr;
    if (offset >= 0) std::memcpy(&component, static_cast<uint8_t*>(player) + offset, sizeof(component));
    return component && R::IsLive(component) ? component : nullptr;
}

bool PrepareLocalRagdoll(void* player, void* target, bool duringConstruction) {
    void* source = puppet::GetMeshPlayerVisibleComponent(player);
    void* asset = puppet::GetComponentSkeletalMeshAsset(source);
    CachedObjRef playerRef, sourceRef, targetRef, assetRef;
    playerRef.Set(player); sourceRef.Set(source); targetRef.Set(target); assetRef.Set(asset);
    if (!playerRef.Get() || !sourceRef.Get() || !targetRef.Get() || !assetRef.Get()) return false;
    const bool haveView = g_ragdollView.Capture(player, target, duringConstruction);
    if (!BeginRagdollVelocity(target)) return false;
    if (!ConfigurePlayerRagdollMesh(source, target) || !playerRef.Get() || !sourceRef.Get() ||
        !targetRef.Get() || !assetRef.Get()) { CancelRagdollVelocity(); return false; }
    g_configuredRagdoll = targetRef; g_selectedRagdoll = assetRef;
    g_ragdollStarted = false; g_recoveryAligned = false;
    g_ragdollReadyAfter = std::chrono::steady_clock::now() + std::chrono::milliseconds(30);
    if (duringConstruction) {
        // FinishSpawningActor returns before ragdollMode attaches the camera,
        // starts physics and issues its chest/fall impulses. Install the final
        // reference pose here so those native operations see the correct rig.
        if (!StartBodySim(target) || !targetRef.Get() || !playerRef.Get() || !FinishRagdollVelocity(target)) return false;
        g_ragdollStarted = true;
        g_entryViewPending = haveView;
        UE_LOGI("ragdoll_transition: selected rig ready before native camera/physics/impulses");
    } else if (haveView) {
        g_ragdollView.Restore();
    }
    return true;
}

void OnLocalRagdollConstructed(void*, void* source, void* result) {
    void* player = g_localRagdollPlayer.Get();
    if (!player || source != player || !result || !R::IsLive(result)) return;
    void* owner = nullptr;
    // Only the local player's native ragdollMode spawn, never puppet/NPC bodies.
    if (R::ClassNameOf(result) != L"playerRagdoll_C" && R::ClassNameOf(result) != L"playerRagdoll_crouch_C") return;
    std::memcpy(&owner, static_cast<uint8_t*>(result) + kPlayerRagdoll_Player, sizeof(owner));
    if (owner != player) return;
    const auto caller = ufunction_hook::CurrentCallerFrame();
    if (!caller.function || R::ToString(R::NameOf(caller.function)) != L"ragdollMode") return;
    g_entryScript.Set(caller.function); g_entryViewPending = false;
    PrepareLocalRagdoll(player, RagdollMeshOf(result), true);
}

void OnNativeRagdollArmRotation(void* component, void* source, void*) {
    if (!g_entryViewPending || source != g_localRagdollPlayer.Get() || component != g_ragdollView.arm.Get() ||
        ufunction_hook::CurrentCallerFrame().function != g_entryScript.Get()) return;
    if (g_ragdollView.Restore()) g_entryViewPending = false;
}

void OnNativeRagdollArmAttached(void* component, void* source, void*) {
    void* player = g_localRagdollPlayer.Get();
    if (!player || source != player || component != g_ragdollView.arm.Get() || !g_ragdollStarted || g_recoveryAligned ||
        GetLocalRagdollBodyMesh(player) != g_configuredRagdoll.Get()) return;
    const auto caller = ufunction_hook::CurrentCallerFrame();
    if (!caller.function || R::ToString(R::NameOf(caller.function)) != L"ExecuteUbergraph_mainPlayer") return;
    void* parent = nullptr;
    ParamFrame attached(R::FindDispatchFunctionCached(R::ClassOf(component), L"GetAttachParent"));
    if (!Call(component, attached) || !attached.GetRaw(L"ReturnValue", &parent, sizeof(parent)) ||
        !parent || parent != LocalComponent(player, L"crouchRoot")) return;
    CachedObjRef arm, controller; arm.Set(component); controller.Set(GetController(player));
    FRotator view{}; FVector position{};
    ParamFrame rotation(R::FindDispatchFunctionCached(R::ClassOf(component), L"K2_GetComponentRotation"));
    ParamFrame location(R::FindDispatchFunctionCached(R::ClassOf(component), L"K2_GetComponentLocation"));
    if (!arm.Get() || !controller.Get() || !Call(component, rotation) ||
        !rotation.GetRaw(L"ReturnValue", &view, sizeof(view)) || !Call(component, location) ||
        !location.GetRaw(L"ReturnValue", &position, sizeof(position)) || !FiniteVelocity(position) || !std::isfinite(view.Yaw) ||
        !std::isfinite(view.Pitch) || !std::isfinite(view.Roll) || !g_localRagdollPlayer.Get()) return;
    // Native wakeup captures this arm's relative transform and blends it to
    // upright, then clears its relative rotation at the end of the timeline.
    // Hand its current yaw to the standing owner now: clearing the arm later
    // must not restore the capsule's pre-fall heading. Keep the visible view.
    g_recoveryAligned = true;
    SetControlRotation(controller.Get(), {0.f, view.Yaw, 0.f});
    if (!g_localRagdollPlayer.Get() || !arm.Get() || !controller.Get() ||
        !SetActorRotation(player, {0.f, view.Yaw, 0.f}) || !arm.Get()) return;
    ParamFrame restore(R::FindDispatchFunctionCached(R::ClassOf(component), L"K2_SetWorldLocationAndRotation"));
    if (!restore.Set<FVector>(L"NewLocation", position) || !restore.Set<FRotator>(L"NewRotation", view) ||
        !restore.Set<bool>(L"bSweep", false) || !restore.Set<bool>(L"bTeleport", true) || !Call(component, restore)) return;
    g_entryViewPending = false;
    UE_LOGI("ragdoll_transition: native recovery owns current view yaw %.2f", view.Yaw);
}

void WatchLocalRagdoll(void* player) {
    g_localRagdollPlayer.Set(player);
    if (!g_localRagdollPlayer.Get()) return;
    static bool spawn = false, rotation = false, attach = false;
    if (!spawn) spawn = ufunction_hook::InstallPostHook(facts::Resolve(kRagdollFinishSpawn), &OnLocalRagdollConstructed);
    if (!rotation) rotation = ufunction_hook::InstallPostHook(facts::Resolve(kRagdollArmRotation), &OnNativeRagdollArmRotation);
    if (!attach) attach = ufunction_hook::InstallPostHook(facts::Resolve(kRagdollArmAttach), &OnNativeRagdollArmAttached);
}

}  // namespace

void* SpawnPlayerRagdollBody(void* ownerPlayer, const FVector& location, const FRotator& rotation) {
    CachedObjRef owner; owner.Set(ownerPlayer);
    if (!owner.Get()) return nullptr;
    void* cls = ResolveRagdollClass();
    if (!cls) return nullptr;
    void* body = BeginDeferredSpawn(cls, location, rotation);
    CachedObjRef bodyRef; bodyRef.Set(body);
    if (!bodyRef.Get()) return nullptr;
    *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(body) + kPlayerRagdoll_Player) = ownerPlayer;
    const bool finished = FinishDeferredSpawn(body, location, rotation);
    if (!bodyRef.Get()) return nullptr;
    void* comp = RagdollMeshOf(body);
    if (!finished || !owner.Get() || !comp ||
        !ConfigurePlayerRagdollMesh(puppet::GetMeshPlayerVisibleComponent(ownerPlayer), comp) ||
        !bodyRef.Get() || !owner.Get()) {
        UE_LOGW("ragdoll_body: selected model/physics could not initialize; no substitute body shown");
        if (bodyRef.Get()) DestroyActor(body);
        return nullptr;
    }
    UE_LOGI("ragdoll_body: selected player model with articulated physics ready (%p)", body);
    return body;
}

bool StartPlayerRagdollSimulation(void* body) {
    return StartBodySim(RagdollMeshOf(body));
}

// Attach `actor` (its RootComponent) to the ragdoll `body`'s mesh (kAragdoll_SkeletalMesh) at the
// named socket, so it rides the simulated bones.
bool AttachActorToRagdollBody(void* actor, void* body) {
    if (!actor || !R::IsLive(actor) || !body || !R::IsLive(body)) return false;
    void* mesh = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(body) + kAragdoll_SkeletalMesh);
    if (!mesh || !R::IsLive(mesh)) return false;
    CachedObjRef actorRef, bodyRef, meshRef;
    actorRef.Set(actor); bodyRef.Set(body); meshRef.Set(mesh);
    void* fn = R::FindFunction(R::FindClass(L"Actor"), L"K2_AttachToComponent");
    if (!fn) { UE_LOGW("ragdoll_body: K2_AttachToComponent unresolved"); return false; }
    uint8_t socket[8] = {};                            // pelvis FName, or NAME_None on miss
    const bool gotPelvis = FindBoneFName(mesh, L"pelvis", socket);
    if (!gotPelvis) UE_LOGW("ragdoll_body: 'pelvis' bone not found on the ragdoll mesh -- attaching to the root");
    ParamFrame f(fn);
    if (!f.Set<void*>(L"Parent", mesh) || !f.SetRaw(L"SocketName", socket, sizeof(socket)) ||
        !f.Set<uint8_t>(L"LocationRule", uint8_t{1}) ||
        !f.Set<uint8_t>(L"RotationRule", uint8_t{1}) ||
        !f.Set<uint8_t>(L"ScaleRule", uint8_t{1}) ||
        !f.Set<bool>(L"bWeldSimulatedBodies", false) || !Call(actor, f) ||
        !actorRef.Get() || !bodyRef.Get() || !meshRef.Get()) return false;
    // Actor::K2_AttachToComponent is void; the component method has a bool return.
    // Verify the resulting relationship rather than reading a nonexistent out param.
    void* parent = nullptr;
    return GetAttachParentActor(actor, parent) && parent == body &&
        actorRef.Get() && bodyRef.Get() && meshRef.Get();
}

// Detach `actor` from its parent (the ragdoll body), KeepWorld so it stays where the
// flop left it (the next pose-drive then takes over). Game thread only.
bool DetachActorFromRagdollBody(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    void* fn = R::FindFunction(R::FindClass(L"Actor"), L"K2_DetachFromActor");
    if (!fn) { UE_LOGW("ragdoll_body: K2_DetachFromActor unresolved"); return false; }
    ParamFrame f(fn);
    f.Set<uint8_t>(L"LocationRule", uint8_t{1});       // EDetachmentRule::KeepWorld
    f.Set<uint8_t>(L"RotationRule", uint8_t{1});       // KeepWorld
    f.Set<uint8_t>(L"ScaleRule",    uint8_t{1});       // KeepWorld
    return Call(actor, f);
}

void* GetRagdollBodyMesh(void* ragdollActor) {
    return RagdollMeshOf(ragdollActor);  // null-safe + liveness-guarded inside
}

void* GetLocalRagdollBodyMesh(void* mainPlayer) {
    if (!mainPlayer || !R::IsLive(mainPlayer)) return nullptr;
    void* ragdollActor =
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mainPlayer) + kMainPlayer_ragdollActor);
    return RagdollMeshOf(ragdollActor);
}

bool RefreshLocalRagdollModel(void* mainPlayer) {
    WatchLocalRagdoll(mainPlayer);
    void* target = GetLocalRagdollBodyMesh(mainPlayer);
    void* source = puppet::GetMeshPlayerVisibleComponent(mainPlayer);
    void* asset = puppet::GetComponentSkeletalMeshAsset(source);
    if (!target || !source || !asset) {
        g_configuredRagdoll.Reset(); g_selectedRagdoll.Reset(); g_ragdollView.Reset();
        g_entryScript.Reset(); g_entryViewPending = false; g_ragdollStarted = false; g_recoveryAligned = false;
        CancelRagdollVelocity(); return false;
    }
    if (g_configuredRagdoll.Get() == target && g_selectedRagdoll.Get() == asset) {
        if (!g_ragdollStarted) {
            g_ragdollView.Restore();
            if (std::chrono::steady_clock::now() < g_ragdollReadyAfter || !StartBodySim(target)) return false;
            g_ragdollView.Restore(); g_ragdollStarted=true;
            UE_LOGI("ragdoll_model: selected body simulation started (mesh=%p asset=%p)", target, asset);
        }
        return !g_velocityMesh.Get() || FinishRagdollVelocity(target);
    }
    // Configure copies the selected standing mesh's world transform. Its legs
    // must remain above the floor: a plushie's low pelvis is not a human anchor.
    // Translating the new pelvis down to the old one buries the full-size limbs
    // and PhysX depenetration launches the player on the next simulation step.
    PrepareLocalRagdoll(mainPlayer, target, false);
    return false;
}

bool ReadArticulatedPose(void* mesh, std::vector<ArticulatedBone>& bones) {
    bones.clear();
    CachedObjRef meshRef; meshRef.Set(mesh);
    if (!meshRef.Get()) return false;
    auto field = [](void* object, const wchar_t* name) -> void* {
        if (!object || !R::IsLive(object)) return nullptr;
        const int off = R::FindPropertyOffset(R::ClassOf(object), name);
        if (off < 0) return nullptr;
        void* value = nullptr; std::memcpy(&value, static_cast<uint8_t*>(object) + off, sizeof(value));
        return value && R::IsLive(value) ? value : nullptr;
    };
    void* physics = field(mesh, L"PhysicsAssetOverride");
    if (!physics) physics = field(puppet::GetComponentSkeletalMeshAsset(mesh), L"PhysicsAsset");
    if (!physics) return false;
    const int off = R::FindPropertyOffset(R::ClassOf(physics), L"SkeletalBodySetups");
    if (off < 0) return false;
    struct Array { void** data; int32_t num, max; } setups{};
    std::memcpy(&setups, static_cast<uint8_t*>(physics) + off, sizeof(setups));
    if (!setups.data || setups.num < 1 || setups.num > 64 || setups.max < setups.num) return false;
    for (int32_t i = 0; i < setups.num; ++i) {
        void* setup = setups.data[i];
        if (!setup || !R::IsLive(setup) || !meshRef.Get()) return false;
        const int boneOff = R::FindPropertyOffset(R::ClassOf(setup), L"BoneName");
        if (boneOff < 0) return false;
        R::FName name{}; std::memcpy(&name, static_cast<uint8_t*>(setup) + boneOff, sizeof(name));
        ParamFrame index(R::FindDispatchFunctionCached(R::ClassOf(mesh), L"GetBoneIndex"));
        int32_t boneIndex = -1;
        if (!index.Set<R::FName>(L"BoneName", name) || !Call(mesh, index) ||
            !index.GetRaw(L"ReturnValue", &boneIndex, sizeof(boneIndex)) || boneIndex < 0 || boneIndex >= 1024) return false;
        ParamFrame transform(R::FindDispatchFunctionCached(R::ClassOf(mesh), L"GetSocketTransform"));
        ArticulatedBone bone; bone.index = static_cast<uint16_t>(boneIndex);
        if (!transform.Set<R::FName>(L"InSocketName", name) || !transform.Set<uint8_t>(L"TransformSpace", 0) ||
            !Call(mesh, transform) || !transform.GetRaw(L"ReturnValue", &bone.world, sizeof(bone.world))) return false;
        bones.push_back(bone);
    }
    return meshRef.Get() != nullptr;
}

uint32_t BoneLayoutSignature(void* mesh) {
    struct Entry { CachedObjRef asset; uint32_t hash = 0; };
    static std::map<void*, Entry> cache;
    void* asset = puppet::GetComponentSkeletalMeshAsset(mesh);
    if (!asset) return 0;
    auto it = cache.find(asset);
    if (it != cache.end() && it->second.asset.Get() == asset) return it->second.hash;
    CachedObjRef meshRef; meshRef.Set(mesh);
    if (!meshRef.Get()) return 0;
    int32_t count = 0;
    ParamFrame num(R::FindDispatchFunctionCached(R::ClassOf(mesh), L"GetNumBones"));
    if (!Call(mesh, num) || !num.GetRaw(L"ReturnValue", &count, sizeof(count)) || count < 1 || count > 1024) return 0;
    uint32_t hash = 2166136261u;
    for (int32_t i = 0; i < count; ++i) {
        if (!meshRef.Get()) return 0;
        ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(mesh), L"GetBoneName")); R::FName name{};
        if (!frame.Set<int32_t>(L"BoneIndex", i) || !Call(mesh, frame) ||
            !frame.GetRaw(L"ReturnValue", &name, sizeof(name))) return 0;
        for (wchar_t ch : R::ToString(name)) {
            hash ^= static_cast<uint16_t>(ch); hash *= 16777619u;
        }
        hash ^= 0; hash *= 16777619u;
    }
    if (!meshRef.Get()) return 0;
    for (auto stale = cache.begin(); stale != cache.end();) {
        if (!stale->second.asset.Get()) stale = cache.erase(stale); else ++stale;
    }
    Entry entry; entry.asset.Set(asset); entry.hash = hash;
    cache[asset] = entry;
    return hash;
}

bool ReadLocalRagdollPelvisPhysics(void* mainPlayer, FVector& outLoc, FRotator& outRot,
                                   FVector& outLinVel, FVector& outAngVel) {
    outLoc = FVector{}; outRot = FRotator{}; outLinVel = FVector{}; outAngVel = FVector{};
    if (!mainPlayer || !R::IsLive(mainPlayer)) return false;
    // The native ragdoll the C-key/faint spawned. Null until the player ragdolls;
    // cleared on recover -- so a non-null + live value IS the "is ragdolling" signal.
    void* ragdollActor =
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mainPlayer) + kMainPlayer_ragdollActor);
    void* mesh = RagdollMeshOf(ragdollActor);
    if (!mesh) return false;
    if (!ResolvePhysicsFns()) return false;
    if (!EnsurePelvisFName(mesh)) return false;
    // Pelvis WORLD location + rotation (the kel-tumble anchor).
    { ParamFrame f(g_getSocketLocFn); f.SetRaw(L"InSocketName", g_pelvisFName, sizeof(g_pelvisFName));
      if (!Call(mesh, f)) return false; outLoc = f.Get<FVector>(L"ReturnValue"); }
    { ParamFrame f(g_getSocketRotFn); f.SetRaw(L"InSocketName", g_pelvisFName, sizeof(g_pelvisFName));
      if (!Call(mesh, f)) return false; outRot = f.Get<FRotator>(L"ReturnValue"); }
    // Pelvis linear (cm/s) + angular (deg/s) velocity -- the "physics properties".
    { ParamFrame f(g_getLinVelFn); f.SetRaw(L"BoneName", g_pelvisFName, sizeof(g_pelvisFName));
      if (!Call(mesh, f)) return false; outLinVel = f.Get<FVector>(L"ReturnValue"); }
    { ParamFrame f(g_getAngVelFn); f.SetRaw(L"BoneName", g_pelvisFName, sizeof(g_pelvisFName));
      if (!Call(mesh, f)) return false; outAngVel = f.Get<FVector>(L"ReturnValue"); }
    return true;
}

void DriveRagdollBodyPelvisVelocity(void* body, const FVector& linVel, const FVector& angVel) {
    void* mesh = RagdollMeshOf(body);
    if (!mesh) return;
    if (!ResolvePhysicsFns()) return;
    if (!EnsurePelvisFName(mesh)) return;
    // Overwrite (bAddToCurrent=false) the pelvis body's velocity with the sender's --
    // the body slaves its gross motion to the real ragdoll. The constrained limb bodies
    // simulate freely on the visible selected-model body. Same
    // SetPhysics{Linear,Angular}Velocity pair as PropRelease.
    { ParamFrame f(g_setLinVelFn);
      f.Set<FVector>(L"NewVel", linVel);
      f.Set<bool>(L"bAddToCurrent", false);
      f.SetRaw(L"BoneName", g_pelvisFName, sizeof(g_pelvisFName));
      Call(mesh, f); }
    { ParamFrame f(g_setAngVelFn);
      f.Set<FVector>(L"NewAngVel", angVel);
      f.Set<bool>(L"bAddToCurrent", false);
      f.SetRaw(L"BoneName", g_pelvisFName, sizeof(g_pelvisFName));
      Call(mesh, f); }
}

bool TranslateRagdollBody(void* body, const FVector& delta) {
    if (!std::isfinite(delta.X) || !std::isfinite(delta.Y) || !std::isfinite(delta.Z)) return false;
    void* mesh = RagdollMeshOf(body);
    if (!mesh) return false;
    CachedObjRef ref; ref.Set(mesh);
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(mesh), L"K2_AddWorldOffset"));
    // Teleport physics together rather than deriving artificial velocities from
    // the correction. No sweep: a blocked mirror must converge on the sender.
    return frame.valid() && frame.Set<FVector>(L"DeltaLocation", delta) &&
        frame.Set<bool>(L"bSweep", false) && frame.Set<bool>(L"bTeleport", true) &&
        Call(mesh, frame) && ref.Get();
}

}  // namespace ue_wrap::engine
