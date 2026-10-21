// coop/items/coingun_collect.cpp -- the collect lane: somebody picks a coin up, and the host
// credits. Two entries reach the coin's credit block: the overlap delegate
// (ProcessEvent-visible, hooked and cancellable) and actionOptionIndex, the E-press, which
// mainPlayer dispatches as EX_LocalVirtualFunction (invisible to ProcessEvent and to a Func
// hook; the script-body gate sees it, and this lane observes it there). So the shape is
// forward-and-authority: the client forwards the coin's identity and cancels its local mirror
// entry; the host runs the coin's own verb on the authoritative instance. Game thread throughout.

#include "coop/items/coingun_sync.h"

#include "coingun_internal.h"   // co-located private header (src tree, not include/)

#include "coop/element/intent_authority.h"   // may this sender name this coin
#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/player/players_registry.h"   // IsLocal, IsPuppet: who tripped the coin
#include "coop/player/roster_ledger.h"   // PerSlotState: throttles that leave with their sender
#include "coop/net/session.h"
#include "coop/world/world_actor_sync.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/economy.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace coop::coingun_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace sg = ue_wrap::script_gate;
namespace I  = coop::coingun_sync::internal;

// The one option baocoin_C::getActionOptions offers, a compile-time constant; actionOptionIndex
// never branches on `action`, so deriving it at runtime would cost a dispatch and a leaked
// engine array per collect for a value the callee never reads.
constexpr uint8_t kCoinCollectAction = 7;

std::atomic<bool> g_installed{false};
std::atomic<bool> g_verbRegistered{false};

UE_FACT_FN(kRowActionOptionIndex, "coingun_collect", nullptr, L"actionOptionIndex", AnyClass);
// Abaocoin_C's collect BndEvt (the PE-visible overlap pickup)
UE_FACT_FN(kRowCoinCollectOverlap, "coingun_collect", L"baocoin_C",
           L"BndEvt__baocoin_collect_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);

void* g_actionOptFn = nullptr;  // baocoin_C::actionOptionIndex -- the host's collect executor

// Per-entry counters, split by role so two numbers printed together count the same population:
// which of the two entries do players use?
std::atomic<unsigned long long> g_seenPressHost{0};      // the E-press entry at the gate, host side
std::atomic<unsigned long long> g_seenPressClient{0};    //   ... client side
std::atomic<unsigned long long> g_seenOverlapHost{0};    // the BndEvt overlap entry, host side
std::atomic<unsigned long long> g_seenOverlapClient{0};  //   ... client side
std::atomic<unsigned long long> g_forwarded{0};
std::atomic<unsigned long long> g_performed{0};
std::atomic<unsigned long long> g_unresolved{0};
std::atomic<unsigned long long> g_noCredit{0};
std::atomic<unsigned long long> g_replayed{0};

// The consumption guard. "The coin will be dead next time" holds only if the credit block's
// K2_DestroyActor takes effect within our dispatch, and that timing is not measured; the
// reliable inbox drains unbounded per tick, so two CoinCollect packets naming one eid in a frame
// would credit twice. Keyed by actor index, valued by a world-stamped CachedObjRef, so the guard
// compares identity and an entry reads dead once the coin or its world is gone.
std::unordered_map<uint32_t, ue_wrap::CachedObjRef> g_collected;
std::unordered_map<uint32_t, ue_wrap::CachedObjRef> g_retiringCoins;
uint64_t g_collectEpoch = 0;
uint64_t g_retireAt = 0;
uint32_t g_retireCursor = 0;

bool CollectAuthoritative(void* coin, void* player) {
    auto* session = I::Session();
    if (!session || !session->connected() || session->role() != coop::net::Role::Host || !I::IsCoinActor(coin)) return false;
    const auto epoch = g_collectEpoch;
    const auto world = ue_wrap::world_identity::Generation();
    ue_wrap::CachedObjRef ref; ref.Set(coin);
    const int index = R::InternalIndexOf(coin);
    if (index < 0) return false;
    const auto key = uint32_t(index);
    if (auto it = g_collected.find(key); it != g_collected.end() && it->second.Get() == coin) {
        ++g_replayed; return false;
    }
    std::erase_if(g_collected, [](const auto& row) { return !row.second.Get(); });
    if (g_collected.size() >= 256) return false;
    if (!g_actionOptFn) g_actionOptFn = R::FindFunction(R::ClassOf(coin), kVerbNameCollect);
    ue_wrap::ParamFrame frame(g_actionOptFn);
    if (!frame.valid() || !frame.Set<void*>(L"player", player) ||
        !frame.Set<uint8_t>(L"action", kCoinCollectAction)) return false;
    int32_t before = 0;
    if (!ue_wrap::economy::ReadPoints(&before) || !ref.Get() || epoch != g_collectEpoch ||
        world != ue_wrap::world_identity::Generation() || I::Session() != session || !session->connected() ||
        session->role() != coop::net::Role::Host) return false;
    // Reserve before native dispatch: a nested overlap/request can run before
    // K2_DestroyActor. Slot serials distinguish a later coin at the same address.
    g_collected[key] = ref;
    const bool dispatched = ue_wrap::Call(coin, frame);
    if (epoch != g_collectEpoch || world != ue_wrap::world_identity::Generation() || I::Session() != session ||
        !session->connected() || session->role() != coop::net::Role::Host) return false;
    int32_t after = 0;
    const bool readable = ue_wrap::economy::ReadPoints(&after);
    if (epoch != g_collectEpoch || world != ue_wrap::world_identity::Generation() || I::Session() != session ||
        !session->connected() || session->role() != coop::net::Role::Host) return false;
    if (readable && after > before) {
        // A reflected call can fault after addPoints and before self-destroy.
        // The credit is committed even if dispatch reports false.
        ++g_performed;
        if (ref.Get()) {
            g_retiringCoins[key] = ref;
            ue_wrap::engine::DestroyActor(coin);
        }
        return true;
    }
    ++g_noCredit;
    // Unknown completion remains reserved: never spend the same coin twice.
    // A proven no-op on the same live actor can be retried normally.
    if (dispatched && readable && after == before && ref.Get()) {
        auto it = g_collected.find(key);
        if (it != g_collected.end() && it->second.Get() == coin) g_collected.erase(it);
    }
    UE_LOGW("coingun[collect]: native completion dispatch=%d balanceReadable=%d credited=%d", dispatched, readable, readable && after > before);
    return false;
}

bool SharedHostCoin(void* coin) {
    const auto eid = coop::element::Registry::Get().EidForActor(coin);
    return coop::element::Registry::IsAllowedHostAllocatedEid(eid);
}

struct PendingCollect {
    ue_wrap::CachedObjRef coin;
    uint64_t expiresMs = 0;
};
std::vector<PendingCollect> g_pendingCollects;
uint64_t g_nextCollectRetryMs = 0;
uint64_t CollectNowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// "Is this coin the host's?" has one predicate. A coin collected inside its own materialisation
// window (shoot a prop at your feet and the coins land on you; a mirror's delegates bind during
// BeginPlay inside FinishSpawning, before the row is installed) must answer yes, and the second
// term is identity-checked: a bare "is some mirror being born" would judge a map-placed coin
// tripped during any materialisation as host-owned and forward it under the other actor's eid.
bool IsHostOwnedCoin(void* coin) {
    if (coop::world_actor_sync::IsMirroredActor(coin)) return true;
    return coin != nullptr && coop::world_actor_sync::MaterializingActor() == coin;
}

// The client half: forward. Called from both entries; true iff a forward was sent, so the
// overlap entry can log what it cancelled.
bool TryForwardCollect(void* coin, const wchar_t* entry) {
    auto* s = I::Session();
    if (!s || !s->connected() || s->role() != coop::net::Role::Client) return false;
    if (!I::IsCoinActor(coin)) return false;

    // Mirror-scoped: a non-mirror coin is one of the maps' placed instances, level content on both
    // peers and never enrolled, so forwarding it would name an eid the host does not have. Left
    // native.
    if (!IsHostOwnedCoin(coin)) return false;

    // One identity source, in order: the actor's own row first, the materialisation window's eid
    // only as the fallback, since registering the spawning actor's collision can fire delegates on
    // other, already-mirrored actors it lands on, and those have rows of their own.
    coop::element::ElementId eid = coop::element::Registry::Get().EidForActor(coin);
    const wchar_t* idFrom = L"row";
    if (eid == coop::element::kInvalidId || eid == 0u) {
        // The window's eid may name this coin only if this coin is the actor being spawned.
        if (coop::world_actor_sync::MaterializingActor() == coin) {
            if (const unsigned int born = coop::world_actor_sync::MaterializingEid()) {
                eid    = static_cast<coop::element::ElementId>(born);
                idFrom = L"materializing";
            }
        }
    }
    if (eid == coop::element::kInvalidId || eid == 0u) {
        // A materializing mirror may not have its row yet. Leave it intact;
        // the bounded retry below can use the row once spawning finishes.
        return false;
    }

    coop::net::CoinCollectPayload p{};
    p.elementId = static_cast<uint32_t>(eid);
    if (!s->SendReliable(coop::net::ReliableKind::CoinCollect, &p, sizeof(p))) return false;
    g_forwarded.fetch_add(1, std::memory_order_relaxed);
    // The proof line, greppable: "coingun[collect seam]".
    UE_LOGI("coingun[collect seam] entry=%ls ctx=%p forwarding eid=%u (id from %ls) -- the host "
            "performs the collect on its own coin; this client's mirror entry is cancelled",
            entry, coin, p.elementId, idFrom);
    return true;
}

bool ForwardCollectToHost(void* coin, const wchar_t* entry) {
    const bool sent = TryForwardCollect(coin, entry);
    for (auto it = g_pendingCollects.begin(); it != g_pendingCollects.end(); ++it) {
        if (!it->coin.Is(coin)) continue;
        if (sent) g_pendingCollects.erase(it);
        return sent;
    }
    if (!sent && coin && IsHostOwnedCoin(coin) && g_pendingCollects.size() < 128) {
        PendingCollect pending;
        pending.coin.Set(coin);
        pending.expiresMs = CollectNowMs() + 2000;
        g_pendingCollects.push_back(pending);
        UE_LOGW("coingun[collect]: request pending for coin=%p (transport or identity unavailable); "
                "the mirror stays intact", coin);
    }
    return sent;
}

// Entry 1, the overlap: the host uses the same reserved native transaction as E/requests.
// The client forwards and
// then cancels for a mirror or a coin still materialising, and leaves a non-mirror coin native,
// since the maps' placed coins would otherwise become uncollectable and ghosted.
bool OnCollectPre(void* self, void* params) {
    if (!I::IsCoinActor(self)) return false;

    auto* s = I::Session();
    if (!s || !s->connected()) return false;     // solo: the native path is correct

    // The delegate signature is (UPrimitiveComponent* Overlapped, AActor* OtherActor, ...): the
    // tripping actor is the second pointer.
    void* other = nullptr;
    if (params) other = *reinterpret_cast<void**>(static_cast<uint8_t*>(params) + sizeof(void*));

    if (s->role() == coop::net::Role::Host) {
        if (other && (coop::players::Registry::Get().IsLocal(other) || coop::players::Registry::Get().IsPuppet(other)) &&
            SharedHostCoin(self)) {
            CollectAuthoritative(self, other);
            return true;
        }
        // Through the module's one reader of the points field.
        const int32_t pts = ReadCoinPoints(self);
        UE_LOGI("coingun[host collect]: coin=%p points=%d TRIPPED BY actor=%p class='%ls' -- the "
                "native credit runs, balance_sync will broadcast the new total",
                self, pts, other, other ? R::ClassNameOf(other).c_str() : L"<null>");
        g_seenOverlapHost.fetch_add(1, std::memory_order_relaxed);
        return false;                            // observe only; the host credits natively
    }

    // The client.
    g_seenOverlapClient.fetch_add(1, std::memory_order_relaxed);

    // Who tripped it decides what may be done, and the game's own gate is the authority: the coin's
    // overlap path casts OtherActor to mainPlayer_C and credits nobody on a miss. This interceptor
    // runs before that cast, and a sale spawns dozens of coins in one spot, so coin-on-coin
    // overlaps are real.
    const bool byLocal  = other && coop::players::Registry::Get().IsLocal(other);
    const bool byPuppet = other && coop::players::Registry::Get().IsPuppet(other);

    if (!byLocal && !byPuppet) {
        // Not a player: the native path bails at the cast, so there is nothing to suppress or
        // forward.
        return false;
    }

    if (IsHostOwnedCoin(self)) {
        if (byPuppet) {
            // Another peer's body tripped our mirror of the host's coin. Suppressed, since a puppet
            // is a mainPlayer_C and the native path would credit this client for their pickup; not
            // forwarded, since that peer forwards its own (or the host already credited natively),
            // and a forward from every peer that can see it would deliver one pickup many times.
            UE_LOGI("coingun[client collect]: SUPPRESSED on mirror coin=%p tripped by PUPPET %p -- no "
                    "forward: the collecting peer authors its own. Cancelled because a puppet is a "
                    "mainPlayer_C and the native cast would otherwise credit US for their pickup.",
                    self, other);
            return true;
        }
        // Forward first, then cancel: after `return true` there is no seam left.
        const bool fwd = ForwardCollectToHost(self, L"overlap");
        UE_LOGI("coingun[client collect]: SUPPRESSED on mirror coin=%p (tripped by LOCAL %p), "
                "forwarded=%d. The local credit is cancelled outright on THIS entry; the E-press "
                "is independently gated at the script body. The host performs the collect on its "
                "own coin.", self, other, fwd ? 1 : 0);
        return true;                             // cancel: no local addPoints, no local destroy
    }

    // Our own pre-barrier coins: the ones our shot just spawned, held by the barrier. Crediting for
    // them is a phantom whenever the shot authors a sale (the host mints the real ones).
    // Suppressed, nothing forwarded (there is no eid to name); if the shot authors nothing, the
    // barrier releases the coin and it is pickable again.
    if (I::IsCapturedCoin(self)) {
        UE_LOGI("coingun[client collect]: SUPPRESSED on OUR OWN pre-barrier coin=%p (tripped by %p) "
                "-- our shot spawned it and the barrier still holds it. If the shot authors a sale "
                "the host mints the real coins; if it does not, the barrier releases this one and it "
                "is pickable again.", self, other);
        return true;
    }

    if (byPuppet) {
        // A map-placed coin tripped by someone else's body: their event, not ours. Suppressed,
        // nothing forwarded.
        UE_LOGI("coingun[client collect]: SUPPRESSED on NON-mirror coin=%p tripped by PUPPET %p -- a "
                "map-placed coin is level content on both peers, and their body picking it up is "
                "their event, not ours.", self, other);
        return true;
    }

    UE_LOGW("coingun[client collect]: NON-MIRROR coin=%p collected locally (a map-placed coin: two "
            "cooked maps carry them, they are level content on both peers and never enrolled). "
            "This credits THIS CLIENT only. If this player's puppet also trips the host's copy, the "
            "host's broadcast overwrites the number shortly; if it does not, this number is wrong "
            "until the host's balance next moves. Pre-existing (A13), deliberately not cancelled.",
            self);
    return false;
}

// Entry 2: the E-press, through the verb's watch at the gate.
sg::Verdict OnCollectVerb(const sg::Call& b) {
    // The host performs a forwarded collect by calling this same verb through reflection, which
    // the gate sees like a press; that echo is ours and is not counted.
    if (b.fromOurCode) return sg::Verdict::Run;
    // actionOptionIndex is the interaction entry of many classes, and a name watch matches on name
    // alone; the class check is the consumer's discrimination.
    if (!I::IsCoinActor(b.object)) return sg::Verdict::Run;

    auto* s = I::Session();
    if (!s || !s->connected()) return sg::Verdict::Run;   // solo: the native credit is correct

    if (s->role() == coop::net::Role::Host) {
        g_seenPressHost.fetch_add(1, std::memory_order_relaxed);
        if (SharedHostCoin(b.object)) {
            CollectAuthoritative(b.object, coop::players::Registry::Get().Local());
            return sg::Verdict::Cancel;
        }
        // The host's own press: observe only, the native credit runs. The positive control for the
        // client's silence: without a line where the observer is expected to fire, a quiet client
        // log is indistinguishable from a dead hook. The host's performed collects below are
        // dispatched by our own reflected call, which the filter above already dropped.
        UE_LOGI("coingun[collect seam] entry=press HOST ctx=%p -- native credit runs here; this line "
                "proves the verb's watch is live in this session", b.object);
        return sg::Verdict::Run;
    }

    g_seenPressClient.fetch_add(1, std::memory_order_relaxed);

    if (I::IsCapturedCoin(b.object)) {
        UE_LOGI("coingun[client collect]: SUPPRESSED on OUR OWN pre-barrier coin=%p via E-press -- "
                "the host has not confirmed the sale yet; if no sale is authored, the barrier "
                "releases this coin and it can be collected again.", b.object);
        return sg::Verdict::Cancel;
    }

    // A map-placed coin exists independently on both peers; it has no host-owned eid to forward.
    // Keep its native, local behavior, matching the overlap entry.
    if (!IsHostOwnedCoin(b.object)) return sg::Verdict::Run;

    // The client must not locally credit and destroy a mirror while asking the host to do the same:
    // on refusal the host leaves its coin alive, and on success its normal balance/destroy lanes
    // update the client. Suppressing both is what makes the operation exactly once.
    const bool forwarded = ForwardCollectToHost(b.object, L"press");
    UE_LOGI("coingun[client collect]: SUPPRESSED E-press on mirror coin=%p, forwarded=%d -- "
            "the host performs the collect on its authoritative copy.", b.object,
            forwarded ? 1 : 0);
    return sg::Verdict::Cancel;
}

}  // namespace

// The host half: perform.
void OnCoinCollect(const uint8_t* payload, int len, uint8_t senderSlot, void* localPlayer) {
    auto* s = I::Session();
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) {
        UE_LOGW("coingun[host]: CoinCollect received off the HOST -- dropping");
        return;
    }
    if (!payload || len != static_cast<int>(sizeof(coop::net::CoinCollectPayload))) {
        UE_LOGW("coingun[host]: CoinCollect payload size invalid (len=%d) -- dropping", len);
        return;
    }
    coop::net::CoinCollectPayload p{};
    std::memcpy(&p, payload, sizeof(p));

    // The eid range: the host band only. A baocoin_C is a host-minted WorldActor with no save key,
    // spawned by the host's own sell and announced with this exact eid, so an id outside the host's
    // band cannot name a coin.
    if (!coop::element::Registry::IsAllowedHostAllocatedEid(p.elementId)) {
        static coop::roster_ledger::PerSlotState<uint32_t> sBad;
        const uint32_t n = ++sBad[senderSlot < coop::players::kMaxPeers ? senderSlot : 0u];
        if (n <= 5 || (n <= 100 && n % 10 == 0) || n % 100 == 0)
            UE_LOGW("coingun[host]: CoinCollect #%u from slot=%u names eid=0x%08x, which is not in "
                    "the HOST allocation band -- dropping. A coin is host-minted by construction, so "
                    "no other band can name one. (Rate-latched: this receiver is reachable from the "
                    "trust boundary at whatever rate a sender likes.)", n, senderSlot, p.elementId);
        return;
    }

    // Resolved against our own registry, fail-closed on type, and gated on reach: may this sender
    // name this coin? The reach is mainPlayer.armLength, 200 units, the game's own reach for
    // picking a coin up. A refusal costs a retry, never an item: the client's local entry was
    // cancelled, and the authoritative coin stays on the ground for whoever stands near it.
    constexpr float kCollectReachUU = 200.0f;
    const auto tok = coop::element::IntentTarget::ForClientIntent(*s, senderSlot, kCollectReachUU);
    const coop::element::IntentSubject sub = tok.Resolve(
        static_cast<coop::element::ElementId>(p.elementId), coop::element::ElementType::WorldActor);
    if (sub.outcome == coop::element::IntentOutcome::OutOfReach ||
        sub.outcome == coop::element::IntentOutcome::NoBody ||
        sub.outcome == coop::element::IntentOutcome::NoTarget) {
        UE_LOGW("coingun[host collect]: REFUSED slot=%u eid=%u -- REASON=%s (dist=%.0f allowed=%.0f). "
                "'no-body' means the sender has no puppet on the host that took a pose, so there is no "
                "body to measure a reach from, and 'no-target' that the coin's place cannot be read; "
                "either way we refuse rather than assume one -- the same fail-closed answer the sale "
                "lane gives. The coin stays where it is.",
                senderSlot, p.elementId, coop::element::OutcomeName(sub.outcome),
                sub.distUU, sub.reachUU);
        return;
    }
    void* coin = sub ? sub.actor : nullptr;
    if (!coin) {
        // The host's registry is authoritative for its own eids, so no live actor under this eid
        // means the coin is gone: somebody else collected it, the ordinary outcome of two players
        // reaching for one coin.
        g_unresolved.fetch_add(1, std::memory_order_relaxed);
        UE_LOGI("coingun[host collect]: slot=%u eid=%u does not resolve to a live WorldActor -- the "
                "coin is already gone (collected by someone else, or destroyed). Nothing to do.",
                senderSlot, p.elementId);
        return;
    }
    // The class gate: a WorldActor eid could name a pyramid or a wisp, and actionOptionIndex on one
    // of those would run an unrelated interaction.
    if (!I::IsCoinActor(coin)) {
        UE_LOGW("coingun[host collect]: slot=%u eid=%u resolves to a '%ls', not a baocoin_C -- "
                "REFUSING. A collect intent may only name a coin.",
                senderSlot, p.elementId, R::ClassNameOf(coin).c_str());
        return;
    }
    CollectAuthoritative(coin, localPlayer);
}

bool internal::CollectInstalled() { return g_installed.load(std::memory_order_acquire); }

void internal::TickCollect() {
    if (auto* s = I::Session(); s && s->connected() && s->role() == coop::net::Role::Host) {
        const auto epoch = g_collectEpoch;
        std::vector<uint32_t> keys;
        const auto now = CollectNowMs();
        if (now < g_retireAt) return;
        g_retireAt = now + 250;
        for (const auto& [key, ref] : g_retiringCoins) keys.push_back(key);
        std::sort(keys.begin(), keys.end());
        std::rotate(keys.begin(), std::upper_bound(keys.begin(), keys.end(), g_retireCursor), keys.end());
        if (keys.size() > 8) keys.resize(8);
        for (auto key : keys) {
            g_retireCursor = key;
            auto it = g_retiringCoins.find(key);
            if (it == g_retiringCoins.end()) continue;
            const auto ref = it->second;
            if (void* coin = ref.Get()) ue_wrap::engine::DestroyActor(coin);
            if (epoch != g_collectEpoch) return;
            if (!ref.Get()) g_retiringCoins.erase(key);
        }
    }
    if (g_pendingCollects.empty()) return;
    auto* s = I::Session();
    if (!s || !s->connected() || s->role() != coop::net::Role::Client) {
        g_pendingCollects.clear();
        g_nextCollectRetryMs = 0;
        return;
    }
    const uint64_t now = CollectNowMs();
    if (now < g_nextCollectRetryMs) return;
    g_nextCollectRetryMs = now + 250;
    for (auto it = g_pendingCollects.begin(); it != g_pendingCollects.end();) {
        void* coin = it->coin.Get();
        if (!coin || now >= it->expiresMs || !IsHostOwnedCoin(coin) ||
            TryForwardCollect(coin, L"retry")) {
            it = g_pendingCollects.erase(it);
        } else {
            ++it;
        }
    }
}

void internal::InstallCollect() {
    if (g_installed.load(std::memory_order_acquire)) return;

    // The sale lane's Install owns class resolution and retries every 125 ticks; inert until it lands
    // rather than a second GUObjectArray walk on the same tick.
    void* coinClass = internal::CoinClass();
    if (!coinClass) return;

    if (!g_verbRegistered.load(std::memory_order_acquire)) {
        if (sg::WatchName(kRowActionOptionIndex, kVerbCoinCollect, &OnCollectVerb, nullptr)) {
            g_verbRegistered.store(true, std::memory_order_release);
            UE_LOGI("coingun[collect]: watching the verb '%ls' (tag=%d) at the script-body gate -- the E-press entry",
                    kVerbNameCollect, kVerbCoinCollect);
        }
    }
    sg::ResolvePendingNames();

    const ue_wrap::facts::Resolved collectFn = ue_wrap::facts::Resolve(kRowCoinCollectOverlap);
    if (!collectFn) return;   // retry next tick

    if (!GT::RegisterInterceptor(collectFn, &OnCollectPre)) {
        UE_LOGE("coingun[collect]: overlap interceptor install FAILED -- the overlap entry is BLIND. "
                "The E-press entry is unaffected (it rides the verb's watch).");
        return;
    }
    g_installed.store(true, std::memory_order_release);
    UE_LOGI("coingun[collect]: installed -- both entries live (the '%ls' watch + the overlap BndEvt "
            "interceptor)", kVerbNameCollect);
}

void internal::OnDisconnectCollect() {
    ++g_collectEpoch;
    // The session summary: the two entry counters, so the next field run answers by grep which
    // entry players use. The labels follow the role, so the first pair is always the comparable
    // one.
    auto* sess = I::Session();
    const bool asHost = sess && sess->role() == coop::net::Role::Host;
    UE_LOGI("coingun[collect]: SESSION SUMMARY (%ls) -- thisPeer{press=%llu overlap=%llu} "
            "otherRole{press=%llu overlap=%llu} forwarded=%llu "
            "host{performed=%llu unresolved=%llu noCredit=%llu replayed=%llu} -- the FIRST pair is the "
            "comparable one: both entries counted on the same side.",
            asHost ? L"host" : L"client",
            (asHost ? g_seenPressHost   : g_seenPressClient).load(std::memory_order_relaxed),
            (asHost ? g_seenOverlapHost : g_seenOverlapClient).load(std::memory_order_relaxed),
            (asHost ? g_seenPressClient : g_seenPressHost).load(std::memory_order_relaxed),
            (asHost ? g_seenOverlapClient : g_seenOverlapHost).load(std::memory_order_relaxed),
            g_forwarded.load(std::memory_order_relaxed),
            g_performed.load(std::memory_order_relaxed),
            g_unresolved.load(std::memory_order_relaxed),
            g_noCredit.load(std::memory_order_relaxed),
            g_replayed.load(std::memory_order_relaxed));
    // Session-owned requests and consumption guards cannot enter the next session.
    g_collected.clear();
    g_retiringCoins.clear();
    g_retireAt = 0; g_retireCursor = 0;
    g_pendingCollects.clear();
    g_nextCollectRetryMs = 0;
}

}  // namespace coop::coingun_sync
