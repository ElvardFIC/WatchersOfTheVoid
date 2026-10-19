#include "coop/creatures/deer_sync.h"
#include "coop/net/session.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/reflection.h"

namespace coop::deer_sync {
namespace {
namespace SG = ue_wrap::script_gate;
std::atomic<net::Session*> g_session{nullptr};

bool IsDeer(void* actor) {
    namespace R = ue_wrap::reflection;
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = R::ClassOf(actor);
    for (int depth = 0; cls && depth < 64; ++depth, cls = R::SuperStructOf(cls)) {
        if (R::NameEquals(R::NameOf(cls), L"deer_C")) return true;
    }
    return false;
}

SG::Verdict OnDestroy(const SG::Call& call) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->running() || !session->connected() ||
        session->role() != net::Role::Client || !IsDeer(call.object))
        return SG::Verdict::Run;

    // Prevent clients from destroying deer independently; host manages lifecycle.
    return SG::Verdict::Cancel;
}
}

void Install(net::Session* session) {
    g_session.store(session, std::memory_order_release);
    
    // Block native destruction on clients.
    SG::WatchClassName(L"deer_C", L"K2_DestroyActor", 0, &OnDestroy, nullptr);
}
}
