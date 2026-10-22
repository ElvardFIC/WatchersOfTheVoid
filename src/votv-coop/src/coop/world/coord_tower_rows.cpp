// coop/world/coord_tower_rows.cpp -- see coop/world/coord_tower_rows.h.

#include "coop/world/coord_tower_rows.h"

#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/world/coord_tower_ops.h"  // the acknowledgements, and my untaken claims

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/desk/coord_tower.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>

namespace coop::coord_tower_rows {
namespace {

namespace CT = ue_wrap::coord_tower;
namespace R  = ue_wrap::reflection;
using coop::net::CoordTowerPayload;
using coop::net::CoordTowerRow;
using coop::net::kCoordTowers;

static_assert(coop::net::kMaxPeers <= 32, "the owed rows are one bit a slot");
static_assert(CT::kMaxFuses <= 8 && CT::kMaxLights <= 16, "a row holds eight fuses and sixteen lights");

// A client brings a panel to the host's with its own retract; a retract that left the panel idle (a montage
// that would not start) is asked again no sooner than this.
constexpr uint64_t kPanelRetryMs = 2000;

std::atomic<coop::net::Session*> g_session{nullptr};

coop::net::Session* Connected() {
    auto* s = g_session.load(std::memory_order_acquire);
    return (s && s->connected()) ? s : nullptr;
}

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// This world's towers, by id; re-read when one dies or the class set moves.
void*    g_towers[kCoordTowers] = {};
int32_t  g_towerCount = 0;
uint64_t g_classVersion = ~0ull;

bool Towers() {
    const uint64_t v = ue_wrap::object_index::ClassSetVersion();
    bool stale = v != g_classVersion || g_towerCount <= 0;
    for (int32_t i = 0; !stale && i < g_towerCount; ++i) stale = !R::IsLive(g_towers[i]);
    if (stale) {
        const int32_t n = CT::ReadAll(g_towers, kCoordTowers);
        g_towerCount = n < 0 ? 0 : n;
        g_classVersion = v;
    }
    return g_towerCount > 0;
}

void* TowerById(int32_t id) {
    for (int32_t i = 0; i < g_towerCount; ++i)
        if (CT::IdOf(g_towers[i]) == id) return g_towers[i];
    return nullptr;
}

bool FillRow(void* tower, CoordTowerRow& w) {
    CT::State st;
    if (!CT::Read(tower, st)) return false;
    w = CoordTowerRow{};
    w.id = st.id;
    w.flags = static_cast<uint8_t>((st.isBroken ? coop::net::kCoordTowerBroken : 0) |
                                   (st.opened ? coop::net::kCoordTowerOpened : 0) |
                                   (st.isAnim ? coop::net::kCoordTowerAnim : 0) |
                                   (st.leverMoving ? coop::net::kCoordTowerLeverMoving : 0) |
                                   (st.leverUp ? coop::net::kCoordTowerLeverUp : 0));
    w.fuseCount = st.fuseCount;
    for (int i = 0; i < st.fuseCount; ++i) w.fuses[i] = st.fuses[i];
    w.lightCount = st.lightCount;
    for (int i = 0; i < st.lightCount; ++i)
        if (st.lights[i]) w.lights = static_cast<uint16_t>(w.lights | (1u << i));
    return true;
}

bool ReadRows(CoordTowerPayload& p) {
    if (!Towers()) return false;
    p.op = coop::net::kCoordTowerOpRows;
    p.count = 0;
    for (int32_t i = 0; i < g_towerCount && p.count < kCoordTowers; ++i)
        if (FillRow(g_towers[i], p.rows[p.count])) ++p.count;
    return p.count > 0;
}

bool SameRows(const CoordTowerPayload& a, const CoordTowerPayload& b) {
    return a.count == b.count && std::memcmp(a.rows, b.rows, sizeof(CoordTowerRow) * a.count) == 0;
}

// ---- the host ------------------------------------------------------------------------------------------------

CoordTowerPayload g_lastSent{};
bool     g_haveSent = false;
bool     g_acksDirty = false;
uint32_t  g_owed = 0;
uint64_t g_broadcasts = 0;

// The rows with every slot's acknowledgement: to `onlySlot`, or to every peer when a tower or an acknowledgement
// changed since the last broadcast.
void HostSendRows(coop::net::Session* s, int onlySlot = -1) {
    CoordTowerPayload p{};
    if (!ReadRows(p)) return;
    coop::coord_tower_ops::FillAcks(p);
    if (onlySlot >= 0) {
        s->SendReliableToSlot(onlySlot, coop::net::ReliableKind::CoordTowerState, &p, sizeof(p));
        return;
    }
    if (!g_acksDirty && g_haveSent && SameRows(p, g_lastSent)) return;
    s->SendReliable(coop::net::ReliableKind::CoordTowerState, &p, sizeof(p));
    if (++g_broadcasts <= 3 || !SameRows(p, g_lastSent))
        UE_LOGI("coord_tower: host rows #%llu (%u towers)", static_cast<unsigned long long>(g_broadcasts), p.count);
    g_lastSent = p;
    g_haveSent = true;
    g_acksDirty = false;
}

void HostServeOwed(coop::net::Session* s) {
    if (!g_owed || !Towers()) return;
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot)
        if (g_owed & (1u << slot)) {
            HostSendRows(s, slot);
            UE_LOGI("coord_tower: host sent slot %d every tower's row at its world-ready", slot);
        }
    g_owed = 0;
}

// ---- a client ------------------------------------------------------------------------------------------------

CoordTowerPayload g_rows{};
bool     g_haveRows = false;
bool     g_rowsWaiting = false;
bool     g_applying = false;
bool     g_sizeWarned = false;
uint64_t g_applied = 0;
uint64_t g_panelAskedMs[kCoordTowers] = {};
Edges    g_edges;

// One tower brought to the host's row with my untaken claims on it, each change made as the tower's own graph
// makes it, so it paints, sounds and animates as the host's did. The first rows a world takes are its state, not a
// player's act: their changes make no sound of their own.
void ApplyOne(void* tower, const CoordTowerRow& host, bool live) {
    CoordTowerRow c = host;
    coop::coord_tower_ops::Overlay(c);
    CT::State l;
    if (!CT::Read(tower, l)) return;
    const bool broken = (c.flags & coop::net::kCoordTowerBroken) != 0;
    bool lights[CT::kMaxLights] = {};
    bool lightsDiffer = false;
    if (c.lightCount == l.lightCount) {
        for (int i = 0; i < c.lightCount; ++i) {
            lights[i] = ((c.lights >> i) & 1u) != 0;
            lightsDiffer = lightsDiffer || lights[i] != l.lights[i];
        }
    } else if (!g_sizeWarned) {
        // Every peer plays under the host's rules (ue_wrap/world/game_rules.h), which size the puzzle.
        g_sizeWarned = true;
        UE_LOGW("coord_tower: tower %d's puzzle has %u lights here and %u on the host -- its lights are not applied",
                c.id, l.lightCount, c.lightCount);
    }
    if (lightsDiffer) CT::WriteLights(tower, lights, c.lightCount);
    if (l.isBroken && !broken) {
        CT::SolvePuzzle(tower);  // the repair: every light lit, painted, its success sound, isBroken off, painted
        ++g_edges.repairs;
        UE_LOGI("coord_tower: tower %d repaired as the host's", c.id);
    } else if (!l.isBroken && broken) {
        CT::WriteBroken(tower, true);
        CT::UpdPuzzle(tower);
        CT::UpdBroken(tower);
        ++g_edges.breaks;
        UE_LOGI("coord_tower: tower %d broke as the host's", c.id);
    } else if (lightsDiffer) {
        CT::UpdPuzzle(tower);
        if (live) CT::Play(tower, CT::Sound::Click);  // a press, the host's route's own click
        ++g_edges.presses;
    }
    bool fusesDiffer = false, pulled = false, inserted = false;
    if (c.fuseCount == l.fuseCount) {
        for (int i = 0; i < c.fuseCount; ++i) {
            if (c.fuses[i] == l.fuses[i]) continue;
            pulled = pulled || (l.fuses[i] == 2 && c.fuses[i] == 0);
            inserted = inserted || (l.fuses[i] == 0 && c.fuses[i] == 1);
            CT::WriteFuse(tower, i, c.fuses[i]);
            fusesDiffer = true;
        }
    }
    if (fusesDiffer) {
        CT::UpdFuses(tower);
        if (live && pulled) CT::Play(tower, CT::Sound::FusePulled);
        if (live && inserted) CT::Play(tower, CT::Sound::FuseInserted);
        g_edges.pulls += pulled ? 1 : 0;
        g_edges.inserts += inserted ? 1 : 0;
    }
    const bool up = (c.flags & coop::net::kCoordTowerLeverUp) != 0;
    if (up != l.leverUp) {
        // A lever that goes down on a tower broken before and after is the judgement's fail branch: its other
        // caller, a scramble, breaks a whole tower (Scramble Radar Dish's end, and loadData, before any join).
        if (!up && l.isBroken && broken) {
            if (live) CT::Play(tower, CT::Sound::Fail);
            ++g_edges.fails;
        }
        CT::MoveLever(tower, up);
        ++g_edges.levers;
    }
    // The lever's end, which judges the puzzle, is refused here, and it is what clears the flag: the host's.
    CT::WriteLeverMoving(tower, (c.flags & coop::net::kCoordTowerLeverMoving) != 0);
}

void ApplyAll() {
    if (!Towers()) {
        g_rowsWaiting = true;
        return;
    }
    g_rowsWaiting = false;
    g_applying = true;
    const bool live = g_applied > 0;
    for (int i = 0; i < g_rows.count && i < kCoordTowers; ++i)
        if (void* t = TowerById(g_rows.rows[i].id)) ApplyOne(t, g_rows.rows[i], live);
    g_applying = false;
    ++g_applied;
}

// Each panel toward the host's: open, closed, or where its montage heads. A client's own montage runs to its end,
// which flips `opened`, before it is asked again.
void ConvergePanels() {
    if (!Towers()) return;
    void* player = coop::players::Registry::Get().Local();
    if (!player) return;
    const uint64_t now = NowMs();
    for (int i = 0; i < g_rows.count && i < kCoordTowers; ++i) {
        const CoordTowerRow& w = g_rows.rows[i];
        void* t = TowerById(w.id);
        CT::State l;
        if (!t || !CT::Read(t, l) || l.isAnim) continue;
        const bool open = (w.flags & coop::net::kCoordTowerOpened) != 0;
        const bool target = (w.flags & coop::net::kCoordTowerAnim) ? !open : open;
        if (l.opened == target || now - g_panelAskedMs[i] < kPanelRetryMs) continue;
        g_panelAskedMs[i] = now;
        g_applying = true;
        const bool ran = CT::Use(t, player, CT::LookAt{CT::Part::Retract, -1});
        g_applying = false;
        ++g_edges.panels;
        UE_LOGI("coord_tower: tower %d's panel %s as the host's (retract ran=%d)", w.id, target ? "opens" : "closes",
                ran ? 1 : 0);
    }
}

}  // namespace

void Install(coop::net::Session* session) { g_session.store(session, std::memory_order_release); }

void Tick() {
    auto* s = Connected();
    if (!s) return;
    if (s->role() == coop::net::Role::Host) {
        HostServeOwed(s);
        HostSendRows(s);
        return;
    }
    if (!g_haveRows || coop::coord_tower_ops::RedOpen()) return;
    if (g_rowsWaiting) ApplyAll();
    ConvergePanels();
}

void OnRows(const CoordTowerPayload& p) {
    const uint8_t me = coop::players::Registry::Get().LocalPeerId();
    if (me < coop::net::kMaxPeers) coop::coord_tower_ops::OnAcks(p.ack[me], p.refused[me]);
    g_rows = p;
    g_rows.count = static_cast<uint8_t>(std::min<int>(p.count, kCoordTowers));
    g_haveRows = true;
    if (coop::coord_tower_ops::RedOpen()) return;  // [dev] the control: kept for the drill's read, never applied
    ApplyAll();
}

void QueueConnectBroadcastForSlot(int slot) {
    auto* s = Connected();
    if (!s || s->role() != coop::net::Role::Host || slot <= 0 || slot >= coop::net::kMaxPeers) return;
    g_owed = static_cast<uint32_t>(g_owed | (1u << slot));
    HostServeOwed(s);
    if (g_owed & (1u << slot)) UE_LOGI("coord_tower: slot %d's rows wait for the host's towers to resolve", slot);
}

void HostSendNow() {
    g_acksDirty = true;
    if (auto* s = Connected(); s && s->role() == coop::net::Role::Host) HostSendRows(s);
}

bool Applying() { return g_applying; }

bool LastRows(CoordTowerPayload& out) {
    if (!g_haveRows) return false;
    out = g_rows;
    return true;
}

Edges AppliedEdges() { return g_edges; }

void OnDisconnect() {
    if (g_broadcasts || g_applied)
        UE_LOGI("coord_tower: session end -- rows broadcast %llu, applied %llu",
                static_cast<unsigned long long>(g_broadcasts), static_cast<unsigned long long>(g_applied));
    g_lastSent = CoordTowerPayload{};
    g_rows = CoordTowerPayload{};
    g_haveSent = g_acksDirty = g_haveRows = g_rowsWaiting = g_applying = g_sizeWarned = false;
    g_owed = 0;
    g_broadcasts = g_applied = 0;
    g_edges = Edges{};
    for (auto& t : g_panelAskedMs) t = 0;
    g_towerCount = 0;
    g_classVersion = ~0ull;
}

}  // namespace coop::coord_tower_rows
