// coop/player/sit_pose.cpp -- see coop/player/sit_pose.h.

#include "coop/player/sit_pose.h"

#include "coop/interactables/atv_sync.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/element/intent_authority.h"
#include "coop/props/prop_element_tracker.h"
#include "ue_wrap/actors/prop.h"
#include "coop/player/players_registry.h"
#include "coop/player/puppet_drive.h"
#include "coop/player/remote_player.h"

#include "ue_wrap/actors/sit_pose.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <chrono>

namespace coop::sit_pose {
namespace {

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_localSitting = false;
ue_wrap::CachedObjRef g_lastChair;
uint64_t g_lastAnchorAt = 0;
float g_sentChairYaw = 0.f;
uint64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool IsChair(void* actor) {
    return actor && ue_wrap::reflection::IsLive(actor) && ue_wrap::prop::IsDescendantOfProp(actor) &&
        ue_wrap::reflection::ClassNameOf(actor).find(L"prop_sit") == 0;
}
float g_sentX = 0.f, g_sentY = 0.f, g_sentZ = 0.f;
float g_sentYaw = 0.f;

struct Anchor {
    bool on = false;
    float x = 0, y = 0, z = 0, yaw = 0;
};
Anchor g_anchor[coop::players::kMaxPeers];

bool SendAnchor(coop::net::Session* s, const ue_wrap::FVector& loc, float yaw, bool active, void* chair) {
    coop::net::SitAnchorPayload p{};
    p.x = loc.X; p.y = loc.Y; p.z = loc.Z; p.yaw = yaw;
    p.active = active ? 1 : 0;
    ue_wrap::FRotator rotation{};
    if (IsChair(chair) && ue_wrap::engine::TryGetActorRotation(chair, rotation)) {
        const auto key = ue_wrap::prop::GetInteractableKeyString(chair);
        if (!key.empty() && key != L"None") coop::net::WireKeyFromString(key, p.chairKey);
        p.chairPitch = ue_wrap::NormalizeAxis(rotation.Pitch);
        p.chairYaw = ue_wrap::NormalizeAxis(rotation.Yaw);
        p.chairRoll = ue_wrap::NormalizeAxis(rotation.Roll);
    }
    return s->SendReliable(coop::net::ReliableKind::SitAnchor, &p, sizeof(p));
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

bool LocalIsSitting() { return g_localSitting; }

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    void* local = coop::players::Registry::Get().Local();
    if (!s || !s->connected() || !local) {
        if (g_localSitting && local) ue_wrap::sit_pose::DriveSitting(local, false);
        if (local) ue_wrap::sit_pose::ClearMeshShift(local);
        g_localSitting = false;
        return;
    }
    g_localSitting = ue_wrap::sit_pose::IsSittingOnSomething(local) ||
                     coop::atv_sync::LocalIsDriving();
    ue_wrap::sit_pose::DriveSitting(local, g_localSitting);

    void* seat = nullptr;
    const bool hasSeat = ue_wrap::engine::ReadMainPlayerSittingOn(local, seat) && seat &&
                         ue_wrap::reflection::IsLive(seat);
    const bool onAtv = hasSeat && ue_wrap::atv::EnsureResolved() && ue_wrap::atv::IsAtv(seat);
    const bool chair = hasSeat && !onAtv;
    // The clip's hips are not on the capsule. Shift the mesh only: the actor stays, so standing
    // up still leaves the ground where the game put the feet. Chair mesh: half the body length,
    // plus two heads down and two heads forward, plus another half head down and two heads
    // forward. ATV mesh stays finished: one and a half heads up and three heads forward.
    if (g_localSitting) {
        const float head = ue_wrap::sit_pose::ModelHead(local);
        if (onAtv || coop::atv_sync::LocalIsDriving())
            ue_wrap::sit_pose::ShiftMesh(local, head * 1.5f, head * 3.f);
        else
            ue_wrap::sit_pose::ShiftMesh(local,
                                         -ue_wrap::sit_pose::ModelHalfLength(local) - head * 2.5f,
                                         head * 4.f);
    } else {
        ue_wrap::sit_pose::ClearMeshShift(local);
    }
    if (chair) {
        // The pawn, not the seat actor: the game has already moved the body onto the sit point,
        // and the seat's own pivot is often the mesh origin. A seat that moves (the ATV rack)
        // sends again when the body has travelled, so the other machine does not leave them behind.
        ue_wrap::FVector loc{};
        ue_wrap::FRotator rot{};
        if (ue_wrap::engine::TryGetActorLocation(local, loc) &&
            ue_wrap::engine::TryGetActorRotation(local, rot)) {
            const float dx = loc.X - g_sentX, dy = loc.Y - g_sentY, dz = loc.Z - g_sentZ;
        const float yawDelta = std::remainder(rot.Yaw - g_sentYaw, 360.f);
        ue_wrap::FRotator chairRot{};
        ue_wrap::engine::TryGetActorRotation(seat, chairRot);
        const uint64_t now = NowMs();
        const bool changed = !g_lastChair.Is(seat) || dx * dx + dy * dy + dz * dz > 40.f * 40.f ||
            std::fabs(yawDelta) >= 2.f || std::fabs(std::remainder(chairRot.Yaw - g_sentChairYaw, 360.f)) >= 2.f ||
            now - g_lastAnchorAt >= 1000;
        if (changed && (!g_lastChair.Is(seat) || now - g_lastAnchorAt >= 100) && SendAnchor(s, loc, rot.Yaw, true, seat)) {
            g_lastChair.Set(seat);
            g_lastAnchorAt = now;
            g_sentChairYaw = chairRot.Yaw;
            g_sentX = loc.X;
            g_sentY = loc.Y;
            g_sentZ = loc.Z;
            g_sentYaw = rot.Yaw;
        }
        }
    } else if (g_lastChair.Raw()) {
        if (SendAnchor(s, {}, 0.f, false, g_lastChair.Get())) g_lastChair.Reset();
    }
}

void OnAnchor(uint8_t slot, const coop::net::SitAnchorPayload& payload) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || !session->connected() || payload.active > 1 || payload.pad[0] || payload.pad[1] || payload.pad[2] ||
        payload.chairKey.len > sizeof(payload.chairKey.data) ||
        !std::isfinite(payload.chairPitch) || !std::isfinite(payload.chairYaw) || !std::isfinite(payload.chairRoll) ||
        std::fabs(payload.chairPitch) > 180.f || std::fabs(payload.chairYaw) > 180.f || std::fabs(payload.chairRoll) > 180.f) return;
    if (slot >= coop::players::kMaxPeers) return;
    if (slot == coop::players::Registry::Get().LocalPeerId()) return;
    if (!std::isfinite(payload.x) || !std::isfinite(payload.y) || !std::isfinite(payload.z) ||
        !std::isfinite(payload.yaw)) return;
    g_anchor[slot].on = payload.active != 0;
    g_anchor[slot].x = payload.x;
    g_anchor[slot].y = payload.y;
    g_anchor[slot].z = payload.z;
    g_anchor[slot].yaw = payload.yaw;
    if (!payload.chairKey.len) return;
    const auto key = coop::net::StringFromWireKey(payload.chairKey);
    void* chair = coop::prop_element_tracker::ResolveLiveActorByKey(key, nullptr);
    if (!IsChair(chair)) return;
    if (session->role() == coop::net::Role::Host &&
        !coop::element::IntentTarget::ForClientIntent(*session, slot, 450.f).Authorize(chair)) return;
    void* localSeat = nullptr;
    if (ue_wrap::engine::ReadMainPlayerSittingOn(coop::players::Registry::Get().Local(), localSeat) && localSeat == chair) return;
    // Apply to the chair itself, not its sitBox child's offset or the puppet.
    ue_wrap::engine::SetActorRotation(chair, {payload.chairPitch, payload.chairYaw, payload.chairRoll});
}

void DriveRemotes() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    for (uint8_t slot = 0; slot < coop::players::kMaxPeers; ++slot) {
        if (slot == localSlot) continue;
        coop::RemotePlayer& pup = coop::puppet_drive::Puppet(static_cast<int>(slot));
        void* actor = pup.GetActor();
        if (!actor || !pup.valid()) continue;
        if (!g_anchor[slot].on) {
            ue_wrap::sit_pose::ClearMeshShift(actor);
            continue;
        }
        ue_wrap::engine::SetActorLocation(actor, ue_wrap::FVector{ g_anchor[slot].x, g_anchor[slot].y,
                                                                    g_anchor[slot].z });
        ue_wrap::engine::SetActorRotation(actor, ue_wrap::FRotator{ 0.f, g_anchor[slot].yaw, 0.f });
        ue_wrap::sit_pose::DriveSitting(actor, true);
        const float head = ue_wrap::sit_pose::ModelHead(actor);
        ue_wrap::sit_pose::ShiftMesh(actor,
                                     -ue_wrap::sit_pose::ModelHalfLength(actor) - head * 2.5f,
                                     head * 4.f);
    }
}

void OnPeerLeft(uint8_t slot) {
    if (slot >= coop::players::kMaxPeers) return;
    g_anchor[slot] = {};
    coop::RemotePlayer& pup = coop::puppet_drive::Puppet(static_cast<int>(slot));
    if (void* actor = pup.GetActor()) ue_wrap::sit_pose::ClearMeshShift(actor);
}

void DriveAtvDrivers() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    const uint8_t localSlot = coop::players::Registry::Get().LocalPeerId();
    void* atvs[8]{};
    uint8_t drivers[8]{};
    const int n = coop::atv_sync::CopyIndexed(atvs, drivers, 8);
    for (int i = 0; i < n; ++i) {
        const uint8_t slot = drivers[i];
        if (slot == 0xFF || slot == localSlot || slot >= coop::players::kMaxPeers) continue;
        coop::RemotePlayer& pup = coop::puppet_drive::Puppet(static_cast<int>(slot));
        void* actor = pup.GetActor();
        if (!actor || !pup.valid()) continue;
        ue_wrap::FVector loc{};
        ue_wrap::FRotator rot{};
        if (void* comp = ue_wrap::sit_pose::AtvDriverSeat(atvs[i])) {
            loc = ue_wrap::engine::GetComponentLocation(comp);
            rot = ue_wrap::engine::GetComponentWorldRotation(comp);
        } else if (ue_wrap::engine::TryGetActorLocation(atvs[i], loc)) {
            ue_wrap::engine::TryGetActorRotation(atvs[i], rot);
            const ue_wrap::FVector fwd = ue_wrap::engine::GetActorForwardVector(atvs[i]);
            loc.X += fwd.X * 40.f;
            loc.Y += fwd.Y * 40.f;
            loc.Z += 45.f;
        } else {
            continue;
        }
        ue_wrap::engine::SetActorLocation(actor, loc);
        ue_wrap::engine::SetActorRotation(actor, rot);
        ue_wrap::sit_pose::DriveSitting(actor, true);
        const float head = ue_wrap::sit_pose::ModelHead(actor);
        ue_wrap::sit_pose::ShiftMesh(actor, head * 1.5f, head * 3.f);
    }
}

void OnDisconnect() {
    g_localSitting = false;
    g_lastChair.Reset();
    g_lastAnchorAt = 0;
    for (auto& a : g_anchor) a = {};
    void* local = coop::players::Registry::Get().Local();
    if (local) {
        ue_wrap::sit_pose::DriveSitting(local, false);
        ue_wrap::sit_pose::ClearMeshShift(local);
    }
    for (uint8_t slot = 0; slot < coop::players::kMaxPeers; ++slot) {
        coop::RemotePlayer& pup = coop::puppet_drive::Puppet(static_cast<int>(slot));
        if (pup.valid()) ue_wrap::sit_pose::ClearMeshShift(pup.GetActor());
    }
}

}  // namespace coop::sit_pose
