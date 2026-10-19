#include "coop/props/prop_support_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"
#include "coop/player/local_prop_hold.h"
#include "coop/player/hand_item.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/remote_prop.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/player_support.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/world_identity.h"

namespace coop::prop_support_sync {
namespace {
struct Support { ue_wrap::CachedObjRef actor; uint64_t until = 0; };
Support g_peers[net::kMaxPeers];
uint64_t g_next = 0;
uint32_t g_world = 0;
bool g_announced = false;
bool Eligible(void* a) {
    namespace P = ue_wrap::prop;
    return a && P::IsDescendantOfProp(a) && !P::IsStatic(a) && !P::IsFrozen(a) &&
        !P::IsGarbageClump(a) && !P::IsTrashActor(a) && !P::IsTrashBitsPile(a) &&
        !(ue_wrap::atv::EnsureResolved() && ue_wrap::atv::IsAtv(a)) &&
        !local_prop_hold::IsHeld(a) && !hand_item::IsHandAxisActor(a) &&
        !remote_prop::IsActorUnderAnyDrive(a) &&
        ue_wrap::world_identity::WorldOf(a) == ue_wrap::world_identity::CurrentWorld();
}
void ResetWorld() {
    const auto gen = ue_wrap::world_identity::Generation();
    if (gen == g_world) return;
    g_world = gen; g_next = 0; g_announced = false;
    for (auto& row : g_peers) row = {};
}
void Sustain(void* actor) {
    // A short coast lease composes with hooks/AI claims: never Release someone else's
    // claim when a player steps off. The existing rest timer closes our coast afterwards.
    prop_drive_host::Coast(actor, "player support");
}
}
void Tick(net::Session& s) {
    if (!s.connected()) return;
    ResetWorld();
    const auto now = active_drive::NowMs();
    if (now < g_next) return;
    g_next = now+200;
    void* actor = ue_wrap::player_support::Read(players::Registry::Get().Local());
    if (!Eligible(actor)) actor = nullptr;
    if (s.role() == net::Role::Host) {
        if (actor) Sustain(actor);
        for (uint8_t slot = 1; slot < net::kMaxPeers; ++slot) {
            auto& row = g_peers[slot];
            void* support = row.actor.Get();
            auto target = element::IntentTarget::ForClientIntent(s, slot, 300.f);
            if (now < row.until && Eligible(support) && target.Authorize(support)) Sustain(support);
            else row = {};
        }
    } else {
        net::PropSupportPayload p{};
        if (actor) {
            const auto key = ue_wrap::prop::GetInteractableKeyString(actor);
            if (!key.empty() && key != L"None" && key.size() <= sizeof(p.key.data))
                net::WireKeyFromString(key, p.key);
            auto eid = remote_prop::ResolveMirrorEidByActor(actor);
            if (eid == element::kInvalidId) eid = prop_element_tracker::GetPropElementIdForActor(actor);
            if (eid != element::kInvalidId) p.elementId = eid;
        }
        const bool on = p.key.len || p.elementId;
        if (on || g_announced)
            if (s.SendReliable(net::ReliableKind::PropSupport, &p, sizeof(p))) g_announced = on;
    }
}
void OnIntent(net::Session& s, const net::PropSupportPayload& p, uint8_t slot) {
    if (!s.connected() || s.role() != net::Role::Host || slot < 1 || slot >= net::kMaxPeers ||
        p.key.len > sizeof(p.key.data)) return;
    ResetWorld();
    auto& row = g_peers[slot];
    if (!p.key.len && !p.elementId) { row = {}; return; }
    void* actor = p.key.len ? prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key)) : nullptr;
    if (!actor && p.elementId) actor = remote_prop::ResolveLiveActorByEid(p.elementId);
    auto target = element::IntentTarget::ForClientIntent(s, slot, 300.f);
    if (!Eligible(actor) || !target.Authorize(actor)) return;
    if (row.actor.Get() != actor) UE_LOGI("[PROP-SUPPORT] peer %u base eid=%u", slot, p.elementId);
    row.actor.Set(actor); row.until = active_drive::NowMs()+1000;
    Sustain(actor);
}
void OnDisconnect() {
    for (auto& row : g_peers) row = {};
    g_next = 0; g_world = 0; g_announced = false;
}
}
