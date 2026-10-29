// coop/props/prop_save_data.cpp -- see coop/props/prop_save_data.h.

#include "coop/props/prop_save_data.h"
#include "coop/props/prop_record_gates.h"
#include "coop/props/remote_prop.h"
#include "coop/props/prop_hold.h"
#include "coop/props/prop_drive_host.h"
#include "ue_wrap/actors/safe.h"
#include "coop/props/prop_food_sync.h"
#include "ue_wrap/actors/food.h"
#include "ue_wrap/devices/grill_saved_state.h"
#include "ue_wrap/actors/prop.h"

#include "coop/dev/food_clock_probe.h"   // [dev] the food catch-up's two clocks, read at these seams
#include "coop/items/save_record_wire.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_key_index.h"
#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"    // ClassByName -- the host's O(1) class resolve for a client's record
#include "ue_wrap/core/reflection.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cwchar>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace coop::prop_save_data {
bool StillAwaited(const std::wstring& key);
namespace {

namespace R  = ue_wrap::reflection;
namespace SR = ue_wrap::save_record;
namespace SW = coop::save_record_wire;
namespace PT = coop::prop_element_tracker;

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// The body grammar, version-tagged so a future field is a parse branch and not a guess.
constexpr uint8_t kBodyVersion = 1;

// A Key is an Aprop_C save key: ASCII, at most WireKey's 31 chars.
constexpr size_t kMaxKeyChars = 31;

// The record ceiling, sized from what a record COSTS rather than from what the transport allows.
// The transport would carry 56 KB, and the codec that decodes one allocates engine memory per
// declared group and orphans it by this layer's out-param doctrine, so the transport's maximum is
// an invitation rather than a bound. MEASURED on the test world: most covered props' records are a
// few hundred bytes and the largest real one is 11.3 KB, so 32 KB is roughly three times the
// biggest thing the game actually produces and still refuses a body shaped to make the decoder
// allocate. [An 8 KB first guess was wrong and the run said so within a minute -- a cap picked
// from an estimate rather than from a census refused a legitimate record.] A record over the cap
// is REFUSED and logged, never truncated: half a record through loadData is worse than none, and
// the peer that still holds the whole prop can re-publish.
constexpr size_t kMaxRecordBytes = 32768;
size_t MaxRecordBytes() { return kMaxRecordBytes; }

// The park: records whose prop has not arrived, or whose apply has not had a budget slot yet.
// Keyed by Key and NEVER expired (the header says why). Sized for a legitimate join: one record
// per covered prop, measured at 766 in the test world.
//
// The cap is PER SENDER, for the reason blob_chunks states about its own table: a global one lets
// one flooding peer evict every other peer's rows. A sender's records are its own to lose. Both a
// count and a byte ceiling, since a record's size is the sender's to choose.
constexpr size_t kMaxParkedPerSender  = 2048;
constexpr size_t kMaxParkBytesPerSender = 4u * 1024u * 1024u;

// An apply is a ProcessEvent into the prop's own loadData, which on the Aprop_C lineage re-runs
// init(), physicsImpact->init() and setNametag() -- the same work a save load does. Measured at
// ~0.7 ms: applying a whole join's worth in the drain frame that closes the snapshot put 519 ms on
// a frame already 1.5 s long. So applies are BUDGETED per frame and the surplus waits in the park,
// which is the queue this lane already has. An apply costs TWO dispatches, the base capture and
// loadData itself, so 8 a frame is about the same ~11 ms and a 766-record join converges in a
// second or two with every prop already correct in everything but its save payload.
constexpr int    kAppliesPerFrame = 8;
// How far into the park one frame looks. The park is ordered by Key, so the cursor rotates through
// it and nothing starves; bounded so a large park is never a per-frame walk of thousands.
constexpr size_t kParkScanPerFrame = 64;

struct Parked {
    SR::SaveRecord rec;
    uint8_t        senderSlot = 0;
    uint64_t       seq        = 0;   // arrival order, for the oldest-first eviction
    size_t         bytes      = 0;   // the body this came from, for the per-sender byte ceiling
    int            failures   = 0;   // applies that reached a dispatch and did not take
    bool preserveGrillRuntime=false;
    uint64_t awaited=0;             // exact initial-state wait that admitted this row
};
// A row that will not apply is kept (the park has no expiry, by design) but stops costing a
// dispatch a frame. Not covered, not resolvable and not applicable all look the same from here.
constexpr int kMaxApplyFailures = 4;
std::map<std::wstring, Parked> g_parked;
uint64_t g_parkSeq = 1;
uint64_t g_worldEpoch = 0;
// Native getData/loadData can re-enter the birth and receive seams. These scopes belong to
// the C++ call stack, not the world: OnDisconnect must not clear them while a call is active.
std::set<std::wstring> g_applying;
std::set<std::wstring> g_publishing;
#include "prop_save_order.inc"
struct KeyScope {
    KeyScope(std::set<std::wstring>& active, const std::wstring& key)
        : active(active), key(key) { active.insert(key); }
    ~KeyScope() { active.erase(key); }
    std::set<std::wstring>& active;
    const std::wstring key;
};
bool g_driving = false;
struct DriveScope {
    DriveScope() { g_driving = true; }
    ~DriveScope() { g_driving = false; }
};
struct SenderUse { size_t count = 0; size_t bytes = 0; };
std::map<uint8_t, SenderUse> g_parkUse;   // per-sender occupancy, so one peer cannot evict another's

// Keys whose capture or transport publish failed. A lost record is a permanent divergence
// unless it is re-sent, and it can be: the record is derivable from the live actor at any time.
// Bounded, and retried a few per frame from Drive().
constexpr size_t kMaxRetry        = 512;
constexpr int    kRetriesPerFrame = 4;
constexpr int    kMaxRetryTries   = 8;
constexpr uint64_t kRetryBackoffMs = 250;   // doubled per attempt; the refusal is usually a full queue
struct Refused {
    ue_wrap::CachedObjRef source; // newborns may not be in the key index yet
    int      peerSlot = -1;    // -1 = it was a broadcast
    int      tries    = 0;
    uint64_t nextTryMs = 0;
    uint64_t seq = 0;          // replacement identity across native capture callbacks
};
using RetryKey = std::pair<std::wstring, int>; // each recipient owns its retry and backoff
std::map<RetryKey, Refused> g_retry;
uint64_t g_retrySeq = 1;
void QueuePublishRetry(const std::wstring& key, int peerSlot,
                       const ue_wrap::CachedObjRef* source = nullptr) {
    const RetryKey target{key, peerSlot};
    auto it = g_retry.find(target);
    if (it != g_retry.end()) {
        it->second.peerSlot = peerSlot;
        it->second.seq = g_retrySeq++;
        it->second.source = source ? *source : ue_wrap::CachedObjRef{};
    } else if (g_retry.size() < kMaxRetry) {
        Refused r;
        r.peerSlot = peerSlot;
        r.seq = g_retrySeq++;
        if (source) r.source = *source;
        g_retry[target] = r;
    } else {
        UE_LOGW("prop_save_data: retry register full (%zu), cannot retain key '%ls'",
                kMaxRetry, key.c_str());
    }
}
std::array<uint64_t, 256> g_peerEpoch{};
coop::net::Session* g_session = nullptr;          // for the retry, which has no caller of its own

// Classes a dedicated lane owns, by name, and the resolved answer per UClass.
// Transparent comparator so the per-tick re-declaration below can LOOK UP a `const wchar_t*`
// without minting a std::wstring for it -- every claimed name is past the small-string buffer, so
// the plain set allocated one per lane per net_pump tick to answer a question it already knew.
std::set<std::wstring, std::less<>> g_ownedElsewhere;
struct OwnedClass { ue_wrap::CachedObjRef identity; bool owned = false; };
std::map<void*, OwnedClass> g_ownedCache;

std::wstring g_parkCursor;      // where the next frame's park scan resumes
int g_applyBudget = kAppliesPerFrame;
uint64_t g_nextSweepMs = 0;
// Applied since connect, both paths. Per-record lines are not logged -- a join lands hundreds --
// so this is the number a run is read for, printed when the park empties.
uint64_t g_appliedTotal = 0;

coop::blob_chunks::Assembler g_asmCanonical;
coop::blob_chunks::Assembler g_asmIntent;
uint32_t g_blobSeq = 1;

// The host birth path's traffic ledger. That path publishes for EVERY prop the host gives birth to
// at runtime, not only discs, so what it costs is measured rather than assumed: one line a minute
// with the window and the session so far (a rig peer is killed, not disconnected, so the running
// total is what survives), and the session again at teardown.
struct BirthTally { uint64_t records = 0; uint64_t bytes = 0; };
struct BirthLedger {
    BirthTally total;
    std::map<std::wstring, BirthTally> byClass;
    uint64_t declined  = 0;   // a covered birth the publisher sent nothing for
    uint64_t uncovered = 0;   // a birth whose class carries no save state of its own
};
BirthLedger g_birthWindow;
BirthLedger g_birthSession;
uint64_t g_birthWindowStartMs = 0;
uint64_t g_birthSessionStartMs = 0;
constexpr uint64_t kBirthReportMs = 60000;

std::string TopClasses(const BirthLedger& l) {
    std::vector<std::pair<std::wstring, BirthTally>> v(l.byClass.begin(), l.byClass.end());
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
        return a.second.bytes > b.second.bytes;
    });
    std::string out;
    for (size_t i = 0; i < v.size() && i < 5; ++i) {
        char row[160];
        std::snprintf(row, sizeof(row), "%s%ls %llu/%lluB", i ? ", " : "", v[i].first.c_str(),
                      static_cast<unsigned long long>(v[i].second.records),
                      static_cast<unsigned long long>(v[i].second.bytes));
        out += row;
    }
    return out.empty() ? std::string("none") : out;
}

void ReportBirthLedger(const char* when, const BirthLedger& l, uint64_t spanMs) {
    const double secs = spanMs > 0 ? static_cast<double>(spanMs) / 1000.0 : 1.0;
    UE_LOGI("prop_save_data: HOST BIRTH RECORDS %s (%.0f s) -- %llu sent, %llu B (%.1f B/s, %.1f "
            "records/min), %llu declined, %llu births with no save state of their own; top by "
            "bytes: %s", when, secs, static_cast<unsigned long long>(l.total.records),
            static_cast<unsigned long long>(l.total.bytes),
            static_cast<double>(l.total.bytes) / secs,
            static_cast<double>(l.total.records) * 60.0 / secs,
            static_cast<unsigned long long>(l.declined),
            static_cast<unsigned long long>(l.uncovered), TopClasses(l).c_str());
}

// Host only, from Drive: the minute's line and the running session line under it.
void TickBirthLedger(uint64_t now) {
    if (!PT::SessionIsHost()) return;
    if (!g_birthWindowStartMs) { g_birthWindowStartMs = g_birthSessionStartMs = now; return; }
    if (now - g_birthWindowStartMs < kBirthReportMs) return;
    // A minute with no host birth says nothing, and the session line under it has not moved.
    const bool quiet = !g_birthWindow.total.records && !g_birthWindow.declined && !g_birthWindow.uncovered;
    if (quiet) { g_birthWindowStartMs = now; return; }
    ReportBirthLedger("last minute", g_birthWindow, now - g_birthWindowStartMs);
    ReportBirthLedger("session so far", g_birthSession, now - g_birthSessionStartMs);
    g_birthWindow = BirthLedger{};
    g_birthWindowStartMs = now;
}

std::vector<uint8_t> BuildBody(const std::wstring& key, const SR::SaveRecord& rec) {
    std::vector<uint8_t> body;
    body.push_back(kBodyVersion);
    body.push_back(static_cast<uint8_t>(key.size()));
    for (wchar_t c : key) body.push_back(static_cast<uint8_t>(c & 0x7F));
    SW::SerSave(body, rec);
    return body;
}

// Strict on format: an ill-formed body is refused whole (the receive-boundary rule -- strict on
// format, permissive on state).
bool ParseBody(const std::vector<uint8_t>& body, std::wstring& key, SR::SaveRecord& rec) {
    size_t o = 0;
    uint8_t ver = 0, keyLen = 0;
    if (!SW::RdU8(body, o, ver) || ver != kBodyVersion) return false;
    if (!SW::RdU8(body, o, keyLen) || keyLen == 0 || keyLen > kMaxKeyChars) return false;
    if (o + keyLen > body.size()) return false;
    key.assign(body.begin() + static_cast<long>(o), body.begin() + static_cast<long>(o + keyLen));
    o += keyLen;
    if (!SW::DeSave(body, o, rec)) return false;
    // The body ADDRESSES a prop by the head key and CARRIES a record whose own key field the
    // game's loadData would write onto that prop. If the two disagree the message is asking to
    // rename one prop to another's identity, so it is refused whole rather than reconciled.
    return rec.key.empty() || rec.key == key;
}

// Drop one parked row and give its sender the room back.
void Unpark(std::map<std::wstring, Parked>::iterator it) {
    SenderUse& u = g_parkUse[it->second.senderSlot];
    if (u.count) --u.count;
    u.bytes = (u.bytes > it->second.bytes) ? (u.bytes - it->second.bytes) : 0;
    UE_LOGI("prop_save_data: Unpark key '%ls' (slot %u)", it->first.c_str(), it->second.senderSlot);
    g_parked.erase(it);
}

// A newer record for a Key supersedes whatever waits under it, whether the newer one is applied at
// once or parked: a predecessor left behind would be landed later by Drive or a birth, on top of it.
void DropParked(const std::wstring& key) {
    auto it = g_parked.find(key);
    if (it != g_parked.end()) Unpark(it);
}

// After an apply of a parked record: drop its row only if the row is still that record. A parked
// apply runs on a COPY of the record, since the node can be replaced or erased while loadData runs,
// and a newer record that replaced it there must survive the older one's completion.
void UnparkIfSame(const std::wstring& key, uint64_t seq) {
    auto it = g_parked.find(key);
    if (it != g_parked.end() && it->second.seq == seq) Unpark(it);
}

void Park(const std::wstring& key, SR::SaveRecord&& rec, uint8_t senderSlot, size_t bytes) {
    RecordChanged(key);
    DropParked(key);
    SenderUse& use = g_parkUse[senderSlot];
    while (use.count >= kMaxParkedPerSender || use.bytes + bytes > kMaxParkBytesPerSender) {
        // This sender's own oldest row goes, never another sender's.
        auto oldest = g_parked.end();
        for (auto it = g_parked.begin(); it != g_parked.end(); ++it) {
            if (it->second.senderSlot != senderSlot) continue;
            if (oldest == g_parked.end() || it->second.seq < oldest->second.seq) oldest = it;
        }
        if (oldest == g_parked.end()) break;   // nothing of this sender's left to give back
        UE_LOGW("prop_save_data: slot %u is at its park ceiling (%zu rows, %zu B) -- evicting its "
                "oldest record (key '%ls'); that prop's save state is now whatever its peer holds",
                static_cast<unsigned>(senderSlot), use.count, use.bytes, oldest->first.c_str());
        Unpark(oldest);
    }
    Parked p;
    p.rec = std::move(rec);
    p.senderSlot = senderSlot;
    p.seq = g_parkSeq++;
    p.bytes = bytes;
    use.count += 1;
    use.bytes += bytes;
    g_parked[key] = std::move(p);
    UE_LOGI("prop_save_data: Parked record for key '%ls' (slot %u, seq %llu)", key.c_str(), senderSlot, g_parkSeq - 1);
}

// A client's intent is a write into the host's world, so its RATE is bounded per sender the way
// its size is. The ceiling is generous against real play -- an eject, a drop, a hand switch are
// human-rate -- and refuses a stream. A refusal is loud once per window, not per message, because
// a flood must not become a log flood.
constexpr int      kIntentsPerWindow = 60;
constexpr uint64_t kIntentWindowMs   = 10000;
struct IntentRate { uint64_t windowStartMs = 0; int count = 0; bool warned = false; };
std::map<uint8_t, IntentRate> g_intentRate;

bool AllowIntent(uint8_t senderSlot) {
    IntentRate& r = g_intentRate[senderSlot];
    const uint64_t now = NowMs();
    if (now - r.windowStartMs >= kIntentWindowMs) {
        r.windowStartMs = now;
        r.count = 0;
        r.warned = false;
    }
    if (++r.count <= kIntentsPerWindow) return true;
    if (!r.warned) {
        r.warned = true;
        UE_LOGW("prop_save_data: slot %u is past %d save-record intents in %llu ms -- refusing the "
                "rest of this window", static_cast<unsigned>(senderSlot), kIntentsPerWindow,
                static_cast<unsigned long long>(kIntentWindowMs));
    }
    return false;
}

// THE ONE APPLY. Every path that writes a record onto an actor goes through here, because the
// class gate belongs with the write and not with the send: `Covers` answers both "does this class
// carry save state of its own" and "has another lane claimed it", and a record that reaches a
// claimed class would write the field that lane arbitrates with a compare-and-swap.
bool SendBody(coop::net::Session* s,int peerSlot,const std::wstring& key,const SR::SaveRecord& rec,
              const ue_wrap::CachedObjRef* source = nullptr,const std::function<bool()>& current = {},bool retryInternally = true);
#include "prop_save_apply.inc"

bool SendBody(coop::net::Session* s, int peerSlot, const std::wstring& key,
              const SR::SaveRecord& rec, const ue_wrap::CachedObjRef* source,const std::function<bool()>& current,bool retryInternally) {
    const std::vector<uint8_t> body = BuildBody(key, rec);
    if (body.size() > MaxRecordBytes()) {
        UE_LOGW("prop_save_data: record for key '%ls' is %zu B, over the %zu cap -- REFUSED, not "
                "truncated; that prop's state stays where it is. If this fires on a legitimate "
                "prop the cap is wrong, not the prop.",
                key.c_str(), body.size(), MaxRecordBytes());
        return false;
    }
    const bool host = PT::SessionIsHost();
    const auto epoch=g_worldEpoch;const auto world=ue_wrap::world_identity::Generation();
    const auto peerEpoch=peerSlot<0?0:g_peerEpoch[peerSlot];
    const auto same=[&]{return s && s->connected() && g_session==s && epoch==g_worldEpoch &&
        world==ue_wrap::world_identity::Generation() && PT::SessionIsHost()==host &&
        (peerSlot<0 || peerEpoch==g_peerEpoch[peerSlot]) && (!source || source->Get()) && (!current || current());};
    const auto valid=[&]{if(!same())return false;if(!source)return true;
        const auto actual=ue_wrap::prop::GetInteractableKeyString(source->Get());return same() && actual==key;};
    const coop::net::ReliableKind kind = host ? coop::net::ReliableKind::PropSaveData
                                              : coop::net::ReliableKind::PropSaveDataIntent;
    const bool ok = (peerSlot < 0)
        ? coop::blob_chunks::SendBlobWhile(s, kind, g_blobSeq++, body,valid)
        : coop::blob_chunks::SendBlobToSlotWhile(s, peerSlot, kind, g_blobSeq++, body,valid);
    if (!valid())return false;
    if (!ok && retryInternally) {
        // blob_chunks' own contract: a refused blob is retried whole under a fresh seq. Doing that
        // is what makes the header's "a dropped record is a permanent divergence" false, so the
        // key goes on the retry register rather than into a warning nobody can act on.
        QueuePublishRetry(key, peerSlot, source);
    }
    return ok;
}

}  // namespace

void ExpectRecordFor(void* actor, const std::wstring& key, uint8_t senderSlot) {
    if (key.empty() || !Covers(actor)) return;   // no record of its own here: none is owed
    if (g_awaiting.size() >= kMaxAwaiting && !g_awaiting.count(key)) {
        // OLDEST, not lexicographically first: evicting by key order drops a wait minted seconds
        // ago and keeps a stale one, which is the opposite of what a cap is for.
        auto oldest = g_awaiting.begin();
        for (auto it = g_awaiting.begin(); it != g_awaiting.end(); ++it)
            if (it->second.deadlineMs < oldest->second.deadlineMs) oldest = it;
        g_awaiting.erase(oldest);
    }
    Await a;
    a.senderSlot = senderSlot;
    a.deadlineMs = NowMs() + kAwaitTimeoutMs;
    a.seq=++g_awaitSeq;
    g_awaiting[key] = a;
    RecordChanged(key); // New birth supersedes an active record scope.
    // An early author record may already have failed on this new actor's readiness.
    // Do not adopt the old row from a still-active native capture/apply scope.
    auto parked=g_parked.find(key);
    if(!g_applying.count(key) && !g_publishing.count(key) && parked!=g_parked.end() && parked->second.senderSlot==senderSlot){
        parked->second.awaited=a.seq;parked->second.preserveGrillRuntime=false;
    }
}

bool AwaitsRecordFrom(const std::wstring& key, uint8_t senderSlot) {
    const auto it = g_awaiting.find(key);
    return it != g_awaiting.end() && it->second.senderSlot == senderSlot && NowMs() < it->second.deadlineMs;
}

// True while the author's record for this key has not landed here. A wait that has run out is
// dropped here rather than lingering: an intent can be lost to a refused rate window, a malformed
// body, a dropped peer or a prop that died first, or sit parked on an apply that keeps failing, and
// a key nobody clears would silence this lane's publisher for that prop for the rest of the session
// -- the join seed included.
bool StillAwaited(const std::wstring& key) {
    auto it = g_awaiting.find(key);
    if (it == g_awaiting.end()) return false;
    if (NowMs() < it->second.deadlineMs) return true;
    UE_LOGW("prop_save_data: waited %llu ms for slot %u's record for key '%ls' and it never landed "
            "-- publishing this key again from what the host has",
            static_cast<unsigned long long>(kAwaitTimeoutMs),
            static_cast<unsigned>(it->second.senderSlot), key.c_str());
    g_awaiting.erase(it);
    return false;
}

void DeclareClassOwnedElsewhere(const wchar_t* className) {
    if (!className || !*className) return;
    // Install re-declares every tick. Only a new claim invalidates the per-class answer.
    if (g_ownedElsewhere.find(className) != g_ownedElsewhere.end()) return;
    g_ownedElsewhere.insert(className);
    g_ownedCache.clear();
}

bool Covers(void* actor) {
    if (!actor) return false;
    void* cls = R::ClassOf(actor);
    if (!SR::OverridesGetData(cls)) return false;
    if (g_ownedElsewhere.empty()) return true;
    // Inspect this actor's short ancestry, never look up every claimed class: an unloaded
    // claim made FindClass scan the entire object array on each cold class/pocket edge.
    // Ancestry also works before the object index has drained a newly loaded class.
    const auto it = g_ownedCache.find(cls);
    if (it != g_ownedCache.end() && it->second.identity.Is(cls)) return !it->second.owned;
    bool owned = false;
    void* ancestor = cls;
    for (int hop = 0; ancestor && hop < 64; ++hop, ancestor = R::SuperStructOf(ancestor)) {
        if (!R::IsLive(ancestor)) return false;
        const std::wstring name = R::ToString(R::NameOf(ancestor));
        if (name.empty()) return false;
        for (const std::wstring& claim : g_ownedElsewhere)
            if (_wcsicmp(name.c_str(), claim.c_str()) == 0) { owned = true; break; }
        if (owned) break;
    }
    if (ancestor && !owned) return false; // malformed/cyclic ancestry: do not claim its state
    OwnedClass result; result.identity.Set(cls); result.owned = owned;
    g_ownedCache[cls] = result;
    return !owned;
}

namespace {
bool PublishToTarget(coop::net::Session* s, int peerSlot, void* actor, const std::wstring& key,
                     const std::function<bool()>& current = {}) {
    if (!s || !actor || key.empty() || key.size() > kMaxKeyChars ||
        peerSlot >= static_cast<int>(g_peerEpoch.size()) || (current && !current())) return false;
    // A capture/apply callback must not publish a partially loaded record or recurse into getData.
    if (g_applying.count(key) || g_publishing.count(key)) return false;
    // Nobody to publish to: return before the capture, not at the send. A host loading its own
    // world births every keyed prop it owns, and paying getData plus a serialize per prop for a
    // send that cannot land is the whole cost of the lane spent on nothing.
    if (!s->connected() || (peerSlot < 0 ? !s->AnyWorldReadyPeer() : !s->IsSlotWorldReady(peerSlot)))
        return false;
    if (StillAwaited(key)) return false;       // the author's own record is still in flight
    g_session = s;
    if (!Covers(actor)) { coop::dev::food_clock_probe::NoteRefused(actor); return false; }
    KeyScope scope(g_publishing, key); // owns the key even if a callback clears its source queue
    RecordScope order(key);const auto revision=order.Revision();
    const RetryKey retryTarget{key, peerSlot};
    const auto previousRetry = g_retry.find(retryTarget);
    const uint64_t retrySequence = previousRetry == g_retry.end() ? 0 : previousRetry->second.seq;
    const uint64_t epoch = g_worldEpoch;
    const auto world=ue_wrap::world_identity::Generation();
    const uint64_t peerEpoch = peerSlot < 0 ? 0 : g_peerEpoch[peerSlot];
    const auto stillCurrent = [&] {
        return epoch == g_worldEpoch && world==ue_wrap::world_identity::Generation() && order.Revision()==revision &&
            s->connected() && g_session==s && (peerSlot < 0 || peerEpoch == g_peerEpoch[peerSlot]) && (!current || current());
    };
    SR::SaveRecord rec;
    ue_wrap::CachedObjRef identity;
    identity.Set(actor);
    if (!stillCurrent() || identity.Get() != actor) return false;
    if (!SR::CaptureRecord(actor, rec)) {
        if (!current && stillCurrent() && identity.Get() == actor && s->connected())
            QueuePublishRetry(scope.key, peerSlot, &identity);
        UE_LOGW("prop_save_data: getData failed on the prop keyed '%ls' -- no record published",
                scope.key.c_str());
        return false;
    }
    if (!stillCurrent() || identity.Get() != actor) return false;
    coop::dev::food_clock_probe::NotePublish(actor, scope.key, rec);
    if (!stillCurrent()) return false;
    const bool sent = SendBody(s, peerSlot, scope.key, rec, &identity,stillCurrent,!current);
    if (sent && retrySequence && stillCurrent()) {
        // A normal publication can satisfy an older failed publication too.
        // Do not erase replacement work queued by capture/transport callbacks.
        const auto current = g_retry.find(retryTarget);
        if (current != g_retry.end() && current->second.seq == retrySequence) g_retry.erase(current);
    }
    return sent;
}
} // namespace

bool Publish(coop::net::Session* s, void* actor, const std::wstring& key) {
    return PublishToTarget(s, -1, actor, key);
}

bool PublishWhile(coop::net::Session* s, void* actor, const std::wstring& key,
                  const std::function<bool()>& current) {
    return current && PublishToTarget(s, -1, actor, key, current);
}

bool PublishToSlot(coop::net::Session* s, int peerSlot, void* actor, const std::wstring& key) {
    return peerSlot >= 0 && PublishToTarget(s, peerSlot, actor, key);
}

bool PublishWithSpawn(coop::net::Session* s, void* actor, const coop::net::WireKey& key,
                      int peerSlot) {
    // Food's native loadData interprets a foreign clock stamp as elapsed time. Its live
    // condition follows the birth through FoodState instead, including same-key rebirths.
    if (s && actor && ue_wrap::food::IsFood(actor))
        return coop::props::prop_food_sync::PublishSpawnState(*s, actor, peerSlot);
    if (key.len == 0 || key.len > kMaxKeyChars) return false;   // a keyless prop has no identity to address
    std::wstring w;
    w.reserve(key.len);
    for (uint8_t i = 0; i < key.len; ++i) w.push_back(static_cast<wchar_t>(key.data[i]));
    return peerSlot < 0 ? Publish(s, actor, w) : PublishToSlot(s, peerSlot, actor, w);
}

void OnChunk(coop::net::Session& s, const coop::net::BlobChunkPayload& p, uint8_t senderSlot,
             bool intent) {
    // An intent is a claim addressed to the authority; a client receiving one has nothing to do
    // with it, and honouring it would make every peer an author. A canonical is the other half of
    // the same rule: only the host writes one, and the host's receive edge refuses a client's
    // (client_authorship.h). A refusal of a message sent by the wrong role, not a gate on state --
    // the record itself is never second-guessed.
    const bool host = PT::SessionIsHost();
    if (intent && !host) return;
    std::vector<uint8_t> body;
    if (!(intent ? g_asmIntent : g_asmCanonical).OnChunk(p, senderSlot, body)) return;
    // The same ceiling the sender enforces, applied on receive: a body between it and the
    // transport's own limit would be applied here and then REFUSED on the re-publish, leaving this
    // peer holding state no other peer can be given.
    if (body.size() > MaxRecordBytes()) {
        UE_LOGW("prop_save_data: %s body from slot %u is %zu B, over the %zu ceiling -- dropped",
                intent ? "intent" : "record", static_cast<unsigned>(senderSlot), body.size(),
                MaxRecordBytes());
        return;
    }
    if (intent && !AllowIntent(senderSlot)) return;

    std::wstring key;
    SR::SaveRecord rec;
    if (!ParseBody(body, key, rec)) {
        UE_LOGW("prop_save_data: ill-formed %s body from slot %u (%zu B) -- dropped whole",
                intent ? "intent" : "record", static_cast<unsigned>(senderSlot), body.size());
        return;
    }
    uint64_t awaited=0;
    if(intent && StillAwaited(key)){
        const auto& wait=g_awaiting.at(key);
        if(wait.senderSlot!=senderSlot)return;
        awaited=wait.seq;
    }
    RecordScope order(key);RecordChanged(key);const auto revision=order.Revision();
    if (!intent) {
        LandRecord(key, std::move(rec), senderSlot, body.size(), /*budgeted=*/true);
        return;
    }
    // Receipt is not native application. Keep the birth wait until this exact
    // author's record lands, including parking and readiness retries.
    const bool initialRecord=awaited!=0;
    // The host queues the claim under its exact revision. Its canonical is sent only after
    // the native apply and the class gates succeed; an unknown actor's claim waits in the park.
    // A newer reentrant claim owns a new revision and cannot be retired by the older apply.
    void* actor = PT::FindLiveActorByKey(key);
    if (actor && !Covers(actor)) {
        // The claim names a class another lane owns, or one that carries no save state of its own.
        // Neither applying nor re-publishing it is this lane's business, and re-publishing would
        // put the field that lane arbitrates on the wire from a second address space.
        UE_LOGW("prop_save_data: client record for key '%ls' (slot %u) names a class this lane does "
                "not carry -- dropped, not republished",
                key.c_str(), static_cast<unsigned>(senderSlot));
        return;
    }
    g_session = &s;
    const auto epoch=g_worldEpoch,peerEpoch=g_peerEpoch[senderSlot];
    const auto world=ue_wrap::world_identity::Generation();
    ue_wrap::CachedObjRef source;source.Set(actor);
    const auto current=[&]{return epoch==g_worldEpoch && peerEpoch==g_peerEpoch[senderSlot] &&
        world==ue_wrap::world_identity::Generation() && order.Revision()==revision && s.connected() &&
        AwaitCompatible(key,senderSlot,awaited) && (!actor || source.Get()==actor);};
    // Ordinary records cannot bypass native host unlock/loot or re-lock a safe.
    // The initial record for a new client-authored birth may restore its saved
    // unlocked state; no new loot belongs to that inventory/drop transition.
    if(actor) {
        const auto& cls = rec.className;
        const bool can = cls == L"prop_cannedFood_C" || cls == L"prop_cannedFood_0_C" ||
            cls == L"prop_cannedFood_1_C" || cls == L"prop_cannedFood_2_C" || cls == L"prop_cannedFood_3_C";
        if (!initialRecord && can) {
            // Cans cannot refill or close natively. An ordinary stale writer
            // may consume more portions, but cannot restore an earlier count.
            // Birth records remain exempt so a reconstructed can restores its
            // actual saved state rather than the new actor's class defaults.
            SR::SaveRecord host;
            if (!PrepareForMutation(actor, key) || !current() || !SR::CaptureRecord(actor, host) || !current()) return;
            if (host.className != cls || host.key != key || rec.ints.empty() || host.ints.empty() ||
                rec.ints[0].size() < 2 || host.ints[0].size() < 2 ||
                rec.bools.size() < 2 || host.bools.size() < 2 || rec.bools[1].empty() || host.bools[1].empty() ||
                rec.ints[0][1] < 0 || host.ints[0][1] < 0) return;
            rec.ints[0][0] = host.ints[0][0];
            rec.ints[0][1] = std::min(rec.ints[0][1], host.ints[0][1]);
            rec.bools[1][0] = (rec.bools[1][0] || host.bools[1][0]) ? 1 : 0;
        }
        if(initialRecord)ue_wrap::safe::PreserveUnlocked(actor,rec);
        else ue_wrap::safe::PreserveHostLock(actor,rec);
        if(!current())return;
        if(!initialRecord && ue_wrap::grill_saved_state::Supported(actor) &&
            !ue_wrap::grill_saved_state::PreserveHostRecord(actor,rec))return;
        if(!current())return;
    }
    if(actor && (!coop::prop_record_gates::IntentAllows(actor,senderSlot) || !current()))return;
    // ApplyQueued publishes the canonical only after this record lands.
    // Preserve immediate host acceptance (bounded by AllowIntent); client join catch-up uses
    // the frame budget. Deferring host acceptance would expose default state to other publishers.
    LandRecord(key, std::move(rec), senderSlot, body.size(), false,!initialRecord,awaited);
}

bool ApplyParked(void* actor, const std::wstring& key) {
    // ApplyRecord's bool is the DISPATCH's, not the Blueprint's: Aprop_C::loadData never assigns
    // its `return` out-param at all, so there is no refusal to read. A failure here is a codec or
    // reflection failure, and the record stays parked for the next frame to retry.
    return ApplyQueued(actor, key, false);
}

bool ApplyInitialParked(void* actor,const std::wstring& key,uint8_t authorSlot) {
    if(g_applying.count(key) || g_publishing.count(key))return false;
    const auto parked=g_parked.find(key);
    if(parked==g_parked.end() || parked->second.senderSlot!=authorSlot)return false;
    parked->second.preserveGrillRuntime=false;
    return ApplyParked(actor,key);
}

bool PrepareForCapture(void* actor, const std::wstring& key) {
    if (!actor || key.empty() || g_applying.count(key) || g_publishing.count(key)) return false;
    if (!g_parked.count(key)) return !StillAwaited(key);
    ue_wrap::CachedObjRef identity;
    identity.Set(actor);
    const uint64_t epoch = g_worldEpoch;
    ApplyQueued(actor, key, false);
    // Native callbacks may replace the actor/world, queue a row or start a new birth.
    // Drain a matching initial row first, then refuse any outstanding author wait.
    return epoch == g_worldEpoch && identity.Get() == actor && !g_parked.count(key) && !StillAwaited(key);
}
bool PrepareForMutation(void* actor,const std::wstring& key) {
    return !StillAwaited(key) && PrepareForCapture(actor,key);
}

// Re-send a few refused records per frame, re-captured from the live actor so what goes out is
// current rather than a stale copy of what was refused.
void DriveRetries() {
    if (g_retry.empty() || !g_session || !g_session->connected()) return;
    const uint64_t now = NowMs();
    const uint64_t epoch = g_worldEpoch;
    int sent = 0;
    for (auto it = g_retry.begin(); it != g_retry.end() && sent < kRetriesPerFrame;) {
        if (now < it->second.nextTryMs) { ++it; continue; }
        const int recipient = it->second.peerSlot;
        const auto& recordKey = it->first.first;
        // Loading and ownership waits are not failed capture/transport attempts.
        // Leave their retry budget intact and continue to other ready recipients.
        if (!(recipient < 0 ? g_session->AnyWorldReadyPeer() : g_session->IsSlotWorldReady(recipient)) ||
            StillAwaited(recordKey) || g_applying.count(recordKey) || g_publishing.count(recordKey)) {
            ++it;
            continue;
        }
        void* actor = PT::FindLiveActorByKey(it->first.first);
        if (!actor) {
            actor = it->second.source.Get();
            // A valid address alone is insufficient: this actor may have been rekeyed.
            if (actor && ue_wrap::prop::GetInteractableKeyString(actor) != it->first.first) actor = nullptr;
        }
        if (!actor) { it = g_retry.erase(it); continue; }   // the prop is gone; nothing to re-send
        if (it->second.tries >= kMaxRetryTries) {
            UE_LOGW("prop_save_data: gave up re-sending the record for key '%ls' after %d "
                    "attempts -- that peer's copy of this prop stays divergent",
                    it->first.first.c_str(), kMaxRetryTries);
            it = g_retry.erase(it);
            continue;
        }
        // Charge the attempt and back off BEFORE publishing. A refusal re-enters this row through
        // SendBody and keeps the count, so a full queue -- which is what refuses in the first
        // place -- is retried on a widening interval instead of four times a frame forever. The
        // Readiness/ownership waits above do not charge attempts; actual capture or transport
        // failures below remain bounded and use the existing backoff.
        ++it->second.tries;
        it->second.nextTryMs = now + (kRetryBackoffMs << (it->second.tries - 1));
        const int slot = it->second.peerSlot;
        const RetryKey target = it->first;
        const std::wstring& key = target.first;
        const uint64_t seq = it->second.seq;
        ++sent;
        const bool ok = (slot < 0) ? Publish(g_session, actor, key)
                                   : PublishToSlot(g_session, slot, actor, key);
        if (epoch != g_worldEpoch) return;
        const auto current = g_retry.find(target);
        if (ok && current != g_retry.end() && current->second.seq == seq) g_retry.erase(current);
        it = g_retry.upper_bound(target); // no iterator or row reference survives native getData
    }
}

void Drive() {
    if (g_driving) return; // a native callback must not replenish this frame's apply budget
    DriveScope scope;
    const uint64_t epoch = g_worldEpoch;
    g_applyBudget = kAppliesPerFrame;
    DriveRetries();
    if (epoch != g_worldEpoch) return;
    // A fresh apply budget per frame, then spend what is left of it on the park. The scan resumes
    // at the cursor and wraps, so a record whose prop appears late is reached in bounded time
    // however large the park is.
    size_t looked = 0;
    // Do not repeatedly revisit a single failing row in one frame. The outer bound also limits
    // work if callbacks replace or insert rows while this pass runs.
    const size_t scanLimit = (std::min)(kParkScanPerFrame, g_parked.size());
    int    applied = 0;
    while (looked < scanLimit && g_applyBudget > 0 && !g_parked.empty()) {
        auto it = g_parked.upper_bound(g_parkCursor);
        if (it == g_parked.end()) it = g_parked.begin();
        ++looked;
        const std::wstring key = it->first;
        g_parkCursor = key;
        void* actor = (it->second.failures < kMaxApplyFailures)
                          ? PT::FindLiveActorByKey(key) : nullptr;
        if (ApplyQueued(actor, key, true)) ++applied;
        if (epoch != g_worldEpoch) return;
        // Never use it after ApplyQueued: native callbacks may invalidate ANY queue node.
    }
    if (applied && !g_parked.empty()) {
        UE_LOGI("prop_save_data: applied %d parked record(s) this frame (%zu still parked)",
                applied, g_parked.size());
    } else if (applied) {
        UE_LOGI("prop_save_data: park drained -- %llu record(s) applied since connect",
                static_cast<unsigned long long>(g_appliedTotal));
    }

    // The assembler TTL sweep is 1 Hz, not per frame: a half-assembly is minutes old before it is
    // stale, and this runs from the gameplay tick.
    const uint64_t nowMs = NowMs();
    if (nowMs < g_nextSweepMs) return;
    g_nextSweepMs = nowMs + 1000;
    const auto now = std::chrono::steady_clock::now();
    g_asmCanonical.Sweep(now, std::chrono::seconds(30));
    g_asmIntent.Sweep(now, std::chrono::seconds(30));
    TickBirthLedger(nowMs);
}

void OnDisconnect() {
    ++g_worldEpoch;
    g_parkCursor.clear();
    g_applyBudget = kAppliesPerFrame;
    g_nextSweepMs = 0;
    g_appliedTotal = 0;
    g_retry.clear();
    g_retrySeq = 1;
    g_intentRate.clear();
    g_parkUse.clear();
    g_session = nullptr;
    g_awaiting.clear();
    // The codec's cached prop_C and per-class override answers are world-scoped: a Blueprint class
    // can be unloaded and reloaded across a level change, and a stale UClass pointer would answer
    // the membership test for a class that no longer exists.
    SR::ResetCodecCache();
    // Same reason, same keys: g_ownedCache is keyed by UClass pointer, and a recycled address
    // would answer the ownership question for a different class.
    g_ownedCache.clear();
    g_parked.clear();
    g_parkSeq = 1;
    g_asmCanonical.Clear();
    g_asmIntent.Clear();
    g_blobSeq = 1;
}

void OnPeerGone(uint8_t senderSlot) {
    ++g_peerEpoch[senderSlot];
    for (auto it = g_retry.begin(); it != g_retry.end();) {
        if (it->second.peerSlot == senderSlot) it = g_retry.erase(it);
        else ++it;
    }
    g_asmCanonical.ClearSlot(senderSlot);
    g_asmIntent.ClearSlot(senderSlot);
    for (auto it = g_parked.begin(); it != g_parked.end();) {
        if (it->second.senderSlot == senderSlot) { const auto dead = it++; Unpark(dead); }
        else ++it;
    }
    // The occupancy goes with the rows. Leaving it behind wedges the NEXT occupant of this slot to
    // a single parked record, evicting its own on every arrival, and turns each Park into two full
    // scans of the park -- slots are recycled lowest-free, so the next joiner inherits it.
    g_parkUse.erase(senderSlot);
    g_intentRate.erase(senderSlot);   // and a fresh occupant starts with a fresh rate window
    for (auto it = g_awaiting.begin(); it != g_awaiting.end();) {
        if (it->second.senderSlot == senderSlot) it = g_awaiting.erase(it);
        else ++it;
    }
}

}  // namespace coop::prop_save_data
