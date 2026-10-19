// coop/world/credit_intent.cpp -- see credit_intent.h.

#include "coop/world/credit_intent.h"
#include "coop/world/balance_sync.h"

#include "coop/element/element.h"
#include "coop/element/intent_authority.h"
#include "coop/element/mirror_manager.h"
#include "coop/element/prop.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_save_data.h"
#include "coop/props/remote_prop.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/data_table.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_pawn.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/economy.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace coop::credit_intent {
namespace {

namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace DT = ue_wrap::data_table;

constexpr int      kTagSack  = 0x43520001;
constexpr int      kTagChest = 0x43520002;
constexpr int      kTagKey   = 0x43520003;
constexpr int      kTagAch   = 0x43520004;
constexpr int      kTagMiner = 0x43520005;
constexpr float    kUseReach = 500.f;
constexpr float    kKeyReach = 1500.f;
constexpr float    kKeyGapCm = 250.f;
constexpr int32_t  kMaxAward = 100000;
constexpr uint64_t kChestPollMs = 250;
// Treasure-chest actionOptionIndex: 4 opens (needs the key), 7 takes the gold (the +5000).
constexpr uint8_t  kActionOpen = 4;
constexpr uint8_t  kActionGold = 7;

const wchar_t* kOverlap =
    L"BndEvt__prop_treasureChest_Sphere_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature";

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_watched = false;
uint64_t g_nextChest = 0;

struct AchPending {
    std::wstring name;
    int32_t points = 0;
    bool have = false;
};
std::vector<AchPending> g_ach;

std::unordered_map<std::wstring, int32_t> g_awards;
bool g_awardsReady = false;
std::unordered_map<std::string, int> g_achCount;          // row -> times credited this session
std::unordered_set<std::string> g_achSlot;                // "slot|row" already credited
std::unordered_map<std::wstring, uint8_t> g_chestSent;    // key -> last flags

int32_t g_chestKeyByte = -2, g_chestOpenByte = -2, g_chestGoldByte = -2;
uint8_t g_chestKeyMask = 0, g_chestOpenMask = 0, g_chestGoldMask = 0;

coop::net::Session* Session() { return g_session.load(std::memory_order_acquire); }

bool IsClient() {
    auto* s = Session();
    return s && s->connected() && s->role() == coop::net::Role::Client;
}

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool IsA(void* actor, const wchar_t* leaf) {
    if (!actor) return false;
    void* base = R::FindClass(leaf);
    if (!base) return false;
    void* cls = R::ClassOf(actor);
    return cls && (cls == base || R::IsDescendantOfAny(cls, &base, 1));
}

bool IsSack(void* actor) {
    if (!actor) return false;
    const std::wstring n = R::ClassNameOf(actor);
    return n.rfind(L"prop_pointSack", 0) == 0;
}

void FillIds(void* actor, coop::net::WireKey& key, uint32_t& eid) {
    key = {};
    eid = 0;
    const std::wstring k = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!k.empty() && k != L"None") coop::net::WireKeyFromString(k, key);
    const auto id = coop::prop_element_tracker::GetPropElementIdForActor(actor);
    if (id != coop::element::kInvalidId) eid = static_cast<uint32_t>(id);
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

bool InReach(coop::net::Session& s, uint8_t slot, void* actor, float reach) {
    auto target = coop::element::IntentTarget::ForClientIntent(s, slot, reach);
    return static_cast<bool>(target.Authorize(actor));
}

void SendIntent(const coop::net::CreditIntentPayload& p) {
    auto* s = Session();
    if (!s) return;
    s->SendReliable(coop::net::ReliableKind::CreditIntent, &p, sizeof(p));
}

uint8_t ActionOf(const sg::Call& c) {
    const int32_t off = R::FindParamOffset(c.function, L"action");
    if (off < 0 || !c.locals) return 0xFF;
    return c.locals[off];
}

sg::Verdict OnSack(const sg::Call& c) {
    if (c.fromOurCode || !IsClient() || !IsSack(c.object)) return sg::Verdict::Run;
    coop::net::CreditIntentPayload p{};
    p.kind = static_cast<uint8_t>(coop::net::CreditKind::Sack);
    p.action = ActionOf(c);
    FillIds(c.object, p.key, p.elementId);
    if (p.key.len == 0 && p.elementId == 0) return sg::Verdict::Run;
    SendIntent(p);
    return sg::Verdict::Cancel;
}

void OnMinerPost(const sg::Call& c) {
    if (!IsA(c.object, L"prop_miner_C") || ActionOf(c) != 4) return;
    auto* s = Session();
    if (s && s->connected() && s->role() == coop::net::Role::Host) {
        coop::balance_sync::Tick();
        coop::net::WireKey key{};
        uint32_t eid = 0;
        FillIds(c.object, key, eid);
        const std::wstring keyString = coop::net::StringFromWireKey(key);
        if (!keyString.empty() && keyString != L"None")
            coop::prop_save_data::Publish(s, c.object, keyString);
    }
}

sg::Verdict OnMiner(const sg::Call& c) {
    if (c.fromOurCode || !IsClient() || !IsA(c.object, L"prop_miner_C")) return sg::Verdict::Run;
    // prop_miner's action enum value 4 is the rack laptop's token-claim action.
    if (ActionOf(c) != 4) return sg::Verdict::Run;
    coop::net::CreditIntentPayload p{};
    p.kind = static_cast<uint8_t>(coop::net::CreditKind::MiningFarm);
    p.action = 4;
    FillIds(c.object, p.key, p.elementId);
    if (p.key.len == 0 && p.elementId == 0) return sg::Verdict::Run;
    SendIntent(p);
    return sg::Verdict::Cancel;
}

sg::Verdict OnChest(const sg::Call& c) {
    if (c.fromOurCode || !IsClient() || !IsA(c.object, L"prop_treasureChest_C")) return sg::Verdict::Run;
    const uint8_t action = ActionOf(c);
    if (action != kActionOpen && action != kActionGold) return sg::Verdict::Run;
    coop::net::CreditIntentPayload p{};
    p.kind = action == kActionOpen ? static_cast<uint8_t>(coop::net::CreditKind::ChestOpen)
                                   : static_cast<uint8_t>(coop::net::CreditKind::ChestGold);
    p.action = action;
    FillIds(c.object, p.key, p.elementId);
    if (p.key.len == 0 && p.elementId == 0) return sg::Verdict::Run;
    SendIntent(p);
    return sg::Verdict::Cancel;
}

sg::Verdict OnKeyOverlap(const sg::Call& c) {
    if (c.fromOurCode || !IsClient() || !IsA(c.object, L"prop_treasureChest_C")) return sg::Verdict::Run;
    const int32_t off = R::FindParamOffset(c.function, L"OtherActor");
    if (off < 0 || !c.locals) return sg::Verdict::Run;
    void* other = *reinterpret_cast<void**>(c.locals + off);
    if (!IsA(other, L"prop_treasureKey_C")) return sg::Verdict::Run;
    coop::net::CreditIntentPayload p{};
    p.kind = static_cast<uint8_t>(coop::net::CreditKind::ChestKey);
    FillIds(c.object, p.key, p.elementId);
    uint32_t keyEid = 0;
    coop::net::WireKey ignored{};
    FillIds(other, ignored, keyEid);
    p.auxId = keyEid;
    // The chest is the artifact. A key in a hand often has no element id; the host then takes the
    // key that is actually touching this chest. Cancelling only when the chest itself is nameless
    // leaves a hand-key's own overlap to run, which the next flag broadcast would undo.
    if (p.key.len == 0 && p.elementId == 0) return sg::Verdict::Run;
    SendIntent(p);
    return sg::Verdict::Cancel;
}

std::wstring NameOf(const sg::Call& c) {
    const int32_t off = R::FindParamOffset(c.function, L"achievement");
    if (off < 0 || !c.locals) return {};
    const auto name = *reinterpret_cast<const R::FName*>(c.locals + off);
    return R::ToString(name);
}

void OnAchPre(const sg::Call& c) {
    if (c.fromOurCode || !IsClient()) return;
    AchPending row;
    row.name = NameOf(c);
    row.have = ue_wrap::economy::ReadPoints(&row.points);
    g_ach.push_back(std::move(row));
}

void OnAchPost(const sg::Call& c) {
    // Same guard as the pre. A nested call that did not push must not pop the achievement
    // that is still in progress.
    if (c.fromOurCode || !IsClient()) return;
    if (g_ach.empty()) return;
    AchPending row = std::move(g_ach.back());
    g_ach.pop_back();
    if (!IsClient() || row.name.empty() || !row.have) return;
    int32_t after = 0;
    if (!ue_wrap::economy::ReadPoints(&after) || after <= row.points) return;
    coop::net::CreditIntentPayload p{};
    p.kind = static_cast<uint8_t>(coop::net::CreditKind::Achievement);
    size_t n = 0;
    for (wchar_t ch : row.name) {
        if (n + 1 >= sizeof(p.name) || ch <= 0 || ch >= 128) return;
        p.name[n++] = static_cast<char>(ch);
    }
    p.name[n] = 0;
    SendIntent(p);
}

sg::Verdict OnAchPreGate(const sg::Call& c) {
    OnAchPre(c);
    return sg::Verdict::Run;
}

bool ResolveChestBools(void* chest) {
    if (g_chestKeyByte != -2) return g_chestKeyByte >= 0;
    void* cls = R::ClassOf(chest);
    if (!cls) return false;
    const bool key = R::FindBoolProperty(cls, L"hasKey", g_chestKeyByte, g_chestKeyMask);
    const bool opened = R::FindBoolProperty(cls, L"opened", g_chestOpenByte, g_chestOpenMask);
    const bool gold = R::FindBoolProperty(cls, L"hasGold", g_chestGoldByte, g_chestGoldMask);
    if (!key || !opened || !gold) {
        g_chestKeyByte = -1;
        UE_LOGW("credit: a treasure chest is missing hasKey, opened or hasGold -- its flags stay local");
        return false;
    }
    return true;
}

bool ReadFlag(void* actor, int32_t byte, uint8_t mask) {
    if (!actor || byte < 0 || mask == 0) return false;
    const uint8_t b = *reinterpret_cast<const uint8_t*>(reinterpret_cast<const uint8_t*>(actor) + byte);
    return (b & mask) != 0;
}

void WriteFlag(void* actor, int32_t byte, uint8_t mask, bool on) {
    if (!actor || byte < 0 || mask == 0) return;
    uint8_t& b = *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(actor) + byte);
    if (on) b = static_cast<uint8_t>(b | mask);
    else    b = static_cast<uint8_t>(b & static_cast<uint8_t>(~mask));
}

uint8_t FlagsOf(void* chest) {
    if (!ResolveChestBools(chest)) return 0;
    uint8_t f = 0;
    if (ReadFlag(chest, g_chestKeyByte, g_chestKeyMask))   f |= coop::net::kChestFlagKey;
    if (ReadFlag(chest, g_chestOpenByte, g_chestOpenMask)) f |= coop::net::kChestFlagOpened;
    if (ReadFlag(chest, g_chestGoldByte, g_chestGoldMask)) f |= coop::net::kChestFlagGold;
    return f;
}

void ApplyFlagsLocal(void* chest, uint8_t flags) {
    if (!ResolveChestBools(chest)) return;
    WriteFlag(chest, g_chestKeyByte, g_chestKeyMask, (flags & coop::net::kChestFlagKey) != 0);
    WriteFlag(chest, g_chestOpenByte, g_chestOpenMask, (flags & coop::net::kChestFlagOpened) != 0);
    WriteFlag(chest, g_chestGoldByte, g_chestGoldMask, (flags & coop::net::kChestFlagGold) != 0);
    void* fn = R::FindFunction(R::ClassOf(chest), L"upd");
    if (!fn) return;
    ue_wrap::ParamFrame frame(fn);
    if (frame.valid()) ue_wrap::Call(chest, frame);
}

void BroadcastFlags(coop::net::Session& s, void* chest, int slot) {
    const uint8_t flags = FlagsOf(chest);
    coop::net::ChestFlagsPayload p{};
    FillIds(chest, p.key, p.elementId);
    if (p.key.len == 0 && p.elementId == 0) return;
    std::wstring k = coop::net::StringFromWireKey(p.key);
    if (k.empty()) k = L"#" + std::to_wstring(p.elementId);
    if (slot < 0) {
        const auto it = g_chestSent.find(k);
        if (it != g_chestSent.end() && it->second == flags) return;
    }
    p.flags = flags;
    const bool ok = slot < 0
        ? s.SendReliable(coop::net::ReliableKind::ChestFlags, &p, sizeof(p))
        : s.SendReliableToSlot(slot, coop::net::ReliableKind::ChestFlags, &p, sizeof(p));
    if (ok) g_chestSent[k] = flags;
}

struct ChestWalk { void* world; void* base; std::vector<void*> actors; };

void OnChestInst(void* ctx, void* obj, int32_t) {
    auto* w = static_cast<ChestWalk*>(ctx);
    if (!obj || !R::IsLive(obj) || !IsA(obj, L"prop_treasureChest_C")) return;
    if (w->world && ue_wrap::world_identity::WorldOf(obj) != w->world) return;
    if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
    w->actors.push_back(obj);
}

void OnChestClass(void* ctx, void* cls, void*) {
    auto* w = static_cast<ChestWalk*>(ctx);
    if (!cls || !w->base || !R::IsDescendantOfAny(cls, &w->base, 1)) return;
    ue_wrap::object_index::ForEachInstance(cls, &OnChestInst, ctx);
}

void CollectChests(std::vector<void*>& out) {
    void* base = R::FindClass(L"prop_treasureChest_C");
    if (!base || !ue_wrap::object_index::IsSeeded()) return;
    ChestWalk w{ ue_wrap::world_identity::CurrentWorld(), base, {} };
    ue_wrap::object_index::ForEachClass(&OnChestClass, &w);
    out.swap(w.actors);
}

bool RunAction(void* actor, uint8_t action) {
    void* fn = R::FindFunction(R::ClassOf(actor), L"actionOptionIndex");
    if (!fn) return false;
    ue_wrap::ParamFrame frame(fn);
    if (!frame.valid()) return false;
    frame.Set<void*>(L"player", coop::players::Registry::Get().Local());
    frame.Set<uint8_t>(L"action", action);
    return ue_wrap::Call(actor, frame);
}

float Dist(void* a, void* b) {
    ue_wrap::FVector pa{}, pb{};
    if (!ue_wrap::engine::TryGetActorLocation(a, pa) || !ue_wrap::engine::TryGetActorLocation(b, pb))
        return 1.0e9f;
    const float dx = pa.X - pb.X, dy = pa.Y - pb.Y, dz = pa.Z - pb.Z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void EnsureAwards() {
    if (g_awardsReady) return;
    void* table = R::FindObject(L"list_achievements", L"DataTable");
    if (!table) return;
    g_awardsReady = true;
    void* rowStruct = DT::RowStruct(table);
    const std::wstring member = rowStruct ? DT::MemberName(rowStruct, L"award_") : std::wstring();
    if (member.empty()) {
        UE_LOGW("credit: list_achievements has no award column -- an achievement earns nothing here");
        return;
    }
    std::vector<DT::RowRef> rows;
    std::vector<std::wstring> awards;
    if (!DT::Rows(table, rows, "credit") || !DT::ColumnAsStrings(table, member, awards)) return;
    const size_t n = rows.size() < awards.size() ? rows.size() : awards.size();
    for (size_t i = 0; i < n; ++i) {
        const std::wstring name = R::ToString(rows[i].key);
        wchar_t* end = nullptr;
        const long v = std::wcstol(awards[i].c_str(), &end, 10);
        if (!end || end == awards[i].c_str()) continue;
        g_awards[name] = static_cast<int32_t>(v);
    }
}

std::string SlotKey(uint8_t slot, const std::string& name) {
    return std::to_string(static_cast<int>(slot)) + "|" + name;
}

void CreditAchievement(uint8_t slot, const char* name) {
    if (!name || !name[0]) return;
    std::wstring wide;
    for (const char* c = name; *c; ++c) {
        if (static_cast<unsigned char>(*c) >= 128) return;
        wide.push_back(static_cast<wchar_t>(*c));
    }
    const std::string narrow(name);
    const std::string sk = SlotKey(slot, narrow);
    if (g_achSlot.count(sk)) return;
    if (g_achCount[narrow] >= static_cast<int>(coop::net::kMaxPeers)) {
        UE_LOGW("credit: achievement '%s' already credited %d times this session -- refusing slot %u",
                name, g_achCount[narrow], static_cast<unsigned>(slot));
        return;
    }
    EnsureAwards();
    const auto it = g_awards.find(wide);
    if (it == g_awards.end()) {
        UE_LOGW("credit: achievement '%s' is not a row of list_achievements -- refusing", name);
        return;
    }
    if (it->second < 0 || it->second > kMaxAward) {
        UE_LOGW("credit: achievement '%s' award %d is outside 0..%d -- refusing",
                name, it->second, kMaxAward);
        return;
    }
    g_achSlot.insert(sk);
    g_achCount[narrow] += 1;
    if (it->second > 0) ue_wrap::economy::AddPoints(it->second);
    UE_LOGI("credit: slot %u achievement '%s' awarded %d (the row's own award)",
            static_cast<unsigned>(slot), name, it->second);
}

void CreditSack(coop::net::Session& s, const coop::net::CreditIntentPayload& p, uint8_t slot) {
    void* actor = Resolve(p.key, p.elementId);
    if (!actor || !IsSack(actor)) return;
    if (!R::IsLive(actor)) return;
    if (!InReach(s, slot, actor, kUseReach)) return;
    const int32_t before = [&] { int32_t v = 0; ue_wrap::economy::ReadPoints(&v); return v; }();
    if (!RunAction(actor, p.action == 0xFF ? 0 : p.action)) return;
    int32_t after = 0;
    ue_wrap::economy::ReadPoints(&after);
    UE_LOGI("credit: slot %u sack credited (%d -> %d)", static_cast<unsigned>(slot), before, after);
}

void CreditMiner(coop::net::Session& s, const coop::net::CreditIntentPayload& p, uint8_t slot) {
    void* miner = Resolve(p.key, p.elementId);
    if (!miner || !IsA(miner, L"prop_miner_C") || !R::IsLive(miner)) return;
    if (p.action != 4 || !InReach(s, slot, miner, kUseReach)) return;
    if (!RunAction(miner, 4)) return;
}

void CreditChest(coop::net::Session& s, const coop::net::CreditIntentPayload& p, uint8_t slot) {
    void* chest = Resolve(p.key, p.elementId);
    if (!chest || !IsA(chest, L"prop_treasureChest_C") || !R::IsLive(chest)) return;
    const auto kind = static_cast<coop::net::CreditKind>(p.kind);
    if (kind == coop::net::CreditKind::ChestKey) {
        void* key = p.auxId ? Resolve({}, p.auxId) : nullptr;
        if (!key || !IsA(key, L"prop_treasureKey_C") || !R::IsLive(key)) {
            // No id on the overlapping key. The sender's held prop is that key when they carried
            // it in; any other key near the chest stays where it is.
            void* driven = coop::remote_prop::DrivenActor(static_cast<int>(slot));
            if (driven && R::IsLive(driven) && IsA(driven, L"prop_treasureKey_C"))
                key = driven;
            else
                key = nullptr;
        }
        if (!key || !IsA(key, L"prop_treasureKey_C") || !R::IsLive(key)) return;
        if (!InReach(s, slot, chest, kKeyReach)) return;
        if (Dist(key, chest) > kKeyGapCm) return;
        if (FlagsOf(chest) & coop::net::kChestFlagKey) return;
        ue_wrap::engine::DestroyActor(key);
        if (ResolveChestBools(chest))
            WriteFlag(chest, g_chestKeyByte, g_chestKeyMask, true);
        ApplyFlagsLocal(chest, FlagsOf(chest));
    } else {
        if (!InReach(s, slot, chest, kUseReach)) return;
        const uint8_t before = FlagsOf(chest);
        if (kind == coop::net::CreditKind::ChestOpen && (before & coop::net::kChestFlagOpened)) return;
        if (kind == coop::net::CreditKind::ChestGold && !(before & coop::net::kChestFlagGold)) return;
        RunAction(chest, kind == coop::net::CreditKind::ChestOpen ? kActionOpen : kActionGold);
    }
    {
        std::wstring ck = coop::net::StringFromWireKey(p.key);
        if (ck.empty()) ck = L"#" + std::to_wstring(p.elementId);
        g_chestSent.erase(ck);
    }
    BroadcastFlags(s, chest, -1);
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_watched) return;
    g_watched = true;
    sg::WatchClassName(L"prop_pointSack_C", L"actionOptionIndex", kTagSack, &OnSack, nullptr);
    sg::WatchClassName(L"prop_miner_C", L"actionOptionIndex", kTagMiner, &OnMiner, &OnMinerPost);
    sg::WatchClassName(L"prop_treasureChest_C", L"actionOptionIndex", kTagChest, &OnChest, nullptr);
    sg::WatchClassName(L"prop_treasureChest_C", kOverlap, kTagKey, &OnKeyOverlap, nullptr);
    sg::WatchClassName(L"save_main_C", L"progressAchievement", kTagAch, &OnAchPreGate, &OnAchPost);
}

void Tick() {
    auto* s = Session();
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) return;
    const uint64_t now = NowMs();
    if (now < g_nextChest) return;
    g_nextChest = now + kChestPollMs;
    std::vector<void*> chests;
    CollectChests(chests);
    for (void* c : chests) BroadcastFlags(*s, c, -1);
}

void OnIntent(const coop::net::CreditIntentPayload& payload, uint8_t senderSlot) {
    auto* s = Session();
    if (!s || s->role() != coop::net::Role::Host) return;
    if (senderSlot < 1 || senderSlot >= coop::net::kMaxPeers) return;
    switch (static_cast<coop::net::CreditKind>(payload.kind)) {
    case coop::net::CreditKind::Sack:        CreditSack(*s, payload, senderSlot); break;
    case coop::net::CreditKind::MiningFarm:  CreditMiner(*s, payload, senderSlot); break;
    case coop::net::CreditKind::ChestKey:
    case coop::net::CreditKind::ChestOpen:
    case coop::net::CreditKind::ChestGold:   CreditChest(*s, payload, senderSlot); break;
    case coop::net::CreditKind::Achievement: CreditAchievement(senderSlot, payload.name); break;
    default: break;
    }
}

void ApplyFlags(const coop::net::ChestFlagsPayload& payload) {
    if (!IsClient()) return;
    void* chest = Resolve(payload.key, payload.elementId);
    if (!chest || !IsA(chest, L"prop_treasureChest_C")) return;
    ApplyFlagsLocal(chest, payload.flags);
}

void SendChestsToSlot(int slot) {
    auto* s = Session();
    if (!s || s->role() != coop::net::Role::Host || slot <= 0) return;
    std::vector<void*> chests;
    CollectChests(chests);
    for (void* c : chests) BroadcastFlags(*s, c, slot);
}

void OnDisconnect() {
    g_ach.clear();
    g_achCount.clear();
    g_achSlot.clear();
    g_chestSent.clear();
    g_nextChest = 0;
    g_session.store(nullptr, std::memory_order_release);
}

}  // namespace coop::credit_intent
