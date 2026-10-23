// coop/dev/atv_seat.cpp -- see coop/dev/atv_seat.h.

#include "atv_seat.h"

#include "coop/interactables/atv_sync.h"
#include "coop/net/net_clock.h"
#include "coop/player/players_registry.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/devices/atv.h"

#include <cstdint>
#include <string>

namespace coop::dev::atv_seat {
namespace {

namespace R = ue_wrap::reflection;

void* g_actionNameFn = nullptr;  // ATV_C::actionName(player, hit, name)

// Which of the seat verb's three gates refused. Two are cheap and side-effect free (the fall
// velocity is a field read; checkEquip is a pure read of the held item), so the arm measures
// them; the third (the seat overlap) is left to elimination, with the occupant pointer as the
// nearest proxy.
void DiagnoseSeatRefusal(void* local, void* atv) {
    void* pcls = R::ClassOf(local);
    float fallZ = 0.f;
    bool haveFall = false;
    if (pcls) {
        const int32_t off = R::FindPropertyOffset(pcls, L"fallVeloc");
        if (off >= 0) {
            const ue_wrap::FVector v =
                *reinterpret_cast<ue_wrap::FVector*>(reinterpret_cast<uint8_t*>(local) + off);
            fallZ = v.Z;
            haveFall = true;
        }
    }

    int emptyHands = -1;              // -1 = could not ask
    std::wstring holdingClass = L"?";
    if (pcls) {
        if (void* fn = R::FindFunction(pcls, L"checkEquip")) {
            ue_wrap::ParamFrame pf(fn);
            if (pf.valid() && ue_wrap::Call(local, pf))
                emptyHands = pf.Get<bool>(L"empty") ? 1 : 0;
        }
        const int32_t hoff = R::FindPropertyOffset(pcls, L"holding_actor");
        if (hoff >= 0) {
            void* held = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(local) + hoff);
            holdingClass = held ? R::ClassNameOf(held) : std::wstring(L"<none>");
        }
    }

    UE_LOGW("[ATVP] ARM sit gates: fallVeloc.Z=%s (need |Z|<800), checkEquip.empty=%s "
            "(need 1), holding_actor=%ls, atv.occupant=%p (need null)",
            haveFall ? std::to_string(fallZ).c_str() : "?",
            emptyHands < 0 ? "?" : (emptyHands ? "1" : "0"),
            holdingClass.c_str(), ue_wrap::atv::GetOccupantPlayer(atv));
}

}  // namespace

bool TrySit(void* player, void* atv, SitTry& t) {
    if (coop::atv_sync::ActorSeatedBy(coop::players::Registry::Get().LocalPeerId()) == atv) return true;
    const uint64_t now = coop::net::NowMs();
    if (now < t.nextMs) return false;
    if (t.tries >= kSitRetries) return true;   // the last try's gap has passed unseated: given up
    if (!g_actionNameFn) {
        if (void* cls = R::FindClass(L"ATV_C")) g_actionNameFn = R::FindFunction(cls, L"actionName");
        if (!g_actionNameFn) {
            UE_LOGW("[ATVP] ARM sit: actionName unresolved -- arm cannot fire");
            return true;
        }
    }

    // The verb dispatches on a string compare, so the frame carries a real FString aliasing this
    // local for the synchronous call; the callee only compares it.
    std::wstring verb = L"sit";
    R::FString fs{ verb.data(),
                   static_cast<int32_t>(verb.size()) + 1,
                   static_cast<int32_t>(verb.size()) + 1 };

    ue_wrap::ParamFrame pf(g_actionNameFn);
    if (!pf.valid()) { UE_LOGE("[ATVP] ARM sit: ParamFrame(actionName) invalid"); return true; }
    pf.Set<void*>(L"player", player);          // `hit` stays the frame's zeroed HitResult
    pf.Set<R::FString>(L"name", fs);
    const bool ok = ue_wrap::Call(atv, pf);
    ++t.tries;
    t.nextMs = now + kSitRetryMs;

    const bool driven = ue_wrap::atv::IsDriven(atv);
    UE_LOGI("[ATVP] ARM sit: actionName(local=%p, 'sit') on atv=%p -> %s, try %d/%d, driven=%d",
            player, atv, ok ? "called" : "CALL FAILED", t.tries, kSitRetries, driven ? 1 : 0);
    if (driven) return false;   // the verb took the seat; the ATV lane claims it on its next tick
    DiagnoseSeatRefusal(player, atv);
    if (t.tries >= kSitRetries) {
        // The three gates are named in the log; the seat overlap cannot be read without a second
        // write into the player.
        UE_LOGW("[ATVP] ARM sit: REFUSED %d times -- the seat verb is gated on "
                "|fallVeloc.Z|<800, empty hands (checkEquip), and an unoccupied playerHit "
                "[disasm @46420/@46522/@46645]; the seat was not taken",
                t.tries);
        return true;
    }
    return false;
}

}  // namespace coop::dev::atv_seat
