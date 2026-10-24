// coop/dev/atv_health_drill_spawned.cpp -- the ATV health drill's `spawned` arm: see coop/dev/atv_health_drill.h. The
// host spawns an ATV of its own beside the players (on the NavMesh, a few metres ahead of its player) and says its key
// in the lobby chat once its own table holds it; the client takes the key from that line, waits for its table to show
// the ATV, walks to it with the director and sits. This increment measures the time from the host's spawn to each
// table (the host reads its own, the client's reading is a lower bound); every wait is a FAIL bound, never a clock.

#include "atv_health_drill_detail.h"  // co-located private header (src tree, not include/)

#include "atv_spawn_near.h"  // SpawnAtNav, KeyOf (co-located private header)

#include "coop/dev/drill_chat.h"
#include "coop/interactables/atv_sync.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace coop::dev::atv_health_drill::detail {

namespace {

namespace A  = ue_wrap::atv;
namespace DC = coop::dev::drill_chat;
namespace E  = ue_wrap::engine;
using coop::atv_sync::TableView;

namespace SN = coop::dev::atv_spawn_near;

constexpr float kAheadCm = 600.f;        // in front of the host's player, as atv_spawn_drill places it
constexpr float kParkedCmS = 5.f;        // an ATV is parked when two successive stamps read under this speed
constexpr auto  kStampGap = std::chrono::milliseconds(250);   // the distance between two stamps, not a wait

// The host's line that carries the key: "ATVHD host spawned <key>".
constexpr const char* kSpawnedPrefix = "ATVHD host spawned ";

float Dist(const ue_wrap::FVector& a, const ue_wrap::FVector& b) {
    const float dx = a.X - b.X, dy = a.Y - b.Y, dz = a.Z - b.Z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ---- the host ----

enum class HPh : int { Wait, Spawn, Seen, Sat };

struct SHost {
    Run                   run;
    ue_wrap::CachedObjRef atv;
    bool                  spawned = false, readySeen = false, satSeen = false;
    Clock::time_point     spawnAt{}, tableAt{}, parkedAt{};
    std::wstring          key;
    ue_wrap::FVector      lastAt{};        // the last stamp's root and when it was read
    Clock::time_point     lastStampAt{};
    int                   calm = 0;        // successive stamp pairs under kParkedCmS
};
SHost g_sh;

void HostPhase(HPh p, const char* name) { Enter(g_sh.run, static_cast<int>(p), name); }

void HostFail(Arm a, const std::string& what, const std::string& got, const std::string& want) {
    Fail("host", a, g_sh.run, what, got, want);
}

void HostWait(Arm a) {
    if (!g_sh.readySeen && DC::PollDue()) g_sh.readySeen = DC::ChatHasLive(ReadyLine(a));
    if (g_sh.readySeen) return HostPhase(HPh::Spawn, "spawn");
    if (Past(g_sh.run, kFailBound)) HostFail(a, "the client never said it was ready", "no line", ReadyLine(a));
}

// The ATV, born a few metres ahead of the host's player on the NavMesh. A point the NavMesh does not project fails the
// arm: the run says whether a point near the base start always projects.
bool HostSpawnAtv(Arm a) {
    void* me = coop::players::Registry::Get().Local();
    ue_wrap::FVector at{};
    if (!me || !E::TryGetActorLocation(me, at)) {
        HostFail(a, "the host's player does not read", "no location", "a location");
        return false;
    }
    const ue_wrap::FVector fwd = E::GetActorForwardVector(me);
    const ue_wrap::FVector point{at.X + fwd.X * kAheadCm, at.Y + fwd.Y * kAheadCm, at.Z + SN::kLiftCm};
    g_sh.spawnAt = Clock::now();
    SN::Spawned sp;
    if (!SN::SpawnAtNav(me, point, sp)) {
        HostFail(a, sp.what, sp.got, sp.want);
        return false;
    }
    UE_LOGI("[ATV-HEALTH-DRILL] host arm=spawned the point is on the NavMesh at (%.0f,%.0f,%.0f)", sp.onMesh.X,
            sp.onMesh.Y, sp.onMesh.Z + SN::kLiftCm);
    g_sh.atv.Set(sp.actor);
    g_sh.spawned = true;
    return true;
}

// Once the host's own table holds the ATV under an announceable key: the key is said, with the ms since the spawn call.
void HostSpawn(Arm a) {
    if (!g_sh.spawned && !HostSpawnAtv(a)) return;
    void* atv = g_sh.atv.Get();
    if (!atv) return HostFail(a, "the spawned ATV is gone before its key", "no ATV", "the spawned ATV");
    std::wstring key;
    const char* why = nullptr;
    const SN::Key k = SN::KeyOf(atv, &key, &why);
    if (k == SN::Key::Bad) return HostFail(a, "the spawned ATV's key cannot ride a chat line", why, "plain ASCII");
    if (k == SN::Key::Ok) {
        g_sh.tableAt = Clock::now();
        g_sh.key = key;
        std::string line = kSpawnedPrefix;
        for (const wchar_t c : key) line += static_cast<char>(c);
        DC::Say(line);
        UE_LOGI("[ATV-HEALTH-DRILL] host arm=spawned the spawned ATV is in the host's table as '%ls' after %lld ms",
                key.c_str(), Ms(g_sh.spawnAt, g_sh.tableAt));
        return HostPhase(HPh::Seen, "seen");
    }
    if (Past(g_sh.run, kFailBound)) HostFail(a, "the spawned ATV never entered the host's table", "no table row", "a row");
}

// Parked: two successive stamps of the root, a gap apart, each pair under kParkedCmS. The lane's block speed is the
// client syncer's, so a host-owned ATV reads its own root.
void HostSeen(Arm a) {
    const Clock::time_point now = Clock::now();
    const bool first = g_sh.lastStampAt == Clock::time_point{};
    if (!first && now - g_sh.lastStampAt < kStampGap) {   // between stamps: no engine call, only the bound
        if (Past(g_sh.run, kFailBound))
            HostFail(a, "the spawned ATV never came to rest", "still moving", "two stamps under 5 cm/s");
        return;
    }
    void* atv = g_sh.atv.Get();
    ue_wrap::FVector at{};
    if (!atv || !E::TryGetActorLocation(atv, at))
        return HostFail(a, "the spawned ATV is gone before it parked", "no ATV", "the spawned ATV");
    if (!first) {
        const float secs = static_cast<float>(Ms(g_sh.lastStampAt, now)) / 1000.f;
        g_sh.calm = Dist(at, g_sh.lastAt) / secs < kParkedCmS ? g_sh.calm + 1 : 0;
    }
    g_sh.lastAt = at;
    g_sh.lastStampAt = now;
    if (g_sh.calm >= 2) {
        g_sh.parkedAt = now;
        DC::Say(PhaseLine(SpecOf(a).first));
        return HostPhase(HPh::Sat, "sat");
    }
    if (Past(g_sh.run, kFailBound))
        HostFail(a, "the spawned ATV never came to rest", "still moving", "two stamps under 5 cm/s");
}

void HostSat(Arm a) {
    if (!g_sh.satSeen && DC::PollDue()) g_sh.satSeen = DC::ChatHasLive(ClientLine("sat"));
    if (g_sh.satSeen) {
        char nums[96];
        std::snprintf(nums, sizeof(nums), "spawn_to_host_table_ms=%lld parked_ms=%lld", Ms(g_sh.spawnAt, g_sh.tableAt),
                      Ms(g_sh.tableAt, g_sh.parkedAt));
        return Pass("host", a, g_sh.run, nums);
    }
    if (Past(g_sh.run, kHostFirstBound)) HostFail(a, "the client never sat in the spawned ATV", "no line", ClientLine("sat"));
}

// ---- the client ----

enum class CPh : int { Wait, Adopt, Walk, Sit };

struct SClient : SeatedClient {
    bool              readySaid = false;
    std::wstring      key;                  // the host's ATV key, from its chat line
    Clock::time_point lineAt{}, seenAt{};   // the host's key line seen, and the table showing the ATV
};
SClient g_sc;

void ClientPhase(CPh p, const char* name) { Enter(g_sc.run, static_cast<int>(p), name); }

void ClientEnd() { WalkCancel(g_sc); }

void ClientFail(Arm a, const std::string& what, const std::string& got, const std::string& want) {
    Fail("client", a, g_sc.run, what, got, want);
    ClientEnd();
}

// The key in the LAST live line that starts with the host's spawn prefix, or false. The retained tier is a record of an
// earlier life and never counts.
bool HostKeyFromChat(std::wstring* out) {
    std::string rest;
    if (!DC::LiveRestOf(kSpawnedPrefix, &rest)) return false;
    out->assign(rest.begin(), rest.end());
    return true;
}

// The client says it is ready once its streams run, then waits for the host's key line.
void ClientWait(Arm a) {
    SClient& c = g_sc;
    if (!c.readySaid) {
        if (StreamsRun()) {
            c.readySaid = true;
            DC::Say(ReadyLine(a));
        }
    } else if (DC::PollDue() && HostKeyFromChat(&c.key)) {
        c.lineAt = Clock::now();
        UE_LOGI("[ATV-HEALTH-DRILL] client arm=spawned the host's ATV key is '%ls'", c.key.c_str());
        return ClientPhase(CPh::Adopt, "adopt");
    }
    if (Past(c.run, kHostFirstBound))
        ClientFail(a, "the host never said the spawned ATV's key", "no line", std::string(kSpawnedPrefix) + "<key>");
}

// The table shows the ATV under the host's key. The ms from the client's sight of the host's key line to here is a LOWER
// bound on the spawn to this table (the clock starts after the key crossed the chat); the host's own
// spawn_to_host_table_ms is the spawn-to-table number.
void ClientAdopt(Arm a) {
    SClient& c = g_sc;
    TableView t;
    ue_wrap::FRotator rot{};
    AtvAt at;
    if (coop::atv_sync::ReadTable(c.key, t) && A::GetRootTransform(t.actor, at.at, rot)) {
        at.actor = t.actor;
        c.seenAt = Clock::now();
        UE_LOGI("[ATV-HEALTH-DRILL] client arm=spawned the table shows the spawned ATV %lld ms after the host's key line "
                "(a lower bound on the spawn to the table: the clock starts after the key crossed the chat)",
                Ms(c.lineAt, c.seenAt));
        DC::Say(ClientLine("saw"));
        WalkStart(c, a, at, kSitReachCm);
        return ClientPhase(CPh::Walk, "walk");
    }
    if (Past(c.run, kFailBound))
        ClientFail(a, "the spawned ATV never entered the client's table", "no table row", "a row under the host's key");
}

void ClientWalk(Arm a) {
    const Stage s = WalkStep(g_sc, a);
    if (s == Stage::Failed) return ClientEnd();
    if (s == Stage::Next) ClientPhase(CPh::Sit, "sit");
}

void ClientSit(Arm a, void* player, uint8_t self) {
    SClient& c = g_sc;
    void* atv = c.atv.Get();
    if (!atv) return ClientFail(a, "the spawned ATV is gone before the sit", "no ATV", "the spawned ATV");
    const Stage s = SitStep(c, a, player, atv, self);
    if (s == Stage::Failed) return ClientEnd();
    if (s == Stage::Wait) return;
    ue_wrap::FVector me{}, root{};
    ue_wrap::FRotator rot{};
    const bool read = E::TryGetActorLocation(player, me) && A::GetRootTransform(atv, root, rot);
    char nums[128];
    std::snprintf(nums, sizeof(nums), "spawn_to_table_ms_lower=%lld walk_ms=%lld sit_cm=%.0f", Ms(c.lineAt, c.seenAt),
                  Ms(c.walkAt, c.arrivedAt), read ? Dist(me, root) : -1.f);
    DC::Say(ClientLine("sat"));
    Pass("client", a, c.run, nums);
    ClientEnd();
}

}  // namespace

void SpawnedHostTick(coop::net::Session* s, Arm a) {
    SHost& h = g_sh;
    if (h.run.done) return;
    if (h.run.at == Clock::time_point{}) {   // the bounds run from the edge of slot 1 being in the world
        if (!s->IsSlotWorldReady(kSlot)) return;
        HostPhase(HPh::Wait, "wait");
    }
    switch (static_cast<HPh>(h.run.phase)) {
    case HPh::Wait:  return HostWait(a);
    case HPh::Spawn: return HostSpawn(a);
    case HPh::Seen:  return HostSeen(a);
    case HPh::Sat:   return HostSat(a);
    }
}

void SpawnedClientTick(coop::net::Session*, Arm a) {
    SClient& c = g_sc;
    void* player = nullptr;
    uint8_t self = 0;
    if (!SeatedActs(c, &player, &self)) return;
    if (c.run.at == Clock::time_point{}) ClientPhase(CPh::Wait, "wait");   // the bounds run from the world-ready edge
    switch (static_cast<CPh>(c.run.phase)) {
    case CPh::Wait:  return ClientWait(a);
    case CPh::Adopt: return ClientAdopt(a);
    case CPh::Walk:  return ClientWalk(a);
    case CPh::Sit:   return ClientSit(a, player, self);
    }
}

void SpawnedReset() {
    ClientEnd();
    g_sh = SHost{};
    g_sc = SClient{};
}

}  // namespace coop::dev::atv_health_drill::detail
