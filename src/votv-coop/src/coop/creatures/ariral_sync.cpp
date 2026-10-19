#include "coop/creatures/ariral_sync.h"
#include "coop/net/session.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/reflection.h"

namespace coop::ariral_sync {
namespace {
namespace SG = ue_wrap::script_gate;
std::atomic<net::Session*> g_session{nullptr};

bool IsAriral(void* actor) {
    namespace R = ue_wrap::reflection;
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = R::ClassOf(actor);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls)) {
        if (R::NameEquals(R::NameOf(cls), L"npc_arirGunStealer_C")) return true;
    }
    return false;
}

SG::Verdict OnSteal(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() ||
        session->role() != net::Role::Client || !IsAriral(call.object))
        return SG::Verdict::Run;

    // Block native gun stealing on clients to avoid duplicated item spawning/destruction.
    return SG::Verdict::Cancel;
}
}

void Install(net::Session* session) {
    g_session.store(session, std::memory_order_release);
    
    // Watch for functions related to gun stealing. 
    // Note: Exact function names will be verified via logs/research.
    SG::WatchClassName(L"npc_arirGunStealer_C", L"StealGun", 0, &OnSteal, nullptr);
    SG::WatchClassName(L"npc_arirGunStealer_C", L"TakeWeapon", 0, &OnSteal, nullptr);
}
}
