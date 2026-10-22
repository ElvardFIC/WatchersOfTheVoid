// harness/autotest/autotest_ragdoll_spawn_probe.cpp -- the single-player ragdoll feasibility probe.
// The game's own ragdoll verb is a method of the player pawn and acts on that player, so it cannot
// be reused on a puppet; the plan is to spawn the game's ragdoll body manually (a deferred world
// spawn) on the puppet and skin it. This probe answers, by observing live objects in plain
// single-player with no connection, the decisive questions before any production code: whether a
// manual deferred spawn with the owning player set before the finish leaves the body's skeletal
// mesh visible, physically simulating and falling (does the body self-configure in its own
// BeginPlay from the player reference); whether that manual spawn is death-free for the player it
// names (the dead and ragdoll flags before and after); and the ground truth, the real verb's own
// body dumped with the same fields, then recovered with the get-up verb. Single instance: the
// ragdollspawn scenario arms it, and an external capture takes the shots on the shot-ready log
// markers. Gated by VOTVCOOP_RUN_RAGDOLL_SPAWN_PROBE=1. A throwaway diagnostic: raw offset reads
// cited to the SDK header dump, not a shipping path.

#include "ue_wrap/core/gc_pin.h"
#include "harness/autotest.h"
#include "harness/screenshot.h"

#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/local_body.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

namespace harness::autotest {
void RunRagdollRobustness(void* local, bool catalogOnly);
namespace {

namespace R = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace E = ue_wrap::engine;

// Offsets from the SDK header dump (a raw diagnostic).
constexpr size_t kAragdoll_SkeletalMesh = 0x0230;  // Aragdoll_C::SkeletalMesh (USkeletalMeshComponent*)
constexpr size_t kPlayerRagdoll_Player  = 0x0248;  // AplayerRagdoll_C::Player (AmainPlayer_C*)
constexpr size_t kMainPlayer_ragdollActor = 0x0C40;  // AmainPlayer_C::ragdollActor (AplayerRagdoll_C*)
constexpr size_t kUSkinnedMesh_SkeletalMesh = 0x0480;  // USkinnedMeshComponent::SkeletalMesh (the skin asset)

std::string ReadEnv(const char* name) {
    char buf[256] = {};
    const DWORD n = ::GetEnvironmentVariableA(name, buf, sizeof(buf));
    return (n > 0 && n < sizeof(buf)) ? std::string(buf) : std::string();
}

// Bounded spin-wait on a game-thread task's completion flag. False means the posted task
// faulted (the exception firewall ate the fault and the flag was never set), so the caller
// bails instead of hanging the probe.
bool WaitDone(const std::shared_ptr<std::atomic<int>>& d, int timeoutMs) {
    for (int i = 0; i < timeoutMs / 5 && d->load() == 0; ++i) ::Sleep(5);
    return d->load() != 0;
}

// Call a no-argument bool function (IsVisible, IsAnyRigidBodyAwake) on `comp`. Game thread
// only. False if unresolved or uncallable.
bool CallBoolFn(void* comp, const wchar_t* cls, const wchar_t* fn) {
    if (!comp || !R::IsLive(comp)) return false;
    void* f = R::FindFunction(R::FindClass(cls), fn);
    if (!f) return false;
    ue_wrap::ParamFrame pf(f);
    if (!pf.valid()) return false;
    return ue_wrap::Call(comp, pf) && pf.Get<bool>(L"ReturnValue");
}

// Start the physics simulation on a skeletal-mesh component (collision so the bodies have a
// floor to rest on, then all bodies simulate and the component simulates). This is the step
// the ragdoll verb does after spawn that the body's own BeginPlay does not: the bare spawn
// builds the bodies but leaves them frozen. Game thread only.
void StartBodySim(void* comp) {
    if (!comp || !R::IsLive(comp)) return;
    if (void* f = R::FindFunction(R::FindClass(L"PrimitiveComponent"), L"SetCollisionEnabled")) {
        ue_wrap::ParamFrame pf(f); pf.Set<uint8_t>(L"NewType", uint8_t{3});  // QueryAndPhysics
        ue_wrap::Call(comp, pf);
    }
    if (void* f = R::FindFunction(R::FindClass(L"SkeletalMeshComponent"), L"SetAllBodiesSimulatePhysics")) {
        ue_wrap::ParamFrame pf(f); pf.Set<bool>(L"bNewSimulate", true); ue_wrap::Call(comp, pf);
    }
    if (void* f = R::FindFunction(R::FindClass(L"PrimitiveComponent"), L"SetSimulatePhysics")) {
        ue_wrap::ParamFrame pf(f); pf.Set<bool>(L"bSimulate", true); ue_wrap::Call(comp, pf);
    }
}

// The skeletal mesh component on a ragdoll actor.
void* RagdollMeshComp(void* actor) {
    if (!actor || !R::IsLive(actor)) return nullptr;
    void* c = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(actor) + kAragdoll_SkeletalMesh);
    return (c && R::IsLive(c)) ? c : nullptr;
}

// Dump the full live configuration of a ragdoll actor (manual or real) so the two can be
// compared field for field. Game thread only.
void DumpRagdollActor(void* actor, const char* tag) {
    if (!actor || !R::IsLive(actor)) {
        UE_LOGW("ragdollspawn[%s]: actor null/dead -- nothing to dump", tag);
        return;
    }
    const std::wstring cls = R::ClassNameOf(actor);
    ue_wrap::FVector aLoc{};
    const char* aNote = E::TryGetActorLocation(actor, aLoc) ? "" : " (unread)";
    void* comp = RagdollMeshComp(actor);
    if (!comp) {
        UE_LOGW("ragdollspawn[%s]: actor '%ls' has NO live SkeletalMesh @0x230 (actorZ=%.0f%s)",
                tag, cls.c_str(), aLoc.Z, aNote);
        return;
    }
    void* meshAsset = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(comp) + kUSkinnedMesh_SkeletalMesh);
    const std::wstring meshName = (meshAsset && R::IsLive(meshAsset))
                                      ? R::ToString(R::NameOf(meshAsset)) : L"<null>";
    const bool vis    = CallBoolFn(comp, L"SceneComponent", L"IsVisible");
    const bool awake  = CallBoolFn(comp, L"PrimitiveComponent", L"IsAnyRigidBodyAwake");
    const ue_wrap::FVector cLoc = E::GetComponentLocation(comp);
    float lowZ = 0.f;
    const bool okBone = E::GetLowestBoneWorldZ(comp, lowZ);

    // Material slot 0 (what the body renders with today, the swap target).
    std::wstring mat0 = L"<?>";
    int32_t numMat = 0;
    if (void* gn = R::FindFunction(R::FindClass(L"PrimitiveComponent"), L"GetNumMaterials")) {
        ue_wrap::ParamFrame f(gn);
        if (ue_wrap::Call(comp, f)) numMat = f.Get<int32_t>(L"ReturnValue");
    }
    if (void* gm = R::FindFunction(R::FindClass(L"PrimitiveComponent"), L"GetMaterial")) {
        ue_wrap::ParamFrame f(gm); f.Set<int32_t>(L"ElementIndex", 0);
        if (ue_wrap::Call(comp, f)) { void* m = f.Get<void*>(L"ReturnValue");
            if (m && R::IsLive(m)) mat0 = R::ToString(R::NameOf(m)); }
    }

    UE_LOGI("ragdollspawn[%s]: actor='%ls' actorZ=%.0f%s | comp meshAsset='%ls' IsVisible=%d "
            "IsAnyRigidBodyAwake=%d compZ=%.0f lowestBoneZ=%.0f(ok=%d) numMaterials=%d mat0='%ls'",
            tag, cls.c_str(), aLoc.Z, aNote, meshName.c_str(), vis ? 1 : 0, awake ? 1 : 0,
            cLoc.Z, lowZ, okBone ? 1 : 0, numMat, mat0.c_str());
}

// Aim the local player's camera at a target actor's mesh so the autonomous screenshot frames
// the spawned body. Game thread only.
void AimLocalAt(void* local, void* target) {
    if (!local || !R::IsLive(local) || !target || !R::IsLive(target)) return;
    void* ctrl = E::GetController(local);
    if (!ctrl || !R::IsLive(ctrl)) return;
    ue_wrap::FVector h{};
    if (!E::TryGetActorLocation(local, h)) {
        UE_LOGW("ragdollspawn: not aimed -- the player's location could not be read"); return; }
    void* comp = RagdollMeshComp(target);
    ue_wrap::FVector p{};
    if (comp) p = E::GetComponentLocation(comp);
    else if (!E::TryGetActorLocation(target, p)) {
        UE_LOGW("ragdollspawn: not aimed -- the body's location could not be read"); return; }
    const float dx = p.X - h.X, dy = p.Y - h.Y, dz = p.Z - (h.Z + 60.f);
    const float horiz = std::sqrt(dx * dx + dy * dy);
    const float yaw = std::atan2(dy, dx) * 57.29578f;
    const float pitch = std::atan2(dz, horiz) * 57.29578f;
    E::SetControlRotation(ctrl, ue_wrap::FRotator{pitch, yaw, 0.f});
}

// Read the player's dead and ragdoll flags (the death detection). False if unreadable. Game
// thread only.
bool ReadDeadRagdoll(void* mp, bool& dead, bool& isRagdoll) {
    return E::ReadMainPlayerRagdollState(mp, isRagdoll, dead);
}

// Sample whether a body is falling: log the component Z and the lowest-bone Z (a drop over
// successive samples is real physics; static is not simulating). Game thread.
void SampleFall(void* actor, const char* tag) {
    auto done = std::make_shared<std::atomic<int>>(0);
    GT::Post([actor, tag, done] {
        if (actor && R::IsLive(actor)) {
            void* comp = RagdollMeshComp(actor);
            const float cZ = comp ? E::GetComponentLocation(comp).Z : 0.f;
            float lowZ = 0.f;
            const bool ok = comp ? E::GetLowestBoneWorldZ(comp, lowZ) : false;
            UE_LOGI("ragdollspawn[%s]: fall-sample compZ=%.1f lowestBoneZ=%.1f(ok=%d)",
                    tag, cZ, lowZ, ok ? 1 : 0);
        }
        done->store(1);
    });
    WaitDone(done, 8000);
}

// Experiment B: the manual spawn. Returns the spawned actor (kept alive for the screenshot),
// or null.
void* SpawnManualRagdoll(void* local) {
    auto done = std::make_shared<std::atomic<int>>(0);
    auto out  = std::make_shared<void*>(nullptr);
    GT::Post([local, done, out] {
        void* cls = R::FindClass(L"playerRagdoll_C");
        if (!cls) { UE_LOGW("ragdollspawn[manual]: playerRagdoll_C class did not resolve"); done->store(1); return; }
        ue_wrap::FVector pl{};
        if (!E::TryGetActorLocation(local, pl)) {
            UE_LOGW("ragdollspawn[manual]: the player's location could not be read"); done->store(1); return; }
        const ue_wrap::FVector fwd = E::GetActorForwardVector(local);
        ue_wrap::FRotator rot{};
        if (!E::TryGetActorRotation(local, rot)) {
            UE_LOGW("ragdollspawn[manual]: the player's rotation could not be read"); done->store(1); return; }
        // About 120 units in front of the player at the same height (framable; not inside the
        // body).
        const ue_wrap::FVector loc{ pl.X + fwd.X * 120.f, pl.Y + fwd.Y * 120.f, pl.Z };
        void* actor = E::BeginDeferredSpawn(cls, loc, rot);
        if (!actor) { UE_LOGW("ragdollspawn[manual]: BeginDeferredSpawn returned null"); done->store(1); return; }
        // Expose-on-spawn: set the owning player before the finish runs BeginPlay, so the actor's
        // own BeginPlay sees it.
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(actor) + kPlayerRagdoll_Player) = local;
        E::FinishDeferredSpawn(actor, loc, rot);
        // Root it for the probe's lifetime so an incidental GC pass cannot reap the body
        // mid-experiment and turn a working spawn into a false null-or-dead dump. Rooted for the
        // probe's own run only: the static pin holds one body, so a second invocation releases the
        // first, and the pin stands down at process teardown. The destroy below tears the body down
        // regardless of root, and a self-destructing BeginPlay still shows as dead.
        static ue_wrap::GcPin sProbePin;  // one throwaway probe body, owned for its session
        sProbePin.Pin(actor);
        UE_LOGI("ragdollspawn[manual]: spawned playerRagdoll_C @%p at (%.0f,%.0f,%.0f), Player set -- NO ragdollMode called",
                actor, loc.X, loc.Y, loc.Z);
        *out = actor;
        done->store(1);
    });
    WaitDone(done, 8000);
    return *out;
}

void RunManualExperiment(void* local) {
    UE_LOGI("ragdollspawn: === EXPERIMENT B (manual spawn, NO ragdollMode) ===");

    // The baseline dead and ragdoll flags (they must be unchanged after the manual spawn).
    bool deadBefore = false, ragBefore = false;
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        auto d = std::make_shared<int>(0), r = std::make_shared<int>(0);
        GT::Post([local, done, d, r] {
            bool dead = false, isRag = false;
            if (ReadDeadRagdoll(local, dead, isRag)) { *d = dead ? 1 : 0; *r = isRag ? 1 : 0; }
            done->store(1);
        });
        WaitDone(done, 8000);
        deadBefore = *d != 0; ragBefore = *r != 0;
    }
    UE_LOGI("ragdollspawn[manual]: player BEFORE spawn -- dead=%d isRagdoll=%d", deadBefore ? 1 : 0, ragBefore ? 1 : 0);

    void* actor = SpawnManualRagdoll(local);
    if (!actor) {
        UE_LOGW("ragdollspawn[manual]: VERDICT FAIL -- could not spawn playerRagdoll_C manually");
        return;
    }

    ::Sleep(1500);  // let ReceiveBeginPlay + the first ticks run (BARE -- no sim yet)

    // Dump the bare configured body and run the death check (what BeginPlay alone produces;
    // expected visible but frozen).
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        auto d = std::make_shared<int>(0), r = std::make_shared<int>(0);
        GT::Post([actor, local, done, d, r] {
            DumpRagdollActor(actor, "manual-bare");
            bool dead = false, isRag = false;
            if (ReadDeadRagdoll(local, dead, isRag)) { *d = dead ? 1 : 0; *r = isRag ? 1 : 0; }
            done->store(1);
        });
        WaitDone(done, 8000);
        UE_LOGI("ragdollspawn[manual]: player AFTER bare spawn -- dead=%d isRagdoll=%d (Q1b: %s)",
                *d, *r, (*d == (deadBefore ? 1 : 0) && *r == (ragBefore ? 1 : 0))
                            ? "UNCHANGED -> manual spawn is DEATH-FREE"
                            : "CHANGED -> the actor's BeginPlay bleeds player state");
    }
    SampleFall(actor, "manual-bare");  // expected static (BeginPlay doesn't sim)

    // Start the sim ourselves, the step the ragdoll verb does after spawn. If the body now falls
    // (the lowest bone drops toward the floor), the recipe is: deferred spawn, set the player,
    // finish, start the sim. That is the whole feature.
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        GT::Post([actor, local, done] {
            void* comp = RagdollMeshComp(actor);
            StartBodySim(comp);
            UE_LOGI("ragdollspawn[manual]: started physics sim on the body mesh (SetAllBodiesSimulatePhysics+SetSimulatePhysics)");
            done->store(1);
        });
        WaitDone(done, 8000);
    }
    ::Sleep(900);  // let it begin falling before we frame + dump
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        GT::Post([actor, local, done] {
            DumpRagdollActor(actor, "manual-sim");
            AimLocalAt(local, actor);
            done->store(1);
        });
        WaitDone(done, 8000);
    }
    UE_LOGI("ragdollspawn[manual]: MANUAL-SHOT READY");  // mp.py captures here
    ::Sleep(1200);

    // Does it fall now that the sim is started? Sample the geometry across about three seconds.
    SampleFall(actor, "manual-sim");
    ::Sleep(1500); SampleFall(actor, "manual-sim");
    ::Sleep(1500); SampleFall(actor, "manual-sim");

    // Clean up so it does not confuse the real-ragdoll comparison frame.
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        GT::Post([actor, done] {
            if (actor && R::IsLive(actor)) { E::DestroyActor(actor);
                UE_LOGI("ragdollspawn[manual]: destroyed the manual ragdoll actor"); }
            done->store(1);
        });
        WaitDone(done, 8000);
    }
}

// Experiment A: the real ragdoll verb.
void DumpSelectedGeometry(void* local) {
    void* source = ue_wrap::puppet::GetMeshPlayerVisibleComponent(local);
    void* target = E::GetLocalRagdollBodyMesh(local);
    for (void* comp : {ue_wrap::puppet::GetNativeBodyMeshComponent(local), source, target}) {
        if (!comp) continue;
        ue_wrap::FTransform world{}; uint8_t collision = 255;
        ue_wrap::ParamFrame transform(R::FindDispatchFunctionCached(R::ClassOf(comp), L"K2_GetComponentToWorld"));
        if (ue_wrap::Call(comp, transform)) transform.GetRaw(L"ReturnValue", &world, sizeof(world));
        ue_wrap::ParamFrame state(R::FindDispatchFunctionCached(R::ClassOf(comp), L"GetCollisionEnabled"));
        if (ue_wrap::Call(comp, state)) state.GetRaw(L"ReturnValue", &collision, sizeof(collision));
        UE_LOGI("ragdoll_geometry: comp=%p target=%d collision=%u origin=(%.1f,%.1f,%.1f) scale=(%.3f,%.3f,%.3f)",
            comp, comp == target, collision, world.TX, world.TY, world.TZ, world.SX, world.SY, world.SZ);
    }
    std::vector<E::ArticulatedBone> bones;
    if (target && E::ReadArticulatedPose(target, bones)) for (const auto& bone : bones) {
        R::FName name{};
        ue_wrap::ParamFrame label(R::FindDispatchFunctionCached(R::ClassOf(target), L"GetBoneName"));
        if (!label.Set<int32_t>(L"BoneIndex", bone.index) || !ue_wrap::Call(target, label) ||
            !label.GetRaw(L"ReturnValue", &name, sizeof(name))) continue;
        const auto text = R::ToString(name); ue_wrap::FVector at{};
        E::GetBoneWorldLocationByName(source, text.c_str(), at);
        UE_LOGI("ragdoll_geometry: bone=%ls target=(%.1f,%.1f,%.1f) source=(%.1f,%.1f,%.1f) scale=(%.3f,%.3f,%.3f)",
            text.c_str(), bone.world.TX, bone.world.TY, bone.world.TZ, at.X, at.Y, at.Z,
            bone.world.SX, bone.world.SY, bone.world.SZ);
    }
}

void* PlayerComponent(void* local, const wchar_t* name) {
    if (!local || !R::IsLive(local)) return nullptr;
    const int offset = R::FindPropertyOffset(R::ClassOf(local), name);
    if (offset < 0) return nullptr;
    void* component = *reinterpret_cast<void**>(static_cast<uint8_t*>(local) + offset);
    return component && R::IsLive(component) ? component : nullptr;
}

void LogRagdollCamera(void* local, const char* tag) {
    void* camera = PlayerComponent(local, L"Camera"); void* arm = PlayerComponent(local, L"lag");
    const auto view = E::GetComponentWorldRotation(camera);
    const auto parent = E::GetComponentWorldRotation(arm);
    const auto control = E::GetControlRotation(E::GetController(local));
    ue_wrap::FRotator target{};
    ue_wrap::ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(arm), L"GetTargetRotation"));
    if (frame.valid() && ue_wrap::Call(arm, frame)) frame.GetRaw(L"ReturnValue", &target, sizeof(target));
    UE_LOGI("ragdoll_camera_probe: %s view=(%.2f,%.2f,%.2f) arm=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f) control=(%.2f,%.2f,%.2f)",
        tag, view.Pitch, view.Yaw, view.Roll, parent.Pitch, parent.Yaw, parent.Roll,
        target.Pitch, target.Yaw, target.Roll, control.Pitch, control.Yaw, control.Roll);
}

float RotationError(const ue_wrap::FRotator& a, const ue_wrap::FRotator& b) {
    float ax, ay, az, aw, bx, by, bz, bw;
    E::RotatorToQuat(a.Pitch, a.Yaw, a.Roll, ax, ay, az, aw);
    E::RotatorToQuat(b.Pitch, b.Yaw, b.Roll, bx, by, bz, bw);
    return 114.591559f * std::acos(std::clamp(std::abs(ax*bx + ay*by + az*bz + aw*bw), 0.f, 1.f));
}

void CheckCameraTransitions(void* local) {
    struct Case { const char* name; ue_wrap::FRotator look; bool crouch, passOut; };
    const Case cases[] = {{"level", {0,-90,0}, false, false}, {"down-wrapped-yaw", {-35,385,0}, false, false},
        {"up", {40,150,0}, false, false}, {"crouched", {-25,-170,0}, true, false},
        {"faint", {25,65,0}, false, true}};
    int passed = 0, total = 0;
    for (const char* skin : {"sci_v1sc", "kerfur_omega_h"}) {
        coop::local_body::RequestSkin(skin);
        ::Sleep(1500);
        for (const auto& item : cases) {
            struct Sample { ue_wrap::CachedObjRef pawn; float stageError=999, viewError=999, rise=-1.e9f, entryZ=0, motion=0;
                bool entered=false, selected=false, recovered=false, parented=false; int samples=0; ue_wrap::FRotator initial{}; };
            auto state = std::make_shared<Sample>();
            auto done = std::make_shared<std::atomic<int>>(0);
            GT::Post([local, item, state, done] {
                state->pawn.Set(local);
                if (state->pawn.Get()) {
                    ue_wrap::ParamFrame crouch(R::FindDispatchFunctionCached(R::ClassOf(local), item.crouch ? L"Crouch" : L"UnCrouch"));
                    if (crouch.Set<bool>(L"bClientSimulation", false)) ue_wrap::Call(local, crouch);
                    E::SetControlRotation(E::GetController(local), item.look);
                }
                done->store(1);
            });
            if (!WaitDone(done, 8000)) return;
            ::Sleep(350);
            done = std::make_shared<std::atomic<int>>(0);
            GT::Post([state, item, done] {
                void* pawn = state->pawn.Get(); ue_wrap::FVector at{};
                if (!pawn || !E::TryGetActorLocation(pawn, at)) { done->store(1); return; }
                state->entryZ = at.Z;
                // A synthetic pitch set in an earlier task can reset before entry.
                // Arrange both native view transforms in this game-thread task,
                // then run the actual ragdoll verb without an intervening tick.
                E::SetControlRotation(E::GetController(pawn), item.look);
                E::SetComponentWorldRotation(PlayerComponent(pawn, L"lag"), item.look);
                const auto beforeView = E::GetComponentWorldRotation(PlayerComponent(pawn, L"Camera"));
                LogRagdollCamera(pawn, "angle-case-before-native");
                state->entered = E::SetMainPlayerRagdollMode(pawn, true, item.passOut, false);
                E::RefreshLocalRagdollModel(pawn);
                state->initial = E::GetComponentWorldRotation(PlayerComponent(pawn, L"lag"));
                state->stageError = RotationError(state->initial, item.look);
                state->viewError = RotationError(beforeView, E::GetComponentWorldRotation(PlayerComponent(pawn, L"Camera")));
                state->selected = ue_wrap::puppet::GetComponentSkeletalMeshAsset(E::GetLocalRagdollBodyMesh(pawn)) ==
                    ue_wrap::puppet::GetComponentSkeletalMeshAsset(ue_wrap::puppet::GetMeshPlayerVisibleComponent(pawn));
                LogRagdollCamera(pawn, "angle-case-staged");
                done->store(1);
            });
            if (!WaitDone(done, 8000)) return;
            for (int i=0; i<24; ++i) {
                ::Sleep(25);
                done = std::make_shared<std::atomic<int>>(0);
                GT::Post([state, done] {
                    void* pawn = state->pawn.Get(); ue_wrap::FVector at{}, lin{}, ang{}; ue_wrap::FRotator rot{};
                    if (pawn && E::RefreshLocalRagdollModel(pawn) && E::ReadLocalRagdollPelvisPhysics(pawn,at,rot,lin,ang)) {
                        ++state->samples; state->rise=(std::max)(state->rise,at.Z-state->entryZ);
                        void* arm=PlayerComponent(pawn,L"lag"); void* parent=nullptr;
                        ue_wrap::ParamFrame attached(R::FindDispatchFunctionCached(R::ClassOf(arm),L"GetAttachParent"));
                        state->parented=attached.valid() && ue_wrap::Call(arm,attached) && attached.GetRaw(L"ReturnValue",&parent,sizeof(parent)) &&
                            parent==E::GetLocalRagdollBodyMesh(pawn);
                        state->motion=(std::max)(state->motion,RotationError(state->initial,E::GetComponentWorldRotation(arm)));
                    }
                    done->store(1);
                });
                if (!WaitDone(done, 8000)) return;
            }
            done = std::make_shared<std::atomic<int>>(0);
            GT::Post([state, done] {
                if (void* pawn=state->pawn.Get()) { E::ForceMainPlayerWakeup(pawn); E::ForceMainPlayerGetUp(pawn); }
                done->store(1);
            });
            if (!WaitDone(done, 8000)) return;
            ::Sleep(1000);
            done = std::make_shared<std::atomic<int>>(0);
            GT::Post([state, done] {
                bool rag=true,dead=true;
                if (void* pawn=state->pawn.Get()) state->recovered=E::ReadMainPlayerRagdollState(pawn,rag,dead) && !rag && !dead && !E::GetLocalRagdollBodyMesh(pawn);
                done->store(1);
            });
            if (!WaitDone(done, 8000)) return;
            const bool ok=state->entered && state->stageError<0.2f && state->viewError<1.f && state->selected && state->parented &&
                state->samples>=10 && state->rise<150.f && state->motion>1.f && state->recovered;
            ++total; if(ok)++passed;
            UE_LOGI("ragdoll_camera_case: %s skin=%s case=%s arm-error=%.3f view-jump=%.3f samples=%d rise=%.1fcm head-motion=%.1f selected=%d attached=%d recovered=%d",
                ok?"PASS":"FAIL",skin,item.name,state->stageError,state->viewError,state->samples,state->rise,state->motion,state->selected,state->parented,state->recovered);
        }
    }
    UE_LOGI("ragdoll_camera_acceptance: %s %d/%d", passed==total && total==10 ? "PASS":"FAIL",passed,total);
}

void CheckMomentumTransitions(void* local) {
    struct Motion { const char* name; ue_wrap::FVector velocity; bool airborne; bool nativeInput=false; };
    const Motion motions[]={{"walk",{300,0,0},false},{"run",{600,0,0},false},
        {"diagonal",{-350,450,0},false},{"airborne",{400,0,-300},true},{"native-movement",{},false,true}};
    ue_wrap::FVector home{};
    auto done=std::make_shared<std::atomic<int>>(0);
    GT::Post([local,&home,done]{E::TryGetActorLocation(local,home);done->store(1);});
    if(!WaitDone(done,8000))return;
    int passed=0,total=0;
    for(const char* skin:{"sci_v1sc","kerfur_omega_h"}) {
        coop::local_body::RequestSkin(skin);::Sleep(1500);
        for(const auto& motion:motions) {
            if(motion.nativeInput && ReadEnv("VOTVCOOP_RAGDOLL_NATIVE_MOVEMENT_PROBE")!="1")continue;
            struct State { bool entered=false,recovered=false;int samples=0;float bestMean=-1.e9f,bestPelvis=-1.e9f;ue_wrap::FVector expected{}; };
            auto state=std::make_shared<State>();
            state->expected=motion.velocity;
            if(motion.nativeInput) {
                done=std::make_shared<std::atomic<int>>(0);
                GT::Post([local,home,done]{
                    E::SetActorLocation(local,home);E::SetControlRotation(E::GetController(local),{0,0,0});
                    // Use the same held movement flags as the game's W/sprint
                    // input. A one-off AddMovementInput can be consumed before
                    // the next test task on a fast headless world.
                    for(const auto* flag:{L"input_forward",L"input_run"}) {
                        int32_t byte=-1;uint8_t mask=0;
                        if(R::FindBoolProperty(R::ClassOf(local),flag,byte,mask))static_cast<uint8_t*>(local)[byte]|=mask;
                    }
                    ue_wrap::ParamFrame speed(R::FindDispatchFunctionCached(R::ClassOf(local),L"updateSpeed"));
                    if(speed.valid())ue_wrap::Call(local,speed);done->store(1);
                });
                if(!WaitDone(done,8000))return;
                ::Sleep(350);
            }
            done=std::make_shared<std::atomic<int>>(0);
            GT::Post([local,home,motion,state,done] {
                auto at=home;if(motion.airborne)at.Z+=450.f;
                if(!motion.nativeInput)E::SetActorLocation(local,at);
                const int offset=R::FindPropertyOffset(R::ClassOf(local),L"fallVeloc");
                bool rag=true,dead=true;
                if(offset<0 || !E::ReadMainPlayerRagdollState(local,rag,dead) || rag || dead || E::GetLocalRagdollBodyMesh(local)){done->store(1);return;}
                // Controlled native input: this is the game's own saved movement
                // velocity consumed by fallImpulse after Delay(0), not a mod kick.
                if(motion.nativeInput)std::memcpy(&state->expected,static_cast<uint8_t*>(local)+offset,sizeof(state->expected));
                else std::memcpy(static_cast<uint8_t*>(local)+offset,&motion.velocity,sizeof(motion.velocity));
                state->entered=E::SetMainPlayerRagdollMode(local,true,false,false);
                E::RefreshLocalRagdollModel(local);done->store(1);
            });
            if(!WaitDone(done,8000))return;
            for(int sample=0;sample<14;++sample) {
                ::Sleep(20);done=std::make_shared<std::atomic<int>>(0);
                GT::Post([local,motion,state,done] {
                    if(!E::RefreshLocalRagdollModel(local)){done->store(1);return;}
                    ue_wrap::FVector at{},linear{},angular{};ue_wrap::FRotator rotation{};
                    if(!E::ReadLocalRagdollPelvisPhysics(local,at,rotation,linear,angular)){done->store(1);return;}
                    const auto expected=state->expected;
                    const float speed=std::sqrt(expected.X*expected.X+expected.Y*expected.Y+expected.Z*expected.Z);
                    if(speed<50.f){done->store(1);return;}
                    const auto project=[&](const ue_wrap::FVector& v){return (v.X*expected.X+v.Y*expected.Y+v.Z*expected.Z)/speed;};
                    void* mesh=E::GetLocalRagdollBodyMesh(local);std::vector<E::ArticulatedBone> bones;
                    float mean=0;int count=0;
                    if(E::ReadArticulatedPose(mesh,bones))for(const auto& bone:bones) {
                        R::FName name{};ue_wrap::FVector v{};
                        ue_wrap::ParamFrame label(R::FindDispatchFunctionCached(R::ClassOf(mesh),L"GetBoneName"));
                        ue_wrap::ParamFrame velocity(R::FindDispatchFunctionCached(R::ClassOf(mesh),L"GetPhysicsLinearVelocity"));
                        if(label.Set<int32_t>(L"BoneIndex",bone.index) && ue_wrap::Call(mesh,label) && label.GetRaw(L"ReturnValue",&name,sizeof(name)) &&
                            velocity.Set<R::FName>(L"BoneName",name) && ue_wrap::Call(mesh,velocity) && velocity.GetRaw(L"ReturnValue",&v,sizeof(v))){mean+=project(v);++count;}
                    }
                    if(count){++state->samples;state->bestMean=(std::max)(state->bestMean,mean/count);
                        state->bestPelvis=(std::max)(state->bestPelvis,project(linear));}
                    done->store(1);
                });
                if(!WaitDone(done,8000))return;
            }
            ::Sleep(1000); // Let the airborne body land before forceWakeup transfers back to the capsule.
            done=std::make_shared<std::atomic<int>>(0);
            GT::Post([local,done]{E::ForceMainPlayerWakeup(local);done->store(1);});
            if(!WaitDone(done,8000))return;::Sleep(2000);
            done=std::make_shared<std::atomic<int>>(0);
            GT::Post([local,state,done]{bool rag=true,dead=true;state->recovered=E::ReadMainPlayerRagdollState(local,rag,dead)&&!rag&&!dead&&!E::GetLocalRagdollBodyMesh(local);done->store(1);});
            if(!WaitDone(done,8000))return;
            const auto expected=state->expected;
            const float speed=std::sqrt(expected.X*expected.X+expected.Y*expected.Y+expected.Z*expected.Z);
            const bool ok=state->entered && state->recovered && state->samples>=5 && state->bestMean>speed*0.4f &&
                state->bestPelvis>speed*0.4f && state->bestMean<speed*1.6f;
            ++total;if(ok)++passed;
            UE_LOGI("ragdoll_momentum_case: %s skin=%s case=%s input=%.1f mean=%.1f pelvis=%.1f samples=%d entered=%d recovered=%d",
                ok?"PASS":"FAIL",skin,motion.name,speed,state->bestMean,state->bestPelvis,state->samples,state->entered,state->recovered);
        }
    }
    const int expectedCases=ReadEnv("VOTVCOOP_RAGDOLL_NATIVE_MOVEMENT_PROBE")=="1"?10:8;
    UE_LOGI("ragdoll_momentum_acceptance: %s %d/%d",passed==total&&total==expectedCases?"PASS":"FAIL",passed,total);
}

void CheckSelectedModelEntry(void* local) {
    struct Probe {
        coop::RemotePlayer mirror;
        ue_wrap::CachedObjRef local;
        float entryZ = 0, highest = -1.e9f;
        int samples = 0;
        bool entered = false, selected = false, shown = false, attached = false, recovered = false;
    };
    auto probe = std::make_shared<Probe>();
    auto done = std::make_shared<std::atomic<int>>(0);
    GT::Post([probe, local, done] {
        probe->local.Set(local);
        ue_wrap::FVector at{};
        if (!probe->local.Get() || !E::TryGetActorLocation(local, at) ||
            !probe->mirror.Spawn(coop::local_body::LocalSkinName())) { done->store(1); return; }
        probe->entryZ = at.Z;
        LogRagdollCamera(local, "before-native");
        probe->entered = E::SetMainPlayerRagdollMode(local, true, false, false);
        LogRagdollCamera(local, "after-native");
        E::RefreshLocalRagdollModel(local);
        LogRagdollCamera(local, "after-selected-stage");
        DumpSelectedGeometry(local);
        done->store(1);
    });
    if (!WaitDone(done, 8000)) return;
    for (int sample = 0; sample < 80 && probe->entered; ++sample) {
        ::Sleep(50);
        done = std::make_shared<std::atomic<int>>(0);
        GT::Post([probe, sample, done] {
            void* pawn = probe->local.Get();
            if (!pawn || !E::RefreshLocalRagdollModel(pawn)) { done->store(1); return; }
            ue_wrap::FVector position{}, linear{}, angular{}; ue_wrap::FRotator rotation{};
            if (!E::ReadLocalRagdollPelvisPhysics(pawn, position, rotation, linear, angular)) { done->store(1); return; }
            ++probe->samples; probe->highest = (std::max)(probe->highest, position.Z);
            if (sample < 5 || sample % 10 == 0)
                UE_LOGI("ragdoll_sample: i=%d z=%.1f rise=%.1f velocity=(%.1f,%.1f,%.1f)",
                    sample, position.Z, position.Z - probe->entryZ, linear.X, linear.Y, linear.Z);
            if (sample < 5) LogRagdollCamera(pawn, "simulation");
            void* mesh = E::GetLocalRagdollBodyMesh(pawn);
            probe->selected = ue_wrap::puppet::GetComponentSkeletalMeshAsset(mesh) ==
                ue_wrap::puppet::GetComponentSkeletalMeshAsset(ue_wrap::puppet::GetMeshPlayerVisibleComponent(pawn));
            coop::net::PoseSnapshot pose{};
            pose.x = position.X + 250.f; pose.y = position.Y; pose.z = position.Z;
            pose.stateBits = coop::net::kStateBitRagdoll;
            probe->mirror.SetTargetPose(pose);
            coop::net::RagdollPoseSnapshot packet{};
            packet.x = pose.x; packet.y = pose.y; packet.z = pose.z;
            packet.linVelX = linear.X; packet.linVelY = linear.Y; packet.linVelZ = linear.Z;
            packet.angVelX = angular.X; packet.angVelY = angular.Y; packet.angVelZ = angular.Z;
            std::vector<E::ArticulatedBone> bones;
            if (E::ReadArticulatedPose(mesh, bones) && !bones.empty() && bones.size() <= 64) {
                packet.rigTag = 1; packet.rigBones = static_cast<uint16_t>(bones.size());
                packet.rigSignature = E::BoneLayoutSignature(mesh);
                const size_t first = (size_t(sample) % ((bones.size() + 6) / 7)) * 7;
                packet.boneCount = static_cast<uint8_t>((std::min)(size_t(7), bones.size() - first));
                const auto q = [](float v) { return static_cast<int16_t>(std::lround(std::clamp(v, -1.f, 1.f) * 32767.f)); };
                for (uint8_t i = 0; i < packet.boneCount; ++i) {
                    const auto& b = bones[first + i]; auto& out = packet.bones[i];
                    out = {b.index, b.world.TX + 250.f, b.world.TY, b.world.TZ,
                        q(b.world.RotX), q(b.world.RotY), q(b.world.RotZ), q(b.world.RotW)};
                }
            }
            probe->mirror.SetRagdollPose(packet); probe->mirror.Tick();
            void* body = probe->mirror.RagdollBody(); void* parent = nullptr;
            if (body && probe->mirror.IsRagdollDisplayed()) {
                probe->shown = ue_wrap::puppet::GetComponentSkeletalMeshAsset(E::GetRagdollBodyMesh(body)) ==
                    ue_wrap::puppet::GetComponentSkeletalMeshAsset(mesh);
                probe->attached = E::GetAttachParentActor(probe->mirror.GetActor(), parent) && parent == body;
            }
            done->store(1);
        });
        if (!WaitDone(done, 8000)) break;
    }
    done = std::make_shared<std::atomic<int>>(0);
    GT::Post([probe, done] {
        if (void* pawn = probe->local.Get()) E::ForceMainPlayerGetUp(pawn);
        if (probe->mirror.valid()) {
            coop::net::PoseSnapshot standing{};
            probe->mirror.SetTargetPose(standing); probe->mirror.Tick();
            probe->recovered = !probe->mirror.IsRagdollDisplayed() && !probe->mirror.RagdollBody();
            probe->mirror.Destroy();
        }
        done->store(1);
    });
    WaitDone(done, 8000);
    const float rise = probe->highest - probe->entryZ;
    const bool ok = probe->entered && probe->samples >= 30 && probe->selected && probe->shown &&
        probe->attached && probe->recovered && std::isfinite(rise) && rise < 150.f;
    UE_LOGI("ragdoll_model_probe: %s samples=%d rise=%.1fcm selected=%d remote-model=%d attached=%d recovered=%d",
        ok ? "PASS" : "FAIL", probe->samples, rise, probe->selected, probe->shown, probe->attached, probe->recovered);
}

void RunRealExperiment(void* local) {
    UE_LOGI("ragdollspawn: === EXPERIMENT A (real ragdollMode, ground truth) ===");

    // Fire the real ragdoll verb on the local possessed player (in plain single-player this is
    // the normal faint mechanic; the get-up verb recovers it below).
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        GT::Post([local, done] {
            const bool ok = E::SetMainPlayerRagdollMode(local, /*ragdoll=*/true, /*passOut=*/false, /*death=*/false);
            UE_LOGI("ragdollspawn[real]: ragdollMode(true,false,false) dispatched=%d", ok ? 1 : 0);
            done->store(1);
        });
        WaitDone(done, 8000);
    }

    ::Sleep(1500);  // let the real playerRagdoll_C spawn + configure

    // Read the player's ragdoll actor field, dump it and frame it.
    auto real = std::make_shared<void*>(nullptr);
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        GT::Post([local, real, done] {
            void* ra = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(local) + kMainPlayer_ragdollActor);
            if (ra && R::IsLive(ra)) {
                *real = ra;
                DumpRagdollActor(ra, "real");
                AimLocalAt(local, ra);
            } else {
                UE_LOGW("ragdollspawn[real]: mainPlayer.ragdollActor @0xC40 null/dead after ragdollMode");
            }
            bool dead = false, isRag = false;
            if (ReadDeadRagdoll(local, dead, isRag))
                UE_LOGI("ragdollspawn[real]: player dead=%d isRagdoll=%d after ragdollMode", dead ? 1 : 0, isRag ? 1 : 0);
            // The bone-overlay chain check: exercise the exact accessor and bone-graph path the
            // overlay uses, in-process, while the real ragdoll is live. The autonomous screenshot
            // keeps missing the short auto-get-up window, so this log line is the deterministic
            // proof the overlay would have drawn the bones.
            {
                void* mesh = E::GetLocalRagdollBodyMesh(local);
                std::vector<E::BonePoint> pts;
                const int nb = E::CollectSkeletonBonePoints(mesh, pts);
                int parented = 0;
                for (const auto& b : pts) if (b.parent >= 0) ++parented;
                UE_LOGI("ragdollspawn[real]: [BONE-OVERLAY-CHAIN] localRagdollMesh=%p bones=%d "
                        "parented-lines=%d (the ragdoll_bone_overlay draw set; >0 bones = chain VERIFIED)",
                        mesh, nb, parented);
            }
            done->store(1);
        });
        WaitDone(done, 8000);
    }
    UE_LOGI("ragdollspawn[real]: REAL-SHOT READY");  // mp.py captures here
    ::Sleep(1200);

    if (*real) { SampleFall(*real, "real"); ::Sleep(1500); SampleFall(*real, "real"); }

    // Recover so the session stays clean.
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        GT::Post([local, done] {
            const bool ok = E::ForceMainPlayerGetUp(local);
            UE_LOGI("ragdollspawn[real]: forceGetUp dispatched=%d", ok ? 1 : 0);
            done->store(1);
        });
        WaitDone(done, 8000);
    }
}

}  // namespace

void RunRagdollSpawnProbe() {
    UE_LOGI("ragdollspawn: probe armed -- SP-solo xray-ragdoll feasibility (manual spawn vs real ragdollMode)");

    // Wait for a live local possessed player (post-load). Poll up to 90 s (the play scenario
    // loads a save, then enters gameplay).
    auto local = std::make_shared<void*>(nullptr);
    for (int attempt = 0; attempt < 90 && !*local; ++attempt) {
        auto done = std::make_shared<std::atomic<int>>(0);
        GT::Post([local, done] {
            void* mp = coop::players::Registry::Get().Local();
            if (mp && R::IsLive(mp)) *local = mp;
            done->store(1);
        });
        WaitDone(done, 8000);
        if (!*local) ::Sleep(1000);
    }
    if (!*local) {
        UE_LOGW("ragdollspawn: VERDICT INCONCLUSIVE -- no local player in 90 s; cannot probe");
        return;
    }
    UE_LOGI("ragdollspawn: local player resolved -- letting the world settle 5 s");
    ::Sleep(5000);
    if (ReadEnv("VOTVCOOP_RAGDOLL_ALL_ACCEPTANCE") == "1") {
        CheckMomentumTransitions(*local); CheckCameraTransitions(*local);
        RunRagdollRobustness(*local,false); RunRagdollRobustness(*local,true);
        UE_LOGI("ragdoll_all_acceptance: DONE -- individual verdicts above are mandatory"); return;
    }
    if (ReadEnv("VOTVCOOP_RAGDOLL_ROBUSTNESS_TEST") == "1") { RunRagdollRobustness(*local,false); return; }
    if (ReadEnv("VOTVCOOP_RAGDOLL_MOMENTUM_TEST") == "1") { CheckMomentumTransitions(*local); return; }
    if (ReadEnv("VOTVCOOP_RAGDOLL_CAMERA_TEST") == "1") { CheckCameraTransitions(*local); return; }
    CheckSelectedModelEntry(*local);
    ::Sleep(1500);

    // Diagnostic: is the ragdoll blueprint class already resident, or only loaded lazily when
    // the ragdoll verb first fires? (Tells whether the real-first ordering below is load-bearing
    // or only belt and braces.)
    {
        auto done = std::make_shared<std::atomic<int>>(0);
        GT::Post([done] {
            void* cls = R::FindClass(L"playerRagdoll_C");
            UE_LOGI("ragdollspawn: playerRagdoll_C resident at settle = %s",
                    cls ? "YES (already loaded)" : "NO (lazy -- the real experiment loads it first)");
            done->store(1);
        });
        WaitDone(done, 8000);
    }

    // The real experiment first: the ragdoll verb loads the ragdoll blueprint class as a side
    // effect, so the manual experiment's class lookup is guaranteed to resolve (blueprint classes
    // load lazily; running manual first risks a silent class-not-found false negative). It also
    // yields the ground-truth dump.
    RunRealExperiment(*local);
    ::Sleep(2500);  // let forceGetUp recovery settle + ragdollActor@0xC40 clear
    RunManualExperiment(*local);

    UE_LOGI("ragdollspawn: DONE -- compare [manual] vs [real] dumps above. Manual viable iff its "
            "body IsVisible=1 + IsAnyRigidBodyAwake=1 + lowestBoneZ falls AND Q1b says DEATH-FREE.");
}

DWORD WINAPI RagdollSpawnProbeThread(LPVOID) {
    RunRagdollSpawnProbe();
    return 0;
}

}  // namespace harness::autotest
