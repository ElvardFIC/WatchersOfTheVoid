// Compile the production receiver against deterministic UE/transport doubles. This
// exercises real Tick/OnEnd paths; it does not pretend to simulate Unreal physics.
#include "../../src/votv-coop/src/coop/element/bind_wait.cpp"
#include "../../src/votv-coop/src/coop/props/prop_drive_stream.cpp"
#include "../../src/votv-coop/src/coop/props/prop_drive_host.cpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <functional>

// Collision enrollment is covered separately; this harness drives its own actors.
namespace coop::prop_knock {
void Tick(coop::net::Session&) {}
void OnDisconnect() {}
}

// Negative controls must exit with a clear failure, without invoking Windows crash UI.
#undef assert
#define assert(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); std::exit(1); } } while (0)

namespace test {
int actor, mesh, mesh2, classObject;
void* currentMesh=&mesh;
int serial = 1, meshSerial = 1;
uint32_t world = 1;
bool present = true, remoteHeld = false, simulates = true, frozen = false;
bool bound = true;
bool handAxis=false, lastHeld=false;
ue_wrap::engine::MainPlayerGrabState grab{};
int writes = 0, parks = 0, unparks = 0;
int linearWrites = 0, angularWrites = 0;
ue_wrap::FVector linear{}, angular{};
float x = 0;
int rotationWrites=0, childReleases=0, trigger=0;
bool locationOK=true, rotationOK=true;
std::vector<int> nativeSteps;
std::function<void()> onNative;
void Fire(int step) {nativeSteps.push_back(step);if(step==trigger){auto call=std::move(onNative);onNative={};if(call)call();}}
}
namespace ue_wrap::reflection {
void* ClassOf(void* p) { return p ? &test::classObject : nullptr; }
std::wstring ClassNameOf(void*) { return L"Aprop_C"; }
int32_t FindParamOffset(void*, const wchar_t*) { return -1; }
void* OuterOf(void*) { return nullptr; }
int32_t InternalIndexOf(void* p) { return p == &test::actor ? 1 : p == &test::mesh ? 2 : p == &test::mesh2 ? 3 : 4; }
bool IsLiveByIndex(void* p, int32_t index) {
    return (p == &test::classObject && index == 4) || (test::present && ((p == &test::actor && index == 1) || (p == &test::mesh && index == 2) || (p == &test::mesh2 && index == 3)));
}
bool IsLive(void* p) { return IsLiveByIndex(p, InternalIndexOf(p)); }
int32_t SlotSerial(int32_t i) { return i == 4 ? 1 : i == 1 ? test::serial : test::meshSerial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
}
namespace ue_wrap::world_identity {
uint32_t Generation() { return test::world; }
void* CurrentWorld() { return nullptr; }
void* WorldOf(void*) { return nullptr; }
}
namespace coop::net::session_serial { uint32_t Current() { return 1; } }
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
void* GetStaticMesh(void*) { return test::currentMesh; }
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*, FVector& p) { p = {test::x, 0, 0}; return true; }
bool TryGetActorRotation(void*, FRotator& p) { p = {}; return true; }
bool ReadMainPlayerGrabState(void*, MainPlayerGrabState& out) { out = test::grab; return true; }
bool SetActorLocation(void*, const FVector& p) { ++test::writes; test::x = p.X; test::Fire(1);return test::locationOK; }
bool SetActorRotation(void*, const FRotator&) { ++test::rotationWrites;test::Fire(2);return test::rotationOK; }
bool IsComponentSimulatingPhysics(void*) {test::Fire(8);return test::simulates; }
bool SetComponentLinearVelocity(void*, float x, float y, float z) { ++test::linearWrites; test::linear={x,y,z}; test::Fire(5);return true; }
bool SetComponentAngularVelocity(void*, float x, float y, float z) { ++test::angularWrites; test::angular={x,y,z}; test::Fire(6);return true; }
}
namespace coop::remote_prop {
void* ResolveLiveActorByEid(uint32_t eid) { return eid == 1 && test::present && test::bound ? &test::actor : nullptr; }
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
namespace coop::local_streams { void* LastHeldActor() { return test::lastHeld ? &test::actor : test::grab.gravityActor; } }
namespace coop::npc_sync { bool KerfurIsHolding(void*) { return false; } }
namespace coop::gravity_motion { void* Target() { return nullptr; } }
namespace coop::hand_item { bool IsHandAxisActor(void* actor) { return actor==&test::actor && test::handAxis; } }
namespace coop::prop_lifecycle { bool IsWireSuppressedPropClass(const std::wstring&) { return false; } }
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*, const wchar_t*, int, PreFn, PostFn) { return true; }
}
namespace coop::prop_park {
void Park(void*, void*) { ++test::parks; test::simulates = false;test::Fire(9); }
void Unpark(void*, void*, bool simulate) { ++test::unparks; if (simulate) test::simulates = true; test::Fire(7); }
bool Park(void* actor,void* mesh,const std::function<bool()>& current){if(current&&!current())return false;Park(actor,mesh);return !current||current();}
bool Unpark(void* actor,void* mesh,bool simulate,const std::function<bool()>& current){if(current&&!current())return false;Unpark(actor,mesh,simulate);return !current||current();}
}
namespace coop::prop_wire_parity {
uint8_t PhysFlagsOf(void*) { return 0; }
void ConvergeFrozenSleep(void*, uint8_t) {test::Fire(3);}
bool SpParitySimulate(uint8_t) { return true; }
void RestoreSpParityPhysicsAfterConverge(void*, uint8_t) { test::simulates = true;test::Fire(4); }
bool ConvergeFrozenSleep(void* actor,uint8_t flags,const std::function<bool()>& current){if(current&&!current())return false;ConvergeFrozenSleep(actor,flags);return !current||current();}
bool RestoreSpParityPhysicsAfterConverge(void* actor,uint8_t flags,const std::function<bool()>& current){if(current&&!current())return false;RestoreSpParityPhysicsAfterConverge(actor,flags);return !current||current();}
}

int main() {
    namespace D = coop::prop_drive_stream;
    namespace BW = coop::element::bind_wait;
    coop::net::Session session;
    auto reset = [&] {
        test::onNative={};test::trigger=0;
        BW::Clear("fixture reset"); D::OnDisconnect(); test::present = true; test::remoteHeld = false;
        test::grab = {}; test::simulates = true; test::frozen = false;
        test::writes = test::parks = test::unparks = 0;
        test::linearWrites = test::angularWrites = 0; test::linear = test::angular = {};
        test::bound = true; session.mode = coop::net::Role::Client;
        session.incoming.clear();
        test::rotationWrites=test::childReleases=0;test::nativeSteps.clear();test::locationOK=test::rotationOK=true;
        test::currentMesh=&test::mesh;
        test::lastHeld=test::handAxis=false;
    };
    auto pose = [&](uint8_t gen, float x) {
        coop::net::PropDriveRow p{}; p.elementId = 1; p.ctx = gen; p.x = x;
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
    reset(); test::present = false; BW::MarkDeferred(1,"test spawn"); end(10, 100); end(9, 90); end(10, 101);
    test::present = true; BW::OnBound(1); BW::Pump(); D::TickApplyAndDrive(session); assert(test::x == 100);
    reset(); test::present = false; BW::MarkDeferred(1,"test spawn"); end(10, 100);
    test::present = true; end(11, 110); D::TickApplyAndDrive(session);
    assert(test::x == 110); // an old pending end must not undo a newer end applied immediately
    reset(); test::present = false; BW::MarkDeferred(1,"test spawn"); end(10, 100);
    test::present = true; pose(11, 110); assert(test::x == 110 && D::IsParked(&test::actor));

    // Every local carry path yields before writing a pose and restores PHC simulation.
    for (int mode = 0; mode < 5; ++mode) {
        reset(); pose(20, 20);
        if (mode == 0) test::grab.grabbingActor = &test::actor;
        if (mode == 1) test::grab.gravityActor = &test::actor;
        if (mode == 2) test::grab.holdingActor = &test::actor;
        if (mode == 3) test::lastHeld = true;
        if (mode == 4) test::handAxis = true;
        const auto before = test::writes;
        D::TickApplyAndDrive(session);
        assert(!D::IsParked(&test::actor) && test::simulates && test::writes == before);
        test::grab = {}; test::lastHeld=test::handAxis=false;pose(20, 200); end(20, 200);
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
    // A packet can arrive before Tick retires the old object-array occupant.
    reset(); pose(40, 40); before = test::writes; ++test::serial; test::bound=false;
    pose(40, 1000); assert(test::writes == before && test::unparks == 0);
    reset(); pose(40, 40); before = test::writes; ++test::serial; test::bound=false;
    coop::net::PropDriveEndPayload retired{}; retired.eid=1; retired.gen=40; retired.x=400;
    D::ReleaseHost(retired); assert(test::writes == before && test::unparks == 0);
    reset(); pose(40, 40); before = test::writes; ++test::serial;
    test::grab.grabbingActor = &test::actor; pose(41, 41);
    assert(test::writes == before && test::unparks == 0); // old park does not own this successor's physics
    reset(); pose(42, 42); test::frozen = true; pose(43, 43);
    test::grab.grabbingActor = &test::actor; D::TickApplyAndDrive(session);
    assert(test::unparks == 1 && !test::simulates); // release rider bookkeeping, retain frozen physics
    reset(); pose(255, 255); end(255, 256); pose(1, 1); end(255, 999);
    assert(D::IsParked(&test::actor) && test::x == 1); // generation wrap
    // Resting translation can still carry angular momentum; all-zero ends stay asleep.
    for (int motion=0; motion<4; ++motion) {
        reset(); pose(50,50);
        coop::net::PropDriveEndPayload final{}; final.eid=1; final.gen=50; final.x=50;
        if (motion&1) final.linVelX=20;
        if (motion&2) final.angVelZ=90;
        D::OnEnd(final);
        assert(test::linearWrites==(motion!=0) && test::angularWrites==(motion!=0));
        assert(test::linear.X==final.linVelX && test::angular.Z==final.angVelZ);
    }
    std::puts("PASS prop lifetime and release: recycled slot before pose/host end, pure spin, translation, combined motion and sleeping rest");
    // Host release uses the same generation/watermark contract as the receiver.
    reset();pose(60,60);pose(61,61);before=test::writes;
    coop::net::PropDriveEndPayload final{};final.eid=1;final.gen=60;final.x=600;
    D::ReleaseHost(final);assert(D::IsParked(&test::actor)&&test::writes==before);
    final.gen=61;final.x=61;D::ReleaseHost(final);before=test::writes;
    D::ReleaseHost(final);pose(61,610);assert(test::writes==before&&!D::IsParked(&test::actor));
    // Every native seam may retire this continuation. No subsequent setter or
    // bookkeeping release is permitted on a reused body, world or stopped scope.
    for(int step=1;step<=7;++step)for(int cause=0;cause<4;++cause){
        reset();pose(60,60);test::nativeSteps.clear();final={};final.eid=1;final.gen=60;final.x=600;final.linVelX=10;
        test::trigger=step;test::onNative=[&,cause]{
            if(cause==0)D::OnDisconnect();if(cause==1)++test::serial;if(cause==2)++test::world;if(cause==3)test::remoteHeld=true;
        };
        D::OnEnd(final);assert(!test::onNative&&test::nativeSteps.back()==step);
    }
    // A callback can create a newer drive or a newer end on the same eid.
    for(int step=1;step<=7;++step)for(int successor=0;successor<2;++successor){
        reset();pose(60,60);final={};final.eid=1;final.gen=60;final.x=600;final.linVelX=10;
        test::trigger=step;test::onNative=[&,successor]{
            if(successor){auto next=final;next.gen=62;next.x=62;next.linVelX=0;D::OnEnd(next);}
            else {coop::net::PropDriveRow next{};next.elementId=1;next.ctx=61;next.x=61;D::FeedAdmitted(next);}
        };
        D::OnEnd(final);assert(!test::onNative&&test::x==(successor?62:61));
        assert(D::IsParked(&test::actor)==!successor);
        if(!successor)assert(!test::simulates&&D::g_drives.at(1).gen==61);
    }
    reset();pose(60,60);final={};final.eid=1;final.gen=60;final.linVelX=10;
    test::trigger=5;test::onNative=[]{++test::meshSerial;};D::OnEnd(final);
    assert(test::linearWrites==1&&!test::angularWrites&&!test::unparks);
    for(int cause=0;cause<2;++cause){reset();pose(60,60);test::nativeSteps.clear();
        test::locationOK=cause!=0;test::rotationOK=cause!=1;final={};final.eid=1;final.gen=60;D::OnEnd(final);
        assert(test::nativeSteps.back()==cause+1&&!test::unparks);}
    // Bad direct input must leave the live generation open and untouched.
    for(int bad=0;bad<8;++bad){reset();pose(60,60);before=test::writes;final={};final.eid=1;final.gen=60;
        if(bad==0)final.x=std::numeric_limits<float>::quiet_NaN();if(bad==1)final.angVelZ=std::numeric_limits<float>::infinity();
        if(bad==2)final.y=coop::net::kMaxCoord+1;if(bad==3)final.pitch=181;if(bad==4)final.gen=0;
        if(bad==5)final._pad[0]=1;if(bad==6)final._pad[1]=1;if(bad==7)final.physFlags=0x80;
        D::OnEnd(final);assert(test::writes==before&&D::IsParked(&test::actor));}
    std::puts("PASS end ownership: host generations, native retirement, successor drive/end, component reuse, refused writes and direct ingress");
    reset();pose(60,60);final={};final.eid=1;final.gen=60;final.linVelX=10;
    test::trigger=1;test::onNative=[&]{final.gen=61;final.linVelX=999;};D::OnEnd(final);
    assert(test::linear.X==10&&test::unparks==1);
    // First-pose callbacks must neither rotate a retired actor nor erase a
    // successor's revision. The transport batch is owned through disconnect.
    for(int step : {1,2,9})for(int successor=0;successor<2;++successor){
        reset();test::trigger=step;test::onNative=[&,successor]{
            if(successor){coop::net::PropDriveRow next{};next.elementId=1;next.ctx=61;next.x=61;D::FeedAdmitted(next);}
            else D::OnDisconnect();
        };
        pose(60,60);assert(!test::onNative);
        if(successor)assert(D::g_drives.at(1).gen==61&&test::x==61&&!test::simulates);
        else assert(D::g_drives.empty()&&test::rotationWrites==(step==2));
    }
    // Same-generation reentry is ordered by application revision as well.
    reset();test::trigger=1;test::onNative=[]{coop::net::PropDriveRow next{};next.elementId=1;next.ctx=60;next.x=600;D::FeedAdmitted(next);};
    pose(60,60);assert(test::x==600&&D::g_drives.at(1).d.lastLoc.X==600);
    reset();pose(60,60);test::trigger=1;test::onNative=[]{coop::net::PropDriveRow next{};next.elementId=1;next.ctx=60;next.x=2000;D::FeedAdmitted(next);};
    pose(60,1000);assert(test::x==2000&&D::g_drives.at(1).d.lastLoc.X==2000);
    // A native query or interpolation setter can retire the tick's map entry.
    for(int step : {1,2,8,9})for(int successor=0;successor<2;++successor){
        reset();pose(60,60);auto& drive=D::g_drives.at(1).d;auto tick=AD::NowMs();
        drive.prevPoseMs=tick-100;drive.lastPoseMs=tick;drive.prevLoc={};drive.lastLoc={100,0,0};drive.haveTwoSnaps=true;
        test::simulates=step==9;test::trigger=step;test::onNative=[&,successor]{
            if(successor){coop::net::PropDriveRow next{};next.elementId=1;next.ctx=61;next.x=61;D::FeedAdmitted(next);}
            else D::OnDisconnect();
        };
        D::TickApplyAndDrive(session);assert(!test::onNative);
        if(successor)assert(D::g_drives.at(1).gen==61&&test::x==61&&!test::simulates);
        else assert(D::g_drives.empty()&&test::rotationWrites==1+(step==2));
    }
    // Component replacement is observed even while its predecessor stays live.
    reset();pose(60,60);test::currentMesh=&test::mesh2;test::simulates=true;D::TickApplyAndDrive(session);
    assert(D::g_drives.at(1).d.mesh==&test::mesh2&&!test::simulates&&test::parks==2);
    // A frame-tail callback may call the tick again; it must leave new input queued.
    reset();pose(60,60);test::trigger=8;test::onNative=[&]{coop::net::PropDriveRow next{};next.elementId=1;next.ctx=61;next.x=61;
        session.incoming.push_back(next);D::TickApplyAndDrive(session);assert(session.incoming.size()==1);};
    D::TickApplyAndDrive(session);assert(session.incoming.size()==1);D::TickApplyAndDrive(session);assert(D::g_drives.at(1).gen==61);
    // Giving a local hand its physics back cannot close a successor installed by
    // the unpark callback or issue its child-release notification.
    reset();pose(60,60);test::grab.grabbingActor=&test::actor;test::trigger=7;test::onNative=[]{test::grab={};
        coop::net::PropDriveRow next{};next.elementId=1;next.ctx=61;next.x=61;D::FeedAdmitted(next);};
    D::TickApplyAndDrive(session);assert(D::g_drives.at(1).gen==61&&test::x==61&&!test::simulates&&test::childReleases==0);
    for(int bad=0;bad<5;++bad){reset();pose(60,60);before=test::writes;coop::net::PropDriveRow next{};next.elementId=1;next.ctx=61;
        if(bad==0)next.x=std::numeric_limits<float>::quiet_NaN();if(bad==1)next.ctx=0;if(bad==2)next.key.len=255;
        if(bad==3)next.z=coop::net::kMaxCoord+1;if(bad==4)next.roll=181;D::FeedAdmitted(next);
        assert(test::writes==before&&D::g_drives.at(1).gen==60);}
    std::puts("PASS pose/tick ownership: first and same-generation reentry, native query/park/render retirement, mesh replacement, nested tick and hand successor");
    D::OnDisconnect();
    namespace H = coop::prop_drive_host;
    reset(); session.mode=coop::net::Role::Host; H::OnDisconnect(); session.ends.clear(); session.published.clear();
    session.online.fill(false); session.online[1]=true;
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
    assert(H::g_driven.empty() && session.ends.empty() && coop::prop_end_delivery::g_pending.count(1));
    // The recipient debt survives row retirement; force its next retry due without sleeping.
    coop::prop_end_delivery::g_pending.at(1)->recipients[1].attempted=false;
    session.sendOk = true; H::Tick(session);
    assert(H::g_driven.empty() && session.ends.size() == 1);
    reset(); session.mode=coop::net::Role::Host; H::Claim(&test::actor, "hook"); H::Tick(session);
    H::g_driven[0].lastSendMs = H::g_driven[0].lastMoveMs = coop::active_drive::NowMs() - 2000;
    H::Tick(session); assert(H::g_driven.size() == 1); // resting claim stays active
    test::grab.gravityActor = &test::actor; H::Tick(session);
    assert(H::g_driven.empty()); // host reads the current hold before local_streams runs
    H::OnDisconnect();
    std::puts("PASS: production receiver/host: reordered ends, late spawn, local/remote holds, recycled slot, wrap, quick drop, slow ticks, send retry");
}

namespace coop::dev::prop_syncer_drill { bool IsEnabled(){return false;} }
namespace coop::prop_child_release { void OnParentReleased(void*){++test::childReleases;} void Tick(uint64_t){} void OnDisconnect(){} }
namespace coop::prop_syncer { void Tick(net::Session&,bool){} bool SkipsHostPose(uint32_t){return false;} void OnDisconnect(){} }
namespace coop::prop_syncer_host { void OnRowOpened(uint32_t){} void Admit(net::Session&){} void OnDisconnect(){} void EndClientRow(prop_drive_host::Driven&,Ending){} }

namespace ue_wrap::engine {
void* RootComponentOf(void*) { return test::currentMesh; }
bool GetActorRootLinearVelocity(CachedObjRef& root,void*,FVector& out){root.Set(test::currentMesh);out=test::linear;return true;}
}
