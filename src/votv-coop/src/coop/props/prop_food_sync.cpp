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
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_element_tracker.h"

#include "ue_wrap/actors/food.h"
#include "ue_wrap/actors/cooking_food.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_pawn.h"
#include "ue_wrap/engine/world_identity.h"

#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
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
uint64_t g_epoch=0;
struct PendingWorld {coop::net::FoodStatePayload state{};uint64_t revision=0;};
std::map<std::wstring,PendingWorld> g_pendingWorld;
std::wstring g_worldCursor;
uint64_t g_worldRevision=0;
bool g_retryingWorld=false;
struct PendingHand {coop::net::FoodStatePayload state{};uint64_t revision=0;bool have=false;};
std::array<PendingHand,coop::net::kMaxPeers> g_pendingHand{};
std::array<uint16_t,coop::net::kMaxPeers> g_handOccupants{};
uint64_t g_handRevision=0;

struct Last {
    float   temperature = 0.f;
    float   ripeness    = 0.f;
    int32_t uses        = 0;
    bool    sent        = false;
    float   cookProgress = -1.f;
};
Last Remember(const F::Condition& c) {return {c.temperature,c.ripeness,c.uses,true,c.cookProgress};}
F::Condition DecodeCondition(const coop::net::FoodStatePayload& p) {
    return {p.temperature,p.ripeness,p.uses,p.cookProgress};
}
void EncodeCondition(const F::Condition& c,coop::net::FoodStatePayload& p) {
    p.temperature=c.temperature;p.ripeness=c.ripeness;p.uses=c.uses;p.cookProgress=c.cookProgress;
}
bool Valid(const F::Condition& c) {return ValidCondition(c.temperature,c.ripeness,c.uses,c.cookProgress);}
std::unordered_map<std::wstring, Last> g_sent;
uint64_t g_publishSequence=0;
std::unordered_map<std::wstring,uint64_t> g_publishRevision;
std::array<uint64_t,coop::net::kMaxPeers> g_peerEpoch{};
std::unordered_map<std::wstring, Last> g_clientHandSent;
std::unordered_map<std::wstring, uint64_t> g_clientHandSentAt;
// Only host-authored samples belong here. The game may construct a fresh hand actor at default
// condition when a player picks up food; restore the last canonical world sample before submitting
// that new hand generation, or its defaults can overwrite the food's actual condition.
std::unordered_map<std::wstring, Last> g_clientHandCanonical;
std::unordered_map<std::wstring, uint32_t> g_clientHandGeneration;
Last g_handSent[coop::net::kMaxPeers];
std::wstring g_handKeys[coop::net::kMaxPeers];
struct HandStamp {uint32_t generation=0;uint16_t playerNo=0;std::wstring key;uint64_t revision=0;};
std::array<HandStamp,coop::net::kMaxPeers> g_handPublished{};
std::array<HandStamp,coop::net::kMaxPeers> g_handAccepted{};
std::array<uint64_t,coop::net::kMaxPeers> g_handPublishRevision{};
std::array<uint32_t,coop::net::kMaxPeers> g_handJoin{};
struct JoinTarget{uint16_t person=0;uint64_t epoch=0,revision=0;};
std::array<std::array<JoinTarget,coop::net::kMaxPeers>,coop::net::kMaxPeers> g_handJoinTarget{};
uint64_t g_foodJoinSequence=0;
struct JoinFood {ue_wrap::CachedObjRef actor;uint32_t pending=0;std::array<JoinTarget,coop::net::kMaxPeers> targets{};};
std::map<std::wstring,JoinFood> g_joinFoods;
std::wstring g_joinCursor;
std::unordered_map<uint64_t, uint64_t> g_biteAt;  // (slot<<32)|eid -> ms
// The client runs the native bite for immediate local feedback, then waits for the host's
// canonical FoodState. Until that reply arrives, a second native bite would consume against the
// same stale shared count. Keep one outstanding request per food, with a timeout for lost replies.
std::unordered_map<std::wstring, uint64_t> g_clientBitePending;
struct BiteRetry{coop::net::FoodBitePayload payload{};uint64_t revision=0;};
std::map<std::wstring,BiteRetry> g_clientBiteRetry;
uint64_t g_biteSendSequence=0;
std::wstring g_biteRetryCursor;
constexpr uint64_t kClientBiteAckTimeoutMs = 2500;
// The eat is watched around the body. The pre only remembers the use count; the post sends a bite
// when that count actually fell. A refused eat never reaches the host.
struct BitePending {
    void* actor = nullptr;
    ue_wrap::CachedObjRef identity;
    coop::net::FoodBitePayload request{};
    int32_t uses = 0;
    int32_t serial = 0;
    bool have = false;
    bool submitted = false;
    coop::net::Session* session=nullptr;
    coop::net::Role role=coop::net::Role::Host;
    uint64_t epoch=0,invocation=0,peerEpoch=0;
    uint32_t world=0;
    uint8_t slot=0;
    uint16_t person=0;
    bool Current()const{return session && session==g_session.load() && session->connected() && session->role()==role &&
        epoch==g_epoch && world==ue_wrap::world_identity::Generation() && g_peerEpoch[slot]==peerEpoch &&
        coop::roster_ledger::Get(slot).playerNo==person && R::SlotSerial(identity.Idx())==serial && sg::IsInvocationActive(invocation);}
};
std::vector<BitePending> g_bite;
void PruneBites(){std::erase_if(g_bite,[](const BitePending& b){return !b.Current();});}

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::wstring BiteKey(const coop::net::FoodBitePayload& p) {
    return p.key.len ? coop::net::StringFromWireKey(p.key)
        : (p.elementId ? L"#" + std::to_wstring(p.elementId) : std::wstring{});
}
coop::net::Session* Session();
bool SubmitBite(coop::net::Session& s, const coop::net::FoodBitePayload p) {
    const auto id = BiteKey(p);
    if (id.empty() || Session()!=&s || !s.connected() || s.role()!=coop::net::Role::Client || (g_clientBiteRetry.size()>=64 && !g_clientBiteRetry.count(id))) return false;
    const auto epoch=g_epoch,revision=++g_biteSendSequence;
    g_clientBiteRetry[id]={p,revision}; // retain before sending: native final-bite may retire the actor
    g_clientBitePending[id] = NowMs();
    const bool sent=s.SendReliable(coop::net::ReliableKind::FoodBite, &p, sizeof(p));
    if(Session()!=&s || !s.connected() || s.role()!=coop::net::Role::Client || epoch!=g_epoch || g_world!=ue_wrap::world_identity::Generation())return false;
    const auto latest=g_clientBiteRetry.find(id);if(sent && latest!=g_clientBiteRetry.end() && latest->second.revision==revision)g_clientBiteRetry.erase(latest);
    return true;
}
void RetryBiteRequests(coop::net::Session& session){
    static bool busy=false;if(busy)return;struct Busy{Busy(){busy=true;}~Busy(){busy=false;}}guard;
    const auto epoch=g_epoch;const auto pending=g_clientBiteRetry;auto it=pending.upper_bound(g_biteRetryCursor);if(it==pending.end())it=pending.begin();
    for(size_t work=0;work<(std::min)(size_t(8),pending.size());++work){
        const auto key=it->first;const auto row=it->second;if(++it==pending.end())it=pending.begin();g_biteRetryCursor=key;
        const auto current=[&]{return Session()==&session && session.connected() && session.role()==coop::net::Role::Client && epoch==g_epoch && g_world==ue_wrap::world_identity::Generation();};
        if(!current())return;auto live=g_clientBiteRetry.find(key);if(live==g_clientBiteRetry.end() || live->second.revision!=row.revision)continue;
        const bool sent=session.SendReliable(coop::net::ReliableKind::FoodBite,&row.payload,sizeof(row.payload));if(!current())return;
        live=g_clientBiteRetry.find(key);if(sent && live!=g_clientBiteRetry.end() && live->second.revision==row.revision){
            g_clientBiteRetry.erase(live);const auto wait=g_clientBitePending.find(key);if(wait!=g_clientBitePending.end())wait->second=NowMs();
        }
    }
}

void EnsureWorld() {
    const uint32_t world = ue_wrap::world_identity::Generation();
    if (world == g_world) return;
    g_world = world;
    ++g_epoch;g_pendingWorld.clear();g_worldCursor.clear();
    g_pendingHand={};g_handOccupants={};
    g_handPublished={};g_handAccepted={};g_handPublishRevision={};
    g_handJoin={};g_handJoinTarget={};g_joinFoods.clear();g_joinCursor.clear();
    g_nextPoll = 0;
    g_sent.clear();g_publishRevision.clear();
    g_clientHandSent.clear();
    g_clientHandSentAt.clear();
    g_clientHandCanonical.clear();
    g_clientHandGeneration.clear();
    std::fill(std::begin(g_handSent), std::end(g_handSent), Last{});
    std::fill(std::begin(g_handKeys), std::end(g_handKeys), std::wstring{});
    g_biteAt.clear();
    g_clientBitePending.clear();
    g_clientBiteRetry.clear();g_biteRetryCursor.clear();
    g_bite.clear();
}

coop::net::Session* Session() { return g_session.load(std::memory_order_acquire); }

bool IsClient() {
    auto* s = Session();
    return s && s->connected() && s->role() == coop::net::Role::Client;
}

bool WorthSending(const Last& prev, const F::Condition& now);
void FillKey(void* actor, coop::net::WireKey& key, uint32_t& eid);
bool BroadcastHandState(coop::net::Session& s, void* actor, uint8_t slot, uint32_t generation,
                        int targetSlot = -1);

uint32_t HandAddress(uint8_t slot, uint32_t generation) {
    return 0x80000000u | (uint32_t(slot & 0x1Fu) << 26) | (generation & 0x03FFFFFFu);
}
bool DecodeHandAddress(uint32_t address, uint8_t& slot, uint32_t& generation) {
    if (!(address & 0x80000000u)) return false;
    slot = static_cast<uint8_t>((address >> 26) & 0x1Fu);
    generation = address & 0x03FFFFFFu;
    return generation != 0 && slot < coop::net::kMaxPeers;
}
void* HandActor(uint8_t slot, uint32_t generation) {
    if (slot >= coop::net::kMaxPeers) return nullptr;
    if (slot == coop::players::Registry::Get().LocalPeerId())
        return (coop::hand_item::Generation(slot)&0x03FFFFFFu) == generation ? coop::hand_item::LocalHandActor() : nullptr;
    return (coop::hand_item::MirrorGeneration(slot)&0x03FFFFFFu) == generation
        ? coop::hand_item::MirrorActorForSlot(slot) : nullptr;
}

bool BroadcastHandState(coop::net::Session& s, void* actor, uint8_t slot, uint32_t generation,
                        int targetSlot) {
    if (Session()!=&s || !s.connected() || s.role()!=coop::net::Role::Host || !actor || slot >= coop::net::kMaxPeers || !generation || targetSlot>=coop::net::kMaxPeers) return false;
    EnsureWorld();const auto epoch=g_epoch,sourceEpoch=g_peerEpoch[slot],capture=++g_publishSequence;
    const auto person=coop::roster_ledger::Get(slot).playerNo;
    const auto recipient=targetSlot>0?coop::roster_ledger::Get(targetSlot).playerNo:0;
    const auto recipientEpoch=targetSlot>0?g_peerEpoch[targetSlot]:0;
    ue_wrap::CachedObjRef identity;identity.Set(actor);const auto sourceKey=g_handKeys[slot];
    const auto sourceCurrent=[&]{return Session()==&s && s.connected() && s.role()==coop::net::Role::Host && epoch==g_epoch &&
        g_world==ue_wrap::world_identity::Generation() && identity.Get() && g_peerEpoch[slot]==sourceEpoch &&
        coop::roster_ledger::Get(slot).playerNo==person && HandActor(slot,generation&0x03FFFFFFu)==actor &&
        (slot==coop::players::Registry::Get().LocalPeerId() || (g_handKeys[slot]==sourceKey &&
        g_handAccepted[slot].generation==(generation&0x03FFFFFFu) && g_handAccepted[slot].playerNo==person && g_handAccepted[slot].key==sourceKey &&
        (!g_pendingHand[slot].have || g_pendingHand[slot].revision==g_handAccepted[slot].revision))) &&
        (targetSlot<0 || (targetSlot>0 && s.IsSlotWorldReady(targetSlot) && g_peerEpoch[targetSlot]==recipientEpoch && coop::roster_ledger::Get(targetSlot).playerNo==recipient));};
    if(!person || !sourceCurrent() || !F::IsFood(actor) || !sourceCurrent())return false;
    F::Condition condition{};
    if (!F::Read(actor, condition) || !sourceCurrent() || !Valid(condition)) return false;
    coop::net::FoodStatePayload p{};
    if (slot == coop::players::Registry::Get().LocalPeerId()) {
        FillKey(actor, p.key, p.elementId);
    } else if (!g_handKeys[slot].empty()) {
        coop::net::WireKeyFromString(g_handKeys[slot], p.key);
    }
    if (!p.key.len || !sourceCurrent() || g_handPublishRevision[slot]>capture) return false;
    g_handPublishRevision[slot]=capture;
    const auto current=[&]{return sourceCurrent() && g_handPublishRevision[slot]==capture;};
    p.elementId = HandAddress(slot, generation);
    p.playerNo=coop::roster_ledger::Get(slot).playerNo;
    if(!p.playerNo)return false;
    EncodeCondition(condition,p);
    // A join snapshot is sent to one peer only. Do not advance the broadcast baseline from a
    // unicast: existing peers still need the next state change.
    const auto previous = g_handSent[slot];
    const auto key=coop::net::StringFromWireKey(p.key);
    const auto stamp=g_handPublished[slot];
    if (targetSlot < 0 && stamp.generation==generation && stamp.playerNo==p.playerNo &&
        stamp.key==key && !WorthSending(previous, condition)) return true;
    const bool sent = targetSlot < 0
        ? s.SendReliable(coop::net::ReliableKind::FoodState, &p, sizeof(p))
        : s.SendReliableToSlot(targetSlot, coop::net::ReliableKind::FoodState, &p, sizeof(p));
    if(!current()){if(sourceCurrent() && targetSlot<0)g_handSent[slot].sent=false;return false;}
    if (!sent) {if(targetSlot<0)g_handSent[slot].sent=false;return false;}
    if (targetSlot < 0) {
        g_handSent[slot] = Remember(condition);
        g_handKeys[slot]=key;
        g_handPublished[slot]={generation,p.playerNo,key};
    }
    return true;
}

#include "prop_food_hand_state.inc"

sg::Verdict OnTick(const sg::Call& c) {
    if (c.fromOurCode || !F::IsFood(c.object)) return sg::Verdict::Run;
    if(Session() && Session()->connected() && coop::hand_item::IsMirrorActor(c.object))return sg::Verdict::Cancel;
    // The host's tick is the author of food sitting in the world. A client's copy would burn,
    // freeze and spoil on its own. Food in a hand is not in that set: the host poll skips it, so
    // cancelling its tick here would freeze a meal the player is holding.
    if (!IsClient() || coop::hand_item::IsHandAxisActor(c.object)) return sg::Verdict::Run;
    return sg::Verdict::Cancel;
}

sg::Verdict OnConsumedOutput(const sg::Call& c) {
    if(c.fromOurCode)return sg::Verdict::Run;
    auto* s=Session();
    if(s && s->connected() && coop::hand_item::IsMirrorActor(c.object))return sg::Verdict::Cancel;
    // A world food leaves one host-authored wrapper. Personal hand output is
    // native on its owner and admitted by its exact factory's provenance.
    return IsClient() && c.object!=coop::hand_item::LocalHandActor()?sg::Verdict::Cancel:sg::Verdict::Run;
}
sg::Verdict OnCook(const sg::Call& c) {
    return ue_wrap::cooking_food::IsCookingFood(c.object)?OnTick(c):sg::Verdict::Run;
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
    if ((prev.cookProgress<0)!=(now.cookProgress<0) || std::fabs(prev.cookProgress-now.cookProgress)>=.05f)return true;
    if (std::fabs(prev.temperature - now.temperature) >= kTempEps) return true;
    if (std::fabs(prev.ripeness - now.ripeness) >= kRipeEps) return true;
    return false;
}

bool Publish(coop::net::Session& s, void* actor, int slot, bool force = false) {
    if(Session()!=&s || !s.connected() || s.role()!=coop::net::Role::Host)return false;
    EnsureWorld();const auto epoch=g_epoch;const auto world=g_world;const auto capture=++g_publishSequence;
    ue_wrap::CachedObjRef identity;identity.Set(actor);
    const uint16_t person=slot>0 && slot<coop::net::kMaxPeers?coop::roster_ledger::Get(slot).playerNo:0;
    const uint64_t peerEpoch=slot>0 && slot<coop::net::kMaxPeers?g_peerEpoch[slot]:0;
    const auto sourceCurrent=[&]{return Session()==&s && s.connected() && s.role()==coop::net::Role::Host &&
        epoch==g_epoch && world==ue_wrap::world_identity::Generation() && identity.Get() &&
        (slot<0 || (slot>0 && slot<coop::net::kMaxPeers && s.IsSlotWorldReady(slot) &&
        g_peerEpoch[slot]==peerEpoch && coop::roster_ledger::Get(slot).playerNo==person));};
    F::Condition now{};
    if (!sourceCurrent() || !F::Read(actor, now) || !sourceCurrent() || !Valid(now)) return false;
    coop::net::FoodStatePayload p{};FillKey(actor,p.key,p.elementId);
    if(!sourceCurrent() || (!p.key.len && !p.elementId))return false;
    const auto id=p.key.len?coop::net::StringFromWireKey(p.key):L"#"+std::to_wstring(p.elementId);
    if(g_publishRevision[id]>capture)return false;g_publishRevision[id]=capture;
    const auto current=[&]{const auto latest=g_publishRevision.find(id);return sourceCurrent() && latest!=g_publishRevision.end() && latest->second==capture;};
    const auto prev=g_sent[id];
    if(!force && slot<0 && !WorthSending(prev,now))return true;
    EncodeCondition(now,p);
    const bool ok=slot<0?s.SendReliable(coop::net::ReliableKind::FoodState,&p,sizeof(p)):
        s.SendReliableToSlot(slot,coop::net::ReliableKind::FoodState,&p,sizeof(p));
    if(!current()){if(sourceCurrent() && slot<0)g_sent[id].sent=false;return false;}
    if(!ok){if(slot<0)g_sent[id].sent=false;return false;}
    if(slot<0)g_sent[id]=Remember(now);
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

bool ApplyWorldCondition(const coop::net::FoodStatePayload& p,const std::wstring& id,uint64_t revision) {
    const auto epoch=g_epoch;auto* session=Session();
    const auto sourceCurrent=[&]{const auto latest=g_pendingWorld.find(id);return Session()==session && session && session->connected() &&
        session->role()==coop::net::Role::Client && epoch==g_epoch && g_world==ue_wrap::world_identity::Generation() &&
        latest!=g_pendingWorld.end() && latest->second.revision==revision;};
    void* actor=Resolve(p.key,p.elementId);
    if(!sourceCurrent() || !actor || !F::IsFood(actor) || !sourceCurrent())return false;
    ue_wrap::CachedObjRef identity;identity.Set(actor);
    const auto current=[&]{return sourceCurrent() && identity.Get()==actor;};
    const auto c=DecodeCondition(p);
    if(!F::Write(actor,c,current) || !current())return false;
    if(!id.empty()) {
        g_clientBitePending.erase(id);
        g_clientHandSent[id]=g_clientHandCanonical[id]=Remember(c);
    }
    if(c.uses<=0)ue_wrap::engine::DestroyActor(actor);
    return true;
}

void RetryWorldConditions() {
    if(g_retryingWorld)return;
    struct Guard{Guard(){g_retryingWorld=true;}~Guard(){g_retryingWorld=false;}}guard;
    const auto epoch=g_epoch;auto* session=Session();
    const auto count=(std::min)(size_t(8),g_pendingWorld.size());
    for(size_t budget=0;budget<count && !g_pendingWorld.empty();++budget) {
        auto it=g_pendingWorld.upper_bound(g_worldCursor);
        if(it==g_pendingWorld.end())it=g_pendingWorld.begin();
        const auto id=it->first;g_worldCursor=id;
        const auto pending=it->second;
        const bool applied=ApplyWorldCondition(pending.state,id,pending.revision);
        if(epoch!=g_epoch || Session()!=session || g_world!=ue_wrap::world_identity::Generation())return;
        it=g_pendingWorld.find(id);
        if(it==g_pendingWorld.end() || it->second.revision!=pending.revision)continue;
        if(applied)g_pendingWorld.erase(it);
    }
}

#include "prop_food_join_state.inc"

sg::Verdict OnAction(const sg::Call& c) {
    auto* s = Session();
    if (c.fromOurCode || !s || !s->connected() ||
        (s->role() != coop::net::Role::Client && s->role() != coop::net::Role::Host) ||
        !F::IsFood(c.object)) return sg::Verdict::Run;
    EnsureWorld();
    PruneBites();if(!c.invocation || g_bite.size()>=64)return sg::Verdict::Run;
    BitePending b;b.actor=c.object;b.identity.Set(c.object);b.serial=R::SlotSerial(b.identity.Idx());
    b.session=s;b.role=s->role();b.epoch=g_epoch;b.world=g_world;b.invocation=c.invocation;
    b.slot=coop::players::Registry::Get().LocalPeerId();if(b.slot>=coop::net::kMaxPeers)return sg::Verdict::Run;
    b.person=coop::roster_ledger::Get(b.slot).playerNo;b.peerEpoch=g_peerEpoch[b.slot];
    const int32_t off = R::FindParamOffset(c.function, L"action");
    if (off < 0 || off >= R::FunctionFrameSize(c.function) || !c.locals || c.locals[off] != 4)
        return sg::Verdict::Run;
    if (s->role() == coop::net::Role::Client) {
        coop::net::WireKey key{};
        uint32_t eid = 0;
        FillKey(c.object, key, eid);
        if(!b.Current() || !b.identity.Get())return sg::Verdict::Run;
        const std::wstring id = key.len ? coop::net::StringFromWireKey(key)
                                        : (eid ? L"#" + std::to_wstring(eid) : std::wstring{});
        if (!id.empty()) {
            if (g_clientBiteRetry.count(id) || g_clientBiteRetry.size() >= 64) return sg::Verdict::Cancel;
            const uint64_t now = NowMs();
            auto pending = g_clientBitePending.find(id);
            if (pending != g_clientBitePending.end()) {
                if (now - pending->second < kClientBiteAckTimeoutMs) return sg::Verdict::Cancel;
                g_clientBitePending.erase(pending);
            }
        }
    }
    FillKey(c.object, b.request.key, b.request.elementId);
    F::Condition now{};
    if(!b.Current() || !b.identity.Get())return sg::Verdict::Run;
    b.have = F::Read(c.object, now);if(!b.Current() || !b.identity.Get())return sg::Verdict::Run;
    b.uses = now.uses;
    g_bite.push_back(b);
    return sg::Verdict::Run;
}

void OnActionPost(const sg::Call& c) {
    // The post runs for every action on this food, including ones the pre did not record. Popping
    // those would throw away an eat that is still on the stack.
    const int32_t off = R::FindParamOffset(c.function, L"action");
    if (off < 0 || off >= R::FunctionFrameSize(c.function) || !c.locals || c.locals[off] != 4) return;
    if (c.fromOurCode) return;
    EnsureWorld();
    const auto captured=std::find_if(g_bite.rbegin(),g_bite.rend(),[&](const BitePending& b){return b.actor==c.object && b.invocation==c.invocation;});
    if(captured==g_bite.rend())return;const auto b=*captured;g_bite.erase(std::next(captured).base());
    if (!b.have || !b.actor || b.submitted || !b.Current()) return;
    F::Condition now{};
    const bool alive = b.identity.Get() == b.actor;
    // Native final-bite destroys the actor before POST. Its identity must have
    // been captured in PRE; reading getData/key from that dead actor loses the bite.
    if (alive ? (!F::Read(b.actor, now) || !b.Current() || now.uses >= b.uses) : b.uses != 1) return;
    auto* s = Session();
    if (!s || !s->connected()) return;
    if (s->role() == coop::net::Role::Host) {
        if (!alive) return; // native host destruction/output already owns this result
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
    const coop::net::FoodBitePayload p = b.request;
    if (p.key.len == 0 && p.elementId == 0) return;
    SubmitBite(*s, p);
}

}  // namespace

bool ConsumesClientDestroy(void* actor) {
    auto* s = Session();
    PruneBites();if (!s || !IsClient()) return false;
    auto it=std::find_if(g_bite.rbegin(),g_bite.rend(),[&](const BitePending& b){return b.actor==actor && b.Current();});
    if(it==g_bite.rend() || !it->have || it->uses!=1)return false;const auto b=*it;if(b.submitted)return true;
    const bool submitted=SubmitBite(*s,b.request);if(!b.Current())return false;
    it=std::find_if(g_bite.rbegin(),g_bite.rend(),[&](const BitePending& row){return row.invocation==b.invocation && row.actor==actor;});
    if(it!=g_bite.rend())it->submitted=submitted;return submitted;
}

void Install(coop::net::Session* session) {
    if(Session()!=session)OnDisconnect();
    g_session.store(session, std::memory_order_release);
    if (g_watched) return;
    const bool tick = sg::WatchClassName(L"prop_food_C", L"ReceiveTick", kTagTick, &OnTick, nullptr);
    const bool bite = sg::WatchClassName(L"prop_food_C", L"actionOptionIndex", kTagBite, &OnAction, &OnActionPost);
    bool outputs=true;
    for(const auto* cls:{L"prop_foodBox_C",L"prop_food_banana_C",L"prop_food_coffeeCup_C"})
        outputs=sg::WatchClassName(cls,L"eaten",0x46440003,&OnConsumedOutput,nullptr) && outputs;
    const bool cup=sg::WatchClassName(L"prop_food_coffeeCup_C",L"ReceiveTick",kTagTick,&OnTick,nullptr);
    const bool cooking=sg::WatchClassName(L"prop_cookingFood_C",L"ReceiveTick",kTagTick,&OnTick,nullptr);
    // Several recipes declare cookItem outside the prop_cookingFood name prefix.
    // Match the rare verb globally, then require the real cooking ancestry.
    const bool cooked=sg::WatchName(L"cookItem",kTagTick,&OnCook,nullptr);
    g_watched = tick && bite && outputs && cup && cooking && cooked;
}

void Tick() {
    auto* s = Session();
    if (!s || !s->connected()) return;
    EnsureWorld();
    ReconcileHandOwners();
    RetryHandConditions();
    if(Session()!=s || !s->connected() || g_world!=ue_wrap::world_identity::Generation())return;
    const uint64_t now = NowMs();
    if (s->role() == coop::net::Role::Client) {
        RetryWorldConditions();
        if(Session()!=s || !IsClient())return;
        RetryBiteRequests(*s);if(Session()!=s || !IsClient())return;
    }
    if(s->role()==coop::net::Role::Host) {
        RetryFoodJoins();
        if(Session()!=s || !s->connected() || g_world!=ue_wrap::world_identity::Generation())return;
    }
    if (now < g_nextPoll) return;
    g_nextPoll = now + kPollMs;
    if (s->role() == coop::net::Role::Client) {
        PublishLocalHand(*s,now);return;
    }
    if (s->role() != coop::net::Role::Host) return;
    std::vector<void*> foods;
    Collect(foods);
    for (void* a : foods) Publish(*s, a, -1);
    // Host-owned local hand food is shared too; Publish's identity dedupe keeps the world
    // actor and hand actor from producing redundant state when they momentarily coexist.
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    for(uint8_t owner=0;owner<coop::net::kMaxPeers;++owner) {
        void* held=owner==localSlot?coop::hand_item::LocalHandActor():coop::hand_item::MirrorActorForSlot(owner);
        if(!held || !F::IsFood(held))continue;
        const auto generation=owner==localSlot?coop::hand_item::Generation(owner):coop::hand_item::MirrorGeneration(owner);
        BroadcastHandState(*s,held,owner,generation);
    }
}

void Apply(const coop::net::FoodStatePayload& payload) {
    if (!IsClient()) return;
    EnsureWorld();
    ReconcileHandOwners();
    if (!Valid(DecodeCondition(payload)) || !ValidFoodKey(payload)) return;
    const std::wstring id = payload.key.len ? coop::net::StringFromWireKey(payload.key)
                                            : (payload.elementId ? L"#" + std::to_wstring(payload.elementId)
                                                                 : std::wstring{});
    uint8_t handSlot = 0;
    uint32_t handGeneration = 0;
    if (DecodeHandAddress(payload.elementId, handSlot, handGeneration)) {
        QueueHandCondition(payload,handSlot,handGeneration);RetryHandConditions();return;
    }
    if((payload.elementId&0x80000000u) || payload.playerNo)return;
    // A world sample can overtake its spawn. Keep it for the next hand generation.
    // Hand retries are separately fenced by owner and generation above.
    if (!id.empty()) g_clientHandCanonical[id] = Remember(DecodeCondition(payload));
    if(id.empty())return;
    auto it=g_pendingWorld.find(id);
    if(it==g_pendingWorld.end()) {
        if(g_pendingWorld.size()>=1024)return;
    }
    g_pendingWorld[id]={payload,++g_worldRevision};
    RetryWorldConditions();
}

void OnBite(const coop::net::FoodBitePayload& payload, uint8_t senderSlot) {
    auto* s = Session();
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) return;
    EnsureWorld();
    ReconcileHandOwners();
    if (senderSlot < 1 || senderSlot >= coop::net::kMaxPeers || !s->IsSlotWorldReady(senderSlot) ||
        payload.key.len>sizeof(payload.key.data)) return;
    const auto person=coop::roster_ledger::Get(senderSlot).playerNo;if(!person)return;
    const auto epoch=g_epoch,peerEpoch=g_peerEpoch[senderSlot];
    const auto current=[&]{return Session()==s && s->connected() && epoch==g_epoch &&
        s->role()==coop::net::Role::Host && s->IsSlotWorldReady(senderSlot) && g_peerEpoch[senderSlot]==peerEpoch &&
        g_world==ue_wrap::world_identity::Generation() && coop::roster_ledger::Get(senderSlot).playerNo==person;};
    const std::wstring key = coop::net::StringFromWireKey(payload.key);
    void* actor = coop::hand_item::MirrorActorForSlot(senderSlot);
    if (!F::IsFood(actor) || g_handKeys[senderSlot] != key) actor = Resolve(payload.key, payload.elementId);
    if (!actor || !F::IsFood(actor)) return;
    ue_wrap::CachedObjRef identity;identity.Set(actor);
    const auto handGeneration=coop::hand_item::MirrorGeneration(senderSlot);const bool held=actor==coop::hand_item::MirrorActorForSlot(senderSlot);
    const auto valid=[&]{return current() && identity.Get() && (!held || (coop::hand_item::MirrorActorForSlot(senderSlot)==actor && coop::hand_item::MirrorGeneration(senderSlot)==handGeneration && g_handKeys[senderSlot]==key));};
    auto target = coop::element::IntentTarget::ForClientIntent(*s, senderSlot, kBiteReach);
    if (!target.Authorize(actor) || !valid()) return;
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
    if (!F::Read(actor, c) || c.uses <= 0 || !valid()) return;
    g_biteAt[token] = now;
    c.uses -= 1;
    if (!F::Write(actor, c,valid) || !valid()) return;
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
        if (c.uses <= 0 && current() && identity.Get()) {
            // These verified overrides create only the empty wrapper/husk/cup.
            // Native bitten, poison, coffee and hunger already ran on the owner.
            void* fn = R::FindDispatchFunctionCached(R::ClassOf(actor), L"eaten");
            void* owner = fn ? R::OuterOf(fn) : nullptr;
            bool output=false;
            if(owner)for(const auto* cls:{L"prop_foodBox_C",L"prop_food_banana_C",L"prop_food_coffeeCup_C"})
                output=R::NameEquals(R::NameOf(owner),cls) || output;
            if (output) {
                ue_wrap::ParamFrame frame(fn);
                if (frame.valid() && frame.Set<void*>(L"player", nullptr) && valid()) ue_wrap::Call(actor, frame);
            }
            if (current() && identity.Get()) ue_wrap::engine::DestroyActor(actor);
        }
    }
}

void OnHandState(const coop::net::FoodStatePayload& payload, uint8_t senderSlot) {
    auto* s = Session();
    if (!s || s->role() != coop::net::Role::Host || senderSlot == 0 ||
        senderSlot >= coop::net::kMaxPeers || !payload.key.len ||
        !s->connected() || !s->IsSlotWorldReady(senderSlot) ||
        !Valid(DecodeCondition(payload)) || !ValidFoodKey(payload)) return;
    EnsureWorld();
    ReconcileHandOwners();
    uint8_t addressedSlot = 0;
    uint32_t generation = 0;
    if (!DecodeHandAddress(payload.elementId, addressedSlot, generation) || addressedSlot != senderSlot)return;
    QueueHandCondition(payload,addressedSlot,generation);RetryHandConditions();
}

void SendAllToSlot(int slot) {
    auto* s=Session();if(!s || !s->connected() || s->role()!=coop::net::Role::Host || slot<=0 || slot>=coop::net::kMaxPeers)return;
    EnsureWorld();ReconcileHandOwners();const auto epoch=g_epoch;
    const auto current=[&]{return Session()==s && s->connected() && s->role()==coop::net::Role::Host && epoch==g_epoch && g_world==ue_wrap::world_identity::Generation();};
    std::vector<void*> foods;Collect(foods);if(!current())return;
    for(void* actor:foods){QueueFoodJoin(actor,slot);if(!current())return;}
    for(uint8_t owner=0;owner<coop::net::kMaxPeers;++owner)if(owner!=slot)QueueHandFoodJoin(owner,slot);
    RetryFoodJoins();
}

void OnDisconnect() {
    ++g_epoch;g_pendingWorld.clear();g_worldCursor.clear();
    g_pendingHand={};g_handOccupants={};
    g_handPublished={};g_handAccepted={};g_handPublishRevision={};
    g_handJoin={};g_handJoinTarget={};g_joinFoods.clear();g_joinCursor.clear();
    g_sent.clear();g_publishRevision.clear();
    g_clientHandSent.clear();
    g_clientHandSentAt.clear();
    g_clientHandCanonical.clear();
    g_clientHandGeneration.clear();
    std::fill(std::begin(g_handSent), std::end(g_handSent), Last{});
    std::fill(std::begin(g_handKeys), std::end(g_handKeys), std::wstring{});
    g_biteAt.clear();
    g_clientBitePending.clear();
    g_clientBiteRetry.clear();g_biteRetryCursor.clear();
    g_bite.clear();
    g_nextPoll = 0;
    g_world = 0;
    g_session.store(nullptr, std::memory_order_release);
}

void OnPeerLeft(int slot) {
    if(slot<0 || slot>=coop::net::kMaxPeers)return;++g_peerEpoch[slot];
    g_pendingHand[slot]={};g_handOccupants[slot]=0;g_handKeys[slot].clear();g_handSent[slot]={};g_handPublished[slot]={};g_handAccepted[slot]={};
    g_handJoin[slot]=0;
    g_handJoinTarget[slot]={};for(auto& targets:g_handJoinTarget)targets[slot]={};
    for(auto& mask:g_handJoin)mask&=uint32_t(~(1u<<slot));
    std::erase_if(g_joinFoods,[slot](auto& entry){entry.second.pending&=uint32_t(~(1u<<slot));return !entry.second.pending;});
}

void ForgetPublished(const std::wstring& key) { g_sent.erase(key); }

bool PublishSpawnState(coop::net::Session& s, void* actor, int slot) {
    if (!s.connected() || s.role() != coop::net::Role::Host) return false;
    return Publish(s, actor, slot, true);
}

}  // namespace coop::props::prop_food_sync
