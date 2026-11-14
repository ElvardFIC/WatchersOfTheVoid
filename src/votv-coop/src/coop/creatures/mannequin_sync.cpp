#include "coop/creatures/mannequin_sync.h"
#include "coop/net/session.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/reflection.h"

namespace coop::mannequin_sync {
namespace {
namespace SG = ue_wrap::script_gate;
std::atomic<net::Session*> g_session{nullptr};

bool IsMannequin(void* actor) {
    namespace R = ue_wrap::reflection;
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = R::ClassOf(actor);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls)) {
        if (R::NameEquals(R::NameOf(cls), L"prop_mannequin_C") ||
            R::NameEquals(R::NameOf(cls), L"prop_wMannequin_C")) return true;
    }
    return false;
}

SG::Verdict OnDestroy(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() ||
        session->role() != net::Role::Client || !IsMannequin(call.object))
        return SG::Verdict::Run;

    // Prevent clients from destroying mannequins independently; host manages lifecycle.
    return SG::Verdict::Cancel;
}
}

void Install(net::Session* session) {
    g_session.store(session, std::memory_order_release);

    // Block native destruction on clients for both mannequin types. Their classes declare no script
    // body for this verb, so the gate settles these watches dead; a per-tick re-register would take a
    // fresh dead slot each time and fill the name table. Register once: settled is final.
    static bool s_watchDone = false;
    if (s_watchDone) return;
    const bool plain = SG::WatchClassName(L"prop_mannequin_C", L"K2_DestroyActor", 0, &OnDestroy, nullptr);
    const bool worn  = SG::WatchClassName(L"prop_wMannequin_C", L"K2_DestroyActor", 0, &OnDestroy, nullptr);
    s_watchDone = (plain || SG::ClassNameWatchSettled(L"prop_mannequin_C", L"K2_DestroyActor", 0)) &&
                  (worn || SG::ClassNameWatchSettled(L"prop_wMannequin_C", L"K2_DestroyActor", 0));
}
}
