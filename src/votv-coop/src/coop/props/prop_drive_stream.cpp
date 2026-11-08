// coop/props/prop_drive_stream.cpp -- see coop/props/prop_drive_stream.h.

#include "coop/props/prop_drive_stream.h"

#include "coop/dev/prop_syncer_drill.h"        // IsEnabled: the drill's view is kept only while it is on
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/element/registry.h"
#include "coop/player/local_prop_hold.h"
#include "coop/props/active_drive.h"
#include "coop/props/drive_generation.h"
#include "coop/props/prop_element_tracker.h"  // FindLiveActorByKey: the index only, never the cold walk
#include "coop/props/prop_key_index.h"
#include "coop/element/bind_wait.h"            // Hold / NoteNeverBound: an end whose prop has no bind yet
#include "coop/element/registry.h"           // IsCorrectionStale: the eid's bound actor
#include "coop/props/active_drive.h"
#include "coop/props/prop_drive_host.h"        // InAHand: who is a hand
#include "coop/props/prop_child_release.h"      // OnParentReleased: a jointed door waits for its parent
#include "coop/props/prop_key_index.h"  // Find: the index only, never the cold walk
#include "coop/props/prop_park.h"             // Park / Unpark: the parked prop, and a Character welded on it
#include "coop/props/prop_syncer.h"           // SkipsHostPose: the prop this peer authors
#include "coop/props/prop_wire_parity.h"      // the join converge's own physics restore
#include "coop/player/local_streams.h"
#include "coop/player/hand_item.h"
#include "coop/props/remote_prop.h"           // ResolveLiveActorByEid, IsActorUnderAnyDrive
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::prop_drive_stream {
namespace {

namespace AD = coop::active_drive;
namespace E  = ue_wrap::engine;
namespace PR = ue_wrap::prop;
namespace R  = ue_wrap::reflection;

struct Drive {
    AD::ActiveDrive d;
    ue_wrap::CachedObjRef actorRef, meshRef, clsRef, meshCls;
    Source source;
    uint32_t serial = 0;
    uint32_t world = 0;
    void* LiveActor() const {
        void* actor = world == ue_wrap::world_identity::Generation() ? actorRef.Get() : nullptr;
        return actor && clsRef.Get() && R::ClassOf(actor) == clsRef.Get() ? actor : nullptr;
    }
    uint8_t gen = 0;
    bool physicsParked = false;   // this module turned simulation off, so the end turns it on
    uint32_t relatches = 0;       // the game switched the parked copy's simulation back on, this park
    uint64_t revision = 0;
};

// A park whose copy the game switches back to simulating this many times is being fought every
// tick, not nudged once: said loudly, once, as the held-prop drive says it (remote_prop).
constexpr uint32_t kRelatchLoud = 60;
std::unordered_map<uint32_t, Drive>   g_drives;    // eid -> drive
uint64_t g_epoch = 0; // native continuations cannot outlive a disconnect
uint64_t g_revision = 0;
bool g_ticking = false;
coop::net::Session* g_session = nullptr;
uint64_t g_stops = 0;
uint32_t g_serial = 0, g_world = 0;
coop::net::Role g_role = coop::net::Role::Host;
struct Context {
    coop::net::Session* session = g_session;
    uint64_t epoch = g_epoch, stops = g_stops;
    uint32_t world = ue_wrap::world_identity::Generation(), serial = coop::net::session_serial::Current();
    coop::net::Role role = g_role;
    bool Current() const {
        return epoch == g_epoch && session == g_session && world == ue_wrap::world_identity::Generation() &&
            serial == coop::net::session_serial::Current() && (!session ||
                (session->connected() && stops == session->StopCount() && role == session->role()));
    }
};
bool BindSession(coop::net::Session& session) {
    const auto serial = coop::net::session_serial::Current(), world = ue_wrap::world_identity::Generation();
    const auto stops = session.StopCount();
    if (g_session && (g_session != &session || g_serial != serial || g_world != world ||
        g_stops != stops || g_role != session.role() || !session.connected())) {
        OnDisconnect();
        if (g_session || serial != coop::net::session_serial::Current() ||
            world != ue_wrap::world_identity::Generation() || stops != session.StopCount()) return false;
    }
    if (!session.connected()) return false;
    if (!g_session) ++g_epoch;
    g_session = &session; g_serial = serial; g_world = world; g_stops = stops; g_role = session.role();
    return Context{}.Current();
}
bool CurrentSource(const Source& source) { return !source.current || source.current(); }
bool TargetMatches(const Source& source, void* actor) {
    return CurrentSource(source) && (!source.actor.Raw() || (source.actor.Get() == actor &&
        source.cls.Get() && R::ClassOf(actor) == source.cls.Get()));
}
bool LifetimeCurrent(const Drive& drive) {
    return drive.serial == coop::net::session_serial::Current() &&
        (!drive.source.lifetime || drive.source.lifetime());
}
bool TrustedEpisode(const Source& source) {
    return source.session && source.session->role() == coop::net::Role::Host &&
        source.episode && source.current && source.lifetime && source.actor.Get() && source.cls.Get();
}
void ReceiveEnd(const coop::net::PropDriveEndPayload& input, Source sourceGuard);
std::unordered_map<uint32_t, uint8_t> g_endedGen;  // eid -> the generation its end edge closed
// eid -> the generation whose END pose ApplyEnd wrote, and onto which actor: captured with its slot
// serial, so a successor re-created in the same slot does not read as the actor the pose was written on.
struct EndOnActor { uint8_t gen = 0; ue_wrap::CachedObjRef actor; };
std::unordered_map<uint32_t, EndOnActor> g_endOnActor;
// eid -> a generation a hand took over before its end edge landed. Its end pose is older than
// whatever the hand did since, so a late end of it closes the generation and writes nothing.
std::unordered_map<uint32_t, uint8_t> g_yieldedGen;

// A pose whose prop has not arrived here is retried on a throttle, never per tick: the host
// re-sends every tick the prop moves, and though the resolve is two O(1) lookups, a line per
// miss is not. One line names a sustained miss.
struct Miss { uint64_t nextTryMs = 0; uint32_t count = 0; };
std::unordered_map<uint32_t, Miss> g_miss;
constexpr uint64_t kMissRetryMs = 250;
constexpr uint32_t kMissSaidAt  = 40;     // about ten seconds of misses: one line

// The drained batch, kept across ticks for its capacity: the session swaps its buffer with this
// one, so the steady state of a drag allocates nothing on either thread. Game thread only.
std::vector<coop::net::PropDriveRow> g_batch;

// Newer-than on the wrapping byte the stream carries.
bool GenAfter(uint8_t a, uint8_t b) { return drive_generation::After(a, b); }
bool Held(void* actor) {
    return coop::prop_drive_host::InAHand(actor);
}

// A frozen or static prop keeps its physics off whatever the stream did: it is moved kinematically
// and left so, the stuck wall-attachable rule the held-prop receiver applies.
bool PhysicsStaysOff(void* actor, const std::function<bool()>& current = {}) {
    if (current && !current()) return true;
    const bool frozen = PR::IsFrozen(actor);
    if ((current && !current()) || frozen) return true;
    const bool fixed = PR::IsStatic(actor);
    return (current && !current()) || fixed;
}

// By eid first -- the registry's O(1) row, which a joiner's keyed copy carries from the bind --
// then the key INDEX; never the cold object-array walk the general key resolver falls back to.
void* Resolve(uint32_t eid, const coop::net::WireKey& key, const std::function<bool()>& current = {}) {
    void* actor = coop::remote_prop::ResolveLiveActorByEid(eid);
    if (current && !current()) return nullptr;
    if (actor) return actor;
    if (key.len == 0) return nullptr;
    return coop::prop_key_index::Find(coop::remote_prop::KeyToWString(key));
}

void GiveBackPhysics(Drive& dr, void* actor, const std::function<bool()>& caller = {}) {
    if (!dr.physicsParked || !actor) return;
    const Context context;
    const auto current = [&] { return context.Current() && (!caller || caller()) && dr.LiveActor() == actor; };
    const bool simulate = !PhysicsStaysOff(actor, current);
    if (!current()) return;
    void* mesh = PR::GetStaticMesh(actor);
    if (!current()) return;
    if (coop::prop_park::Unpark(actor, mesh, simulate, current)) dr.physicsParked = false;
}

void YieldToHand(uint32_t eid, uint8_t gen, Drive* dr, void* actor, const std::function<bool()>& caller = {});
void YieldToHand(uint32_t eid, uint8_t gen, void* actor, Drive* dr, const std::function<bool()>& caller = {}) { YieldToHand(eid,gen,dr,actor,caller); }

// The one way a stream gives a prop up to a hand, from the tick, the first pose and the end edge
// alike: this player's own hand gets back the physics the park took, which no peer's release will
// restore, and the generation is closed as superseded, so neither a pose nor the end of it can
// write its stale pose once the hand lets go.
bool IsOwnHand(void* actor) {
    return local_prop_hold::IsHeld(actor) || actor == coop::local_streams::LastHeldActor() || coop::hand_item::IsHandAxisActor(actor);
}
// Both records only ever move forward. The tick can yield an OLDER generation's row in the same
// pass that the first pose yielded a newer one, and writing the older one back reopened the newer
// generation: its end then found no yield and parked the stale rest pose after the hand let go.
void AdvanceGen(std::unordered_map<uint32_t, uint8_t>& m, uint32_t eid, uint8_t gen) {
    auto it = m.find(eid);
    if (it == m.end()) m.emplace(eid, gen);
    else if (GenAfter(gen, it->second)) it->second = gen;
}
void YieldToHand(uint32_t eid, uint8_t gen, Drive* dr, void* actor, const std::function<bool()>& caller) {
    if (caller && !caller()) return;
    AdvanceGen(g_endedGen, eid, gen);
    AdvanceGen(g_yieldedGen, eid, gen);
    const auto epoch = g_epoch;
    const auto world = ue_wrap::world_identity::Generation();
    ue_wrap::CachedObjRef ref, cls; ref.Set(actor); cls.Set(R::ClassOf(actor));
    const auto current = [&] { return (!caller || caller()) && ref.Get() && cls.Get() &&
        R::ClassOf(actor) == cls.Get() && epoch == g_epoch && world == ue_wrap::world_identity::Generation() && !g_drives.contains(eid); };
    if (dr && actor && current()) {
        const bool own = IsOwnHand(actor);
        if (own && current()) GiveBackPhysics(*dr, actor, current);
    }
    if (current())
        coop::prop_child_release::OnParentReleased(actor);   // the stream let go: a waiting door goes next
}
bool YieldedAtOrAfter(uint32_t eid, uint8_t gen) {
    auto y = g_yieldedGen.find(eid);
    return y != g_yieldedGen.end() && !GenAfter(gen, y->second);
}

// What the prop syncer drill's judge reads (prop_drive_stream.h, dev::ViewOf): per eid, the rows of the last
// generation, where the copy stood before its park, and whether that generation's End applied. Null (and nothing
// kept) unless the drill row is on.
std::unordered_map<uint32_t, dev::DriveView> g_devView;
dev::DriveView* DevViewOf(uint32_t eid, uint8_t gen) {
    if (!coop::dev::prop_syncer_drill::IsEnabled()) return nullptr;
    dev::DriveView& v = g_devView[eid];
    if (v.gen != gen) v = dev::DriveView{gen};
    return &v;
}

// Rows that reached the receive store per eid, counted on the net thread before the store keeps the newest per eid
// (dev::NoteMerged); against the view's rows (taken and applied) it tells coalescing from loss.
std::mutex g_mergedMu;
std::unordered_map<uint32_t, uint32_t> g_merged;

// The last stream ends applied here, a ring of kRecentEnds (prop_drive_stream.h).
AppliedEnd g_ends[kRecentEnds];
size_t g_endCount = 0;   // ends recorded since the session began; the newest is at (g_endCount - 1) % kRecentEnds

// The end edge on a live prop: the final pose, then the host's physics flags through the same
// parity restore the join's converge applies after its teleport, then the velocity -- only into
// a body this module parked and the flags leave simulating, and only when it is not zero, since
// assigning a velocity wakes a body at rest.
void ApplyEnd(void* actor, const coop::net::PropDriveEndPayload& p, bool parkedHere, const Source& source = {}) {
    const auto epoch = g_epoch;
    const auto world = ue_wrap::world_identity::Generation();
    const Context context;
    ue_wrap::CachedObjRef owner, cls; owner.Set(actor); cls.Set(R::ClassOf(actor));
    const auto basic = [&] {
        if (!context.Current() || !TargetMatches(source, actor) || !context.Current()) return false;
        const auto ended = g_endedGen.find(p.eid);
        return epoch == g_epoch && world == ue_wrap::world_identity::Generation() && owner.Get() == actor &&
            cls.Get() && R::ClassOf(actor) == cls.Get() &&
            ended != g_endedGen.end() && ended->second == p.gen && !g_drives.contains(p.eid) &&
            !YieldedAtOrAfter(p.eid, p.gen);
    };
    const auto current = [&] {
        if (!basic()) return false;
        const bool held = Held(actor);
        return basic() && !held;
    };
    // The row and closed watermark are committed before native dispatch. Native
    // calls can install a successor drive, transfer to a hand or tear down the world.
    if (!current() || !E::SetActorLocation(actor, ue_wrap::FVector{p.x, p.y, p.z}) || !current() ||
        !E::SetActorRotation(actor, ue_wrap::FRotator{p.pitch, p.yaw, p.roll}) || !current()) return;
    // The host's frozen and sleep at the end first: a hook that bites a frozen prop unfreezes it on
    // the host only (hook_C's attach_a runs setPropProps), so a copy still frozen here takes the
    // host's flags before the physics decision reads them.
    if (!coop::prop_wire_parity::ConvergeFrozenSleep(actor, p.physFlags, current)) return;
    if (!current()) return;
    if (!PhysicsStaysOff(actor, current)) {
        if (!current()) return;
        if (!coop::prop_wire_parity::RestoreSpParityPhysicsAfterConverge(actor, p.physFlags, current)) return;
        if (!current()) return;
        // Zero is authoritative too: a parked body may retain velocity from before the stream.
        if (parkedHere && coop::prop_wire_parity::SpParitySimulate(p.physFlags)) {
            void* mesh = PR::GetStaticMesh(actor);
            ue_wrap::CachedObjRef component, componentClass; component.Set(mesh); componentClass.Set(R::ClassOf(mesh));
            const auto live = [&] {return current() && component.Get() && componentClass.Get() &&
                R::ClassOf(component.Get()) == componentClass.Get() && PR::GetStaticMesh(actor) == component.Get();};
            if (!live()) return;
            if (!ue_wrap::engine::SetComponentLinearVelocity(mesh, p.linVelX, p.linVelY, p.linVelZ)) return;
            if (!live()) return;
            if (!ue_wrap::engine::SetComponentAngularVelocity(mesh, p.angVelX, p.angVelY, p.angVelZ)) return;
            if (!live()) return;
        }
    }
    // The park is over, whatever the flags left the body: a Character it stopped moves again.
    if (!current()) return;
    if (!coop::prop_park::Unpark(actor, /*mesh=*/nullptr, /*rootSimulates=*/false, current)) return;
    if (!current()) return;
    coop::prop_child_release::OnParentReleased(actor);
    if (!current()) return;
    if (dev::DriveView* v = DevViewOf(p.eid, p.gen)) v->endApplied = true;
    EndOnActor& on = g_endOnActor[p.eid];
    on.gen = p.gen; on.actor.Set(actor);
    g_ends[g_endCount++ % kRecentEnds] = AppliedEnd{p.eid, ue_wrap::FVector{p.x, p.y, p.z}, ::GetTickCount64()};
}

// One pose of a driven prop: parks the copy on a new generation's first, a lerp target after. The client's
// apply of the host's batch, and the host's of a pose it admitted from a prop's syncer (FeedAdmitted); `who`
// names the role in the log.
void ApplyPose(const coop::net::PropDriveRow& input, uint64_t nowMs, const char* who, Source sourceGuard = {}) {
    const auto e = input;
    const uint32_t eid = e.elementId;
    if (!eid || !e.ctx || e.key.len > sizeof(e.key.data) || !coop::net::PropPoseInBounds(e)) return;
    for (float value : {e.x,e.y,e.z,e.pitch,e.yaw,e.roll}) if (!std::isfinite(value)) return;
    const auto epoch = g_epoch;
    const auto world = ue_wrap::world_identity::Generation();
    const Context context;
    const auto scope = [&] {return context.Current() && CurrentSource(sourceGuard) && context.Current() &&
        epoch == g_epoch && world == ue_wrap::world_identity::Generation();};
    if (!scope()) return;
    const bool trusted = TrustedEpisode(sourceGuard);
    // The generation gate: a pose of a closed generation, or older than the live drive's,
    // is a datagram that outlived its stream.
    auto ended = g_endedGen.find(eid);
    if (!trusted && ended != g_endedGen.end() && !GenAfter(e.ctx, ended->second)) return;
    auto it = g_drives.find(eid);
    if (it != g_drives.end()) {
        if (!trusted && GenAfter(it->second.gen, e.ctx)) return;
        if (it->second.gen == e.ctx) {
            // The steady state: the row already names the actor. No resolve, no string.
            if (void* live = it->second.LiveActor()) {
                it->second.revision = ++g_revision;
                Drive owned = it->second;
                owned.source = sourceGuard; owned.source.current = {};
                owned.serial = coop::net::session_serial::Current();
                const auto basic = [&] {
                    if (!scope() || !TargetMatches(sourceGuard, live) || !owned.LiveActor()) return false;
                    const auto found = g_drives.find(eid);
                    return owned.LiveActor() == live && found != g_drives.end() && found->second.revision == owned.revision;
                };
                const auto current = [&] {
                    if (!basic()) return false;
                    const bool held = Held(live);
                    if (!basic() || held) return false;
                    if (!owned.d.mesh) return true;
                    void* mesh = PR::GetStaticMesh(live);
                    return basic() && owned.meshRef.Get() && owned.meshCls.Get() &&
                        R::ClassOf(owned.meshRef.Get()) == owned.meshCls.Get() && mesh == owned.meshRef.Get();
                };
                if (current()) {   // a hand's stream wins
                    if (dev::DriveView* v = DevViewOf(eid, e.ctx)) {
                        ++v->rows;
                        v->lastRowLoc = ue_wrap::FVector{e.x, e.y, e.z};
                    }
                    AD::BeginLerpToPose(owned.d, ue_wrap::FVector{e.x, e.y, e.z},
                                        ue_wrap::FRotator{e.pitch, e.yaw, e.roll}, nowMs, current);
                    if (current()) g_drives.at(eid) = std::move(owned);
                }
                return;
            }
        }
    }
    // A first pose, a new generation, or a row whose actor died: resolve, on a throttle.
    auto miss = g_miss.find(eid);
    if (miss != g_miss.end() && nowMs < miss->second.nextTryMs) return;
    void* actor = Resolve(eid, e.key, scope);
    if (!scope()) return;
    if (!actor) {
        Miss& m = g_miss[eid];
        m.nextTryMs = nowMs + kMissRetryMs;
        if (++m.count == kMissSaidAt)
            UE_LOGI("[PROP-DRIVE] %s eid=%u has not resolved in %u tries -- the host is "
                    "streaming a prop this peer does not have (a join still landing, or a "
                    "prop it never received)", who, eid, m.count);
        return;
    }
    if (!TargetMatches(sourceGuard, actor)) return;
    g_miss.erase(eid);
    ue_wrap::CachedObjRef ref, cls; ref.Set(actor); cls.Set(R::ClassOf(actor));
    const auto source = [&] {return scope() && TargetMatches(sourceGuard, actor) && ref.Get() == actor &&
        cls.Get() && R::ClassOf(actor) == cls.Get();};
    if (!source() || !PR::IsDescendantOfProp(actor) || !source()) return;
    // A hand's stream on the same prop wins: the holder is its syncer for as long as it
    // holds it, and the host closes this stream the moment it sees the hand.
    const bool held = coop::prop_drive_host::InAHand(actor);
    if (!source()) return;
    if (held) {
        // The first pose found the prop already in a hand: the generation is the hand's.
        YieldToHand(eid, e.ctx, nullptr, actor, source);
        UE_LOGI("[PROP-DRIVE] %s eid=%u gen=%u -- a hand holds it at the first pose; "
                "generation yielded", who, eid, static_cast<unsigned>(e.ctx));
        return;
    }
    // Recheck generations after the resolver: its key catch-up can reenter.
    ended = g_endedGen.find(eid);
    if (!source() || (!trusted && ended != g_endedGen.end() && !GenAfter(e.ctx, ended->second))) return;
    it = g_drives.find(eid);
    if (!trusted && it != g_drives.end() && GenAfter(it->second.gen, e.ctx)) return;
    Drive previous = it == g_drives.end() ? Drive{} : it->second;
    Drive dr = previous;
    const bool alreadyParked=dr.physicsParked && dr.actorRef.Is(actor);
    AD::ResetDriveState(dr.d);
    dr.actorRef.Set(actor);
    dr.clsRef = cls; dr.source = sourceGuard; dr.source.current = {};
    dr.serial = coop::net::session_serial::Current();
    dr.world = world;
    dr.d.actor    = actor;
    dr.d.actorIdx = R::InternalIndexOf(actor);
    dr.d.mesh     = PR::GetStaticMesh(actor);
    if (!source()) return;
    dr.meshRef.Set(dr.d.mesh);
    dr.meshCls.Set(R::ClassOf(dr.d.mesh));
    dr.d.isTrashMirror = true;  // the fixed-delay follow of a carried clump: freeze on a gap
    dr.d.lastEid  = eid;
    dr.gen        = e.ctx;
    dr.revision   = ++g_revision;
    dr.physicsParked = alreadyParked;
    dr.relatches  = 0;
    if (!source()) return;
    const auto basic = [&] {
        if (!source()) return false;
        const auto found = g_drives.find(eid);
        return found != g_drives.end() && found->second.revision == dr.revision;
    };
    const auto current = [&] {
        if (!basic()) return false;
        const bool held = Held(actor);
        if (!basic() || held) return false;
        if (!dr.d.mesh) return true;
        void* mesh = PR::GetStaticMesh(actor);
        return basic() && dr.meshRef.Get() && dr.meshCls.Get() &&
            R::ClassOf(dr.meshRef.Get()) == dr.meshCls.Get() && mesh == dr.meshRef.Get();
    };
    g_drives[eid] = dr;
    if (previous.LiveActor() && previous.LiveActor() != actor) GiveBackPhysics(previous, previous.LiveActor(), current);
    if (!current()) return;
    if (dev::DriveView* seen = DevViewOf(eid, e.ctx)) {
        // Never retain the diagnostic map's address over a native read.
        ue_wrap::FVector at{};
        const bool read = E::TryGetActorLocation(actor, at);
        if (!current()) return;
        seen = DevViewOf(eid, e.ctx);
        if (seen) {seen->startRead = read;seen->startLoc = at;++seen->rows;seen->lastRowLoc = {e.x,e.y,e.z};}
    }
    if (dr.d.mesh && !PhysicsStaysOff(actor, current)) {
        if (!current()) return;
        dr.physicsParked = true;
        g_drives.at(eid).physicsParked = true;
        const bool parked = coop::prop_park::Park(actor, dr.d.mesh, current);
        if (!current() || !parked) return;
    }
    // No iterator from before a native call survives to this commit.
    g_endedGen.erase(eid);
    g_yieldedGen.erase(eid);
    // The parked newer generation supersedes the END pose a correction was judged against.
    g_endOnActor.erase(eid);
    // A pose of a closed generation never reaches here, so an end held for this prop (bind_wait) is of
    // an OLDER generation than the one parking now: its replay finds the drive parked and drops it as stale.
    UE_LOGI("[PROP-DRIVE] %s park eid=%u gen=%u actor=%p (physics %s)",
            who, eid, static_cast<unsigned>(e.ctx), actor,
            dr.physicsParked ? "off" : "left as it was");
    AD::BeginLerpToPose(dr.d, ue_wrap::FVector{e.x, e.y, e.z},
                        ue_wrap::FRotator{e.pitch, e.yaw, e.roll}, nowMs, current);
    if (current()) g_drives.at(eid).d = std::move(dr.d);
}

}  // namespace

size_t RecentEnds(AppliedEnd* out, size_t max) {
    const size_t have = g_endCount < kRecentEnds ? g_endCount : kRecentEnds;
    size_t n = 0;
    for (; n < have && n < max; ++n) out[n] = g_ends[(g_endCount - 1 - n) % kRecentEnds];
    return n;
}

void FeedAdmitted(const coop::net::PropDriveRow& e) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::FeedAdmitted");
    ApplyPose(e, AD::NowMs(), "HOST");
}
void FeedAdmitted(const coop::net::PropDriveRow& input, const Source& source) {
    const auto e = input;
    const Source owned = source;
    if (!CurrentSource(owned) || !owned.session || !BindSession(*owned.session) || !CurrentSource(owned)) return;
    ApplyPose(e, AD::NowMs(), "HOST", owned);
}

void ReleaseHost(const coop::net::PropDriveEndPayload& end) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::ReleaseHost");
    OnEnd(end);
}
void ReleaseHost(const coop::net::PropDriveEndPayload& input, const Source& source) {
    const auto end = input;
    const Source owned = source;
    if (!CurrentSource(owned) || !owned.session || !BindSession(*owned.session) || !CurrentSource(owned)) return;
    ReceiveEnd(end, owned);
}

void TickApplyAndDrive(coop::net::Session& s) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::TickApplyAndDrive");
    if (!BindSession(s) || g_ticking) return;
    struct Busy {Busy(){g_ticking=true;}~Busy(){g_ticking=false;}} busy;
    const auto epoch = g_epoch;
    const auto world = ue_wrap::world_identity::Generation();
    const Context context;
    const auto scope = [&] {return context.Current() && epoch == g_epoch && world == ue_wrap::world_identity::Generation();};
    const uint64_t nowMs = AD::NowMs();
    std::vector<coop::net::PropDriveRow> batch;
    batch.swap(g_batch);
    batch.clear();
    // The host's own batch slot is never written (a client's rows go to the origin stores), so only a client drains it.
    if (s.role() != coop::net::Role::Host && s.TakeRemotePropDriveBatch(batch)) {
        for (const coop::net::PropDriveRow& e : batch) {
            // The one skip: a prop this peer authors (a claim pending or granted) is moved by this peer's own
            // physics, and the host's poses for it are its own rows relayed, or a coast it parks onto once a
            // claim is refused. Skipping closes no generation, and an End is never skipped (OnEnd).
            if (coop::prop_syncer::SkipsHostPose(e.elementId)) continue;
            Source source;
            source.session = &s;
            source.current = source.lifetime = [context] { return context.Current(); };
            ApplyPose(e, nowMs, "CLIENT", source);
            if (!scope()) return;
        }
    }
    g_batch.swap(batch);
    std::vector<uint32_t> keys;
    keys.reserve(g_drives.size());
    for (const auto& [eid,dr] : g_drives) keys.push_back(eid);
    for (uint32_t eid : keys) {
        auto it = g_drives.find(eid);
        if (it == g_drives.end()) continue;
        Drive dr = it->second;
        if (!LifetimeCurrent(dr)) continue; // keep the park lease for an authoritative End
        if (!scope()) return;
        it = g_drives.find(eid);
        if (it == g_drives.end() || it->second.revision != dr.revision) continue;
        dr.revision = it->second.revision = ++g_revision;
        const auto basic = [&] {
            if (!scope() || !LifetimeCurrent(dr) || !scope()) return false;
            const auto found = g_drives.find(eid);
            return scope() && found != g_drives.end() && found->second.revision == dr.revision &&
                dr.LiveActor();
        };
        const auto current = [&] {
            if (!basic()) return false;
            if (!dr.d.mesh) return true;
            void* mesh = PR::GetStaticMesh(dr.LiveActor());
            return basic() && dr.meshRef.Get() && dr.meshCls.Get() &&
                R::ClassOf(dr.meshRef.Get()) == dr.meshCls.Get() && mesh == dr.meshRef.Get();
        };
        void* actor = dr.LiveActor();
        if (!actor) {   // the destroy crossed on its own seam
            // The generation closes with the drive: a held End of an older one then reads stale on replay.
            AdvanceGen(g_endedGen, eid, dr.gen);
            g_drives.erase(eid);
            continue;
        }
        const bool held = Held(actor);
        if (!basic()) continue;
        if (held) {
            // A hand took it before the end edge landed: the holder's drive owns the actor from here,
            // and its release restores the physics. This drive yields for good rather than writing
            // a stale target back once the hand lets go.
            UE_LOGI("[PROP-DRIVE] CLIENT yield eid=%u -- a held-prop stream owns the actor", eid);
            g_drives.erase(eid);
            const auto detached = [&] {return scope() && LifetimeCurrent(dr) && dr.LiveActor() && !g_drives.contains(eid);};
            YieldToHand(eid, dr.gen, actor, &dr, detached);
            if (!scope()) return;
            continue;
        }
        // The physics receiver's re-latch, as the held-prop drive does it: a parked copy stays
        // kinematic when the game here switches its simulation back on. That includes a copy parked
        // frozen, which the park left as it was, and which an unstick or an unfreeze here has since
        // freed: it is parked kinematic from then on, and the end edge hands its physics back.
        void* mesh = PR::GetStaticMesh(actor);
        if (!scope()) return;
        if (!dr.LiveActor()) continue;
        if (mesh != dr.d.mesh || (mesh && !dr.meshRef.Is(mesh))) {
            dr.d.mesh = mesh;
            dr.meshRef.Set(dr.d.mesh);
            dr.meshCls.Set(R::ClassOf(dr.d.mesh));
        }
        if (!current()) continue;
        if (dr.d.mesh && ue_wrap::engine::IsComponentSimulatingPhysics(dr.d.mesh)) {
            if (!current()) {if (!scope()) return;continue;}
            dr.physicsParked = true;
            g_drives.at(eid).physicsParked = true;
            const bool parked = coop::prop_park::Park(actor, dr.d.mesh, current);
            if (!current() || !parked) {if (!scope()) return;continue;}
            if (++dr.relatches == 1) {
                UE_LOGI("[PROP-DRIVE] CLIENT eid=%u -- the game turned simulation back on under the park; "
                        "re-latched kinematic", eid);
            } else if (dr.relatches == kRelatchLoud) {
                UE_LOGW("[PROP-DRIVE] CLIENT eid=%u -- re-latched %u times: something here keeps switching the "
                        "parked copy's simulation back on", eid, static_cast<unsigned>(dr.relatches));
            }
        }
        if (!current()) {if (!scope()) return;continue;}
        const auto render = [&] {
            if (!current()) return false;
            const bool heldNow = Held(actor);
            return current() && !heldNow;
        };
        AD::AdvanceLerp(dr.d, nowMs, render);
        if (!scope()) return;
        if (current()) g_drives.at(eid) = std::move(dr);
    }
    if (scope()) coop::prop_child_release::Tick(nowMs);   // a waiting door's liveness and its failure bound
}

namespace {
constexpr uint16_t kEndKind = static_cast<uint16_t>(coop::net::ReliableKind::PropDriveEnd);

// An end held in bind_wait runs again once its prop is bound: the whole of OnEnd, so a drive that parked
// meanwhile takes the parked branch and a hand that took the prop since is yielded to as on a first arrival.
void ReplayEnd(const uint8_t* bytes, size_t n, int /*senderSlot*/) {
    coop::net::PropDriveEndPayload p{};
    if (n != sizeof p) return;
    std::memcpy(&p, bytes, sizeof p);
    // The closed watermark rejects in-flight poses while this end waits for its
    // actor. Only its owned bind replay may retry that exact generation.
    const auto ended = g_endedGen.find(p.eid);
    if (ended == g_endedGen.end() || ended->second != p.gen) return;
    g_endedGen.erase(ended);
    OnEnd(p);
}
}  // namespace

namespace {
void ReceiveEnd(const coop::net::PropDriveEndPayload& input, Source sourceGuard) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::OnEnd");
    // Own the input too: it can be a field in a caller's callback-mutable row.
    const auto p = input;
    if (!p.eid || !p.gen || p._pad[0] || p._pad[1] || (p.physFlags & 0x80) ||
        !coop::net::PropDriveEndInBounds(p)) return;
    const float values[] = {p.x,p.y,p.z,p.pitch,p.yaw,p.roll,
        p.linVelX,p.linVelY,p.linVelZ,p.angVelX,p.angVelY,p.angVelZ};
    for (float value : values) if (!std::isfinite(value)) return;
    const auto epoch = g_epoch;
    const auto world = ue_wrap::world_identity::Generation();
    const Context context;
    const auto scope = [&] {return context.Current() && CurrentSource(sourceGuard) && context.Current() &&
        epoch == g_epoch && world == ue_wrap::world_identity::Generation();};
    if (!scope()) return;
    const auto ended = g_endedGen.find(p.eid);
    if (ended != g_endedGen.end() && !GenAfter(p.gen, ended->second)) return;
    auto it = g_drives.find(p.eid);
    if (it == g_drives.end()) {
        // The actor may have appeared since an older end was queued. Once this newer
        // end lands immediately, that old retry must never overwrite it next tick.
        coop::element::bind_wait::Drop(p.eid, static_cast<uint16_t>(coop::net::ReliableKind::PropDriveEnd), "newer end");
        if (!scope()) return;
        // Never parked here. The generation still closes, so a pose of it in flight cannot park
        // the prop now; the rest pose is applied if the prop is here, held in bind_wait for its bind if the
        // bind is deferred (a joiner mid-drag whose snapshot is still landing), and
        // dropped for a prop a hand holds -- that stream owns it -- or one a hand took this generation
        // over from: the hand moved it since, so this pose is older than where it lies. The same for a
        // generation this peer closed itself (CloseGen). An end older than the generation already closed
        // here is stale outright and touches nothing.
        if (auto closed = g_endedGen.find(p.eid); closed != g_endedGen.end() && GenAfter(closed->second, p.gen))
            return;
        if (YieldedAtOrAfter(p.eid, p.gen)) {
            AdvanceGen(g_endedGen, p.eid, p.gen);
            UE_LOGI("[PROP-DRIVE] CLIENT END eid=%u gen=%u -- this generation was yielded or closed here (a "
                    "hand took it over, or this peer ended it); pose not applied", p.eid,
                    static_cast<unsigned>(p.gen));
            return;
        }
        AdvanceGen(g_endedGen, p.eid, p.gen);
        void* actor = coop::remote_prop::ResolveLiveActorByEid(p.eid);
        if (!scope()) return;
        if (actor) {
            if (!TargetMatches(sourceGuard, actor)) return;
            ue_wrap::CachedObjRef ref, cls; ref.Set(actor); cls.Set(R::ClassOf(actor));
            const auto current = [&] { return scope() && TargetMatches(sourceGuard, actor) && ref.Get() == actor &&
                cls.Get() && R::ClassOf(actor) == cls.Get() && !g_drives.contains(p.eid); };
            if (!current()) return;
            const bool prop = PR::IsDescendantOfProp(actor);
            if (!current() || !prop) return;
            const bool held = Held(actor);
            if (!current()) return;
            if (!held) {
                ApplyEnd(actor, p, /*parkedHere=*/false, sourceGuard);
                UE_LOGI("[PROP-DRIVE] CLIENT END eid=%u gen=%u -> (%.1f,%.1f,%.1f) (never parked here)",
                        p.eid, static_cast<unsigned>(p.gen), p.x, p.y, p.z);
            }
        } else if (sourceGuard.current) {
            return; // a host retirement receipt cannot become a deferred client replay
        } else if (!coop::element::bind_wait::Hold(p.eid, kEndKind, /*senderSlot=*/0, &ReplayEnd, &p, sizeof p,
                                                   /*coalesce=*/true)) {
            // Not deferred: no bind is coming for this eid, so there is nothing to wait for.
            coop::element::bind_wait::NoteNeverBound(kEndKind, p.eid);
        }
        return;
    }
    Drive dr = it->second;
    if (!drive_generation::AtLeast(p.gen, dr.gen)) return;
    // Detach before any physics/transform callback. Never erase through an
    // iterator that a native call may have invalidated or repurposed.
    g_drives.erase(it);
    AdvanceGen(g_endedGen, p.eid, p.gen);
    if (void* actor = dr.LiveActor()) {
        const auto current = [&] { return scope() && TargetMatches(sourceGuard, actor) && dr.LiveActor() == actor && !g_drives.contains(p.eid); };
        if (!current()) return;
        const bool held = Held(actor);
        if (!current()) return;
        if (held) {
            YieldToHand(p.eid, p.gen, actor, &dr, current);
            // A hand's stream parked this prop after ours did, and its release will hand the
            // physics back; touching the pose or the physics here would undo that park.
            UE_LOGI("[PROP-DRIVE] CLIENT END eid=%u gen=%u -- a held-prop stream owns the actor; "
                    "row dropped, nothing touched", p.eid, static_cast<unsigned>(p.gen));
        } else {
            ApplyEnd(actor, p, dr.physicsParked, sourceGuard);
            dr.physicsParked = false;
            UE_LOGI("[PROP-DRIVE] CLIENT END eid=%u gen=%u -> (%.1f,%.1f,%.1f) flags=0x%02x",
                    p.eid, static_cast<unsigned>(p.gen), p.x, p.y, p.z,
                    static_cast<unsigned>(p.physFlags));
        }
    }
}
} // namespace

void OnEnd(const coop::net::PropDriveEndPayload& input) { ReceiveEnd(input, {}); }

void CloseGen(uint32_t eid, uint8_t gen) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::CloseGen");
    AdvanceGen(g_endedGen, eid, gen);
    AdvanceGen(g_yieldedGen, eid, gen);
}

bool IsParked(void* actor) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::IsParked");
    if (!actor) return false;
    // The live actor, not the cached pointer: a row whose actor died is erased only on the next tick,
    // and a new actor at the freed address is not parked.
    for (const auto& kv : g_drives)
        if (kv.second.LiveActor() == actor) return true;
    return false;
}

bool TryGetPresentationVelocity(void* actor, ue_wrap::FVector& velocity) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::TryGetPresentationVelocity");
    if (!actor) return false;
    for (const auto& [eid, drive] : g_drives) {
        if (drive.LiveActor() != actor) continue;
        velocity = AD::PresentationVelocity(drive.d, AD::NowMs());
        return true;
    }
    return false;
}

bool IsCorrectionStale(uint32_t eid, uint8_t gen, bool open) {
    UE_ASSERT_GAME_THREAD("prop_drive_stream::IsCorrectionStale");
    if (const auto d = g_drives.find(eid); d != g_drives.end()) return GenAfter(d->second.gen, gen);
    const auto e = g_endOnActor.find(eid);
    if (e == g_endOnActor.end()) return false;
    // The END pose died with its actor (a load-tail re-create, a purge) unless the bound actor is
    // still the one it was written onto: the same pointer, its slot still carrying the serial taken then.
    coop::element::Element* el = coop::element::Registry::Get().Get(eid);
    void* actor = el ? el->GetActor() : nullptr;
    if (!actor || !R::IsLiveByIndex(actor, el->GetInternalIdx()) || !e->second.actor.Is(actor))
        return false;
    return GenAfter(e->second.gen, gen) || (e->second.gen == gen && open);
}

void OnDisconnect() {
    ++g_epoch;
    g_session = nullptr; g_serial = g_world = 0; g_stops = 0;
    auto retired = std::move(g_drives);
    g_drives.clear();
    g_endedGen.clear();
    g_endOnActor.clear();
    g_yieldedGen.clear();
    g_miss.clear();
    g_devView.clear();
    {
        std::lock_guard<std::mutex> lk(g_mergedMu);
        g_merged.clear();
    }
    g_endCount = 0;
    g_batch.clear();
    // No iterator/reference into live state crosses native release callbacks.
    // A nested disconnect or successor drive owns its own state from this point.
    const auto epoch = g_epoch;
    for (auto& kv : retired) {
        if (epoch != g_epoch) return;
        void* actor = kv.second.LiveActor();
        if (actor && !g_drives.contains(kv.first) && !Held(actor)) GiveBackPhysics(kv.second, actor);
    }
    if (epoch == g_epoch && !g_session) coop::prop_child_release::OnDisconnect();
}

namespace dev {

DriveView ViewOf(uint32_t eid) {
    const auto it = g_devView.find(eid);
    return it == g_devView.end() ? DriveView{} : it->second;
}

void NoteMerged(uint32_t eid) {
    if (!coop::dev::prop_syncer_drill::IsEnabled()) return;
    std::lock_guard<std::mutex> lk(g_mergedMu);
    ++g_merged[eid];
}

uint32_t MergedOf(uint32_t eid) {
    std::lock_guard<std::mutex> lk(g_mergedMu);
    const auto it = g_merged.find(eid);
    return it == g_merged.end() ? 0u : it->second;
}

}  // namespace dev

}  // namespace coop::prop_drive_stream
