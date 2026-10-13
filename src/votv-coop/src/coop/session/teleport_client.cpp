#include "coop/session/teleport_client.h"

#include "coop/player/players_registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/world_identity.h"

#include <atomic>
#include <array>
#include <cmath>
#include <string>

namespace coop::teleport_client {

namespace P = ue_wrap::profile;
namespace R = ue_wrap::reflection;
namespace E = ue_wrap::engine;
namespace GT = ue_wrap::game_thread;

namespace {

std::atomic<coop::net::Session*> g_session{nullptr};
struct PendingGather {ApplyArgs args{};uint64_t deadline=0,retryAt=0,serial=0;bool active=false;};
std::array<PendingGather,coop::net::kMaxPeers> g_gathers{};
coop::net::Session* g_gatherSession=nullptr;
uint32_t g_gatherWorld=0;
uint64_t g_gatherSerial=0;
void RefreshGathers(coop::net::Session* s) {
    const auto world=ue_wrap::world_identity::Generation();
    if(g_gatherSession!=s||g_gatherWorld!=world||!s||!s->connected()) {
        g_gathers={};g_gatherSession=s;g_gatherWorld=world;++g_gatherSerial;
    }
}

// Cached teleportWObackrooms UFunction. VOTV's own player constraints silently revert a
// large-distance K2_TeleportTo or SetActorLocation -- the actor snaps back near the save spawn,
// so a client asked to move a long way appears to jump on the spot. teleportWObackrooms is the
// function the game's own graph uses for in-game teleports (backrooms exit, door teleports) and
// is not subject to them; K2_TeleportTo and SetActorLocation stay as fallbacks for the case where
// it cannot be resolved.
void* g_teleportWObackroomsFn = nullptr;  // mainPlayer_C::teleportWObackrooms UFunction*

// Resolve mainPlayer_C::teleportWObackrooms once. Its signature is
//   void teleportWObackrooms(FTransform NewTransform, bool useRotation, bool trueRotation);
// We pass useRotation=false (we set actor/controller rotation separately, avoiding an
// FRotator->FQuat conversion in the parameter frame). The function pointer is class-level (the
// same UFunction* for all instances), cached forever.
void* ResolveTeleportFn(void* anyMainPlayer) {
    if (g_teleportWObackroomsFn) return g_teleportWObackroomsFn;
    if (!anyMainPlayer) return nullptr;
    void* cls = R::ClassOf(anyMainPlayer);
    if (!cls) return nullptr;
    g_teleportWObackroomsFn = R::FindFunction(cls, L"teleportWObackrooms");
    if (!g_teleportWObackroomsFn) {
        UE_LOGW("teleport_client: teleportWObackrooms UFunction not found on mainPlayer_C -- "
                "will fall back to K2_TeleportTo");
    }
    return g_teleportWObackroomsFn;
}

}  // namespace

void SetSession(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

bool ApplyLocally(const ApplyArgs& args) {
    void* local = coop::players::Registry::Get().Local();
    if (!local) {
        UE_LOGW("teleport_client: no local mainPlayer_C found (not in gameplay?)");
        return false;
    }
    const ue_wrap::FVector  loc{args.locX, args.locY, args.locZ};
    const ue_wrap::FRotator rot{args.rotPitch, args.rotYaw, args.rotRoll};

    // Primary path: VOTV's own teleportWObackrooms. Translation only in the
    // FTransform (identity rotation + unit scale defaults from MakeTransform).
    // useRotation=false -> the BP graph keeps whatever rotation the player /
    // controller already had; we set the target rotation explicitly below.
    void* fn = ResolveTeleportFn(local);
    bool moved = false;
    if (fn) {
        const ue_wrap::FTransform xform = ue_wrap::MakeTransform(loc);
        ue_wrap::ParamFrame f(fn);
        f.SetRaw(L"NewTransform", &xform, sizeof(xform));
        f.Set<bool>(L"useRotation", false);
        f.Set<bool>(L"trueRotation", false);
        moved = ue_wrap::Call(local, f);
        if (!moved) {
            UE_LOGW("teleport_client: teleportWObackrooms call failed -- falling back");
        }
    }
    // Fallback path: K2_TeleportTo (large-distance-aware but can be reverted
    // by VOTV constraints), then SetActorLocation as last resort.
    if (!moved) {
        if (E::TeleportTo(local, loc, rot)) {
            moved = true;
        } else {
            UE_LOGW("teleport_client: TeleportTo failed too -- using SetActorLocation");
            moved = E::SetActorLocation(local, loc);
        }
    }
    // Always update the actor + controller rotation. Without the controller's ControlRotation the
    // camera stays aimed in the old direction after the body moves.
    E::SetActorRotation(local, rot);
    if (void* ctrl = E::GetController(local)) {
        E::SetControlRotation(ctrl, rot);
    }
    UE_LOGI("teleport_client: applied (local=%p path=%s loc=(%.0f,%.0f,%.0f) rot=(p=%.1f y=%.1f r=%.1f))",
            local, fn ? (moved ? "teleportWObackrooms" : "fallback") : "fallback-noFn",
            args.locX, args.locY, args.locZ, args.rotPitch, args.rotYaw, args.rotRoll);
    return moved;
}

namespace {

// Snapshot the host's own mainPlayer pose into a TeleportClientPayload. Game
// thread only (reads the live actor). Returns false if there's no local player.
bool SnapshotHostPose(coop::net::TeleportClientPayload& p) {
    void* local = coop::players::Registry::Get().Local();
    if (!local) {
        UE_LOGW("teleport_client: no local mainPlayer_C to snapshot");
        return false;
    }
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    if (!E::TryGetActorLocation(local, loc) || !E::TryGetActorRotation(local, rot)) {
        UE_LOGW("teleport_client: the host's location or rotation could not be read -- nothing to send");
        return false;
    }
    p.locX = loc.X; p.locY = loc.Y; p.locZ = loc.Z;
    p.rotPitch = rot.Pitch; p.rotYaw = rot.Yaw; p.rotRoll = rot.Roll;
    return true;
}

void SnapshotAndSend(coop::net::Session* s) {
    coop::net::TeleportClientPayload p{};
    if (!SnapshotHostPose(p)) return;
    const bool sent = s->SendReliable(coop::net::ReliableKind::TeleportClient, &p, sizeof(p));
    if (!sent) UE_LOGW("teleport_client: broadcast failed (channel busy or not connected)");
    else       UE_LOGI("teleport_client: sent host pose to all clients (loc=(%.0f,%.0f,%.0f))",
                       p.locX, p.locY, p.locZ);
}

void SnapshotAndSendToSlot(coop::net::Session* s, int slot) {
    coop::net::TeleportClientPayload p{};
    if (!SnapshotHostPose(p)) return;
    // Single-target reliable (default senderSlot=0 -> the receiving client sees
    // senderPeerSlot==0 == host, passing event_feed's host-only TeleportClient
    // trust gate). Only this peer moves.
    const bool sent = s->SendReliableToSlot(slot, coop::net::ReliableKind::TeleportClient,
                                            &p, sizeof(p));
    if (!sent) UE_LOGW("teleport_client: per-slot send to slot %d failed (not connected?)", slot);
    else       UE_LOGI("teleport_client: sent host pose to slot %d (loc=(%.0f,%.0f,%.0f))",
                       slot, p.locX, p.locY, p.locZ);
}

}  // namespace

void GatherOthersTo(uint8_t anchorSlot, float x, float y, float z, float yaw) {
    auto* s = g_session.load(std::memory_order_acquire);
    RefreshGathers(s);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) return;
    if(anchorSlot>=coop::net::kMaxPeers)return;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(yaw)) return;
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    const auto now=coop::active_drive::NowMs();
    g_gathers[anchorSlot]={}; // a newer event cannot move its own initiator to an older anchor
    for (int slot = 0; slot < coop::net::kMaxPeers; ++slot) {
        if (static_cast<uint8_t>(slot) == anchorSlot) continue;
        const float ang = (slot + 1) * 1.7f;
        const ApplyArgs args{ x + std::cos(ang) * 110.f, y + std::sin(ang) * 110.f, z,
                              0.f, yaw, 0.f };
        if (slot!=localSlot && (!s->HasPeerConn(slot)||!s->IsSlotWorldReady(slot))) {g_gathers[slot]={};continue;}
        g_gathers[slot]={args,now+5000,now,++g_gatherSerial,true};
    }
    TickGather();
    UE_LOGI("teleport_client: requested gather to slot %u at (%.0f, %.0f, %.0f)",
            static_cast<unsigned>(anchorSlot), x, y, z);
}

void TickGather() {
    auto* s=g_session.load(std::memory_order_acquire);RefreshGathers(s);
    if(!s||!s->connected()||s->role()!=coop::net::Role::Host)return;
    const auto now=coop::active_drive::NowMs();const auto world=g_gatherWorld;
    const auto local=coop::players::Registry::Get().LocalPeerId();
    for(int slot=0;slot<coop::net::kMaxPeers;++slot) {
        auto& pending=g_gathers[slot];if(!pending.active)continue;
        if(now>=pending.deadline || (slot!=local&&(!s->HasPeerConn(slot)||!s->IsSlotWorldReady(slot)))) {pending={};continue;}
        if(now<pending.retryAt)continue;
        const auto copy=pending;pending.retryAt=now+200;
        bool ok=false;
        if(slot==local)ok=ApplyLocally(copy.args);
        else {
            coop::net::TeleportClientPayload p{};
            p.locX=copy.args.locX;p.locY=copy.args.locY;p.locZ=copy.args.locZ;
            p.rotPitch=copy.args.rotPitch;p.rotYaw=copy.args.rotYaw;p.rotRoll=copy.args.rotRoll;
            ok=s->SendReliableToSlot(slot,coop::net::ReliableKind::TeleportClient,&p,sizeof(p));
        }
        if(g_session.load()!=s||ue_wrap::world_identity::Generation()!=world)return;
        if(ok&&pending.serial==copy.serial)pending={};
    }
}
void ClearGather(){g_gathers={};g_gatherSession=nullptr;++g_gatherSerial;}

void TeleportClientsToHost() {
    // Sender-gates to Role::Host only -- on a client this is a no-op (per scope:
    // "teleport clients to the host when the host triggers it"). The snapshot runs
    // on the game thread; the client applies on receive.
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) {
        UE_LOGI("teleport_client: ignored -- this action is host-only (local role is not Host)");
        return;
    }
    GT::Post([s] { SnapshotAndSend(s); });
}

void TeleportSlotToHost(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) {
        UE_LOGI("teleport_client: per-slot ignored -- host-only (local role is not Host)");
        return;
    }
    if (peerSlot < 1 || peerSlot >= static_cast<int>(coop::players::kMaxPeers)) {
        UE_LOGW("teleport_client: per-slot TP rejected -- slot %d out of range", peerSlot);
        return;
    }
    GT::Post([s, peerSlot] { SnapshotAndSendToSlot(s, peerSlot); });
}

}  // namespace coop::teleport_client
