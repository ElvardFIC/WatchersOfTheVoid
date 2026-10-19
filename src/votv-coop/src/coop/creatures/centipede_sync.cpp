#include "coop/creatures/centipede_sync.h"
#include "coop/net/session.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/reflection.h"

namespace coop::centipede_sync {
namespace {
namespace SG = ue_wrap::script_gate;
std::atomic<net::Session*> g_session{nullptr};

bool IsCentipede(void* actor) {
    namespace R = ue_wrap::reflection;
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = R::ClassOf(actor);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls)) {
        if (R::NameEquals(R::NameOf(cls), L"centipedeHead_C") ||
            R::NameEquals(R::NameOf(cls), L"centipedeSegmet_C")) return true;
    }
    return false;
}

SG::Verdict OnDestroy(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() ||
        session->role() != net::Role::Client || !IsCentipede(call.object))
        return SG::Verdict::Run;

    // Block native destruction on clients; host manages the centipede lifecycle.
    return SG::Verdict::Cancel;
}
}

void Install(net::Session* session) {
    g_session.store(session, std::memory_order_release);

    // Block native destruction for both head and segments.
    SG::WatchClassName(L"centipedeHead_C", L"K2_DestroyActor", 0, &OnDestroy, nullptr);
    SG::WatchClassName(L"centipedeSegmet_C", L"K2_DestroyActor", 0, &OnDestroy, nullptr);
}
}
