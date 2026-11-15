// coop/props/prop_syncer_client.cpp -- see coop/props/prop_syncer.h. The client table: one entry per prop this
// client has claimed, Pending until the host answers and Syncing while the host admits its rows. MTA's
// unoccupied-vehicle push (reference/mtasa-blue/Client/mods/deathmatch/logic/CClientGame.cpp:4712-4733), with
// the claim sent once per episode and the stop decided by the client's own rest.

#include "coop/props/prop_syncer.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/prop_syncer_drill.h"     // IsEnabled: the last-grant record is kept only while the drill is on
#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/props/active_drive.h"        // NowMs
#include "coop/props/prop_drive_host.h"     // Claim (the host's), InAHand
#include "coop/props/prop_drive_rules.h"    // the epsilons, the rest rule, the send pace
#include "coop/props/prop_drive_stream.h"   // CloseGen
#include "coop/props/prop_damage_gate.h"   // ReplayAtGrant: the break a pending sequence held
#include "coop/props/prop_throw.h"         // TakeForGrant: the record of the throw a grant answers
#include "coop/props/remote_prop.h"         // ResolveLiveActorByEid
#include "coop/session/net_pump.h"          // HasAnnouncedWorldReady
#include "prop_drive_rows.h"                // co-located private header (src tree, not include/)
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/engine.h"

#include <cstdint>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace coop::prop_syncer {
namespace {

namespace E     = ue_wrap::engine;
namespace rules = coop::prop_drive_rules;
using coop::net::PropSyncRefusal;
using coop::net::PropSyncResult;
using dev::Count;

// MTA's MIN_PUSH_ANTISPAM_RATE (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.h:15), here per client and eid: after a refusal or a
// revoke the same prop is not claimed again sooner.
constexpr uint64_t kReclaimBackoffMs = 1500;
// A claim's syncer that is standing still re-sends its pose this often, well inside the host's kSyncerSilenceMs, and
// reads the prop at most every kRestProbeMs between beats. MTA sends a syncer's rows every 400 ms by default with the
// move and turn speed in each, on change only and with no beat (reference/mtasa-blue/Server/mods/deathmatch/logic/CMainConfig.cpp:1513, and the client's
// reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:19,66): rows here carry the pose and the copy's velocity at kClientDriveSendMs, the receiver
// lerping to the pose and the velocity serving the host's override, and the beat answers the host's silence rule, which MTA lacks.
constexpr uint64_t kSyncerBeatMs = 1000;
// A claim the host has not answered in this long is lost (an answer can only be lost to a session change): the entry
// goes Idle with the backoff, so nothing skips the host's poses for a prop for good. D7: kPendingMaxMs = 3000.
constexpr uint64_t kPendingMaxMs = 3000;
// An interaction in progress keeps the sync across a still moment: the rest End waits this long after the
// feeder's last Claim.
constexpr uint64_t kInteractionHoldMs = 250;
// The throw arms' red (container_throw_stepped_rows): from a thrown prop's kSteppedRowFrom-th row on, each row is offset
// in x by kSteppedRowCm more than the one before. The host measures a row from the last PASSING one, whose allowance grows
// by about kMaxFlightSpeedCmS * 0.025 = 252 cm per row, so the growing offset keeps every stepped row failing and a run of
// kMaxRefusedRows revokes.
constexpr uint32_t kSteppedRowFrom = 8;
constexpr float    kSteppedRowCm = 1000.f;
// The throwdepart arm's mute (dev::MuteThrownRows), default off, beside the stepped-rows red: every row and the End of
// every thrown (elected) entry on this client stop for the session, as a crash would stop them, so the host sees the
// silence mid-flight. Cleared by OnDisconnect. Game thread.
bool g_muteThrown = false;
enum class Role : uint8_t { Unknown, Host, Client };
enum class State : uint8_t { Pending, Syncing };

struct Entry {
    ue_wrap::CachedObjRef ref;
    ue_wrap::CachedObjRef cls;
    uint64_t revision = 0;
    ue_wrap::CachedObjRef rootComp;                // the actor's primitive root, resolved once for the per-row velocity read
    bool                  thrown = false;          // Syncing a thrown prop (the host's unasked grant), not a claim
    State                 state = State::Pending;
    uint16_t              seq = 0;
    uint8_t               gen = 0;                 // Syncing only
    coop::net::WireKey    key{};
    bool                  everSent = false;        // a row of this episode went out; sentLoc/sentRot hold the last
    ue_wrap::FVector      sentLoc{};
    ue_wrap::FRotator     sentRot{};
    uint32_t              rowsSent = 0;
    uint64_t              lastSendMs = 0;
    uint64_t              lastMoveMs = 0;          // the copy last moved past the epsilons (or the grant)
    uint64_t              claimSentMs = 0;         // Pending: when the claim went out
    uint64_t              nextProbeMs = 0;         // Syncing: the copy is not read before this
    uint64_t              lastInteractMs = 0;      // the feeder's last Claim
    uint64_t              epochMs = 0;             // Syncing: the clock the rows' sendTimeMs counts from (the grant; a throw: the throw)
};

// A re-claim of the eid is held off until `untilMs`, and for an InHand refusal while a hand still holds it.
struct Backoff { uint64_t untilMs = 0; bool inHand = false; uint64_t revision = 0; };

Role                                  g_role = Role::Unknown;
coop::net::Session*                   g_session = nullptr;
uint16_t                              g_seq = 0;
std::unordered_map<uint32_t, Entry>   g_table;
std::unordered_map<uint32_t, Backoff> g_backoff;
std::unordered_map<uint32_t, uint8_t> g_lastGen;     // eid -> the generation of its last grant, for the drill
std::unordered_map<uint32_t, uint64_t> g_lastGrantMs; // eid -> ms from the claim to its last grant, for the drill
uint32_t                              g_count[static_cast<unsigned>(Count::kCount)] = {};
uint32_t                              g_refused[8] = {};
uint64_t g_epoch = 0, g_revision = 0, g_stops = 0;
uint32_t g_world = 0, g_serial = 0;
bool g_ticking = false, g_claiming = false;
struct GrantStamp { uint8_t gen = 0; uint16_t seq = 0; uint8_t kind = 0; };
std::unordered_map<uint32_t, GrantStamp> g_seenGrants;

struct Context {
    coop::net::Session* session = g_session;
    uint64_t epoch = g_epoch, stops = g_stops;
    uint32_t world = g_world, serial = g_serial;
    Role role = g_role;
    bool Current() const {
        return session && session == g_session && epoch == g_epoch && role == g_role &&
            role != Role::Unknown && session->connected() && stops == session->StopCount() &&
            world == ue_wrap::world_identity::Generation() &&
            serial == coop::net::session_serial::Current() &&
            (role == Role::Client ? session->role() == coop::net::Role::Client :
                                   session->role() == coop::net::Role::Host);
    }
};
struct BusyScope {
    bool& flag;
    explicit BusyScope(bool& value) : flag(value) { flag = true; }
    ~BusyScope() { flag = false; }
};
uint64_t Elapsed(uint64_t now, uint64_t before) { return now >= before ? now - before : 0; }
bool Finite(const ue_wrap::FVector& p) { return std::isfinite(p.X) && std::isfinite(p.Y) && std::isfinite(p.Z); }
bool Finite(const ue_wrap::FRotator& p) { return std::isfinite(p.Pitch) && std::isfinite(p.Yaw) && std::isfinite(p.Roll); }
bool BoundEid(void* actor, uint32_t eid) {
    const auto bound = coop::element::Registry::Get().EidForActor(actor);
    return !coop::element::Registry::IsAllowedHostAllocatedEid(bound) || bound == eid;
}
void BindSession(coop::net::Session& s, bool host) {
    const Role role = host ? Role::Host : Role::Client;
    const auto world = ue_wrap::world_identity::Generation();
    const auto serial = coop::net::session_serial::Current();
    if (g_session != &s || g_role != role || g_world != world || g_serial != serial || g_stops != s.StopCount())
        OnDisconnect();
    g_session = &s; g_role = role; g_world = world; g_serial = serial; g_stops = s.StopCount();
}
struct Receipt {
    Context context;
    uint32_t eid;
    Entry value;
    bool Matches() const {
        if (!context.Current()) return false;
        const auto found = g_table.find(eid);
        return found != g_table.end() && found->second.revision == value.revision &&
            found->second.state == value.state && found->second.seq == value.seq && found->second.gen == value.gen;
    }
    bool Current() const {
        return Matches() && value.ref.Get() && value.cls.Get() &&
            ue_wrap::reflection::ClassOf(value.ref.Get()) == value.cls.Get() && BoundEid(value.ref.Get(), eid);
    }
};
struct RootBinding {
    void* actor;
    std::function<bool()> source;
    ue_wrap::CachedObjRef ref, cls;
    RootBinding(void* value, std::function<bool()> guard) : actor(value), source(std::move(guard)) {}
    bool Basic() const {
        return source() && ref.Get() && cls.Get() && ue_wrap::reflection::ClassOf(ref.Get()) == cls.Get();
    }
    bool Capture() {
        if (!source()) return false;
        void* root = E::RootComponentOf(actor);
        if (!source() || !root || !ue_wrap::reflection::IsLive(root)) return false;
        ref.Set(root); cls.Set(ue_wrap::reflection::ClassOf(root));
        return Basic();
    }
    bool Current() const {
        if (!Basic()) return false;
        void* root = E::RootComponentOf(actor);
        return Basic() && root == ref.Get();
    }
};
bool Save(const Receipt& receipt, Entry value) {
    if (!receipt.Current()) return false;
    value.revision = ++g_revision;
    g_table.insert_or_assign(receipt.eid, value);
    return true;
}
bool Erase(const Receipt& receipt) {
    if (!receipt.Matches()) return false;
    g_table.erase(receipt.eid);
    return true;
}
struct OrphanEnd {
    coop::net::PropSyncAnswerPayload answer{};
    Context context;
    uint64_t revision = 0;
    ue_wrap::CachedObjRef ref, cls;
};
std::unordered_map<uint32_t, OrphanEnd> g_orphanEnds;
bool g_ending = false;
bool FiniteEnd(const coop::net::PropDriveEndPayload& p);

const char* const kRefusalName[8] = {"none", "not_ready", "unknown", "in_hand", "host_authored", "other_syncer",
                                     "out_of_reach", "paced"};

uint32_t& CountRef(Count c) { return g_count[static_cast<unsigned>(c)]; }

// Each outcome is counted and said at the first and at every 100th, so a feeder's every-tick call costs the log nothing.
void Tally(Count c, const char* what, uint32_t eid) {
    const uint32_t n = ++CountRef(c);
    if (n == 1 || (n % 100) == 0) UE_LOGI("[PROP-SYNC] CLIENT %s eid=%u (#%u)", what, eid, n);
}

const char* ReasonName(coop::net::PropSyncReason r) {
    switch (r) {
        case coop::net::PropSyncReason::DevDrill:       return "dev drill";
        case coop::net::PropSyncReason::SwingerParent:  return "swinger parent";
    }
    return "client claim";
}

void SetBackoff(uint32_t eid, uint64_t nowMs, bool inHand) {
    g_backoff[eid] = Backoff{nowMs + kReclaimBackoffMs, inHand, ++g_revision};
}

// An episode that ends with a Revoked, a refusal or the host's End closes nothing in the stream's gate: the
// host's End of that generation applies when it arrives. Only this client's own End closes its generation.
void ClientClaim(void* actor, coop::net::PropSyncReason reason) {
    const Context context;
    if (!context.Current() || context.role != Role::Client || g_claiming) return;
    BusyScope claiming(g_claiming);
    // The host answers a slot that is not world-ready on a path its pre-world gate drops, so a claim sent earlier
    // would stay Pending with no answer coming.
    if (!coop::net_pump::HasAnnouncedWorldReady()) {
        Tally(Count::FeederRefused, "claim refused: this client's world is not ready", 0);
        return;
    }
    const coop::element::ElementId id = coop::element::Registry::Get().EidForActor(actor);
    // The host answers a claim only for a host-allocated id and drops any other as malformed, so an unanswered
    // claim would leave the entry Pending (skipping the host's poses) for good.
    if (!coop::element::Registry::IsAllowedHostAllocatedEid(id)) {
        Tally(Count::FeederRefused, "claim refused: the prop has no host element id", id == coop::element::kInvalidId ? 0u : id);
        return;
    }
    const uint32_t eid = static_cast<uint32_t>(id);
    const uint64_t now = coop::active_drive::NowMs();
    if (const auto open = g_table.find(eid); open != g_table.end()) {
        if (!open->second.ref.Is(actor)) return;
        open->second.lastInteractMs = now;   // an open episode: the interaction goes on, no send and no token
        open->second.revision = ++g_revision;
        return;
    }
    ue_wrap::CachedObjRef target, cls;
    if (!ue_wrap::reflection::IsLive(actor)) return;
    target.Set(actor); cls.Set(ue_wrap::reflection::ClassOf(actor));
    const auto current = [&] {
        return context.Current() && target.Get() && cls.Get() &&
            ue_wrap::reflection::ClassOf(actor) == cls.Get() &&
            coop::element::Registry::Get().EidForActor(actor) == eid && g_table.find(eid) == g_table.end();
    };
    if (!current()) return;
    if (const auto b = g_backoff.find(eid); b != g_backoff.end()) {
        const Backoff backoff = b->second;
        const bool held = backoff.inHand && coop::prop_drive_host::InAHand(actor);
        if (!current()) return;
        const auto latest = g_backoff.find(eid);
        if (latest == g_backoff.end() || latest->second.revision != backoff.revision) return;
        if (held || now < backoff.untilMs) {
            Tally(Count::HeldOff, "claim held off after a refusal or a revoke", eid);
            return;
        }
        g_backoff.erase(eid);
    }
    coop::net::PropSyncClaimPayload p{};
    const std::wstring keyW = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!current()) return;
    for (size_t i = 0; i < keyW.size() && i < sizeof(p.key.data); ++i) p.key.data[p.key.len++] = static_cast<char>(keyW[i]);
    p.eid    = eid;
    p.seq    = ++g_seq;
    p.reason = static_cast<uint8_t>(reason);
    Entry entry;
    entry.ref = target; entry.cls = cls; entry.revision = ++g_revision;
    entry.seq = p.seq; entry.key = p.key; entry.lastInteractMs = entry.claimSentMs = now;
    g_table.emplace(eid, entry);
    const Receipt receipt{context, eid, entry};
    const bool sent = context.session->SendReliable(coop::net::ReliableKind::PropSyncClaim, &p, sizeof(p));
    if (!context.Current() || !target.Get() || !cls.Get() || ue_wrap::reflection::ClassOf(actor) != cls.Get()) return;
    if (sent) {
        const uint32_t n = ++CountRef(Count::ClaimsSent);
        if (n == 1 || n % 100 == 0)
            UE_LOGI("[PROP-SYNC] CLIENT claim sent eid=%u seq=%u reason=%u (%s) (#%u)", eid,
                static_cast<unsigned>(p.seq), static_cast<unsigned>(p.reason), ReasonName(reason), n);
    }
    if (!receipt.Current()) return;
    if (!sent) {
        Erase(receipt);
        Tally(Count::SendFailed, "claim not sent: the reliable lane refused it", eid);
        return;
    }
}

// Retain End delivery for a grant with no local episode. Only accepted delivery
// closes the generation; a newer episode cancels this old completion.
void TryOrphanEnd(uint32_t eid) {
    if (g_ending) return;
    BusyScope ending(g_ending);
    const auto found = g_orphanEnds.find(eid);
    if (found == g_orphanEnds.end()) return;
    OrphanEnd owned = found->second;
    const auto matches = [&] {
        const auto row = g_orphanEnds.find(eid);
        return owned.context.Current() && row != g_orphanEnds.end() &&
            row->second.revision == owned.revision && !g_table.contains(eid);
    };
    if (!matches()) return;
    if (!owned.ref.Raw()) {
        void* actor = coop::remote_prop::ResolveLiveActorByEid(eid);
        if (!matches() || !actor || !ue_wrap::reflection::IsLive(actor)) return;
        owned.ref.Set(actor); owned.cls.Set(ue_wrap::reflection::ClassOf(actor));
        g_orphanEnds.at(eid) = owned;
    }
    const auto current = [&] {
        return matches() && owned.ref.Get() && owned.cls.Get() &&
            ue_wrap::reflection::ClassOf(owned.ref.Get()) == owned.cls.Get() && BoundEid(owned.ref.Get(), eid);
    };
    if (!current()) { if (matches()) g_orphanEnds.erase(eid); return; }
    void* actor = owned.ref.Get();
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    if (!E::TryGetActorLocation(actor, loc) || !current() || !Finite(loc)) return;
    if (!E::TryGetActorRotation(actor, rot) || !current() || !Finite(rot)) return;
    RootBinding root(actor, current);
    if (!root.Capture()) return;
    const auto bound = [&] { return root.Current(); };
    const auto end = coop::prop_drive_host::EndFromBody(actor, eid, owned.answer.gen, loc, rot, nullptr, bound);
    if (!current() || !root.Current() || end.eid != eid || end.gen != owned.answer.gen || !FiniteEnd(end)) return;
    const bool held = coop::prop_drive_host::InAHand(actor);
    if (!current() || held || !root.Current()) return;
    const bool sent = owned.context.session->SendReliable(coop::net::ReliableKind::PropDriveEnd, &end, sizeof(end));
    if (!current() || !sent) return;
    g_orphanEnds.erase(eid);
    coop::prop_drive_stream::CloseGen(eid, owned.answer.gen);
}
void EndUnheldGrant(const coop::net::PropSyncAnswerPayload& p) {
    const Context context;
    if (!context.Current() || g_table.contains(p.eid)) return;
    g_orphanEnds[p.eid] = OrphanEnd{p, context, ++g_revision};
    ++CountRef(Count::GrantedNoEntry);
    TryOrphanEnd(p.eid);
}

// The host elected this client the syncer of a prop it threw: the answer's seq is the throw's sequence, and no claim
// was sent. The record prop_throw kept supplies the actor and clock. Missing
// records and cancelled captures are distinct outcomes. The
// grant carries no state: MTA's StartSync carries the vehicle's position, rotation, velocity and turn speed and the
// client snaps to them (reference/mtasa-blue/Server/mods/deathmatch/logic/packets/CUnoccupiedVehicleStartSyncPacket.cpp:23-50, the client's reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:102-160),
// where here the thrower's copy has flown on since the release and is the truth, so a snap would undo the throw.
enum class ThrowGrant { Missing, Entered, Cancelled };
ThrowGrant EnterThrownSync(const coop::net::PropSyncAnswerPayload& p) {
    const Context context;
    if (!context.Current()) return ThrowGrant::Cancelled;
    void* actor = nullptr;
    uint64_t startMs = 0;
    const bool matched = coop::prop_throw::TakeForGrant(p.seq, &actor, &startMs);
    if (!context.Current()) return ThrowGrant::Cancelled;
    if (!matched) return ThrowGrant::Missing;
    if (!actor || !ue_wrap::reflection::IsLive(actor) || !BoundEid(actor, p.eid)) return ThrowGrant::Cancelled;
    const uint64_t now = coop::active_drive::NowMs();
    Entry e;
    e.ref.Set(actor);
    e.cls.Set(ue_wrap::reflection::ClassOf(actor));
    e.revision = ++g_revision;
    e.state = State::Syncing;
    e.seq = p.seq;
    e.gen = p.gen;
    g_table.insert_or_assign(p.eid, e);
    g_orphanEnds.erase(p.eid);
    const Receipt receipt{context, p.eid, e};
    if (!receipt.Current()) return ThrowGrant::Cancelled;
    const std::wstring keyW = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!receipt.Current()) return ThrowGrant::Cancelled;
    for (size_t i = 0; i < keyW.size() && i < sizeof(e.key.data); ++i) e.key.data[e.key.len++] = static_cast<char>(keyW[i]);
    e.lastMoveMs = now;
    e.epochMs = startMs;   // the rows count from the throw, on this clock
    e.lastInteractMs = 0;  // no feeder: the copy's own rest ends the episode
    e.thrown = true;
    if (!Save(receipt, e)) return ThrowGrant::Cancelled;
    const Receipt replay{context, p.eid, g_table.at(p.eid)};
    ++CountRef(Count::Granted);
    UE_LOGI("[PROP-SYNC] CLIENT granted a throw eid=%u gen=%u seq=%u ms=%llu (#%u)", p.eid, static_cast<unsigned>(p.gen),
            static_cast<unsigned>(p.seq), static_cast<unsigned long long>(Elapsed(now, startMs)), CountRef(Count::Granted));
    if (!replay.Current()) return ThrowGrant::Cancelled;
    coop::props::prop_damage_gate::ReplayAtGrant(actor);   // after the entry: the prop is authored here, so the call runs
    return replay.Current() ? ThrowGrant::Entered : ThrowGrant::Cancelled;
}

void OnGranted(const coop::net::PropSyncAnswerPayload& p, bool duplicate = false) {
    // A Throw grant is matched to a pending throw only, a Claim grant to a pending claim only: the two sequences are
    // counters of their own and must not meet. A grant with no match is the End of a generation nobody holds.
    if (p.kind == static_cast<uint8_t>(coop::net::PropSyncKind::Throw)) {
        if (EnterThrownSync(p) == ThrowGrant::Missing && !duplicate) EndUnheldGrant(p);
        return;
    }
    const auto it = g_table.find(p.eid);
    if (it == g_table.end() || it->second.state != State::Pending || it->second.seq != p.seq ||
        p.kind != static_cast<uint8_t>(coop::net::PropSyncKind::Claim)) {
        EndUnheldGrant(p);
        return;
    }
    // The grant carries no state and the copy keeps simulating, where MTA's StartSync resets the syncer to the
    // server's place and speed (the client's reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:102-134): a pusher's own copy is the contact
    // truth, and a snap to the host's pre-kick pose would undo the kick.
    Entry& e = it->second;
    e.state = State::Syncing;
    e.gen = p.gen;
    e.everSent = false;
    e.rowsSent = 0;
    const uint64_t now = coop::active_drive::NowMs();
    e.lastMoveMs = now;
    e.epochMs = now;
    e.nextProbeMs = 0;
    e.revision = ++g_revision;
    const uint64_t ms = Elapsed(now, e.claimSentMs);
    if (coop::dev::prop_syncer_drill::IsEnabled()) {
        g_lastGen[p.eid] = p.gen;
        g_lastGrantMs[p.eid] = ms;
    }
    ++CountRef(Count::Granted);
    UE_LOGI("[PROP-SYNC] CLIENT granted eid=%u gen=%u seq=%u ms=%llu (#%u)", p.eid, static_cast<unsigned>(p.gen),
            static_cast<unsigned>(p.seq), static_cast<unsigned long long>(ms), CountRef(Count::Granted));
}

void OnRefused(const coop::net::PropSyncAnswerPayload& p) {
    const auto it = g_table.find(p.eid);
    if (it == g_table.end() || it->second.state != State::Pending || it->second.seq != p.seq ||
        p.kind != static_cast<uint8_t>(coop::net::PropSyncKind::Claim)) {
        Tally(Count::AnswersDropped, "answer dropped: no claim is open for it", p.eid);
        return;
    }
    const uint64_t now = coop::active_drive::NowMs();
    const uint64_t ms = Elapsed(now, it->second.claimSentMs);
    g_table.erase(it);
    SetBackoff(p.eid, now, p.refusal == static_cast<uint8_t>(PropSyncRefusal::InHand));
    ++CountRef(Count::Refused);
    uint32_t& n = g_refused[p.refusal & 7u];
    if (++n == 1 || (n % 100) == 0)
        UE_LOGI("[PROP-SYNC] CLIENT refused eid=%u: %s ms=%llu (#%u)", p.eid, kRefusalName[p.refusal & 7u],
                static_cast<unsigned long long>(ms), n);
}

void OnRevoked(const coop::net::PropSyncAnswerPayload& p) {
    const auto it = g_table.find(p.eid);
    if (it == g_table.end() || it->second.state != State::Syncing || it->second.gen != p.gen ||
        it->second.seq != p.seq || it->second.thrown != (p.kind == static_cast<uint8_t>(coop::net::PropSyncKind::Throw))) {
        Tally(Count::AnswersDropped, "answer dropped: no entry of that generation", p.eid);
        return;
    }
    const Receipt receipt{Context{}, p.eid, it->second};
    void* actor = receipt.value.ref.Get();
    if (!Erase(receipt)) return;
    SetBackoff(p.eid, coop::active_drive::NowMs(), false);
    ++CountRef(Count::Revoked);
    UE_LOGI("[PROP-SYNC] CLIENT revoked eid=%u gen=%u -- the host took the prop back (#%u)", p.eid,
            static_cast<unsigned>(p.gen), CountRef(Count::Revoked));
    if (receipt.context.Current() && actor && !g_table.contains(receipt.eid)) coop::prop_throw::Ended(actor);
}

void HandEnded(const Receipt& receipt, uint64_t now) {
    void* actor = receipt.value.ref.Get();
    if (!Erase(receipt)) return;
    SetBackoff(receipt.eid, now, true);
    Tally(Count::HandEnds, "entry dropped: a hand took the prop here", receipt.eid);
    if (receipt.context.Current() && actor && !g_table.contains(receipt.eid)) coop::prop_throw::Ended(actor);
}
bool Unheld(const Receipt& receipt, uint64_t now) {
    if (!receipt.Current()) return false;
    const bool held = coop::prop_drive_host::InAHand(receipt.value.ref.Get());
    if (!receipt.Current()) return false;
    if (held) { HandEnded(receipt, now); return false; }
    return true;
}
bool FiniteEnd(const coop::net::PropDriveEndPayload& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
        std::isfinite(p.pitch) && std::isfinite(p.yaw) && std::isfinite(p.roll) &&
        std::isfinite(p.linVelX) && std::isfinite(p.linVelY) && std::isfinite(p.linVelZ) &&
        std::isfinite(p.angVelX) && std::isfinite(p.angVelY) && std::isfinite(p.angVelZ);
}

// One Syncing entry's turn: a row when the copy moved, when it is a thrown prop's (every row until its End), or when
// the beat is due; the rest End when it rested and the interaction is over.
void StepSyncing(coop::net::Session& s, const Receipt& receipt, uint64_t now) {
    if (!receipt.Current()) return;
    const uint32_t eid = receipt.eid;
    Entry e = receipt.value;
    void* actor = e.ref.Get();
    const auto current = [&] { return receipt.Current(); };
    if (e.thrown && g_muteThrown) return;

    // The engine is read (two dispatches for the pose, one more for the velocity of a row that is built) only when a row
    // or the rest End may be due, in the host rows' order: the probe cadence and the send pace first, then the queue's
    // turn. A moving copy is read at the pace, a still one at kRestProbeMs, so a feeder calling every tick costs
    // nothing between turns.
    if (now < e.nextProbeMs) return;
    if (e.everSent && Elapsed(now, e.lastSendMs) < rules::kClientDriveSendMs) return;
    const auto turn = s.PropDrivePoseTurn(eid);
    if (!current() || turn == coop::net::PoseTurn::Wait) return;
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    if (!E::TryGetActorLocation(actor, loc) || !current() || !Finite(loc)) return;
    if (!E::TryGetActorRotation(actor, rot) || !current() || !Finite(rot)) return;
    const bool moved = !e.everSent || rules::MovedPastEps(loc, rot, e.sentLoc, e.sentRot);
    if (moved) e.lastMoveMs = now;
    if (Elapsed(now, e.lastMoveMs) >= rules::kRestMs && Elapsed(now, e.lastInteractMs) >= kInteractionHoldMs) {
        RootBinding root(actor, current);
        if (!root.Capture()) return;
        const auto bound = [&] { return root.Current(); };
        const auto end = coop::prop_drive_host::EndFromBody(actor, eid, e.gen, loc, rot, nullptr, bound);
        if (!current() || !root.Current() || end.eid != eid || end.gen != e.gen || !FiniteEnd(end) ||
            !Unheld(receipt, now) || !root.Current()) return;
        const bool sent = s.SendReliable(coop::net::ReliableKind::PropDriveEnd, &end, sizeof(end));
        if (!current()) return;
        if (!sent) {
            Save(receipt, e);
            Tally(Count::SendFailed, "rest End not sent: the reliable lane refused it", eid);
            return;
        }
        if (!Erase(receipt)) return;
        ++CountRef(Count::RestEnds);
        UE_LOGI("[PROP-SYNC] CLIENT rest End eid=%u gen=%u -> (%.1f,%.1f,%.1f) after %u rows (#%u)", eid,
                static_cast<unsigned>(e.gen), loc.X, loc.Y, loc.Z, e.rowsSent, CountRef(Count::RestEnds));
        if (receipt.context.Current() && !g_table.contains(eid)) coop::prop_throw::Ended(actor);
        if (receipt.context.Current()) coop::prop_drive_stream::CloseGen(eid, e.gen);
        return;
    }
    // A thrown prop's syncer streams every row at the pace until its rest End, still or not: the host reads any gap in a
    // thrown row's arrivals as the thrower being gone (kThrowSilenceMs), so a quiet stretch would be taken for a crash.
    // The cost is about kRestMs / kClientDriveSendMs = 20 more rows per throw, over the rest window. A claim's row keeps
    // the still-skip and the 1 s beat.
    const bool due = moved || e.thrown || Elapsed(now, e.lastSendMs) >= kSyncerBeatMs;
    if (!due) {
        e.nextProbeMs = now + (Elapsed(now, e.lastMoveMs) >= rules::kRestMs ? rules::kRestProbeMs : rules::kClientDriveSendMs);
        Save(receipt, e);
        return;
    }
    coop::net::PropDriveRow pp{};
    pp.key       = e.key;
    pp.elementId = eid;
    pp.ctx       = e.gen;
    pp.x = loc.X; pp.y = loc.Y; pp.z = loc.Z;
    pp.pitch = ue_wrap::NormalizeAxis(rot.Pitch);
    pp.yaw   = ue_wrap::NormalizeAxis(rot.Yaw);
    pp.roll  = ue_wrap::NormalizeAxis(rot.Roll);
    static const bool s_stepped = coop::config::ResolveFlag(coop::config_registry::rows::container_throw_stepped_rows);
    if (!current()) return;
    if (s_stepped && e.thrown && e.rowsSent >= kSteppedRowFrom)   // the red: rows the host's test fails
        pp.x += kSteppedRowCm * static_cast<float>(e.rowsSent - kSteppedRowFrom + 1);
    // The copy's velocity as it is read now, and this row's age on the clock the grant or the throw started: the
    // host judges a row's step against the speed a prop flies and its age, and takes the prop over from the last passing one.
    ue_wrap::FVector lin{};
    RootBinding root(actor, current);
    if (!root.Capture()) return;
    // Let the engine helper validate that the fresh root is primitive before
    // caching it; pre-filling this cache bypasses its primitive-root check.
    e.rootComp.Reset();
    const bool velocity = E::GetActorRootLinearVelocity(e.rootComp, actor, lin);
    if (!current() || !Finite(lin) || !root.Current() || (velocity && e.rootComp.Get() != root.ref.Get())) return;
    if (velocity) { pp.vel[0] = lin.X; pp.vel[1] = lin.Y; pp.vel[2] = lin.Z; }
    pp.sendTimeMs = static_cast<uint32_t>(Elapsed(now, e.epochMs));
    if (!Unheld(receipt, now) || !root.Current()) return;
    s.PublishPropDrivePose(pp);   // queued: it goes out in its turn, refreshed until then
    if (!current()) return;
    e.everSent   = true;
    e.sentLoc    = loc;
    e.sentRot    = rot;
    e.lastSendMs = now;
    e.nextProbeMs = 0;   // the send pace governs the next read
    ++e.rowsSent;
    if (!Save(receipt, e)) return;
    Tally(Count::RowsSent, "row sent", eid);
}

}  // namespace

void Claim(void* actor, coop::net::PropSyncReason reason) {
    UE_ASSERT_GAME_THREAD("prop_syncer::Claim");
    if (!actor) return;
    if (!Context{}.Current()) return;
    if (g_role == Role::Host) { (void)coop::prop_drive_host::ClaimOwned(actor, coop::prop_drive_host::ClaimSource::ClientFeeder, ReasonName(reason)); return; }
    if (g_role == Role::Client) { ClientClaim(actor, reason); return; }
    Tally(Count::FeederRefused, "claim refused: no role yet", 0);
}

void Tick(coop::net::Session& s, bool isHost) {
    UE_ASSERT_GAME_THREAD("prop_syncer::Tick");
    BindSession(s, isHost);
    if (g_ticking || isHost || !Context{}.Current()) return;
    BusyScope ticking(g_ticking);
    const Context context;
    const uint64_t now = coop::active_drive::NowMs();
    std::vector<Receipt> entries;
    entries.reserve(g_table.size());
    for (const auto& [eid, entry] : g_table) entries.push_back({context, eid, entry});
    for (const auto& receipt : entries) {
        if (!context.Current()) return;
        if (!receipt.Matches()) continue;
        const Entry& e = receipt.value;
        void* actor = e.ref.Get();
        if (!actor || !receipt.value.cls.Get() || ue_wrap::reflection::ClassOf(actor) != receipt.value.cls.Get() ||
            !BoundEid(actor, receipt.eid)) {
            const bool sameBinding = actor && BoundEid(actor, receipt.eid);
            if (Erase(receipt)) {
                Tally(Count::Died, "entry dropped: the prop died here", receipt.eid);
                if (sameBinding && context.Current() && !g_table.contains(receipt.eid)) coop::prop_throw::Ended(actor);
            }
            continue;
        }
        if (!receipt.Current() || !Unheld(receipt, now)) continue;
        if (e.state == State::Pending && Elapsed(now, e.claimSentMs) >= kPendingMaxMs) {
            if (Erase(receipt)) {
                SetBackoff(receipt.eid, now, false);
                Tally(Count::PendingExpired, "claim dropped: the host never answered it", receipt.eid);
            }
            continue;
        }
        if (e.state == State::Syncing) StepSyncing(s, receipt, now);
    }
    std::vector<uint32_t> orphans;
    for (const auto& [eid, orphan] : g_orphanEnds) orphans.push_back(eid);
    for (auto eid : orphans) {
        if (!context.Current()) return;
        if (g_table.contains(eid)) { g_orphanEnds.erase(eid); continue; }
        TryOrphanEnd(eid);
    }
}

void OnAnswer(const coop::net::PropSyncAnswerPayload& input) {
    UE_ASSERT_GAME_THREAD("prop_syncer::OnAnswer");
    const Context context;
    if (!context.Current() || context.role != Role::Client) return;
    const auto p = input;
    if (!coop::element::Registry::IsAllowedHostAllocatedEid(p.eid) ||
        p.kind > static_cast<uint8_t>(coop::net::PropSyncKind::Throw)) return;
    bool duplicate = false;
    if (p.result == static_cast<uint8_t>(PropSyncResult::Granted)) {
        if (!p.gen) return;
        const auto seen = g_seenGrants.find(p.eid);
        duplicate = seen != g_seenGrants.end() && p.gen == seen->second.gen &&
            p.seq == seen->second.seq && p.kind == seen->second.kind;
        const auto active = g_table.find(p.eid);
        const bool pending = active != g_table.end() && active->second.state == State::Pending && active->second.seq == p.seq;
        if (duplicate && !pending && p.kind == static_cast<uint8_t>(coop::net::PropSyncKind::Claim)) return;
        if (active != g_table.end() && active->second.state == State::Syncing) {
            if (p.kind == static_cast<uint8_t>(coop::net::PropSyncKind::Claim)) return;
            if (active->second.thrown && p.gen == active->second.gen && p.seq == active->second.seq) return;
            // A fresh TakeForGrant nonce proves a new throw, including after a
            // long gap in the byte-generation clock. An unmatched one cannot
            // replace the active episode.
        }
        g_seenGrants[p.eid] = {p.gen, p.seq, p.kind};
    }
    switch (static_cast<PropSyncResult>(p.result)) {
        case PropSyncResult::Granted: OnGranted(p, duplicate); break;
        case PropSyncResult::Refused: OnRefused(p); break;
        case PropSyncResult::Revoked: OnRevoked(p); break;
        default: Tally(Count::AnswersDropped, "answer dropped: unknown result", p.eid); break;
    }
}

void OnHostEnd(uint32_t eid, uint8_t gen) {
    UE_ASSERT_GAME_THREAD("prop_syncer::OnHostEnd");
    if (!Context{}.Current() || g_role != Role::Client) return;
    const auto orphan = g_orphanEnds.find(eid);
    if (orphan != g_orphanEnds.end() && orphan->second.answer.gen == gen) g_orphanEnds.erase(orphan);
    const auto it = g_table.find(eid);
    if (it == g_table.end() || it->second.state != State::Syncing || it->second.gen != gen) return;
    const Receipt receipt{Context{}, eid, it->second};
    void* actor = receipt.value.ref.Get();
    if (!Erase(receipt)) return;
    SetBackoff(eid, coop::active_drive::NowMs(), false);
    ++CountRef(Count::HostEnds);
    UE_LOGI("[PROP-SYNC] CLIENT host End eid=%u gen=%u -- the host ended this client's row (#%u)", eid,
            static_cast<unsigned>(gen), CountRef(Count::HostEnds));
    if (receipt.context.Current() && actor && !g_table.contains(eid)) coop::prop_throw::Ended(actor);
}

bool SkipsHostPose(uint32_t eid) {
    if (!Context{}.Current() || g_role != Role::Client) return false;
    const auto found = g_table.find(eid);
    if (found == g_table.end() || !Receipt{Context{}, eid, found->second}.Current()) return false;
    Tally(Count::EchoSkipped, "host pose skipped: this client authors the drive", eid);
    return true;
}

bool AuthorsActorHere(void* actor) {
    if (!Context{}.Current() || g_role != Role::Client) return false;
    for (const auto& kv : g_table)
        if (kv.second.ref.Is(actor)) return Receipt{Context{}, kv.first, kv.second}.Current();
    return false;
}

void OnDisconnect() {
    ++g_epoch;
    g_role = Role::Unknown;
    g_session = nullptr;
    g_seq = 0;
    g_table.clear();
    g_backoff.clear();
    g_lastGen.clear();
    g_lastGrantMs.clear();
    g_seenGrants.clear();
    g_orphanEnds.clear();
    g_world = g_serial = 0; g_stops = 0;
    g_muteThrown = false;
    for (uint32_t& n : g_count) n = 0;
    for (uint32_t& n : g_refused) n = 0;
}

namespace dev {

void MuteThrownRows() { g_muteThrown = true; }

EntryView EntryOf(uint32_t eid) {
    EntryView v;
    const auto it = g_table.find(eid);
    if (it == g_table.end()) return v;
    v.state    = it->second.state == State::Pending ? EntryState::Pending : EntryState::Syncing;
    v.gen      = it->second.gen;
    v.seq      = it->second.seq;
    v.rowsSent = it->second.rowsSent;
    return v;
}

uint8_t LastGrantedGen(uint32_t eid) {
    const auto it = g_lastGen.find(eid);
    return it == g_lastGen.end() ? 0 : it->second;
}

uint64_t LastGrantMs(uint32_t eid) {
    const auto it = g_lastGrantMs.find(eid);
    return it == g_lastGrantMs.end() ? 0 : it->second;
}

uint32_t CountOf(Count c) {
    return c < Count::kCount ? g_count[static_cast<unsigned>(c)] : 0;
}

uint32_t RefusedBy(uint8_t refusal) {
    return refusal < 8 ? g_refused[refusal] : 0;
}

}  // namespace dev

}  // namespace coop::prop_syncer
