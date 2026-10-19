// coop/interactables/drone_call_intent.cpp -- see coop/interactables/drone_call_intent.h.

#include "coop/interactables/drone_call_intent.h"

#include "coop/element/intent_authority.h"
#include "coop/net/intent_bucket.h"
#include "coop/net/intent_queue.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/roster_ledger.h"  // PerSlotState
#include "coop/props/active_drive.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/devices/drone.h"
#include "coop/session/net_pump.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/drone_console.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <cstring>
#include <iterator>
#include <array>

namespace coop::drone_call_intent {
namespace {

namespace D  = ue_wrap::drone_console;
namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;

// The verb the console's buttons arrive on, watched on the console's own class by name, so the watch
// matches whichever class the running world loads under that name: a level class is never kept by
// pointer across worlds.
UE_FACT_FN(kRowActionOptionIndex, "drone_call_intent", L"droneConsole_C", L"actionOptionIndex", Declared);
constexpr int kTagDroneCall = 0x44524f4e;  // 'DRON'

// The action the console's own switch routes to its faces. Its other cases, 10 and 11, are the
// lid, which is already a synced door and must keep running locally.
constexpr uint8_t kActionUse = 4;

// A console press is an arm's-length interaction, so the sender's reach is the desk's rather than
// the pile grab's: the console is a small box a player stands at.
constexpr float kConsoleReachUU = 400.0f;

// One press is one call. The drone's own body ignores a press it cannot honour, so a burst costs
// nothing but the dispatch; the bound is here so a stalled client's backlog neither stalls a frame
// nor arrives refused.
constexpr coop::net::IntentBudget kPressBudget{2.0f, 1.0f};
constexpr size_t   kMaxPending     = 4;
constexpr uint64_t kRefusalSayMs   = 10000;

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_watchInstalled = false;
bool g_watchLive = false;

uint64_t g_sent = 0, g_flown = 0, g_denied = 0;

// One sender: its queued presses with their bucket, and the latches that clear with its occupant.
struct Slot {
    coop::net::IntentQueue<coop::net::DroneFlyIntentPayload> queue;
    uint64_t nextSayMs = 0;      // the full queue's refusal, said once in kRefusalSayMs
    bool     waitSaid  = false;  // a wait for the body, said once a streak
};
Bucket g_rate[coop::net::kMaxPeers];
struct Request { coop::net::DroneFlyIntentPayload payload; uint64_t at; };
std::deque<Request> g_pending[coop::net::kMaxPeers];
uint64_t g_epoch = 0, g_occupancy[coop::net::kMaxPeers]{};
uint32_t g_world = 0;
bool g_ticking = false;
void World() {
    auto world = ue_wrap::world_identity::Generation();
    if (g_world == world) return;
    ++g_epoch; g_world = world;
    for (auto& q : g_pending) q.clear();
    for (auto& b : g_rate) b = {};
}
bool Current(uint64_t epoch, uint64_t occupant, uint8_t slot) {
    auto* s = g_session.load();
    return epoch == g_epoch && occupant == g_occupancy[slot] &&
           g_world == ue_wrap::world_identity::Generation() && s && s->connected();
}

bool TakeToken(uint8_t slot) {
    Bucket& b = g_rate[slot];
    const uint64_t now = coop::active_drive::NowMs();
    if (b.lastMs != 0 && now > b.lastMs) {
        b.tokens += kPressPerSecond * static_cast<float>(now - b.lastMs) / 1000.0f;
        if (b.tokens > kPressBurst) b.tokens = kPressBurst;
    }
    b.lastMs = now;
    if (b.tokens < 1.0f) return false;
    b.tokens -= 1.0f;
    return true;
}

// ---- host side -------------------------------------------------------------------------------
// One press, run or refused and then consumed, or left at the head of its queue: false while the host
// has no body for the sender, the seconds between a joiner's curtain and its first pose. That is a
// mid-join window rather than a verdict, which the door-verb, keypad and container lanes wait out the
// same way (principle 8): a press made in it is not lost.
bool Execute(coop::net::Session& s, const coop::net::DroneFlyIntentPayload& p, uint8_t slot) {
    const auto epoch = g_epoch, occupant = g_occupancy[slot];
    // The console carries no identity to name, so the sender's reach IS the resolve: of this
    // world's consoles, the one the sender is standing at. Any of them calls the same drone, so
    // the first within reach is the press.
    const auto token = coop::element::IntentTarget::ForClientIntent(s, slot, kConsoleReachUU);
    if (!Current(epoch, occupant, slot)) return true;
    if (!token.HasBody()) {
        if (!g_slots[slot].waitSaid) {
            g_slots[slot].waitSaid = true;
            UE_LOGI("[DRONE-CALL] slot %u's presses wait: the host has no body for it yet",
                    static_cast<unsigned>(slot));
        }
        return false;
    }
    void* consoles[4];
    const int32_t nc = D::LiveConsoles(consoles, static_cast<int32_t>(std::size(consoles)));
    std::array<ue_wrap::CachedObjRef, 4> candidates;
    for (int32_t i = 0; i < nc; ++i) candidates[i].Set(consoles[i]);
    void* console = nullptr;
    for (int32_t i = 0; i < nc && !console; ++i) {
        const auto ref = candidates[i];
        void* candidate = ref.Get();
        if (!candidate) continue;
        const bool ok = token.Authorize(candidate).outcome == coop::element::IntentOutcome::Ok;
        if (!Current(epoch, occupant, slot)) return true;
        if (ok) console = ref.Get();
    }
    if (!console) {
        ++g_denied;
        UE_LOGI("[DRONE-CALL] DENY slot=%u -- none of the %d live console(s) is within reach",
                static_cast<unsigned>(slot), nc);
        return true;
    }
    if (p.verb == 1) {
        // The native timer face is usable with the lid closed. It changes only this field;
        // the host's existing timer/flight graph owns when the drone actually departs.
        return ue_wrap::drone::WriteDeparturePreference(ue_wrap::drone::Find(), p.leaveAfter5min != 0);
    }
    // The lid is the console's own gate on its keyboard, and it is shared state the box lane
    // (door_box) already carries, so the host reads its own copy rather than trusting the press.
    if (!D::IsLidOpen(console)) {
        ++g_denied;
        UE_LOGI("[DRONE-CALL] DENY slot=%u -- the console's lid is shut here",
                static_cast<unsigned>(slot));
        return true;
    }
    const bool flew = D::TriggerFly(console);
    if (!Current(epoch, occupant, slot)) return true;
    if (!flew) {
        ++g_denied;
        UE_LOGW("[DRONE-CALL] slot=%u -- triggerFly did not dispatch (no drone reference or the "
                "verb did not resolve)", static_cast<unsigned>(slot));
        return true;
    }
    ++g_flown;
    UE_LOGI("[DRONE-CALL] PRESSED console %p for slot=%u (#%llu) -- the drone's own body owns the "
            "outcome", console, static_cast<unsigned>(slot),
            static_cast<unsigned long long>(g_flown));
    return true;
}

// ---- the client's gate -----------------------------------------------------------------------
sg::Verdict OnActionPre(const sg::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Client || !call.object) return sg::Verdict::Run;

    if (!coop::net_pump::IsInAnnouncedWorld(call.object)) return sg::Verdict::Run;
    static R::ParamOffset action{L"action"}, player{L"player"};
    const int sActionOff = action.Of(call.function), playerOff = player.Of(call.function);
    if (!call.locals || sActionOff < 0 || playerOff < 0) return sg::Verdict::Cancel;
    void* who = nullptr; std::memcpy(&who, call.locals + playerOff, sizeof(who));
    if (who != coop::players::Registry::Get().Local()) return sg::Verdict::Cancel;
    if (*reinterpret_cast<uint8_t*>(call.locals + sActionOff) != kActionUse)
        return sg::Verdict::Run;  // the lid's cases stay the game's, on every peer

    // Native priority: timer face before keyboard. Do not replay a local timer toggle.
    const bool departure = D::IsCursorOnDeparture(call.object);
    if (!departure && !D::IsCursorOnKeyboard(call.object)) return sg::Verdict::Run;

    coop::net::DroneFlyIntentPayload p{};
    p.verb = departure ? 1 : 0;
    if (departure) {
        bool leave = false;
        if (!ue_wrap::drone::ReadDeparturePreference(ue_wrap::drone::Find(), leave)) return sg::Verdict::Cancel;
        p.leaveAfter5min = !leave;
    }
    World();
    if (g_pending[0].size() < kMaxPending) g_pending[0].push_back({p, coop::active_drive::NowMs()});
    else {
        UE_LOGW("[DRONE-CALL] console request queue full; the drone keeps the host's state");
        return sg::Verdict::Cancel;
    }
    return sg::Verdict::Cancel;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    World();
    if (!g_watchInstalled)
        g_watchInstalled = sg::WatchName(kRowActionOptionIndex, kTagDroneCall, &OnActionPre, nullptr);
}

void Tick(coop::net::Session& session) {
    sg::ResolvePendingNames();
    if (!g_watchInstalled) {
        g_watchInstalled = sg::WatchName(kRowActionOptionIndex, kTagDroneCall, &OnActionPre, nullptr);
        return;
    }
    if (!g_watchLive && sg::NameWatchLive(kRowActionOptionIndex, kTagDroneCall)) {
        g_watchLive = true;
        UE_LOGI("[DRONE-CALL] the console action gate is live");
    }
    if (!session.connected() || g_ticking) return;
    World();
    struct Guard { Guard(){g_ticking=true;} ~Guard(){g_ticking=false;} } guard;
    if (session.role() != coop::net::Role::Host) {
        auto& q = g_pending[0];
        if (q.empty()) return;
        const auto r = q.front();
        if (coop::active_drive::NowMs() - r.at > 3000) { q.pop_front(); return; }
        if (session.SendReliable(coop::net::ReliableKind::DroneFlyIntent, &r.payload, sizeof(r.payload))) {
            q.pop_front();
            if (r.payload.verb == 0) ++g_sent;
        }
        return;
    }
    for (uint8_t slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        if (g_pending[slot].empty() || !TakeToken(slot)) continue;
        const Request r = g_pending[slot].front();
        g_pending[slot].pop_front(); // never retain a deque reference across native callbacks
        if (coop::active_drive::NowMs() - r.at > 3000) continue;
        const auto epoch = g_epoch, occupant = g_occupancy[slot];
        const bool consumed = Execute(session, r.payload, slot);
        if (!Current(epoch, occupant, slot)) return;
        if (consumed) {
            g_waitSaid[slot] = false;        // a consumed press ends the wait's streak
        } else {
            g_pending[slot].push_front(r);
            if (g_pending[slot].size() > kMaxPending) g_pending[slot].pop_back();
            g_rate[slot].tokens += 1.0f;     // a wait runs nothing, so it spends no token
        }
    }
}

void OnDroneFlyIntent(coop::net::Session& session, const coop::net::DroneFlyIntentPayload& payload,
                      uint8_t senderSlot) {
    if (session.role() != coop::net::Role::Host || !session.connected() || payload.verb > 1 ||
        (payload.verb == 1 && payload.leaveAfter5min > 1)) return;
    World();
    if (senderSlot < 1 || senderSlot >= coop::net::kMaxPeers) return;
    Slot& sl = g_slots[senderSlot];
    coop::net::DroneFlyIntentPayload copy = payload;
    if (!sl.queue.Push(std::move(copy), kMaxPending)) {
        const uint64_t now = coop::active_drive::NowMs();
        if (now >= sl.nextSayMs) {
            sl.nextSayMs = now + kRefusalSayMs;
            UE_LOGW("[DRONE-CALL] slot %u queue full (%zu) -- refusing presses until it drains",
                    static_cast<unsigned>(senderSlot), sl.queue.size());
        }
        ++g_denied;
    }
    q.push_back({payload, coop::active_drive::NowMs()});
}

void OnPeerLeft(uint8_t slot) {
    if (slot >= coop::net::kMaxPeers) return;
    ++g_occupancy[slot];
    g_pending[slot].clear();
    g_rate[slot] = Bucket{};
    g_waitSaid[slot] = false;
}

uint64_t SentCount() { return g_sent; }

void OnDisconnect() {
    ++g_epoch; g_session.store(nullptr);
    if (g_sent || g_flown || g_denied)
        UE_LOGI("[DRONE-CALL] session end -- sent=%llu pressed=%llu denied=%llu",
                static_cast<unsigned long long>(g_sent), static_cast<unsigned long long>(g_flown),
                static_cast<unsigned long long>(g_denied));
    g_sent = g_flown = g_denied = 0;
}

}  // namespace coop::drone_call_intent
