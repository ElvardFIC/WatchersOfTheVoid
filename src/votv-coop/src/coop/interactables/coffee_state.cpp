// Invocation-owned coffee snapshots and guarded record publication. Game thread.
#include "coop/interactables/coffee_state.h"
#include "coffee_lifetime.h"
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/props/prop_record_gates.h"
#include "coop/props/prop_save_data.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/coffee.h"
#include "ue_wrap/facts/function_row.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <utility>

namespace coop::coffee_state {
namespace {
namespace R = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace CF = ue_wrap::coffee;
UE_FACT_FN(kRowBrew, "coffee_state", L"prop_coffeeMaker_C", L"actionOptionIndex", Declared);
UE_FACT_FN(kRowHit, "coffee_state", L"prop_coffeeMaker_C",
           L"BndEvt__StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature", Declared);
constexpr int kTagBrew = 0x43534250, kTagHit = 0x43534854;
constexpr uint64_t kRefusalSayMs = 10000, kRetryMs = 250;
std::atomic<net::Session*> g_session{nullptr};
coffee_lifetime::Session g_scope;
bool g_brewAsked = false, g_hitAsked = false;
bool g_brewWatched = false, g_hitWatched = false, g_settled = false, g_gatesRegistered = false;
uint64_t g_cancelled = 0, g_madeMaker = 0, g_madeBag = 0, g_recordsRefused = 0, g_publishFailed = 0;
uint64_t g_tallyHits = 0, g_refusalNext = 0, g_refusalSince = 0;
ue_wrap::CachedObjRef g_lastOther;
uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct Note {
    uint64_t invocation = 0;
    int tag = 0;
    coffee_lifetime::Session scope;
    ue_wrap::CachedObjRef actor, function, bag;
    CF::MakerFlags flags{};
    int32_t amounts = 0;
};
std::array<Note, 128> g_notes;
struct Debt {
    uint64_t revision = 0, nextMs = 0;
    coffee_lifetime::Session scope;
    ue_wrap::CachedObjRef actor;
    std::wstring key;
    bool bag = false;
};
std::array<Debt, 128> g_debt;
uint64_t g_revision = 0;
size_t g_retryCursor = 0;
// Do not reset this admission guard when a callback installs a successor session.
bool g_publishing = false;
struct CancelSay {
    ue_wrap::CachedObjRef maker;
    uint64_t n = 0, nextMs = 0, lastN = 0;
};
std::array<CancelSay, 128> g_cancelSay;
struct ClassTally {
    ue_wrap::CachedObjRef cls;
    std::wstring name;
    uint64_t n = 0;
};
std::array<ClassTally, 64> g_tally;
void ClearState() {
    g_notes = {}; g_debt = {}; g_cancelSay = {}; g_tally = {};
    g_lastOther.Reset(); g_retryCursor = 0;
    g_cancelled = g_madeMaker = g_madeBag = g_recordsRefused = g_publishFailed = 0;
    g_tallyHits = g_refusalNext = g_refusalSince = 0;
}
void Reconcile() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!g_scope.Same(s)) { ClearState(); g_scope = coffee_lifetime::Session::Capture(s); }
}
bool Current(const coffee_lifetime::Session& scope) { return scope.Current(g_session.load()); }
bool Host(const coffee_lifetime::Session& scope) { return Current(scope) && scope.role == net::Role::Host; }
bool Current(const Note& n) { return Host(n.scope) && n.actor.Get() && n.function.Get(); }
Note Capture(const sg::Call& call, int tag) {
    Note n;
    n.invocation = call.invocation; n.tag = tag; n.scope = g_scope;
    n.actor.Set(call.object); n.function.Set(call.function);
    return n;
}
// Synchronous bodies only: notes cancelled by another watch may never get POST.
// Keep active ancestors, and reclaim completed/cancelled invocations on the next PRE.
void Store(Note n, const sg::Call& call) {
    if (!n.invocation || !Current(n)) return;
    for (auto& slot : g_notes) {
        if (slot.invocation && (!Current(slot) ||
            (slot.invocation != call.invocation && !sg::IsInvocationActive(slot.invocation)))) slot = {};
    }
    for (auto& slot : g_notes) {
        if (slot.invocation == n.invocation && slot.tag == n.tag) return;
    }
    for (auto& slot : g_notes) if (!slot.invocation) { slot = std::move(n); return; }
    ++g_publishFailed;
    UE_LOGW("[COFFEE-STATE] snapshot table full; this invocation has no publication note");
}
Note Consume(const sg::Call& call, int tag) {
    for (auto& slot : g_notes) {
        if (slot.invocation == call.invocation && slot.tag == tag &&
            slot.actor.Raw() == call.object && slot.function.Raw() == call.function) {
            Note n = std::move(slot); slot = {};
            return n;
        }
    }
    return {};
}
bool ReadOther(const sg::Call& call, const Note& n, void*& out) {
    if (!call.locals || !call.function || !Current(n)) return false;
    static R::ParamOffset other{L"OtherActor"};
    const int32_t off = other.Of(call.function), size = R::FunctionFrameSize(call.function);
    if (!Current(n) || off < 0 || size < static_cast<int32_t>(sizeof(void*)) ||
        off > size - static_cast<int32_t>(sizeof(void*))) return false;
    std::memcpy(&out, call.locals + off, sizeof(out));
    return true;
}
// A debt belongs to one live actor/key and one controller revision. PublishWhile
// never lends this authority to the save lane's unguarded background retry queue.
void TryDebt(size_t i) {
    if (g_publishing || !g_debt[i].revision) return;
    const Debt d = g_debt[i];
    const auto owned = [&] { return g_debt[i].revision == d.revision && Host(d.scope) && d.actor.Get(); };
    const auto current = [&] {
        if (!owned()) return false;
        const auto key = ue_wrap::prop::GetKeyString(d.actor.Get());
        return owned() && key == d.key;
    };
    struct Busy { Busy() { g_publishing = true; } ~Busy() { g_publishing = false; } } busy;
    if (!current()) { if (g_debt[i].revision == d.revision) g_debt[i] = {}; return; }
    // Set the retry time before dispatch; a reentrant newer revision owns its own time.
    g_debt[i].nextMs = NowMs() + kRetryMs;
    const bool sent = prop_save_data::PublishWhile(d.scope.pointer, d.actor.Get(), d.key, current);
    if (!Host(d.scope)) return;
    // Publication callbacks may have replaced this entry. Never erase their debt.
    if (g_debt[i].revision != d.revision) return;
    if (!current()) { if (g_debt[i].revision == d.revision) g_debt[i] = {}; return; }
    if (sent) {
        ++(d.bag ? g_madeBag : g_madeMaker);
        g_debt[i] = {};
    } else ++g_publishFailed;
}
void PublishRecord(const Note& n, const ue_wrap::CachedObjRef& actor, bool bag) {
    if (!Current(n) || !actor.Get()) return;
    const auto key = ue_wrap::prop::GetKeyString(actor.Get());
    if (!Current(n) || !actor.Get()) return;
    if (!coffee_lifetime::KeyOk(key)) { ++g_publishFailed; return; }
    size_t chosen = g_debt.size();
    for (size_t i = 0; i < g_debt.size(); ++i) {
        if (g_debt[i].revision && g_debt[i].actor.Is(actor.Get()) && g_debt[i].key == key) { chosen = i; break; }
        if (chosen == g_debt.size() && (!g_debt[i].revision || !Host(g_debt[i].scope) || !g_debt[i].actor.Get())) chosen = i;
    }
    if (chosen == g_debt.size()) {
        ++g_publishFailed;
        UE_LOGW("[COFFEE-STATE] publication debt full; record refused");
        return;
    }
    g_debt[chosen] = {++g_revision, 0, n.scope, actor, key, bag};
    TryDebt(chosen);
}
void SayCancelled(void* maker) {
    CancelSay* selected = nullptr;
    for (auto& x : g_cancelSay) if (x.maker.Is(maker)) { selected = &x; break; }
    if (!selected) for (auto& x : g_cancelSay) if (!x.maker.Get()) {
        x = {}; x.maker.Set(maker); selected = &x; break;
    }
    if (!selected) return;
    auto& c = *selected;
    ++c.n;
    if (c.n > 3 && NowMs() < c.nextMs) return;
    const auto count = c.n, since = c.n - c.lastN;
    c.lastN = count; c.nextMs = NowMs() + kRefusalSayMs;
    UE_LOGI("[COFFEE-STATE] client hit cancelled maker=%p count=%llu since=%llu", maker,
        static_cast<unsigned long long>(count), static_cast<unsigned long long>(since));
}
void TallyOther(void* other, const coffee_lifetime::Session& scope) {
    ue_wrap::CachedObjRef actor, clsRef; actor.Set(other);
    void* cls = other ? R::ClassOf(other) : nullptr;
    clsRef.Set(cls);
    if (!Current(scope) || (other && (!actor.Get() || R::ClassOf(other) != cls)) || (cls && !clsRef.Get())) return;
    for (auto& t : g_tally) if (t.n && (cls ? t.cls.Is(cls) : t.name == L"none")) {
        ++t.n; ++g_tallyHits; return;
    }
    const auto name = cls ? R::ClassNameOf(other) : std::wstring(L"none");
    if (!Current(scope) || (other && (!actor.Get() || R::ClassOf(other) != cls)) || (cls && !clsRef.Get())) return;
    for (auto& t : g_tally) if (!t.n || (t.cls.Raw() && !t.cls.Get())) {
        t = {clsRef, name, 1}; ++g_tallyHits; return;
    }
    ++g_tallyHits;
}
sg::Verdict OnBrewPre(const sg::Call& call) {
    Reconcile();
    if (!Host(g_scope) || !call.object || !call.function || !call.locals) return sg::Verdict::Run;
    Note n = Capture(call, kTagBrew);
    static R::ParamOffset action{L"action"};
    const int32_t off = action.Of(call.function), size = R::FunctionFrameSize(call.function);
    if (!Current(n) || off < 0 || off >= size || call.locals[off] != CF::kBrewAction) return sg::Verdict::Run;
    if (CF::ReadMakerFlags(call.object, n.flags) && Current(n)) Store(std::move(n), call);
    return sg::Verdict::Run;
}
void OnBrewPost(const sg::Call& call) {
    Reconcile();
    const Note n = Consume(call, kTagBrew);
    if (!n.invocation || !Current(n)) return;
    static const bool noPublish = config::ResolveEnum(config_registry::rows::coffee_red) == "no_publish";
    if (noPublish) return;
    CF::MakerFlags flags;
    if (CF::ReadMakerFlags(call.object, flags) && Current(n) && !(flags == n.flags)) PublishRecord(n, n.actor, false);
}
sg::Verdict OnHitPre(const sg::Call& call) {
    Reconcile();
    if (!Current(g_scope) || !call.object) return sg::Verdict::Run;
    const auto scope = g_scope;
    ue_wrap::CachedObjRef maker; maker.Set(call.object);
    Note n = Capture(call, kTagHit);
    // Client cancellation is independent of whether the optional diagnostic frame is readable.
    static const bool drill = config::ResolveFlag(config_registry::rows::coffee_drill);
    if (drill && call.function && call.locals) {
        // ReadOther requires a host note; the same bounds apply to a client frame.
        static R::ParamOffset other{L"OtherActor"};
        const int32_t off = other.Of(call.function), size = R::FunctionFrameSize(call.function);
        if (!Current(scope) || !maker.Get() || !n.function.Get()) return sg::Verdict::Cancel;
        if (off >= 0 && size >= static_cast<int32_t>(sizeof(void*)) && off <= size - static_cast<int32_t>(sizeof(void*))) {
            void* seen = nullptr; std::memcpy(&seen, call.locals + off, sizeof(seen));
            ue_wrap::CachedObjRef seenRef; seenRef.Set(seen);
            TallyOther(seen, scope);
            if (!Current(scope) || !maker.Get()) return sg::Verdict::Cancel;
            if (scope.role == net::Role::Client) g_lastOther = seenRef;
        }
    }
    if (scope.role == net::Role::Client) {
        static const bool noCancel = config::ResolveEnum(config_registry::rows::coffee_red) == "no_cancel";
        if (noCancel) return sg::Verdict::Run;
        if (Current(scope) && maker.Get()) { ++g_cancelled; SayCancelled(call.object); }
        return sg::Verdict::Cancel;
    }
    if (!Current(n) || !CF::ReadMakerFlags(call.object, n.flags) || !Current(n)) return sg::Verdict::Run;
    void* other = nullptr;
    if (ReadOther(call, n, other)) {
        ue_wrap::CachedObjRef bag; bag.Set(other);
        if (other && CF::IsBag(other) && bag.Get() && Current(n) &&
            CF::ReadBagAmounts(other, n.amounts) && bag.Get() && Current(n)) n.bag = bag;
    }
    if (Current(n)) Store(std::move(n), call);
    return sg::Verdict::Run;
}
void OnHitPost(const sg::Call& call) {
    Reconcile();
    const Note n = Consume(call, kTagHit);
    if (!n.invocation || !Current(n)) return;
    CF::MakerFlags flags;
    if (CF::ReadMakerFlags(call.object, flags) && Current(n) && !(flags == n.flags)) PublishRecord(n, n.actor, false);
    if (!Current(n)) return;
    void* bag = n.bag.Get();
    int32_t amounts = 0;
    if (bag && CF::ReadBagAmounts(bag, amounts) && n.bag.Get() && Current(n) && amounts != n.amounts)
        PublishRecord(n, n.bag, true);
}
bool AcceptsClientRecord(void* actor, uint8_t slot) {
    Reconcile();
    if (!Host(g_scope) || !actor || slot == 0 || slot >= net::kMaxPeers) return false;
    const auto sender = coffee_lifetime::Sender::Capture(*g_scope.pointer, slot);
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    const auto current = [&] { return sender.Current(g_session.load()) && ref.Get(); };
    if (!current()) return false;
    const auto key = ue_wrap::prop::GetKeyString(actor);
    if (!current()) return false;
    if (coffee_lifetime::KeyOk(key) && prop_save_data::AwaitsRecordFrom(key, slot) && current()) return true;
    if (!current()) return false;
    ++g_recordsRefused;
    if (NowMs() < g_refusalNext) { ++g_refusalSince; return false; }
    const auto since = g_refusalSince;
    g_refusalNext = NowMs() + kRefusalSayMs; g_refusalSince = 0;
    UE_LOGW("[COFFEE-STATE] slot %u record refused key='%ls' since=%llu", static_cast<unsigned>(slot), key.c_str(),
        static_cast<unsigned long long>(since));
    return false;
}
} // namespace
void Install(net::Session* s) {
    g_session.store(s, std::memory_order_release); Reconcile();
    if (!g_brewAsked) { g_brewAsked = true; g_brewWatched = sg::WatchName(kRowBrew, kTagBrew, OnBrewPre, OnBrewPost); }
    if (!g_hitAsked) { g_hitAsked = true; g_hitWatched = sg::WatchName(kRowHit, kTagHit, OnHitPre, OnHitPost); }
    if (!g_gatesRegistered) {
        g_gatesRegistered = true;
        prop_record_gates::RegisterIntentGate(L"prop_coffeeMaker_C", AcceptsClientRecord);
        prop_record_gates::RegisterIntentGate(L"prop_coffeeBag_C", AcceptsClientRecord);
    }
}
void Tick() {
    Reconcile();
    const auto scope = g_scope;
    sg::ResolvePendingNames();
    if (!g_settled && ((g_brewWatched && g_hitWatched && sg::NameWatchLive(kRowBrew, kTagBrew) &&
        sg::NameWatchLive(kRowHit, kTagHit)) || sg::PendingNameCount() == 0)) {
        g_settled = true;
        UE_LOGI("[COFFEE-STATE] watches settled brew=%d hit=%d",
            g_brewWatched && sg::NameWatchLive(kRowBrew, kTagBrew), g_hitWatched && sg::NameWatchLive(kRowHit, kTagHit));
    }
    if (!Host(scope) || g_publishing) return;
    size_t budget = 8;
    for (size_t scanned = 0; scanned < g_debt.size() && budget && Host(scope); ++scanned) {
        const size_t i = g_retryCursor++ % g_debt.size();
        if (!g_debt[i].revision) continue;
        if (!Host(g_debt[i].scope) || !g_debt[i].actor.Get()) { g_debt[i] = {}; continue; }
        if (NowMs() >= g_debt[i].nextMs) { --budget; TryDebt(i); }
    }
}
void OnDisconnect() {
    if (g_cancelled || g_madeMaker || g_madeBag || g_recordsRefused || g_publishFailed)
        UE_LOGI("[COFFEE-STATE] session end cancelled=%llu maker=%llu bag=%llu refused=%llu failed=%llu",
            static_cast<unsigned long long>(g_cancelled), static_cast<unsigned long long>(g_madeMaker),
            static_cast<unsigned long long>(g_madeBag), static_cast<unsigned long long>(g_recordsRefused),
            static_cast<unsigned long long>(g_publishFailed));
    g_session.store(nullptr, std::memory_order_release);
    ClearState(); g_scope = coffee_lifetime::Session::Capture(nullptr);
}
DrillView ReadForDrill() {
    Reconcile();
    DrillView v;
    v.cancelled = g_cancelled; v.hits = g_tallyHits; v.madeMaker = g_madeMaker; v.recordsRefused = g_recordsRefused;
    for (const auto& t : g_tally) if (t.n) v.byClass.push_back({t.name, t.n});
    const auto scope = g_scope;
    const auto ref = g_lastOther;
    void* other = ref.Get();
    if (other && ue_wrap::prop::IsDescendantOfProp(other) && ref.Get() && Current(scope)) {
        const auto key = ue_wrap::prop::GetKeyString(other);
        if (ref.Get() && Current(scope)) v.lastOtherKey = key;
    }
    if (!scope.Same(g_session.load())) return {};
    return v;
}
} // namespace coop::coffee_state
