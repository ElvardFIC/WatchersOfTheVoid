// Compile the production receiver against deterministic UE/transport doubles. This
// exercises real Tick/OnEnd paths; it does not pretend to simulate Unreal physics.
#include "../../src/votv-coop/src/coop/props/prop_drive_stream.cpp"
#include "../../src/votv-coop/src/coop/props/prop_drive_host.cpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <limits>

// Collision enrollment is covered separately; this harness drives its own actors.
namespace coop::prop_knock {
void Tick(coop::net::Session&) {}
void OnDisconnect() {}
}

// Negative controls must exit with a clear failure, without invoking Windows crash UI.
#undef assert
#define assert(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); std::exit(1); } } while (0)

namespace test {
int actor, mesh;
int serial = 1, meshSerial = 1;
bool present = true, remoteHeld = false, simulates = true, frozen = false;
ue_wrap::engine::MainPlayerGrabState grab{};
int writes = 0, parks = 0, unparks = 0;
float x = 0;
}
namespace ue_wrap::reflection {
std::wstring ClassNameOf(void*) { return L"Aprop_C"; }
int32_t FindParamOffset(void*, const wchar_t*) { return -1; }
void* OuterOf(void*) { return nullptr; }
int32_t InternalIndexOf(void* p) { return p == &test::actor ? 1 : 2; }
bool IsLiveByIndex(void* p, int32_t index) {
    return test::present && ((p == &test::actor && index == 1) || (p == &test::mesh && index == 2));
}
int32_t SlotSerial(int32_t i) { return i == 1 ? test::serial : test::meshSerial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
}
namespace ue_wrap::world_identity {
void* CurrentWorld() { return nullptr; }
void* WorldOf(void*) { return nullptr; }
}
namespace ue_wrap::game_thread { bool IsDefinitelyOffGameThread() { return false; } }
namespace ue_wrap::hot_path {
unsigned long long CallerRva(const void*) { return 0; }
unsigned long CurrentThreadId() { return 1; }
}
namespace ue_wrap::log { void Write(Level, const char*, ...) {} }
namespace ue_wrap::prop {
std::wstring GetInteractableKeyString(void*) { return L"prop1"; }
VelocityState GetPhysicsVelocity(void*, bool) { return {}; }
bool IsGarbageClump(void*) { return false; }
bool IsTrashActor(void*) { return false; }
bool IsTrashBitsPile(void*) { return false; }
bool IsDescendantOfProp(void* p) { return p == &test::actor && test::present; }
bool IsFrozen(void*) { return test::frozen; }
bool IsStatic(void*) { return false; }
void* GetStaticMesh(void*) { return &test::mesh; }
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*, FVector& p) { p = {test::x, 0, 0}; return true; }
bool TryGetActorRotation(void*, FRotator& p) { p = {}; return true; }
bool ReadMainPlayerGrabState(void*, MainPlayerGrabState& out) { out = test::grab; return true; }
bool SetActorLocation(void*, const FVector& p) { ++test::writes; test::x = p.X; return true; }
bool SetActorRotation(void*, const FRotator&) { return true; }
bool IsComponentSimulatingPhysics(void*) { return test::simulates; }
bool SetComponentLinearVelocity(void*, float, float, float) { return true; }
bool SetComponentAngularVelocity(void*, float, float, float) { return true; }
}
namespace coop::remote_prop {
void* ResolveLiveActorByEid(uint32_t eid) { return eid == 1 && test::present ? &test::actor : nullptr; }
bool IsActorUnderAnyDrive(void* p) { return p == &test::actor && test::remoteHeld; }
std::wstring KeyToWString(const net::WireKey&) { return L""; }
}
namespace coop::prop_element_tracker {
void* FindLiveActorByKey(const std::wstring&) { return nullptr; }
element::ElementId GetPropElementIdForActor(void*) { return 1; }
}
namespace coop::element {
Registry::Registry() = default;
Registry::~Registry() = default;
Registry& Registry::Get() { static Registry registry; return registry; }
Element* Registry::Get(ElementId) const { return nullptr; }
}
namespace coop::prop_key_index { void* Find(const std::wstring&) { return nullptr; } }
namespace coop::local_streams { void* LastHeldActor() { return test::grab.gravityActor; } }
namespace coop::npc_sync { bool KerfurIsHolding(void*) { return false; } }
namespace coop::gravity_motion { void* Target() { return nullptr; } }
namespace coop::hand_item { bool IsHandAxisActor(void*) { return false; } }
namespace coop::prop_lifecycle { bool IsWireSuppressedPropClass(const std::wstring&) { return false; } }
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*, const wchar_t*, int, PreFn, PostFn) { return true; }
}
namespace coop::prop_park {
void Park(void*, void*) { ++test::parks; test::simulates = false; }
void Unpark(void*, void*, bool simulate) { ++test::unparks; if (simulate) test::simulates = true; }
}
namespace coop::prop_wire_parity {
uint8_t PhysFlagsOf(void*) { return 0; }
void ConvergeFrozenSleep(void*, uint8_t) {}
bool SpParitySimulate(uint8_t) { return true; }
void RestoreSpParityPhysicsAfterConverge(void*, uint8_t) { test::simulates = true; }
}

int main() {
    namespace D = coop::prop_drive_stream;
    coop::net::Session session;
    auto reset = [&] {
        D::OnDisconnect(); test::present = true; test::remoteHeld = false;
        test::grab = {}; test::simulates = true; test::frozen = false;
        test::writes = test::parks = test::unparks = 0;
        session.incoming.clear();
    };
    auto pose = [&](uint8_t gen, float x) {
        coop::net::PropPoseSnapshot p{}; p.elementId = 1; p.ctx = gen; p.x = x;
        session.incoming.push_back(p); D::TickApplyAndDrive(session);
    };
    auto end = [&](uint8_t gen, float x) {
        coop::net::PropDriveEndPayload p{}; p.eid = 1; p.gen = gen; p.x = x;
        D::OnEnd(p);
    };
    // Cosmetic velocity is the derivative of the same delayed render segment,
    // not a velocity written into the parked body's physics.
    namespace AD = coop::active_drive;
    AD::ActiveDrive visual;
    visual.isTrashMirror=visual.lerpSeeded=visual.haveTwoSnaps=true;
    visual.prevPoseMs=100;visual.lastPoseMs=200;visual.lastLoc={100,0,-20};
    auto v=AD::PresentationVelocity(visual,199);assert(v.X==0&&v.Z==0);
    v=AD::PresentationVelocity(visual,200);assert(v.X==1000&&v.Z==-200);
    v=AD::PresentationVelocity(visual,250);assert(v.X==1000);
    visual.actor=&test::actor;visual.actorIdx=1;
    AD::AdvanceLerp(visual,250);assert(test::x==50);
    AD::AdvanceLerp(visual,260);assert(std::fabs(test::x-60)<0.001f);
    AD::AdvanceLerp(visual,300);assert(test::x==100);
    v=AD::PresentationVelocity(visual,300);assert(v.X==0);
    visual.lastPoseMs=104;visual.lastLoc={4,0,0};
    assert(AD::PresentationVelocity(visual,115).X==0);
    assert(AD::PresentationVelocity(visual,116).X==1000);
    assert(AD::PresentationVelocity(visual,120).X==0);
    visual.lastPoseMs=1100;visual.lastLoc={100,0,0};
    assert(AD::PresentationVelocity(visual,1000).X==100);
    assert(AD::PresentationVelocity(visual,1300).X==0);
    visual.lastLoc.X=std::numeric_limits<float>::quiet_NaN();
    assert(AD::PresentationVelocity(visual,1000).X==0);
    visual.lastLoc={};visual.lastPoseMs=visual.prevPoseMs;
    assert(AD::PresentationVelocity(visual,1000).X==0);
    AD::BeginLerpToPose(visual,{1000,0,0},{},2000);
    assert(!visual.haveTwoSnaps&&AD::PresentationVelocity(visual,2000).X==0);
    reset();pose(40,40);
    assert(D::TryGetPresentationVelocity(&test::actor,v)&&v.X==0);
    auto& moving=D::g_drives.at(1).d;const auto now=AD::NowMs();
    moving.haveTwoSnaps=true;moving.prevPoseMs=now-100;moving.lastPoseMs=now;
    moving.prevLoc={};moving.lastLoc={100,0,0};
    assert(D::TryGetPresentationVelocity(&test::actor,v)&&v.X==1000);
    assert(!D::TryGetPresentationVelocity(&test::mesh,v));
    ++test::serial;assert(!D::TryGetPresentationVelocity(&test::actor,v));
    reset();pose(40,40);end(40,40);assert(!D::TryGetPresentationVelocity(&test::actor,v));
    std::puts("PASS presentation velocity: delayed motion, interval clamps, stop/teleport, invalid samples, ended and recycled drives");
    // Duplicate/old reliable ends must neither teleport nor regress the watermark.
    reset(); end(9, 90); const auto writes = test::writes;
    end(9, 99); end(8, 80); pose(9, 900);
    assert(test::writes == writes && test::x == 90 && !D::IsParked(&test::actor));
    pose(10, 100); assert(D::IsParked(&test::actor) && test::x == 100);
    end(9, -1); assert(test::x == 100);
    end(10, 101); assert(!D::IsParked(&test::actor) && test::x == 101 && test::simulates);

    // The end arriving before spawn retains only the newest final pose.
    reset(); test::present = false; end(10, 100); end(9, 90); end(10, 101);
    test::present = true; D::TickApplyAndDrive(session); assert(test::x == 100);
    reset(); test::present = false; end(10, 100);
    test::present = true; end(11, 110); D::TickApplyAndDrive(session);
    assert(test::x == 110); // an old pending end must not undo a newer end applied immediately
    reset(); test::present = false; end(10, 100);
    test::present = true; pose(11, 110); assert(test::x == 110 && D::IsParked(&test::actor));

    // Every local carry path yields before writing a pose and restores PHC simulation.
    for (int mode = 0; mode < 3; ++mode) {
        reset(); pose(20, 20);
        if (mode == 0) test::grab.grabbingActor = &test::actor;
        if (mode == 1) test::grab.gravityActor = &test::actor;
        if (mode == 2) test::grab.holdingActor = &test::actor;
        const auto before = test::writes;
        D::TickApplyAndDrive(session);
        assert(!D::IsParked(&test::actor) && test::simulates && test::writes == before);
        test::grab = {}; pose(20, 200); end(20, 200);
        assert(test::writes == before); // the yielded generation stays closed after release
        pose(21, 21); assert(test::x == 21 && D::IsParked(&test::actor));
    }
    reset(); pose(22, 22); test::grab.grabbingActor = &test::actor;
    auto before = test::writes; end(22, 220);
    assert(test::simulates && test::writes == before && !D::IsParked(&test::actor));
    reset(); test::grab.gravityActor = &test::actor; pose(23, 23);
    test::grab = {}; end(23, 230); assert(test::writes == 0); // held on first datagram

    // A remote hand keeps its own park; this stream must not turn physics on under it.
    reset(); pose(30, 30); test::remoteHeld = true; before = test::writes;
    pose(30, 300); assert(!test::simulates && test::unparks == 0 && test::writes == before);
    test::remoteHeld = false; end(30, 300); assert(test::writes == before);

    // Same address AND object-array slot reused: the cached serial must still reject it.
    reset(); pose(40, 40); before = test::writes; ++test::serial;
    assert(!D::IsParked(&test::actor)); D::TickApplyAndDrive(session);
    assert(test::writes == before && test::unparks == 0);
    reset(); pose(40, 40); before = test::writes; ++test::serial;
    test::grab.grabbingActor = &test::actor; pose(41, 41);
    assert(test::writes == before && test::unparks == 0); // old park does not own this successor's physics
    reset(); pose(42, 42); test::frozen = true; pose(43, 43);
    test::grab.grabbingActor = &test::actor; D::TickApplyAndDrive(session);
    assert(test::unparks == 1 && !test::simulates); // release rider bookkeeping, retain frozen physics
    reset(); pose(255, 255); end(255, 256); pose(1, 1); end(255, 999);
    assert(D::IsParked(&test::actor) && test::x == 1); // generation wrap
    D::OnDisconnect();
    namespace H = coop::prop_drive_host;
    reset(); H::OnDisconnect(); session.ends.clear(); session.published.clear();
    H::Coast(&test::actor, "test"); H::Tick(session);
    assert(H::g_driven.size() == 1 && session.published.size() == 1);
    auto generation = H::g_driven[0].gen;
    H::Coast(&test::actor, "player support"); assert(H::g_driven[0].gen == generation);
    // A brief take/drop that happens entirely between host ticks still changes generation.
    H::Coast(&test::actor, "drop", true);
    assert(coop::drive_generation::After(H::g_driven[0].gen, generation));
    H::Tick(session);
    H::g_driven[0].lastSendMs = H::g_driven[0].lastMoveMs = coop::active_drive::NowMs() - 2000;
    session.sendOk = false; H::Tick(session);
    assert(H::g_driven.size() == 1 && session.ends.empty()); // failed end is retained
    // Another very slow tick sends a heartbeat AND must still send the pending rest edge.
    H::g_driven[0].lastSendMs = coop::active_drive::NowMs() - 2000;
    session.sendOk = true; H::Tick(session);
    assert(H::g_driven.empty() && session.ends.size() == 1);
    reset(); H::Claim(&test::actor, "hook"); H::Tick(session);
    H::g_driven[0].lastSendMs = H::g_driven[0].lastMoveMs = coop::active_drive::NowMs() - 2000;
    H::Tick(session); assert(H::g_driven.size() == 1); // resting claim stays active
    test::grab.gravityActor = &test::actor; H::Tick(session);
    assert(H::g_driven.empty()); // host reads the current hold before local_streams runs
    H::OnDisconnect();
    std::puts("PASS: production receiver/host: reordered ends, late spawn, local/remote holds, recycled slot, wrap, quick drop, slow ticks, send retry");
}
