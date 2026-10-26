// coop/interactables/lightgroup_verbs.cpp -- see coop/interactables/lightgroup_verbs.h.

#include "coop/interactables/lightgroup_verbs.h"

#include "coop/interactables/interactable_sync.h"  // the group lane's key, its apply and the host's edge
#include "coop/net/session.h"
#include "coop/session/net_pump.h"  // IsInAnnouncedWorld: a client's own world load runs natively

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/lightswitch.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_set>

namespace coop::lightgroup_verbs {
namespace {

namespace LS = ue_wrap::lightswitch;
namespace sg = ue_wrap::script_gate;

// Every trigger class declares runTrigger, so the callbacks see a door's, a garage's and an
// eventer's too; each gates on the light root class first.
UE_FACT_FN(kRowRunTrigger, "lightgroup_verbs", nullptr, L"runTrigger", AnyClass);
constexpr int kTag = 0x4C475254;  // 'LGRT'

std::atomic<coop::net::Session*> g_session{nullptr};
// A refused registration may precede gate installation. Retry with a bounded
// cadence instead of disabling light-group authority for the rest of the process.
enum class Reg : uint8_t { Pending, Registered, Refused };
Reg  g_reg = Reg::Pending;
bool g_settled = false;
bool g_lampWatched = false;
bool g_ambientWatched = false;
std::chrono::steady_clock::time_point g_nextWatchTry{};
bool g_saidWatchFailure = false;
constexpr int kLampTag = 0x4C414D50;
constexpr int kAmbientTag = 0x414D424C;
coop::net::Session* ConnectedAs(coop::net::Role role);

sg::Verdict OnLampPre(const sg::Call& call) {
    if (call.fromOurCode || !ConnectedAs(coop::net::Role::Client)) return sg::Verdict::Run;
    if (!call.object || !LS::IsCeilingLamp(call.object) ||
        !coop::net_pump::IsInAnnouncedWorld(call.object) ||
        coop::interactable_sync::CeilingLampKey(call.object).empty()) return sg::Verdict::Run;
    // Native flick() and its delayed continuations bypass runTrigger and write lamps
    // directly. Only the host's lamp/group replay may repaint an indexed client lamp.
    return sg::Verdict::Cancel;
}
void OnLampPost(const sg::Call& call) {
    if (ConnectedAs(coop::net::Role::Host) && call.object && LS::IsCeilingLamp(call.object))
        coop::interactable_sync::OnCeilingLampVerb(call.object);
}

sg::Verdict OnAmbientPre(const sg::Call& call) {
    if (call.fromOurCode || !ConnectedAs(coop::net::Role::Client)) return sg::Verdict::Run;
    if (!call.object || !LS::IsAmbientLight(call.object) ||
        !coop::net_pump::IsInAnnouncedWorld(call.object) ||
        coop::interactable_sync::AmbientLightKey(call.object).empty()) return sg::Verdict::Run;
    return sg::Verdict::Cancel;
}
void OnAmbientPost(const sg::Call& call) {
    if (ConnectedAs(coop::net::Role::Host) && call.object && LS::IsAmbientLight(call.object))
        coop::interactable_sync::OnAmbientLightVerb(call.object);
}

uint64_t g_refused = 0, g_edges = 0;
uint64_t g_loadNative = 0;  // a client's calls run natively as its own world's load (IsInAnnouncedWorld)
// A group's first refusal is said; the rest are counted. The set holds each group this client's own
// world tried to move, once.
std::unordered_set<std::wstring> g_saidRefused;

coop::net::Session* ConnectedAs(coop::net::Role role) {
    auto* s = g_session.load(std::memory_order_acquire);
    return (s && s->running() && s->connected() && s->role() == role) ? s : nullptr;
}

// CLIENT, before the body.
sg::Verdict OnTriggerPre(const sg::Call& call) {
    if (!ConnectedAs(coop::net::Role::Client)) return sg::Verdict::Run;
    if (!call.object || !LS::IsLightRoot(call.object)) return sg::Verdict::Run;
    // A call on an object of a world this client has not announced ready is that world's load, the
    // host's save: it runs natively, and the host's own states for what the lane owns follow the
    // announce.
    if (!coop::net_pump::IsInAnnouncedWorld(call.object)) {
        ++g_loadNative;
        return sg::Verdict::Run;
    }
    if (coop::interactable_sync::ApplyingLightGroup(call.object)) return sg::Verdict::Run;  // the lane's own apply
    const std::wstring key = coop::interactable_sync::LightGroupKey(call.object);
    if (key.empty()) return sg::Verdict::Run;  // no lane owns it: native
    ++g_refused;
    if (g_saidRefused.insert(key).second)
        UE_LOGI("[LIGHT-GROUP] client refused its own runTrigger on key='%ls' -- the host's copy moves this "
                "group (first refusal on this key; the rest are counted)", key.c_str());
    return sg::Verdict::Cancel;
}

// HOST, after the body.
void OnTriggerPost(const sg::Call& call) {
    if (!ConnectedAs(coop::net::Role::Host)) return;
    if (!call.object || !LS::IsLightRoot(call.object)) return;
    ++g_edges;
    coop::interactable_sync::OnLightGroupVerb(call.object);
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (!g_ambientWatched)
        g_ambientWatched = sg::WatchClassName(L"ambientLight_C", L"setActive", kAmbientTag,
                                             &OnAmbientPre, &OnAmbientPost);
    if (!g_lampWatched)
        g_lampWatched = sg::WatchClassName(L"ceilingLamp_C", L"setActive", kLampTag,
                                          &OnLampPre, &OnLampPost);
    if (g_reg == Reg::Registered) return;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextWatchTry) return;
    g_nextWatchTry = now + std::chrono::seconds(1);
    if (sg::WatchName(kRowRunTrigger, kTag, &OnTriggerPre, &OnTriggerPost)) {
        g_reg = Reg::Registered;
        return;
    }
    g_reg = Reg::Refused;
    if (!g_saidWatchFailure) {
        g_saidWatchFailure = true;
        UE_LOGE("[LIGHT-GROUP] the script-body gate refused runTrigger -- retrying once a second");
    }
}

void Tick() {
    sg::ResolvePendingNames();
    if (g_settled || g_reg != Reg::Registered) return;
    if (g_reg == Reg::Registered && sg::NameWatchLive(kRowRunTrigger, kTag)) {
        g_settled = true;
        UE_LOGI("[LIGHT-GROUP] the light group gate is live: runTrigger");
    } else if (sg::PendingNameCount() == 0) {
        g_settled = true;
        UE_LOGE("[LIGHT-GROUP] the light group gate is dead (refused, or resolved into a full table) -- the group "
                "lane cannot see runTrigger");
    }
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    if (g_refused || g_edges || g_loadNative)
        UE_LOGI("[LIGHT-GROUP] session end -- a client refused %llu local runTrigger call(s) on %zu group(s) "
                "and ran %llu of its own world's load; the host saw %llu",
                static_cast<unsigned long long>(g_refused), g_saidRefused.size(),
                static_cast<unsigned long long>(g_loadNative), static_cast<unsigned long long>(g_edges));
    g_refused = g_edges = g_loadNative = 0;
    g_saidRefused.clear();
}

}  // namespace coop::lightgroup_verbs
