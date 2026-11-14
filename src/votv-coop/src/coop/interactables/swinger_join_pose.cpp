// coop/interactables/swinger_join_pose.cpp -- see coop/interactables/swinger_join_pose.h.

#include "coop/interactables/swinger_join_pose.h"

#include "coop/interactables/interactable_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/wire_key_util.h"
#include "coop/props/remote_prop.h"  // IsOwnedHere: a remote drive or this peer's own grab owns the lid

#include "ue_wrap/actors/swinger.h"
#include "ue_wrap/core/hot_path_guard.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_attach.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include <memory>
#include <array>
#include <cmath>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace coop::swinger_join_pose {
namespace {

// The place the host's copy rests at, as it arrived.
struct Rest {
    ue_wrap::FVector  loc;
    ue_wrap::FRotator rot;
};

// A rest pose whose lid this peer's index does not hold yet, or whose copy does not read open yet (the
// ContainerState that opens it may still be parked in the lane's own retry). The latest pose for a key wins.
// `queuedAt` is the container index's pass count when it was parked, for the lane's drop rule.
struct Pending {
    std::wstring key;
    uint64_t epoch = 0;
    uint32_t world = 0;
    ue_wrap::CachedObjRef actor, cls;
    bool applying = false;
    Rest rest;
    uint64_t queuedAt;
    std::chrono::steady_clock::time_point deadline;
};

// The receive boundary's bound on a store this peer's messages grow (every such store has one): the host sends
// one pose per open lid, a few dozen at most, and a key is parked without bound otherwise. A new key past it is
// refused, said once per session.
constexpr size_t kPendingCap = 256;
using Receipt = std::shared_ptr<Pending>;
std::unordered_map<std::wstring, Receipt> g_pending;
uint64_t g_epoch = 1;
bool g_ticking = false;
uint64_t g_lastPass = 0;
// How each key's last pose ended: true placed, false dropped (a stream or a drag owned the lid, the wait ran out,
// or the pending cap refused it). Read by the dev query only (COLD: one write per pose). A key the host invents
// is recorded too, so an insert that would grow it past kEndedCap clears it first (the shape of
// unresolved_pose_ledger): the dev reader loses only old outcomes. Cleared with the session.
constexpr size_t kEndedCap = 512;
std::unordered_map<std::wstring, bool> g_ended;

void Ended(const std::wstring& key, bool placed) {
    if (g_ended.size() >= kEndedCap && g_ended.find(key) == g_ended.end()) g_ended.clear();
    g_ended[key] = placed;
}

bool Installed(const Receipt& p) {
    auto it=g_pending.find(p->key);
    return it!=g_pending.end() && it->second==p;
}
bool Current(const Receipt& p) {
    return Installed(p) && p->epoch==g_epoch && p->world==ue_wrap::world_identity::Generation();
}
void Finish(const Receipt& p,bool placed) {
    if (!Installed(p)) return;
    g_pending.erase(p->key);
    Ended(p->key,placed);
}
bool Finite(const Rest& r) {
    return std::isfinite(r.loc.X) && std::isfinite(r.loc.Y) && std::isfinite(r.loc.Z) &&
        std::isfinite(r.rot.Pitch) && std::isfinite(r.rot.Yaw) && std::isfinite(r.rot.Roll);
}
enum class Try { Applied, Owned, Wait };
Try TryApply(const Receipt& p) {
    if (!Current(p)) return Try::Owned;
    if (p->applying) return Try::Wait;
    struct Applying { bool& b; Applying(bool& v):b(v){b=true;} ~Applying(){b=false;} } applying(p->applying);
    void* lid=coop::interactable_sync::ContainerActor(p->key);
    if (!Current(p)) return Try::Owned;
    if (!lid) return p->actor.Raw()?Try::Owned:Try::Wait;
    if (!p->actor.Raw()) {
        if (!ue_wrap::reflection::IsLive(lid)) return Try::Wait;
        p->actor.Set(lid);
        p->cls.Set(ue_wrap::reflection::ClassOf(lid));
    }
    auto bound=[&] {
        return Current(p) && p->actor.Is(lid) && p->cls.Get() &&
            ue_wrap::reflection::ClassOf(lid)==p->cls.Raw() &&
            coop::interactable_sync::ContainerActor(p->key)==lid;
    };
    auto available=[&] {
        if (!bound()) return false;
        const bool owned=coop::remote_prop::IsOwnedHere(lid);
        return bound() && !owned;
    };
    if (!available()) return Try::Owned;
    bool open=false;
    const bool read=ue_wrap::swinger::TryReadOpen(lid,open);
    if (!available()) return Try::Owned;
    if (!read || !open) return Try::Wait;
    const bool moved=ue_wrap::engine::SetActorLocation(lid,p->rest.loc);
    if (!available()) return Try::Owned;
    if (!moved) return Try::Wait;
    const bool rotated=ue_wrap::engine::SetActorRotation(lid,p->rest.rot);
    if (!available()) return Try::Owned;
    if (!rotated) return Try::Wait;
    const bool still=ue_wrap::engine::SetActorRootPhysicsVelocity(lid,{},{});
    if (!available()) return Try::Owned;
    return still?Try::Applied:Try::Wait;
}

struct HostLid {
    std::wstring key;
    ue_wrap::CachedObjRef actor, cls;
    bool done = false;
    std::chrono::steady_clock::time_point due{};
};
struct HostJoin {
    coop::net::Session* session = nullptr;
    uint64_t epoch = 0, stopped = 0;
    uint32_t serial = 0, world = 0, generation = 0;
    int slot = 0;
    bool building = true;
    size_t cursor = 0, remaining = 0;
    std::vector<std::shared_ptr<HostLid>> lids;
    std::unordered_set<std::wstring> forgotten;
};
using JoinReceipt = std::shared_ptr<HostJoin>;
std::array<JoinReceipt, coop::net::kMaxPeers> g_hostJoins;
coop::net::Session* g_hostSession = nullptr;
uint64_t g_hostEpoch = 1;
int g_hostCursor = 1;
bool g_hostDraining = false;
constexpr int kHostBudget = 16;
constexpr auto kHostRetry = std::chrono::milliseconds(250);

void ClearHost() {
    ++g_hostEpoch;
    g_hostJoins.fill({});
    g_hostSession = nullptr;
    g_hostCursor = 1;
}
bool HostCurrent(const JoinReceipt& join) {
    return g_hostJoins[join->slot] == join && join->epoch == g_hostEpoch &&
        join->session == g_hostSession && join->serial == coop::net::session_serial::Current() &&
        join->world == ue_wrap::world_identity::Generation() &&
        join->session->StopCount() == join->stopped && join->session->connected() &&
        join->session->role() == coop::net::Role::Host &&
        join->generation && join->session->peerGenerationForSlot(join->slot) == join->generation &&
        join->session->IsSlotConnected(join->slot);
}
bool HostActorCurrent(const JoinReceipt& join, const std::shared_ptr<HostLid>& lid) {
    return HostCurrent(join) && !lid->done && lid->actor.Get() && lid->cls.Get() &&
        ue_wrap::reflection::ClassOf(lid->actor.Raw()) == lid->cls.Raw();
}
bool HostBound(const JoinReceipt& join, const std::shared_ptr<HostLid>& lid) {
    if (!HostActorCurrent(join, lid)) return false;
    void* bound = coop::interactable_sync::ContainerActor(lid->key);
    return HostActorCurrent(join, lid) && bound == lid->actor.Raw();
}
void FinishHostLid(const JoinReceipt& join, const std::shared_ptr<HostLid>& lid) {
    if (!HostCurrent(join) || lid->done) return;
    lid->done = true;
    if (--join->remaining == 0) g_hostJoins[join->slot].reset();
}
bool HostReady(const JoinReceipt& join) {
    return HostCurrent(join) && join->session->IsSlotWorldReady(join->slot);
}
void TryHostLid(const JoinReceipt& join, const std::shared_ptr<HostLid>& lid) {
    if (!HostReady(join)) return;
    if (!HostBound(join, lid)) { FinishHostLid(join, lid); return; }
    bool open = false;
    const bool read = ue_wrap::swinger::TryReadOpen(lid->actor.Raw(), open);
    if (!HostBound(join, lid)) { FinishHostLid(join, lid); return; }
    if (!read) return;
    if (!open) { FinishHostLid(join, lid); return; }
    Rest rest{};
    const bool location = ue_wrap::engine::TryGetActorLocation(lid->actor.Raw(), rest.loc);
    if (!HostBound(join, lid)) { FinishHostLid(join, lid); return; }
    if (!location) return;
    const bool rotation = ue_wrap::engine::TryGetActorRotation(lid->actor.Raw(), rest.rot);
    if (!HostBound(join, lid)) { FinishHostLid(join, lid); return; }
    if (!rotation || !Finite(rest) || std::fabs(rest.loc.X) > coop::net::kMaxCoord ||
        std::fabs(rest.loc.Y) > coop::net::kMaxCoord || std::fabs(rest.loc.Z) > coop::net::kMaxCoord) return;
    coop::net::SwingerRestPosePayload payload{};
    coop::net::WireKeyFromString(lid->key, payload.key);
    payload.locX = rest.loc.X; payload.locY = rest.loc.Y; payload.locZ = rest.loc.Z;
    payload.rotPitch = rest.rot.Pitch; payload.rotYaw = rest.rot.Yaw; payload.rotRoll = rest.rot.Roll;
    if (!HostReady(join)) return;
    const bool sent = join->session->SendReliableToSlot(join->slot,
        coop::net::ReliableKind::SwingerRestPose, &payload, sizeof(payload));
    // The callback may install a newer request or invalidate this actor/recipient.
    if (!HostBound(join, lid)) { FinishHostLid(join, lid); return; }
    if (sent) FinishHostLid(join, lid);
}
void DrainHost() {
    if (g_hostDraining) return;
    struct Drain { Drain(){g_hostDraining=true;} ~Drain(){g_hostDraining=false;} } drain;
    const auto epoch = g_hostEpoch;
    const auto now = std::chrono::steady_clock::now();
    int budget = kHostBudget;
    for (int visited = 0; visited < coop::net::kMaxPeers - 1 && budget; ++visited) {
        const int slot = g_hostCursor;
        g_hostCursor = slot + 1 < coop::net::kMaxPeers ? slot + 1 : 1;
        auto join = g_hostJoins[slot];
        if (!join) continue;
        if (!HostCurrent(join)) { if (g_hostJoins[slot] == join) g_hostJoins[slot].reset(); continue; }
        if (join->building || !HostReady(join) || join->lids.empty()) continue;
        auto lid = join->lids[join->cursor++ % join->lids.size()];
        if (lid->done || now < lid->due) continue;
        --budget;
        lid->due = now + kHostRetry;
        TryHostLid(join, lid);
        if (!HostCurrent(join) && g_hostJoins[slot] == join) g_hostJoins[slot].reset();
        if (epoch != g_hostEpoch) return;
    }
}

}  // namespace

bool SendsRestPose(const std::wstring& laneKey) {
    if (laneKey.rfind(L"mv_", 0) != 0 || laneKey.size() > 31) return false;
    for (wchar_t c : laneKey) if (c < 32 || c > 126) return false;
    return true;
}

void QueueConnectBroadcastForSlot(coop::net::Session* s, int peerSlot) {
    if (peerSlot <= 0 || peerSlot >= coop::net::kMaxPeers) return;
    UE_ASSERT_GAME_THREAD("swinger_join_pose::QueueConnectBroadcastForSlot");
    if (g_hostSession && g_hostSession != s) ClearHost();
    if (!s || s->role() != coop::net::Role::Host || !s->connected()) return;
    g_hostSession = s;
    auto join = std::make_shared<HostJoin>();
    join->session = s; join->epoch = g_hostEpoch; join->stopped = s->StopCount();
    join->serial = coop::net::session_serial::Current();
    join->world = ue_wrap::world_identity::Generation();
    join->slot = peerSlot; join->generation = s->peerGenerationForSlot(peerSlot);
    g_hostJoins[peerSlot] = join; // Publish before collection; a nested request wins.
    if (!HostCurrent(join)) { g_hostJoins[peerSlot].reset(); return; }
    std::vector<std::pair<std::wstring, void*>> lids;
    coop::interactable_sync::ContainerLids(lids);
    if (!HostCurrent(join)) return;
    std::unordered_set<std::wstring> seen;
    for (const auto& [key, actor] : lids) {
        if (!SendsRestPose(key) || join->forgotten.count(key) || !actor ||
            !ue_wrap::reflection::IsLive(actor) || !seen.insert(key).second) continue;
        auto lid = std::make_shared<HostLid>();
        lid->key = key; lid->actor.Set(actor); lid->cls.Set(ue_wrap::reflection::ClassOf(actor));
        join->lids.push_back(std::move(lid));
    }
    join->remaining = join->lids.size();
    join->building = false;
    if (!join->remaining) { if (g_hostJoins[peerSlot] == join) g_hostJoins[peerSlot].reset(); return; }
    DrainHost();
}

void OnReliable(const coop::net::SwingerRestPosePayload& p) {
    UE_ASSERT_GAME_THREAD("swinger_join_pose::OnReliable");
    const auto key=coop::net::StringFromWireKey(p.key);
    const Rest rest{{p.locX,p.locY,p.locZ},{p.rotPitch,p.rotYaw,p.rotRoll}};
    if (!SendsRestPose(key) || !Finite(rest)) return;
    if (g_pending.size()>=kPendingCap && !g_pending.count(key)) { Ended(key,false); return; }
    auto row=std::make_shared<Pending>();
    row->key=key; row->rest=rest; row->epoch=g_epoch;
    row->world=ue_wrap::world_identity::Generation();
    row->queuedAt=coop::interactable_sync::ContainerIndexPasses();
    row->deadline=std::chrono::steady_clock::now()+coop::interactable_sync::ContainerWaitBackstop();
    g_ended.erase(key);
    g_pending[key]=row; // Publish before native calls so Forget and replacement cancel this receipt.
    const Try result=TryApply(row);
    if (result!=Try::Wait) Finish(row,result==Try::Applied);
}

void Tick() {
    UE_ASSERT_GAME_THREAD("swinger_join_pose::Tick");
    DrainHost();
    if (g_pending.empty() || g_ticking) return;
    struct Pump { Pump(){g_ticking=true;} ~Pump(){g_ticking=false;} } pump;
    const auto pass=coop::interactable_sync::ContainerIndexPasses();
    const bool retry=pass!=g_lastPass;
    g_lastPass=pass;
    const auto now=std::chrono::steady_clock::now();
    std::vector<Receipt> rows;
    rows.reserve(g_pending.size());
    for(const auto& [key,row]:g_pending) rows.push_back(row);
    for(const auto& row:rows) {
        if (!Current(row)) { Finish(row,false); continue; }
        if (row->applying) continue;
        if (now>=row->deadline) { Finish(row,false); continue; }
        if (!retry) continue;
        const Try result=TryApply(row);
        if (result!=Try::Wait) Finish(row,result==Try::Applied);
        else if (Current(row) && coop::interactable_sync::ContainerWaitSettled(row->queuedAt)) Finish(row,false);
    }
}

void Forget(const std::wstring& key) {
    UE_ASSERT_GAME_THREAD("swinger_join_pose::Forget");
    const auto joins = g_hostJoins;
    for (const auto& join : joins) if (join) {
        if (join->building) join->forgotten.insert(key);
        for (const auto& lid : join->lids) if (lid->key == key) FinishHostLid(join, lid);
    }
    if (g_pending.erase(key) == 0) return;
    Ended(key, false);
    UE_LOGI("swinger_join_pose: '%ls' is streamed or released now -- the connect pose is older and is dropped",
            key.c_str());
}

namespace dev {
State Outcome(const std::wstring& key) {
    if (g_pending.count(key)) return State::Pending;
    const auto it = g_ended.find(key);
    return it == g_ended.end() ? State::None : it->second ? State::Applied : State::Dropped;
}
}  // namespace dev

void OnDisconnect() {
    ClearHost();
    ++g_epoch;
    g_pending.clear();
    g_ended.clear();
    g_lastPass = 0;
}

}  // namespace coop::swinger_join_pose
