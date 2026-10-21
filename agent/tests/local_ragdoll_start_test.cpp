#include "ue_wrap/engine/engine.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <thread>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
namespace test {
struct Obj { int id, serial = 1; bool live = true; char storage[0xc48]{}; } objects[6];
Obj& player = objects[0]; Obj& source = objects[1]; Obj& target = objects[2];
Obj& asset = objects[3]; Obj& body = objects[4]; Obj& replacement = objects[5];
void* world = &player;
void* currentTarget = &target;
void* currentSource = &source;
void* currentAsset = &asset;
ue_wrap::FVector pelvis{10, 20, 5}, sourcePelvis{10, 20, 50}, origin{}, startedAt{}, drivenLinear{}, drivenAngular{};
bool readable = true, configured = true, translated = true, velocityReadable = true;
int configurations = 0, starts = 0, drives = 0, moves = 0;
std::function<void()> onRead, onConfigure, onMove, onStart;
void Body(void* value) { std::memcpy(reinterpret_cast<char*>(&player) + 0xc40, &value, sizeof(value)); }
void Warm() { std::this_thread::sleep_for(std::chrono::milliseconds(35)); }
}
namespace ue_wrap::reflection {
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
int32_t InternalIndexOf(void* p) { return static_cast<test::Obj*>(p)->id; }
bool IsLiveByIndex(void* p, int32_t i) { return i >= 0 && i < 6 && p == &test::objects[i] && IsLive(p); }
int32_t SlotSerial(int32_t i) { return i >= 0 && i < 6 ? test::objects[i].serial : 0; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
void* ClassOf(void* p) { return p; }
void* FindDispatchFunctionCached(void*, const wchar_t* name) { return const_cast<wchar_t*>(name); }
}
namespace ue_wrap::world_identity {
void* CurrentWorld() { return test::world; }
void* WorldOf(void* p) { return p == &test::asset ? nullptr : &test::player; }
}
namespace ue_wrap::puppet {
void* GetMeshPlayerVisibleComponent(void*) { return test::currentSource; }
void* GetComponentSkeletalMeshAsset(void*) { return test::currentAsset; }
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f) : fn_(f), buf_(128) {}
bool ParamFrame::SetRaw(const wchar_t* n, const void* data, int32_t bytes) {
    const std::wstring name(n);
    if (name == L"NewLocation" || name == L"DeltaLocation") std::memcpy(buf_.data(), data, bytes);
    if (name == L"bSweep") CHECK(!*static_cast<const bool*>(data));
    if (name == L"bTeleport") CHECK(*static_cast<const bool*>(data));
    return valid();
}
bool Call(void*, ParamFrame& f) {
    using namespace test;
    ++moves;
    if (!translated) return false;
    FVector value{}; std::memcpy(&value, f.data(), sizeof(value));
    if (std::wstring(static_cast<wchar_t*>(f.function())) == L"K2_SetWorldLocation") {
        pelvis = {pelvis.X + value.X - origin.X, pelvis.Y + value.Y - origin.Y, pelvis.Z + value.Z - origin.Z};
        origin = value;
    } else {
        CHECK(std::wstring(static_cast<wchar_t*>(f.function())) == L"K2_AddWorldOffset");
        pelvis = {pelvis.X + value.X, pelvis.Y + value.Y, pelvis.Z + value.Z};
    }
    if (onMove) onMove();
    return true;
}
}
namespace ue_wrap::engine {
namespace R = reflection;
constexpr size_t kMainPlayer_ragdollActor = 0xc40;
bool StartBodySim(void*) {
    ++test::starts; test::startedAt = test::pelvis;
    if (test::onStart) test::onStart();
    return true;
}
void* GetLocalRagdollBodyMesh(void*) { return test::currentTarget; }
FVector GetComponentLocation(void*) { return test::origin; }
bool GetBoneWorldLocationByName(void* mesh, const wchar_t*, FVector& value) {
    value = mesh == &test::source ? test::sourcePelvis : test::pelvis;
    if (test::onRead) test::onRead();
    return test::readable;
}
bool ConfigurePlayerRagdollMesh(void*, void*) {
    ++test::configurations; test::pelvis = {100, 200, 300};
    if (test::onConfigure) test::onConfigure();
    return test::configured;
}
bool ReadLocalRagdollPelvisPhysics(void*, FVector&, FRotator&, FVector& linear, FVector& angular) {
    linear = {1, 2, 3}; angular = {4, 5, 6};
    return test::velocityReadable;
}
void DriveRagdollBodyPelvisVelocity(void* actor, const FVector& linear, const FVector& angular) {
    CHECK(actor == &test::body); ++test::drives;
    test::drivenLinear = linear; test::drivenAngular = angular;
}
#include "local_ragdoll_start_production.inc"
}
namespace test {
void Reset() {
    currentTarget = nullptr; ue_wrap::engine::RefreshLocalRagdollModel(&player);
    for (int i = 0; i < 6; ++i) { objects[i] = {}; objects[i].id = i; }
    world = &player; currentTarget = &target; currentSource = &source; currentAsset = &asset; Body(&body);
    pelvis = {10, 20, 5}; sourcePelvis = {10, 20, 50}; origin = startedAt = drivenLinear = drivenAngular = {};
    readable = configured = translated = velocityReadable = true;
    configurations = starts = drives = moves = 0;
    onRead = onConfigure = onMove = onStart = {};
}
bool Refresh() { return ue_wrap::engine::RefreshLocalRagdollModel(&player); }
void Ready() { CHECK(!Refresh()); CHECK(starts == 0); Warm(); pelvis = {400, 500, 600}; }
}
int main() {
    using namespace test;
    Reset(); Ready(); CHECK(Refresh());
    CHECK(startedAt.X == 10 && startedAt.Y == 20 && startedAt.Z == 50);
    CHECK(drives == 1 && drivenLinear.Z == 3 && drivenAngular.Z == 6);
    CHECK(Refresh()); CHECK(starts == 1 && configurations == 1 && moves == 1);
    Reset(); sourcePelvis.Z = 30; Ready(); sourcePelvis.Z = 90;
    CHECK(Refresh()); CHECK(startedAt.Z == 30); // crouched entry is neither shell nor later standing height
    // A missing warm pose or failed teleport must not enable physics; retry
    // retains the selected model's entry anchor rather than its temporary pose.
    Reset(); Ready(); readable = false; CHECK(!Refresh()); CHECK(starts == 0);
    readable = true; translated = false; CHECK(!Refresh()); CHECK(starts == 0);
    translated = true; CHECK(Refresh()); CHECK(startedAt.Z == 50);
    Reset(); configured = false; CHECK(!Refresh()); CHECK(starts == 0);
    configured = true; Ready(); CHECK(Refresh()); CHECK(startedAt.Z == 50);
    // Reject non-finite transforms without a move/start.
    Reset(); sourcePelvis.Z = std::numeric_limits<float>::infinity(); CHECK(!Refresh()); CHECK(configurations == 0);
    Reset(); Ready(); pelvis.Z = std::numeric_limits<float>::quiet_NaN(); CHECK(!Refresh()); CHECK(starts == 0 && moves == 0);
    // Dispatch may replace the pawn/body/model or tear down the world. Never
    // start/drive a successor using the previous episode's anchor and velocity.
    for (int failure = 0; failure < 6; ++failure) {
        Reset(); Ready();
        onRead = [failure] {
            if (failure == 0) ++target.serial;
            if (failure == 1) ++player.serial;
            if (failure == 2) currentAsset = &replacement;
            if (failure == 3) currentSource = &replacement;
            if (failure == 4) Body(&replacement);
            if (failure == 5) world = &replacement;
        };
        CHECK(!Refresh()); CHECK(starts == 0 && moves == 0 && drives == 0);
    }
    Reset(); Ready(); onMove = [] { ++target.serial; }; CHECK(!Refresh()); CHECK(starts == 0 && drives == 0);
    Reset(); Ready(); onStart = [] { Body(&replacement); }; CHECK(!Refresh()); CHECK(drives == 0);
    Reset(); onConfigure = [] { ++source.serial; }; CHECK(!Refresh()); CHECK(starts == 0 && moves == 0);
    Reset(); velocityReadable = false; Ready(); CHECK(Refresh()); CHECK(drivenLinear.Z == 0 && drivenAngular.Z == 0);
    puts("PASS selected-rig standing/crouched entry height, warm-pose anchoring, retry and lifetime guards");
}
