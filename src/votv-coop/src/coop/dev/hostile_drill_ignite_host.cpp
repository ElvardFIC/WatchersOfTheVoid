// coop/dev/hostile_drill_ignite_host.cpp -- the ignite family's judge: see coop/dev/hostile_drill_ignite.h.
// The host spawns the targets before the client's puppet, says it is ready in the lobby chat once it holds a posed
// puppet and the hand state the arm needs for slot 1, and then judges by accounting: every message is exactly one
// performed light or one refusal, so the counter deltas since the baseline sum to the messages sent and each reason
// matches the arm's table.

#include "hostile_drill_ignite_detail.h"  // co-located private header (src tree, not include/)

#include "hostile_drill_act_detail.h"  // SpawnOnFloor (co-located private header of the act family)

#include "coop/dev/drill_chat.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/puppet_drive.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_snapshot.h"
#include "coop/save/join_window_baseline.h"

#include "ue_wrap/core/log.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace coop::dev::hostile_drill::ignite::detail {
namespace {

namespace DC  = coop::dev::drill_chat;
namespace SRI = coop::salt_rocket_ignite;
using Refusal = SRI::Refusal;
constexpr size_t kReasons = static_cast<size_t>(Refusal::Count);

enum class HStep { Begin, WaitReady, Judge, Done };
HStep             g_step = HStep::Begin;
SRI::Counters     g_base{};
Clock::time_point g_armAt{}, g_judgeAt{};
bool              g_spawned = false, g_clientReady = false;
const char*       g_waiting = "slot 1's world-ready";
uint16_t          g_playerNo = 0;    // slot 1's occupant when the host said ready

void Finish(const Spec& sp, const char* what, const std::string& why) {
    Verdict(sp.arm, what, why);
    g_step = HStep::Done;
}

// ---- the host's half before the ready line ----

// Slot 1 is world-ready with its bracket closed and a posed puppet: the host can place the sender.
bool PuppetReady(coop::net::Session* s) {
    if (!s->IsSlotWorldReady(kSlot)) { g_waiting = "slot 1 to be world-ready"; return false; }
    if (!coop::prop_snapshot::IsBracketClosed(kSlot) || coop::join_window_baseline::IsLateWindowOpen(kSlot)) {
        g_waiting = "slot 1's join window to close";
        return false;
    }
    coop::RemotePlayer& rp = coop::puppet_drive::Puppet(kSlot);
    if (!rp.valid() || !rp.HasPose()) { g_waiting = "a posed puppet for slot 1"; return false; }
    return true;
}

// The targets round the client's puppet: the near rocket, the far rocket (past the 400 reach) and the world lighter.
// True once all three stand; an arm that cannot place one ends INCONCLUSIVE.
bool SpawnTargets(const Spec& sp) {
    coop::RemotePlayer& rp = coop::puppet_drive::Puppet(kSlot);
    namespace A = coop::dev::hostile_drill::act::detail;
    void* spawned = nullptr;
    if (const char* why = A::SpawnOnFloor(rp, kRocketPath, false, 0, nullptr, &spawned)) {
        Finish(sp, "INCONCLUSIVE", std::string("the near rocket: ") + why);
        return false;
    }
    if (const char* why = A::SpawnOnFloor(rp, kRocketPath, true, 0, nullptr, &spawned)) {
        Finish(sp, "INCONCLUSIVE", std::string("the far rocket: ") + why);
        return false;
    }
    if (const char* why = A::SpawnOnFloor(rp, kLighterPath, false, 2, nullptr, &spawned)) {
        Finish(sp, "INCONCLUSIVE", std::string("the world lighter: ") + why);
        return false;
    }
    return true;
}

// The state of slot 1's hand the arm needs: the tool named held, or an empty hand.
bool HandReady(const Spec& sp) {
    if (sp.hand == Hand::Empty) return !coop::hand_item::HoldsItem(kSlot);
    return coop::hand_item::HeldClassIs(kSlot, HandClass(sp.hand));
}

void WaitReady(coop::net::Session* s, const Spec& sp) {
    if (Clock::now() - g_armAt > kFailBound) {
        Finish(sp, "INCONCLUSIVE", std::string("the precondition never held: waiting for ") + g_waiting);
        return;
    }
    // The client's line, latched on sight from the arm's first tick: a live chat row retires after seconds.
    if (!g_clientReady && DC::PollDue()) g_clientReady = DC::ChatHasLive(ClientReadyLine(sp.arm));
    if (!PuppetReady(s)) return;
    if (!g_spawned) {
        // The client records the rockets and lighters it already holds, then says so: a target born earlier would
        // sit in that set.
        if (!g_clientReady) { g_waiting = "the client's ready line"; return; }
        if (!SpawnTargets(sp)) return;
        g_spawned = true;
    }
    if (!HandReady(sp)) { g_waiting = "slot 1's hand state"; return; }
    g_base = SRI::GetCounters();
    g_playerNo = coop::roster_ledger::Get(kSlot).playerNo;
    DC::Say(ReadyLine(sp.arm));
    g_judgeAt = Clock::now();
    g_step = HStep::Judge;
    UE_LOGI("[HOSTILE-DRILL] %s armed on slot %u (the targets stand, the hand state is in)", sp.name,
            static_cast<unsigned>(kSlot));
}

// ---- the judge ----

struct Delta {
    uint64_t performed[2] = {}, refused[kReasons] = {}, sum = 0;
};

Delta Diff(const SRI::Counters& now) {
    Delta d;
    for (int k = 0; k < 2; ++k) {
        d.performed[k] = now.performed[k] - g_base.performed[k];
        d.sum += d.performed[k];
    }
    for (size_t r = 0; r < kReasons; ++r) {
        d.refused[r] = now.refused[r] - g_base.refused[r];
        d.sum += d.refused[r];
    }
    return d;
}

std::string Summary(const Delta& d) {
    std::string out = "performed " + std::to_string(d.performed[0]) + "/" + std::to_string(d.performed[1]) +
                      ", refused";
    bool any = false;
    for (size_t r = 0; r < kReasons; ++r) {
        if (!d.refused[r]) continue;
        out += std::string(" ") + SRI::RefusalName(static_cast<Refusal>(r)) + "=" + std::to_string(d.refused[r]);
        any = true;
    }
    return any ? out : out + " none";
}

// What does not match the arm's table, or empty. The flood's table is its own shape: queue_full and not_rocket each at
// least 1 (the split depends on when the messages arrive), everything else 0.
std::string Mismatch(const Spec& sp, const Delta& d) {
    const size_t want = static_cast<size_t>(sp.refusal);
    const bool flood = sp.mode == Mode::Flood;
    std::string bad;
    for (size_t r = 0; r < kReasons; ++r) {
        const bool floodReason = flood && (r == static_cast<size_t>(Refusal::QueueFull) ||
                                           r == static_cast<size_t>(Refusal::NotRocket));
        const uint64_t w = !flood && r == want ? sp.refused : 0;
        const char* name = SRI::RefusalName(static_cast<Refusal>(r));
        if (floodReason) {
            if (d.refused[r] < 1) bad += std::string(" ") + name + " is 0 (the flood never reached it);";
        } else if (d.refused[r] != w) {
            bad += std::string(" ") + name + " " + std::to_string(d.refused[r]) + " (wanted " +
                   std::to_string(w) + ");";
        }
    }
    for (int k = 0; k < 2; ++k)
        if (d.performed[k] != sp.performed[k])
            bad += " performed[" + std::to_string(k) + "] " + std::to_string(d.performed[k]) + " (wanted " +
                   std::to_string(sp.performed[k]) + ");";
    return bad;
}

void Judge(coop::net::Session* s, const Spec& sp) {
    const Delta d = Diff(SRI::GetCounters());
    // The per-slot state resets when the slot's occupant changes, so a replaced occupant is not judged on it.
    if (sp.mode != Mode::NotReady) {
        if (!s->HasPeerConn(kSlot)) return Finish(sp, "INCONCLUSIVE", "the judged slot left before the verdict");
        if (coop::roster_ledger::Get(kSlot).playerNo != g_playerNo)
            return Finish(sp, "INCONCLUSIVE", "slot-replaced (the judged slot's occupant changed)");
    }
    if (d.sum > sp.sent)
        return Finish(sp, "FAIL", "more outcomes than messages (" + std::to_string(sp.sent) + " sent): " + Summary(d));
    if (d.sum < sp.sent) {
        if (Clock::now() - g_judgeAt <= kFailBound) return;
        if (sp.mode == Mode::NotReady)
            return Finish(sp, "FAIL", "the light sent before the announce was never refused: " + Summary(d));
        return Finish(sp, "FAIL", "timeout, " + std::to_string(d.sum) + " of " + std::to_string(sp.sent) +
                                      " accounted: " + Summary(d));
    }
    const std::string bad = Mismatch(sp, d);
    if (bad.empty())
        Finish(sp, "PASS", "all " + std::to_string(sp.sent) + " messages accounted, as the arm's table: " + Summary(d));
    else
        Finish(sp, "FAIL", "the deltas differ from the arm's table:" + bad + " got " + Summary(d));
}

}  // namespace

void HostTick(coop::net::Session* s, const Spec& sp) {
    if (g_step == HStep::Done) return;
    if (g_step == HStep::Begin) {
        // The not_ready arm's baseline is the session's start: it has no ready line and no slot yet.
        g_base = SRI::GetCounters();
        g_armAt = Clock::now();
        if (sp.mode == Mode::NotReady) {
            g_judgeAt = g_armAt;
            g_step = HStep::Judge;
            UE_LOGI("[HOSTILE-DRILL] %s armed at the session's start (the client's announce is held)", sp.name);
        } else {
            g_step = HStep::WaitReady;
        }
    }
    if (g_step == HStep::WaitReady) WaitReady(s, sp);
    else if (g_step == HStep::Judge) Judge(s, sp);
}

void HostReset() {
    g_step = HStep::Begin;
    g_base = SRI::Counters{};
    g_armAt = g_judgeAt = {};
    g_spawned = g_clientReady = false;
    g_waiting = "slot 1's world-ready";
    g_playerNo = 0;
}

}  // namespace coop::dev::hostile_drill::ignite::detail
