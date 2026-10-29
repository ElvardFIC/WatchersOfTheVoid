// coop/interactables/serverbox_sync.cpp -- see coop/interactables/serverbox_sync.h.
//
// The engine's break state -- the box's IsBroken, damaged and minigame, the notify-free check() it re-skins from, the
// farm's three totals, the verbs and the repair widget -- is ue_wrap/devices/serverbox's; this lane owns the wire half:
// the row, its width, the poll, the repair intent and who may author any of it.

#include "coop/interactables/serverbox_sync.h"

#include "coop/element/intent_authority.h"  // IntentTarget: a repair's presser within reach of its box
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"  // kMaxPeers
#include "coop/player/roster_ledger.h"     // PerSlotState

#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/serverbox.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/economy.h"           // AddPoints and the saveSlot: the repair reward, paid on the host
#include "ue_wrap/engine/world_identity.h"   // Generation, the baseline's anchor
#include "ue_wrap/world/world_singleton.h"   // Gamemode, its other anchor

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace coop::serverbox_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace SB = ue_wrap::serverbox;
namespace sg = ue_wrap::script_gate;

std::atomic<coop::net::Session*> g_session{nullptr};
std::atomic<uint64_t> g_epoch{0};
uint32_t g_sessionSerial = 0;
bool g_driving = false;
Counts g_counts;

constexpr int       kMaxServers     = 64;    // the row's width; a base runs ~54 boxes
constexpr long long kPollIntervalMs = 1000;
constexpr float     kRepairReachUU  = 400.0f;  // the repair widget is used standing at the box
constexpr auto      kSayEvery       = std::chrono::seconds(10);

UE_FACT_FN(kRowBreakServer, "serverbox_sync", L"serverBox_C", L"breakServer", Declared);
UE_FACT_FN(kRowBreakType, "serverbox_sync", L"serverBox_C", L"break_type", Declared);
UE_FACT_FN(kRowFix, "serverbox_sync", L"serverBox_C", L"fix", Declared);
UE_FACT_FN(kRowAddPoints, "serverbox_sync", L"lib_C", L"addPoints", Declared);
constexpr int kTagBreak = 0x53424231;  // 'SBB1'
constexpr int kTagType  = 0x53425432;  // 'SBT2'
constexpr int kTagFix   = 0x53424633;  // 'SBF3'

coop::net::Session* ClientSession() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->connected() && g_sessionSerial == coop::net::session_serial::Current() &&
        s->role() == coop::net::Role::Client ? s : nullptr;
}
coop::net::Session* HostSession() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->connected() && g_sessionSerial == coop::net::session_serial::Current() &&
        s->role() == coop::net::Role::Host ? s : nullptr;
}

struct Root {
    uint64_t epoch = 0;
    uint32_t world = 0;
    void* gm = nullptr;
    int32_t gmIndex = -1, gmSerial = 0;
};

Root CaptureRoot() {
    Root root{g_epoch.load(), ue_wrap::world_identity::Generation(), ue_wrap::world_singleton::Gamemode()};
    if (root.gm) {
        root.gmIndex = R::InternalIndexOf(root.gm);
        root.gmSerial = R::AllocateSlotSerial(root.gmIndex);
    }
    return root;
}

bool Current(coop::net::Session* s, const Root& root) {
    return s && s == g_session.load() && s->connected() && g_epoch.load() == root.epoch &&
        g_sessionSerial == coop::net::session_serial::Current() &&
        root.world == ue_wrap::world_identity::Generation() &&
        root.gm == ue_wrap::world_singleton::Gamemode() &&
        (!root.gm || (R::IsLiveByIndex(root.gm, root.gmIndex) && R::SlotSerial(root.gmIndex) == root.gmSerial));
}

bool SameRoot(const Root& a, const Root& b) {
    return a.epoch == b.epoch && a.world == b.world && a.gm == b.gm &&
        a.gmIndex == b.gmIndex && a.gmSerial == b.gmSerial;
}

struct DriveGuard {
    DriveGuard() { g_driving = true; }
    ~DriveGuard() { g_driving = false; }
};

long long NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// The box list has one owner, ue_wrap/devices/serverbox: this lane keeps only its own cap, the row's width.
int32_t ReadServers(std::vector<void*>& out) {
    out.clear();
    const int32_t num = static_cast<int32_t>(SB::ReadServers(out));
    if (num > kMaxServers) out.resize(kMaxServers);
    return num;
}

// The current row from the live gamemode. False if not readable yet. The repair group's fields ride when it resolved.
bool ReadState(coop::net::ServerStatePayload& p) {
    SB::Aggregates agg;
    if (!SB::ReadAggregates(agg)) return false;
    std::vector<void*> servers;
    const int32_t num = ReadServers(servers);
    if (num > kMaxServers) {
        static bool warned = false;
        if (!warned) { warned = true; UE_LOGW("serverbox_sync: %d servers > cap %d -- syncing first %d only",
                                              num, kMaxServers, kMaxServers); }
    }
    std::memset(&p, 0, sizeof(p));
    const bool repair = SB::EnsureRepairResolved();
    for (size_t i = 0; i < servers.size(); ++i) {
        void* sb = servers[i];
        if (!sb || !R::IsLive(sb)) continue;
        if (SB::ReadIsBroken(sb)) p.isBrokenMask |= (1ull << i);
        SB::RepairState r;
        if (repair && SB::ReadRepairState(sb, r)) {
            if (r.damaged) p.damagedMask |= (1ull << i);
            p.minigame[i] = static_cast<uint8_t>(r.minigame);
        }
    }
    p.brokenServers = agg.brokenServers;
    p.effCalc  = agg.efficiencyCalc;
    p.effDownl = agg.efficiencyDownload;
    p.serverCount = static_cast<uint8_t>(servers.size());
    // A best that does not read, or reads as no number, goes out as none: a NaN never equals the last
    // row and would send one every poll.
    if (!SB::ReadRepairBest(p.timeBest) || !std::isfinite(p.timeBest)) p.timeBest = 0.f;
    return true;
}

// ---- host delivery -------------------------------------------------------------------------------
uint32_t g_polledWorldGen = 0;
void* g_polledGm = nullptr;  // the gamemode the baseline was read from; an identity, never dereferenced
int32_t g_polledGmSerial = 0;
long long g_lastPollMs = 0;
struct Delivery {
    uint32_t occupant = 0;
    bool sent = false;
    uint64_t requested = 0, delivered = 0;
    coop::net::ServerStatePayload last{};
};
coop::roster_ledger::PerSlotState<Delivery> g_delivery;

struct Pending {
    bool present = false;
    uint64_t version = 0;
    Root root{};
    coop::net::ServerStatePayload state{};
    // ApplyBreak writes IsBroken before invoking the native check(). A failed
    // call leaves that flag changed, so retain its separate effect debt, bound
    // to the exact box incarnation, even when a newer row has the same flag.
    ue_wrap::CachedObjRef skinDebt[kMaxServers];
} g_pending;

void ConformWorld(const Root& root) {
    if (g_polledWorldGen == root.world && g_polledGm == root.gm && g_polledGmSerial == root.gmSerial) return;
    g_polledWorldGen = root.world;
    g_polledGm = root.gm;
    g_polledGmSerial = root.gmSerial;
    for (int slot = 0; slot < g_delivery.size(); ++slot) g_delivery[slot] = {};
}

// The flags, the types and the broken count are the edges; the efficiencies also move off one (a download ramps
// serverEfficiency_downl), so they count with a small epsilon and the SAT console's sv.*/tw.* reads stay fresh.
bool StateChanged(const coop::net::ServerStatePayload& p, const coop::net::ServerStatePayload& last) {
    return p.isBrokenMask != last.isBrokenMask || p.damagedMask != last.damagedMask ||
           p.brokenServers != last.brokenServers || p.serverCount != last.serverCount ||
           std::memcmp(p.minigame, last.minigame, sizeof(p.minigame)) != 0 ||
           std::fabs(p.effCalc  - last.effCalc)  > 0.005f ||
           std::fabs(p.effDownl - last.effDownl) > 0.005f || p.timeBest != last.timeBest;
}

void Deliver(coop::net::Session* s, int slot, const coop::net::ServerStatePayload& p, const Root& root) {
    auto& row = g_delivery[slot];
    const uint32_t occupant = s->peerGenerationForSlot(slot);
    if (row.occupant != occupant) { row = {}; row.occupant = occupant; }
    if (!s->HasPeerConn(slot) || !s->IsSlotWorldReady(slot)) { row.sent = false; return; }
    if (row.sent && row.requested == row.delivered && !StateChanged(p, row.last)) return;
    const uint64_t request = row.requested;
    const bool sent = s->SendReliableToSlot(slot, coop::net::ReliableKind::ServerState, &p, sizeof(p));
    if (!Current(s, root) || s->peerGenerationForSlot(slot) != occupant) return;
    // A fan-out's true means ANY peer accepted it, so only the single-target
    // accepted send can retire this recipient's debt.
    if (!sent) return;
    row.sent = true; row.last = p; row.delivered = request;
    UE_LOGI("serverbox_sync: host state sent to slot %d (broken=%d mask=0x%llX damaged=0x%llX count=%d)", slot,
        p.brokenServers, static_cast<unsigned long long>(p.isBrokenMask),
        static_cast<unsigned long long>(p.damagedMask), p.serverCount);
}

// HOST: every recipient keeps its own accepted row, including an unchanged join/refusal correction.
void BroadcastIfChanged(coop::net::Session* s) {
    if (g_driving) return;
    DriveGuard guard;
    const Root root = CaptureRoot();
    ConformWorld(root);
    coop::net::ServerStatePayload p{};
    if (!root.world || !root.gm || !ReadState(p) || !Current(s, root)) return;
    for (int slot = 1; slot < g_delivery.size(); ++slot) {
        Deliver(s, slot, p, root);
        if (!Current(s, root)) return;
    }
}

bool SendStateTo(coop::net::Session* s, int slot) {
    const Root root = CaptureRoot();
    ConformWorld(root);
    auto& row = g_delivery[slot];
    const uint32_t occupant = s->peerGenerationForSlot(slot);
    if (row.occupant != occupant) { row = {}; row.occupant = occupant; }
    ++row.requested;
    if (g_driving) return false;
    DriveGuard guard;
    coop::net::ServerStatePayload p{};
    if (!root.world || !root.gm || !SB::EnsureBreakResolved() || !ReadState(p) || !Current(s, root)) return false;
    Deliver(s, slot, p, root);
    return Current(s, root) && row.occupant == occupant && row.sent && row.delivered == row.requested;
}

// ---- client apply ---------------------------------------------------------------------------------
bool ApplyState(coop::net::Session* s, const coop::net::ServerStatePayload& p, const Root& root) {
    // The totals, so the SAT console's sv.*/tw.* queries read the host's.
    SB::Aggregates agg;
    agg.brokenServers      = p.brokenServers;
    agg.efficiencyCalc     = p.effCalc;
    agg.efficiencyDownload = p.effDownl;
    if (!SB::WriteAggregates(agg) || !Current(s, root)) return false;
    // The record the host pays against, so this peer's repair widget judges a record (its sound, its own best write)
    // against the same best the host's payment does. The widget still writes its own solve here first; this row,
    // broadcast after the host's repair, puts the host's answer over it.
    bool complete = true;
    if (std::isfinite(p.timeBest) && p.timeBest >= 0.f) complete = SB::WriteRepairBest(p.timeBest);
    if (!Current(s, root)) return false;
    std::vector<void*> servers;
    ReadServers(servers);
    const bool repair = SB::EnsureRepairResolved();
    if (!Current(s, root)) return false;
    complete = complete && repair && servers.size() >= p.serverCount;
    int reskinned = 0;
    const int32_t n = static_cast<int32_t>(servers.size());
    const int32_t take = n < p.serverCount ? n : p.serverCount;
    for (int32_t i = 0; i < take && i < kMaxServers; ++i) {
        void* sb = servers[i];
        if (!sb || !R::IsLive(sb)) { complete = false; continue; }
        if (repair) {
            // What the repair widget reads: the type it enters with, and whether the repair pays.
            SB::RepairState r;
            r.damaged = (p.damagedMask >> i) & 1ull;
            r.minigame = p.minigame[i];
            complete = SB::WriteRepairState(sb, r) && complete;
            if (!Current(s, root)) return false;
        }
        const bool desired = (p.isBrokenMask >> i) & 1ull;
        auto& debt = g_pending.skinDebt[i];
        const bool retry = debt.Is(sb);
        if (!retry) debt.Reset();
        if (SB::ReadIsBroken(sb) == desired && !retry) continue;
        debt.Set(sb);   // publish before native callbacks can deliver a newer row
        const bool applied = SB::ApplyBreak(sb, desired);
        if (!Current(s, root)) return false;
        if (applied && debt.Is(sb)) {
            ++reskinned;
            debt.Reset();
        } else complete = false;
    }
    if (reskinned)
        UE_LOGI("serverbox_sync: client applied host state (broken=%d mask=0x%llX, %d server(s) re-skinned)",
                p.brokenServers, static_cast<unsigned long long>(p.isBrokenMask), reskinned);
    return complete;
}

// ---- the verbs at the gate ------------------------------------------------------------------------
std::chrono::steady_clock::time_point g_nextBreakSay{}, g_nextFixSay{};
coop::roster_ledger::PerSlotState<std::chrono::steady_clock::time_point> g_nextRefusedSay;

bool SayNow(std::chrono::steady_clock::time_point& next) {
    const auto now = std::chrono::steady_clock::now();
    if (now < next) return false;
    next = now + kSayEvery;
    return true;
}

std::wstring CallerName(const sg::Call& call) {
    return call.callerFunction ? R::ToString(R::NameOf(call.callerFunction)) : std::wstring(L"ProcessEvent");
}

// CLIENT: a break is the host's world event or its save's; the host's row brings it.
sg::Verdict OnBreakPre(const sg::Call& call) {
    if (!ClientSession()) return sg::Verdict::Run;
    ++g_counts.refusedBreaks;
    if (SayNow(g_nextBreakSay))
        UE_LOGI("serverbox_sync: this client's own %ls refused (called from %ls; %llu refused) -- the break is the "
                "host's", call.tag == kTagType ? kRowBreakType.Name() : kRowBreakServer.Name(),
                CallerName(call).c_str(),
                static_cast<unsigned long long>(g_counts.refusedBreaks));
    return sg::Verdict::Cancel;
}

// The minigame's reward, read from ui_serverMinigame's end(correct): a box broken by damage pays nothing;
// any other pays SelectInt(50, 15, isLol) and runs fix; then a solve time under saveSlot.servertimeBest
// pays SelectInt(100, 30, isLol) more, and the best becomes the time (the first solve only sets it).
constexpr int32_t kRewardPlain = 15, kRewardLol = 50, kRecordPlain = 30, kRecordLol = 100;

// CLIENT: the widget's own fields the reward reads, at the moment its fix is sent.
void ReadWidgetReward(void* widget, coop::net::ServerRepairPayload& p) {
    bool lol = false;
    float t = 0.f;
    if (!SB::ReadRepairReward(widget, lol, t)) {
        UE_LOGW("serverbox_sync: the repair widget's isLol/time did not resolve -- the host fixes without the reward");
        return;
    }
    p.flags = static_cast<uint8_t>(coop::net::kRepairSettles | (lol ? coop::net::kRepairLol : 0));
    const float ds = std::isfinite(t) && t > 0.f ? t * 10.f + 0.5f : 0.f;
    p.timeDs = static_cast<uint16_t>(ds >= 65535.f ? 65535.f : ds);
}

// CLIENT: the widget's points are the host's to pay with the fix, so its own credit is refused here: the
// shared balance would take it, then drop it when the host's row lands, and the host's later payment showed
// as the difference (a +15 reward, then +60 from the task, read +45).
uint64_t g_rewardsRefused = 0;
std::chrono::steady_clock::time_point g_nextRewardSay{};
sg::Verdict OnRewardPre(const sg::Call& call) {
    if (!ClientSession() || !SB::IsRepairWidget(call.callerObject)) return sg::Verdict::Run;
    ++g_rewardsRefused;
    if (SayNow(g_nextRewardSay))
        UE_LOGI("serverbox_sync: this client's repair reward refused (%llu) -- the host pays it with the fix",
                static_cast<unsigned long long>(g_rewardsRefused));
    return sg::Verdict::Cancel;
}

// The lol mode and the solve time are the client's report: the host cannot see the minigame, so it bounds what it
// takes (bounds apply to clients). A time under this settles no record and leaves the host's best alone -- one
// impossible time written there would sit out of every honest solve's reach for the life of the save. One second
// is an assumption, not a measurement: no solve of the eight puzzles has been timed, and the bound only has to
// sit under the fastest honest one.
constexpr uint16_t kMinSolveDs = 10;

// HOST: the reward of a client's repair the host just ran, as the widget computes it, from the host's own
// box and saveSlot. Returns the base reward paid; `record` gets the record bonus paid.
int32_t PayRepairReward(coop::net::Session* s, const Root& root,
                       const coop::net::ServerRepairPayload& p, bool damaged, int32_t& record) {
    record = 0;
    if (!(p.flags & coop::net::kRepairSettles) || damaged) return 0;
    const bool lol = (p.flags & coop::net::kRepairLol) != 0;
    const int32_t reward = lol ? kRewardLol : kRewardPlain;
    if (!ue_wrap::economy::AddPoints(reward)) return 0;
    if (!Current(s, root)) return reward;
    if (p.timeDs < kMinSolveDs) return reward;   // no time, or none a hand makes: the reward, no record
    float best = 0.f;
    if (!SB::ReadRepairBest(best) || !Current(s, root)) return reward;
    const float t = static_cast<float>(p.timeDs) / 10.f;
    const int32_t bonus = lol ? kRecordLol : kRecordPlain;
    if (t < best && ue_wrap::economy::AddPoints(bonus)) record = bonus;
    if (!Current(s, root)) return reward;
    SB::WriteRepairBest(best == 0.f ? t : std::fmin(best, t));
    return reward;
}

// CLIENT: its player's repair, the gamemode's repair widget calling fix, goes to the host; any other fix is refused.
sg::Verdict OnFixPre(const sg::Call& call) {
    auto* s = ClientSession();
    if (!s) return sg::Verdict::Run;
    if (!SB::IsRepairWidget(call.callerObject)) {
        ++g_counts.refusedFixes;
        if (SayNow(g_nextFixSay))
            UE_LOGI("serverbox_sync: this client's own fix refused (called from %ls; %llu refused) -- a repair is the "
                    "host's", CallerName(call).c_str(), static_cast<unsigned long long>(g_counts.refusedFixes));
        return sg::Verdict::Cancel;
    }
    const int32_t box = SB::IndexOf(call.object);
    if (box < 0 || box >= kMaxServers) {
        UE_LOGW("serverbox_sync: this player's repair names a box outside the server list (index %d) -- not sent", box);
        return sg::Verdict::Cancel;
    }
    coop::net::ServerRepairPayload p{};
    p.box = static_cast<uint8_t>(box);
    ReadWidgetReward(call.callerObject, p);
    g_counts.lastSentLol = (p.flags & coop::net::kRepairLol) != 0;
    if (s->SendReliableToSlot(0, coop::net::ReliableKind::ServerRepair, &p, sizeof(p))) ++g_counts.repairsSent;
    UE_LOGI("serverbox_sync: this player's repair of box %d sent to the host (%llu sent)", box,
            static_cast<unsigned long long>(g_counts.repairsSent));
    return sg::Verdict::Cancel;
}

struct Watch {
    const ue_wrap::facts::Function* row;
    int            tag;
    sg::PreFn      pre;
    bool           registered = false;
    bool           settled = false;
};
Watch g_watches[] = {
    {&kRowBreakServer, kTagBreak, &OnBreakPre},
    {&kRowBreakType, kTagType, &OnBreakPre},
    {&kRowFix, kTagFix, &OnFixPre},
    {&kRowAddPoints, 0x53425034 /* 'SBP4' */, &OnRewardPre},
};
bool g_watchesSettled = false;

void DriveWatches() {
    if (g_watchesSettled) return;
    sg::ResolvePendingNames();
    bool all = true;
    for (Watch& w : g_watches) {
        if (w.settled) continue;
        if (!w.registered) {
            w.registered = sg::WatchName(*w.row, w.tag, w.pre, nullptr);
            if (!w.registered) {
                w.settled = true;
                UE_LOGE("serverbox_sync: the gate took no watch on %ls::%ls -- a client authors that verb",
                        w.row->Class(), w.row->Name());
                continue;
            }
        }
        if (sg::NameWatchSettled(*w.row, w.tag)) {
            w.settled = true;
            if (sg::NameWatchLive(*w.row, w.tag)) UE_LOGI("serverbox_sync: the watch on %ls::%ls is live",
                                                           w.row->Class(), w.row->Name());
            else UE_LOGE("serverbox_sync: the watch on %ls::%ls settled dead -- a client authors that verb",
                         w.row->Class(), w.row->Name());
            continue;
        }
        all = false;
    }
    g_watchesSettled = all;
}

}  // namespace

void Install(coop::net::Session* session) {
    ++g_epoch;
    g_sessionSerial = coop::net::session_serial::Current();
    g_polledWorldGen = 0; g_polledGm = nullptr; g_polledGmSerial = 0;
    g_lastPollMs = 0;
    for (int slot = 0; slot < g_delivery.size(); ++slot) g_delivery[slot] = {};
    g_pending = {};
    g_session.store(session, std::memory_order_release);
    DriveWatches();
}

void Tick() {
    if (!GT::IsGameThread()) return;
    DriveWatches();
    if (g_driving) return;
    if (auto* client = ClientSession()) {
        if (!g_pending.present) return;
        // The world can be announced before its gamemode is discoverable. Bind
        // the snapshot once it appears; once bound, a replacement cancels it.
        if (!g_pending.root.gm) {
            const Root discovered = CaptureRoot();
            if (g_pending.root.epoch == discovered.epoch && g_pending.root.world == discovered.world)
                g_pending.root = discovered;
        }
        const Pending pending = g_pending;
        if (!Current(client, pending.root)) { g_pending.present = false; return; }
        if (!pending.root.gm) return;
        DriveGuard guard;
        if (!SB::EnsureBreakResolved() || !Current(client, pending.root)) return;
        if (ApplyState(client, pending.state, pending.root) && Current(client, pending.root) &&
            g_pending.version == pending.version) g_pending.present = false;
        return;
    }
    auto* s = HostSession();
    if (!s) return;
    const long long now = NowMs();
    if (now - g_lastPollMs < kPollIntervalMs) return;
    g_lastPollMs = now;
    const Root root = CaptureRoot();
    if (!SB::EnsureBreakResolved() || !Current(s, root)) return;
    BroadcastIfChanged(s);
}

void QueueConnectBroadcastForSlot(int slot) {
    auto* s = HostSession();
    if (!GT::IsGameThread() || !s || slot <= 0 || slot >= static_cast<int>(coop::players::kMaxPeers)) return;
    // Unconditional (even all-healthy): the joiner's load ran none of the verbs; this row is its state.
    if (SendStateTo(s, slot)) UE_LOGI("serverbox_sync: connect-snapshot sent to slot %d", slot);
    else UE_LOGW("serverbox_sync: connect-snapshot to slot %d send FAILED", slot);
}

void OnReliable(const coop::net::ServerStatePayload& payload) {
    if (!GT::IsGameThread()) {
        UE_LOGW("serverbox_sync: OnReliable off-game-thread -- dropping");
        return;
    }
    auto* s = ClientSession();
    if (!s) return;  // from the host alone: its receive edge refuses a client's copy
    const Root root = CaptureRoot();
    if (!root.world) return;
    // A world-ready snapshot can precede wrapper resolution or the server list.
    // Keep the latest row until ALL fields apply in this same root lifetime.
    if (!SameRoot(root, g_pending.root)) {
        for (auto& debt : g_pending.skinDebt) debt.Reset();
    }
    g_pending.present = true;
    ++g_pending.version;
    g_pending.root = root;
    g_pending.state = payload;
    Tick();
}

void OnRepair(const coop::net::ServerRepairPayload& payload, int senderPeerSlot) {
    if (!GT::IsGameThread()) {
        UE_LOGW("serverbox_sync: OnRepair off-game-thread -- dropping");
        return;
    }
    auto* s = HostSession();
    if (!s || senderPeerSlot < 1 || senderPeerSlot >= static_cast<int>(coop::players::kMaxPeers)) return;
    const Root root = CaptureRoot();
    if (!SB::EnsureBreakResolved() || !SB::EnsureRepairResolved() || !Current(s, root)) return;
    const uint8_t slot = static_cast<uint8_t>(senderPeerSlot);
    const uint32_t occupant = s->peerGenerationForSlot(slot);
    std::vector<void*> servers;
    ReadServers(servers);
    void* box = payload.box < servers.size() ? servers[payload.box] : nullptr;
    const char* why = nullptr;
    SB::RepairState repair{};   // the damage flag the reward reads, taken before the fix clears it
    if (!box || !R::IsLive(box)) why = "no such box";
    else if (!SB::ReadIsBroken(box)) why = "the box is not broken here";
    else {
        const auto token = coop::element::IntentTarget::ForClientIntent(*s, slot, kRepairReachUU);
        if (!token.HasBody()) why = "the host has no body for that player";
        else if (const auto outcome = token.Authorize(box).outcome; outcome != coop::element::IntentOutcome::Ok)
            why = outcome == coop::element::IntentOutcome::NoTarget ? "the box's place did not read"
                                                                      : "the box is out of its reach";
        else if (!Current(s, root) || s->peerGenerationForSlot(slot) != occupant) return;
        else if (!SB::ReadRepairState(box, repair) || !SB::CallFix(box)) why = "the box's fix did not run";
    }
    if (!Current(s, root) || s->peerGenerationForSlot(slot) != occupant) return;
    if (why) {
        ++g_counts.repairsRefused;
        SendStateTo(s, slot);
        if (SayNow(g_nextRefusedSay[slot]))
            UE_LOGI("serverbox_sync: slot %u's repair of box %u refused (%s) -- answered with the host's row", slot,
                    payload.box, why);
        return;
    }
    ++g_counts.repairsRun;
    int32_t record = 0;
    const int32_t reward = PayRepairReward(s, root, payload, repair.damaged, record);
    if (!Current(s, root)) return;
    g_counts.lastPaid = reward;
    g_counts.lastRecord = record;
    g_counts.lastPaidLol = (payload.flags & coop::net::kRepairLol) != 0;
    UE_LOGI("serverbox_sync: HOST ran slot %u's repair of box %u (reward %d, record %d%s)", slot, payload.box,
            reward, record, (payload.flags & coop::net::kRepairSettles) ? "" : ", the sender sent no reward fields");
    BroadcastIfChanged(s);
}

void OnDisconnect() {
    if (g_counts.refusedBreaks || g_counts.refusedFixes || g_counts.repairsSent || g_counts.repairsRun ||
        g_counts.repairsRefused)
        UE_LOGI("serverbox_sync: session end -- refused breaks=%llu fixes=%llu, repairs sent=%llu run=%llu "
                "refused=%llu", static_cast<unsigned long long>(g_counts.refusedBreaks),
                static_cast<unsigned long long>(g_counts.refusedFixes),
                static_cast<unsigned long long>(g_counts.repairsSent),
                static_cast<unsigned long long>(g_counts.repairsRun),
                static_cast<unsigned long long>(g_counts.repairsRefused));
    g_counts = Counts{};
    ++g_epoch;
    g_sessionSerial = 0;
    g_polledWorldGen = 0; g_polledGm = nullptr; g_polledGmSerial = 0;
    for (int slot = 0; slot < g_delivery.size(); ++slot) g_delivery[slot] = {};
    g_pending = {};
    g_lastPollMs = 0;
    g_session.store(nullptr, std::memory_order_release);
}

Counts LaneCounts() { return g_counts; }

}  // namespace coop::serverbox_sync
