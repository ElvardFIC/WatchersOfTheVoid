// coop/interactables/sleepingbag_sync.cpp -- see coop/interactables/sleepingbag_sync.h.

#include "coop/interactables/sleepingbag_sync.h"

#include "coop/element/intent_authority.h"
#include "coop/element/registry.h"
#include "coop/net/intent_bucket.h"
#include "coop/net/intent_queue.h"
#include "coop/net/session.h"
#include "coop/player/roster_ledger.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/props/prop_hold.h"

#include "ue_wrap/actors/sleepingbag.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <chrono>
#include <utility>

namespace coop::sleepingbag_sync {
namespace {

namespace EL = coop::element;
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
namespace SB = ue_wrap::sleepingbag;

constexpr float kReachUU = 450.0f;
constexpr coop::net::IntentBudget kBudget{4.0f, 4.0f};
constexpr size_t kMaxPending = 8;
// The enum_interactionActions value each form's transition press carries: the packed bag's only
// action (unroll), the deployed bag's second (fold, which its graph calls fold() for) and the bunk
// mattress's second (take it off).
constexpr uint8_t kActionUnroll = 4;
constexpr uint8_t kActionFold = 7;
constexpr uint8_t kActionTakeMattress = 7;

UE_FACT_FN(kRowAction, "sleepingbag_sync", nullptr, L"actionOptionIndex", AnyClass);
constexpr int kTagAction = 0x53424143;  // 'SBAC'

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_actionWatch = false;
bool g_saidLive = false;
uint64_t g_sent = 0;
uint64_t g_ran = 0;
uint64_t g_denied = 0;

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

struct Slot {
    coop::net::IntentQueue<coop::net::SleepingbagIntentPayload> queue;
    uint64_t nextSayMs = 0;
};
coop::roster_ledger::PerSlotState<Slot> g_slots;

bool IsClient(coop::net::Session* s) {
    return s && s->running() && s->connected() && s->role() == coop::net::Role::Client;
}

bool SendIntent(coop::net::Session* s, void* actor, uint8_t verb, uint8_t action) {
    const EL::ElementId eid = EL::Registry::Get().EidForActor(actor);
    if (eid == EL::kInvalidId) {
        UE_LOGW("sleepingbag_sync: client transition not sent -- the sleepingbag source has no host prop id; "
                "the local Blueprint verb runs");
        return false;
    }
    coop::net::SleepingbagIntentPayload p{};
    p.elementId = static_cast<uint32_t>(eid);
    p.verb = verb;
    p.action = action;
    if (!s->SendReliableToSlot(0, coop::net::ReliableKind::SleepingbagIntent, &p, sizeof(p))) {
        UE_LOGW("sleepingbag_sync: transition for eid=%u was not sent -- the local Blueprint verb runs",
                p.elementId);
        return false;
    }
    ++g_sent;
    UE_LOGI("sleepingbag_sync: CLIENT sent %s for prop eid=%u (#%llu)",
            verb == coop::net::sleepingbag_intent::kFold ? "fold" : "unfold", p.elementId,
            static_cast<unsigned long long>(g_sent));
    return true;
}

// Which verb a press of `action` on `actor` is, or 0 when it is none of the three transitions: the packed bag's
// unroll (4), the deployed bag's fold (7) and the bunk mattress's take-off (7). Every other action is the form's
// own native business (the deployed bag's 4 is sleeping).
uint8_t VerbOf(void* actor, uint8_t action) {
    if (action == kActionUnroll && SB::IsPacked(actor)) return coop::net::sleepingbag_intent::kUnwrap;
    if (action == kActionFold && SB::IsDeployed(actor)) return coop::net::sleepingbag_intent::kFold;
    if (action == kActionTakeMattress && SB::IsBunkMattress(actor)) return coop::net::sleepingbag_intent::kUnwrap;
    return 0;
}

// The press, not the function behind it: the deployed bag's fold() also runs from its own BeginPlay when its
// foundation traces fail, and that run is no player's. The body is cancelled only when the host was asked; a refused
// send runs it as a press would with no lane.
SG::Verdict OnActionPre(const SG::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!IsClient(s)) return SG::Verdict::Run;

    const int32_t playerOff = R::FindParamOffset(call.function, L"Player");
    const int32_t actionOff = R::FindParamOffset(call.function, L"Action");
    if (playerOff < 0 || actionOff < 0 || !call.locals) return SG::Verdict::Run;
    const uint8_t action = call.locals[actionOff];
    const uint8_t verb = VerbOf(call.object, action);
    if (verb == 0) return SG::Verdict::Run;
    void* player = *reinterpret_cast<void* const*>(call.locals + playerOff);
    if (!coop::players::Registry::Get().IsLocal(player)) return SG::Verdict::Run;
    return SendIntent(s, call.object, verb, action) ? SG::Verdict::Cancel : SG::Verdict::Run;
}

// A missing prop or first puppet pose is a bounded wait. All other judgments consume the queued
// action, so an invalid eid cannot hold later verbs for that sender.
bool Execute(coop::net::Session& session, const coop::net::SleepingbagIntentPayload& p, uint8_t slot) {
    const auto token = EL::IntentTarget::ForClientIntent(session, slot, kReachUU);
    const EL::IntentSubject subject = token.Resolve(static_cast<EL::ElementId>(p.elementId), EL::ElementType::Prop);
    if (subject.outcome == EL::IntentOutcome::NoBody) return false;
    if (!subject) {
        ++g_denied;
        UE_LOGI("sleepingbag_sync: DENY slot=%u verb=%u eid=%u -- %s", static_cast<unsigned>(slot),
                static_cast<unsigned>(p.verb), p.elementId, EL::OutcomeName(subject.outcome));
        return true;
    }

    // Another peer's hand on the source: the verb would destroy a prop under its holder.
    if (const coop::prop_hold::Holder h = coop::prop_hold::WhoHolds(subject.actor, slot);
        h != coop::prop_hold::Holder::None) {
        ++g_denied;
        UE_LOGI("sleepingbag_sync: DENY slot=%u verb=%u eid=%u -- %s", static_cast<unsigned>(slot),
                static_cast<unsigned>(p.verb), p.elementId, coop::prop_hold::HolderWhy(h));
        return true;
    }

    bool ran = false;
    // The form and the action byte must be the pair a press of that form carries.
    if (VerbOf(subject.actor, p.action) == p.verb) {
        if (p.verb == coop::net::sleepingbag_intent::kUnwrap) {
            auto* puppet = coop::players::Registry::Get().Puppet(slot);
            void* player = (puppet && puppet->valid()) ? puppet->GetActor() : nullptr;
            ran = player && SB::CallUnwrap(subject.actor, player, p.action);
        } else {
            ran = SB::CallFold(subject.actor);
        }
    }
    if (!ran) {
        ++g_denied;
        UE_LOGI("sleepingbag_sync: DENY slot=%u verb=%u eid=%u -- source form mismatch or Blueprint dispatch failed",
                static_cast<unsigned>(slot), static_cast<unsigned>(p.verb), p.elementId);
        return true;
    }
    ++g_ran;
    UE_LOGI("sleepingbag_sync: HOST ran %s for slot %u on prop eid=%u (#%llu)",
            p.verb == coop::net::sleepingbag_intent::kFold ? "fold" : "unfold",
            static_cast<unsigned>(slot), p.elementId, static_cast<unsigned long long>(g_ran));
    return true;
}

void RegisterWatches() {
    if (!g_actionWatch)
        g_actionWatch = SG::WatchName(kRowAction, kTagAction, &OnActionPre, nullptr);
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    RegisterWatches();
}

void Tick(coop::net::Session& session) {
    RegisterWatches();
    if (!g_saidLive) {
        SG::ResolvePendingNames();
        if (g_actionWatch && SG::NameWatchLive(kRowAction, kTagAction)) {
            g_saidLive = true;
            UE_LOGI("sleepingbag_sync: the bag's and the bunk mattress's action presses are watched");
        }
    }
    if (!session.running() || session.role() != coop::net::Role::Host) return;
    for (uint8_t slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        Slot& state = g_slots[slot];
        state.queue.Serve(kBudget, NowMs, [&](const coop::net::SleepingbagIntentPayload& p) {
            if (!Execute(session, p, slot)) return coop::net::Served::Wait;
            return coop::net::Served::Done;
        });
    }
}

void OnIntent(coop::net::Session& session, const coop::net::SleepingbagIntentPayload& payload,
              uint8_t senderSlot) {
    if (session.role() != coop::net::Role::Host || senderSlot < 1 || senderSlot >= coop::net::kMaxPeers ||
        (payload.verb != coop::net::sleepingbag_intent::kUnwrap &&
         payload.verb != coop::net::sleepingbag_intent::kFold) || payload.reserved[0] || payload.reserved[1])
        return;
    auto& state = g_slots[senderSlot];
    auto copy = payload;
    if (!state.queue.Push(std::move(copy), kMaxPending)) {
        const uint64_t now = NowMs();
        if (now >= state.nextSayMs) {
            state.nextSayMs = now + 10000;
            UE_LOGW("sleepingbag_sync: slot %u intent queue full (%zu) -- refusing transitions until it drains",
                    static_cast<unsigned>(senderSlot), state.queue.size());
        }
        ++g_denied;
    }
}

void OnDisconnect() {
    if (g_sent || g_ran || g_denied)
        UE_LOGI("sleepingbag_sync: session end -- sent=%llu host-run=%llu denied=%llu",
                static_cast<unsigned long long>(g_sent), static_cast<unsigned long long>(g_ran),
                static_cast<unsigned long long>(g_denied));
    g_sent = g_ran = g_denied = 0;
}

}  // namespace coop::sleepingbag_sync
