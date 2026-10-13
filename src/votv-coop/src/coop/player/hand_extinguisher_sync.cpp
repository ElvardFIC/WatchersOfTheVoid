#include "coop/player/hand_extinguisher_sync.h"

#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/remote_player.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "coop/player/players_registry.h"
#include "coop/element/intent_authority.h"
#include "ue_wrap/actors/fire_extinguisher.h"
#include "ue_wrap/actors/pressure_washer.h"
#include "ue_wrap/actors/building_material.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/world_identity.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>

namespace coop::hand_extinguisher_sync {
namespace {
namespace FX = ue_wrap::fire_extinguisher;
namespace Water = ue_wrap::pressure_washer;
namespace Material = ue_wrap::building_material;

struct Row {
    net::HandExtinguisherPayload state{};
    uint64_t sentAt = 0;
    uint8_t pending = 0;
    bool have = false;
    bool applied = false;
    ue_wrap::CachedObjRef appliedActor;
    uint64_t nextWaterPulse = 0;
};

std::atomic<net::Session*> g_session{nullptr};
std::array<Row, net::kMaxPeers> g_rows{};
uint32_t g_world = 0;
uint32_t g_localSequence = 0;
uint64_t g_nextPoll = 0;
uint64_t g_rateAt[net::kMaxPeers]{};
uint8_t g_rateCount[net::kMaxPeers]{};

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

net::Session* Session() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->connected() ? s : nullptr;
}

bool After(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) > 0; }

void ResetWorld() {
    const uint32_t world = ue_wrap::world_identity::Generation();
    if (world == g_world) return;
    g_world = world;
    g_rows = {};
    g_localSequence = 0;
    g_nextPoll = 0;
    std::fill(std::begin(g_rateAt), std::end(g_rateAt), 0);
    std::fill(std::begin(g_rateCount), std::end(g_rateCount), 0);
}

uint8_t ReadyMask(net::Session& s, uint8_t owner) {
    uint8_t mask = 0;
    for (int slot = 1; slot < net::kMaxPeers; ++slot)
        if (slot != owner && s.IsSlotReady(slot)) mask |= uint8_t(1u << slot);
    return mask;
}

void* ActorFor(uint8_t slot, uint32_t generation) {
    if (slot >= net::kMaxPeers) return nullptr;
    if (slot == players::Registry::Get().LocalPeerId())
        return hand_item::Generation(slot) == generation ? hand_item::LocalHandActor() : nullptr;
    return hand_item::MirrorGeneration(slot) == generation ? hand_item::MirrorActorForSlot(slot) : nullptr;
}

bool ReadState(void* actor, uint8_t tool, bool& spraying, float& charge, uint8_t& details) {
    if (!actor) return false;
    details=0;
    if(tool==4) {Material::Stock stock{};spraying=false;
        if(!Material::ReadStock(actor,stock) || stock.kind!=Material::StockKind::ConcreteBucket || stock.units>20)return false;
        charge=static_cast<float>((std::max)(0,stock.units));details=stock.dry?1:0;return true;}
    if(tool==3) {Material::Water water{};spraying=false;
        if(!Material::ReadWater(actor,water))return false;
        charge=std::clamp(water.height,0.f,38.f);
        details=uint8_t((water.soap>0?1:0)|(water.alcohol && charge>0?2:0));return true;}
    if(tool==2) {int32_t units=0;spraying=false;
        if(!Material::ReadConcrete(actor,units))return false;
        charge=static_cast<float>(units);return true;}
    if (tool == 1) {
        if (!Water::Read(actor, spraying, charge)) return false;
    } else if (!FX::IsExtinguisher(actor) || !FX::ReadSpraying(actor, spraying) ||
               !FX::ReadCharge(actor, charge)) return false;
    if (!std::isfinite(charge) || charge > (tool == 1 ? 10.f : 1.5f)) return false;
    // Native depletion can cross zero by one frame before fire(false).
    charge = (std::max)(0.f, charge);
    if (charge == 0.f) spraying = false;
    return true;
}

bool WriteState(void* actor, uint8_t tool, bool spraying, float charge, uint8_t details) {
    if(tool==4)return !spraying && !(details&~1u) && std::isfinite(charge) && charge>=0 && charge<=20 &&
        std::floor(charge)==charge && Material::SetRemoteConcreteBucketDisplay(actor,static_cast<int32_t>(charge),(details&1)!=0);
    if(tool==3)return !spraying && !(details&~3u) &&
        Material::SetRemoteBucketDisplay(actor,charge,(details&1)!=0,(details&2)!=0);
    if(tool==2)return !spraying && std::isfinite(charge) && charge>=0 && charge<=5 &&
        std::floor(charge)==charge && Material::SetRemoteConcrete(actor,static_cast<int32_t>(charge));
    if (tool == 1) return Water::SetRemoteLevel(actor, charge);
    return actor && FX::IsExtinguisher(actor) && std::isfinite(charge) && charge >= 0.f && charge <= 1.5f &&
           FX::WriteCharge(actor, charge) && FX::SetRemoteSpray(actor, spraying && charge > 0.f);
}

bool Same(const Row& row, bool spraying, float charge, uint8_t details) {
    return row.have && row.state.reserved==details && row.state.spraying == uint8_t(spraying) && std::fabs(row.state.charge - charge) < .01f;
}

void SampleLocal(net::Session& s, uint64_t now) {
    const uint8_t slot = players::Registry::Get().LocalPeerId();
    if (slot >= net::kMaxPeers) return;
    void* actor = hand_item::LocalHandActor();
    const uint32_t generation = hand_item::Generation(slot);
    const uint8_t tool = Material::IsConcreteBucket(actor) ? 4 : (Material::IsBucket(actor) ? 3 : (Material::IsTrowel(actor) ? 2 : (Water::IsWasher(actor) ? 1 : 0)));
    if (!generation || (!tool && !FX::IsExtinguisher(actor))) {
        g_rows[slot] = {};
        return;
    }
    bool spraying = false;
    float charge = 0.f;
    uint8_t details=0;
    if (!ReadState(actor, tool, spraying, charge, details)) return;
    Row& row = g_rows[slot];
    const bool changed = row.state.generation != generation || row.state.tool != tool || !Same(row, spraying, charge, details);
    const uint64_t refreshMs = spraying ? 500 : 1000;
    if (!changed && now - row.sentAt < refreshMs) return;
    if (!++g_localSequence) ++g_localSequence;
    net::HandExtinguisherPayload p{};
    p.slot = slot;
    p.spraying = uint8_t(spraying);
    p.tool = tool;
    p.reserved = details;
    p.generation = generation;
    p.sequence = g_localSequence;
    p.charge = charge;
    if (s.role() == net::Role::Client) {
        if (!s.SendReliableToSlot(0, net::ReliableKind::HandExtinguisher, &p, sizeof(p))) return;
        row.state = p;
        row.have = true;
        row.sentAt = now;
        return;
    }
    row.state = p;
    row.have = true;
    row.applied = true;
    row.sentAt = now;
    row.pending = ReadyMask(s, slot);
}

void ApplyPending(net::Session& s, uint64_t now) {
    const auto local = players::Registry::Get().LocalPeerId();
    for (uint8_t slot = 0; slot < net::kMaxPeers; ++slot) {
        if (slot == local) continue; // network display state never touches the owner's hand
        Row& row = g_rows[slot];
        if (!row.have) continue;
        void* actor = ActorFor(slot, row.state.generation);
        if (!actor) continue; // HandItem and state can arrive in either order, including on host
        if (!row.applied || row.appliedActor.Get() != actor) {
            if (now - row.sentAt > 5000) continue;
            const auto state = row.state;
            const auto world = g_world;
            ue_wrap::CachedObjRef identity; identity.Set(actor);
            if (s.role() == net::Role::Host &&
                !element::IntentTarget::ForClientIntent(s, slot, 600.f).Authorize(actor)) continue;
            if (!identity.Get() || world != ue_wrap::world_identity::Generation() ||
                g_session.load() != &s || !row.have || row.state.sequence != state.sequence) continue;
            if (!WriteState(actor, state.tool, state.spraying != 0, state.charge, state.reserved) || !identity.Get() ||
                world != ue_wrap::world_identity::Generation() || g_session.load() != &s ||
                !row.have || row.state.sequence != state.sequence) continue;
            row.applied = true;
            row.appliedActor = identity;
            if (s.role() == net::Role::Host) row.pending = ReadyMask(s, slot);
        }
        if (row.applied && row.state.spraying && row.state.charge > 0.f) {
            auto* puppet = players::Registry::Get().Puppet(slot);
            if (puppet && puppet->valid() && puppet->HasPose()) {
                if (row.state.tool == 0) FX::AimRemoteSpray(actor, puppet->GetSyncedAimRotation());
                else if (now >= row.nextWaterPulse && now - row.sentAt <= 1500) {
                    row.nextWaterPulse = now + 100; // native water bursts; cap visual spawn rate
                    ue_wrap::FVector origin{};
                    if (puppet->TryGetHeadPosition(origin))
                        Water::EmitRemote(actor, origin, puppet->GetSyncedAimRotation());
                }
            }
        }
    }
}

void Flush(net::Session& s) {
    for (uint8_t owner = 0; owner < net::kMaxPeers; ++owner) {
        Row& row = g_rows[owner];
        if (!row.have || !row.pending) continue;
        for (int slot = 1; slot < net::kMaxPeers; ++slot) {
            const uint8_t bit = uint8_t(1u << slot);
            if (!(row.pending & bit) || !s.IsSlotReady(slot)) continue;
            if (s.SendReliableToSlot(slot, net::ReliableKind::HandExtinguisher,
                                     &row.state, sizeof(row.state))) row.pending &= uint8_t(~bit);
        }
    }
}

}  // namespace

void Install(net::Session* session) {
    g_session.store(session, std::memory_order_release);
    FX::ResolveNames();
    ResetWorld();
}

void Tick() {
    auto* s = Session();
    if (!s) return;
    ResetWorld();
    const uint64_t now = NowMs();
    if (now >= g_nextPoll) {
        g_nextPoll = now + 100;
        SampleLocal(*s, now);
    }
    ApplyPending(*s, now);
    if (g_session.load() == s && s->connected()) Flush(*s);
}

void Receive(const net::HandExtinguisherPayload& p, uint8_t senderSlot) {
    auto* s = Session();
    if (!s || p.slot >= net::kMaxPeers || !p.generation || !p.sequence || p.spraying > 1 || p.tool > 4 ||
        (p.tool==4 ? (p.reserved&~1u)!=0 : (p.tool==3 ? (p.reserved&~3u)!=0 : p.reserved!=0)) ||
        !std::isfinite(p.charge) || p.charge < 0.f || p.charge > (p.tool==4 ? 20.f : (p.tool==3 ? 38.f : (p.tool == 2 ? 5.f : (p.tool == 1 ? 10.f : 1.5f)))) ||
        (p.tool>=2 && p.spraying) || ((p.tool==2 || p.tool==4) && std::floor(p.charge)!=p.charge) ||
        (p.tool==3 && p.charge==0 && (p.reserved&2))) return;
    ResetWorld();
    if (s->role() == net::Role::Host) {
        if (senderSlot == 0 || senderSlot != p.slot || senderSlot >= net::kMaxPeers) return;
        const uint64_t now = NowMs();
        if (now - g_rateAt[senderSlot] >= 1000) {
            g_rateAt[senderSlot] = now;
            g_rateCount[senderSlot] = 0;
        }
        if (g_rateCount[senderSlot] >= 20) return;
        Row& row = g_rows[senderSlot];
        if (row.have && !After(p.sequence, row.state.sequence)) return;
        ++g_rateCount[senderSlot];
        row.state = p;
        row.have = true;
        row.applied = false;
        row.pending = 0;
        row.sentAt = now;
        return;
    }
    if (senderSlot != 0 || p.slot == players::Registry::Get().LocalPeerId()) return;
    Row& row = g_rows[p.slot];
    if (row.have && !After(p.sequence, row.state.sequence)) return;
    row.state = p;
    row.have = true;
    row.applied = false;
    row.sentAt = NowMs();
}

void QueueConnect(int slot) {
    if (slot <= 0 || slot >= net::kMaxPeers) return;
    ResetWorld();
    for (auto& row : g_rows)
        if (row.have && row.applied && row.state.slot != slot) row.pending |= uint8_t(1u << slot);
    g_nextPoll = 0;
}

void OnPeerLeft(int slot) {
    if (slot <= 0 || slot >= net::kMaxPeers) return;
    g_rows[slot] = {};
    g_rateAt[slot] = 0;
    g_rateCount[slot] = 0;
    for (auto& row : g_rows) row.pending &= uint8_t(~(1u << slot));
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_rows = {};
    g_localSequence = 0;
    g_nextPoll = 0;
    std::fill(std::begin(g_rateAt), std::end(g_rateAt), 0);
    std::fill(std::begin(g_rateCount), std::end(g_rateCount), 0);
    g_world = 0;
}

}  // namespace coop::hand_extinguisher_sync
