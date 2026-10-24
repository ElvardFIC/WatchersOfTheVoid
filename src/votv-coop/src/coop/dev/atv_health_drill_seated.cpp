// coop/dev/atv_health_drill_seated.cpp -- the client half the ATV health drill's walk-and-sit arms share: the idle
// head, the wait for the ATV, the director's walk to it and the seating. The arms keep their own phases and ends
// (atv_health_drill_arms.cpp, atv_health_drill_damage.cpp); a step here prints the FAIL line and the arm ends its run.

#include "atv_health_drill_detail.h"  // co-located private header (src tree, not include/)

#include "coop/dev/director/director.h"
#include "coop/dev/drill_chat.h"
#include "coop/interactables/atv_sync.h"
#include "coop/player/players_registry.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/devices/atv.h"

#include <string>

namespace coop::dev::atv_health_drill::detail {

bool SeatedActs(SeatedClient& c, void** player, uint8_t* self) {
    if (c.run.done) return false;
    *self = coop::players::Registry::Get().LocalPeerId();
    *player = coop::players::Registry::Get().Local();
    if (!coop::net_pump::HasAnnouncedWorldReady() || !*player || *self == 0 || *self >= coop::players::kMaxPeers)
        return false;
    if (*self != kSlot) {
        if (!c.idleSaid) {
            c.idleSaid = true;
            UE_LOGI("[ATV-HEALTH-DRILL] client idles: this client is slot %u, the drill acts on slot %u",
                    static_cast<unsigned>(*self), static_cast<unsigned>(kSlot));
        }
        return false;
    }
    return true;
}

bool AtvReady(AtvAt* out) {
    coop::atv_sync::TableView t;
    ue_wrap::FRotator rot{};
    if (!StreamsRun() || !coop::atv_sync::ReadTable(kKey, t) || !ue_wrap::atv::GetRootTransform(t.actor, out->at, rot))
        return false;
    out->actor = t.actor;
    return true;
}

void WalkStart(SeatedClient& c, Arm a, const AtvAt& t, float reachCm) {
    c.atv.Set(t.actor);
    c.walk = coop::director::StartBackgroundWalk(t.at, reachCm, kWalkDeadlineS);
    c.walkAt = Clock::now();
    coop::dev::drill_chat::Say(ReadyLine(a));
    UE_LOGI("[ATV-HEALTH-DRILL] client arm=%s the ATV is at (%.0f,%.0f,%.0f), walking to it", SpecOf(a).name, t.at.X,
            t.at.Y, t.at.Z);
}

Stage WaitBound(SeatedClient& c, Arm a) {
    if (!Past(c.run, kFailBound)) return Stage::Wait;
    Fail("client", a, c.run, "the client's streams or the ATV's table never held", "not ready",
         "streams running, ATV indexed");
    return Stage::Failed;
}

Stage WalkStep(SeatedClient& c, Arm a) {
    if (!c.atv.Get()) {
        Fail("client", a, c.run, "the ATV is gone before the walk ended", "no ATV", "the world's ATV");
        return Stage::Failed;
    }
    const int state = c.walk->state.load();
    if (state == 2) {
        Fail("client", a, c.run, "the director's walk to the ATV did not arrive", "walk failed", "arrived");
        return Stage::Failed;
    }
    if (state != 1) return Stage::Wait;
    c.arrivedAt = Clock::now();
    return Stage::Next;
}

Stage SitStep(SeatedClient& c, Arm a, void* player, void* atv, uint8_t self) {
    if (!atv_seat::TrySit(player, atv, c.sit)) {
        if (!Past(c.run, kFailBound)) return Stage::Wait;
        Fail("client", a, c.run, "the sit never finished", "no grant", "seated");
        return Stage::Failed;
    }
    if (coop::atv_sync::ActorSeatedBy(self) != atv) {
        Fail("client", a, c.run, "the sit did not take (see the [ATVP] sit lines)", "not seated", "seated");
        return Stage::Failed;
    }
    return Stage::Next;
}

void WalkCancel(SeatedClient& c) {
    if (c.walk && c.walk->state.load() == 0) coop::director::CancelBackgroundWalk(*c.walk);
    c.walk.reset();
}

}  // namespace coop::dev::atv_health_drill::detail
