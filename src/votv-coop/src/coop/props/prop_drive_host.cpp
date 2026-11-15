// coop/props/prop_drive_host.cpp -- see coop/props/prop_drive_host.h.

#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_knock.h"

#include "coop/creatures/npc_sync.h"  // KerfurIsHolding: a take is not a throw
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/throw_bounds.h"
#include "coop/player/hand_item.h"            // IsHandAxisActor: the hotbar hand and its mirrors
#include "coop/player/local_streams.h"         // LastHeldActor
#include "coop/player/local_prop_hold.h"      // live pawn state, before this frame's stream tick
#include "coop/props/active_drive.h"          // NowMs
#include "coop/props/drive_generation.h"
#include "coop/props/prop_drive_rules.h"      // the epsilons and the rest rule both authors share
#include "coop/props/prop_drive_stream.h"     // Pump: the apply tick that follows the publish
#include "coop/props/prop_element_tracker.h"  // GetPropElementIdForActor
#include "coop/props/prop_lifecycle.h"        // IsWireSuppressedPropClass: a class no peer holds
#include "coop/props/prop_syncer.h"           // Pump: the role and a client's rows; OnDisconnect
#include "coop/props/prop_syncer_host.h"      // Pump: admit a client's rows; OnDisconnect
#include "coop/props/prop_wire_parity.h"      // PhysFlagsOf: the flags the end edge carries
#include "coop/props/remote_prop.h"           // IsActorUnderAnyDrive: a peer's held-prop stream
#include "prop_drive_rows.h"                  // co-located private header (src tree, not include/)
#include "prop_end_delivery.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::prop_drive_host {
namespace {

namespace E  = ue_wrap::engine;
namespace PR = ue_wrap::prop;
namespace R  = ue_wrap::reflection;

using coop::prop_drive_rules::kRestMs;
using coop::prop_drive_rules::kRestProbeMs;
using coop::prop_drive_rules::kRestVelCmS;

std::vector<Driven> g_driven;   // game thread only
uint64_t g_rowRevision = 0;
coop::net::Session* g_session = nullptr;
uint64_t g_epoch = 0, g_stops = 0;
uint32_t g_world = 0, g_serial = 0, g_published = 0;
bool g_host = false, g_ticking = false, g_pumping = false;
size_t              g_turn = 0; // where the next tick's publishing starts, so no prop waits forever
// The claim generation is PER PROP: the receiver's closed-generation gate is keyed by eid and
// compares in a signed 8-bit window, so a counter shared by every prop would read a re-claim as
// older than the closed one after enough claims of other props in between. Outlives the row.
std::unordered_map<uint32_t, uint8_t> g_genByEid;

struct Context {
    coop::net::Session* session = g_session;
    uint64_t epoch = g_epoch, stops = g_stops;
    uint32_t world = g_world, serial = g_serial;
    bool host = g_host;
    bool Current() const {
        return session && session == g_session && epoch == g_epoch && host == g_host &&
            world == ue_wrap::world_identity::Generation() && serial == coop::net::session_serial::Current() &&
            stops == session->StopCount() && session->connected() &&
            session->role() == (host ? coop::net::Role::Host : coop::net::Role::Client);
    }
};
struct Busy {
    bool& flag;
    explicit Busy(bool& value) : flag(value) { flag = true; }
    ~Busy() { flag = false; }
};
uint64_t Elapsed(uint64_t now, uint64_t before) { return now >= before ? now - before : 0; }
bool BindSession(coop::net::Session& s, bool host) {
    const auto world = ue_wrap::world_identity::Generation();
    const auto serial = coop::net::session_serial::Current();
    const auto stops = s.StopCount();
    const bool valid = s.connected() && s.role() == (host ? coop::net::Role::Host : coop::net::Role::Client);
    if (g_session && (g_session != &s || g_world != world || g_serial != serial || g_stops != stops ||
                      g_host != host || !valid)) {
        OnDisconnect();
        if (g_session || world != ue_wrap::world_identity::Generation() ||
            serial != coop::net::session_serial::Current() || stops != s.StopCount()) return false;
    }
    if (!valid) return false;
    if (!g_session) ++g_epoch;
    g_session = &s; g_world = world; g_serial = serial; g_stops = stops; g_host = host;
    return Context{}.Current();
}
struct Receipt {
    Context context;
    Driven value;
    bool Matches() const {
        if (!context.Current()) return false;
        const Driven* row = FindByEid(value.eid);
        return row && row->revision == value.revision && row->gen == value.gen &&
            row->syncer == value.syncer && row->claimSeq == value.claimSeq;
    }
    bool SourceCurrent() const {
        const auto slot = value.syncer;
        return !slot || (slot < coop::net::kMaxPeers && value.ownerGeneration &&
            value.ownerGeneration == context.session->peerGenerationForSlot(slot) &&
            context.session->IsSlotConnected(slot) && context.session->IsSlotWorldReady(slot));
    }
    bool ActorCurrent() const {
        return value.ref.Get() && value.cls.Get() && R::ClassOf(value.ref.Get()) == value.cls.Get() &&
            static_cast<uint32_t>(coop::prop_element_tracker::GetPropElementIdForActor(value.ref.Get())) == value.eid;
    }
    bool Current() const { return Matches() && SourceCurrent() && ActorCurrent(); }
};
bool Save(const Receipt& receipt, Driven row) {
    if (!receipt.Current()) return false;
    row.revision = NewRowRevision();
    *FindByEid(row.eid) = row;
    return true;
}
bool Retire(const Receipt& receipt) {
    if (!receipt.Matches()) return false;
    EraseRow(*FindByEid(receipt.value.eid));
    return true;
}
struct RootBinding {
    Receipt receipt;
    ue_wrap::CachedObjRef ref, cls;
    explicit RootBinding(const Receipt& value) : receipt(value) {}
    bool Basic() const {
        return receipt.Current() && ref.Get() && cls.Get() && R::ClassOf(ref.Get()) == cls.Get();
    }
    bool Capture() {
        if (!receipt.Current()) return false;
        void* root = E::RootComponentOf(receipt.value.ref.Get());
        if (!receipt.Current() || !root || !R::IsLive(root)) return false;
        ref.Set(root); cls.Set(R::ClassOf(root));
        return Basic();
    }
    bool Current() const {
        if (!Basic()) return false;
        void* root = E::RootComponentOf(receipt.value.ref.Get());
        return Basic() && root == ref.Get();
    }
};
bool Finite(const ue_wrap::FVector& value) {
    return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z);
}
bool Finite(const ue_wrap::FRotator& value) {
    return std::isfinite(value.Pitch) && std::isfinite(value.Yaw) && std::isfinite(value.Roll);
}
bool PositionInBounds(const ue_wrap::FVector& value) {
    return Finite(value) && std::fabs(value.X) <= coop::net::kMaxCoord &&
        std::fabs(value.Y) <= coop::net::kMaxCoord && std::fabs(value.Z) <= coop::net::kMaxCoord;
}
bool FiniteEnd(const coop::net::PropDriveEndPayload& p) {
    for (float value : {p.x,p.y,p.z,p.pitch,p.yaw,p.roll,p.linVelX,p.linVelY,p.linVelZ,p.angVelX,p.angVelY,p.angVelZ})
        if (!std::isfinite(value)) return false;
    return coop::net::PropDriveEndInBounds(p);
}

Driven* Find(void* actor) {
    for (Driven& d : g_driven)
        if (!d.dead && d.ref.Get() == actor) return &d;
    return nullptr;
}

// Admission can run before the first connected pump. Once bound, the session is
// part of the proof; an unbound admission still owns its world/serial/epoch.
struct VerbProof {
    coop::net::Session* session = g_session;
    uint64_t epoch = g_epoch, stops = session ? session->StopCount() : 0;
    uint32_t world = ue_wrap::world_identity::Generation(), serial = coop::net::session_serial::Current();
    void* worldObject = ue_wrap::world_identity::CurrentWorld();
    ue_wrap::CachedObjRef actor, cls;
    std::function<bool()> caller;
    explicit VerbProof(void* value, const std::function<bool()>& current = {}) : caller(current) { actor.Set(value); }
    explicit VerbProof(const Driven& value) : actor(value.ref), cls(value.cls) {}
    bool Scope() const {
        return epoch == g_epoch && session == g_session && world == ue_wrap::world_identity::Generation() &&
            serial == coop::net::session_serial::Current() && worldObject == ue_wrap::world_identity::CurrentWorld() &&
            (!session || (g_host && g_world == world && g_serial == serial && g_stops == stops &&
                           session->StopCount() == stops && session->connected() &&
                           session->role() == coop::net::Role::Host));
    }
    bool Basic() const {
        return Scope() && actor.Get() && cls.Get() && R::ClassOf(actor.Raw()) == cls.Raw();
    }
    bool Current(bool requireCaller = true) const { return Basic() && (!requireCaller || !caller || caller()) && Basic(); }
    bool Capture() {
        if (!Scope() || !actor.Get()) return false;
        void* type = R::ClassOf(actor.Raw());
        if (!Scope() || !actor.Get()) return false;
        cls.Set(type);
        return Current();
    }
    bool Matches(const Driven& value, bool requireCaller = true) const {
        if (!Current(requireCaller)) return false;
        const auto eid = coop::prop_element_tracker::GetPropElementIdForActor(actor.Raw());
        if (!Current(requireCaller) || eid != value.eid) return false;
        const auto* row = FindByEid(value.eid);
        return row && row->revision == value.revision && row->episode == value.episode &&
            row->gen == value.gen && row->syncer == value.syncer && row->ref.Is(actor.Raw()) && row->cls.Is(cls.Raw());
    }
    bool Unchanged(const std::optional<Driven>& value) const {
        return value ? Matches(*value) : Current() && !Find(actor.Raw());
    }
    std::optional<Driven> Handoff(const Driven& before) const {
        if (!Current()) return {};
        const auto eid = coop::prop_element_tracker::GetPropElementIdForActor(actor.Raw());
        if (!Current() || eid != before.eid) return {};
        const auto* row = FindByEid(before.eid);
        if (!row || row->syncer || row->handoffFrom != before.revision ||
            row->ref.Raw() != actor.Raw() || !row->cls.Is(cls.Raw()) || !row->ref.Is(actor.Raw())) return {};
        return *row;
    }
};

bool ClearHands(void* actor, const std::function<bool()>& current) {
    if (!current()) return false;
    const bool local = coop::local_prop_hold::IsHeld(actor);
    if (!current() || local) return false;
    void* last = coop::local_streams::LastHeldActor();
    if (!current() || actor == last) return false;
    const bool remote = coop::remote_prop::IsActorUnderAnyDrive(actor);
    if (!current() || remote) return false;
    const bool hotbar = coop::hand_item::IsHandAxisActor(actor);
    return current() && !hotbar;
}

// Keep this stack guard through teardown: a recursive opener at a recycled
// address must wait for the old stack to unwind. It never dereferences an entry.
std::vector<void*> g_opening;
struct Opening {
    void* actor;
    bool entered;
    explicit Opening(void* value) : actor(value), entered(std::find(g_opening.begin(), g_opening.end(), value) == g_opening.end()) {
        if (entered) g_opening.push_back(actor);
    }
    ~Opening() { if (entered) g_opening.erase(std::find(g_opening.begin(), g_opening.end(), actor)); }
};

uint64_t g_claimToken = 0; // outlives disconnects and old callback stacks
bool ValidSource(ClaimSource source) { return static_cast<size_t>(source) < kClaimSourceCount; }
uint32_t SourceBit(ClaimSource source) { return ValidSource(source) ? (1u << static_cast<unsigned>(source)) : 0; }
uint64_t NextClaimToken() { if (!++g_claimToken) ++g_claimToken; return g_claimToken; }
void ClearClaims(Driven& row) { row.claimMask = 0; row.claimTokens.fill(0); }
bool ClaimsCurrent(const Driven& row) {
    return row.claimEpoch == g_epoch && row.claimWorld == ue_wrap::world_identity::Generation() &&
        row.claimSerial == coop::net::session_serial::Current() && row.claimWorldObject == ue_wrap::world_identity::CurrentWorld();
}
void StampClaims(Driven& row) {
    row.claimEpoch = g_epoch; row.claimWorld = ue_wrap::world_identity::Generation();
    row.claimSerial = coop::net::session_serial::Current(); row.claimWorldObject = ue_wrap::world_identity::CurrentWorld();
}
void ExpireClaims(Driven& row) {
    row.claimMask &= SourceBit(ClaimSource::Legacy);
    for (size_t i = 1; i < row.claimTokens.size(); ++i) row.claimTokens[i] = 0;
    row.claimed = row.claimMask != 0; StampClaims(row);
}
ClaimLease LeaseFor(const Driven& row, ClaimSource source) {
    if (!ValidSource(source) || !ClaimsCurrent(row) || row.syncer || !row.claimed || !(row.claimMask & SourceBit(source))) return {};
    return {row.eid, row.claimTokens[static_cast<size_t>(source)], g_epoch,
        ue_wrap::world_identity::Generation(), coop::net::session_serial::Current(),
        ue_wrap::world_identity::CurrentWorld(), source};
}
bool LeaseScope(const ClaimLease& lease) {
    return lease && ValidSource(lease.source) && lease.epoch == g_epoch &&
        lease.world == ue_wrap::world_identity::Generation() && lease.serial == coop::net::session_serial::Current() &&
        lease.worldObject == ue_wrap::world_identity::CurrentWorld();
}

// A prop in a hand: this player's grab slot, a peer's held-prop stream, or the hotbar hand axis
// (the local hand actor and every peer's display mirror), which the hand lane owns outright.
bool HeldBySomeone(void* actor) {
    return coop::local_prop_hold::IsHeld(actor) ||
           coop::remote_prop::IsActorUnderAnyDrive(actor) ||
           coop::hand_item::IsHandAxisActor(actor);
}

bool Moved(const Driven& d, const ue_wrap::FVector& loc, const ue_wrap::FRotator& rot) {
    if (!d.everSent || d.resend) return true;
    return coop::prop_drive_rules::MovedPastEps(loc, rot, d.sentLoc, d.sentRot);
}

bool Unheld(const Receipt& receipt) {
    if (!receipt.Current()) return false;
    const bool held = InAHand(receipt.value.ref.Get());
    return receipt.Current() && !held;
}
bool SendEnd(coop::net::Session& s, const Receipt& receipt, const ue_wrap::FVector& loc,
             const ue_wrap::FRotator& rot, const char* why, bool requireUnheld) {
    if (!receipt.Current() || !PositionInBounds(loc) || !Finite(rot)) return false;
    const Driven& d = receipt.value;
    RootBinding root(receipt);
    if (!root.Capture()) return false;
    const auto current = [&] { return root.Current(); };
    float lin = 0.f;
    const auto p = EndFromBody(d.ref.Get(), d.eid, d.gen, loc, rot, &lin, current);
    if (!root.Current() || p.eid != d.eid || p.gen != d.gen || !FiniteEnd(p)) return false;
    if (requireUnheld && !Unheld(receipt)) return false;
    if (!root.Current()) return false;
    coop::prop_end_delivery::Receipt delivery;
    const bool sent = coop::prop_end_delivery::Queue(s, p, d.ref, d.cls, -1, current, &delivery);
    if (!receipt.Current() || !root.Current() || !sent) { coop::prop_end_delivery::Cancel(delivery); return false; }
    UE_LOGI("[PROP-DRIVE] HOST END eid=%u gen=%u %s -> (%.1f,%.1f,%.1f) |v|=%.1f cm/s flags=0x%02x",
            d.eid, static_cast<unsigned>(d.gen), why, loc.X, loc.Y, loc.Z, lin,
            static_cast<unsigned>(p.physFlags));
    return true;
}

// A client row's turn: the last pose admitted from its syncer goes out at the row's generation when it is new
// (or a joiner needs it again), as a host-authored row's would; the host's copy is never read. 1 when the pose
// joined the queue (the tick's rotation counts those), else 0.
size_t PublishAdmitted(coop::net::Session& s, const Receipt& receipt) {
    if (!receipt.Current()) return 0;
    Driven d = receipt.value;
    if (!d.admittedEver || !(d.freshAdmit || d.resend)) return 0;
    // The developer red arm of the prop syncer drill: an admitted row is applied to the host's copy (at admission)
    // and never re-published, so the drill's observer must fail. The End still goes. Resolved once.
    static const bool s_redNoRelay =
        coop::config::ResolveEnum(coop::config_registry::rows::prop_syncer_red) == "no_relay";
    if (s_redNoRelay) {
        d.freshAdmit = false;
        d.resend     = false;
        Save(receipt, d);
        return 0;
    }
    const coop::net::PoseTurn turn = s.PropDrivePoseTurn(d.eid);
    if (!receipt.Current() || turn == coop::net::PoseTurn::Wait) return 0;
    if (!PositionInBounds(d.admitLoc) || !Finite(d.admitRot) || !Finite(d.admitVel)) return 0;
    coop::net::PropDriveRow pp{};
    pp.key       = d.key;
    pp.elementId = d.eid;
    pp.ctx       = d.gen;
    pp.x = d.admitLoc.X; pp.y = d.admitLoc.Y; pp.z = d.admitLoc.Z;
    pp.pitch = ue_wrap::NormalizeAxis(d.admitRot.Pitch);
    pp.yaw   = ue_wrap::NormalizeAxis(d.admitRot.Yaw);
    pp.roll  = ue_wrap::NormalizeAxis(d.admitRot.Roll);
    pp.vel[0] = d.admitVel.X; pp.vel[1] = d.admitVel.Y; pp.vel[2] = d.admitVel.Z;
    pp.sendTimeMs = d.admitSendMs;
    // A broadcast, the syncer included: MTA relays the accepted sync to every joined player but its source
    // (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:472). The pose batch is one shared datagram per tick, so the source is not left out
    // here; its client skips the echo (prop_syncer::SkipsHostPose). The End is sent to the others alone.
    if (!Unheld(receipt)) return 0;
    s.PublishPropDrivePose(pp);
    if (!receipt.Current()) return 0;
    d.freshAdmit = false;
    d.resend     = false;
    if (!Save(receipt, d)) return 0;
    return turn == coop::net::PoseTurn::Join ? 1 : 0;
}

// A new row for `actor` when the channel can carry it: a keyed prop with a host element id, in no
// hand, of a class peers hold. Null otherwise. `claimed` false opens it coasting. `syncer` 0 is a host
// verb's row, n a client slot's grant.
Driven* Open(void* actor, bool claimed, const char* why, uint8_t syncer = 0,
             const std::function<bool()>& current = {}, ClaimSource source = ClaimSource::Legacy) {
    if (!ValidSource(source) || !actor || syncer >= coop::net::kMaxPeers) return nullptr;
    Opening opening(actor);
    if (!opening.entered) return nullptr;
    const std::string reason = why ? why : "";
    VerbProof proof(actor, current);
    if (!proof.Capture()) return nullptr;
    uint32_t candidate = 0;
    const auto vacant = [&] {
        return proof.Current() && !Find(actor) && (!candidate || !FindByEid(candidate));
    };
    if (!vacant()) return nullptr;
    const bool prop = PR::IsDescendantOfProp(actor);
    if (!vacant() || !prop || !ClearHands(actor, vacant)) return nullptr;
    const auto eid = coop::prop_element_tracker::GetPropElementIdForActor(actor);
    if (!vacant() || eid == 0 || eid >= coop::element::kHostRangeSize) return nullptr;
    candidate = static_cast<uint32_t>(eid);
    if (!vacant()) return nullptr;
    const auto className = R::ClassNameOf(actor);
    if (!vacant()) return nullptr;
    const bool suppressed = coop::prop_lifecycle::IsWireSuppressedPropClass(className);
    if (!vacant() || suppressed) return nullptr;
    const std::wstring key = PR::GetInteractableKeyString(actor);
    if (!vacant() || key.empty() || key == L"None" || key.size() > sizeof(coop::net::WireKey{}.data)) return nullptr;
    coop::net::WireKey wire{};
    for (wchar_t ch : key) {
        if (ch <= 0 || ch > 255) return nullptr;
        wire.data[wire.len++] = static_cast<char>(ch);
    }
    const auto finalEid = coop::prop_element_tracker::GetPropElementIdForActor(actor);
    if (!vacant() || finalEid != candidate) return nullptr;
    const auto now = coop::active_drive::NowMs();
    if (!vacant()) return nullptr;
    Driven d;
    d.ref = proof.actor; d.cls = proof.cls;
    d.eid = candidate; d.key = wire; d.claimed = claimed; d.syncer = syncer;
    ClearClaims(d);
    StampClaims(d);
    if (claimed && !syncer) { d.claimMask = SourceBit(source); d.claimTokens[static_cast<size_t>(source)] = NextClaimToken(); }
    d.revision = NewRowRevision(); d.episode = d.revision;
    d.gen = BumpGen(d.eid);
    d.openMs = d.lastMoveMs = now;
    g_driven.push_back(d);
    coop::prop_syncer_host::OnRowOpened(d.eid);
    // Insertion consumes an absent-target precondition. Its own row receipt now
    // owns the continuation; the grant caller separately rechecks its sender.
    if (!proof.Matches(d, false)) return nullptr;
    UE_LOGI("[PROP-DRIVE] HOST %s eid=%u gen=%u key='%ls' (%s) -- %zu driven",
            syncer ? "GRANT" : (claimed ? "CLAIM" : "COAST"), d.eid,
            static_cast<unsigned>(d.gen), key.c_str(), reason.c_str(), g_driven.size());
    return proof.Matches(d, false) ? FindByEid(d.eid) : nullptr;
}

}  // namespace

Driven* FindByEid(uint32_t eid) {
    for (Driven& d : g_driven)
        if (!d.dead && d.eid == eid) return &d;
    return nullptr;
}

uint64_t NewRowRevision() { return ++g_rowRevision; }

uint8_t BumpGen(uint32_t eid) {
    uint8_t& gen = g_genByEid[eid];
    if (++gen == 0) ++gen;   // 0 is the stream's "no generation"
    return gen;
}

coop::net::PropDriveEndPayload EndFromBody(void* actor, uint32_t eid, uint8_t gen, const ue_wrap::FVector& loc,
                                           const ue_wrap::FRotator& rot, float* speedCmS,
                                           const std::function<bool()>& current) {
    coop::net::PropDriveEndPayload p{};
    if (speedCmS) *speedCmS = 0.f;
    if (current && !current()) return p;
    p.eid = eid;
    p.gen = gen;
    // This peer's own flags at this instant: the receiver restores the same parity the join's
    // converge applies, rather than guessing that a simulating prop is what it took off.
    p.physFlags = coop::prop_wire_parity::PhysFlagsOf(actor);
    if (current && !current()) return {};
    p.x = loc.X; p.y = loc.Y; p.z = loc.Z;
    p.pitch = ue_wrap::NormalizeAxis(rot.Pitch);
    p.yaw   = ue_wrap::NormalizeAxis(rot.Yaw);
    p.roll  = ue_wrap::NormalizeAxis(rot.Roll);
    // The body's velocity at this instant. At rest it is noise below kRestVelCmS and goes out as
    // zero, so no receiver wakes a resting body by writing a velocity into it. MTA's syncer
    // (CUnoccupiedVehicleSync.cpp, bSyncVelocity) streams a velocity in every packet it moved in; a drive's rows carry
    // the copy's velocity too (PropDriveRow), and this End carries the body's angular velocity as well, which no row does.
    const PR::VelocityState v = PR::GetPhysicsVelocity(actor);
    if (current && !current()) return {};
    const float lin = std::sqrt(v.linearCmS.X * v.linearCmS.X + v.linearCmS.Y * v.linearCmS.Y +
                                v.linearCmS.Z * v.linearCmS.Z);
    if (v.ok && lin >= kRestVelCmS) {
        p.linVelX = v.linearCmS.X;   p.linVelY = v.linearCmS.Y;   p.linVelZ = v.linearCmS.Z;
        p.angVelX = v.angularDegS.X; p.angVelY = v.angularDegS.Y; p.angVelZ = v.angularDegS.Z;
    }
    if (speedCmS) *speedCmS = v.ok ? lin : 0.f;
    return p;
}

Driven* OpenClientRow(void* actor, uint8_t slot, const char* why, const std::function<bool()>& current) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::OpenClientRow");
    if (!actor || slot == 0 || slot >= coop::net::kMaxPeers) return nullptr;
    Driven* d = Open(actor, /*claimed=*/true, why ? why : "", slot, current);
    if (d) d->admitMs = d->lastMoveMs;   // the silence clock starts at the grant
    return d;
}

Driven* ElectClientRow(void* actor, uint8_t slot, const char* why, const std::function<bool()>& current) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::ElectClientRow");
    if (!actor || !slot || slot >= coop::net::kMaxPeers) return nullptr;
    const std::string reason = why ? why : "";
    VerbProof proof(actor, current);
    if (!proof.Capture()) return nullptr;
    const auto* found = Find(actor);
    if (!found) return OpenClientRow(actor, slot, reason.c_str(), current);
    Driven before = *found;
    const auto unchanged = [&] { return proof.Matches(before); };
    if (!ClearHands(actor, unchanged)) return nullptr;
    if (!before.syncer && before.claimed) return nullptr;
    if (before.syncer && before.syncer != slot) {
        coop::prop_syncer_host::EndClientRow(*FindByEid(before.eid), coop::prop_syncer_host::Ending::HostVerb);
        auto host = proof.Handoff(before);
        if (!host) return nullptr;
        before = *host;
    }
    const auto now = coop::active_drive::NowMs();
    if (!proof.Matches(before)) return nullptr;
    Driven d = before;
    d.syncer = slot; d.handoffFrom = 0; ClearClaims(d);
    d.revision = NewRowRevision(); d.episode = d.revision;
    d.claimed = true; d.gen = BumpGen(d.eid);
    d.everSent = d.resend = d.unreadSaid = false;
    d.openMs = d.lastMoveMs = now; d.nextProbeMs = 0;
    d.admittedEver = d.freshAdmit = false;
    d.admitMs = now; d.admitSendMs = 0; d.admitVel = {}; d.claimSeq = 0;
    d.streamBucket.Reset();
    *FindByEid(d.eid) = d;
    coop::prop_syncer_host::OnRowOpened(d.eid);
    if (!proof.Matches(d)) return nullptr;
    UE_LOGI("[PROP-DRIVE] HOST GRANT eid=%u gen=%u (%s) -- the host row becomes slot %u's",
            d.eid, static_cast<unsigned>(d.gen), reason.c_str(), static_cast<unsigned>(slot));
    return proof.Matches(d) ? FindByEid(d.eid) : nullptr;
}

void EraseRow(Driven& row) {
    row.revision = NewRowRevision();
    row.dead = true;   // Tick's sweep erases it; every reader skips a dead row
}

std::vector<Driven>& Rows() {
    return g_driven;
}

bool InAHand(void* actor) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::InAHand");
    return actor && (coop::local_prop_hold::IsHeld(actor) || actor == coop::local_streams::LastHeldActor() ||
           coop::remote_prop::IsActorUnderAnyDrive(actor) ||
           coop::hand_item::IsHandAxisActor(actor));
}

ClaimLease ClaimOwned(void* actor, ClaimSource source, const char* reason, const std::function<bool()>& current) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::ClaimOwned");
    if (!actor || !ValidSource(source)) return {};
    const std::string why = reason ? reason : "";
    VerbProof proof(actor, current);
    if (!proof.Capture()) return {};
    const auto* found = Find(actor);
    if (!found) {
        auto* opened = Open(actor, true, why.c_str(), 0, current, source);
        const auto lease = opened ? LeaseFor(*opened, source) : ClaimLease{};
        if (current && !current()) { (void)ReleaseOwned(lease); return {}; }
        return OwnsClaim(lease) ? lease : ClaimLease{};
    }
    Driven before = *found;
    if (!ClearHands(actor, [&] { return proof.Matches(before); })) return {};
    if (before.syncer) {
        coop::prop_syncer_host::EndClientRow(*FindByEid(before.eid), coop::prop_syncer_host::Ending::HostVerb);
        auto host = proof.Handoff(before);
        if (!host) return {};
        before = *host;
    }
    if (!proof.Matches(before)) return {};
    const auto index = static_cast<size_t>(source);
    const auto bit = SourceBit(source);
    if (ClaimsCurrent(before) && before.claimed && !before.handoffFrom && (before.claimMask & bit) && before.claimTokens[index])
        return LeaseFor(before, source);
    Driven next = before;
    if (!ClaimsCurrent(next)) ExpireClaims(next);
    if (!before.claimed || before.handoffFrom) ClearClaims(next);
    StampClaims(next);
    next.claimMask |= bit;
    next.claimTokens[index] = NextClaimToken();
    next.revision = NewRowRevision(); next.claimed = true; next.handoffFrom = 0; next.nextProbeMs = 0;
    *FindByEid(next.eid) = next;
    UE_LOGI("[PROP-DRIVE] HOST re-claim eid=%u (%s)", next.eid, why.c_str());
    return LeaseFor(next, source);
}
void Claim(void* actor, const char* reason) { (void)ClaimOwned(actor, ClaimSource::Legacy, reason); }

void Coast(void* actor, const char* reason, bool afterHandRelease) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::Coast");
    if (!actor) return;
    const std::string why = reason ? reason : "";
    VerbProof proof(actor);
    if (!proof.Capture()) return;
    const auto* found = Find(actor);
    std::optional<Driven> before = found ? std::optional<Driven>(*found) : std::nullopt;
    const bool robot = coop::npc_sync::KerfurIsHolding(actor);
    if (!proof.Unchanged(before) || robot) return;
    if (!before) { Open(actor, false, why.c_str()); return; }
    if (before->syncer) {
        coop::prop_syncer_host::EndClientRow(*FindByEid(before->eid), coop::prop_syncer_host::Ending::HostVerb);
        auto host = proof.Handoff(*before);
        if (!host) return;
        before = *host;
        before->claimed = false; ClearClaims(*before);
    }
    const auto now = coop::active_drive::NowMs();
    if (!proof.Matches(*before)) return;
    Driven next = *before;
    next.revision = NewRowRevision(); next.handoffFrom = 0;
    if (afterHandRelease) {
        next.episode = NewRowRevision(); next.gen = BumpGen(next.eid); next.resend = true;
        ClearClaims(next); next.claimed = false;
    }
    next.nextProbeMs = 0; next.lastMoveMs = now;
    *FindByEid(next.eid) = next;
    if (afterHandRelease) coop::prop_syncer_host::OnRowOpened(next.eid);
}

void OverrideClientSyncer(void* actor, const char* reason) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::OverrideClientSyncer");
    const Driven* d = actor ? Find(actor) : nullptr;
    if (d && d->syncer != 0) Coast(actor, reason);
}

uint8_t ClientSyncerOf(uint32_t eid) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::ClientSyncerOf");
    const Driven* d = FindByEid(eid);
    return d ? d->syncer : 0;
}

bool ClientRowElected(uint32_t eid) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::ClientRowElected");
    const Driven* d = FindByEid(eid);
    return d && d->syncer != 0 && d->elected;
}

size_t CoastingCount() {
    size_t n = 0;
    for (const Driven& d : g_driven)
        if (!d.claimed && !d.dead) ++n;
    return n;
}

DriveContext ContextOf(void* actor, uint32_t eid) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::ContextOf");
    DriveContext c;
    const auto it = g_genByEid.find(eid);   // find, never operator[]: a read inserts nothing
    if (it != g_genByEid.end()) c.gen = it->second;
    c.open = actor && Find(actor) != nullptr;
    return c;
}

bool OwnsClaim(const ClaimLease& lease) {
    if (!LeaseScope(lease)) return false;
    const auto* row = FindByEid(lease.eid);
    if (!row || !ClaimsCurrent(*row) || row->syncer || !row->claimed || !(row->claimMask & SourceBit(lease.source)) ||
        row->claimTokens[static_cast<size_t>(lease.source)] != lease.token) return false;
    const Driven before = *row; VerbProof proof(before);
    return proof.Current() && proof.Matches(before) && LeaseScope(lease);
}
bool ReleaseOwned(const ClaimLease& lease, const std::function<bool()>& current) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::ReleaseOwned");
    if (!LeaseScope(lease)) return false;
    const auto* found = FindByEid(lease.eid);
    if (!found || !ClaimsCurrent(*found) || found->syncer || !found->claimed || !(found->claimMask & SourceBit(lease.source)) ||
        found->claimTokens[static_cast<size_t>(lease.source)] != lease.token) return false;
    const Driven before = *found;
    VerbProof proof(before); proof.caller = current;
    if (!proof.Current()) return false;
    Driven next = before;
    next.claimMask &= ~SourceBit(lease.source);
    next.claimTokens[static_cast<size_t>(lease.source)] = 0;
    uint64_t now = 0;
    if (!next.claimMask) now = coop::active_drive::NowMs();
    if (!proof.Matches(before) || !LeaseScope(lease)) return false;
    next.revision = NewRowRevision(); next.handoffFrom = 0; next.claimed = next.claimMask != 0;
    if (!next.claimed) { next.lastMoveMs = now; next.nextProbeMs = 0; }
    *FindByEid(next.eid) = next;
    return true;
}
void Release(void* actor) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::Release");
    const auto* found = actor ? Find(actor) : nullptr;
    if (!found || !found->claimed || found->syncer || !(found->claimMask & SourceBit(ClaimSource::Legacy))) return;
    Driven before = *found;
    // Old internal/default rows can have an implicit legacy contribution.
    if (!ClaimsCurrent(before) || !before.claimTokens[0]) {
        VerbProof proof(before); if (!proof.Current() || !proof.Matches(before)) return;
        if (!ClaimsCurrent(before)) ExpireClaims(before);
        StampClaims(before);
        before.claimTokens[0] = NextClaimToken(); before.revision = NewRowRevision();
        *FindByEid(before.eid) = before;
    }
    (void)ReleaseOwned(LeaseFor(before, ClaimSource::Legacy));
}
bool HasClaims(void* actor) {
    const auto* row = actor ? Find(actor) : nullptr;
    return row && (row->syncer || (row->claimed && (ClaimsCurrent(*row) ? row->claimMask : row->claimMask & SourceBit(ClaimSource::Legacy))));
}

void OnPeerWorldReady() {
    UE_ASSERT_GAME_THREAD("prop_drive_host::OnPeerWorldReady");
    if (g_driven.empty()) return;
    // Every driven prop's pose goes out again on the next tick, resting ones included, so the
    // joiner parks them where the other peers already have them. MTA's ResyncForPlayer
    // (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp) sends its
    // unoccupied vehicles, resting ones included, reliably and to that player alone, when it enters
    // a dimension; ours rides the unreliable broadcast stream once per join, so the others receive a
    // pose they already hold. The one-second stationary heartbeat now repairs a lost baseline
    // even if the prop never moves again.
    for (Driven& d : g_driven) {
        d.revision = NewRowRevision();
        d.resend      = true;
        d.nextProbeMs = 0;
    }
    UE_LOGI("[PROP-DRIVE] HOST world-ready re-send armed for %zu driven prop(s)", g_driven.size());
}

bool IsDriven(void* actor) {return actor && Find(actor)!=nullptr;}

namespace {
void EndHeld(coop::net::Session& s, const Receipt& receipt) {
    void* actor = receipt.value.ref.Get();
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    const bool location = E::TryGetActorLocation(actor, loc);
    if (!receipt.Current()) return;
    const bool rotation = location && E::TryGetActorRotation(actor, rot);
    if (!receipt.Current()) return;
    bool sent = false;
    if (location && rotation) sent = SendEnd(s, receipt, loc, rot, "taken by a hand", false);
    else if (receipt.value.everSent)
        sent = SendEnd(s, receipt, receipt.value.sentLoc, receipt.value.sentRot, "taken by a hand", false);
    else { Retire(receipt); return; }
    if (sent) Retire(receipt);
}

size_t StepHost(coop::net::Session& s, Receipt receipt, uint64_t now) {
    if (!receipt.Current()) return 0;
    Driven d = receipt.value;
    if (now < d.nextProbeMs) return 0;
    const auto turn = s.PropDrivePoseTurn(d.eid);
    if (!receipt.Current() || turn == coop::net::PoseTurn::Wait) return 0;
    void* actor = d.ref.Get();
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    const bool location = E::TryGetActorLocation(actor, loc);
    if (!receipt.Current() || (location && !PositionInBounds(loc))) return 0;
    const bool rotation = location && E::TryGetActorRotation(actor, rot);
    if (!receipt.Current() || (rotation && !Finite(rot))) return 0;
    const bool poseRead = location && rotation;
    if (!poseRead && !d.unreadSaid) {
        d.unreadSaid = true;
        UE_LOGW("[PROP-DRIVE] HOST eid=%u -- its location or rotation could not be read; it streams nothing and rests", d.eid);
    }
    const bool moved = poseRead && Moved(d, loc, rot);
    size_t joined = 0;
    if (poseRead && (moved || Elapsed(now, d.lastSendMs) >= 1000)) {
        coop::net::PropDriveRow pp{};
        pp.key = d.key; pp.elementId = d.eid; pp.ctx = d.gen;
        pp.x = loc.X; pp.y = loc.Y; pp.z = loc.Z;
        pp.pitch = ue_wrap::NormalizeAxis(rot.Pitch);
        pp.yaw = ue_wrap::NormalizeAxis(rot.Yaw);
        pp.roll = ue_wrap::NormalizeAxis(rot.Roll);
        RootBinding root(receipt);
        if (!root.Capture()) return 0;
        ue_wrap::FVector lin{};
        d.rootComp.Reset();
        const bool velocity = E::GetActorRootLinearVelocity(d.rootComp, actor, lin);
        if (!receipt.Current() || !Finite(lin) || !root.Current() || (velocity && d.rootComp.Get() != root.ref.Get())) return 0;
        if (velocity) { pp.vel[0] = lin.X; pp.vel[1] = lin.Y; pp.vel[2] = lin.Z; }
        pp.sendTimeMs = static_cast<uint32_t>(Elapsed(now, d.openMs));
        if (!Unheld(receipt) || !root.Current()) return 0;
        s.PublishPropDrivePose(pp);
        if (!receipt.Current() || !root.Current()) return 0;
        d.everSent = true; d.resend = false;
        d.sentLoc = loc; d.sentRot = rot; d.lastSendMs = now;
        if (moved) d.lastMoveMs = now;
        d.nextProbeMs = 0;
        if (!Save(receipt, d)) return 0;
        joined = turn == coop::net::PoseTurn::Join ? 1 : 0;
        if ((++g_published % 120) == 1)
            UE_LOGI("[PROP-DRIVE] HOST publish eid=%u gen=%u %s -> (%.1f,%.1f,%.1f)",
                    d.eid, static_cast<unsigned>(d.gen), d.claimed ? "claimed" : "coasting", loc.X, loc.Y, loc.Z);
        if (moved) return joined;
        // A heartbeat may end at rest in the same turn. Continue with the exact
        // revision just committed, without touching the previous receipt again.
        receipt = Receipt{receipt.context, *FindByEid(d.eid)};
        d = receipt.value;
    }
    if (Elapsed(now, d.lastMoveMs) >= kRestMs) {
        if (!d.claimed) {
            bool sent = false;
            if (poseRead) sent = SendEnd(s, receipt, loc, rot, "rested", true);
            else if (d.everSent) sent = SendEnd(s, receipt, d.sentLoc, d.sentRot, "rested", true);
            else { Retire(receipt); return joined; }
            if (sent) Retire(receipt);
            else Save(receipt, d);
            return joined;
        }
        d.nextProbeMs = now + kRestProbeMs;
    }
    Save(receipt, d);
    return joined;
}
} // namespace

static void Tick(coop::net::Session& s) {
    UE_ASSERT_GAME_THREAD("prop_drive_host::Tick");
    if (!BindSession(s, true) || g_ticking) return;
    Busy ticking(g_ticking);
    const Context context;
    coop::prop_end_delivery::Tick(s);
    if (!context.Current()) return;
    prop_knock::Tick(s);
    if (!context.Current() || g_driven.empty()) return;
    const uint64_t now = coop::active_drive::NowMs();
    const size_t n = g_driven.size(), start = g_turn % n;
    std::vector<Receipt> rows;
    rows.reserve(n);
    for (size_t k = 0; k < n; ++k) rows.push_back({context, g_driven[(start + k) % n]});
    size_t joined = 0;
    for (const Receipt& receipt : rows) {
        if (!context.Current()) return;
        if (!receipt.Matches()) continue;
        const Driven& d = receipt.value;
        const auto end = [&](coop::prop_syncer_host::Ending why) {
            if (receipt.Matches()) coop::prop_syncer_host::EndClientRow(*FindByEid(d.eid), why);
        };
        if (!receipt.ActorCurrent()) {
            if (d.syncer) end(coop::prop_syncer_host::Ending::Died);
            else Retire(receipt);
            continue;
        }
        if (!receipt.SourceCurrent()) { end(coop::prop_syncer_host::Ending::SlotLeft); continue; }
        if (!d.syncer && d.claimed && !ClaimsCurrent(d) && (d.claimMask & ~SourceBit(ClaimSource::Legacy))) {
            Driven expired = d; ExpireClaims(expired); expired.lastMoveMs = now;
            Save(receipt, expired); continue;
        }
        if (!d.syncer && d.claimed && !d.claimMask && d.handoffFrom) {
            Driven coast = d; coast.claimed = false; coast.handoffFrom = 0; coast.lastMoveMs = now;
            Save(receipt, coast); continue;
        }
        const bool held = InAHand(d.ref.Get());
        if (!receipt.Current()) continue;
        if (held) {
            if (d.syncer) end(coop::prop_syncer_host::Ending::Hand);
            else EndHeld(s, receipt);
            continue;
        }
        if (d.syncer) {
            const float v2 = d.admitVel.X*d.admitVel.X + d.admitVel.Y*d.admitVel.Y + d.admitVel.Z*d.admitVel.Z;
            const float rest = coop::net::throw_bounds::kThrowRestCmS;
            if (d.elected && d.admittedEver && v2 > rest*rest &&
                Elapsed(now, d.admitMs) >= coop::net::throw_bounds::kThrowSilenceMs)
                end(coop::prop_syncer_host::Ending::Silence);
            else joined += PublishAdmitted(s, receipt);
        } else joined += StepHost(s, receipt, now);
    }
    if (!context.Current()) return;
    g_driven.erase(std::remove_if(g_driven.begin(), g_driven.end(), [](const Driven& d) { return d.dead; }), g_driven.end());
    g_turn = g_driven.empty() ? 0 : (start + joined) % g_driven.size();
}

void Pump(coop::net::Session& s, bool isHost) {
    if (!BindSession(s, isHost) || g_pumping) return;
    Busy pumping(g_pumping);
    const Context context;
    coop::prop_syncer::Tick(s, isHost);   // first: the role for a feeder's Claim, and a client's rows of the props it syncs
    if (!context.Current()) return;
    if (isHost) {
        // The order is the point: a client's rows are judged first, so the row's turn in Tick publishes what
        // this tick admitted, and the apply tick then advances the host's copies they just fed.
        coop::prop_syncer_host::Admit(s);
        if (!context.Current()) return;
        Tick(s);
    }
    if (!context.Current()) return;
    coop::prop_drive_stream::TickApplyAndDrive(s);
}

void OnDisconnect() {
    const auto epoch = ++g_epoch;
    coop::prop_end_delivery::Clear();
    g_session = nullptr; g_world = g_serial = g_published = 0; g_stops = 0; g_host = false;
    g_driven.clear();
    g_turn = 0;
    g_genByEid.clear();
    prop_knock::OnDisconnect();
    if (epoch != g_epoch || g_session) return;
    coop::prop_syncer_host::OnDisconnect();   // its buckets, hand-back records and counters go with the rows
    if (epoch != g_epoch || g_session) return;
    coop::prop_syncer::OnDisconnect();        // a client's table, backoffs and counters (a link that drops ends the session)
}

}  // namespace coop::prop_drive_host
