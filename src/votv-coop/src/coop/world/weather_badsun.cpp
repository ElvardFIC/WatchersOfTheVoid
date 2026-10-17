// coop/world/weather_badsun.cpp -- see coop/world/weather_badsun.h.

#include "coop/world/weather_badsun.h"

#include "coop/element/element.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/bad_sun.h"
#include "ue_wrap/world/world_singleton.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

namespace coop::weather_badsun {
namespace {

namespace BS = ue_wrap::bad_sun;
namespace GT = ue_wrap::game_thread;
namespace R = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;

UE_FACT_FN(kRowSpawnBadSun, "weather_badsun", L"mainGamemode_C", L"Spawn Bad Sun", Declared);
UE_FACT_FN(kRowRemove, "weather_badsun", L"badSun_C", L"remove", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};

// Game thread, but for the session pointer.
constexpr int kSpawnTag = 0x42535350;   // 'BSSP'
constexpr int kRemoveTag = 0x42535245;  // 'BSRE'
bool g_gateAsked = false;     // the watches were registered (or refused, said)
bool g_saidSpawnWatch = false;   // the spawn watch's settled state was said
bool g_saidRemoveWatch = false;  // the remove watch's settled state was said
bool g_applying = false;      // the lane's own apply of the host's Bad Sun runs
// This session's counts, said at its end.
uint64_t g_refused = 0;       // a client's own `Spawn Bad Sun` refused
bool     g_saidRefused = false;
uint64_t g_sent = 0;          // the host's state sent as a call left it
uint64_t g_spawned = 0;       // the host's Bad Sun spawned here
uint64_t g_destroyed = 0;     // an inactive copy destroyed here
uint64_t g_leftActive = 0;    // an active copy left to end by its own clock
uint64_t g_applyFailed = 0;   // an apply that did not leave this peer as the host's state

// The lane's own apply, for the length of one spawn: the gate lets exactly this call run.
struct Applying {
    Applying() { g_applying = true; }
    ~Applying() { g_applying = false; }
    Applying(const Applying&) = delete;
    Applying& operator=(const Applying&) = delete;
};

bool IsHost(coop::net::Session* s) { return s && s->connected() && s->role() == coop::net::Role::Host; }

// CLIENT, before the gamemode's `Spawn Bad Sun`: the host's state owns whether a copy exists, and only the lane's
// apply runs. No world's load calls this verb (a saved copy comes back with the world), so no clause for a world
// this client has not announced ready.
sg::Verdict OnSpawnPre(const sg::Call& /*call*/) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() == coop::net::Role::Host) return sg::Verdict::Run;
    if (g_applying) return sg::Verdict::Run;
    ++g_refused;
    if (!g_saidRefused) {
        g_saidRefused = true;
        UE_LOGI("weather: a client refused its own Spawn Bad Sun -- the host's state owns whether a Bad Sun exists "
                "(first refusal; the rest are counted)");
    }
    return sg::Verdict::Cancel;
}

// HOST: the state a call left, to every world-ready client. `what` names the call for the log.
void SendState(coop::net::Session* s, bool present, const char* what) {
    if (!s->AnyWorldReadyPeer()) {
        UE_LOGI("weather: the host's %s ran -- Bad Sun %d; no client's world is ready, the seed carries it", what,
                present ? 1 : 0);
        return;
    }
    coop::net::BadSunPayload p{};
    const coop::element::ElementId self = coop::players::Registry::Get().LocalPlayerElementId();
    p.senderElementId = self == coop::element::kInvalidId ? 0u : self;
    p.state = present ? 1 : 0;
    if (!s->SendReliable(coop::net::ReliableKind::BadSun, &p, sizeof(p))) {
        UE_LOGW("weather: the host's Bad Sun %d was not sent -- SendReliable failed", present ? 1 : 0);
        return;
    }
    ++g_sent;
    UE_LOGI("weather: the host's %s ran -- sent Bad Sun %d", what, present ? 1 : 0);
}

// HOST, after `Spawn Bad Sun` ran: the field the verb just wrote.
void OnSpawnPost(const sg::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!IsHost(s)) return;
    bool present = false;
    if (!BS::ReadGamemodeCopy(call.object, present)) {
        UE_LOGW("weather: the host's Spawn Bad Sun ran and the gamemode's copy did not read -- nothing sent");
        return;
    }
    SendState(s, present, "Spawn Bad Sun");
}

// EVERY peer, after a copy's own remove() ran; on the host also the end the clients are told. Nothing else sends
// absent: a world's teardown or a session's end is no edge.
void OnRemovePost(const sg::Call& /*call*/) {
    UE_LOGI("weather: this copy's own remove() ran");
    auto* s = g_session.load(std::memory_order_acquire);
    if (!IsHost(s)) return;
    SendState(s, false, "remove()");
}

void FailedApply(const char* why, bool want) {
    if (g_applyFailed++ == 0)
        UE_LOGW("weather: the host's Bad Sun %d did not apply -- %s (first failure; the rest are counted, and the "
                "next edge tries again)", want ? 1 : 0, why);
}

// One copy's state, for the log.
const char* StateOf(void* copy) {
    bool active = false;
    if (!BS::ReadActive(copy, active)) return "unreadable";
    return active ? "active" : "inactive";
}

// One watch's settled state, said once.
void SayWatch(bool& said, const ue_wrap::facts::Function& row, int tag, const char* what, const char* consequence) {
    if (said || !sg::NameWatchSettled(row, tag)) return;
    said = true;
    if (sg::NameWatchLive(row, tag))
        UE_LOGI("weather: the %s watch is live", what);
    else
        UE_LOGE("weather: the %s watch is dead -- %s", what, consequence);
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (g_gateAsked) return;
    g_gateAsked = true;
    if (!sg::WatchName(kRowSpawnBadSun, kSpawnTag, &OnSpawnPre, &OnSpawnPost))
        UE_LOGE("weather: the script gate refused the watch on mainGamemode_C.Spawn Bad Sun -- the host's fresh Bad "
                "Sun does not cross, and a client's own Spawn Bad Sun runs");
    if (!sg::WatchName(kRowRemove, kRemoveTag, nullptr, &OnRemovePost))
        UE_LOGE("weather: the script gate refused the watch on badSun_C.remove -- client copies still end by their "
                "clocks; the host's removal sends nothing");
}

void Tick() {
    if (g_saidSpawnWatch && g_saidRemoveWatch) return;
    if (!g_gateAsked) return;
    // The class loads lazily: its watch goes live when the class resolves.
    sg::ResolvePendingNames();
    SayWatch(g_saidSpawnWatch, kRowSpawnBadSun, kSpawnTag, "Spawn Bad Sun",
             "the host's fresh Bad Sun sends nothing, and a client's own Spawn Bad Sun runs");
    SayWatch(g_saidRemoveWatch, kRowRemove, kRemoveTag, "remove",
             "client copies still end by their clocks; the host's removal sends nothing");
}

bool LocalBadSunPresent() {
    bool present = false;
    return GT::IsGameThread() && BS::ReadGamemodeCopy(nullptr, present) && present;
}

void Apply(const coop::net::BadSunPayload& payload) {
    if (!GT::IsGameThread()) {
        UE_LOGW("weather: the host's Bad Sun applied off the game thread -- dropping");
        return;
    }
    // From the host alone: the host's receive edge refuses a client's copy (client_authorship.h).
    const bool want = payload.state != 0;
    if (!ue_wrap::world_singleton::Gamemode()) {
        UE_LOGI("weather: the host's Bad Sun %d arrived before this client's gamemode -- its world-ready seed "
                "carries it", want ? 1 : 0);
        return;
    }
    void* const world = ue_wrap::world_identity::CurrentWorld();
    const std::wstring worldName = world ? R::ToString(R::NameOf(world)) : std::wstring(L"?");
    if (!BS::HasCycle()) {
        UE_LOGI("weather: the host's Bad Sun %d arrived in world %ls, which has no day cycle here; the re-seed at "
                "the return carries it", want ? 1 : 0, worldName.c_str());
        return;
    }
    const size_t backlog = ue_wrap::object_index::Backlog();
    void* copies[BS::kMaxLive] = {};
    int n = BS::CountLive(copies, BS::kMaxLive);
    if (n < 0) {
        FailedApply("the object index could not drain", want);
        return;
    }
    UE_LOGI("weather: apply Bad Sun %d in world %ls -- index backlog %zu, %d live cop%s", want ? 1 : 0,
            worldName.c_str(), backlog, n, n == 1 ? "y" : "ies");
    if (want) {
        if (n == 0) {
            bool ran = false;
            {
                Applying scope;
                ran = BS::CallSpawn();
            }
            n = BS::CountLive(copies, BS::kMaxLive);
            if (ran && n == 1) {
                ++g_spawned;
                UE_LOGI("weather: applied the host's Bad Sun 1 -- spawned one copy (%s)", StateOf(copies[0]));
            } else {
                char why[96];
                std::snprintf(why, sizeof(why), "Spawn Bad Sun ran=%d, then %d live cop%s", ran ? 1 : 0, n,
                              n == 1 ? "y" : "ies");
                FailedApply(why, want);
            }
        } else if (n >= 2) {
            for (int i = 0; i < n && i < BS::kMaxLive; ++i)
                UE_LOGE("weather: %d Bad Sun copies live on this peer, the host has one -- copy %d at %p is %s; "
                        "nothing repaired", n, i, copies[i], StateOf(copies[i]));
        }
        return;
    }
    for (int i = 0; i < n && i < BS::kMaxLive; ++i) {
        bool active = false;
        if (!BS::ReadActive(copies[i], active)) {
            FailedApply("a copy's widget did not read", want);
        } else if (active) {
            ++g_leftActive;
            UE_LOGI("weather: the host's Bad Sun 0 -- copy %d at %p is active, left to end by its own clock", i,
                    copies[i]);
        } else if (BS::DestroyInactive(copies[i])) {
            ++g_destroyed;
            UE_LOGI("weather: the host's Bad Sun 0 -- destroyed the inactive copy %d at %p", i, copies[i]);
        } else {
            FailedApply("an inactive copy was not destroyed", want);
        }
    }
}

void OnDisconnect() {
    if (g_refused || g_sent || g_spawned || g_destroyed || g_leftActive || g_applyFailed)
        UE_LOGI("weather: Bad Sun session end -- %llu of this client's own spawns refused, %llu sent, %llu spawned, "
                "%llu inactive copies destroyed, %llu active copies left to end, %llu did not apply",
                static_cast<unsigned long long>(g_refused), static_cast<unsigned long long>(g_sent),
                static_cast<unsigned long long>(g_spawned), static_cast<unsigned long long>(g_destroyed),
                static_cast<unsigned long long>(g_leftActive), static_cast<unsigned long long>(g_applyFailed));
    g_refused = g_sent = g_spawned = g_destroyed = g_leftActive = g_applyFailed = 0;
    g_saidRefused = false;
    g_applying = false;
    g_session.store(nullptr, std::memory_order_release);
}

}  // namespace coop::weather_badsun
