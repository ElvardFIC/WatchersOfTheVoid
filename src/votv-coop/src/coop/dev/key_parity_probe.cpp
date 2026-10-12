// coop/dev/key_parity_probe.cpp -- [dev] a key index fed only at the key writes, against the world. See the
// header.
#include "coop/dev/key_parity_probe.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/element/registry.h"  // EidForActor, naming a persistent miss
#include "coop/player/hand_item.h"  // CollectHandAxisActors: the index never holds the hand axis
#include "coop/player/players_registry.h"  // kMaxPeers
#include "coop/props/prop_element_tracker.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"  // IsChildActor
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/facts/function_row.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::dev::key_parity_probe {
namespace {

namespace R  = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
namespace WI = ue_wrap::world_identity;
namespace PT = coop::prop_element_tracker;
using Clock = std::chrono::steady_clock;

// The four bodies after which a prop's key can have changed: init writes it from getKey (a None key
// becomes a random one), loadData writes the saved key and runs init, the setKey event writes it alone, and
// a getKey follows the one raw cross-object write the game makes (ATV.createContainer).
UE_FACT_FN(kRowInit,     "dev.key_parity_probe", nullptr, L"init",     AnyClass);
UE_FACT_FN(kRowSetKey,   "dev.key_parity_probe", nullptr, L"setKey",   AnyClass);
UE_FACT_FN(kRowGetKey,   "dev.key_parity_probe", nullptr, L"getKey",   AnyClass);
UE_FACT_FN(kRowLoadData, "dev.key_parity_probe", nullptr, L"loadData", AnyClass);
enum Body : int { kInit = 0, kSetKey, kGetKey, kLoadData, kBodies };
constexpr int kTagBase = 0x4B505200;  // 'KPR' + body
const wchar_t* const kBodyName[kBodies] = {L"init", L"setKey", L"getKey", L"loadData"};

// The keyed domain: prop_C's lineage (its own `key`) and trashBitsPile (actor_save_C's `key`). The chip
// pile and the garbage clump have no key field, and their getKey is a stub returning None.
bool InDomain(void* cls) {
    if (!cls) return false;
    if (ue_wrap::prop::IsClassDescendantOfProp(cls)) return true;
    void* bits = ue_wrap::prop::TrashBitsPileClass();
    return bits && ue_wrap::prop::WalksToBase(cls, bits);
}

bool SameName(const R::FName& a, const R::FName& b) {
    return a.ComparisonIndex == b.ComparisonIndex && a.Number == b.Number;
}

// The shadow index, by slot: the tenant it saw (pointer and serial) and the key it read after the body.
struct Shadow {
    void*   obj    = nullptr;
    int32_t serial = 0;
    R::FName key{0, 0};
};
std::unordered_map<int32_t, Shadow> g_shadow;
std::uint64_t g_posts[kBodies] = {};
std::uint64_t g_changed[kBodies] = {};

void Post(const SG::Call& call) {
    void* obj = call.object;
    if (!obj) return;
    if (!InDomain(R::ClassOf(obj))) return;
    const int body = call.tag - kTagBase;
    if (body < 0 || body >= kBodies) return;
    ++g_posts[body];
    const int32_t idx = R::InternalIndexOf(obj);
    if (idx < 0) return;
    const R::FName key = ue_wrap::prop::GetInteractableKey(obj);  // a field read on this domain
    const int32_t serial = R::AllocateSlotSerial(idx);
    Shadow& s = g_shadow[idx];
    if (s.obj != obj || s.serial != serial || !SameName(s.key, key)) ++g_changed[body];
    s.obj = obj;
    s.serial = serial;
    s.key = key;
}

// One live keyed prop, as the walk finds it.
struct Truth {
    void*    obj;
    int32_t  idx;
    R::FName key;
};

struct Counts {
    size_t truth = 0, dupKeys = 0;
    size_t liveMiss = 0, liveStale = 0;
    size_t shadowMiss = 0, shadowStale = 0;
    bool operator==(const Counts& o) const {
        return truth == o.truth && dupKeys == o.dupKeys && liveMiss == o.liveMiss && liveStale == o.liveStale &&
               shadowMiss == o.shadowMiss && shadowStale == o.shadowStale;
    }
};

// Examples logged per world, so a miss is named once and not every sample.
int g_liveExamples = 0, g_shadowExamples = 0;
constexpr int kExamples = 12;
std::map<std::wstring, size_t> g_shadowMissByClass;  // per sample, logged with the examples
// A miss that outlives a load: consecutive samples missing, by slot and tenant, for each index; named
// once at the third sample (about 6 s), up to kExamples per world.
struct Run { void* obj = nullptr; int samples = 0; bool named = false; };
std::unordered_map<int32_t, Run> g_liveRun, g_shadowRun;
int g_livePersistNamed = 0, g_shadowPersistNamed = 0;
constexpr int kPersistSamples = 3;

void NoteMiss(std::unordered_map<int32_t, Run>& runs, std::unordered_map<int32_t, Run>& next, int& named,
              const char* which, void* obj, int32_t idx, const std::wstring& keyStr) {
    Run r{};
    const auto it = runs.find(idx);
    if (it != runs.end() && it->second.obj == obj) r = it->second;
    r.obj = obj;
    ++r.samples;
    if (r.samples >= kPersistSamples && !r.named && named < kExamples) {
        r.named = true;
        ++named;
        const auto anyEid = coop::element::Registry::Get().EidForActor(obj);
        UE_LOGI("key_parity: %s PERSISTENT MISS (%d samples) %ls key='%ls' slot=%d eid=%u local-eid=%u", which,
                r.samples, R::ClassNameOf(obj).c_str(), keyStr.c_str(), idx, static_cast<unsigned>(anyEid),
                static_cast<unsigned>(PT::GetPropElementIdForActor(obj)));
    }
    next[idx] = r;
}

// The shadow's starting point: every live keyed prop as it stands, the seed a real index takes when its
// watches go live. Taken once, when all four watches are live.
void SeedShadow() {
    const int32_t n = R::NumObjects();
    size_t seeded = 0;
    for (int32_t i = 0; i < n; ++i) {
        void* obj = R::ObjectAt(i);
        if (!obj) continue;
        if (R::SlotFlags(i) & (R::slot_flags::NotYetReadable | R::slot_flags::Unreachable)) continue;
        if (!InDomain(R::ClassOf(obj))) continue;
        if (!R::IsLive(obj)) continue;
        if (R::NameStartsWith(R::NameOf(obj), L"Default__")) continue;
        Shadow& s = g_shadow[i];
        s.obj = obj;
        s.serial = R::AllocateSlotSerial(i);
        s.key = ue_wrap::prop::GetInteractableKey(obj);
        ++seeded;
    }
    UE_LOGI("key_parity: shadow seeded with %zu live keyed-domain objects (the four watches are live)", seeded);
}

std::wstring LevelOf(void* obj) {
    void* level = R::OuterOf(obj);
    void* world = level ? R::OuterOf(level) : nullptr;
    return world ? R::ToString(R::NameOf(world)) : L"?";
}

Counts Sample() {
    Counts c;
    void* hand[1 + coop::players::kMaxPeers];
    const size_t handN = coop::hand_item::CollectHandAxisActors(hand, 1 + coop::players::kMaxPeers);
    std::vector<Truth> truth;
    truth.reserve(4096);
    const int32_t n = R::NumObjects();
    for (int32_t i = 0; i < n; ++i) {
        void* obj = R::ObjectAt(i);
        if (!obj) continue;
        if (R::SlotFlags(i) & (R::slot_flags::NotYetReadable | R::slot_flags::Unreachable)) continue;
        if (!InDomain(R::ClassOf(obj))) continue;
        if (!R::IsLive(obj)) continue;
        if (R::NameStartsWith(R::NameOf(obj), L"Default__")) continue;
        if (ue_wrap::engine::IsChildActor(obj)) continue;
        bool isHand = false;
        for (size_t h = 0; h < handN; ++h) isHand = isHand || hand[h] == obj;
        if (isHand) continue;
        const R::FName key = ue_wrap::prop::GetInteractableKey(obj);
        if (key.ComparisonIndex == 0) continue;  // None: never indexed
        truth.push_back(Truth{obj, i, key});
    }
    c.truth = truth.size();

    // The index we keep today, copied (no lookup: a lookup evicts).
    std::vector<PT::KeyIndexEntry> live;
    PT::CollectKeyIndexEntries(live);
    std::unordered_map<void*, std::wstring> liveByActor;
    liveByActor.reserve(live.size());
    for (const auto& e : live)
        if (R::IsLiveByIndex(e.actor, e.internalIdx)) liveByActor[e.actor] = e.key;

    std::unordered_map<std::wstring, int> holders;
    holders.reserve(truth.size());
    g_shadowMissByClass.clear();
    std::unordered_map<int32_t, Run> liveNext, shadowNext;
    for (const Truth& t : truth) {
        const std::wstring keyStr = R::ToString(t.key);
        if (++holders[keyStr] == 2) ++c.dupKeys;
        const auto li = liveByActor.find(t.obj);
        if (li == liveByActor.end() || li->second != keyStr)
            NoteMiss(g_liveRun, liveNext, g_livePersistNamed, "live", t.obj, t.idx, keyStr);
        if (li == liveByActor.end()) {
            ++c.liveMiss;
            if (g_liveExamples < kExamples) {
                ++g_liveExamples;
                UE_LOGI("key_parity: live MISS #%d %ls key='%ls' slot=%d world=%ls", g_liveExamples,
                        R::ClassNameOf(t.obj).c_str(), keyStr.c_str(), t.idx, LevelOf(t.obj).c_str());
            }
        } else if (li->second != keyStr) {
            ++c.liveStale;
            if (g_liveExamples < kExamples) {
                ++g_liveExamples;
                UE_LOGI("key_parity: live STALE #%d %ls key='%ls' indexed as '%ls' slot=%d", g_liveExamples,
                        R::ClassNameOf(t.obj).c_str(), keyStr.c_str(), li->second.c_str(), t.idx);
            }
        }
        const auto si = g_shadow.find(t.idx);
        const bool seen = si != g_shadow.end() && si->second.obj == t.obj &&
                          si->second.serial == R::SlotSerial(t.idx);
        if (!seen || !SameName(si->second.key, t.key))
            NoteMiss(g_shadowRun, shadowNext, g_shadowPersistNamed, "shadow", t.obj, t.idx, keyStr);
        if (!seen) {
            ++c.shadowMiss;
            ++g_shadowMissByClass[R::ClassNameOf(t.obj)];
            if (g_shadowExamples < kExamples) {
                ++g_shadowExamples;
                UE_LOGI("key_parity: shadow MISS #%d %ls key='%ls' slot=%d world=%ls", g_shadowExamples,
                        R::ClassNameOf(t.obj).c_str(), keyStr.c_str(), t.idx, LevelOf(t.obj).c_str());
            }
        } else if (!SameName(si->second.key, t.key)) {
            ++c.shadowStale;
            if (g_shadowExamples < kExamples) {
                ++g_shadowExamples;
                UE_LOGI("key_parity: shadow STALE #%d %ls key='%ls' shadow='%ls' slot=%d", g_shadowExamples,
                        R::ClassNameOf(t.obj).c_str(), keyStr.c_str(), R::ToString(si->second.key).c_str(),
                        t.idx);
            }
        }
    }
    g_liveRun.swap(liveNext);
    g_shadowRun.swap(shadowNext);
    return c;
}

void Report(const Counts& c, const char* why) {
    const char* role = PT::SessionIsHost() ? "host" : (PT::SessionIsClient() ? "client" : "none");
    UE_LOGI("key_parity: %s role=%s gen=%u truth=%zu dupkeys=%zu | live miss=%zu stale=%zu | shadow miss=%zu "
            "stale=%zu | posts init=%llu setKey=%llu getKey=%llu loadData=%llu changed=%llu/%llu/%llu/%llu | "
            "offgt=%llu backlog=%zu shadow=%zu",
            why, role, WI::Generation(), c.truth, c.dupKeys, c.liveMiss, c.liveStale, c.shadowMiss, c.shadowStale,
            static_cast<unsigned long long>(g_posts[kInit]), static_cast<unsigned long long>(g_posts[kSetKey]),
            static_cast<unsigned long long>(g_posts[kGetKey]), static_cast<unsigned long long>(g_posts[kLoadData]),
            static_cast<unsigned long long>(g_changed[kInit]), static_cast<unsigned long long>(g_changed[kSetKey]),
            static_cast<unsigned long long>(g_changed[kGetKey]),
            static_cast<unsigned long long>(g_changed[kLoadData]),
            static_cast<unsigned long long>(SG::GetStats().offGameThread), ue_wrap::object_index::Backlog(),
            g_shadow.size());
    if (c.shadowMiss > 0) {
        int shown = 0;
        for (const auto& [cls, k] : g_shadowMissByClass) {
            if (++shown > 20) break;
            UE_LOGI("key_parity:   shadow miss %6zu  %ls", k, cls.c_str());
        }
    }
}

}  // namespace

void Tick() {
    static const bool s_on = coop::config::ResolveFlag(::coop::config_registry::rows::key_parity_probe);
    if (!s_on) return;
    static bool s_watched = false;
    if (!s_watched && SG::IsInstalled()) {
        s_watched = true;
        SG::Acquire("key_parity_probe");
        SG::WatchName(kRowInit, kTagBase + kInit, nullptr, &Post);
        SG::WatchName(kRowSetKey, kTagBase + kSetKey, nullptr, &Post);
        SG::WatchName(kRowGetKey, kTagBase + kGetKey, nullptr, &Post);
        SG::WatchName(kRowLoadData, kTagBase + kLoadData, nullptr, &Post);
        UE_LOGI("key_parity: armed -- four POST watches (init, setKey, getKey, loadData), a walk every 2 s");
    }
    static bool s_seeded = false;
    if (s_watched && !s_seeded && SG::NameWatchLive(kRowInit, kTagBase + kInit) &&
        SG::NameWatchLive(kRowSetKey, kTagBase + kSetKey) && SG::NameWatchLive(kRowGetKey, kTagBase + kGetKey) &&
        SG::NameWatchLive(kRowLoadData, kTagBase + kLoadData)) {
        s_seeded = true;
        SeedShadow();
    }
    static std::uint32_t s_gen = 0;
    static Clock::time_point s_last = Clock::now(), s_lastPrint = Clock::now();
    static Counts s_prev{};
    const auto now = Clock::now();
    const std::uint32_t gen = WI::Generation();
    if (gen != s_gen) {
        s_gen = gen;
        g_liveExamples = 0;
        g_shadowExamples = 0;
        g_livePersistNamed = 0;
        g_shadowPersistNamed = 0;
        UE_LOGI("key_parity: world generation %u", gen);
    }
    if (now - s_last < std::chrono::seconds(2)) return;
    s_last = now;
    const Counts c = Sample();
    // A line when a count moved, else one every 30 s.
    if (!(c == s_prev) || now - s_lastPrint >= std::chrono::seconds(30)) {
        Report(c, c == s_prev ? "steady" : "moved");
        s_prev = c;
        s_lastPrint = now;
    }
}

}  // namespace coop::dev::key_parity_probe
