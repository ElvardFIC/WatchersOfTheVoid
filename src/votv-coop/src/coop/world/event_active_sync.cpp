// coop/world/event_active_sync.cpp -- see coop/world/event_active_sync.h.
//
// Bytecode ground truth, from the blueprint:
//   - lib_C::setEvent(isEventActive, deactivateAmbientTrack, __WorldContext) -- the context
//     is the LAST parameter. Active adds 1 to gamemode.activeEvents and appends the context
//     to activeEvents_senders; inactive subtracts 1, removes the context, and clamps a
//     negative count back to 0. The sender registers ITSELF, so ClassOf(sender) names the
//     event class.
//   - lib_C::getEvent is `activeEvents > 0` OR the player camera being outside the base box.
//     The native no-save-during-event gate reads it; single player does not even pause
//     mid-event.
//   - Around ninety-five classes call setEvent: creature controllers, story events, pranks,
//     ambience. Several can be active at once, which is why the game keeps a refcount rather
//     than a bool.

#include "coop/world/event_active_sync.h"

#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/world/event_fire_sync.h"

#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"   // Generation, the world the begin times belong to
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/active_events.h"

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::event_active_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace AE = ue_wrap::active_events;
namespace sg = ue_wrap::script_gate;

std::atomic<coop::net::Session*> g_session{nullptr};

// ---- the host's watch on setEvent (game thread) ----------------------------------------------
constexpr int kTagEventActive = 0x45564143;  // 'EVAC'
UE_FACT_FN(kRowSetEvent, "event_active_sync", nullptr, L"setEvent", AnyClass);
bool g_watchInstalled = false;
bool g_watchLive = false;
void* g_setEventFn = nullptr;      // lib_C::setEvent, resolved from its class on first need
int32_t g_offActive = -1;          // isEventActive
int32_t g_offContext = -1;         // __WorldContext, the sender

// When each event the watch saw begin began, for the snapshot's elapsed time and the END line.
struct Began {
    ue_wrap::CachedObjRef sender;
    std::string className;  // ClassOf(sender), narrowed: the event's implementation class
    long long ms;           // steady-clock ms at its setEvent(true)
};
std::unordered_map<void*, Began> g_began;
uint32_t g_beganGen = 0;           // the world the begin times belong to
struct JoinDebt { bool pending = false; uint32_t generation = 0; };
std::array<JoinDebt, coop::net::kMaxPeers> g_joinDebt{};
std::chrono::steady_clock::time_point g_joinRetry{};
uint64_t g_joinEpoch = 0;
uint32_t g_joinWorld = 0, g_joinSerial = 0;
bool g_sendingJoin = false;

long long NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ---- the class->row map ------------------------------------------------------------------
// ClassOf(sender) is the event's IMPLEMENTATION class, not the list_events row the replay
// policy is keyed by. A WRONG entry replays the wrong event, and a MISSING one logs LOUD on
// the receiver and names the class to add, so a row is added only once its link has been read
// out of the runEvent case table or seen live -- never guessed. Classes that register but map to no scheduled row (sub-chains like
// trigger_alarm_C, weather senders, creature controllers spawned BY events) stay unmapped by
// design: their state rides their own lanes, not replay.
struct ClassRow { const char* className; const char* rowName; };
const ClassRow kClassRowMap[] = {
    { "obelisk_C", "obelisk" },                         // proven live
    { "piramid2_C", "piramid" },                        // proven live
    { "trigger_solarBoom_C", "solar" },
    { "trigger_vehtp_C", "vehtp" },
    { "trigger_agrav_C", "agrav" },
    { "trigger_bigmRoar_C", "call0" },
    { "trigger_wispSwarm_C", "wisps" },
    { "trigger_spawnFollowingArir_C", "arirFollower" },
    { "trigger_arirEgg_C", "arirEgg" },
    { "trigger_bedEvent_C", "bedEvent" },
    { "tentacleBallsFollower_C", "tentacleBalls" },
    { "soltomiaCleaning_C", "soltoClean" },
    { "morningUfo_C", "morningGay" },
    { "rozitBorg_C", "borgRozital" },
    { "event_bottomHoleController_C", "rozitalHole" },
    { "ventCrawler_C", "ventCrawler" },
    { "kocker_C", "ventKnocker" },
    { "grayEventController_C", "graysforest" },
    { "arirBusterSpawner_C", "arirBuster" },
    { "saltpile_C", "salt" },
    { "superEgger_C", "eggvasion" },
    { "boarInvasion_C", "boarwar" },
    { "dreamer_dreambase_C", "dreambase" },
    { "arirShip_C", "arirShip" },
};

const char* RowForClass(const std::string& className) {
    for (const auto& e : kClassRowMap)
        if (className == e.className) return e.rowName;
    return nullptr;
}

// Registrants whose state a dedicated LANE snapshots at the same join edge. An EventSnapshot
// for them would only ship an unmapped-row WARN to every joiner, so they are skipped with an
// INFO and the WARN stays meaningful for genuinely uncovered classes.
struct LaneOwned { const char* className; const char* lane; };
const LaneOwned kLaneOwnedClasses[] = {
    { "trigger_alarm_C", "alarm_sync" },
    { "newsky_C", "sky_sync (current black-hole material/light)" },
    { "arirShipAppear_C", "ariral_ship_sync (current phase and photo listener)" },
};
const char* LaneFor(const std::string& className) {
    for (const auto& e : kLaneOwnedClasses)
        if (className == e.className) return e.lane;
    return nullptr;
}

// Does the registry name `actor` an Npc element: a creature, whose count rides coop/world/creature_event_relay?
bool IsCreature(void* actor) {
    const coop::element::Registry& reg = coop::element::Registry::Get();
    const coop::element::Element* e = reg.Get(reg.EidForActor(actor));
    return e && e->GetType() == coop::element::ElementType::Npc;
}

std::string Narrow(const std::wstring& w) {
    std::string s;
    s.reserve(w.size());
    for (wchar_t c : w) s.push_back((c > 0 && c < 128) ? static_cast<char>(c) : '?');
    return s;
}

int ReadRefcount() {
    int32_t n = 0;
    if (AE::EnsureResolved()) AE::ReadCount(n);
    return n;
}

// lib_C::setEvent and its two parameters, from the library's class once it has loaded.
bool ResolveSetEvent() {
    static bool s_failed = false;  // the class loaded without the function or its parameters: said once
    if (g_offContext >= 0) return true;
    if (s_failed) return false;
    void* cls = ue_wrap::object_index::ClassByName(L"lib_C");
    if (!cls) return false;
    g_setEventFn = R::FindFunction(cls, L"setEvent");
    g_offActive = g_setEventFn ? R::FindParamOffset(g_setEventFn, L"isEventActive") : -1;
    const int32_t ctx = g_setEventFn ? R::FindParamOffset(g_setEventFn, L"__WorldContext") : -1;
    if (g_offActive < 0 || ctx < 0) {
        UE_LOGW("event_active: lib_C::setEvent did not resolve (fn=%p isEventActive=%d __WorldContext=%d) -- "
                "no event's begin or end is seen", g_setEventFn, g_offActive, ctx);
        g_setEventFn = nullptr;
        s_failed = true;
        return false;
    }
    g_offContext = ctx;
    return true;
}

// A world or save reload minted a new gamemode with its world, and the old begin times name dead
// senders: drop them (an event still in flight in the new world has no begin the watch saw).
void ForgetOtherWorlds() {
    const uint32_t gen = ue_wrap::world_identity::Generation();
    if (gen == g_beganGen) return;
    g_began.clear();
    g_beganGen = gen;
}

// HOST: an event began or ended, after the game's own bookkeeping ran, so the refcount the lines
// print is the one it left.
void OnSetEventPost(const sg::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) return;
    if (!call.locals || !ResolveSetEvent() || call.function != g_setEventFn) return;
    const auto size = R::FunctionFrameSize(call.function);
    if (g_offActive < 0 || g_offActive >= size || g_offContext < 0 ||
        size < int(sizeof(void*)) || g_offContext > size - int(sizeof(void*))) return;
    void* sender = *reinterpret_cast<void* const*>(call.locals + g_offContext);
    if (!sender || !R::IsLive(sender)) return;
    const bool active = call.locals[g_offActive] != 0;
    ForgetOtherWorlds();
    const long long now = NowMs();
    if (const auto old = g_began.find(sender); old != g_began.end() && old->second.sender.Get() != sender)
        g_began.erase(old);
    if (active) {
        ue_wrap::CachedObjRef identity; identity.Set(sender);
        const auto [it, fresh] = g_began.emplace(sender, Began{identity, Narrow(R::ClassNameOf(sender)), now});
        if (fresh) UE_LOGI("event_active: BEGIN class=%s n=%d", it->second.className.c_str(), ReadRefcount());
        return;
    }
    const auto it = g_began.find(sender);
    if (it == g_began.end()) {
        UE_LOGI("event_active: END class=%s n=%d (began before this host watched)",
                Narrow(R::ClassNameOf(sender)).c_str(), ReadRefcount());
        return;
    }
    UE_LOGI("event_active: END class=%s n=%d elapsed=%llds", it->second.className.c_str(), ReadRefcount(),
            (now - it->second.ms) / 1000);
    g_began.erase(it);
}

}  // namespace

void Install(coop::net::Session* session) {
    const auto world = ue_wrap::world_identity::Generation(), serial = coop::net::session_serial::Current();
    if (g_session.load(std::memory_order_acquire) != session || g_joinWorld != world || g_joinSerial != serial) {
        ++g_joinEpoch; g_joinDebt = {}; g_joinRetry = {};
        g_joinWorld = world; g_joinSerial = serial;
    }
    g_session.store(session, std::memory_order_release);
    // Called every pump tick by the install fanout, which is also the retry until the gate has
    // resolved the watch's name.
    if (!g_watchInstalled)
        g_watchInstalled = sg::WatchName(kRowSetEvent, kTagEventActive, nullptr, &OnSetEventPost);
    if (g_watchInstalled && !g_watchLive) {
        sg::ResolvePendingNames();
        if (sg::NameWatchLive(kRowSetEvent, kTagEventActive)) {
            g_watchLive = true;
            UE_LOGI("event_active: every event's begin and end is seen at lib_C::setEvent (a script-gate watch)");
        }
    }
    const auto now = std::chrono::steady_clock::now();
    if (!g_sendingJoin && session && session->connected() && session->role() == coop::net::Role::Host && now >= g_joinRetry) {
        g_joinRetry = now + std::chrono::seconds(1);
        const auto epoch = g_joinEpoch;
        for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) if (g_joinDebt[slot].pending) {
            if (epoch != g_joinEpoch || g_session.load(std::memory_order_acquire) != session) return;
            if (!session->HasPeerConn(slot) || g_joinDebt[slot].generation != session->peerGenerationForSlot(slot))
                g_joinDebt[slot] = {};
            else SendJoinSnapshotForSlot(slot);
        }
    }
}

void SendJoinSnapshotForSlot(int slot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host ||
        slot <= 0 || slot >= coop::net::kMaxPeers || !s->HasPeerConn(slot)) return;
    const auto generation = s->peerGenerationForSlot(slot);
    g_joinDebt[slot] = {true, generation};
    if (g_sendingJoin) return;
    g_sendingJoin = true;
    struct Sending { ~Sending() { g_sendingJoin = false; } } sending;
    const auto epoch = g_joinEpoch;
    const auto serial = coop::net::session_serial::Current();
    const auto world = ue_wrap::world_identity::Generation();
    const auto current = [&] {
        return epoch == g_joinEpoch && g_session.load(std::memory_order_acquire) == s && s->connected() &&
            s->role() == coop::net::Role::Host && world == ue_wrap::world_identity::Generation() &&
            serial == coop::net::session_serial::Current() && s->HasPeerConn(slot) &&
            generation == s->peerGenerationForSlot(slot);
    };
    // The game's own registry is the membership: read at the edge, it holds exactly the events in
    // flight now, the ones that began before this host watched included.
    AE::Senders arr{};
    if (!AE::EnsureResolved() || !AE::ReadSenders(arr) || arr.num < 0 || arr.num > 4096) {
        UE_LOGI("event_active: join-edge slot=%d -- no event registry to read (no world yet)", slot);
        return;
    }
    if (!current()) return;
    ForgetOtherWorlds();
    const long long now = NowMs();
    std::vector<ue_wrap::CachedObjRef> members;
    if (arr.num && !arr.data) return;
    for (int32_t i = 0; i < arr.num; ++i) {
        void* obj = arr.data[i];
        if (obj && R::IsLive(obj)) { ue_wrap::CachedObjRef ref; ref.Set(obj); members.push_back(ref); }
    }
    std::vector<void*> sent;
    bool complete = true;
    for (const auto& member : members) {
        if (!current()) return;
        void* obj = member.Get();
        // The array is a property, so the collector nulls a destroyed sender's entry; one marked
        // for death and not yet collected is skipped here.
        if (!obj || !R::IsLive(obj)) continue;
        bool dup = false;
        for (void* o : sent) dup |= (o == obj);
        if (dup) continue;  // a sender that registered twice is one event
        sent.push_back(obj);
        const std::string className = Narrow(R::ClassNameOf(obj));
        if (const char* lane = LaneFor(className)) {
            UE_LOGI("event_active: join-edge slot=%d class=%s is LANE-OWNED (%s snapshots it) "
                    "-- no EventSnapshot",
                    slot, className.c_str(), lane);
            continue;
        }
        const char* row = RowForClass(className);
        // A creature with no row has no replay to ship: its count rides CreatureEventActive, or stays each
        // viewer's own (coop/world/creature_event_relay). The WARN below stays for classes nothing covers.
        if (!row && IsCreature(obj)) {
            UE_LOGI("event_active: join-edge slot=%d class=%s is a creature -- its count rides CreatureEventActive "
                    "(or stays per viewer), no EventSnapshot", slot, className.c_str());
            continue;
        }
        const auto b = g_began.find(obj);
        const bool known = b != g_began.end() && b->second.sender.Get() == obj;
        const long long elapsed = known ? (now - b->second.ms) / 1000 : 0;
        coop::net::EventSnapshotPayload p{};  // zero-init -> both name[]s pre-NUL-bound
        std::strncpy(p.className, className.c_str(), sizeof(p.className) - 1);
        if (row) std::strncpy(p.rowName, row, sizeof(p.rowName) - 1);
        p.elapsedSec = static_cast<uint16_t>(elapsed < 0 ? 0 : (elapsed > 65535 ? 65535 : elapsed));
        if (!current()) return;
        if (member.Get() != obj) continue;
        if (s->SendReliableToSlot(slot, coop::net::ReliableKind::EventSnapshot, &p, sizeof(p))) {
            UE_LOGI("event_active: join-edge slot=%d SNAPSHOT class=%s row=%s elapsed=%s", slot,
                    className.c_str(), row ? row : "<unmapped>",
                    known ? (std::to_string(elapsed) + "s").c_str() : "unknown (began before this host watched)");
        } else {
            complete = false;
            UE_LOGW("event_active: join-edge slot=%d EventSnapshot send FAILED (class=%s)",
                    slot, className.c_str());
        }
    }
    if (sent.empty())
        UE_LOGI("event_active: join-edge slot=%d -- 0 in flight (no EventSnapshot needed)", slot);
    if (complete && current()) g_joinDebt[slot] = {};
}

void OnReliable(const coop::net::EventSnapshotPayload& payload) {
    if (!GT::IsGameThread()) { UE_LOGW("event_active: OnReliable off-game-thread -- dropping"); return; }
    // NUL-bound both names (payload crosses the trust boundary; the dispatcher length-checked it).
    char cls[sizeof(payload.className) + 1] = {};
    std::memcpy(cls, payload.className, sizeof(payload.className));
    char row[sizeof(payload.rowName) + 1] = {};
    std::memcpy(row, payload.rowName, sizeof(payload.rowName));
    if (cls[0] == '\0') {
        UE_LOGW("event_active: EventSnapshot with empty className -- dropping");
        return;
    }
    if (row[0] == '\0') {
        // The fill signal: this exact line names the class the map is missing.
        UE_LOGW("event_active: in-flight event class=%s elapsed=%us has NO class->row map entry "
                "-- skipped (add it to kClassRowMap; lanes still deliver lane-owned state)",
                cls, static_cast<unsigned>(payload.elapsedSec));
        return;
    }
    UE_LOGI("event_active: join snapshot -- in-flight class=%s row=%s elapsed=%us",
            cls, row, static_cast<unsigned>(payload.elapsedSec));
    coop::event_fire_sync::ReplayInFlightRow(row);
}

void OnDisconnect() {
    ++g_joinEpoch; g_joinDebt = {}; g_joinRetry = {};
    g_began.clear();
    g_beganGen = 0;
    g_session.store(nullptr, std::memory_order_release);
}

}  // namespace coop::event_active_sync
