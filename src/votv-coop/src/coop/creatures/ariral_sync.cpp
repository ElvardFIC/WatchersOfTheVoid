#include "coop/creatures/ariral_sync.h"
#include "coop/element/mirror_managers.h"
#include "coop/element/npc.h"
#include "coop/net/session.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/reflection.h"

#include <vector>

namespace coop::ariral_sync {
namespace {
namespace SG = ue_wrap::script_gate;
std::atomic<net::Session*> g_session{nullptr};

bool IsGunStealer(void* actor) {
    namespace R = ue_wrap::reflection;
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = R::ClassOf(actor);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls)) {
        if (R::NameEquals(R::NameOf(cls), L"npc_arirGunStealer_C")) return true;
    }
    return false;
}

bool IsHostMirror(void* actor) {
    std::vector<coop::element::Npc*> mirrors;
    coop::element::NpcMirrors().Snapshot(mirrors);
    for (auto* entity : mirrors)
        if (entity && entity->IsMirror() && entity->LiveActor() == actor) return true;
    return false;
}

SG::Verdict OnPunching(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() ||
        session->role() != net::Role::Client || !IsGunStealer(call.object) ||
        !IsHostMirror(call.object))
        return SG::Verdict::Run;

    // ReceiveBeginPlay arms this 0.5 s timer before npc_mirror parks actor and movement ticks.
    // Its native body punches the local player, damages/pushes props and opens/hacks doors.
    // The host owns those outcomes; a pose mirror must not run them against its local world.
    return SG::Verdict::Cancel;
}
}

void Install(net::Session* session) {
    g_session.store(session, std::memory_order_release);

    // Native 0.9.0n exports `punching`; the previously guessed StealGun/TakeWeapon names do
    // not exist. Gun acquisition/attachment is latent ubergraph state and needs a separate
    // host state transport before it can be claimed as mirrored.
    SG::WatchClassName(L"npc_arirGunStealer_C", L"punching", 0, &OnPunching, nullptr);
}
}
