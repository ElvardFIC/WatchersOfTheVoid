#include "coop/world/placed_save_sync.h"
#include "coop/world/garden_actions.h"
#include "coop/world/wall_build_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/element/object_scan_hub.h"
#include "coop/items/save_record_wire.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/player/hand_item.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/actors/llama_marker.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/keyed_objects.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <deque>
#include <map>
#include <random>
#include <set>
#include <vector>
namespace coop::placed_save_sync {
namespace {
UE_FACT_FN(kFinishSpawningRow, "placed_save", L"GameplayStatics", L"FinishSpawningActor", Declared);
namespace P = ue_wrap::placed_save;
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
namespace W = coop::save_record_wire;
using Record = ue_wrap::save_record::SaveRecord;
enum Op : uint8_t { Create, Remove, State, Retired, Refresh, Harvest, Care, HoseRoute, HoseReply };
constexpr int kTag = 0x504C5341;
constexpr size_t kMaxRows = 1024;
constexpr uint64_t kRetryMs = 1000;
struct Row {
    ue_wrap::CachedObjRef ref;
    Record record;
    bool local = false, ready = false, accepted = false, incoming = false, settled = true;
    bool dead = false, remove = false, dirty = false, link = false, retireLookup = false, spawnedForClient=false;
    uint8_t owner = 0;
    uint32_t playerNo = 0;
    uint32_t hoseStateSeq = 0;
    bool hoseStateSeen = false;
    uint64_t next = 0, linkAt = 0, manualUntil = 0, pollAt = 0;
};
void ResetHoseState();
bool Reach(uint8_t slot, const Record& record);
void Dead(Row& row);
std::map<std::wstring, Row> g_rows;
net::Session* g_session = nullptr;
blob_chunks::Assembler g_assembler;
ue_wrap::CachedObjRef g_finish;
uint32_t g_world = 0, g_nonce = 0, g_keySeq = 0, g_blobSeq = 0;
uint64_t g_epoch = 0, g_sweep = 0, g_tickAt = 0;
int g_applying = 0;
bool g_busy = false, g_watched = false;
bool g_scanRegistered = false;
std::map<int32_t, ue_wrap::CachedObjRef> g_discoveredPlaced;
std::deque<ue_wrap::CachedObjRef> g_enrollPlaced;
std::wstring g_pollCursor;
struct Use {
    std::wstring key; ue_wrap::CachedObjRef ref; uint32_t seq = 0; uint64_t expires = 0;
    Op op=Harvest; P::CareUse care{};
    uint16_t person=0;uint8_t slot=0;uint64_t peerEpoch=0;
};
struct Used { uint32_t seq = 0, playerNo = 0; uint64_t at = 0; };
std::deque<Use> g_uses;
std::array<Used, players::kMaxPeers> g_used{};
uint32_t g_useSeq = 0;
struct CareFrame {
    ue_wrap::CachedObjRef tool, plant;
    uint64_t invocation=0;
    P::CareUse care; std::wstring key; uint64_t epoch=0,peerEpoch=0;
    uint32_t world=0;uint16_t person=0;uint8_t slot=0;
};
std::vector<CareFrame> g_careFrames;
std::set<std::pair<uint32_t,std::wstring>> g_spentFertilizers;
ue_wrap::CachedObjRef g_harvestCollector;
struct Applying { Applying() { ++g_applying; } ~Applying() { --g_applying; } };
struct HarvestCollector {
    ue_wrap::CachedObjRef previous = g_harvestCollector;
    explicit HarvestCollector(void* actor) { g_harvestCollector.Set(actor); }
    ~HarvestCollector() { g_harvestCollector = previous; }
};
uint64_t Now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool Host() { return g_session && g_session->role() == net::Role::Host; }
bool Active() { return g_session && g_session->connected() && ue_wrap::game_thread::IsGameThread(); }
bool Tracking() { return g_session && g_session->running() && ue_wrap::game_thread::IsGameThread() && (Host() || g_session->connected()); }
void ResetWorld() {
    g_rows.clear(); g_assembler.Clear(); g_sweep = g_tickAt = 0; ++g_epoch;
    g_discoveredPlaced.clear(); g_enrollPlaced.clear(); g_pollCursor.clear();
    g_uses.clear(); g_used = {};
    g_careFrames.clear(); g_spentFertilizers.clear();
    ResetHoseState();
    g_world = ue_wrap::world_identity::Generation();
}
void World() { if (g_world != ue_wrap::world_identity::Generation()) ResetWorld(); }
bool Ours(const std::wstring& key) {
    if (key.size() != 22 || key.compare(0, 5, L"mvpl_") || key[13] != L'_') return false;
    for (size_t i = 5; i < key.size(); ++i) {
        if (i == 13) continue;
        if (!((key[i] >= L'0' && key[i] <= L'9') || (key[i] >= L'a' && key[i] <= L'f'))) return false;
    }
    return true;
}
std::wstring Mint() {
    if (!g_nonce) { std::random_device rd; g_nonce = uint32_t(rd()) | 1u; }
    wchar_t key[32];
    swprintf(key, 32, L"mvpl_%08x_%08x", g_nonce, ++g_keySeq);
    return key;
}
bool Send(Op op, const Record& record, uint32_t useSeq = 0, const P::CareUse* care=nullptr,const std::function<bool()>& current={},uint16_t person=0) {
    if((op==Care || op==Harvest) && !person)return false;
    std::vector<uint8_t> bytes;
    W::AppU8(bytes, op);
    if (op == Remove || op == Retired || op == Refresh || op == Harvest || op == Care) W::AppWStr(bytes, record.key);
    else W::SerSave(bytes, record);
    if (op == Harvest || op == Care) {W::AppU32(bytes,useSeq);W::AppU32(bytes,person);}
    if (op == Care && care) { W::AppU8(bytes,uint8_t(care->kind));W::AppWStr(bytes,care->sourceKey);W::AppF32(bytes,care->amount); }
    return current?blob_chunks::SendBlobWhile(g_session,net::ReliableKind::PlacedSave,++g_blobSeq,bytes,current):
        blob_chunks::SendBlob(g_session, net::ReliableKind::PlacedSave, ++g_blobSeq, bytes);
}
bool ValidKey(const std::wstring& key) {
    if (key.empty() || key == L"None" || key.size() > 128) return false;
    for (auto c : key) if (c < 32 || c > 126) return false;
    return true;
}
// Discovery rides the shared sliced index pass. Never run getData/getKey from
// its callback: those can reenter the engine or allocate objects.
bool LegacyClass(const std::wstring& name) {
    const auto kind = P::ClassKind(name);
    return kind != P::Kind::None;
}
bool PlacedClass(void* actor) { return LegacyClass(R::ClassNameOf(actor)); }
bool ScanReady() { return true; }
void ScanBegin(void*, bool) {
    if (!Tracking() || !Host()) return;
    World();
    std::erase_if(g_discoveredPlaced, [](const auto& p) { return !p.second.Get(); });
}
void ScanPlaced(void*, void* actor) {
    if (!Tracking() || !Host() || !actor || !R::IsLive(actor)) return;
    const auto world = ue_wrap::world_identity::CurrentWorld();
    if (!world || ue_wrap::world_identity::WorldOf(actor) != world) return;
    World();
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    if (!PlacedClass(actor)) return;
    if (P::Type(actor) == P::Kind::Plant && !ValidKey(P::ExistingKey(actor))) return;
    auto it = g_discoveredPlaced.find(ref.Idx());
    if (it != g_discoveredPlaced.end() && it->second.Is(actor)) return;
    if (g_discoveredPlaced.size() >= kMaxRows || g_enrollPlaced.size() >= kMaxRows) return;
    g_discoveredPlaced[ref.Idx()] = ref; g_enrollPlaced.push_back(ref);
}
size_t ScanComplete(void*, bool, uint32_t) { return g_discoveredPlaced.size(); }
// Preserve native save keys so a joiner's save-loaded twin is reused. Only the
// host may enroll legacy keys; client creation still requires minted keys.
bool EnrollPlaced(const ue_wrap::CachedObjRef& ref) {
    void* actor = ref.Get();
    if (!actor || !PlacedClass(actor)) return true;
    if (!ue_wrap::keyed_objects::Available() || !P::Ready(actor)) return false;
    const auto epoch = g_epoch;
    Applying applying;
    // getData can call native getKey. Do not give unkeyed generated vegetation
    // a new persistent identity just because the census saw it. Saved plants of
    // every measured subclass already have their key restored by loadData.
    if (P::Type(actor) == P::Kind::Plant) {
        const auto key = P::ExistingKey(actor);
        if (epoch != g_epoch || !ref.Get() || !Tracking() || !Host()) return false;
        if (!ValidKey(key)) return false;
    }
    if (P::Type(actor) == P::Kind::DirtHole && !ValidKey(P::ExistingKey(actor))) {
        if (!P::SetKey(actor, Mint()) || !ref.Get() || epoch != g_epoch) return false;
    }
    Record raw, record;
    if (!P::Capture(actor, raw) || !ref.Get() || epoch != g_epoch) return false;
    // A census cannot rename an actor: a join snapshot may already contain its
    // old identity. Wait for native key initialization; express births mint in Finished.
    if (!ValidKey(raw.key)) return false;
    if (!P::Normalize(raw, record)) return false;
    auto it = g_rows.find(record.key);
    if (it != g_rows.end() && (it->second.dead || it->second.ref.Raw())) return true;
    if (it == g_rows.end() && g_rows.size() >= kMaxRows) return false;
    void* existing = ue_wrap::keyed_objects::Resolve(record.key.c_str());
    if (epoch != g_epoch || !ref.Get()) return false;
    if (existing && existing != actor) return false;
    if (!P::SetKey(actor, record.key) || epoch != g_epoch || !ref.Get()) return false;
    // Refetch after native getKey; no iterator may cross a native callback.
    it = g_rows.find(record.key);
    if (it != g_rows.end() && (it->second.dead || it->second.ref.Raw())) return true;
    Row row; row.ref = ref; row.record = std::move(record);
    row.ready = row.accepted = row.dirty = true;
    g_rows.insert_or_assign(row.record.key, row);
    return true;
}
void EnrollPlacedActors() {
    const auto epoch = g_epoch;
    const auto count = (std::min)(size_t(4), g_enrollPlaced.size());
    for (size_t i = 0; i < count; ++i) {
        const auto ref = g_enrollPlaced.front(); g_enrollPlaced.pop_front();
        const bool done = EnrollPlaced(ref);
        if (epoch != g_epoch || !Tracking() || !Host()) return;
        if (!done && ref.Get()) g_enrollPlaced.push_back(ref);
    }
}
bool Garden(P::Kind kind) { return kind == P::Kind::Plant || kind == P::Kind::Faucet || kind == P::Kind::Hose; }
bool SameState(const Record& a, const Record& b) {
    // Normalize leaves only these groups in the garden schemas.
    return a.key == b.key && a.className == b.className && a.xform == b.xform &&
        a.bools == b.bools && a.names == b.names && a.floats == b.floats && a.vectors == b.vectors;
}
void PollGarden(uint64_t now) {
    const auto epoch = g_epoch;
    // A persistent cursor prevents low-sorted plants from monopolizing captures.
    // Four candidates per tick, at most two native captures, including failures.
    for (int visited = 0, captures = 0; visited < 4 && captures < 2 && !g_rows.empty(); ++visited) {
        auto it = g_rows.upper_bound(g_pollCursor);
        if (it == g_rows.end()) it = g_rows.begin();
        const auto key = it->first; g_pollCursor = key;
        const auto& row = it->second;
        const bool polled = Garden(P::ClassKind(row.record.className)) || row.record.className == L"customWall_wetConcrete_C";
        if (!row.accepted || row.dead || row.incoming || row.link || !polled || now < row.pollAt) continue;
        const auto ref = row.ref;
        void* actor = ref.Get();
        if (!actor) continue;
        it->second.pollAt = now + 2000;
        ++captures;
        Applying applying;
        Record raw, record;
        const bool captured = P::Ready(actor) && P::Capture(actor, raw) && P::Normalize(raw, record);
        if (epoch != g_epoch || !Tracking() || !Host()) return;
        if (!captured || !ref.Get() || record.key != key) continue;
        it = g_rows.find(key);
        if (it == g_rows.end() || !it->second.ref.Is(actor) || it->second.dead || it->second.incoming ||
            record.className != it->second.record.className) continue;
        if (!SameState(record, it->second.record)) {
            it->second.record = std::move(record); it->second.dirty = true; it->second.next = 0;
        }
    }
}
void GardenChanged(const SG::Call& c) {
    if (!Tracking() || !Host() || g_applying || c.fromOurCode) return;
    World();
    for (auto& [key, row] : g_rows) if (row.ref.Is(c.object)) { row.pollAt = 0; break; }
}
SG::Verdict BeforePlantSimulation(const SG::Call& c) {
    if (!Tracking() || Host() || P::Type(c.object) != P::Kind::Plant) return SG::Verdict::Run;
    World();
    // loadData calls processGrown, whose Stolas override creates lightning and
    // destroys the plant. Restoring a mirror must not author that outcome.
    if (g_applying) return SG::Verdict::Cancel;
    for (const auto& [key, row] : g_rows) if (row.accepted && row.ref.Is(c.object)) return SG::Verdict::Cancel;
    return SG::Verdict::Run;
}
SG::Verdict BeforeHoleBegin(const SG::Call&) {
    // A hole's native BeginPlay searches for buried treasure and spawns loot.
    // Constructing its network mirror must only construct the surface/material.
    return g_applying ? SG::Verdict::Cancel : SG::Verdict::Run;
}
SG::Verdict BeforeHarvestPocket(const SG::Call& c) {
    // Keep this scoped refusal alive even if native harvest callbacks stop the
    // session or travel. An in-flight puppet must never touch personal stores.
    if (!c.object || g_harvestCollector.Raw() != c.object) return SG::Verdict::Run;
    if (c.function && c.locals) {
        const int off = R::FindParamOffset(c.function, L"return"), size = R::FunctionFrameSize(c.function);
        if (off >= 0 && off < size) {
            c.locals[off] = 0;
            if (auto* out = SG::OutParamPtr(c, off)) *out = 0;
        }
    }
    return SG::Verdict::Cancel;
}
void Finished(void*, void*, void* result) {
    if (!Tracking() || g_applying || !result) return;
    const auto kind = P::Type(result);
    if (kind == P::Kind::None || !P::PlacementCaller(kind, ue_wrap::ufunction_hook::CurrentCallerFrame().function)) return;
    World();
    if (g_rows.size() >= kMaxRows) { UE_LOGW("placed_save: placement table full; new placement not shared"); return; }
    const auto key = Mint(); const auto epoch = g_epoch;
    ue_wrap::CachedObjRef ref; ref.Set(result);
    Applying applying;
    if (!P::SetKey(result, key) || !ref.Get() || epoch != g_epoch) return;
    Row row; row.ref = ref; row.local = true; row.record.key = key;
    row.settled = kind != P::Kind::Hose;
    g_rows.emplace(key, std::move(row));
}
#include "placed_save_hose.inc"
SG::Verdict BeforeAction(const SG::Call& c) {
    if (!Active() || g_applying || c.fromOurCode) return SG::Verdict::Run;
    World();
    for (auto& [key, row] : g_rows) if (row.ref.Is(c.object)) {
        if (Host()) {
            if (row.link && P::ClassKind(row.record.className) == P::Kind::Hose && c.locals && c.function) {
                const int off = R::FindParamOffset(c.function, L"action"), size = R::FunctionFrameSize(c.function);
                if (off >= 0 && off < size && c.locals[off] == 4) return SG::Verdict::Cancel;
            }
            return SG::Verdict::Run;
        }
        // Refuse before native action 4 creates the temporary held hose. A
        // refusal only inside resume would leave that temporary item behind.
        if (row.accepted && P::ClassKind(row.record.className) == P::Kind::Hose && c.locals && c.function) {
            const int off = R::FindParamOffset(c.function, L"action"), size = R::FunctionFrameSize(c.function);
            if (off >= 0 && off < size && c.locals[off] == 4) {
                const auto edit = g_hoseEdits.find(key);
                if (row.incoming || row.link || g_hoseEdits.size() >= 32 ||
                    (edit != g_hoseEdits.end() && edit->second.finished) ||
                    !SG::ClassNameWatchLive(L"groundHose_C", L"resume", kTag) ||
                    !SG::ClassNameWatchLive(L"groundHose_C", L"finish", kTag)) return SG::Verdict::Cancel;
            }
        }
        if (row.accepted && P::CanHarvest(c.object) && c.locals && c.function) {
            const int off = R::FindParamOffset(c.function, L"action"), size = R::FunctionFrameSize(c.function);
            if (off >= 0 && off < size && c.locals[off] == 4) {
                for (const auto& use : g_uses) if (use.ref.Is(c.object)) return SG::Verdict::Cancel;
                if (g_uses.size() + g_careFrames.size() < 32) {
                    Use use{key, row.ref, ++g_useSeq, Now() + 10000};
                    if (!use.seq) use.seq = ++g_useSeq;
                    use.slot=players::Registry::Get().LocalPeerId();if(use.slot>=players::kMaxPeers)return SG::Verdict::Cancel;
                    use.person=roster_ledger::Get(use.slot).playerNo;use.peerEpoch=g_hosePeerEpoch[use.slot];if(!use.person)return SG::Verdict::Cancel;
                    g_uses.push_back(std::move(use));
                }
                return SG::Verdict::Cancel;
            }
        }
        row.manualUntil = Now() + 2000; break;
    }
    return SG::Verdict::Run;
}
void FlushUses(uint64_t now) {
    const auto epoch = g_epoch;
    for (int work = 0; !g_uses.empty() && work < 4; ++work) {
        const Use use = g_uses.front();
        // A completed owner-paid care use still belongs to this world if its
        // local crop copy vanished before transport; the host validates the key.
        const auto current=[&]{return epoch==g_epoch && g_world==ue_wrap::world_identity::Generation() && Tracking() && !Host() &&
            use.slot<players::kMaxPeers && use.slot==players::Registry::Get().LocalPeerId() && use.person &&
            use.person==roster_ledger::Get(use.slot).playerNo && use.peerEpoch==g_hosePeerEpoch[use.slot];};
        if (!current() || (use.op==Harvest && (!use.ref.Get() || now >= use.expires))) { g_uses.pop_front(); continue; }
        Record record; record.key = use.key;
        const bool sent = Send(use.op, record, use.seq, &use.care,current,use.person);
        if (epoch != g_epoch || !Tracking() || Host()) return;
        if (!sent) break;
        if(!g_uses.empty() && g_uses.front().seq==use.seq)g_uses.pop_front();
    }
}
SG::Verdict BeforeCare(const SG::Call& c) {
    if (!Active()||Host()||c.fromOurCode||g_applying||!c.function||!c.locals||c.object!=hand_item::LocalHandActor()) return SG::Verdict::Run;
    World();
    const int off=R::FindParamOffset(c.function,L"player"),size=R::FunctionFrameSize(c.function);
    if(off<0||off>size||size-off<int(sizeof(void*)))return SG::Verdict::Run;
    void* player=nullptr;std::memcpy(&player,c.locals+off,sizeof(player));
    if(player!=players::Registry::Get().Local())return SG::Verdict::Run;
    const auto kind=R::ClassNameOf(c.object)==L"prop_fertilizer_C"?P::CareKind::Fertilizer:P::CareKind::Water;
    const auto epoch=g_epoch;const auto world=g_world;const auto slot=players::Registry::Get().LocalPeerId();
    if(slot>=players::kMaxPeers)return SG::Verdict::Run;
    const auto person=roster_ledger::Get(slot).playerNo;const auto peerEpoch=g_hosePeerEpoch[slot];ue_wrap::CachedObjRef tool;tool.Set(c.object);
    const auto current=[&]{return epoch==g_epoch && world==ue_wrap::world_identity::Generation() && Tracking() && !Host() && tool.Get() &&
        c.object==hand_item::LocalHandActor() && slot==players::Registry::Get().LocalPeerId() && person && roster_ledger::Get(slot).playerNo==person && g_hosePeerEpoch[slot]==peerEpoch;};
    P::CareUse care;
    if(!current())return SG::Verdict::Cancel;
    const bool read=P::ReadCareUse(c.object,player,kind,care);if(!current())return SG::Verdict::Cancel;
    if(!read)return SG::Verdict::Run;
    std::erase_if(g_careFrames,[](const CareFrame& frame){return !SG::IsInvocationActive(frame.invocation);});
    for(const auto& [key,row]:g_rows)if(row.accepted&&!row.dead&&row.ref.Is(care.plant)) {
        // Reserve storage before native gameplay spends anything. Refuse a
        // tracked crop use only if there is nowhere to retain its result.
        if(!c.invocation || g_uses.size()+g_careFrames.size()>=32)return SG::Verdict::Cancel;
        CareFrame frame;frame.tool=tool;frame.invocation=c.invocation;frame.plant=row.ref;frame.care=std::move(care);frame.key=key;frame.epoch=epoch;
        frame.world=world;frame.person=person;frame.slot=slot;frame.peerEpoch=peerEpoch;
        g_careFrames.push_back(std::move(frame));break;
    }
    return SG::Verdict::Run;
}
void AfterCare(const SG::Call& c) {
    if(!c.invocation)return;
    const auto found=std::find_if(g_careFrames.rbegin(),g_careFrames.rend(),[&](const CareFrame& frame){return frame.invocation==c.invocation && frame.tool.Raw()==c.object;});
    if(found==g_careFrames.rend())return;const auto frame=*found;g_careFrames.erase(std::next(found).base());
    const auto current=[&]{return frame.epoch==g_epoch && frame.world==ue_wrap::world_identity::Generation() && Tracking() && !Host() &&
        frame.slot==players::Registry::Get().LocalPeerId() && roster_ledger::Get(frame.slot).playerNo==frame.person && g_hosePeerEpoch[frame.slot]==frame.peerEpoch;};
    if(!current())return;
    const bool spent=frame.care.kind==P::CareKind::Fertilizer?!frame.tool.Get():
        frame.tool.Get()&&P::CareWasSpent(c.object,frame.care.kind);
    if(!spent || !current() || (frame.care.kind==P::CareKind::Water && !frame.tool.Get()))return;
    Use use{frame.key,frame.plant,++g_useSeq,0,Care,frame.care};
    if(!use.seq)use.seq=++g_useSeq;
    use.person=frame.person;use.slot=frame.slot;use.peerEpoch=frame.peerEpoch;
    g_uses.push_back(std::move(use));
}
// Save-loaded placements keep their persistent identity, including specialized
// plants selected by list_farmPlants.plantObject in either placement factory.
void Loaded(const SG::Call& c) {
    if (!Tracking() || g_applying || c.fromOurCode || P::Type(c.object) == P::Kind::None) return;
    if (Host() && PlacedClass(c.object)) { ScanPlaced(nullptr, c.object); return; }
    World();
    Record record; const auto epoch = g_epoch;
    ue_wrap::CachedObjRef ref; ref.Set(c.object);
    Applying applying;
    if (!P::Capture(c.object, record) || !ref.Get() || epoch != g_epoch || !Ours(record.key)) return;
    if (auto it = g_rows.find(record.key); it != g_rows.end()) {
        if (it->second.dead) { it->second.ref = ref; it->second.next = 0; }
        return;
    }
    if (g_rows.size() >= kMaxRows) return;
    Row row; row.ref = ref; row.record = record; row.accepted = true;
    row.ready = true; g_rows.emplace(record.key, std::move(row));
}
bool Reach(uint8_t slot, const Record& r) {
    const auto target = element::IntentTarget::ForClientIntent(*g_session, slot, 1000.f);
    ue_wrap::FVector p{r.xform[4], r.xform[5], r.xform[6]};
    // A hose is committed from its completed far end, not from the faucet where
    // its actor root still stands. Both are valid positions for its placer.
    if (P::ClassKind(r.className) == P::Kind::Hose && !r.vectors.empty() && !r.vectors[0].empty()) {
        const auto& end = r.vectors[0].back(); p = {end[0], end[1], end[2]};
    }
    return bool(target.AuthorizeSegment(p, p));
}
void Dead(Row& row) {
    row.dead = true; row.remove = false; row.incoming = false; row.link = false;
    row.accepted = true; row.dirty = Host(); row.next = 0;
}

}

void Install(net::Session* session) {
    if (g_session != session) { g_session = session; ResetWorld(); }
    wall_build_sync::Install(session);
    if (!g_scanRegistered) {
        element::scan_hub::Register({"placed_save", nullptr, &ScanReady, &PlacedClass,
            &ScanBegin, &ScanPlaced, &ScanComplete, 0});
        g_scanRegistered = true;
    }
    if (!g_watched) {
        g_watched = SG::WatchClassName(L"faucet_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"groundHose_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"growingPlant_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"customWall_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"customWall_unfinished_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"groundHose_C", L"finish", kTag, nullptr, &HoseFinished) &&
            SG::WatchClassName(L"groundHose_C", L"resume", kTag, &BeforeHoseResume, &HoseResumed) &&
            SG::WatchClassName(L"growingPlant_C", L"ReceiveTick", kTag, &BeforePlantSimulation, &GardenChanged) &&
            SG::WatchClassName(L"growingPlant_C", L"turnDead", kTag, nullptr, &GardenChanged) &&
            SG::WatchClassName(L"growingPlant_C", L"addFruit", kTag, &BeforePlantSimulation, &GardenChanged) &&
            SG::WatchClassName(L"growingPlant_stolasHerb_C", L"grown", kTag, &BeforePlantSimulation, nullptr) &&
            SG::WatchClassName(L"faucet_C", L"upd", kTag, nullptr, &GardenChanged) &&
            SG::WatchClassName(L"mainPlayer_C", L"putObjectInventory2", kTag, &BeforeHarvestPocket, nullptr) &&
            SG::WatchClassName(L"prop_bucket_C", L"playerHandUse_LMB", kTag, &BeforeCare, &AfterCare) &&
            SG::WatchClassName(L"prop_fertilizer_C", L"playerHandUse_LMB", kTag, &BeforeCare, &AfterCare) &&
            SG::WatchClassName(L"faucet_C", L"actionOptionIndex", kTag, &BeforeAction, nullptr) &&
            SG::WatchClassName(L"groundHose_C", L"actionOptionIndex", kTag, &BeforeAction, nullptr) &&
            SG::WatchClassName(L"growingPlant_C", L"actionOptionIndex", kTag, &BeforeAction, nullptr) &&
            SG::WatchClassName(L"dirthole_C", L"ReceiveBeginPlay", kTag, &BeforeHoleBegin, nullptr) &&
            SG::WatchClassName(L"dirthole_C", L"playerUsedOn", kTag, &BeforeAction, nullptr);
    }
    if (!g_finish.Get()) {
        const auto fn = ue_wrap::facts::Resolve(kFinishSpawningRow);
        if (!fn) return;
        void* rawFn = fn.Fn();
        const int off = R::FindParamOffset(rawFn, L"ReturnValue"), size = R::FunctionFrameSize(rawFn);
        if (off < 0 || size < off || size - off < int(sizeof(void*))) return;
        if (ue_wrap::ufunction_hook::InstallPostHook(fn, &Finished)) g_finish.Set(rawFn);
    }
}
void Receive(const net::BlobChunkPayload& chunk, uint8_t slot) {
    if (!Active() || slot >= players::kMaxPeers || (Host() ? slot == 0 : slot != 0)) return;
    World();
    std::vector<uint8_t> bytes;
    if (chunk.chunks > 300 || !g_assembler.OnChunk(chunk, slot, bytes) || bytes.size() > 64 * 1024) return;
    if (garden_actions::Receive(bytes, slot)) return;
    size_t offset = 0; uint8_t op = 0; uint32_t useSeq = 0,usePerson=0; Record record;P::CareUse care;
    if (!W::RdU8(bytes, offset, op) || op > HoseReply ||
        (Host() ? (op != Create && op != Remove && op != Refresh && op != Harvest && op != Care && op != HoseRoute) :
            (op != State && op != Retired && op != HoseReply))) return;
    if (op == HoseReply) { AcceptHoseAnswer(bytes, offset, chunk.blobSeq); return; }
    if (op == Remove || op == Retired || op == Refresh || op == Harvest || op == Care) {
        if (!W::RdWStr(bytes, offset, record.key)) return;
    } else {
        Record input;
        if (!W::DeSave(bytes, offset, input) || !P::Normalize(input, record)) return;
    }
    if ((op == Harvest || op == Care || op == HoseRoute) && (!W::RdU32(bytes, offset, useSeq) || !useSeq)) return;
    if((op==Harvest || op==Care) && (!W::RdU32(bytes,offset,usePerson) || !usePerson || usePerson>65535))return;
    uint32_t baselineLo = 0, baselineHi = 0;
    if (op == HoseRoute && (!W::RdU32(bytes, offset, baselineLo) || !W::RdU32(bytes, offset, baselineHi))) return;
    if(op==Care) {
        uint8_t kind=0;
        if(!W::RdU8(bytes,offset,kind)||kind>uint8_t(P::CareKind::Fertilizer)||!W::RdWStr(bytes,offset,care.sourceKey)||
           !ValidKey(care.sourceKey)||!W::RdF32(bytes,offset,care.amount)||!std::isfinite(care.amount)||care.amount<=0||care.amount>600)return;
        care.kind=P::CareKind(kind);
        if(care.kind==P::CareKind::Fertilizer&&care.amount!=600.f)return;
    }
    if (offset != bytes.size() || !ValidKey(record.key)) return;
    // Legacy identity is host-authored and schema-limited. Unknown tombstones
    // carry no schema, so their lookup below must enforce that same class gate.
    auto it = g_rows.find(record.key);
    // The console marker is host world state. A client may ask for a known
    // snapshot, but no personal placement/removal/resource verb may mutate it.
    if (Host() && ((P::ClassKind(record.className)==P::Kind::Marker && op!=Refresh) ||
        (it!=g_rows.end() && P::ClassKind(it->second.record.className)==P::Kind::Marker && op!=Refresh))) return;
    if (!Ours(record.key)) {
        if (Host()) {
            const bool knownGardenRemoval = op == Remove && it != g_rows.end() && it->second.accepted &&
                Garden(P::ClassKind(it->second.record.className));
            if (op != Refresh && op != Harvest && op != Care && op != HoseRoute && !knownGardenRemoval) return;
        } else if (op == State && !LegacyClass(record.className)) return;
    }
    if (Host()) {
        if (op == HoseRoute) { ReceiveHoseRoute(record, useSeq, uint64_t(baselineLo) | (uint64_t(baselineHi) << 32), slot); return; }
        const auto useEpoch=g_epoch;const auto useWorld=g_world;const auto useConnection=g_hosePeerEpoch[slot];auto* useSession=g_session;
        const auto useCurrent=[&]{return useEpoch==g_epoch && useWorld==ue_wrap::world_identity::Generation() && g_session==useSession &&
            Tracking() && Host() && useSession->IsSlotWorldReady(slot) && g_hosePeerEpoch[slot]==useConnection && roster_ledger::Get(slot).playerNo==usePerson;};
        if((op==Care || op==Harvest) && !useCurrent())return;
        if(op==Care) {
            if(it==g_rows.end()||!it->second.accepted||it->second.dead)return;
            const auto ref=it->second.ref;void* actor=ref.Get();const auto epoch=g_epoch;
            const auto playerNo=usePerson;const auto prior=g_used[slot];
            const auto stock=std::make_pair(uint32_t(playerNo),care.sourceKey);
            if(!actor||P::Type(actor)!=P::Kind::Plant||!playerNo||
               (prior.playerNo==playerNo&&int32_t(useSeq-prior.seq)<=0)||
               !hand_item::LastHeldClassIs(slot,care.kind==P::CareKind::Water?L"prop_bucket_C":L"prop_fertilizer_C")||
               (care.kind==P::CareKind::Fertilizer&&(g_spentFertilizers.count(stock)||g_spentFertilizers.size()>=4096))||
               !element::IntentTarget::ForClientIntent(*g_session,slot,450.f).Authorize(actor))return;
            if(epoch!=g_epoch||!useCurrent()||!ref.Get())return;
            if(!P::ApplyCare(actor,care.kind,care.amount))return;
            if(!useCurrent() || !ref.Get())return;
            g_used[slot]={useSeq,playerNo,Now()};
            if(care.kind==P::CareKind::Fertilizer)g_spentFertilizers.insert(stock);
            it=g_rows.find(record.key);if(it!=g_rows.end()){it->second.pollAt=it->second.next=0;it->second.dirty=true;}
            return;
        }
        if (op == Harvest) {
            if (it == g_rows.end() || !it->second.accepted || it->second.dead) return;
            const auto ref = it->second.ref; const auto epoch = g_epoch;
            void* actor = ref.Get();
            const auto playerNo = usePerson;
            const auto prior = g_used[slot]; const auto now = Now();
            auto* remote = players::Registry::Get().Puppet(slot);
            void* collector = remote && remote->valid() ? remote->GetActor() : nullptr;
            ue_wrap::CachedObjRef collectorRef; if (collector) collectorRef.Set(collector);
            if (!actor || !playerNo || !P::CanHarvest(actor) ||
                !collectorRef.Get() || !SG::ClassNameWatchLive(L"mainPlayer_C", L"putObjectInventory2", kTag) ||
                (prior.playerNo == playerNo && (int32_t(useSeq - prior.seq) <= 0 || now < prior.at + 250)) ||
                !element::IntentTarget::ForClientIntent(*g_session, slot, 450.f).Authorize(actor)) return;
            if (epoch != g_epoch || !useCurrent() || !ref.Get() || !collectorRef.Get()) return;
            g_used[slot] = {useSeq, playerNo, now};
            Applying applying;
            HarvestCollector collecting(collector);
            P::Harvest(actor, collector);
            if (epoch != g_epoch || !useCurrent()) return;
            if (g_world != ue_wrap::world_identity::Generation()) { World(); return; }
            it = g_rows.find(record.key);
            if (it != g_rows.end()) {
                void* current = it->second.ref.Get();
                if (ref.Is(current)) {
                    it->second.pollAt = it->second.next = 0; it->second.dirty = true;
                } else if (!current && it->second.ref.Raw() == ref.Raw()) Dead(it->second);
            }
            return;
        }
        // A missing legacy mirror requests the canonical snapshot by key. It
        // cannot create, overwrite or enroll anything on the host.
        if (op == Refresh) {
            if (it != g_rows.end() && it->second.accepted) { it->second.dirty = true; it->second.next = 0; }
            return;
        }
        if (it != g_rows.end()) {
            // Repeated creation never reapplies stale planting/fuel/geometry state.
            if (op == Create) { if (it->second.accepted) it->second.dirty = true; return; }
            const Row row = it->second;
            if (row.dead) { it->second.dirty = true; return; }
            void* actor = row.ref.Get();
            if (actor && !element::IntentTarget::ForClientIntent(*g_session, slot, 1000.f).Authorize(actor) &&
                !Reach(slot, row.record)) return;
            if (!actor && row.owner != slot) return;
            const auto epoch = g_epoch;
            Applying applying;
            if (actor) ue_wrap::engine::DestroyActor(actor);
            if (epoch != g_epoch) return;
            if (row.ref.Get()) return; // failed native destroy is not a retirement
            it = g_rows.find(record.key);
            if (it != g_rows.end()) Dead(it->second);
            return;
        }
        if (g_rows.size() >= kMaxRows || !roster_ledger::Get(slot).playerNo) return;
        // A cancel may complete before a partially sent creation blob. Retain
        // its tombstone so the delayed creation cannot resurrect the item.
        Row row; row.record = record; row.owner = slot;
        row.playerNo = roster_ledger::Get(slot).playerNo;
        if (op == Remove) {
            const auto epoch = g_epoch;
            if (!ue_wrap::keyed_objects::Available()) return;
            void* existing = ue_wrap::keyed_objects::Resolve(record.key.c_str());
            if (epoch != g_epoch || existing) return; // not an unknown, cancelled creation
            Dead(row);
        }
        else {
            if (!Reach(slot, record)) return;
            row.ready = true; row.incoming = true;
        }
        g_rows.emplace(record.key, std::move(row));
    } else {
        if (it == g_rows.end()) {
            if (g_rows.size() >= kMaxRows) return;
            Row row; row.record = record; row.ready = true;
            row.incoming = op == State; row.dead = op == Retired; row.accepted = row.dead;
            row.retireLookup = row.dead;
            row.hoseStateSeen = op == State && P::ClassKind(record.className) == P::Kind::Hose;
            row.hoseStateSeq = chunk.blobSeq;
            g_rows.emplace(record.key, std::move(row)); return;
        }
        if (op == Retired) {
            g_hoseEdits.erase(record.key);
            const auto epoch = g_epoch; const auto ref = it->second.ref;
            Dead(it->second);
            Applying applying;
            if (void* a = ref.Get()) ue_wrap::engine::DestroyActor(a);
            if (epoch != g_epoch) return;
        } else if (!it->second.dead) {
            if (P::ClassKind(record.className) == P::Kind::Hose) {
                if (it->second.hoseStateSeen && int32_t(chunk.blobSeq - it->second.hoseStateSeq) <= 0) return;
                it->second.hoseStateSeq = chunk.blobSeq; it->second.hoseStateSeen = true;
            }
            // The creator already owns the correctly initialized native actor.
            // Acknowledgement must neither respawn it nor overwrite a newer local use.
            const bool wasAccepted = it->second.accepted;
            it->second.accepted = true;
            if (it->second.ref.Raw() && !it->second.ref.Get()) {
                if (it->second.manualUntil >= Now() || !wasAccepted) it->second.remove = true;
                else { it->second.ref.Reset(); it->second.local = false; }
            }
            // Preserve the initial garden acknowledgement, then the host drives
            // growth/watering/ripeness even on the original placing peer.
            if ((!it->second.local || wasAccepted || P::IsWall(P::ClassKind(record.className))) && !it->second.remove) {
                it->second.record = record; it->second.incoming = true;
                it->second.next = 0;
            }
        }
    }
}
void Tick() {
    if (g_busy || !ue_wrap::game_thread::IsGameThread()) return;
    g_busy = true;
    struct Busy { ~Busy() { g_busy = false; } } busy;
    // Drain native construction queued just before the last guest disconnected.
    if (!Tracking()) { wall_build_sync::Tick(); return; }
    World();
    const auto now = Now();
    if (now < g_tickAt) return;
    g_tickAt = now + 100;
    const auto epoch = g_epoch;
    wall_build_sync::Tick();
    if (epoch != g_epoch) return;
    if (Host()) EnrollPlacedActors();
    if (epoch != g_epoch) return;
    if (Host()) PollGarden(now);
    else { FlushUses(now); if (epoch != g_epoch) return; FlushHoseEdits(now); }
    if (epoch != g_epoch) return;
    // Another script-gate consumer can cancel after our pre watch; that call
    // has no post callback. Reclaim its reservation once the body is inactive.
    std::erase_if(g_careFrames, [](const CareFrame& frame) {
        return !SG::IsInvocationActive(frame.invocation);
    });
    if (now >= g_sweep) {
        g_sweep = now + 1000;
        g_assembler.Sweep(std::chrono::steady_clock::now(), std::chrono::seconds(20));
    }
    std::vector<std::wstring> keys;
    for (const auto& [key, row] : g_rows) keys.push_back(key);
    int work = 0;
    for (const auto& key : keys) {
        if (epoch != g_epoch) return;
        auto it = g_rows.find(key); if (it == g_rows.end()) continue;
        if (now < it->second.next) continue;
        // Resting accepted rows need only a weak-lifetime check. Do not copy a
        // potentially 4096-point hose record on every frame/poll.
        if (it->second.accepted && !it->second.dead && !it->second.incoming &&
            !it->second.remove && !it->second.dirty && !it->second.link && it->second.ref.Get()) {
            it->second.next = now + 500; continue;
        }
        // An incoming canonical record can contain thousands of spline points.
        // Retain it in place while editing instead of copying it every tick.
        if (!Host() && !it->second.dead && g_hoseEdits.count(key) && it->second.ref.Get()) continue;
        Row row = it->second;
        void* a = row.ref.Get();
        if (row.dead && (a || row.retireLookup)) {
            if (++work > 4) break;
            it->second.next = now + 500;
            Applying applying;
            if (!a && row.retireLookup) {
                if (!ue_wrap::keyed_objects::Available()) continue;
                a = ue_wrap::keyed_objects::Resolve(key.c_str());
                if (epoch != g_epoch) return;
                if (a && (P::Type(a) == P::Kind::None || (!Ours(key) && !PlacedClass(a)))) continue;
                it = g_rows.find(key); if (it == g_rows.end()) continue;
                it->second.ref.Set(a); it->second.retireLookup = false;
            }
            if (a) ue_wrap::engine::DestroyActor(a);
            continue; // failed destruction is retried; never silently abandon a live twin
        }
        if (a && row.local && !row.ready && row.settled) {
            if (++work > 4) break;
            it->second.next = now + 250;
            Applying applying;
            Record raw, normalized;
            if (!P::Ready(a) || !P::Capture(a, raw) || !P::Normalize(raw, normalized) ||
                normalized.key != key || !row.ref.Get()) continue;
            if (epoch != g_epoch) return;
            it = g_rows.find(key); if (it == g_rows.end()) continue;
            it->second.record = normalized; it->second.ready = true;
            it->second.accepted = Host(); it->second.dirty = Host();
            it->second.next = 0; continue;
        }
        if (row.incoming) {
            if (++work > 4) break;
            it->second.next = now + 500;
            if (Host() && roster_ledger::Get(row.owner).playerNo != row.playerNo) { g_rows.erase(it); continue; }
            if (!ue_wrap::keyed_objects::Available()) continue;
            Applying applying;
            void* existing = ue_wrap::keyed_objects::Resolve(key.c_str());
            if (epoch != g_epoch) return;
            if (existing) {
                // A joiner may already have loaded this exact actor from the save.
                // Never bind another class under an incoming identity.
                if (R::ClassNameOf(existing) != row.record.className) continue;
                if (Host()) {
                    Record raw, normalized;
                    if (!P::Capture(existing, raw) || !P::Normalize(raw, normalized)) continue;
                    // An existing host actor wins a colliding creation identity.
                    row.record = std::move(normalized);
                } else if (!P::Apply(existing, row.record)) continue;
                a = existing;
            } else {
                // Registration alone does not mean the name watch is live yet.
                // Never enter native hole BeginPlay before its loot suppression
                // can run; the retained row retries when resolution completes.
                if (P::ClassKind(row.record.className) == P::Kind::DirtHole &&
                    !SG::ClassNameWatchLive(L"dirthole_C", L"ReceiveBeginPlay", kTag)) continue;
                a = P::Spawn(row.record);row.spawnedForClient=Host() && a;
            }
            if (epoch != g_epoch) return;
            if (!a) continue;
            it = g_rows.find(key); if (it == g_rows.end()) continue;
            it->second.record = row.record;
            it->second.ref.Set(a); it->second.incoming = false; it->second.accepted = true;
            it->second.spawnedForClient=row.spawnedForClient;
            it->second.link = true; it->second.dirty = Host(); it->second.next = 0;
            continue;
        }
        if (!a && row.ref.Raw() && !row.dead) {
            if (!row.ready) { g_rows.erase(it); continue; }
            if (Host()) Dead(it->second);
            else if (!row.accepted || row.manualUntil >= now) it->second.remove = true;
            else {
                // Local lifespan/weather simulation is not permission to destroy
                // the host's plant. Ask for its current canonical record instead.
                it->second.ref.Reset(); it->second.local = false; it->second.accepted = false;
            }
            row = it->second;
        }
        if (a && row.link && now >= row.linkAt) {
            if (++work > 4) break;
            it->second.linkAt = now + 1000;
            Applying applying;
            const bool linked = P::Link(a, row.record);
            if (epoch != g_epoch) return;
            it = g_rows.find(key); if (it == g_rows.end()) continue;
            if (linked && row.ref.Get()) it->second.link = false;
            row = it->second;
        }
        if (Active() && ((Host() && row.dirty && !(row.link && P::ClassKind(row.record.className) == P::Kind::Hose)) ||
            (!Host() && (row.remove || (row.ready && !row.accepted))))) {
            if (++work > 4) break;
            it = g_rows.find(key); if (it == g_rows.end()) continue;
            it->second.next = now + kRetryMs;
            const Op op = Host() ? (row.dead ? Retired : State) :
                (row.remove ? Remove : Ours(key) ? Create : Refresh);
            if (Host() && !row.dead && a) {
                Applying applying;
                Record raw, normalized;
                if (!P::Ready(a) || !P::Capture(a, raw) || !P::Normalize(raw, normalized) ||
                    normalized.key != key || !row.ref.Get()) continue;
                if (epoch != g_epoch) return;
                row.record = std::move(normalized);
                it = g_rows.find(key); if (it == g_rows.end()) continue;
                it->second.record = row.record;
            }
            const bool sent = Send(op, row.record);
            if (epoch != g_epoch) return;
            it = g_rows.find(key);
            if (Host() && sent && it != g_rows.end()) it->second.dirty = false;
        }
    }
    if (epoch == g_epoch && Active() && Host()) FlushHoseAnswers();
}
void QueueConnectBroadcastForSlot(int) {
    if (!Active() || !Host()) return;
    World();
    // Queued, bounded by Tick's work budget and retried on backpressure. Includes
    // tombstones for actors present in the joiner's older save capture.
    for (auto& [key, row] : g_rows) if (row.accepted) { row.dirty = true; row.next = 0; }
}
void OnPeerLeftSlot(int slot) {
    if (slot < 0 || slot >= players::kMaxPeers) return;
    g_assembler.ClearSlot(uint8_t(slot));
    g_used[slot] = {};
    g_hoseSeen[slot] = {}; g_hoseAnswers[slot].clear();
    ++g_hosePeerEpoch[slot];
    // Accepted objects belong to the host save, not to the departing connection.
    std::erase_if(g_rows, [slot](const auto& pair) { return pair.second.incoming &&
        Host() && pair.second.owner == slot && !pair.second.accepted; });
}
void MarkChanged(void* actor) {
    if (!Tracking() || !Host() || g_applying || !actor) return;
    World();
    for (auto& [key, row] : g_rows) if (row.ref.Is(actor)) {
        row.dirty = true; row.next = 0; break;
    }
}
void TrackWall(void* actor) {
    if (!Tracking() || !Host() || g_applying || !actor) return;
    World();
    for (const auto& [key, row] : g_rows) if (row.ref.Is(actor)) return;
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    EnrollPlaced(ref);
}
bool OwnsClientWall(const std::wstring& key,void* actor,uint8_t slot,uint16_t playerNo) {
    if(!Tracking() || !Host() || !playerNo)return false;
    const auto it=g_rows.find(key);if(it==g_rows.end())return false;
    const auto& row=it->second;return row.accepted && !row.dead && row.spawnedForClient && row.ref.Is(actor) &&
        row.owner==slot && row.playerNo==playerNo && row.record.className==L"customWall_unfinished_C";
}
void OnDisconnect() { wall_build_sync::OnDisconnect(); g_session = nullptr; ResetWorld(); ue_wrap::llama_marker::OnDisconnect(); }
bool SendGardenAction(const std::vector<uint8_t>& bytes,const std::function<bool()>& current) {
    return Active() && !Host() && bytes.size() <= 64 * 1024 &&
        blob_chunks::SendBlobWhile(g_session, net::ReliableKind::PlacedSave, ++g_blobSeq, bytes,current);
}
}
