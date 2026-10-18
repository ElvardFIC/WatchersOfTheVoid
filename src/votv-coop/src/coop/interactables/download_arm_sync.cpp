// coop/interactables/download_arm_sync.cpp -- see coop/interactables/download_arm_sync.h.

#include "coop/interactables/download_arm_sync.h"

#include "coop/interactables/desk_input_sync.h"
#include "coop/interactables/dish_sync.h"
#include "coop/net/session.h"
#include "coop/session/net_pump.h"  // IsInAnnouncedWorld: a client's own world load runs natively

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/dish.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <string>

namespace coop::download_arm_sync {
namespace {

namespace CD = ue_wrap::console_desk;
namespace D  = ue_wrap::dish;
namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;

const wchar_t* const kDeskClass   = L"analogDScreenTest_C";
const wchar_t* const kGameClass   = L"mainGamemode_C";
const wchar_t* const kFormVerb    = L"formDownload";
const wchar_t* const kInitVerb    = L"initDownloadSignal";
const wchar_t* const kDeleteVerb  = L"deleteActiveSignal";
constexpr int kTagForm   = 0x44414631;  // 'DAF1'
constexpr int kTagInit   = 0x44414932;  // 'DAI2'
constexpr int kTagDelete = 0x44414434;  // 'DAD4'
constexpr auto kRefusalSayEvery = std::chrono::seconds(10);

std::atomic<coop::net::Session*> g_session{nullptr};
Counts g_counts;
struct PendingSend {
    coop::net::DishArmPayload payload{};
    int slot = -1;
    uint64_t queuedAt = 0;
};
std::deque<PendingSend> g_pendingSends;
constexpr size_t kMaxPendingSends = 64;
std::deque<coop::net::DishArmPayload> g_pendingReceives;
constexpr size_t kMaxPendingReceives = 32;

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// CLIENT: the host's values while a replay runs the game's verb; the PREs write them into the verb's parameters.
struct Replay {
    bool    active = false;
    bool    arm = false;
    float   decoded = 0.f;
    int32_t polarity = 0;
    int     written = 0;    // parameter frames the PREs wrote
    int     unwritten = 0;  // frames whose parameters did not resolve
};
Replay g_replay;

// A replay's values for its extent, the enclosing ones back after it.
struct ReplayScope {
    Replay prev;
    ReplayScope(bool arm, float decoded, int32_t polarity) : prev(g_replay) {
        g_replay = Replay{true, arm, decoded, polarity, 0, 0};
    }
    ~ReplayScope() { g_replay = prev; }
};

// HOST: the resets its own machine made, and the count at the capture of each joiner's world. A joiner whose count
// moved gets a reset ahead of the desk's seed, the game's order (a reset, then a catch, then an arm).
uint64_t g_resetSerial = 0;
struct Captured {
    bool     valid = false;
    uint64_t resetSerial = 0;
};
Captured g_captured[coop::net::kMaxPeers];

std::chrono::steady_clock::time_point g_nextRefusalSay{};

coop::net::Session* ClientSession() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->connected() && s->role() == coop::net::Role::Client ? s : nullptr;
}
coop::net::Session* HostSession() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->connected() && s->role() == coop::net::Role::Host ? s : nullptr;
}
bool IsHostRole() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->running() && s->role() == coop::net::Role::Host;
}

// CLIENT: its own arm or reset, outside a replay: the machine is the host's. A call on an object of a world this client
// has not announced is that world's own load, which forms the download the host's save holds, and runs.
sg::Verdict RefuseOwn(const sg::Call& call, const wchar_t* verb) {
    if (!coop::net_pump::IsInAnnouncedWorld(call.object)) {
        ++g_counts.loadNative;
        return sg::Verdict::Run;
    }
    ++g_counts.refused;
    const auto now = std::chrono::steady_clock::now();
    if (now >= g_nextRefusalSay) {
        g_nextRefusalSay = now + kRefusalSayEvery;
        const std::wstring caller =
            call.callerFunction ? R::ToString(R::NameOf(call.callerFunction)) : std::wstring(L"ProcessEvent");
        UE_LOGW("download_arm: this client's own %ls refused (called from %ls; %llu refused) -- the machine is the host's",
                verb, caller.c_str(), static_cast<unsigned long long>(g_counts.refused));
    }
    return sg::Verdict::Cancel;
}

sg::Verdict OnFormPre(const sg::Call& call) {
    if (!ClientSession()) return sg::Verdict::Run;
    if (!g_replay.active) return RefuseOwn(call, kFormVerb);
    if (!g_replay.arm) return sg::Verdict::Run;
    CD::WriteFormDownloadArgs(call.function, call.locals, g_replay.decoded, g_replay.polarity) ? ++g_replay.written
                                                                                               : ++g_replay.unwritten;
    return sg::Verdict::Run;
}

// Inside a reset replay only: the reset's init is its one call there, and it would roll its own polarity. Inside an
// arm replay formDownload hands its own, already the host's.
sg::Verdict OnInitPre(const sg::Call& call) {
    if (!g_replay.active || g_replay.arm || !ClientSession()) return sg::Verdict::Run;
    CD::WriteInitDownloadPolarity(call.function, call.locals, g_replay.polarity) ? ++g_replay.written
                                                                                 : ++g_replay.unwritten;
    return sg::Verdict::Run;
}

sg::Verdict OnDeletePre(const sg::Call& call) {
    if (!ClientSession() || g_replay.active) return sg::Verdict::Run;
    return RefuseOwn(call, kDeleteVerb);
}

void SendRow(coop::net::Session* s, bool armed, int toSlot) {
    coop::net::DishArmPayload p{};
    p.armed = armed ? 1 : 0;
    CD::ReadDownloadProgress(p.decoded, p.polarity);
    if (!armed) p.decoded = 0.f;
    const bool sent = toSlot < 0
        ? s->SendReliable(coop::net::ReliableKind::DishArm, &p, sizeof(p))
        : s->SendReliableToSlot(toSlot, coop::net::ReliableKind::DishArm, &p, sizeof(p));
    if (!sent) {
        if (g_pendingSends.size() < kMaxPendingSends) {
            g_pendingSends.push_back({p, toSlot, NowMs()});
        } else {
            UE_LOGE("download_arm: outbound queue full; %s row for slot %d was lost",
                    armed ? "ARM" : "RESET", toSlot);
        }
    }
}

// HOST: the arm, when the body formed one (it returns early when the objects table has no row for the caught signal).
void OnFormPost(const sg::Call&) {
    auto* s = HostSession();
    if (!s) return;
    if (!CD::DownloadMeshValid()) {
        UE_LOGI("download_arm: host formDownload formed nothing (no objects row for the caught signal) -- no ARM sent");
        return;
    }
    SendRow(s, true, -1);
    ++g_counts.sentArm;
    float decoded = 0.f;
    int32_t polarity = 0;
    CD::ReadDownloadProgress(decoded, polarity);
    UE_LOGI("download_arm: host ARM sent (decoded=%.1f polarity=%d)", decoded, polarity);
}

// HOST: the reset, after the whole verb: the desk's reset, the renderer's signal actor, the signal camera's trigger.
void OnDeletePost(const sg::Call&) {
    if (!IsHostRole()) return;
    ++g_resetSerial;
    auto* s = HostSession();
    if (!s) return;
    SendRow(s, false, -1);
    ++g_counts.sentReset;
    float decoded = 0.f;
    int32_t polarity = 0;
    CD::ReadDownloadProgress(decoded, polarity);
    UE_LOGI("download_arm: host RESET sent (polarity=%d)", polarity);
}

// The three watches, each registered once and driven to settled; one refused by a full table is said once.
struct Watch {
    const wchar_t* cls;
    const wchar_t* fn;
    int            tag;
    sg::PreFn      pre;
    sg::PostFn     post;
    bool           registered = false;
    bool           settled = false;
};
Watch g_watches[] = {
    {kDeskClass, kFormVerb, kTagForm, &OnFormPre, &OnFormPost},
    {kDeskClass, kInitVerb, kTagInit, &OnInitPre, nullptr},
    {kGameClass, kDeleteVerb, kTagDelete, &OnDeletePre, &OnDeletePost},
};
bool g_watchesSettled = false;

void DriveWatches() {
    if (g_watchesSettled) return;
    sg::ResolvePendingNames();
    bool all = true;
    for (Watch& w : g_watches) {
        if (w.settled) continue;
        if (!w.registered) {
            w.registered = sg::WatchClassName(w.cls, w.fn, w.tag, w.pre, w.post);
            if (!w.registered) {
                w.settled = true;
                UE_LOGE("download_arm: the gate took no watch on %ls::%ls -- that half of the machine is not the host's",
                        w.cls, w.fn);
                continue;
            }
        }
        if (sg::ClassNameWatchSettled(w.cls, w.fn, w.tag)) {
            w.settled = true;
            if (sg::ClassNameWatchLive(w.cls, w.fn, w.tag)) UE_LOGI("download_arm: the watch on %ls::%ls is live", w.cls, w.fn);
            else UE_LOGE("download_arm: the watch on %ls::%ls settled dead -- that half of the machine is not the host's",
                         w.cls, w.fn);
            continue;
        }
        all = false;
    }
    g_watchesSettled = all;
}

void ReplayArm(float decoded, int32_t polarity) {
    CD::CoordSignal sig;
    if (!CD::ReadCoordSignal(sig) || sig.objectName.empty() || sig.objectName == L"None") {
        ++g_counts.dropped;
        UE_LOGW("download_arm: ARM with no caught signal here -- the catch row has not applied; nothing formed");
        return;
    }
    // The host's arm proves its dishes stopped: the mirror settles, and checkFordDishes' own gate (no active dish)
    // opens, as it did on the host.
    coop::dish_sync::SettleForArm();
    bool called = false;
    int written = 0, unwritten = 0;
    {
        ReplayScope scope(true, decoded, polarity);
        called = D::CallCheckFordDishes();
        written = g_replay.written;
        unwritten = g_replay.unwritten;
    }
    float d = 0.f;
    int32_t pol = 0;
    const bool read = CD::ReadDownloadProgress(d, pol);
    if (!called || written != 1 || unwritten != 0 || !read || pol != polarity || !CD::DownloadMeshValid()) {
        UE_LOGW("download_arm: ARM replay off -- checkFordDishes %s, formDownload written %d time(s) (%d unresolved), "
                "polarity %d against the host's %d, mesh %s", called ? "ran" : "FAILED", written, unwritten, pol, polarity,
                CD::DownloadMeshValid() ? "valid" : "INVALID");
        ++g_counts.replayOff;
        return;
    }
    ++g_counts.replayedArm;
    UE_LOGI("download_arm: ARM replayed through the game's own arm (decoded=%.1f polarity=%d)", decoded, polarity);
}

// The gamemode's deleteActiveSignal whole, as the host ran it: the desk's reset (its init's polarity the host's), the
// renderer's signal actor, and the signal camera's trigger and field, each peer's own.
void ReplayReset(int32_t polarity) {
    bool called = false;
    int written = 0, unwritten = 0;
    {
        ReplayScope scope(false, 0.f, polarity);
        called = CD::CallDeleteActiveSignal();
        written = g_replay.written;
        unwritten = g_replay.unwritten;
    }
    // The reset zeroes the detection needle, a field the input lane polls: the replay's write is not this peer's input.
    coop::desk_input_sync::PrimeBaselines();
    float d = 0.f;
    int32_t pol = 0;
    const bool read = CD::ReadDownloadProgress(d, pol);
    if (!called || written != 1 || unwritten != 0 || !read || pol != polarity) {
        UE_LOGW("download_arm: RESET replay off -- deleteActiveSignal %s, init written %d time(s) (%d unresolved), "
                "polarity %d against the host's %d", called ? "ran" : "FAILED", written, unwritten, pol, polarity);
        ++g_counts.replayOff;
        return;
    }
    ++g_counts.replayedReset;
    UE_LOGI("download_arm: RESET replayed through the game's own reset (polarity=%d)", polarity);
}

bool CanApplyReceived(const coop::net::DishArmPayload& p) {
    if (!D::EnsureResolved() || !CD::EnsureResolved() || !CD::Instance()) return false;
    if (p.armed) {
        CD::CoordSignal sig;
        // DishArm may overtake the client's local signal-catch materialization.
        // Keep it queued until that actor exists; otherwise the reliable event
        // is consumed and this peer remains permanently unarmed.
        if (!CD::ReadCoordSignal(sig) || sig.objectName.empty() || sig.objectName == L"None") return false;
    }
    return true;
}

void ApplyReceived(const coop::net::DishArmPayload& p) {
    if (p.armed) ReplayArm(p.decoded, p.polarity);
    else ReplayReset(p.polarity);
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    DriveWatches();
}

void Tick() {
    DriveWatches();
    if (ClientSession() && !g_pendingReceives.empty() && CanApplyReceived(g_pendingReceives.front())) {
        ApplyReceived(g_pendingReceives.front());
        g_pendingReceives.pop_front();
    }
    auto* s = HostSession();
    if (!s) return;
    const uint64_t now = NowMs();
    const size_t queued = g_pendingSends.size();
    for (size_t i = 0; i < queued && !g_pendingSends.empty(); ++i) {
        PendingSend pending = g_pendingSends.front();
        g_pendingSends.pop_front();
        if (now < pending.queuedAt || now - pending.queuedAt > 30000) {
            UE_LOGW("download_arm: expired queued %s row for slot %d",
                    pending.payload.armed ? "ARM" : "RESET", pending.slot);
            continue;
        }
        if (pending.slot >= 0 && !s->IsSlotReady(pending.slot)) {
            g_pendingSends.push_back(pending);
            continue;
        }
        const bool sent = pending.slot < 0
            ? s->SendReliable(coop::net::ReliableKind::DishArm, &pending.payload, sizeof(pending.payload))
            : s->SendReliableToSlot(pending.slot, coop::net::ReliableKind::DishArm,
                                    &pending.payload, sizeof(pending.payload));
        if (!sent) {
            g_pendingSends.push_front(pending);
            break;
        }
    }
}

void OnDishArm(const coop::net::DishArmPayload& p, uint8_t senderSlot) {
    if (!ClientSession() || senderSlot != 0) return;
    if (g_pendingReceives.empty() && CanApplyReceived(p)) {
        ApplyReceived(p);
        return;
    }
    if (g_pendingReceives.size() >= kMaxPendingReceives) {
        ++g_counts.dropped;
        UE_LOGE("download_arm: receive queue full; host %s row was dropped",
                p.armed ? "ARM" : "RESET");
        return;
    }
    g_pendingReceives.push_back(p);
    UE_LOGI("download_arm: queued host %s row until the client's %s is ready (%zu pending)",
            p.armed ? "ARM" : "RESET", p.armed ? "caught signal" : "desk", g_pendingReceives.size());
}

void CaptureJoinSnapshot(int peerSlot) {
    if (peerSlot < 1 || peerSlot >= coop::net::kMaxPeers || !HostSession()) return;
    g_captured[peerSlot] = Captured{true, g_resetSerial};
}

void CancelJoinSnapshot(int peerSlot) {
    if (peerSlot >= 0 && peerSlot < coop::net::kMaxPeers) g_captured[peerSlot] = Captured{};
}

void OnPeerLeft(int peerSlot) {
    if (peerSlot < 1 || peerSlot >= coop::net::kMaxPeers) return;
    std::erase_if(g_pendingSends, [peerSlot](const PendingSend& p) { return p.slot == peerSlot; });
}

void QueueConnectResetForSlot(int peerSlot) {
    auto* s = HostSession();
    if (!s || peerSlot < 1 || peerSlot >= coop::net::kMaxPeers) return;
    const Captured c = g_captured[peerSlot];
    g_captured[peerSlot] = Captured{};
    if (c.valid && c.resetSerial == g_resetSerial) return;
    if (!CD::EnsureResolved() || !CD::Instance()) return;
    // A reset since the capture, or a world with no capture of this lane (the canonical save's fallback, whose machine
    // may hold anything): the joiner's machine starts from the game's reset, and the desk's seed follows.
    SendRow(s, false, peerSlot);
    float decoded = 0.f;
    int32_t polarity = 0;
    CD::ReadDownloadProgress(decoded, polarity);
    UE_LOGI("download_arm: connect RESET row -> slot %d (%s; polarity=%d)", peerSlot,
            c.valid ? "reset since the capture" : "no capture of the machine", polarity);
}

void QueueConnectArmForSlot(int peerSlot) {
    auto* s = HostSession();
    if (!s || peerSlot < 1 || peerSlot >= coop::net::kMaxPeers) return;
    if (!CD::EnsureResolved() || !CD::Instance() || !CD::DownloadMeshValid()) return;
    float decoded = 0.f;
    int32_t polarity = 0;
    CD::ReadDownloadProgress(decoded, polarity);
    SendRow(s, true, peerSlot);
    UE_LOGI("download_arm: connect ARM row -> slot %d (decoded=%.1f polarity=%d)", peerSlot, decoded, polarity);
}

void OnDisconnect() {
    if (g_counts.sentArm || g_counts.sentReset || g_counts.replayedArm || g_counts.replayedReset || g_counts.refused ||
        g_counts.dropped || g_counts.replayOff)
        UE_LOGI("download_arm: session end -- sent arm=%llu reset=%llu, replayed arm=%llu reset=%llu, refused=%llu, "
                "load-native=%llu, dropped=%llu, off=%llu",
                static_cast<unsigned long long>(g_counts.sentArm), static_cast<unsigned long long>(g_counts.sentReset),
                static_cast<unsigned long long>(g_counts.replayedArm),
                static_cast<unsigned long long>(g_counts.replayedReset),
                static_cast<unsigned long long>(g_counts.refused), static_cast<unsigned long long>(g_counts.loadNative),
                static_cast<unsigned long long>(g_counts.dropped), static_cast<unsigned long long>(g_counts.replayOff));
    g_counts = Counts{};
    for (auto& c : g_captured) c = Captured{};
    g_replay = Replay{};
    g_resetSerial = 0;
    g_pendingSends.clear();
    g_pendingReceives.clear();
}

Counts LaneCounts() { return g_counts; }

}  // namespace coop::download_arm_sync
