// coop/world/fuse_tower_ops.cpp -- see coop/world/fuse_tower_ops.h.

#include "coop/world/fuse_tower_ops.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/element/intent_authority.h"
#include "coop/net/intent_bucket.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_lifecycle.h"  // a refused pull's fuse reaped
#include "coop/props/remote_prop.h"     // the destroys this host applies: a spent fuse
#include "coop/session/net_pump.h"      // IsInAnnouncedWorld
#include "coop/world/fuse_tower_kind.h"
#include "coop/world/fuse_tower_rows.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"             // TryGetActorLocation
#include "ue_wrap/engine/engine_mainplayer.h"  // what the local player holds
#include "ue_wrap/facts/function_row.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coop::fuse_tower_ops {
namespace {

namespace FK = coop::fuse_tower_kind;
namespace EL = coop::element;
namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
using coop::net::FuseTowerPayload;

static_assert(sizeof(FuseTowerPayload{}.ack) / sizeof(uint16_t) == coop::net::kMaxPeers, "FuseTower acknowledgements cover every peer");

// A slot's ops wait, in arrival order, while the towers have not resolved, their sender has no body here yet (a
// joiner's first pose follows its world-ready by seconds) or an insert's spend has not arrived (its destroy rides
// the Default lane); each is taken as refused once it has waited this long (coop/world/power_grid's bound).
constexpr uint64_t kWaitMs = 10000;
constexpr size_t   kMaxQueued = 32;
// A client keeps at most this many claims unanswered (its pulls and inserts the host has not told it of): one past
// it is refused at its own gate. A queue-full refusal then pushes no more refused claims than the client holds, and
// the refused queue below always has room for them.
constexpr size_t   kMaxUnansweredClaims = kMaxQueued;
// A lights-out puzzle is pressed a few times a second; a sender's ops run at this rate, the rest waiting their turn.
constexpr coop::net::IntentBudget kOpBudget{8.0f, 8.0f};
// A spend waits for its insert's claim; a player's inserts come seconds apart, so a few a slot cover any that race.
constexpr int kSpendsPerSlot = 4;

std::atomic<coop::net::Session*> g_session{nullptr};
uint64_t g_epoch = 0;
uint32_t g_world = 0, g_serial = 0;
uint64_t g_boundStops=0;
coop::net::Role g_boundRole=coop::net::Role::Host;
std::array<uint64_t, coop::net::kMaxPeers> g_peerEpoch{};
std::array<uint32_t, coop::net::kMaxPeers> g_occupants{};
bool g_hostExecuting = false, g_draining = false;
uint8_t g_drainSlot = 1;
uint8_t g_activeSlot = 0;
uint16_t g_activeSeq = 0;

struct Scope {
    coop::net::Session* session = nullptr;
    uint64_t epoch = 0, departure = 0, stops = 0;
    uint32_t world = 0, serial = 0, occupant = 0;
    uint8_t slot = 0;
    bool Current(bool requireReady = true) const {
        return session && slot > 0 && slot < coop::net::kMaxPeers &&
            session == g_session.load() && epoch == g_epoch &&
            world == ue_wrap::world_identity::Generation() &&
            serial == coop::net::session_serial::Current() && stops == session->StopCount() &&
            session->connected() && session->role() == coop::net::Role::Host &&
            occupant && occupant == session->peerGenerationForSlot(slot) &&
            departure == g_peerEpoch[slot] && session->IsSlotConnected(slot) &&
            (!requireReady || session->IsSlotWorldReady(slot));
    }
};
Scope Capture(coop::net::Session* session, uint8_t slot) {
    if (!session || slot == 0 || slot >= coop::net::kMaxPeers) return {};
    return {session, g_epoch, g_peerEpoch[slot], session->StopCount(),
        ue_wrap::world_identity::Generation(), coop::net::session_serial::Current(),
        session->peerGenerationForSlot(slot), slot};
}
struct TowerScope {
    Scope source;
    ue_wrap::CachedObjRef actor, cls;
    TowerScope(const Scope& owner, void* tower) : source(owner) {
        if (!source.Current() || !tower) return;
        actor.Set(tower);
        if (actor.Get()) cls.Set(R::ClassOf(tower));
    }
    bool Current() const {
        return source.Current() && actor.Get() && cls.Get() && R::ClassOf(actor.Get()) == cls.Get();
    }
};
struct BusyScope {
    bool& flag;
    explicit BusyScope(bool& value) : flag(value) { flag = true; }
    ~BusyScope() { flag = false; }
};
struct ExecutionScope {
    explicit ExecutionScope(uint8_t slot, uint16_t seq = 0) {
        g_hostExecuting = true; g_activeSlot = slot; g_activeSeq = seq;
    }
    ~ExecutionScope() { g_hostExecuting = false; g_activeSlot = 0; g_activeSeq = 0; }
};

coop::net::Session* Connected() {
    auto* s = g_session.load(std::memory_order_acquire);
    return (s && s->connected()) ? s : nullptr;
}

bool IsClient(coop::net::Session* s) { return s && s->role() == coop::net::Role::Client; }

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// `a` is later than `b` in a 16-bit sequence that wraps.
bool SeqAfter(uint16_t a, uint16_t b) { return static_cast<int16_t>(static_cast<uint16_t>(a - b)) > 0; }

bool IsClaim(uint8_t op) { return op == coop::net::kFuseTowerOpPull || op == coop::net::kFuseTowerOpInsert; }

const char* OpName(uint8_t op) {
    switch (op) {
    case coop::net::kFuseTowerOpButton:  return "button press";
    case coop::net::kFuseTowerOpLever:   return "lever";
    case coop::net::kFuseTowerOpRetract: return "panel retract";
    case coop::net::kFuseTowerOpReset:   return "reset lever";
    case coop::net::kFuseTowerOpPull:    return "fuse pull";
    case coop::net::kFuseTowerOpInsert:  return "fuse insert";
    default:                              return "?";
    }
}

// The tower kind of an op, which HostOffer has admitted.
FK::Kind KindOf(const FuseTowerPayload& p) { return static_cast<FK::Kind>(p.kind); }

uint64_t g_rollsRefused = 0;
bool     g_shortWarned = false;

// ---- the host ------------------------------------------------------------------------------------------------

coop::roster_ledger::PerSlotState<uint16_t> g_ack;
// A slot's refused claim seqs, oldest first. The rows carry one refused seq for a slot, so each is told in an answer
// of its own and popped once that answer's send was taken (RefusedTold). Its cap, the op queue's, holds every claim a
// client keeps unanswered.
constexpr size_t kRefusedCap = kMaxQueued + 1;
static_assert(kRefusedCap >= kMaxUnansweredClaims, "the refused queue holds every claim a client keeps unanswered");
struct RefusedFifo {
    uint16_t seq[kRefusedCap] = {};
    uint8_t  head = 0, count = 0;
    uint16_t At(size_t i) const { return seq[(head + i) % kRefusedCap]; }
};
coop::roster_ledger::PerSlotState<RefusedFifo> g_refused;
coop::roster_ledger::PerSlotState<bool> g_overfullWarned;  // a sender past the claim bound, said once per occupant
struct Waiting { FuseTowerPayload p; uint64_t arrivedMs; Scope source; };
std::deque<Waiting> g_waiting[coop::net::kMaxPeers];
std::deque<Waiting> g_rejections[coop::net::kMaxPeers];
coop::net::IntentBucket g_rate[coop::net::kMaxPeers];
// A spend at the tower (kind, id); tower -1 is an empty place.
struct Spend {
    int32_t tower = -1;
    FK::Kind kind = FK::Kind::Coordinate;
    uint64_t ms = 0, receipt = 0;
    Scope source;
    ue_wrap::CachedObjRef actor, cls;
    bool reserved = false;
};
uint64_t g_spendReceipt = 0;
struct Spends { Spend ring[kSpendsPerSlot]; uint8_t next = 0; };
coop::roster_ledger::PerSlotState<Spends> g_spends;
uint64_t g_taken = 0, g_refusedCount = 0, g_refunds = 0, g_queuesFull = 0, g_outOfReach = 0, g_staleSeqs = 0;
bool BindPeer(coop::net::Session* s, uint8_t slot);

// A client's destroy this host applies: a good fuse its sender spent within reach of a tower's fuses is a spend
// there, judged from the host's puppet of the sender with the lane's own reach.
void OnDestroyHeard(int senderSlot, void* actor) {
    auto* s = Connected();
    if (!s || s->role() != coop::net::Role::Host) return;
    if (senderSlot <= 0 || senderSlot >= coop::net::kMaxPeers) return;
    const auto sender = static_cast<uint8_t>(senderSlot);
    Install(s);
    if (!BindPeer(s, sender)) return;
    const Scope source = Capture(s, sender);
    if (!source.Current()) return;
    ue_wrap::CachedObjRef fuse; fuse.Set(actor);
    if (!fuse.Get() || !FK::IsFuse(actor) || !source.Current() || !fuse.Get()) return;
    FK::Tower towers[coop::net::kFuseTowers];
    const int32_t n = FK::ReadAll(towers, coop::net::kFuseTowers);
    if (!source.Current() || !fuse.Get()) return;
    const EL::IntentTarget target = EL::IntentTarget::ForClientIntent(*s, sender, kTowerReachUU);
    for (int32_t i = 0; i < std::min<int32_t>(n, coop::net::kFuseTowers); ++i) {
        const TowerScope tower{source, towers[i].obj};
        if (!tower.Current() || !fuse.Get()) return;
        ue_wrap::FVector at{};
        const bool located = FK::PartLocation(towers[i].kind, towers[i].obj, coop::net::kFuseTowerOpPull, 0, at);
        if (!tower.Current() || !fuse.Get()) return;
        if (!located) continue;
        const EL::IntentSubject reach = target.AuthorizeSegment(at, at);
        if (!tower.Current() || !fuse.Get()) return;
        if (!reach) continue;
        Spends& sp = g_spends[sender];
        int next = -1;
        for (int offset = 0; offset < kSpendsPerSlot; ++offset) {
            const int candidate = (sp.next + offset) % kSpendsPerSlot;
            if (sp.ring[candidate].reserved) continue;
            next = candidate; break;
        }
        if (next < 0) return;
        sp.ring[next] = {towers[i].id, towers[i].kind, NowMs(), ++g_spendReceipt,
            source, tower.actor, tower.cls};
        sp.next = static_cast<uint8_t>((next + 1) % kSpendsPerSlot);
        UE_LOGI("fuse_tower: slot %u spent a fuse at tower %d (%.0f uu, allowed %.0f)", sender, towers[i].id,
                reach.distUU, reach.reachUU);
        return;
    }
}

// Reserve the newest current spend by receipt; validation does not consume it.
uint64_t FindSpend(uint8_t slot, FK::Kind kind, int32_t id, void* tower) {
    if (!tower || slot == 0 || slot >= coop::net::kMaxPeers) return 0;
    uint64_t newest = 0;
    for (const Spend& e : g_spends[slot].ring)
        if (!e.reserved && e.tower == id && e.kind == kind && e.source.Current() && e.actor.Get() == tower &&
            e.cls.Get() && R::ClassOf(tower) == e.cls.Get() && e.receipt > newest) newest = e.receipt;
    return newest;
}
bool CommitSpend(uint8_t slot, uint64_t receipt) {
    if (!receipt || slot == 0 || slot >= coop::net::kMaxPeers) return false;
    for (Spend& e : g_spends[slot].ring)
        if (e.receipt == receipt && e.source.Current()) { e = Spend{}; return true; }
    return false;
}
bool TakeSpend(uint8_t slot, FK::Kind kind, int32_t id, void* tower) {
    return CommitSpend(slot, FindSpend(slot, kind, id, tower));
}
struct SpendLease {
    uint8_t slot;
    uint64_t receipt;
    SpendLease(uint8_t owner, uint64_t value) : slot(owner), receipt(value) {
        for (Spend& e : g_spends[slot].ring) if (e.receipt == receipt && receipt) e.reserved = true;
    }
    SpendLease(const SpendLease&) = delete;
    ~SpendLease() {
        for (Spend& e : g_spends[slot].ring) if (e.receipt == receipt && receipt) e.reserved = false;
    }
};

struct RefundDebt {
    Scope source;
    FK::Kind kind;
    ue_wrap::CachedObjRef tower,cls;
    uint16_t seq=0;
    bool attempted=false;
    uint64_t retryAt=0;
};
std::vector<std::shared_ptr<RefundDebt>> g_refundDebt;
bool g_refunding=false;
bool RefundPending(uint8_t slot,uint16_t seq) {
    for(const auto& debt:g_refundDebt)
        if(debt->source.slot==slot && debt->seq==seq && debt->source.Current(false))return true;
    return false;
}
void DrainRefunds() {
    if(g_refunding)return;
    BusyScope busy(g_refunding);
    const auto pending=g_refundDebt;
    size_t budget=8;
    for(const auto& debt:pending){
        const auto erase=[&]{const auto found=std::find(g_refundDebt.begin(),g_refundDebt.end(),debt);
            if(found!=g_refundDebt.end())g_refundDebt.erase(found);};
        if(!debt->source.Current(false)){erase();continue;}
        if(debt->attempted || !debt->source.session->IsSlotWorldReady(debt->source.slot) || NowMs()<debt->retryAt)continue;
        if(!budget)break;
        --budget;
        debt->retryAt=NowMs()+250;
        ue_wrap::FVector at{};
        auto* puppet=coop::players::Registry::Get().Puppet(debt->source.slot);
        void* body=puppet?puppet->GetActor():nullptr;
        ue_wrap::CachedObjRef bodyRef;bodyRef.Set(body);
        bool placed=bodyRef.Get() && ue_wrap::engine::TryGetActorLocation(body,at);
        if(!debt->source.Current(false))continue;
        if(placed && (!bodyRef.Get() || coop::players::Registry::Get().Puppet(debt->source.slot)!=puppet || puppet->GetActor()!=body))placed=false;
        if(!placed && debt->tower.Get() && debt->cls.Get() && R::ClassOf(debt->tower.Raw())==debt->cls.Raw()){
            placed=FK::PartLocation(debt->kind,debt->tower.Raw(),coop::net::kFuseTowerOpPull,0,at);
            if(!debt->tower.Get()||!debt->cls.Get())placed=false;
            at.Z+=80.f;
        }
        if(!placed || !debt->source.Current(false) || !std::isfinite(at.X) || !std::isfinite(at.Y) || !std::isfinite(at.Z))continue;
        debt->attempted=true;
        bool attempted=false;
        void* fuse=FK::SpawnFuseTracked(at,attempted,[source=debt->source]{return source.Current(false);});
        if(!debt->source.Current(false))continue;
        debt->attempted=attempted;
        if(fuse && R::IsLive(fuse)){
            ++g_refunds;erase();coop::fuse_tower_rows::HostAnswerOwed(debt->source.slot);
        }
    }
}
void Refund(uint8_t slot, FK::Kind kind, void* tower, const std::function<bool()>& current) {
    if (!current() || RefundPending(slot,g_activeSeq)) return;
    const auto& refused=g_refused[slot];bool queued=false;
    for(size_t i=0;i<refused.count;++i)if(refused.At(i)==g_activeSeq)queued=true;
    if(!queued)return;
    auto debt=std::make_shared<RefundDebt>();debt->source=Capture(Connected(),slot);debt->kind=kind;debt->seq=g_activeSeq;
    debt->tower.Set(tower);if(debt->tower.Get())debt->cls.Set(R::ClassOf(tower));
    g_refundDebt.push_back(debt);
    DrainRefunds();
}

// The kinds of fault in an op the host drops or refuses: each is warned once per slot, however many ops carry it
// (the totals are in the session-end line). The first five are the receive boundary's and no client sends them.
enum class Fault : uint8_t { Op, Kind, OpKind, Stale, Dead, NoTower, Button, Fuse, NoPart, Reach, Unread, Rule, NoUse,
                             Count };
static_assert(static_cast<int>(Fault::Count) <= 16, "a slot's faults seen are one 16-bit mask");
coop::roster_ledger::PerSlotState<uint16_t> g_faultsSeen;

bool FirstOfFault(Fault f, uint8_t slot) {
    uint16_t& seen = g_faultsSeen[slot];
    const auto bit = static_cast<uint16_t>(1u << static_cast<int>(f));
    if (seen & bit) return false;
    seen = static_cast<uint16_t>(seen | bit);
    return true;
}

bool IsUse(uint8_t op) {
    return op == coop::net::kFuseTowerOpButton || op == coop::net::kFuseTowerOpLever ||
           op == coop::net::kFuseTowerOpRetract || op == coop::net::kFuseTowerOpReset;
}

// A claim refused to `slot`: its seq joins the queue its answers drain. An honest client never has more claims
// unanswered than the queue holds (its own gate stops at kMaxUnansweredClaims); a full queue means a sender past
// that bound, whose further refusals are not queued, so the queue never overruns.
void PushRefused(uint8_t slot, uint16_t seq) {
    RefusedFifo& f = g_refused[slot];
    if (f.count >= kRefusedCap) {
        if (!g_overfullWarned[slot]) {
            g_overfullWarned[slot] = true;
            UE_LOGW("fuse_tower: slot %u has more refused claims unanswered than any client keeps -- the rest go "
                    "unanswered", slot);
        }
        return;
    }
    f.seq[(f.head + f.count) % kRefusedCap] = seq;
    ++f.count;
}

// A refused op: counted, warned once per kind of fault per slot, a claim queued for its answer, a spent fuse given
// back, and the answer owed to the sender alone (the rows carry no news for the others).
void Refused(uint8_t sender, const FuseTowerPayload& p, const char* refusal, Fault why,
             const EL::IntentSubject& reach, bool spent, void* tower,
             const std::function<bool()>& current) {
    if (!current()) return;
    ++g_refusedCount;
    if (why == Fault::Reach) ++g_outOfReach;
    if (FirstOfFault(why, sender))
        UE_LOGW("fuse_tower: host refused slot %u's %s of tower %d (seq %u) -- %s (%.0f uu, allowed %.0f); "
                "the rest of this kind from this slot are counted, not said", sender, OpName(p.op), p.tower, p.seq,
                refusal, reach.distUU, reach.reachUU);
    if (IsClaim(p.op)) PushRefused(sender, p.seq);
    if (!current()) return;
    if (spent) Refund(sender, KindOf(p), tower, current);
    if (!current()) return;
    coop::fuse_tower_rows::HostAnswerOwed(sender);
}

enum class Take : uint8_t { Done, Wait, Cancelled };

Take HostTakeOp(coop::net::Session* s, const Waiting& wt, uint8_t sender) {
    const Waiting work = wt;
    const Scope source = work.source;
    if (s != source.session || sender != source.slot || !source.Current()) return Take::Cancelled;
    const FuseTowerPayload& p = work.p;
    const uint64_t now = NowMs();
    const bool waited = now >= work.arrivedMs && now - work.arrivedMs >= kWaitMs;
    const FK::Kind kind = KindOf(p);
    const bool resolved = FK::Resolved(kind);
    if (!source.Current()) return Take::Cancelled;
    if (!resolved && !waited) return Take::Wait;
    if (!SeqAfter(p.seq, g_ack[sender])) {
        ++g_staleSeqs;
        if (FirstOfFault(Fault::Stale, sender))
            UE_LOGW("fuse_tower: host dropped slot %u's %s seq %u (last taken %u); the rest of this kind from this "
                    "slot are counted, not said", sender, OpName(p.op), p.seq, g_ack[sender]);
        return Take::Done;
    }
    // A dead sender's act is refused as the other intent lanes refuse it (coop/player/players_registry LifeOf).
    const bool dead = coop::players::Registry::Get().LifeOf(sender) == coop::players::PuppetLife::Dead;
    void* tower = FK::Find(kind, p.tower);
    if (!source.Current()) return Take::Cancelled;
    const TowerScope binding{source, tower};
    const auto current = [&] { return source.Current() && (!tower || binding.Current()); };
    if (!current()) return Take::Cancelled;
    ue_wrap::FVector at{};
    const bool located = tower && FK::PartLocation(kind, tower, p.op, p.index, at);
    if (!current()) return Take::Cancelled;
    EL::IntentSubject reach{};
    if (located) reach = EL::IntentTarget::ForClientIntent(*s, sender, kTowerReachUU).AuthorizeSegment(at, at);
    if (!current()) return Take::Cancelled;
    if (located && reach.outcome == EL::IntentOutcome::NoBody && !waited) return Take::Wait;
    // Above the two easiest difficulties an insert rests on the fuse its own game spent, which the host saw
    // destroyed beside this tower. Reserve it until validation finishes so a
    // cancelled continuation cannot consume it. None yet waits for the destroy.
    // On the two easiest the game spends none and the host runs the insert itself.
    const bool easyInsert = p.op == coop::net::kFuseTowerOpInsert && FK::EasyInsert();
    if (!current()) return Take::Cancelled;
    const bool needSpend = located && p.op == coop::net::kFuseTowerOpInsert && !easyInsert;
    SpendLease reserved(sender, needSpend ? FindSpend(sender, kind, p.tower, tower) : 0);
    const uint64_t spend = reserved.receipt;
    if (!current()) return Take::Cancelled;
    if (needSpend && !spend && !waited && !dead) return Take::Wait;
    FK::View st;
    const bool viewed = tower && FK::ReadView(kind, tower, st);
    if (!current()) return Take::Cancelled;
    // The first reason found, in this order: the sender, the tower, the part, the reach, then the tower's own gates
    // on its copy. A button or a fuse the copy does not have is a fault of its own kind.
    const char* refusal = nullptr;
    Fault why = Fault::Rule;
    auto refuse = [&](const char* reason, Fault f) {
        if (!refusal) { refusal = reason; why = f; }
    };
    if (dead) refuse("its sender is dead", Fault::Dead);
    if (!tower) refuse("no tower of that id", Fault::NoTower);
    if (viewed && p.op == coop::net::kFuseTowerOpButton && p.index >= st.lightCount)
        refuse("that button is outside the puzzle", Fault::Button);
    if (viewed && IsClaim(p.op) && p.index >= st.fuseCount)
        refuse("that fuse is outside the tower's fuses", Fault::Fuse);
    if (!located) refuse("that part is not there", Fault::NoPart);
    if (!reach) refuse(EL::OutcomeName(reach.outcome), Fault::Reach);
    if (!viewed) refuse("the tower does not read", Fault::Unread);
    if (!refusal && p.op == coop::net::kFuseTowerOpPull) {
        refuse(FK::CanPull(kind, tower, st, p.index), Fault::Rule);
    } else if (!refusal && p.op == coop::net::kFuseTowerOpInsert) {
        if (needSpend && !spend) refuse("no fuse of the inserter's was spent at it", Fault::Rule);
        else refuse(FK::CanInsert(kind, tower, st, p.index), Fault::Rule);
    } else if (!refusal && IsUse(p.op)) {
        refuse(FK::CanUse(kind, tower, p.op), Fault::Rule);
    }
    if (!current()) return Take::Cancelled;
    const bool spent = spend && CommitSpend(sender, spend);
    if (spend && !spent) return Take::Wait;
    g_ack[sender] = p.seq;
    if (refusal) {
        Refused(sender, p, refusal, why, reach, spent, tower, current);
        return current() ? Take::Done : Take::Cancelled;
    }
    coop::RemotePlayer* rp = coop::players::Registry::Get().Puppet(sender);
    void* puppet = rp ? rp->GetActor() : nullptr;
    bool committed=true;
    switch (p.op) {
    case coop::net::kFuseTowerOpButton:
    case coop::net::kFuseTowerOpLever:
    case coop::net::kFuseTowerOpRetract:
    case coop::net::kFuseTowerOpReset:
        // The tower's own use, its gates checked on the host's copy above; one that did not run is no taken op.
        {
        const bool used = FK::Use(kind, tower, puppet, p.op, p.index);
        if (!current()) return Take::Cancelled;
        if (!used) {
            Refused(sender, p, "the tower's use did not run", Fault::NoUse, reach, false, tower, current);
            return current() ? Take::Done : Take::Cancelled;
        }
        }
        break;
    case coop::net::kFuseTowerOpPull:
        committed=FK::TakePull(kind, tower, p.index, current);
        break;
    case coop::net::kFuseTowerOpInsert:
        if (easyInsert) FK::TakeEasyInsert(kind, tower, p.index, current);
        else committed=FK::TakeInsert(kind, tower, p.index, current);
        break;
    default: break;
    }
    if (!current()) return Take::Cancelled;
    if(p.op==coop::net::kFuseTowerOpInsert && easyInsert){
        FK::View after;
        const bool read=FK::ReadView(kind,tower,after);
        if(!current())return Take::Cancelled;
        committed=read&&!after.broken;
    }
    if(!committed){
        Refused(sender,p,"the fuse output was not confirmed",Fault::NoUse,reach,spent,tower,current);
        return current()?Take::Done:Take::Cancelled;
    }
    ++g_taken;
    UE_LOGI("fuse_tower: host took slot %u's %s of tower %d (index %u, seq %u)", sender, OpName(p.op), p.tower,
            p.index, p.seq);
    coop::fuse_tower_rows::HostSendNow();
    return current() ? Take::Done : Take::Cancelled;
}

bool BindPeer(coop::net::Session* s, uint8_t slot) {
    if (!s || slot == 0 || slot >= coop::net::kMaxPeers) return false;
    const auto generation = s->peerGenerationForSlot(slot);
    if (g_occupants[slot] != generation) {
        OnPeerLeft(slot);
        g_occupants[slot] = generation;
    }
    if (!Capture(s, slot).Current(false)) {
        if (g_occupants[slot]) OnPeerLeft(slot);
        return false;
    }
    return s->IsSlotWorldReady(slot);
}

Take ExecuteOwned(coop::net::Session* s, const Waiting& work, uint8_t slot) {
    if (g_hostExecuting) return Take::Wait;
    ExecutionScope busy(slot, work.p.seq);
    return HostTakeOp(s, work, slot);
}

void DrainRejections(coop::net::Session* s, uint8_t slot, size_t& budget) {
    if (g_hostExecuting) return;
    ExecutionScope busy(slot);
    while (budget && !g_rejections[slot].empty()) {
        Waiting work = std::move(g_rejections[slot].front());
        g_rejections[slot].pop_front();
        --budget;
        const Scope source = work.source;
        if (s != source.session || !source.Current()) return;
        const auto& p = work.p;
        g_activeSeq = p.seq;
        if (!SeqAfter(p.seq, g_ack[slot])) continue;
        void* tower = FK::Find(KindOf(p), p.tower);
        if (!source.Current()) return;
        const TowerScope binding{source, tower};
        const auto current = [&] { return source.Current() && (!tower || binding.Current()); };
        if (!current()) return;
        g_ack[slot] = p.seq;
        if (IsClaim(p.op)) PushRefused(slot, p.seq);
        ++g_refusedCount;
        if (p.op == coop::net::kFuseTowerOpInsert && tower &&
            TakeSpend(slot, KindOf(p), p.tower, tower)) Refund(slot, KindOf(p), tower, current);
        if (!current()) return;
        coop::fuse_tower_rows::HostAnswerOwed(slot);
        if (!current()) return;
    }
}

void HostOffer(coop::net::Session* s, const FuseTowerPayload& p, uint8_t sender) {
    if (sender == 0 || sender >= coop::net::kMaxPeers || !p.seq || p.op == coop::net::kFuseTowerOpRows) return;
    if (!BindPeer(s, sender)) return;
    // The receive boundary: an op no client sends is dropped here, with one warning for each kind of fault.
    const char* fault = nullptr;
    Fault which = Fault::Op;
    if (p.op > coop::net::kFuseTowerOpReset) {
        fault = "an op outside 1..6";
    } else if (p.kind >= coop::net::kFuseTowerKinds) {
        which = Fault::Kind;
        fault = "a tower kind that is neither tower";
    } else if ((p.op == coop::net::kFuseTowerOpRetract && p.kind == coop::net::kFuseTowerKindRadio) ||
               (p.op == coop::net::kFuseTowerOpReset && p.kind == coop::net::kFuseTowerKindCoordinate)) {
        which = Fault::OpKind;
        fault = "an op its tower kind does not have (the panel retract is the coordinate towers', the reset lever "
                "the radio tower's)";
    }
    if (fault) {
        if (FirstOfFault(which, sender)) UE_LOGW("fuse_tower: host dropped slot %u's op %u -- %s", sender, p.op, fault);
        return;
    }
    if (!SeqAfter(p.seq, g_ack[sender])) return;
    if (g_hostExecuting && g_activeSlot == sender && g_activeSeq == p.seq) return;
    for (const auto& work : g_waiting[sender]) if (work.p.seq == p.seq) return;
    for (const auto& work : g_rejections[sender]) if (work.p.seq == p.seq) return;
    const Waiting w{p, NowMs(), Capture(s, sender)};
    if (g_waiting[sender].empty() && g_rejections[sender].empty() && !g_hostExecuting &&
        g_rate[sender].Take(kOpBudget, NowMs())) {
        const auto taken = ExecuteOwned(s, w, sender);
        if (!w.source.Current() || taken != Take::Wait) return;
        g_rate[sender].Refund(kOpBudget);
        // A nested offer may have arrived while this operation waited.
        if (g_rejections[sender].empty()) g_waiting[sender].push_front(w);
        else g_rejections[sender].push_front(w);
    } else {
        const size_t limit = kMaxQueued - (g_hostExecuting && g_activeSlot == sender ? 1 : 0);
        if (g_waiting[sender].size() >= limit && !g_rejections[sender].empty()) return;
        g_waiting[sender].push_back(w);
    }
    const size_t limit = kMaxQueued - (g_hostExecuting && g_activeSlot == sender ? 1 : 0);
    if (g_waiting[sender].size() > limit && g_rejections[sender].empty()) {
        g_rejections[sender].swap(g_waiting[sender]);
        ++g_queuesFull;
        size_t budget = kMaxQueued + 1;
        DrainRejections(s, sender, budget);
    }
}

void HostDrainWaiting(coop::net::Session* s) {
    if (g_draining || g_hostExecuting || !s || s != g_session.load()) return;
    BusyScope drain(g_draining);
    const auto epoch = g_epoch;
    size_t budget = kMaxQueued + 1;
    const uint8_t first = g_drainSlot;
    for (uint8_t offset = 0; offset < coop::net::kMaxPeers - 1 && budget; ++offset) {
        if (epoch != g_epoch || s != g_session.load() || !s->connected()) return;
        const uint8_t slot = uint8_t(1 + (first - 1 + offset) % (coop::net::kMaxPeers - 1));
        g_drainSlot = uint8_t(1 + slot % (coop::net::kMaxPeers - 1));
        if (!BindPeer(s, slot)) continue;
        DrainRejections(s, slot, budget);
        if (epoch != g_epoch || !Capture(s, slot).Current()) return;
        while (budget && !g_waiting[slot].empty() && g_rejections[slot].empty() &&
               g_rate[slot].Take(kOpBudget, NowMs())) {
            Waiting work = std::move(g_waiting[slot].front());
            g_waiting[slot].pop_front();
            --budget;
            const auto taken = ExecuteOwned(s, work, slot);
            if (!work.source.Current()) return;
            if (taken == Take::Wait) {
                g_rate[slot].Refund(kOpBudget);
                if (g_rejections[slot].empty()) g_waiting[slot].push_front(std::move(work));
                else g_rejections[slot].push_front(std::move(work));
                break;
            }
        }
    }
}

// ---- a client ------------------------------------------------------------------------------------------------

// My claims the host has not answered, oldest first.
struct Claim { uint16_t seq; uint8_t op; FK::Kind kind; int32_t tower; uint8_t slot; ue_wrap::CachedObjRef output,outputClass; };
std::vector<Claim> g_claims;
std::vector<Claim> g_unknownPulls;
struct ClientCommand {
    FuseTowerPayload p{};
    coop::net::Session* session=nullptr;
    ue_wrap::CachedObjRef actor,cls;
    uint64_t epoch=g_epoch,stops=0,at=0;
    uint32_t world=g_world,serial=g_serial,host=0;
    bool physical=false,attempted=false;
    bool CurrentContext()const{
        return session && session==g_session.load() && epoch==g_epoch &&
            world==ue_wrap::world_identity::Generation() && serial==coop::net::session_serial::Current() &&
            stops==session->StopCount() && session->connected() && IsClient(session) &&
            host==session->peerGenerationForSlot(0);
    }
    bool Current()const{return CurrentContext()&&actor.Get()&&cls.Get()&&R::ClassOf(actor.Raw())==cls.Raw();}
};
std::vector<std::shared_ptr<ClientCommand>> g_clientCommands;
ClientCommand ClientContext(){
    ClientCommand scope;scope.session=Connected();
    if(scope.session){scope.stops=scope.session->StopCount();scope.host=scope.session->peerGenerationForSlot(0);}
    return scope;
}
bool g_sendingClient=false;
size_t g_postingClaims=0;
size_t g_postingInserts=0;
void DrainClientCommands(){
    if(g_sendingClient)return;
    BusyScope busy(g_sendingClient);
    const auto pending=g_clientCommands;
    for(const auto& command:pending){
        const auto found=std::find(g_clientCommands.begin(),g_clientCommands.end(),command);
        if(found==g_clientCommands.end())continue;
        if(!command->CurrentContext()){g_clientCommands.erase(found);continue;}
        if(!command->Current())return;
        const auto now=NowMs();
        if(command->attempted && (now<command->at || now-command->at<500))return;
        command->attempted=true;command->at=now;
        const auto packet=command->p;
        command->session->SendReliableToSlot(0,coop::net::ReliableKind::FuseTowerState,&packet,sizeof packet);
        return;
    }
}
uint16_t g_seq = 0;
uint16_t g_lastAck = 0;
uint64_t g_sent = 0, g_reaped = 0, g_claimsCapped = 0;
struct ClientCapture {
    ue_wrap::CachedObjRef actor, cls, function, functionClass;
    ue_wrap::CachedObjRef owner,ownerClass,heldBefore;
    bool handKnown=false;
    coop::net::Session* session = nullptr;
    uint64_t epoch = g_epoch, stops = 0, invocation = 0;
    uint32_t world = g_world, serial = g_serial, host = 0;
    void* stack = nullptr;
    uint8_t* locals = nullptr;
    int depth = 0;
    int32_t slot = -1, id = -1;
    FK::Kind kind = FK::Kind::Coordinate;
    uint8_t op = 0;
    bool Matches(const sg::Call& call) const {
        return actor.Raw() == call.object && function.Raw() == call.function &&
            invocation == call.invocation && stack == call.stack && depth == call.depth && locals == call.locals;
    }
    bool Current() const {
        return session && session == g_session.load() && epoch == g_epoch &&
            world == ue_wrap::world_identity::Generation() && serial == coop::net::session_serial::Current() &&
            stops == session->StopCount() && IsClient(session) && session->connected() &&
            host == session->peerGenerationForSlot(0) && actor.Get() && cls.Get() &&
            R::ClassOf(actor.Raw()) == cls.Raw() && function.Get() && functionClass.Get() &&
            R::ClassOf(function.Raw()) == functionClass.Raw() &&
            (!owner.Raw() || (owner.Get()&&ownerClass.Get()&&R::ClassOf(owner.Raw())==ownerClass.Raw()&&coop::players::Registry::Get().Local()==owner.Raw()));
    }
};
std::vector<ClientCapture> g_clientCaptures;
ClientCapture CaptureClient(const sg::Call& call, uint8_t op) {
    ClientCapture capture;
    capture.session = Connected();
    if (!capture.session) return capture;
    capture.stops = capture.session->StopCount();capture.host = capture.session->peerGenerationForSlot(0);
    capture.actor.Set(call.object);capture.cls.Set(R::ClassOf(call.object));
    capture.function.Set(call.function);capture.functionClass.Set(R::ClassOf(call.function));
    capture.invocation = call.invocation;capture.stack = call.stack;capture.locals = call.locals;
    capture.depth = call.depth;capture.op = op;
    return capture;
}
bool KeepCapture(const ClientCapture& capture) {
    g_clientCaptures.erase(std::remove_if(g_clientCaptures.begin(),g_clientCaptures.end(),
        [](const auto& row){return !row.Current();}),g_clientCaptures.end());
    if (!capture.Current() || (!capture.invocation && !capture.stack) || g_clientCaptures.size() >= 64) return false;
    g_clientCaptures.push_back(capture);return true;
}

// A pull or an insert whose fuse this client's own game moved is a claim the rows answer; an insert the gate
// cancelled (the two easiest difficulties) moved nothing here and claims nothing.
bool ClientSequenceRoom() {
    auto next=static_cast<uint16_t>(g_seq+1);if(!next)next=1;
    for(const auto& claim:g_claims)if(static_cast<uint16_t>(next-claim.seq)>32767)return false;
    for(const auto& claim:g_unknownPulls)if(static_cast<uint16_t>(next-claim.seq)>32767)return false;
    return true;
}
void ClientSend(coop::net::Session* s, uint8_t op, FK::Kind kind, int32_t tower, int32_t index, bool claim = true,void* actor=nullptr,void* output=nullptr) {
    if(!ClientSequenceRoom())return;
    const bool physical=claim&&IsClaim(op);
    if(!physical){size_t controls=0;for(const auto& command:g_clientCommands)if(!command->physical)++controls;
        if(controls+g_clientCaptures.size()+g_postingClaims>=kMaxQueued)return;}
    auto command=std::make_shared<ClientCommand>();command->session=s;
    if(!s)return;
    command->stops=s->StopCount();command->host=s->peerGenerationForSlot(0);command->physical=physical;
    command->actor.Set(actor?actor:FK::Find(kind,tower));
    if(command->actor.Get())command->cls.Set(R::ClassOf(command->actor.Raw()));
    if(!command->Current())return;
    FuseTowerPayload p{};
    p.op = op;
    p.kind = static_cast<uint8_t>(kind);
    p.tower = tower;
    p.index = static_cast<uint8_t>(std::max(index, 0));
    if (++g_seq == 0) ++g_seq;  // 0 is the rows' "no refusal", never a claim's seq
    p.seq = g_seq;
    if (physical) {
        Claim receipt{p.seq,op,kind,tower,p.index};receipt.output.Set(output);
        if(receipt.output.Get())receipt.outputClass.Set(R::ClassOf(output));
        g_claims.push_back(receipt);
    }
    command->p=p;g_clientCommands.push_back(command);
    DrainClientCommands();
    if(!command->CurrentContext())return;
    ++g_sent;
    UE_LOGI("fuse_tower: local %s of tower %d (index %d, seq %u) -- queued for host acknowledgement", OpName(op), tower, index,
            p.seq);
}

// A pull or an insert past the claims a client may keep unanswered is refused at its own gate: its act does not run
// and nothing moves. Said for the first few, then every hundredth.
bool ClaimsFull(bool spending = false) {
    if(spending){
        size_t inserts=g_postingInserts;
        for(const auto& claim:g_claims)if(claim.op==coop::net::kFuseTowerOpInsert)++inserts;
        for(const auto& capture:g_clientCaptures)if(capture.op==coop::net::kFuseTowerOpInsert)++inserts;
        if(inserts)return true;
    }
    if (g_claims.size()+g_unknownPulls.size()+g_clientCaptures.size()+g_postingClaims < kMaxUnansweredClaims && ClientSequenceRoom()) return false;
    if (++g_claimsCapped <= 3 || g_claimsCapped % 100 == 0)
        UE_LOGW("fuse_tower: %zu of my fuse claims are unanswered by the host -- this pull or insert is refused here "
                "(%llu so far)", g_claims.size(), static_cast<unsigned long long>(g_claimsCapped));
    return true;
}

// A refused pull: the fuse its own game put in my hand goes, when it is still there (the floppy crate's reap).
bool Reap(const Claim& c) {
    const auto context=ClientContext();
    if(!context.CurrentContext() || !c.output.Raw())return false;
    if(!c.output.Get())return true;
    if(!c.outputClass.Get() || R::ClassOf(c.output.Raw())!=c.outputClass.Raw() || !FK::IsPulledFuse(c.output.Raw()))return false;
    coop::prop_lifecycle::DestroyLocalProp(c.output.Raw(),/*deferred*/true);
    if(!context.CurrentContext())return false;
    ++g_reaped;return true;
}

bool ClientGatesOpen(const sg::Call& call) {
    return !IsClient(Connected()) || RedOpen() || coop::fuse_tower_rows::Applying() || !call.object ||
           !coop::net_pump::IsInAnnouncedWorld(call.object);
}

// ---- the watches ---------------------------------------------------------------------------------------------

// A client's own roll and its lever's judgement never run: the host's world rolls and judges every tower, and
// its rows run each change here. Every route: the fuckuper's timer, an explosion, a tower that loads broken.
sg::Verdict OnRollPre(const sg::Call&) {
    if (!IsClient(Connected()) || RedOpen()) return sg::Verdict::Run;
    if (++g_rollsRefused == 1)
        UE_LOGI("fuse_tower: this client refuses its towers' own rolls and lever judgements -- the host's towers "
                "are the author");
    return sg::Verdict::Cancel;
}

// A parameter's offset in a watched body, one slot per kind (each kind's body is its own function): a player who
// alternates between the towers never walks the parameter chain again.
struct ParamSlot { ue_wrap::CachedObjRef fn, cls; int32_t off = -1; };
ParamSlot g_actionCache[FK::kKinds];
ParamSlot g_holdCache[FK::kKinds];

int32_t ParamOffsetOf(ParamSlot (&cache)[FK::kKinds], void* fn, const wchar_t* name) {
    if (!fn || !R::IsLive(fn)) return -1;
    for (ParamSlot& c : cache)
        if (c.fn.Get() == fn && c.cls.Get() && R::ClassOf(fn) == c.cls.Get()) return c.off;
    for (ParamSlot& c : cache)
        if (!c.fn.Get() || !c.cls.Get() || R::ClassOf(c.fn.Raw()) != c.cls.Raw()) {
            c.fn.Set(fn);c.cls.Set(R::ClassOf(fn));c.off = R::FindParamOffset(fn, name);
            return c.off;
        }
    return R::FindParamOffset(fn, name);
}
bool ParamFits(const sg::Call& call, int32_t offset, size_t bytes) {
    if (!call.locals || !call.function || !R::IsLive(call.function) || offset < 0) return false;
    const int32_t size = R::FunctionFrameSize(call.function);
    return size >= 0 && size_t(offset) <= size_t(size) && bytes <= size_t(size) - size_t(offset);
}

// A client's use: a press of a button, a lever or the retract goes to the host, which runs it; a pull runs here
// and its move is claimed as the use ends.
sg::Verdict OnUsePre(const sg::Call& call) {
    if (ClientGatesOpen(call)) return sg::Verdict::Run;
    Install(Connected());
    if(call.depth>0)g_clientCaptures.erase(std::remove_if(g_clientCaptures.begin(),g_clientCaptures.end(),
        [&](const auto& capture){return capture.depth>=call.depth;}),g_clientCaptures.end());
    const int32_t actionOff = ParamOffsetOf(g_actionCache, call.function, L"action");
    if (!ParamFits(call,actionOff,sizeof(uint8_t))) return sg::Verdict::Cancel;
    if (call.locals[actionOff] != 4) return sg::Verdict::Run;
    auto capture = CaptureClient(call,coop::net::kFuseTowerOpPull);
    FK::Kind kind;
    int32_t id;
    uint8_t op = 0;
    int32_t index = -1;
    if (!capture.Current() || !FK::Identify(call.object, kind, id) || !capture.Current()) return sg::Verdict::Cancel;
    const auto look = FK::ReadUse(kind, call.object, op, index);
    if (!capture.Current()) return sg::Verdict::Cancel;
    switch (look) {
    case FK::Look::Send:
        ClientSend(Connected(), op, kind, id, index,true,call.object);
        return sg::Verdict::Cancel;
    case FK::Look::Pull:
        if (ClaimsFull()) return sg::Verdict::Cancel;
        capture.owner.Set(coop::players::Registry::Get().Local());
        if(capture.owner.Get())capture.ownerClass.Set(R::ClassOf(capture.owner.Raw()));
        if(capture.owner.Get()){
            ue_wrap::engine::MainPlayerGrabState before{};
            capture.handKnown=ue_wrap::engine::ReadMainPlayerGrabState(capture.owner.Raw(),before);
            if(capture.handKnown)capture.heldBefore.Set(before.grabbingActor?before.grabbingActor:before.holdingActor);
        }
        if(!capture.Current())return sg::Verdict::Cancel;
        capture.kind=kind;capture.id=id;capture.slot=index;
        return index >= 0 && KeepCapture(capture) ? sg::Verdict::Run : sg::Verdict::Cancel;
    default:
        return sg::Verdict::Run;
    }
}

void OnUsePost(const sg::Call& call) {
    const auto found=std::find_if(g_clientCaptures.begin(),g_clientCaptures.end(),
        [&](const auto& row){return row.op==coop::net::kFuseTowerOpPull && row.Matches(call);});
    if (found==g_clientCaptures.end()) return;
    const auto capture=*found;g_clientCaptures.erase(found);
    struct Posting{Posting(){++g_postingClaims;}~Posting(){--g_postingClaims;}}posting;
    FK::Kind kind;int32_t id;FK::View st;
    if (!capture.Current() || !FK::Identify(call.object,kind,id) || !capture.Current() ||
        kind!=capture.kind || id!=capture.id || !FK::ReadView(kind,call.object,st) || !capture.Current() ||
        capture.slot<0 || capture.slot>=st.fuseCount || capture.slot>=int32_t(sizeof st.fuses) || st.fuses[capture.slot]!=0) return;
    void* output=nullptr;
    if(capture.handKnown && capture.owner.Get()){
        ue_wrap::engine::MainPlayerGrabState after{};
        const bool read=ue_wrap::engine::ReadMainPlayerGrabState(capture.owner.Raw(),after);
        if(!capture.Current())return;
        void* held=read?(after.grabbingActor?after.grabbingActor:after.holdingActor):nullptr;
        if(held && held!=capture.heldBefore.Get() && FK::IsPulledFuse(held))output=held;
    }
    ClientSend(capture.session,capture.op,kind,id,capture.slot,true,call.object,output);
}

// A client's insert above the two easiest difficulties runs here, and its move is claimed as it ends; on those two
// it keeps the fuse and solves, so it is refused here and sent to the host like a press.
sg::Verdict OnInsertPre(const sg::Call& call) {
    if (ClientGatesOpen(call)) return sg::Verdict::Run;
    Install(Connected());
    if(call.depth>0)g_clientCaptures.erase(std::remove_if(g_clientCaptures.begin(),g_clientCaptures.end(),
        [&](const auto& capture){return capture.depth>=call.depth;}),g_clientCaptures.end());
    const int32_t holdOff = ParamOffsetOf(g_holdCache, call.function, L"holdObject");
    if (!ParamFits(call,holdOff,sizeof(void*))) return sg::Verdict::Cancel;
    void* held = nullptr;std::memcpy(&held,call.locals+holdOff,sizeof held);
    auto capture = CaptureClient(call,coop::net::kFuseTowerOpInsert);
    FK::Kind kind;
    int32_t id;
    FK::View st;
    int32_t slot = -1;
    if(!capture.Current())return sg::Verdict::Cancel;
    if(!FK::IsFuse(held) || !FK::Identify(call.object,kind,id))return sg::Verdict::Run;
    const bool read=FK::ReadView(kind,call.object,st);
    if(!capture.Current()||!read)return sg::Verdict::Cancel;
    const bool looked=FK::ReadFuseLook(kind,call.object,slot);
    if(!capture.Current())return sg::Verdict::Cancel;
    if(!looked || slot<0 || slot>=st.fuseCount || slot>=int32_t(sizeof st.fuses) || st.fuses[slot]!=0)return sg::Verdict::Run;
    const bool easy=FK::EasyInsert();
    if (!capture.Current()) return sg::Verdict::Cancel;
    if (easy) {
        ClientSend(Connected(), coop::net::kFuseTowerOpInsert, kind, id, slot, /*claim*/false,call.object);
        return sg::Verdict::Cancel;
    }
    if (ClaimsFull(true)) return sg::Verdict::Cancel;
    capture.kind=kind;capture.id=id;capture.slot=slot;
    return KeepCapture(capture) ? sg::Verdict::Run : sg::Verdict::Cancel;
}

void OnInsertPost(const sg::Call& call) {
    const auto found=std::find_if(g_clientCaptures.begin(),g_clientCaptures.end(),
        [&](const auto& row){return row.op==coop::net::kFuseTowerOpInsert && row.Matches(call);});
    if (found==g_clientCaptures.end()) return;
    const auto capture=*found;g_clientCaptures.erase(found);
    struct Posting{Posting(){++g_postingClaims;++g_postingInserts;}~Posting(){--g_postingInserts;--g_postingClaims;}}posting;
    FK::Kind kind;int32_t id;FK::View st;
    if (!capture.Current() || !FK::Identify(call.object,kind,id) || !capture.Current() ||
        kind!=capture.kind || id!=capture.id || !FK::ReadView(kind,call.object,st) || !capture.Current() ||
        capture.slot<0 || capture.slot>=st.fuseCount || capture.slot>=int32_t(sizeof st.fuses) || st.fuses[capture.slot]!=1) return;
    ClientSend(capture.session,capture.op,kind,id,capture.slot,true,call.object);
}

// A tower generated its puzzle or its fuses (the radio tower's BeginPlay, a late construction): on a client the
// host's kept row for it applies again, since the generator wrote a fresh one over what the rows had brought.
void OnGenPost(const sg::Call& call) {
    if (!IsClient(Connected()) || RedOpen() || !call.object) return;
    FK::Kind kind;
    int32_t id;
    if (FK::Identify(call.object, kind, id))
        coop::fuse_tower_rows::ReapplyKept(static_cast<uint8_t>(kind), id);
}

// Each kind's bodies (coop/world/fuse_tower_kind), under one tag a kind and a role.
struct WatchDef {
    const ue_wrap::facts::Function* row;
    int tag;
    sg::PreFn pre;
    sg::PostFn post;
};
constexpr int kRoles = 6;
constexpr int kTags[FK::kKinds][kRoles] = {
    { 0x43545352 /*'CTSR'*/, 0x4354464E /*'CTFN'*/, 0x4354414F /*'CTAO'*/, 0x43545555 /*'CTUU'*/,
      0x43544750 /*'CTGP'*/, 0x43544746 /*'CTGF'*/ },
    { 0x52545352 /*'RTSR'*/, 0x5254464E /*'RTFN'*/, 0x5254414F /*'RTAO'*/, 0x52545555 /*'RTUU'*/,
      0x52544750 /*'RTGP'*/, 0x52544746 /*'RTGF'*/ },
};
WatchDef g_watches[FK::kKinds * kRoles];
int  g_watchCount = 0;  // the rows a kind has; the rest of its roles are null
bool g_registered = false;
bool g_settled = false;

void BuildWatches() {
    g_watchCount = 0;
    for (int k = 0; k < FK::kKinds; ++k) {
        const FK::WatchRows& r = FK::WatchesOf(static_cast<FK::Kind>(k));
        const WatchDef defs[kRoles] = {
            { r.roll,      kTags[k][0], &OnRollPre,   nullptr },
            { r.judge,     kTags[k][1], &OnRollPre,   nullptr },
            { r.use,       kTags[k][2], &OnUsePre,    &OnUsePost },
            { r.insert,    kTags[k][3], &OnInsertPre, &OnInsertPost },
            { r.genPuzzle, kTags[k][4], nullptr,      &OnGenPost },
            { r.genFuses,  kTags[k][5], nullptr,      &OnGenPost },
        };
        for (const WatchDef& d : defs)
            if (d.row) g_watches[g_watchCount++] = d;
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    const auto world = ue_wrap::world_identity::Generation();
    const auto serial = coop::net::session_serial::Current();
    if (g_session.load() != session || g_world != world || g_serial != serial ||
        (session && (g_boundStops!=session->StopCount() || g_boundRole!=session->role()))) OnDisconnect();
    g_world = world; g_serial = serial;
    g_boundStops=session?session->StopCount():0;g_boundRole=session?session->role():coop::net::Role::Host;
    g_session.store(session, std::memory_order_release);
    if (g_registered) return;
    g_registered = true;
    coop::remote_prop::AddDestroyListener(&OnDestroyHeard);
    BuildWatches();
    for (int i = 0; i < g_watchCount; ++i) {
        const WatchDef& w = g_watches[i];
        if (!sg::WatchName(*w.row, w.tag, w.pre, w.post))
            UE_LOGE("fuse_tower: the script-body gate refused the watch on %ls::%ls -- that act stays each peer's own",
                    w.row->Class(), w.row->Name());
    }
}

void Tick() {
    if (auto* s = Connected()) Install(s);
    if (auto* s = Connected(); s && s->role() == coop::net::Role::Host) HostDrainWaiting(s);
    DrainRefunds();
    DrainClientCommands();
    if (g_settled || !g_registered) return;
    sg::ResolvePendingNames();
    int live = 0, settled = 0;
    for (int i = 0; i < g_watchCount; ++i) {
        const WatchDef& w = g_watches[i];
        if (sg::NameWatchLive(*w.row, w.tag)) ++live;
        if (sg::NameWatchSettled(*w.row, w.tag)) ++settled;
    }
    if (live == g_watchCount) {
        g_settled = true;
        UE_LOGI("fuse_tower: the towers' gates are live (the roll, the lever's judgement, a use, an insert, the "
                "radio tower's generators)");
    } else if (settled == g_watchCount) {
        g_settled = true;
        UE_LOGE("fuse_tower: %d of %d tower gates are dead -- those acts stay each peer's own", g_watchCount - live,
                g_watchCount);
    }
}

void OnOp(const FuseTowerPayload& p, uint8_t sender) {
    auto* s = Connected();
    if (!s || s->role() != coop::net::Role::Host) return;
    Install(s);
    HostOffer(s, p, sender);
}

// A slot with refused claims queued is acknowledged only below its oldest: a claim from it on, refused or taken, is
// told by the answers owed that slot, so its client does not drop one as taken meanwhile. The answer to `answerSlot`
// alone carries that oldest refused seq, and acknowledges up to the one before the next; every other entry, and a
// broadcast (-1), carries none, so only a per-slot answer ever pops.
void FillAcks(FuseTowerPayload& p, int answerSlot) {
    for (int i = 0; i < coop::net::kMaxPeers; ++i) {
        const RefusedFifo& f = g_refused[i];
        if (i == answerSlot && f.count && !RefundPending(uint8_t(i),f.At(0))) {
            p.refused[i] = f.At(0);
            p.ack[i] = f.count > 1 ? static_cast<uint16_t>(f.At(1) - 1) : g_ack[i];
        } else {
            p.refused[i] = 0;
            p.ack[i] = f.count ? static_cast<uint16_t>(f.At(0) - 1) : g_ack[i];
        }
    }
}

void RefusedTold(uint8_t slot, uint16_t expectedSeq) {
    if (slot >= coop::net::kMaxPeers) return;
    RefusedFifo& f = g_refused[slot];
    if (!f.count || !expectedSeq || f.At(0) != expectedSeq) return;
    f.head = static_cast<uint8_t>((f.head + 1) % kRefusedCap);
    --f.count;
}

bool RefusedPending(uint8_t slot) { return slot < coop::net::kMaxPeers && g_refused[slot].count != 0; }

void OnAcks(uint16_t ack, uint16_t refused) {
    const auto scope=ClientContext();
    if(!scope.CurrentContext())return;
    g_lastAck = ack;
    const auto epoch=g_epoch;
    const auto claims=std::move(g_claims);g_claims.clear();
    std::vector<Claim> reaps;
    g_clientCommands.erase(std::remove_if(g_clientCommands.begin(),g_clientCommands.end(),
        [&](const auto& command){return !SeqAfter(command->p.seq,ack);}),g_clientCommands.end());
    for (const Claim& c : claims) {
        if (SeqAfter(c.seq, ack)) {
            g_claims.push_back(c);
            continue;
        }
        if (c.seq == refused) {
            if (c.op == coop::net::kFuseTowerOpPull) reaps.push_back(c);
            else UE_LOGW("fuse_tower: my insert into tower %d's slot %u was refused -- the host gives the fuse back",
                         c.tower, c.slot);
        }
    }
    for(const auto& claim:reaps){
        if(epoch!=g_epoch||!scope.CurrentContext())return;
        const bool retired=Reap(claim);
        if(!scope.CurrentContext())return;
        if(!retired)g_unknownPulls.push_back(claim);
    }
}

void Overlay(coop::net::FuseTowerRow& row) {
    for (const Claim& c : g_claims) {
        if (static_cast<uint8_t>(c.kind) != row.kind || c.tower != row.id || c.slot >= row.fuseCount) continue;
        if (c.op == coop::net::kFuseTowerOpPull) {
            row.fuses[c.slot] = 0;
        } else {
            row.fuses[c.slot] = 1;
        }
    }
}

void OnPeerLeft(uint8_t slot) {
    if (slot == 0 || slot >= coop::net::kMaxPeers) return;
    ++g_peerEpoch[slot];
    g_occupants[slot] = 0;
    g_waiting[slot].clear();
    g_rejections[slot].clear();
    g_rate[slot].Reset();
    g_ack[slot] = 0;
    g_refused[slot] = RefusedFifo{};
    g_overfullWarned[slot] = false;
    g_spends[slot] = Spends{};
    g_faultsSeen[slot] = 0;
}

uint64_t OpsSent() { return g_sent; }
size_t PendingClaims() { return g_claims.size(); }
uint64_t OpsRefused() { return g_refusedCount; }
uint64_t RollsRefused() { return g_rollsRefused; }
uint16_t LastSentSeq() { return g_seq; }
uint16_t LastAck() { return g_lastAck; }
uint64_t Reaped() { return g_reaped; }

bool RedOpen() {
    static const bool red = [] {
        const std::string v = coop::config::ResolveString(::coop::config_registry::rows::tower_drill);
        return v == "red" || v == "joinred" || v == "radiored" || v == "radiojoinred" || v == "radioresetred" ||
               v == "radiofusered";
    }();
    return red;
}

void OnShortPayload(size_t got) {
    if (g_shortWarned) return;
    g_shortWarned = true;
    UE_LOGW("fuse_tower: a FuseTowerState payload is too short (%zu < %zu) -- dropped; said once a session", got,
            sizeof(FuseTowerPayload));
}

void OnDisconnect() {
    ++g_epoch;
    g_session.store(nullptr, std::memory_order_release);
    if (g_sent || g_taken || g_refusedCount || g_rollsRefused)
        UE_LOGI("fuse_tower: session end -- ops sent %llu, taken %llu, refused %llu (out of reach %llu), dropped "
                "seqs %llu, refunds %llu, reaped %llu, rolls refused %llu", static_cast<unsigned long long>(g_sent),
                static_cast<unsigned long long>(g_taken), static_cast<unsigned long long>(g_refusedCount),
                static_cast<unsigned long long>(g_outOfReach), static_cast<unsigned long long>(g_staleSeqs),
                static_cast<unsigned long long>(g_refunds), static_cast<unsigned long long>(g_reaped),
                static_cast<unsigned long long>(g_rollsRefused));
    g_shortWarned = false;
    for (auto& q : g_waiting) q.clear();
    for (auto& q : g_rejections) q.clear();
    for (auto& b : g_rate) b.Reset();
    for (int slot = 0; slot < g_spends.size(); ++slot) {
        g_spends[slot] = Spends{};
        g_ack[slot] = 0;
        g_refused[slot] = RefusedFifo{};
        g_overfullWarned[slot] = false;
        g_faultsSeen[slot] = 0;
    }
    g_occupants.fill(0);
    g_drainSlot = 1;
    g_claims.clear();
    g_unknownPulls.clear();
    g_clientCommands.clear();
    g_seq = g_lastAck = 0;
    g_clientCaptures.clear();
    g_refundDebt.clear();
    for(auto& c:g_actionCache)c={};
    for(auto& c:g_holdCache)c={};
    g_sent = g_reaped = g_claimsCapped = g_taken = g_refusedCount = g_refunds = g_queuesFull = g_outOfReach = g_rollsRefused = 0;
    g_staleSeqs = 0;
}

}  // namespace coop::fuse_tower_ops
