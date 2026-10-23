// coop/world/salt_rocket_ignite_host.cpp -- see coop/world/salt_rocket_ignite.h: the host half. The receive edge,
// the per-sender queues and the judge.

#include "coop/world/salt_rocket_ignite.h"

#include "salt_rocket_ignite_detail.h"  // co-located private header (src tree, not include/)

#include "coop/element/intent_authority.h"
#include "coop/element/registry.h"
#include "coop/net/net_clock.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_history.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_key_index.h"

#include "ue_wrap/actors/prop_fire.h"
#include "ue_wrap/core/log.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace coop::salt_rocket_ignite {
namespace {

namespace EL = coop::element;

coop::roster_ledger::PerSlotState<detail::Slot> g_slots;

// A sender still in its load window has no puppet and no hand state to judge against; MTA drops a packet from a
// player that has not finished joining (reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:2835).
bool SenderWorldReady(uint8_t slot) {
    const coop::net::Session* s = detail::SessionPtr();
    return s && s->IsSlotWorldReady(slot);
}

// The form of the message: a known verb, the pad zero, a key that fits, and for a keyless rocket the host's eid (a
// client only ever names a host rocket).
bool WellFormed(const coop::net::PropIgniteIntentPayload& p) {
    if (p.verb > 1) return false;
    if (p.pad[0] != 0 || p.pad[1] != 0 || p.pad[2] != 0) return false;
    if (p.key.len > sizeof(p.key.data)) return false;
    if (p.key.len == 0 && !EL::Registry::IsAllowedHostAllocatedEid(p.elementId)) return false;
    return true;
}

// The rocket the intent names: a key resolves by the index and never falls back to the eid, which may name another
// prop by now; only a keyless rocket is named by its eid.
void* ResolveRocket(const coop::net::PropIgniteIntentPayload& p) {
    if (p.key.len > 0) return coop::prop_key_index::Find(coop::net::StringFromWireKey(p.key));
    return EL::LivePropActor(static_cast<EL::ElementId>(p.elementId));
}

// Whether the sender's puppet exists and lives: the test the explosion request judge refuses a self blast by
// (NoPuppet, PuppetDead), the health the puppet's pose stream last carried above 0. MTA refuses a sender that is not
// spawned (reference/mtasa-blue/Server/mods/deathmatch/logic/CGame.cpp:3096-3100).
bool SenderAlive(uint8_t slot) {
    coop::RemotePlayer* rp = coop::players::Registry::Get().Puppet(slot);
    return rp && rp->valid() && rp->GetActor() && rp->GetHealth() > 0.0f;
}

coop::net::Served Refused(uint8_t slot, Refusal r) {
    detail::Refuse(slot, r);
    return coop::net::Served::Done;
}

// One intent at its sender's queue head: the rocket, that the sender lives, its reach, the tool its hand held, then
// the verb run on the host's copy. A hand state still owed holds the head, as the explosion lane's judge holds its
// launcher check. The shape is MTA's request, check, perform of Packet_Vehicle_InOut (see salt_rocket_ignite.h).
coop::net::Served Judge(coop::net::Session& s, uint8_t slot, const detail::Item& it) {
    const coop::net::PropIgniteIntentPayload& p = it.p;
    void* rocket = ResolveRocket(p);
    if (!rocket) return Refused(slot, Refusal::Gone);
    if (!detail::IsRocket(rocket)) return Refused(slot, Refusal::NotRocket);
    if (!SenderAlive(slot)) return Refused(slot, Refusal::SenderDead);
    // A puppet that exists but has no pose yet (NoBody) is a Reach refusal: its place cannot be measured.
    if (!EL::IntentTarget::ForClientIntent(s, slot, detail::kReachUU).Authorize(rocket)) return Refused(slot, Refusal::Reach);
    const bool lighter = coop::hand_history::HeldWithin(slot, L"prop_lighter_C", detail::kToolWindowMs, it.recvMs);
    const bool torch = p.verb == 1 &&
                       coop::hand_history::HeldWithin(slot, L"prop_torch_C", detail::kToolWindowMs, it.recvMs);
    if (!lighter && !torch) {
        if (coop::hand_item::HandStateOwed(slot)) {
            ++detail::MutCounters().waited;
            return coop::net::Served::Wait;
        }
        return Refused(slot, Refusal::NoTool);
    }
    // The game's own fuel for a lighter's second call. Only a call that ran counts as performed.
    const bool ran = p.verb == 0 ? ue_wrap::prop::CallAttemptIgnite(rocket) : ue_wrap::prop::CallIgnite(rocket, 10.f);
    if (!ran) {
        if (!detail::NotRunSaid()) {
            detail::NotRunSaid() = true;
            UE_LOGW("[SALT] the host could not run verb %u on a rocket for slot %u -- the light is dropped",
                    static_cast<unsigned>(p.verb), static_cast<unsigned>(slot));
        }
        return coop::net::Served::Done;
    }
    ++detail::MutCounters().performed[p.verb];
    return coop::net::Served::Done;
}

}  // namespace

namespace detail {

void Refuse(uint8_t slot, Refusal r) {
    ++MutCounters().refused[static_cast<int>(r)];
    Slot& sl = g_slots[slot];
    const uint64_t now = coop::net::NowMs();
    if (now < sl.nextSayMs) return;
    sl.nextSayMs = now + kRefusalSayMs;
    UE_LOGI("[SALT] ignite intent from slot %u refused: %s", static_cast<unsigned>(slot), RefusalName(r));
}

void HostServe(coop::net::Session& s) {
    for (uint8_t slot = 1; slot < coop::net::kMaxPeers; ++slot)
        g_slots[slot].queue.Serve(kBudget, coop::net::NowMs, [&](const Item& it) { return Judge(s, slot, it); });
}

void HostReset() {
    for (int i = 0; i < g_slots.size(); ++i) g_slots[i] = Slot{};
}

}  // namespace detail

void OnIntentReceived(int senderSlot, const uint8_t* bytes, uint16_t len) {
    if (senderSlot < 1 || senderSlot >= coop::net::kMaxPeers) return;  // the dispatch's own guard, kept at this edge
    const auto slot = static_cast<uint8_t>(senderSlot);
    coop::net::PropIgniteIntentPayload p{};
    if (!SenderWorldReady(slot)) return detail::Refuse(slot, Refusal::NotWorldReady);
    if (!bytes || len != sizeof(p)) return detail::Refuse(slot, Refusal::Malformed);
    std::memcpy(&p, bytes, sizeof(p));
    if (!WellFormed(p)) return detail::Refuse(slot, Refusal::Malformed);
    if (!g_slots[slot].queue.Push(detail::Item{p, coop::net::NowMs()}, detail::kMaxPending))
        detail::Refuse(slot, Refusal::QueueFull);
}

}  // namespace coop::salt_rocket_ignite
