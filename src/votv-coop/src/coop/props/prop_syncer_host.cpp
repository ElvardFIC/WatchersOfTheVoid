// coop/props/prop_syncer_host.cpp -- see coop/props/prop_syncer_host.h. The claim judge, the admission of a
// syncing client's rows, the once-a-second pass that takes a row back, and the endings of a client's row.
// The template is the ATV's table (coop/interactables/atv_authority.cpp: a judged request, stream admission
// from the syncer only with a bucket and a late-syncer window, hand-back on silence or range); MTA files
// named below are reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp.

#include "coop/props/prop_syncer_host.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/prop_syncer_drill.h"        // IsEnabled: the drills' views are written only while one is on
#include "coop/dev/swinger_drill.h"            // IsEnabled: the swinger drill reads a dragged lid's parent count
#include "coop/element/intent_authority.h"
#include "coop/net/intent_bucket.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/element/registry.h"
#include "coop/net/throw_bounds.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"          // NowMs
#include "coop/props/prop_drive_host.h"       // InAHand, Coast
#include "coop/props/prop_drive_rules.h"      // kClientDriveSendMs
#include "coop/props/prop_drive_stream.h"     // FeedAdmitted, ReleaseHost
#include "coop/props/prop_element_tracker.h"  // GetPropElementIdForActor
#include "coop/props/prop_lifecycle.h"        // IsWireSuppressedPropClass
#include "coop/props/prop_throw.h"            // Mark: the thrown drop intent's election
#include "coop/props/prop_wire_parity.h"      // PhysFlagsOf: the flags an End carries
#include "coop/props/remote_prop.h"           // ResolveLiveActorByEid
#include "prop_drive_rows.h"                  // co-located private header (src tree, not include/)
#include "prop_end_delivery.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <vector>

namespace coop::prop_syncer_host {
namespace {

namespace E  = ue_wrap::engine;
namespace EL = coop::element;
namespace PR = ue_wrap::prop;
namespace R  = ue_wrap::reflection;
using coop::net::IntentBudget;
using coop::net::PropSyncRefusal;
using coop::net::PropSyncResult;
using coop::prop_drive_host::Driven;
namespace tb = coop::net::throw_bounds;

// The ATV's numbers (atv_authority.cpp): the claim pace is per sender, where MTA's is per vehicle, one push per
// 1500 ms (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:490); the stream bucket is the row's, its burst absorbing a syncer's send jitter.
// MTA polices no rate on the syncer's stream (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:263-474 has no bucket): the bucket is ours,
// because rows here are sent at 40 Hz and each one reaches the host's physics and every other peer.
constexpr IntentBudget kClaimBudget{2.f, 1.f};
constexpr IntentBudget kStreamBudget{16.f, 40.f};
// Twice mainPlayer.armLength, the helper adding the prop's bounds. MTA's contact radius is 30 m by default
// (reference/mtasa-blue/Server/mods/deathmatch/logic/CMainConfig.cpp:1528 vehicle_contact_sync_radius): a kick is within arm's reach, and the radius there is slack for
// the lag between a player's contact and the server's last position of it.
constexpr float kReachUU = 400.f;
constexpr uint64_t kSyncerSilenceMs = 5000;      // no admitted row this long: the host takes the prop
constexpr float kSyncerDistanceCm = 13000.f;     // MTA's default unoccupied syncer distance, 130 m
constexpr float kSyncerMaxSpeedCmS = 5000.f;     // provisional: a faster step between admitted rows is logged only
constexpr uint64_t kPassMs = 1000;

// The host's session, cached by the entry points: a row's reader that ends it (Tick, Claim, Coast) has none of its own.
coop::net::Session* g_session = nullptr;
uint64_t g_epoch = 0, g_stops = 0;
uint32_t g_world = 0, g_serial = 0;
std::array<uint64_t, coop::net::kMaxPeers> g_departures{};
bool g_draining = false;
uint64_t g_offerRevision = 0;
std::unordered_map<uint32_t, uint64_t> g_offers, g_retiring;
coop::roster_ledger::PerSlotState<coop::net::IntentBucket> g_claimBuckets;
// The slot a row was last handed back from, and when: the ATV's late-syncer record, kept after the row is gone.
struct HandBack { uint8_t slot = 0; uint64_t ms = 0; uint32_t occupant = 0; uint64_t departure = 0; };
std::unordered_map<uint32_t, HandBack> g_handBack;
uint64_t g_nextPassMs = 0;
struct Context {
    coop::net::Session* session = g_session;
    uint64_t epoch = g_epoch, stops = g_stops;
    uint32_t world = g_world, serial = g_serial;
    bool Current() const {
        return session && session == g_session && epoch == g_epoch &&
            world == ue_wrap::world_identity::Generation() && serial == coop::net::session_serial::Current() &&
            stops == session->StopCount() && session->connected() && session->role() == coop::net::Role::Host;
    }
};
uint64_t Elapsed(uint64_t now, uint64_t before) { return now >= before ? now - before : 0; }
bool ValidSlot(uint8_t slot) { return slot > 0 && slot < coop::net::kMaxPeers; }
struct Sender {
    Context context;
    uint8_t slot = 0;
    uint32_t occupant = 0;
    uint64_t departure = 0;
    explicit Sender(uint8_t value) : slot(value) {
        if (context.Current() && ValidSlot(slot)) {
            occupant = context.session->peerGenerationForSlot(slot);
            departure = g_departures[slot];
        }
    }
    bool Alive() const {
        return context.Current() && ValidSlot(slot) && occupant &&
            occupant == context.session->peerGenerationForSlot(slot) && departure == g_departures[slot] &&
            context.session->IsSlotConnected(slot);
    }
    bool Current() const { return Alive() && context.session->IsSlotWorldReady(slot); }
};
bool BoundActor(const Driven& row) {
    void* actor = row.ref.Get();
    return actor && row.cls.Get() && R::ClassOf(actor) == row.cls.Get() &&
        static_cast<uint32_t>(coop::prop_element_tracker::GetPropElementIdForActor(actor)) == row.eid;
}
struct Receipt {
    Context context;
    Sender sender;
    Driven value;
    explicit Receipt(const Driven& row) : sender(row.syncer), value(row) {}
    bool Matches() const {
        if (!context.Current()) return false;
        const Driven* row = coop::prop_drive_host::FindByEid(value.eid);
        return row && row->revision == value.revision && row->gen == value.gen && row->syncer == value.syncer &&
            row->claimSeq == value.claimSeq && row->ref.Raw() == value.ref.Raw();
    }
    bool SourceCurrent() const {
        return sender.Current() && value.ownerGeneration == sender.occupant && value.ownerDeparture == sender.departure;
    }
    bool Current() const { return Matches() && SourceCurrent() && BoundActor(value); }
    bool Alive() const {
        if (!context.Current() || !SourceCurrent() || !BoundActor(value)) return false;
        const Driven* row = coop::prop_drive_host::FindByEid(value.eid);
        return row && row->episode == value.episode && row->gen == value.gen &&
            row->syncer == value.syncer && row->claimSeq == value.claimSeq;
    }
};
bool Save(const Receipt& receipt, Driven row) {
    if (!receipt.Current()) return false;
    row.revision = coop::prop_drive_host::NewRowRevision();
    *coop::prop_drive_host::FindByEid(row.eid) = row;
    return true;
}
std::vector<Receipt> Snapshot() {
    std::vector<Receipt> rows;
    for (const auto& row : coop::prop_drive_host::Rows()) if (!row.dead && row.syncer) rows.emplace_back(row);
    return rows;
}
void Bind(coop::net::Session& s) {
    if (g_session && (g_session != &s || g_world != ue_wrap::world_identity::Generation() ||
        g_serial != coop::net::session_serial::Current() || g_stops != s.StopCount() ||
        s.role() != coop::net::Role::Host || !s.connected()))
        coop::prop_drive_host::OnDisconnect();
    g_session = &s; g_world = ue_wrap::world_identity::Generation();
    g_serial = coop::net::session_serial::Current(); g_stops = s.StopCount();
}
struct Target {
    Sender sender;
    uint32_t eid;
    uint64_t offer;
    ue_wrap::CachedObjRef ref, cls;
    Target(const Sender& source, uint32_t id) : sender(source), eid(id), offer(++g_offerRevision) { g_offers[eid] = offer; }
    bool Matches() const {
        const auto found = g_offers.find(eid);
        return sender.Current() && found != g_offers.end() && found->second == offer &&
            !coop::prop_drive_host::FindByEid(eid);
    }
    bool Current() const {
        return Matches() && ref.Get() && cls.Get() && R::ClassOf(ref.Get()) == cls.Get() &&
            static_cast<uint32_t>(coop::prop_element_tracker::GetPropElementIdForActor(ref.Get())) == eid;
    }
    ~Target() {
        const auto found = g_offers.find(eid);
        if (found != g_offers.end() && found->second == offer) g_offers.erase(found);
    }
};
struct Retirement {
    Receipt receipt;
    uint64_t token = 0;
    bool departed;
    Retirement(const Receipt& source, bool left = false) : receipt(source), departed(left) {
        if (!receipt.Matches()) return;
        token = coop::prop_drive_host::NewRowRevision();
        coop::prop_drive_host::EraseRow(*coop::prop_drive_host::FindByEid(receipt.value.eid));
        g_retiring[receipt.value.eid] = token;
    }
    static bool OwnsProof(const Receipt& receipt, uint64_t token, bool departed) {
        const auto found = g_retiring.find(receipt.value.eid);
        const auto& sender = receipt.sender;
        return token && receipt.context.Current() && found != g_retiring.end() && found->second == token &&
            !coop::prop_drive_host::FindByEid(receipt.value.eid) &&
            (departed ? (ValidSlot(sender.slot) && g_departures[sender.slot] == sender.departure &&
                         sender.context.session->peerGenerationForSlot(sender.slot) == sender.occupant) : receipt.SourceCurrent());
    }
    bool Owns() const { return OwnsProof(receipt, token, departed); }
    bool Current() const { return Owns() && BoundActor(receipt.value); }
    std::function<bool()> Guard() const {
        return [copy = receipt, id = token, left = departed] {
            return OwnsProof(copy, id, left) && BoundActor(copy.value);
        };
    }
    ~Retirement() {
        const auto found = g_retiring.find(receipt.value.eid);
        if (token && found != g_retiring.end() && found->second == token) g_retiring.erase(found);
    }
};

// Each outcome is counted and said at the first and at every 100th, so a client's flood costs the log nothing.
uint32_t g_granted = 0, g_admitted = 0, g_notSyncer = 0, g_lateSyncer = 0, g_staleGen = 0, g_paced = 0, g_steps = 0;
uint32_t g_endsAdmitted = 0, g_endsNotSyncer = 0, g_endsStale = 0, g_endsOutOfBounds = 0, g_endsClamped = 0;
uint32_t g_nonSyncerAdmitted = 0;
uint32_t g_elected = 0, g_electRefused = 0, g_rowsFailed = 0, g_thrownAdmitted = 0, g_jitRows = 0, g_rowsClamped = 0;
int64_t  g_jitMax = 0;   // the largest (sender dt - host arrival dt) of an elected row's later rows, ms
std::unordered_map<uint32_t, dev::Admitted> g_admittedBy;   // eid -> rows admitted, for the drill's judge
dev::ThrowView g_tv;   // the throw arms' view of the elected rows
uint32_t g_refused[8] = {};
uint32_t g_ended[static_cast<unsigned>(Ending::kCount)] = {};

const char* const kRefusalName[8] = {"none", "not_ready", "unknown", "in_hand", "host_authored", "other_syncer",
                                     "out_of_reach", "paced"};
const char* const kEndingName[static_cast<unsigned>(Ending::kCount)] = {
    "hand", "host verb", "died", "silent", "out of range", "slot left", "flight bound", "rows untrusted", "grant timeout"};

void Tally(uint32_t& n, const char* what, uint32_t eid, unsigned slot) {
    if (++n == 1 || (n % 100) == 0)
        UE_LOGI("[PROP-SYNC] HOST %s eid=%u slot %u (#%u)", what, eid, slot, n);
}

struct ControlAnswer;
using ControlReceipt = std::shared_ptr<ControlAnswer>;
std::unordered_map<uint64_t, ControlReceipt> g_answers;
bool g_answerDraining = false;
size_t g_answerCursor = 0;
constexpr size_t kAnswerBudget = 16;
constexpr uint64_t kAnswerRetryMs = 250;
uint64_t AnswerKey(uint32_t eid, uint8_t slot) { return (uint64_t(eid) << 8) | slot; }
struct ControlAnswer {
    Sender recipient;
    Driven owner;
    coop::net::PropSyncAnswerPayload payload{};
    bool grant = false, tombstone = false, attempted = false;
    uint64_t lastAttempt = 0;
    explicit ControlAnswer(const Driven& row) : recipient(row.syncer), owner(row) {}
    uint64_t Key() const { return AnswerKey(payload.eid, recipient.slot); }
    bool Basic() const {
        const auto found = g_answers.find(Key());
        if (found == g_answers.end() || found->second.get() != this || !recipient.Alive()) return false;
        if (owner.ownerGeneration != recipient.occupant || owner.ownerDeparture != recipient.departure) return false;
        if (!grant) return true;
        const Driven* row = coop::prop_drive_host::FindByEid(owner.eid);
        return row && row->episode == owner.episode && row->gen == owner.gen &&
            row->syncer == owner.syncer && row->claimSeq == owner.claimSeq &&
            row->ref.Raw() == owner.ref.Raw();
    }
    bool ActorCurrent() const { return Basic() && owner.ref.Get() && owner.cls.Get(); }
    bool Current() const {
        if (tombstone) return Basic(); // A death notice stops the old grant without dereferencing its dead actor.
        if (!ActorCurrent()) return false;
        void* cls = R::ClassOf(owner.ref.Raw());
        if (!ActorCurrent() || cls != owner.cls.Raw()) return false;
        const auto eid = coop::prop_element_tracker::GetPropElementIdForActor(owner.ref.Raw());
        if (!ActorCurrent() || static_cast<uint32_t>(eid) != owner.eid) return false;
        cls = R::ClassOf(owner.ref.Raw());
        return ActorCurrent() && cls == owner.cls.Raw();
    }
};
void EraseAnswer(const ControlReceipt& answer) {
    const auto found = g_answers.find(answer->Key());
    if (found != g_answers.end() && found->second == answer) g_answers.erase(found);
}
bool TryAnswer(const ControlReceipt& answer, uint64_t now) {
    if (!answer->Basic()) { EraseAnswer(answer); return false; }
    if (!answer->recipient.context.session->IsSlotWorldReady(answer->recipient.slot)) return false;
    if (answer->attempted && Elapsed(now, answer->lastAttempt) < kAnswerRetryMs) return false;
    answer->attempted = true; answer->lastAttempt = now;
    if (!answer->Current()) { EraseAnswer(answer); return false; }
    const auto payload = answer->payload;
    const bool sent = answer->recipient.context.session->SendReliableToSlot(answer->recipient.slot,
        coop::net::ReliableKind::PropSyncAnswer, &payload, sizeof(payload));
    if (!answer->Current()) { EraseAnswer(answer); return false; }
    if (sent) EraseAnswer(answer);
    return sent;
}
struct AnswerDrain {
    AnswerDrain() { g_answerDraining = true; }
    ~AnswerDrain() { g_answerDraining = false; }
};
void DrainAnswers(uint64_t now) {
    const Context context;
    if (g_answerDraining) return;
    if (!context.Current()) { g_answers.clear(); g_answerCursor = 0; return; }
    if (g_answers.empty()) return;
    AnswerDrain drain;
    std::vector<ControlReceipt> answers;
    answers.reserve(g_answers.size());
    for (const auto& [key, answer] : g_answers) answers.push_back(answer);
    const size_t count = (std::min)(kAnswerBudget, answers.size());
    for (size_t i = 0; i < count; ++i) {
        auto answer = answers[g_answerCursor++ % answers.size()];
        TryAnswer(answer, now);
        if (!context.Current()) return;
    }
}

bool Answer(coop::net::Session& s, uint8_t slot, uint32_t eid, uint16_t seq, uint8_t gen, PropSyncResult result,
            PropSyncRefusal why, coop::net::PropSyncKind kind = coop::net::PropSyncKind::Claim,
            const Driven* owner = nullptr, bool tombstone = false) {
    coop::net::PropSyncAnswerPayload a{};
    a.eid     = eid;
    a.seq     = seq;
    a.gen     = gen;
    a.result  = static_cast<uint8_t>(result);
    a.refusal = static_cast<uint8_t>(why);
    a.kind    = static_cast<uint8_t>(kind);
    if (result == PropSyncResult::Refused)
        return s.SendReliableToSlot(slot, coop::net::ReliableKind::PropSyncAnswer, &a, sizeof(a));
    if (!owner || !ValidSlot(slot) || owner->syncer != slot || owner->eid != eid || owner->gen != gen ||
        owner->claimSeq != seq || &s != g_session) return false;
    auto answer = std::make_shared<ControlAnswer>(*owner);
    answer->payload = a; answer->grant = result == PropSyncResult::Granted;
    answer->tombstone = tombstone && !answer->grant;
    g_answers[answer->Key()] = answer; // Publish before callbacks; a newer control for this recipient wins.
    if (g_answerDraining) return false;
    AnswerDrain drain;
    return TryAnswer(answer, coop::active_drive::NowMs());
}

// A refusal: counted, answered, and, for an out-of-reach claim on a prop nothing streams, its pose sent to the
// refused client by a host coast (one pose, then the rested End), so the client parks onto where the prop is. One
// coast per claim token, so the claim bucket bounds them. A Paced refusal costs the host nothing past the answer:
// MTA's refused push does nothing at all (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:490-507), and a flood must not buy rows.
void Refuse(coop::net::Session& s, const coop::net::PropSyncClaimPayload& p, uint8_t slot, PropSyncRefusal why,
            void* actor = nullptr) {
    const Sender sender(slot);
    if (!sender.Current()) return;
    ue_wrap::CachedObjRef ref, cls;
    if (actor) { ref.Set(actor); cls.Set(R::ClassOf(actor)); }
    Tally(g_refused[static_cast<unsigned>(why) & 7u], kRefusalName[static_cast<unsigned>(why) & 7u], p.eid, slot);
    Answer(s, slot, p.eid, p.seq, 0, PropSyncResult::Refused, why);
    if (why == PropSyncRefusal::OutOfReach && sender.Current() && ref.Get() && cls.Get() &&
        R::ClassOf(ref.Get()) == cls.Get() && !coop::prop_drive_host::FindByEid(p.eid) &&
        static_cast<uint32_t>(coop::prop_element_tracker::GetPropElementIdForActor(ref.Get())) == p.eid)
        coop::prop_drive_host::Coast(ref.Get(), "refused claim");
}

// The live prop a claim names: an Aprop_C the host minted the element id of (a client-spawned prop is a mirror
// here and reads another id), of a class the wire carries.
void* ResolveProp(Target& target) {
    void* a = coop::remote_prop::ResolveLiveActorByEid(target.eid);
    if (!target.Matches() || !a || !R::IsLive(a)) return nullptr;
    target.ref.Set(a); target.cls.Set(R::ClassOf(a));
    if (!target.Current() || !PR::IsDescendantOfProp(a) || !target.Current()) return nullptr;
    const auto name = R::ClassNameOf(a);
    if (!target.Current() || coop::prop_lifecycle::IsWireSuppressedPropClass(name)) return nullptr;
    return a;
}

void RecordHandBack(uint32_t eid, uint8_t slot, uint64_t nowMs) {
    const Sender sender(slot);
    if (sender.Current()) g_handBack[eid] = HandBack{slot, nowMs, sender.occupant, sender.departure};
}

// The honest race: the row of the syncer a hand-back just replaced, sent before it knew (the ATV's IsLateSyncer).
bool IsLateSyncer(uint32_t eid, uint8_t slot, uint64_t nowMs) {
    const auto it = g_handBack.find(eid);
    if (it == g_handBack.end() || it->second.slot != slot) return false;
    const HandBack record = it->second;
    const Sender sender(slot);
    if (!sender.Current() || record.occupant != sender.occupant || record.departure != sender.departure) return false;
    const int rtt = sender.context.session->rttMsForSlot(slot);
    const uint64_t window = static_cast<uint64_t>(rtt > 0 ? rtt : 0) + 2 * coop::prop_drive_rules::kClientDriveSendMs;
    return sender.Current() && Elapsed(nowMs, record.ms) <= window;
}

// The End of a row ended by the host: the pose and linear velocity the host trusts, and the released copy's flags. For
// an elected row, the last PASSING row; when none passed, the release pose the reach gate checked, at rest (a displayed
// row that failed the step is the syncer's claim, never the host's anchor). For any other row, the last admitted one;
// zero when the syncer never sent one; the rows carry no spin. MTA's receivers keep the last speed after a sync stops.
coop::net::PropDriveEndPayload EndOf(const Driven& d, void* actor) {
    static const ue_wrap::FVector kAtRest{};
    const bool passing = d.elected && d.passedEver;
    const bool fromRelease = d.elected && !d.passedEver;
    const ue_wrap::FVector&  loc = passing ? d.passLoc : fromRelease ? d.startLoc : d.admitLoc;
    const ue_wrap::FRotator& rot = passing ? d.passRot : d.admitRot;
    const ue_wrap::FVector&  vel = passing ? d.passVel : fromRelease ? kAtRest : d.admitVel;
    coop::net::PropDriveEndPayload p{};
    p.eid       = d.eid;
    p.gen       = d.gen;
    p.physFlags = coop::prop_wire_parity::PhysFlagsOf(actor);
    p.x = loc.X; p.y = loc.Y; p.z = loc.Z;
    p.pitch = ue_wrap::NormalizeAxis(rot.Pitch);
    p.yaw   = ue_wrap::NormalizeAxis(rot.Yaw);
    p.roll  = ue_wrap::NormalizeAxis(rot.Roll);
    p.linVelX = vel.X; p.linVelY = vel.Y; p.linVelZ = vel.Z;
    return p;
}

// An elected row's step test (coop/net/throw_bounds.h): its step agreeing with the speed a prop can fly. The first row is
// measured from the release (its pose, and its arrival on this clock), the rest from the last PASSING row (its stamp on
// the sender's clock, its arrival on this one); the host never compares clocks across peers, only deltas. A failing
// row is still displayed: AdmitRow counts it on the row, and a run of them revokes the syncer's right. MTA applies
// every row of the current syncer with no such test (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:263-340); ours costs trust and never
// display, the rule for a client's movement discontinuity (the host relays).
bool ElectedRowPasses(const Driven& row, const coop::net::PropDriveRow& e, uint64_t nowMs) {
    const bool first = !row.passedEver;
    const ue_wrap::FVector from = first ? row.startLoc : row.passLoc;
    const float dx = e.x - from.X, dy = e.y - from.Y, dz = e.z - from.Z;
    const float step = std::sqrt(dx * dx + dy * dy + dz * dz);
    const int64_t dtRow = first ? static_cast<int64_t>(e.sendTimeMs)
                                : static_cast<int64_t>(e.sendTimeMs) - static_cast<int64_t>(row.passSendMs);
    const int64_t arrival = static_cast<int64_t>(Elapsed(nowMs, first ? row.startMs : row.passMs));
    if (!first) {
        if (dtRow - arrival > g_jitMax || g_jitRows == 0) g_jitMax = dtRow - arrival;
        ++g_jitRows;
    }
    return tb::RowStepAgrees(step, dtRow, arrival);
}

// One row from slot `slot`, judged against the row that names it: the sender, the generation, the pace.
void AdmitRow(const coop::net::PropDriveRow& input, uint8_t slot, uint64_t nowMs) {
    const auto e = input;
    const Sender sender(slot);
    if (!sender.Current() || !EL::Registry::IsAllowedHostAllocatedEid(e.elementId) || !e.ctx ||
        !coop::net::PropPoseInBounds(e)) return;
    for (float value : {e.x,e.y,e.z,e.pitch,e.yaw,e.roll}) if (!std::isfinite(value)) return;
    // The developer red arm of the prop syncer drill: the sender test is skipped, so a forged row of the open
    // generation is admitted and the drill's forge arm must fail. Resolved once.
    static const bool s_redNotSyncer =
        coop::config::ResolveEnum(coop::config_registry::rows::prop_syncer_red) == "not_syncer";
    const Driven* live = coop::prop_drive_host::FindByEid(e.elementId);
    if (!live || (live->syncer != slot && !s_redNotSyncer)) {
        if (IsLateSyncer(e.elementId, slot, nowMs)) Tally(g_lateSyncer, "late row of a handed-back syncer", e.elementId, slot);
        else                                        Tally(g_notSyncer, "row refused: sender is no syncer", e.elementId, slot);
        return;
    }
    const Receipt receipt(*live);
    const auto current = [&] { return sender.Current() && receipt.Current(); };
    if (!current()) return;
    Driven owned = receipt.value;
    Driven* row = &owned;
    // Strict equality, where MTA's CanUpdateSync also accepts a zero stamp from either side (reference/mtasa-blue/Server/mods/deathmatch/logic/CElement.cpp:1304): gen 0
    // is "none" here, never issued, so a row that names no generation is stale by construction.
    if (e.ctx != row->gen) { Tally(g_staleGen, "row refused: stale generation", e.elementId, slot); return; }
    if (!row->streamBucket.Take(kStreamBudget, nowMs)) { Tally(g_paced, "row refused: paced", e.elementId, slot); return; }
    // A client's value is bounded, never trusted: every row's velocity is clamped to the flight ceiling before it is
    // stored, relayed or carried into an End (a velocity that is not finite becomes zero), so no End carries an unbounded
    // one. The clamp is a value bound on input, not a refusal.
    coop::net::PropDriveRow r = e;
    if (tb::ClampRowVelocity(r.vel) != tb::Clamp::Ok) Tally(g_rowsClamped, "row velocity clamped to the flight ceiling", e.elementId, slot);
    // An elected row's step is tested (a failing row is displayed all the same; it is counted on the row, and a run of
    // kMaxRefusedRows of them revokes the thrower's right: the host takes the prop back from the last passing row).
    // Any other row keeps the old rule: a movement discontinuity is logged, never refused (the host may cheat and the
    // relay holds), and its first row after a grant is not tested, the claim's reach test having bounded where it started.
    bool passed = false;
    if (row->elected) {
        passed = ElectedRowPasses(*row, r, nowMs);
        if (passed) {
            row->failedRun = 0;
        } else {
            ++g_rowsFailed;
            if (row->failedRun < 255) ++row->failedRun;
            if (row->failedTotal < 65535) ++row->failedTotal;
            // A run of failures, or as many again over the flight in any pattern (a forged row between true ones resets
            // the run, never the total); one lost or reordered row is not a cheat.
            if (row->failedRun >= tb::kMaxRefusedRows || row->failedTotal >= 2 * tb::kMaxRefusedRows) {
                if (Save(receipt, owned)) EndClientRow(*coop::prop_drive_host::FindByEid(e.elementId), Ending::RowsUntrusted);
                return;
            }
        }
    } else if (row->admittedEver) {
        const float dx = r.x - row->admitLoc.X, dy = r.y - row->admitLoc.Y, dz = r.z - row->admitLoc.Z;
        const float dtMs = static_cast<float>(std::max<uint64_t>(Elapsed(nowMs, row->admitMs), 1));
        if (std::sqrt(dx * dx + dy * dy + dz * dz) * 1000.f / dtMs > kSyncerMaxSpeedCmS)
            Tally(g_steps, "row steps faster than a prop moves (admitted)", e.elementId, slot);
    }
    row->admitLoc = ue_wrap::FVector{r.x, r.y, r.z};
    row->admitRot = ue_wrap::FRotator{r.pitch, r.yaw, r.roll};
    if (row->elected && !row->admittedEver) {   // the first row: where the host copy stood against where the thrower's copy was
        ue_wrap::FVector cur{};
        void* a = row->ref.Get();
        const bool read = a && E::TryGetActorLocation(a, cur);
        if (!current()) return;
        if (read)
            UE_LOGI("[PROP-SYNC] HOST first row eid=%u slot %u: the host copy is %.1f cm from the row, |v|=%.0f cm/s, release-to-row %llu ms",
                    e.elementId, static_cast<unsigned>(slot), std::sqrt((cur.X - r.x) * (cur.X - r.x) + (cur.Y - r.y) * (cur.Y - r.y) + (cur.Z - r.z) * (cur.Z - r.z)),
                    std::sqrt(r.vel[0] * r.vel[0] + r.vel[1] * r.vel[1] + r.vel[2] * r.vel[2]), static_cast<unsigned long long>(Elapsed(nowMs, row->startMs)));
    }
    row->admitVel = ue_wrap::FVector{r.vel[0], r.vel[1], r.vel[2]};
    if (row->elected && row->admittedEver) {   // kThrowSilenceMs' instrument: the host's arrival gap between two admitted rows
        const uint32_t gap = static_cast<uint32_t>(Elapsed(nowMs, row->admitMs));
        if (gap > row->maxGapMs) row->maxGapMs = gap;
    }
    row->admitSendMs = r.sendTimeMs;
    row->admitMs = nowMs;
    if (passed) {   // the take-back's starting point: the last row that agreed with the throw
        row->passedEver = true;
        row->passLoc    = row->admitLoc;
        row->passRot    = row->admitRot;
        row->passVel    = row->admitVel;
        row->passSendMs = r.sendTimeMs;
        row->passMs     = nowMs;
    }
    row->admittedEver = true;
    row->freshAdmit = true;
    row->nextProbeMs = 0;
    // The pose is the sender's, the key the row's: a forged key must not name another prop at the apply's fallback.
    r.key = row->key;
    coop::prop_drive_stream::FeedAdmitted(r, {receipt.context.session,
        [receipt] { return receipt.Current(); }, [receipt] { return receipt.Alive(); },
        receipt.value.ref, receipt.value.cls, receipt.value.episode});
    if (!current() || !Save(receipt, owned)) return;
    if (row->elected) ++g_thrownAdmitted;
    if (row->elected) {
        ++g_tv.lastRows;
        g_tv.lastRowMs = nowMs;
        if (!passed) ++g_tv.failedDisplayed;
    }
    if (row->syncer != slot) ++g_nonSyncerAdmitted;
    if (coop::dev::prop_syncer_drill::IsEnabled() || coop::dev::swinger_drill::IsEnabled()) {   // the drills' judges read it
        dev::Admitted& seen = g_admittedBy[e.elementId];
        if (seen.gen != row->gen) seen = dev::Admitted{row->gen, 0};
        ++seen.rows;
    }
    Tally(g_admitted, "row admitted", e.elementId, slot);
}

// Once a second (the ATV's RunPass rules 1 and 3): no admitted row for kSyncerSilenceMs (a thrown prop still moving is
// taken back sooner, per tick, at kThrowSilenceMs: prop_drive_host Tick; a resting one keeps this rule while its rest End
// is in flight), or the syncer's puppet beyond kSyncerDistanceCm; and for a thrown prop no admitted row GrantTimeoutMs
// of Session::rttMsForSlot (session.h:291; -1 before the slot's first sample) after the election, judged at this pass
// so the real wait is that value to that value plus a second, or a flight past kMaxThrowFlightMs. MTA re-elects the nearest player on its pulse (:133-146, FindSyncer :171-260); a prop here
// belongs to no author at rest and the host's world is always loaded (the ATV's reason, atv_authority.cpp:366-368).
// Diverges from MTA, which has no cap on a syncer's tenure and no first-row deadline (its only take-back is distance,
// :134-146): a thrown prop has a bounded life, and a lost grant would otherwise wait out the silence rule below.
void Pass(uint64_t nowMs) {
    const Context context;
    const auto rows = Snapshot();
    for (const Receipt& receipt : rows) {
        if (!context.Current()) return;
        if (!receipt.Matches()) continue;
        const Driven& d = receipt.value;
        const auto end = [&](Ending why) {
            if (receipt.Matches()) EndClientRow(*coop::prop_drive_host::FindByEid(d.eid), why);
        };
        if (!receipt.SourceCurrent()) { end(Ending::SlotLeft); continue; }
        if (!BoundActor(d)) { end(Ending::Died); continue; }
        // A thrown prop is the host's again when no row was admitted in time, or when its flight outlasts any throw.
        if (d.elected && !d.admittedEver &&
            Elapsed(nowMs, d.electedMs) >= tb::GrantTimeoutMs(g_session ? g_session->rttMsForSlot(d.syncer) : -1)) {
            end(Ending::GrantTimeout);
            continue;
        }
        if (!receipt.Current()) continue;
        if (d.elected && Elapsed(nowMs, d.startMs) >= tb::kMaxThrowFlightMs) { end(Ending::FlightBound); continue; }
        if (Elapsed(nowMs, d.admitMs) >= kSyncerSilenceMs) { end(Ending::Silence); continue; }
        void* actor = d.ref.Get();
        coop::RemotePlayer* rp = coop::players::Registry::Get().Puppet(d.syncer);
        ue_wrap::FVector at{}, prop{};
        // A puppet or a prop whose place does not read is not out of range: silence and departure cover a gone client.
        if (!actor || !rp) continue;
        const bool playerRead = rp->TryGetLocation(at);
        if (!receipt.Current() || !playerRead) continue;
        const bool propRead = E::TryGetActorLocation(actor, prop);
        if (!receipt.Current() || !propRead) continue;
        bool finite = true;
        for (float value : {at.X,at.Y,at.Z,prop.X,prop.Y,prop.Z}) finite &= std::isfinite(value);
        if (!finite) continue;
        const float dx = at.X - prop.X, dy = at.Y - prop.Y, dz = at.Z - prop.Z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) > kSyncerDistanceCm) end(Ending::Range);
    }
    if (!context.Current()) return;
    for (auto it = g_handBack.begin(); it != g_handBack.end();)   // a record outlives any late window (rtt + 50 ms)
        it = (Elapsed(nowMs, it->second.ms) > kSyncerSilenceMs) ? g_handBack.erase(it) : std::next(it);
}

}  // namespace

void OnRowOpened(uint32_t eid) {
    coop::prop_end_delivery::Opened(eid);
    g_offers.erase(eid);
    g_retiring.erase(eid);
    for (auto it = g_answers.begin(); it != g_answers.end();)
        it = (it->second->grant && it->second->owner.eid == eid) ? g_answers.erase(it) : std::next(it);
}

void OnClaim(coop::net::Session& s, const coop::net::PropSyncClaimPayload& input, uint8_t slot) {
    UE_ASSERT_GAME_THREAD("prop_syncer_host::OnClaim");
    const auto p = input;
    Bind(s);
    const Sender sender(slot);
    if (!sender.Current() || !EL::Registry::IsAllowedHostAllocatedEid(p.eid) ||
        p.key.len > sizeof(p.key.data)) return;
    // The tests, in the order of the design: the sender's tokens go before any other test runs (MTA tests its
    // anti-spam first, per vehicle), and a row on the prop is judged before the reach is measured.
    // The developer red arm of the prop syncer drill: every claim is refused as paced, so the drill's kick arm
    // must fail at its claim. Resolved once.
    static const bool s_redRefuse =
        coop::config::ResolveEnum(coop::config_registry::rows::prop_syncer_red) == "refuse";
    // The client sends no claim before its own world-ready (prop_syncer_client), so this is the host's backstop: the
    // pre-world gate drops a PropSyncAnswer to a slot that is not world-ready, and the answer here may not arrive.
    if (!s.IsSlotWorldReady(slot)) return Refuse(s, p, slot, PropSyncRefusal::NotReady);
    if (s_redRefuse || !g_claimBuckets[slot].Take(kClaimBudget, coop::active_drive::NowMs()))
        return Refuse(s, p, slot, PropSyncRefusal::Paced);
    // A host verb or push in flight wins (MTA's persistent syncer); another client's row is not stolen (MTA steals
    // from a non-persistent syncer, :498-501, which would ping-pong two pushers here). The sender's own row is
    // refused the same way: the client never re-claims while it waits or syncs.
    if (const Driven* row = coop::prop_drive_host::FindByEid(p.eid))
        return Refuse(s, p, slot, row->syncer == 0 ? PropSyncRefusal::HostAuthored : PropSyncRefusal::OtherSyncer);
    Target target(sender, p.eid);
    void* actor = ResolveProp(target);
    if (!target.Matches()) return;
    if (target.ref.Raw() && !target.Current()) return;
    if (!actor) return Refuse(s, p, slot, PropSyncRefusal::Unknown);
    const bool held = coop::prop_drive_host::InAHand(actor);
    if (!target.Current()) return;
    if (held) return Refuse(s, p, slot, PropSyncRefusal::InHand, actor);
    // MTA tests the dimension here too (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:492); VOTV has one level, so the reach stands in for
    // it. A body or a target the helper cannot read (a puppet that is dead, ragdolled or not yet posed) is Unknown, the
    // nearest analogue of a wrong-dimension player, and only a measured distance beyond reach is OutOfReach.
    const auto outcome = EL::IntentTarget::ForClientIntent(s, slot, kReachUU).Authorize(actor).outcome;
    if (!target.Current()) return;
    switch (outcome) {
        case EL::IntentOutcome::Ok:         break;
        case EL::IntentOutcome::OutOfReach: return Refuse(s, p, slot, PropSyncRefusal::OutOfReach, actor);
        default:                            return Refuse(s, p, slot, PropSyncRefusal::Unknown, actor);
    }
    const auto current = [&] { return target.Current(); };
    Driven* row = coop::prop_drive_host::OpenClientRow(actor, slot, "client claim", current);
    if (!sender.Current()) return;
    if (!row) return Refuse(s, p, slot, PropSyncRefusal::Unknown, actor);   // a hand or a class took it since the tests
    row->claimSeq = p.seq;
    row->ownerGeneration = sender.occupant; row->ownerDeparture = sender.departure;
    row->revision = coop::prop_drive_host::NewRowRevision();
    const Receipt receipt(*row);
    ++g_granted;
    UE_LOGI("[PROP-SYNC] HOST granted eid=%u gen=%u to slot %u (reason %u, seq %u) (#%u)", p.eid,
            static_cast<unsigned>(row->gen), static_cast<unsigned>(slot), static_cast<unsigned>(p.reason),
            static_cast<unsigned>(p.seq), g_granted);
    coop::dev::prop_syncer_drill::BeforeGrant(s, slot);   // the load arm's filler goes out ahead of the answer
    if (!receipt.Current()) return;
    Answer(s, slot, p.eid, p.seq, receipt.value.gen, PropSyncResult::Granted, PropSyncRefusal::None,
           coop::net::PropSyncKind::Claim, &receipt.value);
}

bool ElectSyncer(void* actor, uint8_t slot, uint16_t throwSeq, const float releaseLoc[3], uint64_t releaseMs,
                 bool reachJudged) {
    UE_ASSERT_GAME_THREAD("prop_syncer_host::ElectSyncer");
    static const bool s_noElect = coop::config::ResolveFlag(coop::config_registry::rows::container_throw_no_elect);
    if (s_noElect) return false;   // the throw arms' red: the host keeps its own simulation of the throw
    const Sender sender(slot);
    if (!sender.Current() || !actor || !R::IsLive(actor) || !releaseLoc) return false;
    const std::array<float, 3> release{releaseLoc[0], releaseLoc[1], releaseLoc[2]};
    for (float value : release) if (!std::isfinite(value) || std::fabs(value) > coop::net::kMaxCoord) return false;
    ue_wrap::CachedObjRef ref, cls; ref.Set(actor); cls.Set(R::ClassOf(actor));
    const uint32_t eid = static_cast<uint32_t>(coop::prop_element_tracker::GetPropElementIdForActor(actor));
    if (!EL::Registry::IsAllowedHostAllocatedEid(eid)) return false;
    const auto current = [&] {
        return sender.Current() && ref.Get() && cls.Get() && R::ClassOf(ref.Get()) == cls.Get() &&
            static_cast<uint32_t>(coop::prop_element_tracker::GetPropElementIdForActor(ref.Get())) == eid;
    };
    if (!current()) return false;
    if (throwSeq == 0) {
        Tally(g_electRefused, "election refused: the request carries no sequence", eid, slot);
        return false;
    }
    // The grant is answered to the thrower alone, on a lane a slot that is not world-ready drops.
    if (!g_session->IsSlotWorldReady(slot)) {
        Tally(g_electRefused, "election refused: the thrower is not world-ready", eid, slot);
        return false;
    }
    // The release is within the thrower's reach on the host: its puppet stands where a hand can have held the prop
    // (the claim's reach, kReachUU). MTA's takeover by a client (the push) is gated on contact radius too
    // (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:490-505). Out of reach: the release applied as it always did, and elects nothing.
    // A swing's caller has already judged the struck point against the held weapon's own reach from the eye
    // (coop/player/hit_judge), which a long weapon carries past this hand's reach.
    if (!reachJudged) {
        const ue_wrap::FVector at{release[0], release[1], release[2]};
        const auto outcome = EL::IntentTarget::ForClientIntent(*sender.context.session, slot, kReachUU).AuthorizeSegment(at, at).outcome;
        if (!current()) return false;
        if (outcome != EL::IntentOutcome::Ok) {
            Tally(g_electRefused, "election refused: the release is out of the thrower's reach", eid, slot);
            return false;
        }
    }
    // A per-slot cap where MTA balances (FindPlayerCloseToVehicle picks the player with the fewest syncing vehicles,
    // reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:228-261): a thrown prop has one candidate, the thrower.
    int held = 0;
    for (const Driven& d : coop::prop_drive_host::Rows())
        if (!d.dead && d.syncer == slot && d.elected) ++held;
    if (held >= tb::kMaxThrowRowsPerSlot) {
        Tally(g_electRefused, "election refused: the slot syncs too many thrown props", eid, slot);
        return false;
    }
    Driven* row = coop::prop_drive_host::ElectClientRow(actor, slot, "throw", current);
    if (!current()) return false;
    if (!row) {
        Tally(g_electRefused, "election refused: the prop cannot open a row", eid, slot);
        return false;
    }
    row->elected    = true;
    row->throwSeq   = throwSeq;
    row->startLoc   = ue_wrap::FVector{release[0], release[1], release[2]};
    row->startMs    = releaseMs;
    row->electedMs  = coop::active_drive::NowMs();
    row->failedRun  = 0;
    row->failedTotal = 0;
    row->passedEver = false;
    row->passSendMs = 0;
    row->passMs     = 0;
    row->passVel    = ue_wrap::FVector{};
    row->maxGapMs   = 0;
    row->claimSeq   = throwSeq;   // the grant, and a Revoked after it, echo the throw
    row->ownerGeneration = sender.occupant; row->ownerDeparture = sender.departure;
    row->revision = coop::prop_drive_host::NewRowRevision();
    const Receipt receipt(*row);
    ++g_tv.elected;
    g_tv.lastEid = row->eid;
    g_tv.lastGen = row->gen;
    g_tv.lastRows = 0;
    g_tv.lastRowMs = 0;
    g_tv.failedDisplayed = 0;
    UE_LOGI("[PROP-SYNC] HOST ELECT eid=%u gen=%u slot %u throw seq %u (#%u)", row->eid, static_cast<unsigned>(row->gen),
            static_cast<unsigned>(slot), static_cast<unsigned>(throwSeq), ++g_elected);
    Answer(*sender.context.session, slot, receipt.value.eid, throwSeq, receipt.value.gen, PropSyncResult::Granted, PropSyncRefusal::None,
           coop::net::PropSyncKind::Throw, &receipt.value);
    return receipt.Current();
}

bool ElectFromMark(void* actor) {
    UE_ASSERT_GAME_THREAD("prop_syncer_host::ElectFromMark");
    const Context context;
    if (!context.Current() || !actor || !R::IsLive(actor) || !coop::prop_throw::HasMarks()) return false;
    ue_wrap::CachedObjRef ref, cls; ref.Set(actor); cls.Set(R::ClassOf(actor));
    const auto current = [&] { return context.Current() && ref.Get() && cls.Get() && R::ClassOf(ref.Get()) == cls.Get(); };
    coop::prop_throw::Mark m;
    const auto key = PR::GetInteractableKeyString(actor);
    if (!current() || !coop::prop_throw::TakeMark(key, &m) || !current()) return false;
    return ElectSyncer(actor, m.slot, m.seq, m.loc, m.ms, /*reachJudged=*/false);
}

void OnClientEnd(coop::net::Session& s, const coop::net::PropDriveEndPayload& sent, uint8_t slot) {
    coop::net::PropDriveEndPayload p = sent;   // clamped below before it reaches the host's body or any relay
    UE_ASSERT_GAME_THREAD("prop_syncer_host::OnClientEnd");
    Bind(s);
    const Sender sender(slot);
    if (!sender.Current() || !EL::Registry::IsAllowedHostAllocatedEid(p.eid) || !p.gen) return;
    if (p._pad[0] || p._pad[1] || (p.physFlags & 0x80)) return;
    const uint64_t nowMs = coop::active_drive::NowMs();
    Driven* row = coop::prop_drive_host::FindByEid(p.eid);
    if (!row || row->syncer != slot) {
        Tally(IsLateSyncer(p.eid, slot, nowMs) ? g_lateSyncer : g_endsNotSyncer, "End refused: sender is no syncer", p.eid, slot);
        return;
    }
    if (p.gen != row->gen) { Tally(g_endsStale, "End refused: stale generation", p.eid, slot); return; }
    const Receipt receipt(*row);
    if (!receipt.Current()) return;
    // The End reaches the host's body: its pose meets the bounds a client's held pose meets (a refused End leaves the
    // row, which the silence pass takes back) and its velocity the throw bounds, clamped, before the host copy or any
    // relay. An End is not a throw input, so its spin meets the body ceiling, not the throw's.
    if (!coop::net::PropDriveEndInBounds(p)) {
        Tally(g_endsOutOfBounds, "End refused: pose out of bounds", p.eid, slot);
        return;
    }
    for (float value : {p.x,p.y,p.z,p.pitch,p.yaw,p.roll,p.linVelX,p.linVelY,p.linVelZ,p.angVelX,p.angVelY,p.angVelZ})
        if (!std::isfinite(value)) return;
    {
        float lin[3] = {p.linVelX, p.linVelY, p.linVelZ};
        float ang[3] = {p.angVelX, p.angVelY, p.angVelZ};
        if (tb::ClampPair(lin, tb::kMaxFlightSpeedCmS, ang, tb::kMaxBodyAngDegS) == tb::Clamp::Clamped) {
            p.linVelX = lin[0]; p.linVelY = lin[1]; p.linVelZ = lin[2];
            p.angVelX = ang[0]; p.angVelY = ang[1]; p.angVelZ = ang[2];
            Tally(g_endsClamped, "End velocity clamped", p.eid, slot);
        }
    }
    // The client's rest End is always admitted when syncer, generation and bounds match, admitted pose or not: it
    // carries the client's truth. The host's copy takes it, every other client gets it as received (the syncer
    // itself is skipped, MTA's pSkip: it ended the drive), and the row goes with no Revoked, since the client ended it.
    const Retirement retiring(receipt);
    if (!retiring.Current()) return;
    coop::prop_drive_stream::ReleaseHost(p, {receipt.context.session,
        retiring.Guard(), {}, receipt.value.ref, receipt.value.cls, receipt.value.episode});
    if (!retiring.Current()) return;
    coop::prop_end_delivery::Receipt delivery;
    if (!coop::prop_end_delivery::Queue(s, p, receipt.value.ref, receipt.value.cls, slot, retiring.Guard(), &delivery)) return;
    if (!retiring.Current()) { coop::prop_end_delivery::Cancel(delivery); return; }
    RecordHandBack(p.eid, slot, nowMs);
    Tally(g_endsAdmitted, "End admitted (rest)", p.eid, slot);
    if (receipt.value.elected)
        UE_LOGI("[PROP-SYNC] HOST thrown eid=%u gen=%u slot %u rested: release-to-End %llu ms, rows admitted %u, jitter (sender dt - arrival dt) max %lld ms over %u rows, maxgap=%u ms",
                p.eid, static_cast<unsigned>(p.gen), static_cast<unsigned>(slot), static_cast<unsigned long long>(Elapsed(nowMs, receipt.value.startMs)), g_thrownAdmitted, static_cast<long long>(g_jitMax), g_jitRows,
                static_cast<unsigned>(receipt.value.maxGapMs));
}

void Admit(coop::net::Session& s) {
    UE_ASSERT_GAME_THREAD("prop_syncer_host::Admit");
    Bind(s);
    const Context context;
    if (g_draining || !context.Current()) return;
    struct Busy { Busy(){g_draining = true;} ~Busy(){g_draining = false;} } busy;
    const uint64_t nowMs = coop::active_drive::NowMs();
    std::vector<coop::net::PropDriveRow> take[coop::net::kMaxPeers];
    std::vector<Sender> senders;
    for (uint8_t slot = 0; slot < coop::net::kMaxPeers; ++slot) senders.emplace_back(slot);
    const uint32_t got = s.TakeRemotePropDriveBatches(take);
    if (!context.Current()) return;
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot)
        if (got & (1u << slot))
            for (const auto& e : take[slot]) {
                if (!context.Current()) return;
                if (!senders[slot].Current()) break;
                AdmitRow(e, static_cast<uint8_t>(slot), nowMs);
            }
    if (nowMs >= g_nextPassMs) {
        g_nextPassMs = nowMs + kPassMs;
        Pass(nowMs);
    }
    if (context.Current()) DrainAnswers(nowMs);
}

void OnSlotLeft(uint8_t slot) {
    UE_ASSERT_GAME_THREAD("prop_syncer_host::OnSlotLeft");
    if (!ValidSlot(slot) || !Context{}.Current()) return;
    ++g_departures[slot];
    coop::prop_end_delivery::Departed(slot);
    g_claimBuckets[slot].Reset();
    const Context context;
    const auto rows = Snapshot();
    for (const auto& receipt : rows) {
        if (!context.Current()) return;
        if (receipt.value.syncer == slot && receipt.Matches())
            EndClientRow(*coop::prop_drive_host::FindByEid(receipt.value.eid), Ending::SlotLeft);
    }
    if (!context.Current()) return;
    for (auto it = g_handBack.begin(); it != g_handBack.end();)
        it = (it->second.slot == slot) ? g_handBack.erase(it) : std::next(it);
}

void EndClientRow(Driven& input, Ending why) {
    UE_ASSERT_GAME_THREAD("prop_syncer_host::EndClientRow");
    if (static_cast<unsigned>(why) >= static_cast<unsigned>(Ending::kCount) || !input.syncer) return;
    const Receipt receipt(input);
    if (!receipt.Matches()) return;
    const bool sourceCurrent = receipt.SourceCurrent();
    const bool hostOverride = why == Ending::HostVerb && BoundActor(receipt.value);
    if (why != Ending::SlotLeft && why != Ending::Died && !hostOverride && !receipt.Current()) return;
    Driven d = receipt.value;
    const Retirement retiring(receipt, why == Ending::SlotLeft || (hostOverride && !sourceCurrent));
    if (!retiring.Owns()) return;
    const uint64_t nowMs = coop::active_drive::NowMs();
    const uint8_t syncer = d.syncer;
    const uint8_t gen = d.gen;
    void* actor = d.ref.Get();
    // A host verb, the three endings that are the host's own judgement of a thrown prop, a leaving or silent thrower and any
    // ending of a thrown prop nothing was admitted for (but Died and Hand: the held lane streams a hand's) hand the prop to
    // the host: its copy goes on from the last passing row, or from its own state when none was, and its stream takes over
    // under a new generation. MTA re-elects a syncer whenever one is lost, so the prop is never left unsynced while it
    // moves (reference/mtasa-blue/Server/mods/deathmatch/logic/CUnoccupiedVehicleSync.cpp:36-40, :143-152).
    const bool unadmitted = d.elected && !d.admittedEver && why != Ending::Died && why != Ending::Hand;
    const bool toHost = why == Ending::HostVerb || why == Ending::FlightBound || why == Ending::RowsUntrusted ||
                        why == Ending::GrantTimeout ||
                        (d.elected && (why == Ending::SlotLeft || why == Ending::Silence)) || unadmitted;
    // (1) the host's copy is released at the trusted pose and velocity, (2) the End is built from it. With nothing
    // admitted no receiver saw this generation and the copy was never parked, and a dead prop has no copy: no release,
    // no End.
    if (why != Ending::Died && d.admittedEver && actor) {
        if (!retiring.Current()) return;
        coop::net::PropDriveEndPayload end = EndOf(d, actor);
        if (!retiring.Current()) return;
        if (d.elected) g_tv.lastTakeBackSpeed = std::sqrt(end.linVelX * end.linVelX + end.linVelY * end.linVelY + end.linVelZ * end.linVelZ);
        coop::prop_drive_stream::ReleaseHost(end, {receipt.context.session,
            retiring.Guard(), {}, receipt.value.ref, receipt.value.cls, receipt.value.episode});
        if (!retiring.Current()) return;
        // A hand-over sends no End: its new generation's poses supersede this one on every receiver.
        if (!toHost && g_session) {
            end.physFlags = coop::prop_wire_parity::PhysFlagsOf(actor);
            if (!retiring.Current()) return;
            coop::prop_end_delivery::Receipt delivery;
            if (!coop::prop_end_delivery::Queue(*g_session, end, d.ref, d.cls, -1, retiring.Guard(), &delivery)) return;
            if (!retiring.Current()) { coop::prop_end_delivery::Cancel(delivery); return; }
        }
    }
    // (3) the syncer is told, with the generation it ends (before any bump) and the seq of its claim or the sequence of
    // its throw; a slot that left is told nothing.
    if (why != Ending::SlotLeft && sourceCurrent && g_session)
        Answer(*g_session, syncer, d.eid, d.claimSeq, gen, PropSyncResult::Revoked, PropSyncRefusal::None,
               d.elected ? coop::net::PropSyncKind::Throw : coop::net::PropSyncKind::Claim, &d, why == Ending::Died);
    if (!retiring.Owns() || (why != Ending::Died && !retiring.Current())) return;
    if (sourceCurrent) RecordHandBack(d.eid, syncer, nowMs);
    Tally(g_ended[static_cast<unsigned>(why)], kEndingName[static_cast<unsigned>(why)], d.eid, syncer);
    if (d.elected && toHost) {
        ++g_tv.takeBacks;
        if (why == Ending::SlotLeft) ++g_tv.slotLeft;
        if (why == Ending::Silence) ++g_tv.silence;
        g_tv.lastTakeBack = static_cast<uint8_t>(why);
        g_tv.lastTakeBackMs = nowMs;
        if (!d.admittedEver) g_tv.lastTakeBackSpeed = 0.f;
        if (why == Ending::RowsUntrusted) {
            ++g_tv.revokes;
            g_tv.revokeRows = g_tv.lastRows;
            g_tv.revokeTotal = d.failedTotal;
        }
    }
    // One line per flight at the take-back; a row that failed the step test logs nothing itself.
    if (why == Ending::RowsUntrusted)
        UE_LOGI("[PROP-SYNC] HOST took eid=%u back from slot %u (rows untrusted: %u in a row, %u in the flight) -- %s, maxgap=%u ms",
                d.eid, static_cast<unsigned>(syncer), static_cast<unsigned>(d.failedRun), static_cast<unsigned>(d.failedTotal),
                d.passedEver ? "from its last passing row" : "no row passed, from its release pose at rest",
                static_cast<unsigned>(d.maxGapMs));
    else if (d.elected && toHost && why != Ending::HostVerb) {
        char label[40];   // a silent thrower's line names how long it was silent
        if (why == Ending::Silence)
            std::snprintf(label, sizeof(label), "silence %llu ms", static_cast<unsigned long long>(Elapsed(nowMs, d.admitMs)));
        else
            std::snprintf(label, sizeof(label), "%s", kEndingName[static_cast<unsigned>(why)]);
        UE_LOGI("[PROP-SYNC] HOST took eid=%u back from slot %u (%s) -- %s, maxgap=%u ms", d.eid, static_cast<unsigned>(syncer),
                label,
                d.passedEver ? "from its last passing row"
                             : d.admittedEver ? "no row passed, from its release pose at rest"
                                              : "from its own copy, no row was admitted",
                static_cast<unsigned>(d.maxGapMs));
    }
    // (4) the row goes, or for a hand-over it becomes the host's: a new generation, nothing sent yet, the stream
    // continuing from the released copy. A verb's claim stays its own; the host's judgement coasts to rest.
    if (!toHost) return;
    if (!retiring.Current()) return;
    d.syncer       = 0;
    d.gen          = coop::prop_drive_host::BumpGen(d.eid);
    d.claimed      = why == Ending::HostVerb;
    d.everSent     = false;
    d.resend       = false;
    d.unreadSaid   = false;
    d.openMs       = nowMs;
    d.lastMoveMs   = nowMs;
    d.nextProbeMs  = 0;
    d.admittedEver = false;
    d.freshAdmit   = false;
    d.admitMs      = 0;
    d.admitVel     = ue_wrap::FVector{};
    d.admitSendMs  = 0;
    d.claimSeq     = 0;
    d.elected      = false;
    d.throwSeq     = 0;
    d.failedRun    = 0;
    d.failedTotal  = 0;
    d.electedMs    = 0;
    d.passedEver   = false;
    d.passVel      = ue_wrap::FVector{};
    d.passSendMs   = 0;
    d.passMs       = 0;
    d.maxGapMs     = 0;
    d.streamBucket.Reset();
    d.dead = false;
    d.ownerGeneration = 0; d.ownerDeparture = 0;
    d.handoffFrom = receipt.value.revision;
    d.claimMask = 0; d.claimTokens.fill(0);
    d.revision = coop::prop_drive_host::NewRowRevision();
    d.episode = d.revision;
    auto& rows = coop::prop_drive_host::Rows();
    const auto old = std::find_if(rows.begin(), rows.end(), [&](const Driven& row) {
        return row.dead && row.eid == d.eid && row.ref.Raw() == d.ref.Raw();
    });
    if (old != rows.end()) *old = d;
    else rows.push_back(d);
    OnRowOpened(d.eid);
}

void OnDisconnect() {
    coop::prop_end_delivery::Clear();
    ++g_epoch;
    g_session = nullptr;
    for (int slot = 0; slot < g_claimBuckets.size(); ++slot) g_claimBuckets[slot].Reset();
    g_handBack.clear();
    g_offers.clear(); g_retiring.clear();
    g_answers.clear(); g_answerCursor = 0;
    g_world = g_serial = 0; g_stops = 0;
    g_departures.fill(0);
    g_nextPassMs = 0;
    g_granted = g_admitted = g_notSyncer = g_lateSyncer = g_staleGen = g_paced = g_steps = 0;
    g_endsAdmitted = g_endsNotSyncer = g_endsStale = g_endsOutOfBounds = g_endsClamped = 0;
    g_nonSyncerAdmitted = 0;
    g_elected = g_electRefused = g_rowsFailed = g_thrownAdmitted = g_jitRows = g_rowsClamped = 0;
    g_jitMax = 0;
    g_admittedBy.clear();
    g_tv = dev::ThrowView{};
    std::fill(std::begin(g_refused), std::end(g_refused), 0u);
    std::fill(std::begin(g_ended), std::end(g_ended), 0u);
}

namespace dev {

Admitted AdmittedOf(uint32_t eid) {
    const auto it = g_admittedBy.find(eid);
    return it == g_admittedBy.end() ? Admitted{} : it->second;
}

uint32_t NotSyncerCount() { return g_notSyncer; }

uint32_t NonSyncerAdmitted() { return g_nonSyncerAdmitted; }

const ThrowView& ThrowViewNow() { return g_tv; }

}  // namespace dev

}  // namespace coop::prop_syncer_host
