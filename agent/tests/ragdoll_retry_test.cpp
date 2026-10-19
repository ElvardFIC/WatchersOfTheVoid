#include "../../src/votv-coop/src/coop/player/remote_player_ragdoll.cpp"
#include <cassert>
#include <cstdio>
#include <thread>
#include <limits>

namespace test {
struct Obj { int index; int serial = 1; bool live = true; } puppet{0}, body{1};
bool spawnOk = false, attachOk = true, visible = true;
int spawns = 0, destroys = 0, detaches = 0;
ue_wrap::FVector pelvis{}, lastLinear{}, lastAngular{};
int translations = 0, velocityWrites = 0;
bool translateOk = true, boneReadable = true;
int articulatedDrives = 0, displayRemovals = 0;
std::vector<ue_wrap::engine::ArticulatedBone> displayedBones;
}
namespace ue_wrap::sleep_display {
void Remove(void*) { ++test::displayRemovals; test::displayedBones.clear(); }
bool DriveArticulated(void*, const std::vector<engine::ArticulatedBone>& bones) {
    ++test::articulatedDrives; test::displayedBones = bones; return true;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
int32_t InternalIndexOf(void* p) { return static_cast<test::Obj*>(p)->index; }
bool IsLiveByIndex(void* p, int32_t i) {
    return (i == 0 ? p == &test::puppet : i == 1 && p == &test::body) && IsLive(p);
}
int32_t SlotSerial(int32_t i) { return i == 0 ? test::puppet.serial : test::body.serial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
}
namespace ue_wrap::world_identity {
void* CurrentWorld() { return nullptr; }
void* WorldOf(void*) { return nullptr; }
}
namespace ue_wrap::log { void Write(Level, const char*, ...) {} }
namespace ue_wrap::puppet {
void* GetNativeBodyMeshComponent(void*) { return &test::puppet; }
void* GetMeshPlayerVisibleComponent(void*) { return &test::puppet; }
}
namespace coop::skin_effects { void SetRigVisible(void*, bool) {} }
namespace ue_wrap::engine {
bool SetSceneComponentVisibility(void* object, bool v, bool) { if(object==&test::puppet)test::visible = v; return true; }
bool TryGetActorLocation(void*, FVector& v) { v = {}; return true; }
bool TryGetActorRotation(void*, FRotator& v) { v = {}; return true; }
void* SpawnPlayerRagdollBody(void*, const FVector&, const FRotator&) {
    ++test::spawns;
    if (!test::spawnOk) return nullptr;
    test::body.live = true;
    return &test::body;
}
bool AttachActorToRagdollBody(void*, void*) { return test::attachOk; }
bool StartPlayerRagdollSimulation(void*) { return true; }
uint32_t BoneLayoutSignature(void*) { return 1; }
bool DetachActorFromRagdollBody(void*) { ++test::detaches; return true; }
bool DestroyActor(void*) { ++test::destroys; test::body.live = false; return true; }
void* GetRagdollBodyMesh(void*) { return &test::body; }
bool GetBoneWorldLocationByName(void*, const wchar_t*, FVector& out) {
    out = test::pelvis; return test::boneReadable;
}
bool TranslateRagdollBody(void*, const FVector& delta) {
    if (!test::translateOk) return false;
    ++test::translations;
    test::pelvis.X += delta.X; test::pelvis.Y += delta.Y; test::pelvis.Z += delta.Z;
    return true;
}
void DriveRagdollBodyPelvisVelocity(void*, const FVector& linear, const FVector& angular) {
    ++test::velocityWrites; test::lastLinear = linear; test::lastAngular = angular;
}
}
int main() {
    coop::RagdollDisplay display;
    using namespace test;
    display.OnWireBit(true, &puppet, 0);
    assert(spawns == 1 && !display.Active() && visible);
    spawnOk = true;
    for (int i = 0; i < 20; ++i) display.OnWireBit(true, &puppet, 0);
    assert(spawns == 1); // Repeated pose packets must not hammer asset loading.
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    display.OnWireBit(true, &puppet, 0);
    assert(spawns == 2 && display.Active() && visible);
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    display.DriveAttached(&puppet, 0);
    assert(!visible); // Animation warmup precedes simulation and standing-body hide.
    coop::net::RagdollPoseSnapshot pose{};
    pose.x = 100; pose.y = 200; pose.z = 300;
    pose.linVelX = 7; pose.angVelY = 9;
    assert(display.SetPose(pose));
    assert(pelvis.X == 100 && pelvis.Y == 200 && pelvis.Z == 300);
    assert(lastLinear.X == 7 && lastAngular.Y == 9);
    pelvis.X = 80;
    assert(display.SetPose(pose) && pelvis.X == 85); // Correct resting drift gradually.
    for (int i = 0; i < 20; ++i) assert(display.SetPose(pose));
    assert(std::fabs(pelvis.X - pose.x) <= 2.f);
    const int atRest = translations;
    display.SetPose(pose);
    assert(translations == atRest); // Dead zone prevents resting jitter.
    pose.x = 5000;
    display.SetPose(pose);
    assert(pelvis.X == 5000); // Large displacement is a teleport.
    const int beforeInvalid = velocityWrites;
    pose.z = std::numeric_limits<float>::quiet_NaN();
    assert(!display.SetPose(pose) && velocityWrites == beforeInvalid);
    pose.z = coop::net::kMaxCoord + 100;
    assert(!display.SetPose(pose) && velocityWrites == beforeInvalid);
    pose.z = 300;
    pose.rigTag = 1; pose.rigSignature = 1; pose.rigBones = 2; pose.boneCount = 1;
    pose.bones[0] = {8, pose.x, pose.y, pose.z, 0, 0, 0, 32767};
    assert(display.SetPose(pose));
    display.DriveAttached(&puppet, 0);
    assert(articulatedDrives == 0); // Wait for a complete authored body set.
    pose.bones[0].index = 2;
    pose.bones[0].x = pose.x + 30;
    assert(display.SetPose(pose));
    display.DriveAttached(&puppet, 0);
    assert(articulatedDrives == 1 && displayedBones.size() == 2);
    assert(displayedBones[0].index == 2 && displayedBones[0].world.TX == pose.x + 30);
    assert(displayedBones[1].index == 8 && displayedBones[1].world.RotW == 1);
    pose.bones[0].qx = 32767; // Invalid, non-unit quaternion must never reach a renderer.
    assert(!display.SetPose(pose));
    pose.bones[0].qx = 0;
    pose.rigTag = 2;
    assert(display.SetPose(pose));
    display.DriveAttached(&puppet, 0);
    assert(articulatedDrives == 1); // New body/model generation cannot reuse old limbs.
    pose.boneCount = 0; pose.rigBones = 0;
    display.OnWireBit(true, &puppet, 0);
    assert(spawns == 2); // A healthy body is never duplicated.
    body.live = false;
    assert(display.DriveAttached(&puppet, 0) == coop::RagdollDisplay::Drive::StoppedNow);
    assert(visible && !display.Active());
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    display.OnWireBit(true, &puppet, 0);
    assert(spawns == 3 && display.Active() && visible);
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    display.DriveAttached(&puppet, 0);
    assert(!visible);
    pelvis = {};
    translateOk = false;
    display.SetPose(pose);
    assert(pelvis.X == 0);
    translateOk = true;
    display.SetPose(pose);
    assert(pelvis.X == 5000); // Recreated body/failed first alignment resets to exact.
    assert(display.OnWireBit(false, &puppet, 0));
    assert(visible && !display.Active() && destroys == 1);
    // A new ragdoll transition starts immediately, without the old retry delay.
    display.OnWireBit(true, &puppet, 0);
    assert(spawns == 4 && display.Active());
    display.TeardownForDestroy();
    assert(!display.Active() && destroys == 2);
    // Failed attachment destroys the orphan and retries on the same wire state.
    attachOk = false;
    display.OnWireBit(true, &puppet, 0);
    assert(!display.Active() && destroys == 3);
    attachOk = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    display.OnWireBit(true, &puppet, 0);
    assert(display.Active());
    display.OnWireBit(false, &puppet, 0);
    assert(visible && !display.Active());
    puts("PASS ragdoll lifecycle/drift, articulated batches, parent order, rig replacement and invalid poses");
}
