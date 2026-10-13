// coop/props/prop_food_sync.cpp -- see prop_food_sync.h.

#include "coop/props/prop_food_sync.h"

#include "coop/element/element.h"
#include "coop/element/intent_authority.h"
#include "coop/element/mirror_manager.h"
#include "coop/element/prop.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_element_tracker.h"

#include "ue_wrap/actors/food.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_pawn.h"
#include "ue_wrap/engine/world_identity.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::props::prop_food_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace F  = ue_wrap::food;

constexpr int      kTagTick = 0x46440001;
constexpr int      kTagBite = 0x46440002;
constexpr uint64_t kPollMs  = 500;
constexpr float    kTempEps = 1.0f;
constexpr float    kRipeEps = 0.02f;
constexpr uint64_t kBiteGapMs = 250;
constexpr float    kBiteReach = 500.f;

std::atomic<coop::net::Session*> g_session{nullptr};
bool     g_watched = false;
uint64_t g_nextPoll = 0;
uint32_t g_world = 0;

struct Last {
    float   temperature = 0.f;
    float   ripeness    = 0.f;
    int32_t uses        = 0;
    bool    sent        = false;
};
std::unordered_map<std::wstring, Last> g_sent;
std::unordered_map<std::wstring, Last> g_clientHandSent;
std::unordered_map<std::wstring, uint64_t> g_clientHandSentAt;
// Only host-authored samples belong here. The game may construct a fresh hand actor at default
// condition when a player picks up food; restore the last canonical world sample before submitting
// that new hand generation, or its defaults can overwrite the food's actual condition.
std::unordered_map<std::wstring, Last> g_clientHandCanonical;
std::unordered_map<std::wstring, uint32_t> g_clientHandGeneration;
Last g_handSent[coop::net::kMaxPeers];
std::wstring g_handKeys[coop::net::kMaxPeers];
std::unordered_map<uint64_t, uint64_t> g_biteAt;  // (slot<<32)|eid -> ms
// The client runs the native bite for immediate local feedback, then waits for the host's
// canonical FoodState. Until that reply arrives, a second native bite would consume against the
// same stale shared count. Keep one outstanding request per food, with a timeout for lost replies.
std::unordered_map<std::wstring, uint64_t> g_clientBitePending;
constexpr uint64_t kClientBiteAckTimeoutMs = 2500;
// The eat is watched around the body. The pre only remembers the use count; the post sends a bite
// when that count actually fell. A refused eat never reaches the host.
struct BitePending { void* actor = nullptr; int32_t uses = 0; bool have = false; };
std::vector<BitePending> g_bite;

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

void EnsureWorld() {
    const uint32_t world = ue_wrap::world_identity::Generation();
    if (world == g_world) return;
    g_world = world;
    g_nextPoll = 0;
    g_sent.clear();
    g_clientHandSent.clear();
    g_clientHandSentAt.clear();
    g_clientHandCanonical.clear();
    g_clientHandGeneration.clear();
    std::fill(std::begin(g_handSent), std::end(g_handSent), Last{});
    std::fill(std::begin(g_handKeys), std::end(g_handKeys), std::wstring{});
    g_biteAt.clear();
    g_clientBitePending.clear();
    g_bite.clear();
}

coop::net::Session* Session() { return g_session.load(std::memory_order_acquire); }

bool IsClient() {
    auto* s = Session();
    return s && s->connected() && s->role() == coop::net::Role::Client;
}

bool WorthSending(const Last& prev, const F::Condition& now);
void FillKey(void* actor, coop::net::WireKey& key, uint32_t& eid);
void BroadcastHandState(coop::net::Session& s, void* actor, uint8_t slot, uint32_t generation,
                        int targetSlot = -1);

uint32_t HandAddress(uint8_t slot, uint32_t generation) {
    return 0x80000000u | (uint32_t(slot & 0x3u) << 29) | (generation & 0x1FFFFFFFu);
}
bool DecodeHandAddress(uint32_t address, uint8_t& slot, uint32_t& generation) {
    if (!(address & 0x80000000u)) return false;
    slot = static_cast<uint8_t>((address >> 29) & 0x3u);
    generation = address & 0x1FFFFFFFu;
    return generation != 0 && slot < coop::net::kMaxPeers;
}
void* HandActor(uint8_t slot, uint32_t generation) {
    if (slot >= coop::net::kMaxPeers) return nullptr;
    if (slot == coop::players::Registry::Get().LocalPeerId())
        return coop::hand_item::Generation(slot) == generation ? coop::hand_item::LocalHandActor() : nullptr;
    return coop::hand_item::MirrorGeneration(slot) == generation
        ? coop::hand_item::MirrorActorForSlot(slot) : nullptr;
}

void BroadcastHandState(coop::net::Session& s, void* actor, uint8_t slot, uint32_t generation,
                        int targetSlot) {
    if (!actor || !F::IsFood(actor) || slot >= coop::net::kMaxPeers || !generation) return;
    F::Condition condition{};
    if (!F::Read(actor, condition) || !std::isfinite(condition.temperature) ||
        !std::isfinite(condition.ripeness)) return;
    coop::net::FoodStatePayload p{};
    if (slot == coop::players::Registry::Get().LocalPeerId()) {
        FillKey(actor, p.key, p.elementId);
        if (p.key.len) g_handKeys[slot] = coop::net::StringFromWireKey(p.key);
    } else if (!g_handKeys[slot].empty()) {
        coop::net::WireKeyFromString(g_handKeys[slot], p.key);
    }
    if (!p.key.len) return;
    p.elementId = HandAddress(slot, generation);
    p.temperature = condition.temperature;
    p.ripeness = condition.ripeness;
    p.uses = condition.uses;
    // A join snapshot is sent to one peer only. Do not advance the broadcast baseline from a
    // unicast: existing peers still need the next state change.
    Last& previous = g_handSent[slot];
    if (targetSlot < 0 && !WorthSending(previous, condition)) return;
    const bool sent = targetSlot < 0
        ? s.SendReliable(coop::net::ReliableKind::FoodState, &p, sizeof(p))
        : s.SendReliableToSlot(targetSlot, coop::net::ReliableKind::FoodState, &p, sizeof(p));
    if (!sent) return;
    if (targetSlot < 0)
        previous = {condition.temperature, condition.ripeness, condition.uses, true};
}

sg::Verdict OnTick(const sg::Call& c) {
    if (c.fromOurCode || !F::IsFood(c.object)) return sg::Verdict::Run;
    // The host's tick is the author of food sitting in the world. A client's copy would burn,
    // freeze and spoil on its own. Food in a hand is not in that set: the host poll skips it, so
    // cancelling its tick here would freeze a meal the player is holding.
    if (!IsClient() || coop::hand_item::IsHandAxisActor(c.object)) return sg::Verdict::Run;
    return sg::Verdict::Cancel;
}

void FillKey(void* actor, coop::net::WireKey& key, uint32_t& eid) {
    key = {};
    eid = 0;
    const std::wstring k = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!k.empty() && k != L"None") coop::net::WireKeyFromString(k, key);
    const auto id = coop::prop_element_tracker::GetPropElementIdForActor(actor);
    if (id != coop::element::kInvalidId) eid = static_cast<uint32_t>(id);
}

bool WorthSending(const Last& prev, const F::Condition& now) {
    if (!prev.sent) return true;
    if (prev.uses != now.uses) return true;
    if (std::fabs(prev.temperature - now.temperature) >= kTempEps) return true;
    if (std::fabs(prev.ripeness - now.ripeness) >= kRipeEps) return true;
    return false;
}

bool Publish(coop::net::Session& s, void* actor, int slot, bool force = false) {
    F::Condition now{};
    if (!F::Read(actor, now) || !ValidCondition(now.temperature, now.ripeness, now.uses)) return false;
    coop::net::FoodStatePayload p{};
    FillKey(actor, p.key, p.elementId);
    if (p.key.len == 0 && p.elementId == 0) return false;
    const std::wstring id = p.key.len ? coop::net::StringFromWireKey(p.key)
                                      : (L"#" + std::to_wstring(p.elementId));
    Last& prev = g_sent[id];
    if (!force && slot < 0 && !WorthSending(prev, now)) return true;
    p.temperature = now.temperature;
    p.ripeness    = now.ripeness;
    p.uses        = now.uses;
    const bool ok = slot < 0
        ? s.SendReliable(coop::net::ReliableKind::FoodState, &p, sizeof(p))
        : s.SendReliableToSlot(slot, coop::net::ReliableKind::FoodState, &p, sizeof(p));
    if (!ok) { if (slot < 0) prev.sent = false; return false; }
    // A seed for one joiner must not mark an update as delivered to existing peers.
    if (slot < 0) {
        prev.temperature = now.temperature;
        prev.ripeness    = now.ripeness;
        prev.uses        = now.uses;
        prev.sent        = true;
    }
    return true;
}

struct Walk {
    void* world = nullptr;
    void* base  = nullptr;
    std::vector<void*> actors;
};

void OnInst(void* ctx, void* obj, int32_t) {
    auto* w = static_cast<Walk*>(ctx);
    if (!obj || !R::IsLive(obj) || !F::IsFood(obj)) return;
    if (w->world && ue_wrap::world_identity::WorldOf(obj) != w->world) return;
    if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
    if (coop::hand_item::IsHandAxisActor(obj)) return;
    w->actors.push_back(obj);
}

std::vector<void*> g_foodClasses;
uint64_t g_foodClassVer = ~uint64_t{0};

void RefreshFoodClasses() {
    const uint64_t ver = ue_wrap::object_index::ClassSetVersion();
    void* base = R::FindClass(L"prop_food_C");
    if (ver == g_foodClassVer && !g_foodClasses.empty()) return;
    g_foodClassVer = ver;
    g_foodClasses.clear();
    if (!base || !ue_wrap::object_index::IsSeeded()) return;
    struct Ctx { void* base; } ctx{base};
    ue_wrap::object_index::ForEachClass([](void* p, void* cls, void*) {
        auto* c = static_cast<Ctx*>(p);
        if (!cls || !c->base) return;
        if (cls == c->base || R::IsDescendantOfAny(cls, &c->base, 1))
            g_foodClasses.push_back(cls);
    }, &ctx);
}

void Collect(std::vector<void*>& out) {
    RefreshFoodClasses();
    if (g_foodClasses.empty()) return;
    Walk w;
    w.world = ue_wrap::world_identity::CurrentWorld();
    w.base  = R::FindClass(L"prop_food_C");
    for (void* cls : g_foodClasses)
        ue_wrap::object_index::ForEachInstance(cls, &OnInst, &w);
    out.swap(w.actors);
}

void* Resolve(const coop::net::WireKey& key, uint32_t eid) {
    const std::wstring k = coop::net::StringFromWireKey(key);
    if (!k.empty() && k != L"None") {
        if (void* a = coop::prop_element_tracker::FindLiveActorByKey(k)) return a;
    }
    if (eid == 0) return nullptr;
    auto* el = coop::element::MirrorManager<coop::element::Prop>::Instance().Get(
        static_cast<coop::element::ElementId>(eid));
    return el ? el->GetActor() : nullptr;
}

sg::Verdict OnAction(const sg::Call& c) {
    auto* s = Session();
    if (c.fromOurCode || !s || !s->connected() ||
        (s->role() != coop::net::Role::Client && s->role() != coop::net::Role::Host) ||
        !F::IsFood(c.object)) return sg::Verdict::Run;
    EnsureWorld();
    const int32_t off = R::FindParamOffset(c.function, L"action");
    if (off < 0 || !c.locals || c.locals[off] != 4) return sg::Verdict::Run;
    if (s->role() == coop::net::Role::Client) {
        coop::net::WireKey key{};
        uint32_t eid = 0;
        FillKey(c.object, key, eid);
        const std::wstring id = key.len ? coop::net::StringFromWireKey(key)
                                        : (eid ? L"#" + std::to_wstring(eid) : std::wstring{});
        if (!id.empty()) {
            const uint64_t now = NowMs();
            auto pending = g_clientBitePending.find(id);
            if (pending != g_clientBitePending.end()) {
                if (now - pending->second < kClientBiteAckTimeoutMs) return sg::Verdict::Cancel;
                g_clientBitePending.erase(pending);
            }
        }
    }
    BitePending b;
    b.actor = c.object;
    F::Condition now{};
    b.have = F::Read(c.object, now);
    b.uses = now.uses;
    g_bite.push_back(b);
    return sg::Verdict::Run;
}

void OnActionPost(const sg::Call& c) {
    // The post runs for every action on this food, including ones the pre did not record. Popping
    // those would throw away an eat that is still on the stack.
    const int32_t off = R::FindParamOffset(c.function, L"action");
    if (off < 0 || !c.locals || c.locals[off] != 4) return;
    if (c.fromOurCode || !F::IsFood(c.object)) return;
    EnsureWorld();
    if (g_bite.empty() || g_bite.back().actor != c.object) return;
    const BitePending b = g_bite.back();
    g_bite.pop_back();
    if (!b.have || !b.actor || !R::IsLive(b.actor)) return;
    F::Condition now{};
    if (!F::Read(b.actor, now) || now.uses >= b.uses) return;
    auto* s = Session();
    if (!s || !s->connected()) return;
    if (s->role() == coop::net::Role::Host) {
        // A host bite is already canonical. Publish it at the action edge rather than waiting for
        // the 500 ms condition poll; otherwise a client can take another optimistic bite against
        // the stale count before the host's new count reaches it.
        const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
        if (b.actor == coop::hand_item::LocalHandActor()) {
            g_handSent[localSlot].sent = false;
            BroadcastHandState(*s, b.actor, localSlot, coop::hand_item::Generation(localSlot));
        } else {
            Publish(*s, b.actor, -1);
        }
        return;
    }
    if (s->role() != coop::net::Role::Client) return;
    coop::net::FoodBitePayload p{};
    FillKey(b.actor, p.key, p.elementId);
    if (p.key.len == 0 && p.elementId == 0) return;
    if (s->SendReliable(coop::net::ReliableKind::FoodBite, &p, sizeof(p))) {
        const std::wstring id = p.key.len ? coop::net::StringFromWireKey(p.key)
                                          : (p.elementId ? L"#" + std::to_wstring(p.elementId) : std::wstring{});
        if (!id.empty()) g_clientBitePending[id] = NowMs();
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_watched) return;
    g_watched = true;
    sg::WatchClassName(L"prop_food_C", L"ReceiveTick", kTagTick, &OnTick, nullptr);
    sg::WatchClassName(L"prop_food_C", L"actionOptionIndex", kTagBite, &OnAction, &OnActionPost);
}

void Tick() {
    auto* s = Session();
    if (!s || !s->connected()) return;
    EnsureWorld();
    const uint64_t now = NowMs();
    if (now < g_nextPoll) return;
    g_nextPoll = now + kPollMs;
    if (s->role() == coop::net::Role::Client) {
        void* held = coop::hand_item::LocalHandActor();
        F::Condition condition{};
        if (!held || !F::IsFood(held) || !F::Read(held, condition) ||
            !std::isfinite(condition.temperature) || !std::isfinite(condition.ripeness)) return;
        coop::net::FoodStatePayload p{};
        FillKey(held, p.key, p.elementId);
        if (!p.key.len) return;
        const std::wstring id = coop::net::StringFromWireKey(p.key);
        const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
        const uint32_t generation = coop::hand_item::Generation(localSlot);
        if (!generation) return;
        auto canonical = g_clientHandCanonical.find(id);
        auto previousGeneration = g_clientHandGeneration.find(id);
        if (canonical != g_clientHandCanonical.end() &&
            (previousGeneration == g_clientHandGeneration.end() || previousGeneration->second != generation)) {
            const Last& state = canonical->second;
            F::Condition restored{state.temperature, state.ripeness, state.uses};
            if (!F::Write(held, restored)) return;  // retry next poll; never publish the default
            condition = restored;
            g_clientHandGeneration[id] = generation;
        }
        p.elementId = HandAddress(localSlot, generation);
        auto bite = g_clientBitePending.find(id);
        if (bite != g_clientBitePending.end()) {
            if (now - bite->second < kClientBiteAckTimeoutMs) return;
            g_clientBitePending.erase(bite);
        }
        Last& prev = g_clientHandSent[id];
        const uint64_t lastSent = g_clientHandSentAt[id];
        if (!WorthSending(prev, condition) && now - lastSent < 2000) return;
        p.temperature = condition.temperature;
        p.ripeness = condition.ripeness;
        p.uses = condition.uses;
        if (!s->SendReliableToSlot(0, coop::net::ReliableKind::FoodState, &p, sizeof(p))) return;
        prev = {condition.temperature, condition.ripeness, condition.uses, true};
        g_clientHandSentAt[id] = now;
        return;
    }
    if (s->role() != coop::net::Role::Host) return;
    std::vector<void*> foods;
    Collect(foods);
    for (void* a : foods) Publish(*s, a, -1);
    // Host-owned local hand food is shared too; Publish's identity dedupe keeps the world
    // actor and hand actor from producing redundant state when they momentarily coexist.
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    if (void* held = coop::hand_item::LocalHandActor(); held && F::IsFood(held))
        BroadcastHandState(*s, held, localSlot, coop::hand_item::Generation(localSlot));
}

void Apply(const coop::net::FoodStatePayload& payload) {
    if (!IsClient()) return;
    EnsureWorld();
    if (!std::isfinite(payload.temperature) || !std::isfinite(payload.ripeness)) return;
    if (payload.temperature < 0.f || payload.temperature > 2000.f) return;
    if (payload.ripeness < -100.f || payload.ripeness > 10000.f) return;
    if (payload.uses < -10 || payload.uses > 100000) return;
    const std::wstring id = payload.key.len ? coop::net::StringFromWireKey(payload.key)
                                            : (payload.elementId ? L"#" + std::to_wstring(payload.elementId)
                                                                 : std::wstring{});
    uint8_t handSlot = 0;
    uint32_t handGeneration = 0;
    if (DecodeHandAddress(payload.elementId, handSlot, handGeneration)) {
        void* held = HandActor(handSlot, handGeneration);
        if (!held || !F::IsFood(held)) return;
        F::Condition c{payload.temperature, payload.ripeness, payload.uses};
        if (!F::Write(held, c)) return;
        if (!id.empty()) g_clientBitePending.erase(id);
        if (handSlot < coop::net::kMaxPeers) g_handKeys[handSlot] = id;
        if (!id.empty()) g_clientHandCanonical[id] = {c.temperature, c.ripeness, c.uses, true};
        if (!id.empty()) g_clientHandSent[id] = {c.temperature, c.ripeness, c.uses, true};
        return;
    }
    void* actor = Resolve(payload.key, payload.elementId);
    if (!actor || !F::IsFood(actor)) return;
    F::Condition c;
    c.temperature = payload.temperature;
    c.ripeness    = payload.ripeness;
    c.uses        = payload.uses;
    if (!F::Write(actor, c)) return;
    if (!id.empty()) g_clientBitePending.erase(id);
    if (!id.empty()) g_clientHandSent[id] = {c.temperature, c.ripeness, c.uses, true};
    if (!id.empty()) g_clientHandCanonical[id] = {c.temperature, c.ripeness, c.uses, true};
    if (c.uses <= 0) ue_wrap::engine::DestroyActor(actor);
}

void OnBite(const coop::net::FoodBitePayload& payload, uint8_t senderSlot) {
    auto* s = Session();
    if (!s || s->role() != coop::net::Role::Host) return;
    EnsureWorld();
    if (senderSlot < 1 || senderSlot >= coop::net::kMaxPeers) return;
    const std::wstring key = coop::net::StringFromWireKey(payload.key);
    void* actor = coop::hand_item::MirrorActorForSlot(senderSlot);
    if (!F::IsFood(actor) || g_handKeys[senderSlot] != key) actor = Resolve(payload.key, payload.elementId);
    if (!actor || !F::IsFood(actor)) return;
    auto target = coop::element::IntentTarget::ForClientIntent(*s, senderSlot, kBiteReach);
    if (!target.Authorize(actor)) return;
    const uint64_t now = NowMs();
    // Key and element id together: a food that has only a key must not share the 250 ms gate
    // with every other key-only food that player bites.
    uint64_t token = 1469598103934665603ull;
    token ^= senderSlot;
    token *= 1099511628211ull;
    token ^= payload.elementId;
    token *= 1099511628211ull;
    const uint8_t keyN = payload.key.len < sizeof(payload.key.data)
                             ? payload.key.len
                             : static_cast<uint8_t>(sizeof(payload.key.data));
    for (uint8_t i = 0; i < keyN; ++i) {
        token ^= static_cast<unsigned char>(payload.key.data[i]);
        token *= 1099511628211ull;
    }
    const auto it = g_biteAt.find(token);
    if (it != g_biteAt.end() && now - it->second < kBiteGapMs) return;
    F::Condition c{};
    if (!F::Read(actor, c) || c.uses <= 0) return;
    g_biteAt[token] = now;
    c.uses -= 1;
    F::Write(actor, c);
    const std::wstring id = payload.key.len ? coop::net::StringFromWireKey(payload.key)
                                            : (L"#" + std::to_wstring(payload.elementId));
    g_sent[id].uses = c.uses;
    g_sent[id].sent = false;  // the next poll publishes the new count
    // Commit and broadcast the host's decision immediately. A delayed poll allowed the host and
    // client to each consume the same stale final portions.
    const uint32_t generation = coop::hand_item::MirrorGeneration(senderSlot);
    if (actor == coop::hand_item::MirrorActorForSlot(senderSlot) && generation) {
        g_handSent[senderSlot].sent = false;
        BroadcastHandState(*s, actor, senderSlot, generation);
    } else {
        Publish(*s, actor, -1);
        if (c.uses <= 0) ue_wrap::engine::DestroyActor(actor);
    }
}

void OnHandState(const coop::net::FoodStatePayload& payload, uint8_t senderSlot) {
    auto* s = Session();
    if (!s || s->role() != coop::net::Role::Host || senderSlot == 0 ||
        senderSlot >= coop::net::kMaxPeers || !payload.key.len ||
        !std::isfinite(payload.temperature) || !std::isfinite(payload.ripeness) ||
        payload.temperature < 0.f || payload.temperature > 2000.f ||
        payload.ripeness < -100.f || payload.ripeness > 10000.f ||
        payload.uses < -10 || payload.uses > 100000) return;
    EnsureWorld();
    uint8_t addressedSlot = 0;
    uint32_t generation = 0;
    if (!DecodeHandAddress(payload.elementId, addressedSlot, generation) || addressedSlot != senderSlot ||
        coop::hand_item::MirrorGeneration(senderSlot) != generation) return;
    const std::wstring key = coop::net::StringFromWireKey(payload.key);
    void* actor = coop::hand_item::MirrorActorForSlot(senderSlot);
    if (!F::IsFood(actor) || key.empty()) return;
    const auto target = coop::element::IntentTarget::ForClientIntent(*s, senderSlot, kBiteReach);
    if (!target.Authorize(actor)) return;
    F::Condition condition{payload.temperature, payload.ripeness, payload.uses};
    if (!F::Write(actor, condition)) return;
    g_handKeys[senderSlot] = key;
    // Re-broadcast the host's accepted sample. The owner and every observer apply it to the
    // matching held actor; ordinary world food still uses its existing host poll.
    g_handSent[senderSlot].sent = false;
    BroadcastHandState(*s, actor, senderSlot, generation);
}

void SendAllToSlot(int slot) {
    auto* s = Session();
    if (!s || s->role() != coop::net::Role::Host || slot <= 0) return;
    EnsureWorld();
    std::vector<void*> foods;
    Collect(foods);
    for (void* a : foods) Publish(*s, a, slot);
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    for (uint8_t owner = 0; owner < coop::net::kMaxPeers; ++owner) {
        void* held = owner == localSlot ? coop::hand_item::LocalHandActor()
                                        : coop::hand_item::MirrorActorForSlot(owner);
        if (!held || !F::IsFood(held)) continue;
        const uint32_t generation = owner == localSlot ? coop::hand_item::Generation(owner)
                                                       : coop::hand_item::MirrorGeneration(owner);
        BroadcastHandState(*s, held, owner, generation, slot);
    }
}

void OnDisconnect() {
    g_sent.clear();
    g_clientHandSent.clear();
    g_clientHandSentAt.clear();
    g_clientHandCanonical.clear();
    g_clientHandGeneration.clear();
    std::fill(std::begin(g_handSent), std::end(g_handSent), Last{});
    std::fill(std::begin(g_handKeys), std::end(g_handKeys), std::wstring{});
    g_biteAt.clear();
    g_clientBitePending.clear();
    g_bite.clear();
    g_nextPoll = 0;
    g_world = 0;
    g_session.store(nullptr, std::memory_order_release);
}

void ForgetPublished(const std::wstring& key) { g_sent.erase(key); }

bool PublishSpawnState(coop::net::Session& s, void* actor, int slot) {
    if (!s.connected() || s.role() != coop::net::Role::Host) return false;
    return Publish(s, actor, slot, true);
}

}  // namespace coop::props::prop_food_sync
