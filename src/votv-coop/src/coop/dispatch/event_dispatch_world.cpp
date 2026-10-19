// coop/dispatch/event_dispatch_world.cpp -- the ambient and world-event reliable-kind case
// bodies (the firefly, the event cue, fire and snapshot, the alarm, the server box, the
// roaches, the inventory pickup, chat, the sky, the red sky, lightning, weather)
// and the shared sender-eid range check; see coop/dispatch/event_dispatch.h.

#include "event_dispatch.h"  // co-located private header (src tree, not include/)

#include "coop/element/registry.h"
#include "coop/creatures/grayboar_actions.h"

#include "coop/comms/chat_sync.h"
#include "coop/world/alarm_sync.h"
#include "coop/interactables/serverbox_sync.h"  // the host-authoritative signal-server state
#include "coop/interactables/server_upgrade_sync.h"  // the servers' physical upgrades
#include "coop/creatures/roach_sync.h"           // the host-authoritative roach snapshot
#include "coop/world/event_active_sync.h"
#include "coop/world/event_cue_sync.h"
#include "coop/world/event_fire_sync.h"
#include "coop/world/media_sync.h"
#include "coop/interactables/kitchen_sync.h"
#include "coop/interactables/cord_sync.h"
#include "coop/props/prop_support_sync.h"
#include "coop/world/service_panel_sync.h"
#include "coop/world/desk_short_circuit_sync.h"
#include "coop/world/madness_sync.h"
#include "coop/props/dingus_music_sync.h"
#include "coop/world/radio_sync.h"
#include "coop/world/cargo_lift_sync.h"
#include "coop/props/prop_fire_sync.h"
#include "coop/props/explosion_sync.h"
#include "coop/props/explosion_events.h"
#include "coop/player/hand_fire_sync.h"
#include "coop/player/hand_extinguisher_sync.h"
#include "coop/player/hand_toy_audio.h"
#include "coop/player/sit_pose.h"
#include "coop/player/players_registry.h"
#include "coop/world/coord_tower_ops.h"
#include "coop/world/coord_tower_rows.h"
#include "coop/world/firefly_sync.h"
#include "coop/world/power_grid.h"
#include "coop/items/inventory_pickup_sync.h"
#include "coop/items/hook_retrieve.h"
#include "coop/world/placed_save_sync.h"
#include "coop/world/sky_sync.h"
#include "coop/world/weather_lightning.h"
#include "coop/world/weather_redsky.h"
#include "coop/world/weather_sync.h"

#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"

#include <cmath>
#include <cstring>

namespace coop::event_feed {

// The role-range validation for an inbound eid-carrying packet. Without it a malicious client
// could stamp a packet with the host's sender element id, the Registry would resolve it to the
// host's Player Element, and the receiver would apply the packet's effect under host identity.
// The range partition makes that impersonation detectable at the wire boundary: host-role
// packets must carry host-range eids and client-role packets peer-range eids. True when the
// eid is in range or no compare is possible (the 0 sentinel, an invalid sender slot); logs and
// returns false on out-of-range. The per-peer stale-generation defence lives in the session's
// sender-epoch latch, applied to every inbound packet by the transport; this helper keeps the
// role-range half because the eid range is the sender's claimed role, a wire-format trust
// boundary independent of that.
bool VerifySenderEidRange(int senderPeerSlot,
                          uint32_t senderElementId,
                          const char* kind) {
    if (senderElementId == 0u ||
        senderElementId == coop::element::kInvalidId) {
        return true;  // 0 sentinel; no compare. peer-slot fallback applies.
    }
    if (senderPeerSlot < 0) return true;  // unknown sender; skip range check
    const bool senderIsHost = (senderPeerSlot == 0);
    if (!coop::element::Registry::IsAllowedSenderEid(
            senderIsHost, senderElementId)) {
        UE_LOGW("event_feed: %s senderElementId=0x%08x out of allowed %s "
                "range (senderPeerSlot=%d) -- dropping (role impersonation?)",
                kind, senderElementId,
                senderIsHost ? "host" : "peer",
                senderPeerSlot);
        return false;
    }
    return true;
}

bool HandleWorldEvent(net::Session& session,
                      const net::Session::ReliableMessage& msg) {
    switch (msg.kind) {
    case net::ReliableKind::GrayboarAction: {
        if(msg.senderPeerSlot!=0 || msg.payloadLen!=sizeof(net::GrayboarActionPayload))break;
        net::GrayboarActionPayload packet{};std::memcpy(&packet,msg.payload,sizeof(packet));
        coop::grayboar_actions::Receive(packet,msg.senderPeerSlot);break;
    }
    case net::ReliableKind::PlacedSave: {
        if (msg.payloadLen != sizeof(net::BlobChunkPayload) || msg.senderPeerSlot < 0 ||
            msg.senderPeerSlot >= net::kMaxPeers) break;
        net::BlobChunkPayload chunk{};
        std::memcpy(&chunk, msg.payload, sizeof(chunk));
        coop::placed_save_sync::Receive(chunk, static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::HandToyAudio: {
        if(msg.payloadLen!=sizeof(net::HandToyAudioPayload) || msg.senderPeerSlot<0 || msg.senderPeerSlot>=net::kMaxPeers)break;
        net::HandToyAudioPayload p{};std::memcpy(&p,msg.payload,sizeof(p));
        coop::hand_toy_audio::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));break;
    }
    case net::ReliableKind::DingusMusic: {
        if(msg.payloadLen!=sizeof(net::DingusMusicPayload) || msg.senderPeerSlot<0 || msg.senderPeerSlot>=net::kMaxPeers)break;
        net::DingusMusicPayload p{};std::memcpy(&p,msg.payload,sizeof(p));
        coop::dingus_music_sync::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));break;
    }
    case net::ReliableKind::MadnessState: {
        if(msg.senderPeerSlot!=0 || msg.payloadLen!=sizeof(net::MadnessStatePayload))break;
        net::MadnessStatePayload p{};std::memcpy(&p,msg.payload,sizeof(p));
        coop::madness_sync::Receive(p,0);break;
    }
    case net::ReliableKind::HookRetrieve: {
        if(msg.payloadLen!=sizeof(net::HookRetrievePayload) || msg.senderPeerSlot<0 ||
           msg.senderPeerSlot>=net::kMaxPeers)break;
        net::HookRetrievePayload p{};std::memcpy(&p,msg.payload,sizeof(p));
        coop::hook_retrieve::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));break;
    }
    case net::ReliableKind::DeskShortCircuit: {
        if (msg.payloadLen != sizeof(net::DeskShortCircuitPayload) || msg.senderPeerSlot < 0 ||
            msg.senderPeerSlot >= net::kMaxPeers) break;
        net::DeskShortCircuitPayload p{};
        std::memcpy(&p, msg.payload, sizeof(p));
        coop::desk_short_circuit_sync::Receive(p, static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::PropFire: {
        if(msg.payloadLen!=sizeof(net::PropFirePayload)||msg.senderPeerSlot<0||msg.senderPeerSlot>=net::kMaxPeers)break;
        net::PropFirePayload p{};std::memcpy(&p,msg.payload,sizeof(p));
        coop::prop_fire_sync::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::Explosion: {
        if(msg.payloadLen!=sizeof(net::ExplosionPayload)||msg.senderPeerSlot<0||msg.senderPeerSlot>=net::kMaxPeers)break;
        net::ExplosionPayload p{};std::memcpy(&p,msg.payload,sizeof(p));
        coop::explosion_sync::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::ExplosionEvent: {
        if(msg.payloadLen!=sizeof(net::ExplosionEventPayload)||msg.senderPeerSlot<0||msg.senderPeerSlot>=net::kMaxPeers)break;
        net::ExplosionEventPayload p{};std::memcpy(&p,msg.payload,sizeof(p));
        coop::explosion_events::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::HandFire: {
        if (msg.payloadLen != sizeof(net::HandFirePayload) || msg.senderPeerSlot < 0 || msg.senderPeerSlot >= net::kMaxPeers) break;
        net::HandFirePayload p{}; std::memcpy(&p, msg.payload, sizeof(p));
        coop::hand_fire_sync::Receive(p, static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::HandExtinguisher: {
        if (msg.payloadLen != sizeof(net::HandExtinguisherPayload) || msg.senderPeerSlot < 0 ||
            msg.senderPeerSlot >= net::kMaxPeers) break;
        net::HandExtinguisherPayload p{};
        std::memcpy(&p, msg.payload, sizeof(p));
        coop::hand_extinguisher_sync::Receive(p, static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::CargoLift: {
        if(msg.payloadLen!=sizeof(net::CargoLiftPayload)||msg.senderPeerSlot<0||msg.senderPeerSlot>=net::kMaxPeers)break;
        net::CargoLiftPayload p{};std::memcpy(&p,msg.payload,sizeof(p));
        coop::cargo_lift_sync::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::ServicePanel: {
        if (msg.payloadLen!=sizeof(net::ServicePanelPayload) || msg.senderPeerSlot<0 || msg.senderPeerSlot>=net::kMaxPeers) break;
        net::ServicePanelPayload p{};
        std::memcpy(&p,msg.payload,sizeof(p));
        coop::service_panel_sync::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::RadioState: {
        if (msg.payloadLen!=sizeof(net::RadioStatePayload) || msg.senderPeerSlot<0 || msg.senderPeerSlot>=net::kMaxPeers) break;
        net::RadioStatePayload p{}; std::memcpy(&p,msg.payload,sizeof(p));
        coop::radio_sync::Receive(p,static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::CordState: {
        if (msg.payloadLen != sizeof(net::CordStatePayload) ||
            session.role() != net::Role::Client || msg.senderPeerSlot != 0) break;
        net::CordStatePayload p{};
        std::memcpy(&p, msg.payload, sizeof(p));
        coop::cord_sync::Apply(p);
        break;
    }
    case net::ReliableKind::CordIntent: {
        if (msg.payloadLen != sizeof(net::CordStatePayload) || session.role() != net::Role::Host ||
            msg.senderPeerSlot <= 0 || msg.senderPeerSlot >= net::kMaxPeers) break;
        net::CordStatePayload p{};
        std::memcpy(&p, msg.payload, sizeof(p));
        coop::cord_sync::OnIntent(p, static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::PropSupport: {
        if (msg.payloadLen != sizeof(net::PropSupportPayload) || session.role() != net::Role::Host ||
            msg.senderPeerSlot <= 0 || msg.senderPeerSlot >= net::kMaxPeers) break;
        net::PropSupportPayload p{};
        std::memcpy(&p, msg.payload, sizeof(p));
        coop::prop_support_sync::OnIntent(session, p, static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::KitchenState: {
        if (msg.payloadLen != sizeof(net::KitchenStatePayload) ||
            session.role() != net::Role::Client || msg.senderPeerSlot != 0) break;
        net::KitchenStatePayload p{};
        std::memcpy(&p,msg.payload,sizeof(p));
        coop::kitchen_sync::Apply(p);
        break;
    }
    case net::ReliableKind::KitchenIntent: {
        if (msg.payloadLen != sizeof(net::KitchenIntentPayload) ||
            session.role() != net::Role::Host || msg.senderPeerSlot <= 0 || msg.senderPeerSlot >= net::kMaxPeers) break;
        net::KitchenIntentPayload p{};
        std::memcpy(&p,msg.payload,sizeof(p));
        coop::kitchen_sync::OnIntent(p,static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::FireflySpawn: {
        // The peer-symmetric ambient firefly: any peer may originate one (each runs its own spawner
        // near its own camera and shares), and the host relays a client's spawn to the other
        // clients. No trust gate, a cosmetic transient particle; the origin never receives its own
        // send, so the receiver always materialises another peer's firefly at its world position.
        if (msg.payloadLen < sizeof(net::FireflySpawnPayload)) {
            UE_LOGW("event_feed: FireflySpawn payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::FireflySpawnPayload));
            break;
        }
        net::FireflySpawnPayload fp{};
        std::memcpy(&fp, msg.payload, sizeof(fp));
        coop::firefly_sync::OnReliable(fp);
        break;
    }
    case net::ReliableKind::EventCue: {
        // The host-authoritative cosmetic emitter cue. Only the host originates one (it is the sole
        // event producer; clients run a dormant scheduler), so this only ever runs on a client and
        // always replays the host's cue emitter. No trust gate needed for a cosmetic transient; the
        // sender epoch and the host-only send topology already bound it.
        if (msg.payloadLen < sizeof(net::EventCuePayload)) {
            UE_LOGW("event_feed: EventCue payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::EventCuePayload));
            break;
        }
        net::EventCuePayload ep{};
        std::memcpy(&ep, msg.payload, sizeof(ep));
        coop::event_cue_sync::OnReliable(ep);
        break;
    }
    case net::ReliableKind::EventFire: {
        // The host-authoritative scheduled or story event fired: the client replays the native verb
        // per the replay policy (the level, save and cosmetic flips no lane carries; lane-covered
        // rows are logged and skipped there). World-mutating, so host trust-bound like the weather;
        // there is no eid in the payload, and the identity is the transport slot.
        if (msg.payloadLen < sizeof(net::EventFirePayload)) {
            UE_LOGW("event_feed: EventFire payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::EventFirePayload));
            break;
        }
        if (session.role() == net::Role::Host) {
            UE_LOGI("event_feed: EventFire received on host -- dropping (host is the authority)");
            break;
        }
        if (msg.senderPeerSlot != 0) {
            UE_LOGW("event_feed: EventFire from non-host senderPeerSlot=%d -- dropping",
                    msg.senderPeerSlot);
            break;
        }
        net::EventFirePayload fp{};
        std::memcpy(&fp, msg.payload, sizeof(fp));
        coop::event_fire_sync::OnReliable(fp);
        break;
    }
    case net::ReliableKind::EventGather: {
        if (msg.payloadLen < sizeof(net::EventGatherPayload)) break;
        if (session.role() != net::Role::Host || msg.senderPeerSlot == 0) break;
        net::EventGatherPayload gp{};
        std::memcpy(&gp, msg.payload, sizeof(gp));
        coop::event_fire_sync::OnLocationGather(gp, static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::SitAnchor: {
        if (msg.payloadLen < sizeof(net::SitAnchorPayload)) break;
        if (msg.senderPeerSlot < 0 || msg.senderPeerSlot >= coop::players::kMaxPeers) break;
        net::SitAnchorPayload sp{};
        std::memcpy(&sp, msg.payload, sizeof(sp));
        coop::sit_pose::OnAnchor(static_cast<uint8_t>(msg.senderPeerSlot), sp);
        break;
    }
    case net::ReliableKind::MediaState: {
        if (msg.payloadLen < sizeof(net::MediaStatePayload)) break;
        if (session.role() != net::Role::Client || msg.senderPeerSlot != 0) break;
        net::MediaStatePayload mp{};
        std::memcpy(&mp, msg.payload, sizeof(mp));
        coop::media_sync::OnReliable(mp);
        break;
    }
    case net::ReliableKind::EventSnapshot: {
        // The host-to-joiner in-flight event registry entry at the world-ready edge. The same trust
        // shape as the event fire: world-mutating, host-only origin, never relayed.
        if (msg.payloadLen < sizeof(net::EventSnapshotPayload)) {
            UE_LOGW("event_feed: EventSnapshot payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::EventSnapshotPayload));
            break;
        }
        if (session.role() == net::Role::Host) {
            UE_LOGI("event_feed: EventSnapshot received on host -- dropping (host is the authority)");
            break;
        }
        if (msg.senderPeerSlot != 0) {
            UE_LOGW("event_feed: EventSnapshot from non-host senderPeerSlot=%d -- dropping",
                    msg.senderPeerSlot);
            break;
        }
        net::EventSnapshotPayload sp{};
        std::memcpy(&sp, msg.payload, sizeof(sp));
        coop::event_active_sync::OnReliable(sp);
        break;
    }
    case net::ReliableKind::AlarmState: {
        // The base radar alarm shared-world toggle, both directions by design: host to all is the
        // canonical state, client to host a local transition request (the client's own scan or stop
        // press); the role validation lives in the module, where a client drops non-host senders.
        // Not relayed by the transport: the host's reaction to a client's request is its own
        // poll-driven broadcast, never a forward of the client packet.
        if (msg.payloadLen < sizeof(net::AlarmStatePayload)) {
            UE_LOGW("event_feed: AlarmState payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::AlarmStatePayload));
            break;
        }
        net::AlarmStatePayload ap{};
        std::memcpy(&ap, msg.payload, sizeof(ap));
        coop::alarm_sync::OnReliable(ap, msg.senderPeerSlot);
        break;
    }
    case net::ReliableKind::ServerState: {
        // The host-authoritative signal-server state, host to clients only; the client drives the
        // real actor (a raw write of the broken flag and the reflected check). The module drops a
        // non-host sender.
        if (msg.payloadLen < sizeof(net::ServerStatePayload)) {
            UE_LOGW("event_feed: ServerState payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::ServerStatePayload));
            break;
        }
        net::ServerStatePayload sp{};
        std::memcpy(&sp, msg.payload, sizeof(sp));
        coop::serverbox_sync::OnReliable(sp, msg.senderPeerSlot);
        break;
    }
    case net::ReliableKind::ServerRepair: {
        // A client's finished repair of a server box, client to host only; the module runs it or answers the
        // presser with the state row, and drops it on a client.
        if (msg.payloadLen < sizeof(net::ServerRepairPayload)) {
            UE_LOGW("event_feed: ServerRepair payload too short (%zu < %zu)", static_cast<size_t>(msg.payloadLen),
                    sizeof(net::ServerRepairPayload));
            break;
        }
        net::ServerRepairPayload rp{};
        std::memcpy(&rp, msg.payload, sizeof(rp));
        coop::serverbox_sync::OnRepair(rp, msg.senderPeerSlot);
        break;
    }
    case net::ReliableKind::ServerUpgradeState: {
        // A box's upgrade op (client to host), every box's level (host to all) or a refusal (host to
        // the op's author). Role and trust gates live in server_upgrade_sync::OnState.
        if (msg.payloadLen < sizeof(net::ServerUpgradeStatePayload)) {
            UE_LOGW("event_feed: ServerUpgradeState payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::ServerUpgradeStatePayload));
            break;
        }
        net::ServerUpgradeStatePayload up{};
        std::memcpy(&up, msg.payload, sizeof(up));
        const uint8_t upslot =
            (msg.senderPeerSlot >= 0 && msg.senderPeerSlot < net::kMaxPeers)
                ? static_cast<uint8_t>(msg.senderPeerSlot)
                : static_cast<uint8_t>(0xFF);
        coop::server_upgrade_sync::OnState(up, upslot);
        break;
    }
    case net::ReliableKind::PowerGridState: {
        // The base's generators: the host's rows (host to all) or a client's op on one, a repair, a service, an
        // upgrade or a hit (client to host). Role, reach and trust gates live in power_grid::OnReliable.
        if (msg.payloadLen < sizeof(net::PowerGridPayload)) {
            UE_LOGW("event_feed: PowerGridState payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::PowerGridPayload));
            break;
        }
        net::PowerGridPayload gp{};
        std::memcpy(&gp, msg.payload, sizeof(gp));
        const uint8_t gslot =
            (msg.senderPeerSlot >= 0 && msg.senderPeerSlot < net::kMaxPeers)
                ? static_cast<uint8_t>(msg.senderPeerSlot)
                : static_cast<uint8_t>(0xFF);
        coop::power_grid::OnReliable(gp, gslot);
        break;
    }
    case net::ReliableKind::CoordTowerState: {
        // The coordinate towers: the host's rows (host to all) or a client's op on one, a press or a fuse it moved
        // (client to host). Role, reach and trust gates live in the two modules.
        if (msg.payloadLen < sizeof(net::CoordTowerPayload)) {
            UE_LOGW("event_feed: CoordTowerState payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::CoordTowerPayload));
            break;
        }
        net::CoordTowerPayload tp{};
        std::memcpy(&tp, msg.payload, sizeof(tp));
        if (msg.senderPeerSlot < 0 || msg.senderPeerSlot >= net::kMaxPeers) break;
        const auto tslot = static_cast<uint8_t>(msg.senderPeerSlot);
        if (tp.op == net::kCoordTowerOpRows) {
            if (tslot == 0) coop::coord_tower_rows::OnRows(tp);  // the host's alone
        } else {
            coop::coord_tower_ops::OnOp(tp, tslot);
        }
        break;
    }
    case net::ReliableKind::RoachState: {
        // The host-authoritative roach-infestation snapshot, paged. The client assembles the pages
        // and applies by ordinal (driving location and scale, or rebuilding through the game's own
        // add and delete). The module drops a non-host sender.
        if (msg.payloadLen < sizeof(net::RoachStatePayload)) {
            UE_LOGW("event_feed: RoachState payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::RoachStatePayload));
            break;
        }
        net::RoachStatePayload rp{};
        std::memcpy(&rp, msg.payload, sizeof(rp));
        coop::roach_sync::OnState(rp, msg.senderPeerSlot);
        break;
    }
    case net::ReliableKind::InventoryPickup: {
        // A peer collected an item into its inventory: play the native inventory cue at the
        // broadcast position. Peer-symmetric, host-relayed, cosmetic; the origin never receives its
        // own send.
        if (msg.payloadLen < sizeof(net::InventoryPickupPayload)) {
            UE_LOGW("event_feed: InventoryPickup payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::InventoryPickupPayload));
            break;
        }
        net::InventoryPickupPayload ip{};
        std::memcpy(&ip, msg.payload, sizeof(ip));
        coop::inventory_pickup_sync::OnReliable(ip);
        break;
    }
    case net::ReliableKind::ChatMessage: {
        // A client's chat intent, client to host only. Identity comes from the transport slot, so a
        // peer cannot speak as someone else, and the payload is text only, decoded strictly at the
        // boundary in chat_sync before it can enter the lobby's record or reach a screen.
        if (msg.payloadLen < sizeof(net::ChatMessagePayload)) {
            UE_LOGW("event_feed: ChatMessage payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::ChatMessagePayload));
            break;
        }
        if (msg.senderPeerSlot < 0 || msg.senderPeerSlot >= net::kMaxPeers) {
            UE_LOGW("event_feed: ChatMessage invalid senderPeerSlot=%d -- dropping",
                    msg.senderPeerSlot);
            break;
        }
        net::ChatMessagePayload cp{};
        std::memcpy(&cp, msg.payload, sizeof(cp));
        coop::chat_sync::OnReliable(cp, static_cast<uint8_t>(msg.senderPeerSlot));
        break;
    }
    case net::ReliableKind::ChatSpeaker: {
        // Who the chat line that follows is from, host to client; it always immediately precedes
        // its line on the same ordered lane.
        if (msg.payloadLen < sizeof(net::ChatSpeakerPayload)) {
            UE_LOGW("event_feed: ChatSpeaker payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::ChatSpeakerPayload));
            break;
        }
        net::ChatSpeakerPayload sp{};
        std::memcpy(&sp, msg.payload, sizeof(sp));
        coop::chat_sync::OnChatSpeaker(sp);
        break;
    }
    case net::ReliableKind::ChatLine: {
        // The host's authored chat row, carrying the line sequence that is the order.
        if (msg.payloadLen < sizeof(net::ChatLinePayload)) {
            UE_LOGW("event_feed: ChatLine payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::ChatLinePayload));
            break;
        }
        if (msg.senderPeerSlot != 0) {
            // Only the host authors. A client claiming to is a protocol violation, and accepting it
            // would let any peer write the lobby's permanent record.
            UE_LOGW("event_feed: ChatLine from senderPeerSlot=%d -- only the host "
                    "authors chat; dropping", msg.senderPeerSlot);
            break;
        }
        net::ChatLinePayload lp{};
        std::memcpy(&lp, msg.payload, sizeof(lp));
        coop::chat_sync::OnChatLine(lp);
        break;
    }
    case net::ReliableKind::SkyState: {
        // The host-authoritative night-sky orientation, moon phase and eye, host to client, trust-gated
        // to slot 0 like the clock. The client writes the sky mesh's world rotation and the moon
        // phase and runs the sky's setEye for the eye (the module no-ops on the host).
        if (msg.senderPeerSlot != 0) {
            UE_LOGW("event_feed: SkyState from non-host senderPeerSlot=%d -- dropping", msg.senderPeerSlot);
            break;
        }
        if (msg.payloadLen < sizeof(net::SkyStatePayload)) {
            UE_LOGW("event_feed: SkyState payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::SkyStatePayload));
            break;
        }
        net::SkyStatePayload sp{};
        std::memcpy(&sp, msg.payload, sizeof(sp));
        // The NaN and infinity guard before the raw float writes (a NaN rotation is a rotator
        // assert or a garbage transform). The rotations are bounded angles and the moon phase a
        // material scalar; the module re-checks too.
        if (!std::isfinite(sp.skyPitch) || !std::isfinite(sp.skyYaw) ||
            !std::isfinite(sp.skyRoll) || !std::isfinite(sp.moonPhase)) {
            UE_LOGW("event_feed: SkyState non-finite floats -- dropping");
            break;
        }
        coop::sky_sync::OnReliable(sp);
        break;
    }
    case net::ReliableKind::RedSky: {
        // The red sky's edge: the host sends its red sky as each run of the gamemode's toggle
        // leaves it (watched at the script gate); the receiver runs the same toggle on its own
        // gamemode when its sky differs.
        if (msg.payloadLen < sizeof(net::RedSkyPayload)) {
            UE_LOGW("event_feed: RedSky payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::RedSkyPayload));
            break;
        }
        net::RedSkyPayload p{};
        std::memcpy(&p, msg.payload, sizeof(p));
        if (session.role() == net::Role::Host) {
            UE_LOGI("event_feed: RedSky received on host -- dropping");
            break;
        }
        // Host trust-bound: the red sky is host-only, and a non-host sender is a protocol
        // violation.
        if (msg.senderPeerSlot != 0) {
            UE_LOGW("event_feed: RedSky from non-host senderPeerSlot=%d "
                    "(senderElementId=0x%08x) -- dropping",
                    msg.senderPeerSlot, p.senderElementId);
            break;
        }
        // The role-range trust on the sender element id.
        if (!VerifySenderEidRange(msg.senderPeerSlot, p.senderElementId,
                                   "RedSky")) {
            break;
        }
        if (p.state != 0 && p.state != 1) {
            UE_LOGW("event_feed: RedSky state=%u out of range -- dropping",
                    static_cast<unsigned>(p.state));
            break;
        }
        net::RedSkyPayload pCopy = p;
        ue_wrap::game_thread::Post([pCopy] {
            ::coop::weather_redsky::Apply(pCopy);
        });
        break;
    }
    case net::ReliableKind::LightningStrike: {
        // The discrete lightning strike: the host's POST observer on the deferred spawn caught a
        // strike spawn (a blueprint-internal spawn inside the cycle's lightning timer) and
        // broadcast its world location. The client suppressed its own lightning timer through the
        // interceptor, so no local strike happened; this packet drives the visual.
        if (msg.payloadLen < sizeof(net::LightningStrikePayload)) {
            UE_LOGW("event_feed: LightningStrike payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::LightningStrikePayload));
            break;
        }
        net::LightningStrikePayload p{};
        std::memcpy(&p, msg.payload, sizeof(p));
        if (session.role() == net::Role::Host) {
            UE_LOGI("event_feed: LightningStrike received on host -- dropping");
            break;
        }
        // Host trust-bound: the strike is host-only, and a non-host sender is a protocol violation.
        if (msg.senderPeerSlot != 0) {
            UE_LOGW("event_feed: LightningStrike from non-host "
                    "senderPeerSlot=%d (senderElementId=0x%08x) -- dropping",
                    msg.senderPeerSlot, p.senderElementId);
            break;
        }
        // The role-range trust on the sender element id.
        if (!VerifySenderEidRange(msg.senderPeerSlot, p.senderElementId,
                                   "LightningStrike")) {
            break;
        }
        // The trust boundary: the location must be finite and within sane bounds.
        if (!std::isfinite(p.locX) || !std::isfinite(p.locY) || !std::isfinite(p.locZ) ||
            std::fabs(p.locX) > coop::net::kMaxCoord ||
            std::fabs(p.locY) > coop::net::kMaxCoord ||
            std::fabs(p.locZ) > coop::net::kMaxCoord) {
            UE_LOGW("event_feed: LightningStrike loc out of bounds (%.0f, %.0f, %.0f) -- dropping",
                    p.locX, p.locY, p.locZ);
            break;
        }
        net::LightningStrikePayload pCopy = p;
        ue_wrap::game_thread::Post([pCopy] {
            ::coop::weather_lightning::Apply(pCopy);
        });
        break;
    }
    case net::ReliableKind::WeatherState: {
        // The host-authoritative weather state. The receiver looks up its local day-night cycle and
        // invokes the cycle's mutator UFunctions to apply each delta; see the weather sync's apply.
        if (msg.payloadLen < sizeof(net::WeatherStatePayload)) {
            UE_LOGW("event_feed: WeatherState payload too short (%zu < %zu)",
                    static_cast<size_t>(msg.payloadLen), sizeof(net::WeatherStatePayload));
            break;
        }
        net::WeatherStatePayload p{};
        std::memcpy(&p, msg.payload, sizeof(p));
        // The self-echo guard: weather is host to client only, so a packet arriving on the host is
        // a loopback bounce, dropped.
        if (session.role() == net::Role::Host) {
            UE_LOGI("event_feed: WeatherState received on host -- dropping "
                    "(host is the authority; no inbound from client)");
            break;
        }
        // Host trust-bound: the weather is host-only, and a non-host sender is a protocol
        // violation.
        if (msg.senderPeerSlot != 0) {
            UE_LOGW("event_feed: WeatherState from non-host "
                    "senderPeerSlot=%d (senderElementId=0x%08x) -- dropping",
                    msg.senderPeerSlot, p.senderElementId);
            break;
        }
        // The role-range trust on the sender element id.
        if (!VerifySenderEidRange(msg.senderPeerSlot, p.senderElementId,
                                   "WeatherState")) {
            break;
        }
        // The trust boundary: every float the receiver writes into engine memory must be finite and
        // within a sane range. The rain scalars are unitless and small (the chances reach about a
        // hundred), the fog density about 15 at most, the wind about 50; a generous bound of a
        // thousand catches garbage and NaN without clamping any legitimate value. The wind floats
        // are written raw into the wind actor and the fog and rain floats validated here too, one
        // trust boundary in one check.
        const float vals[] = {
            p.rainStrength, p.rainLightningChance, p.rainDeactivateChance, p.rainWindSpeed,
            p.rain, p.finalFogDensity, p.fogAlpha, p.fogStrength,
            p.windSpeedBg, p.windStrengthBg, p.windSpeedRain, p.windStrengthRain
        };
        bool bad = false;
        for (float v : vals) {
            if (!std::isfinite(v) || std::fabs(v) > 1.0e3f) { bad = true; break; }
        }
        if (bad) {
            UE_LOGW("event_feed: WeatherState floats out of bounds (rain=%.2f lc=%.2f dc=%.2f "
                    "ws=%.2f fog=%.2f windBg=%.2f/%.2f windRain=%.2f/%.2f) -- dropping",
                    p.rainStrength, p.rainLightningChance, p.rainDeactivateChance,
                    p.rainWindSpeed, p.finalFogDensity, p.windSpeedBg, p.windStrengthBg,
                    p.windSpeedRain, p.windStrengthRain);
            break;
        }
        net::WeatherStatePayload pCopy = p;
        ue_wrap::game_thread::Post([pCopy] {
            ::coop::weather_sync::ApplyFromHost(pCopy);
        });
        break;
    }
    default:
        return false;  // not a world-family kind -> event_feed tries the next family
    }
    return true;  // a world-family kind was matched (processed or validation-dropped)
}

}  // namespace coop::event_feed
