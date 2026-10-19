#include "coop/creatures/grayboar_sync.h"
#include "coop/net/session.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_drive_stream.h"
#include "coop/props/prop_echo_suppress.h"
#include "coop/player/player_damage.h"
#include "coop/player/players_registry.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include <atomic>
#include <cmath>
#include <cstring>

namespace coop::grayboar_sync {
namespace {
namespace SG = ue_wrap::script_gate;
std::atomic<coop::net::Session*> g_session{nullptr};
std::atomic<bool> g_watched{false}, g_damageWatched{false};
constexpr int kBodyTickTag = 0x474254;
constexpr int kDamageTag = 0x474244;

template<class T> bool ReadArg(const SG::Call& call, const wchar_t* name, T& value) {
    if (!call.function || !call.locals) return false;
    const int off = ue_wrap::reflection::FindParamOffset(call.function, name);
    const int size = ue_wrap::reflection::FunctionFrameSize(call.function);
    if (off < 0 || off > size || sizeof(T) > static_cast<size_t>(size - off)) return false;
    std::memcpy(&value, call.locals + off, sizeof(value));
    return true;
}

bool IsBoar(void* actor) {
    namespace R = ue_wrap::reflection;
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = R::ClassOf(actor);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls)) {
        if (R::NameEquals(R::NameOf(cls), L"grayboar_C")) return true;
    }
    return false;
}

SG::Verdict OnPlayerDamage(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() || !call.object)
        return SG::Verdict::Run;
    void* source = nullptr;
    if (!ReadArg(call, L"source", source) || !IsBoar(source)) return SG::Verdict::Run;
    auto& players = coop::players::Registry::Get();
    if (session->role() == coop::net::Role::Client) {
        if (call.object != players.Local()) return SG::Verdict::Cancel;
        // Wire damage is applied with no source actor and does not enter this
        // refusal. A personal/unbound boar retains its native local encounter.
        return coop::prop_echo_suppress::IsMirrorSpawn(source) ||
               coop::prop_drive_stream::IsParked(source) ? SG::Verdict::Cancel : SG::Verdict::Run;
    }
    if (call.object == players.Local()) return SG::Verdict::Run;
    const auto slot = players.PeerIdOfActor(call.object);
    float damage = 0.f;
    if (slot > 0 && slot < coop::players::kMaxPeers &&
        ReadArg(call, L"damage", damage) && std::isfinite(damage) && damage > 0.f && damage <= 1000.f)
        coop::player_damage::SendPlayerDamage(slot, damage);
    // A foreign body (including a preview/unregistered puppet) must never
    // write this machine's saveSlot.health, even if no relay can be addressed.
    return SG::Verdict::Cancel;
}

SG::Verdict OnBodyTick(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || session->role() != coop::net::Role::Host ||
        !call.object) return SG::Verdict::Run;
    // Init/record authoring may finish after the first tick. Retry naturally at
    // the next tick, without assigning a second identity or claiming a mirror.
    const auto eid = coop::prop_element_tracker::GetPropElementIdForActor(call.object);
    if (eid == 0 || eid >= coop::element::kHostRangeSize) return SG::Verdict::Run;
    // ReceiveTick applies force/torque to the prop's StaticMesh. Streaming the
    // helper pawn would not reproduce that movement. Claim is idempotent and
    // already handles held-item arbitration, join repair and dead-body cleanup.
    coop::prop_drive_host::Claim(call.object, "grayboar body");
    return SG::Verdict::Run;
}
}

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    // Class-scoped body watch also sees subclasses inheriting this function,
    // including the overcharged variant. Other props' ticks never enter here.
    if (!g_watched.load(std::memory_order_acquire) &&
        SG::WatchClassName(L"grayboar_C", L"ReceiveTick", kBodyTickTag, &OnBodyTick, nullptr))
        g_watched.store(true, std::memory_order_release);
    else if (!g_watched.load(std::memory_order_acquire))
        UE_LOGW("grayboar: body movement watch unavailable");
    if (!g_damageWatched.load(std::memory_order_acquire) &&
        SG::WatchClassName(L"mainPlayer_C", L"Add Player Damage", kDamageTag, &OnPlayerDamage, nullptr))
        g_damageWatched.store(true, std::memory_order_release);
    else if (!g_damageWatched.load(std::memory_order_acquire))
        UE_LOGW("grayboar: player damage watch unavailable");
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    // prop_drive_host owns and clears the claims at the session disconnect edge.
}
}
