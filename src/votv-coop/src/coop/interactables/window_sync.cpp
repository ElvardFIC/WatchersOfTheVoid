// coop/interactables/window_sync.cpp -- see coop/interactables/window_sync.h. Base-window dirt
// scalar sync.
//
// A window's dirt is one scalar, `clean`, lowered only by its cleanSponge (clean -= strength*0.01,
// floored at 0, then setClean repaints) and set wholesale by loadData from the save. Each peer sends
// a fall at the verb that made it: a PRE/POST watch on baseWindow_C's cleanSponge reads the value on
// both sides of the body. A receiver applies MIN-WINS (monotone cooperative clean), so a live wipe
// never raises a window another peer cleaned further; the apply writes the field and repaints through
// setClean, never the verb, so it is never heard back. The host's connect snapshot is an adopt: the
// joiner takes the host's world unchanged. The Key->actor index rides the shared scan hub, and a
// value for a window this peer has not indexed yet waits in a pending entry until a pass finds it.

#include "coop/interactables/window_sync.h"

#include "coop/config/config.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"  // WireKeyFromString / StringFromWireKey / FnvKey (shared)
#include "coop/player/players_registry.h"  // coop::players::kMaxPeers
#include "coop/player/roster_ledger.h"

#include "ue_wrap/devices/base_window.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"     // gen-stamped index (dead-world guard)
#include "ue_wrap/facts/function_row.h"
#include "coop/element/object_scan_hub.h"      // the shared sliced scan pass

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <array>
#include <set>

namespace coop::window_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace BW = ue_wrap::base_window;
namespace sg = ue_wrap::script_gate;

using coop::net::WireKeyFromString;
using coop::net::StringFromWireKey;
using coop::net::FnvKey;

// `clean` changes in wipe steps (cleanSponge: clean -= strength*0.01), and a step can be smaller
// than this epsilon (a water-shooter droplet, a slow bucket pour), so a fall is measured against the
// value last sent and small ones add up (see Sent). It also skips an idempotent apply.
constexpr float kCleanEps = 0.0005f;

bool ProbeLog() {
    static const bool s_enabled = ::coop::config::ResolveFlag(::coop::config_registry::rows::window_log);
    return s_enabled;
}

std::atomic<coop::net::Session*> g_session{nullptr};
uint64_t g_epoch=0;
uint32_t g_world=0;
void World(){const auto world=ue_wrap::world_identity::Generation();if(g_world==world)return;
    auto* session=g_session.load();OnDisconnect();g_session.store(session);g_world=world;}

struct Ref { void* actor; int32_t idx,serial; };
bool Holds(const Ref& ref){return R::IsLiveByIndex(ref.actor,ref.idx) && R::SlotSerial(ref.idx)==ref.serial;}
std::mutex g_indexMutex;
std::unordered_map<std::wstring, Ref> g_byKey;

struct Pending {float clean=0;bool adopt=false;uint64_t sequence=0;};
std::map<std::wstring,Pending> g_pending;
uint64_t g_pendingSequence=0;
std::wstring g_paintCursor;
bool g_painting=false;
size_t g_lastLogCount = SIZE_MAX;  // GT-only: dedup the rebuilt log
uint64_t g_lastLogHash = 0;        // GT-only

// World generation of the last completed hub pass; a stale-gen index is treated as EMPTY on every
// read path (dead-world guard).
uint32_t g_indexGen = 0;
bool IndexCurrent() { return g_indexGen == ue_wrap::world_identity::Generation(); }

void* ResolveFast(const std::wstring& key) {
    if (!IndexCurrent()) return nullptr;  // stale-gen index = another world's actors (R-1 class)
    std::lock_guard<std::mutex> lk(g_indexMutex);
    auto it = g_byKey.find(key);
    if (it != g_byKey.end() && Holds(it->second))
        return it->second.actor;
    return nullptr;
}

// ---- shared-scan hub consumer -------------------------------------------------------------
// There is no per-module walk; the hub's shared sliced pass drives these callbacks. settleScans=2:
// a window break/repair changes the count and would otherwise re-arm 15 passes of full demand for
// a routine event. Only 4 windows exist, so 2 stable passes suffice and the hub's 60 s backstop
// covers recycled-slot stragglers.
std::vector<std::pair<std::wstring, Ref>> g_scanFound;  // pass scratch (GT-only)
uint32_t g_scanWorld=0;

void HubPassBegin(void*, bool) { g_scanFound.clear();g_scanWorld=ue_wrap::world_identity::Generation(); }

void HubMatch(void*, void* obj) {
    const std::wstring nm = R::ToString(R::NameOf(obj));
    if (nm.rfind(L"Default__", 0) == 0) return;  // skip CDO
    if (!R::IsLive(obj)) return;
    ue_wrap::CachedObjRef identity;identity.Set(obj);
    std::wstring key = BW::GetKeyString(obj);
    if(!identity.Get() || g_scanWorld!=ue_wrap::world_identity::Generation())return;
    if (key.empty() || key == L"None") return;
    g_scanFound.emplace_back(std::move(key), Ref{ obj, identity.Idx(), R::SlotSerial(identity.Idx()) });
}

size_t HubPassComplete(void*, bool isFull, uint32_t worldGen) {
    if(worldGen!=ue_wrap::world_identity::Generation() || g_scanWorld!=worldGen){g_scanFound.clear();return 0;}
    const size_t added = g_scanFound.size();
    uint64_t keysHash = 0;
    size_t   total;
    {
        std::lock_guard<std::mutex> lk(g_indexMutex);
        if (isFull) g_byKey.clear();                           // full pass: rebuild from scratch
        for (auto& f : g_scanFound) if(Holds(f.second))g_byKey[f.first] = f.second;
        if (!isFull) {                                         // tail pass: prune dead entries (cheap, O(index))
            for (auto it = g_byKey.begin(); it != g_byKey.end(); ) {
                if (Holds(it->second)) ++it;
                else it = g_byKey.erase(it);
            }
        }
        for (auto& kv : g_byKey) keysHash ^= FnvKey(kv.first);  // recompute over the index (cheap, O(index))
        total = g_byKey.size();
        g_indexGen = worldGen;
    }
    if (total != g_lastLogCount || keysHash != g_lastLogHash) {
        g_lastLogCount = total;
        g_lastLogHash = keysHash;
        UE_LOGI("window: index now %zu live keyed window(s), keysHash=0x%016llX (%s pass, +%zu new) "
                "(compare host vs client for cross-peer Key stability)",
                total, static_cast<unsigned long long>(keysHash), isFull ? "full" : "tail", added);
    }
    if (ProbeLog())
        for (auto& f : g_scanFound)
            UE_LOGI("window[probe]: key='%ls' idx=%d actor=%p", f.first.c_str(), f.second.idx, f.second.actor);
    g_scanFound.clear();
    return total;
}

void RegisterWithScanHub() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::element::scan_hub::Register(coop::element::scan_hub::Consumer{
        "window", nullptr, &BW::EnsureResolved, &BW::IsBaseWindow,
        &HubPassBegin, &HubMatch, &HubPassComplete, /*settleScans*/ 2});
}

// Per window, the value this peer last sent, seeded at the entry of its first wipe and lowered by what
// it applies: a fall is measured against it, so falls smaller than the epsilon -- a water-shooter
// droplet, a slow bucket pour -- add up and go out once they pass it. Per world, since the save's
// load sets every window anew.
std::unordered_map<std::wstring, float> g_sent;  // GT-only
uint32_t g_sentGen = 0;
std::unordered_map<std::wstring, float>& Sent() {
    const uint32_t gen = ue_wrap::world_identity::Generation();
    if (gen != g_sentGen) {
        g_sent.clear();
        g_sentGen = gen;
    }
    return g_sent;
}
struct WipeSend {float value=0;uint64_t sequence=0,epoch=0;uint32_t world=0;coop::net::Role role=coop::net::Role::Host;};
std::map<std::wstring,WipeSend> g_wipeSends;
uint64_t g_wipeSequence=0;
std::wstring g_wipeCursor;
bool g_sendingWipes=false;
void DrainWipeSends(){
    if(g_sendingWipes)return;
    struct Guard{Guard(){g_sendingWipes=true;}~Guard(){g_sendingWipes=false;}}guard;
    const auto pending=g_wipeSends;unsigned work=8;
    auto first=pending.upper_bound(g_wipeCursor);if(first==pending.end())first=pending.begin();
    auto it=first;
    for(size_t visited=0;visited<pending.size() && work;++visited){
        const auto key=it->first;const auto row=it->second;if(++it==pending.end())it=pending.begin();
        auto* session=g_session.load();
        const auto current=[&]{return session && session==g_session.load() && row.epoch==g_epoch && session->connected() &&
            row.world==ue_wrap::world_identity::Generation() && row.role==session->role();};
        auto queued=g_wipeSends.find(key);if(queued==g_wipeSends.end() || queued->second.sequence!=row.sequence)continue;
        if(!current()){g_wipeSends.erase(queued);continue;}
        --work;g_wipeCursor=key;
        coop::net::KeyedScalarPayload p{};WireKeyFromString(key,p.key);p.value=row.value;
        const auto floor=Sent().find(key);if(floor!=Sent().end())p.value=std::min(p.value,floor->second);
        const bool sent=session->SendReliable(coop::net::ReliableKind::WindowCleanState,&p,sizeof(p));
        if(!current())return;
        queued=g_wipeSends.find(key);
        if(sent && queued!=g_wipeSends.end() && queued->second.sequence==row.sequence)g_wipeSends.erase(queued);
    }
}

bool ValidKey(const std::wstring& key){if(key.empty() || key==L"None" || key.size()>31)return false;
    return std::all_of(key.begin(),key.end(),[](wchar_t c){return c>=33 && c<=126;});}
void QueuePaint(const std::wstring& key,float value,bool adopt){
    if(!g_pending.contains(key) && g_pending.size()>=256)return;
    auto& row=g_pending[key];if(!row.sequence || adopt){row.clean=value;row.adopt=adopt;}else row.clean=std::min(row.clean,value);
    row.sequence=++g_pendingSequence;
}
void DrainPaints(){
    if(g_painting)return;
    struct Guard{Guard(){g_painting=true;}~Guard(){g_painting=false;}}guard;
    auto* session=g_session.load();if(!session || !session->connected())return;
    const auto epoch=g_epoch;const auto world=g_world;const auto role=session->role();
    const auto sourceCurrent=[&]{return g_session.load()==session && epoch==g_epoch && world==ue_wrap::world_identity::Generation() &&
        session->connected() && session->role()==role;};
    const auto pending=g_pending;unsigned work=8;auto it=pending.upper_bound(g_paintCursor);if(it==pending.end())it=pending.begin();
    for(size_t visited=0;visited<pending.size() && work;++visited){
        const auto key=it->first;const auto row=it->second;if(++it==pending.end())it=pending.begin();
        --work;g_paintCursor=key;if(!sourceCurrent())return;
        const auto queued=g_pending.find(key);if(queued==g_pending.end() || queued->second.sequence!=row.sequence)continue;
        void* actor=ResolveFast(key);if(!actor)continue;ue_wrap::CachedObjRef ref;ref.Set(actor);
        const auto current=[&]{const auto now=g_pending.find(key);return sourceCurrent() && ref.Get()==actor && ResolveFast(key)==actor &&
            now!=g_pending.end() && now->second.sequence==row.sequence;};
        float cur=0;const bool read=BW::ReadClean(actor,cur);if(!sourceCurrent())return;
        if(!read || !current() || !std::isfinite(cur))continue;
        float target=row.adopt?row.clean:std::min(cur,row.clean);
        const auto floor=Sent().find(key);const bool haveFloor=floor!=Sent().end();const float oldFloor=haveFloor?floor->second:0;
        if(!row.adopt && haveFloor)target=std::min(target,oldFloor);
        const auto valid=[&]{if(!current())return false;const auto latest=Sent().find(key);
            return (latest!=Sent().end())==haveFloor && (!haveFloor || latest->second==oldFloor);};
        const bool painted=BW::WriteCleanAndApply(actor,target,valid);if(!sourceCurrent())return;
        if(!painted || !valid())continue;
        Sent()[key]=target;g_pending.erase(key);
    }
}
#include "window_join.inc"

// ---- the sender: the wipe verb ---------------------------------------------------------------
constexpr int kTagWindowWipe = 0x57495045;  // 'WIPE'
UE_FACT_FN(kRowCleanSponge, "window_sync", L"baseWindow_C", L"cleanSponge", Declared);
bool g_wipeWatched = false;

struct WipeCapture {
    ue_wrap::CachedObjRef actor;
    coop::net::Session* session=nullptr;
    coop::net::Role role=coop::net::Role::Host;
    uint64_t invocation=0,epoch=0;
    uint32_t world=0;
    float before=0;
    bool Current()const{return session && session==g_session.load() && epoch==g_epoch &&
        world==ue_wrap::world_identity::Generation() && session->connected() && session->role()==role &&
        actor.Get() && sg::IsInvocationActive(invocation);}
};
std::vector<WipeCapture> g_wipes;
void PruneWipes(){std::erase_if(g_wipes,[](const WipeCapture& row){return !row.Current();});}

sg::Verdict OnWipePre(const sg::Call& call) {
    World();PruneWipes();if(!call.object || !call.invocation || g_wipes.size()>=128)return sg::Verdict::Run;
    WipeCapture row{};row.actor.Set(call.object);row.invocation=call.invocation;row.epoch=g_epoch;
    row.world=ue_wrap::world_identity::Generation();row.session=g_session.load();
    if(row.session)row.role=row.session->role();
    if(row.Current() && BW::IsBaseWindow(call.object) && row.Current() && BW::ReadClean(call.object,row.before) &&
       row.Current() && std::isfinite(row.before))g_wipes.push_back(row);
    return sg::Verdict::Run;
}

// After the body: the clean it left, sent when it fell. A session with nobody connected has no one
// to send to; the joiner's snapshot is the connect half.
void OnWipePost(const sg::Call& call) {
    const auto it=std::find_if(g_wipes.rbegin(),g_wipes.rend(),[&](const WipeCapture& row){
        return row.invocation==call.invocation && row.actor.Raw()==call.object;});
    if(it==g_wipes.rend())return;const auto capture=*it;g_wipes.erase(std::next(it).base());
    if(!capture.Current())return;auto* s=capture.session;
    float cur = 0.f;
    if (!BW::ReadClean(call.object, cur) || !capture.Current() || !std::isfinite(cur) || cur >= capture.before) return;
    const std::wstring key = BW::GetKeyString(call.object);
    if (!capture.Current() || key.empty() || key == L"None") return;
    auto& sent = Sent();
    const auto floor = sent.find(key);
    if(floor!=sent.end() && cur>floor->second+kCleanEps){QueuePaint(key,floor->second,false);DrainPaints();return;}
    if (cur >= (floor != sent.end() ? floor->second : capture.before) - kCleanEps) {
        if (floor == sent.end()) sent[key] = capture.before;
        return;
    }
    sent[key] = cur;
    if(auto queued=g_pending.find(key);queued!=g_pending.end()){queued->second.clean=std::min(queued->second.clean,cur);queued->second.sequence=++g_pendingSequence;}
    g_wipeSends[key]={cur,++g_wipeSequence,g_epoch,ue_wrap::world_identity::Generation(),s->role()};
    DrainWipeSends();
}

// DEV-ONLY synthetic wipe (`window_synth=1`). One-shot, host-only, armed when the joiner's world is
// ready: it runs cleanSponge through the verb itself, as a sponge would, on the first indexed window
// with dirt left (the body returns at 0), in falls too small to send alone: the watch above sends the
// value once their fall passes the epsilon, about halfway through, and the client applies it, and
// the rest, under the epsilon, stays on the host by design -- the live-wipe chain end to end without a
// hand at the keyboard. Run it with window_log=1, which logs the send and the apply. NOT shipped
// behavior.
void MaybeSyntheticWipe() {
    static const bool s_on = ::coop::config::ResolveFlag(::coop::config_registry::rows::window_synth);
    if (!s_on) return;
    static bool s_done = false;
    if (s_done) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host || !s->IsSlotWorldReady(1)) return;
    if (!sg::NameWatchLive(kRowCleanSponge, kTagWindowWipe)) return;  // the send rides it
    void* actor = nullptr;
    std::wstring key;
    float before = 0.f;
    {
        std::lock_guard<std::mutex> lk(g_indexMutex);
        for (const auto& kv : g_byKey) {
            float clean = 0.f;
            if (!Holds(kv.second) || !BW::ReadClean(kv.second.actor, clean) ||
                clean <= 0.f)
                continue;
            key = kv.first;
            actor = kv.second.actor;
            before = clean;
            break;
        }
    }
    if (!actor) return;
    s_done = true;
    // Ten dabs of 0.0001, each under the epsilon and twice it together: the watch must send once they
    // have added up past it, and the client apply that value.
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(actor), kRowCleanSponge.Name());
    int ran = 0;
    for (int i = 0; fn && i < 10; ++i) {
        ue_wrap::ParamFrame f(fn);
        if (f.valid() && f.Set<float>(L"clean", 0.01f) && ue_wrap::Call(actor, f)) ++ran;
    }
    float after = before;
    BW::ReadClean(actor, after);
    UE_LOGW("window[SYNTH]: %d of 10 dabs of 0.0001 through cleanSponge -- key='%ls' clean %.4f -> %.4f",
            ran, key.c_str(), before, after);
}

}  // namespace

void Install(coop::net::Session* session) {
    if(g_session.load()!=session)OnDisconnect();
    World();g_world=ue_wrap::world_identity::Generation();
    g_session.store(session, std::memory_order_release);
    RegisterWithScanHub();  // the hub builds the index on its own cadence
    if (!g_wipeWatched)
        g_wipeWatched = sg::WatchName(kRowCleanSponge, kTagWindowWipe, &OnWipePre, &OnWipePost);
}

void OnReliable(const coop::net::KeyedScalarPayload& payload, uint8_t senderPeerSlot) {
    std::wstring key = StringFromWireKey(payload.key);
    if (key.empty()) { UE_LOGW("window: OnReliable empty key -- dropping"); return; }
    World();auto* session=g_session.load();if(!session || !session->connected() || !ValidKey(key) ||
        !std::isfinite(payload.value) || payload.value<0 || payload.adopt>1)return;
    const bool adopt=payload.adopt && senderPeerSlot==0 && session->role()==coop::net::Role::Client;
    QueuePaint(key,payload.value,adopt);
    DrainPaints();
}

void QueueConnectBroadcastForSlot(int slot){
    World();auto* session=g_session.load();
    if(!session || !session->connected() || session->role()!=coop::net::Role::Host || slot<=0 || slot>=coop::net::kMaxPeers)return;
    auto& row=g_joins[slot];row={};row.sequence=++g_joinSequence;row.person=roster_ledger::Get(slot).playerNo;row.waiting=true;
}
void OnPeerLeftSlot(int slot){if(slot<=0 || slot>=coop::net::kMaxPeers)return;++g_peerEpoch[slot];g_joins[slot]={};}

void Tick() {
    World();PruneWipes();DrainWipeSends();DrainJoins();DrainPaints();
    if (!BW::EnsureResolved()) return;
    RegisterWithScanHub();
    if (IndexCurrent())MaybeSyntheticWipe();
}

void OnDisconnect() {
    ++g_epoch;g_session.store(nullptr,std::memory_order_release);g_world=0;g_joins={};g_joinTurn=1;
    g_pending.clear();g_paintCursor.clear();
    g_wipes.clear();
    g_wipeSends.clear();g_wipeCursor.clear();
    g_sent.clear();
}

}  // namespace coop::window_sync
