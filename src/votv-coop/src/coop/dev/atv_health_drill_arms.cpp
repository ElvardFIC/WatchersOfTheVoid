// coop/dev/atv_health_drill_arms.cpp -- the ATV health drill's arms where the client acts, `sit`, `grab` and `silent`,
// and the client's side of `occupied` and `synced`, for both roles: see coop/dev/atv_health_drill.h. The client walks
// to the world's own ATV with the director and acts on it (the seat through the game's own verb, the grav hand's grab,
// or a raw request) and undoes it on the host's word; the host reads the table it judged and the lane's counters as
// deltas against a baseline taken at the client's ready line, says each edge in the lobby chat, and prints the times
// the hand-back took. The host's side of `occupied` and `synced` is atv_health_drill_host.cpp.

#include "atv_health_drill_detail.h"  // co-located private header (src tree, not include/)
#include "atv_seat.h"                 // TrySit (co-located private header)

#include "coop/dev/director/aimed_grab.h"
#include "coop/dev/director/director.h"
#include "coop/dev/drill_chat.h"
#include "coop/interactables/atv_authority.h"
#include "coop/interactables/atv_sync.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"
#include "coop/props/join_membership_sweep.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine_mainplayer.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace coop::dev::atv_health_drill::detail {

Host g_h;

std::string Plus(uint64_t n) { return "+" + std::to_string(n); }

// The grab's failure: why, and which calls of the use key's chain ran.
std::string GrabWhy(const coop::director::AimedGrab& g) {
    char what[200];
    std::snprintf(what, sizeof(what), "the grab did not take: %s, chain pre/use/post %d/%d/%d", g.Why(),
                  g.ChainPre() ? 1 : 0, g.ChainUse() ? 1 : 0, g.ChainPost() ? 1 : 0);
    return what;
}

namespace {

namespace A  = ue_wrap::atv;
namespace AU = coop::atv_authority;
namespace DC = coop::dev::drill_chat;
namespace E  = ue_wrap::engine;
using coop::atv_sync::TableView;

unsigned long long U(uint64_t v) { return static_cast<unsigned long long>(v); }

// ---- the host ----

uint64_t Admitted() { return AU::ReadCounters().admitted - g_h.base.admitted; }
uint64_t HandedBack() { return AU::ReadCounters().handedBack - g_h.base.handedBack; }

// The times the hand-back took, then the stream's cost: the idle blocks admitted after the act and the speed of the
// last block, the numbers the next increments are written on.
std::string Numbers(Arm a, Clock::time_point end, float lastBlockSpeed) {
    char buf[200];
    int  n = 0;
    if (a == Arm::Sit)
        n = std::snprintf(buf, sizeof(buf), "seated_ms=%lld leave_to_back_ms=%lld ", Ms(g_h.firstAt, g_h.leftAt),
                          Ms(g_h.leftAt, end));
    else
        n = std::snprintf(buf, sizeof(buf), "release_to_back_ms=%lld ", Ms(g_h.firstAt, end));
    std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "idle_stream_blocks=%llu last_block_speed=%.2f",
                  U(AU::ReadCounters().idleStreamBlocks - g_h.base.idleStreamBlocks), lastBlockSpeed);
    return buf;
}

void HostWait(Arm a) {
    if (DC::PollDue() && DC::ChatHasLive(ReadyLine(a))) {
        g_h.base = AU::ReadCounters();
        if (a == Arm::Occupied || a == Arm::Synced) return HostEnter(HP::Walk, "walk");
        return HostEnter(HP::First, SpecOf(a).first);
    }
    if (Past(g_h.run, kFailBound))
        Fail("host", a, g_h.run, "the client never said it was ready", "no line", ReadyLine(a));
}

// The edge the client's act makes: the sit (and the silent arm's) names the client occupant and syncer, the grab names
// it syncer with the seat free (and `admitted` +1, the takeover). The host then says the edge, which the client undoes
// its act on.
void HostFirst(Arm a, const TableView* t) {
    const bool sit = a == Arm::Sit || a == Arm::Silent;
    if (t && t->syncerSlot == kSlot && t->occupantSlot == (sit ? kSlot : kNoSeat)) {
        if (!sit && Admitted() != 1)
            return Fail("host", a, g_h.run, "the grant", "admitted " + Plus(Admitted()), "admitted +1");
        g_h.firstAt = Clock::now();
        DC::Say(PhaseLine(SpecOf(a).first));
        if (a == Arm::Silent) return HostEnter(HP::Silenced, "silenced");
        return HostEnter(sit ? HP::Left : HP::Back, sit ? "left" : "back");
    }
    if (!Past(g_h.run, kHostFirstBound)) return;
    char got[64];
    std::snprintf(got, sizeof(got), "occupant %u syncer %u", t ? static_cast<unsigned>(t->occupantSlot) : 0xFFFFu,
                  t ? static_cast<unsigned>(t->syncerSlot) : 0xFFFFu);
    Fail("host", a, g_h.run, sit ? "the table never named the client occupant and syncer"
                                 : "the table never named the client syncer with the seat free",
         t ? got : "no table", sit ? "occupant 1 syncer 1" : "occupant 255 syncer 1");
}

// The leave: the seat is free again, the sit and the leave being the two admitted requests.
void HostLeft(Arm a, const TableView* t) {
    if (t && t->occupantSlot == kNoSeat) {
        if (Admitted() != 2)
            return Fail("host", a, g_h.run, "the sit and the leave", "admitted " + Plus(Admitted()), "admitted +2");
        g_h.leftAt = Clock::now();
        return HostEnter(HP::Back, "back");
    }
    if (Past(g_h.run, kFailBound))
        Fail("host", a, g_h.run, "the seat was never freed", t ? "occupant " + std::to_string(t->occupantSlot) : "no table",
             "occupant 255");
}

// The ATV back with the host after the act is undone: the syncer is the host and the lane counted one hand-back at rest.
// A red that skips the hand-back leaves the client the syncer, which ends here at the bound.
void HostBack(Arm a, const TableView* t) {
    const Clock::time_point now = Clock::now();
    if (t && t->syncerSlot == 0) {
        if (HandedBack() != 1)
            return Fail("host", a, g_h.run, "the ATV went back to the host by no at-rest hand-back",
                        "handedBack " + Plus(HandedBack()), "handedBack +1");
        DC::Say(PhaseLine("back"));
        return Pass("host", a, g_h.run, Numbers(a, now, t->lastBlockSpeed));
    }
    if (!Past(g_h.run, kFailBound)) return;
    Fail("host", a, g_h.run, "the ATV was not handed back to the host at rest",
         "handedBack " + Plus(HandedBack()) + " syncer " + (t ? std::to_string(t->syncerSlot) : "?") + ", " +
             Numbers(a, now, t ? t->lastBlockSpeed : 0.f),
         "handedBack +1 syncer 0");
}

// The silent syncer: from the client's `muted` line the lane must take the seat and the ATV back by its silence rule
// (the seat free, the host the syncer, one `silenced`); the bound runs from the muting.
void HostSilenced(Arm a, const TableView* t) {
    if (!g_h.sawMuted) {
        if (DC::PollDue() && DC::ChatHasLive(ClientLine("muted"))) {
            g_h.sawMuted = true;
            g_h.mutedAt = Clock::now();
            return HostEnter(HP::Silenced, "silenced");
        }
        if (Past(g_h.run, kFailBound))
            Fail("host", a, g_h.run, "the client never said it muted its stream", "no line", ClientLine("muted"));
        return;
    }
    const uint64_t silenced = AU::ReadCounters().silenced - g_h.base.silenced;
    if (t && t->occupantSlot == kNoSeat && t->syncerSlot == 0) {
        if (silenced != 1)
            return Fail("host", a, g_h.run, "the seat and the ATV went back to the host by no silence rule",
                        "silenced " + Plus(silenced), "silenced +1");
        g_h.silencedAt = Clock::now();
        return HostEnter(HP::Yielded, "yielded");
    }
    if (!Past(g_h.run, kFailBound)) return;
    char got[96];
    std::snprintf(got, sizeof(got), "occupant %u syncer %u silenced +%llu",
                  t ? static_cast<unsigned>(t->occupantSlot) : 0xFFFFu, t ? static_cast<unsigned>(t->syncerSlot) : 0xFFFFu,
                  U(silenced));
    Fail("host", a, g_h.run, "the silent syncer was not taken back", t ? got : "no table",
         "occupant 255 syncer 0 silenced +1");
}

// The client's own copy gives up the seat on the host's stamp: its `yielded` line ends the run.
void HostYielded(Arm a) {
    if (DC::PollDue() && DC::ChatHasLive(ClientLine("yielded"))) {
        DC::Say(PhaseLine("back"));
        char nums[64];
        std::snprintf(nums, sizeof(nums), "mute_to_silenced_ms=%lld", Ms(g_h.mutedAt, g_h.silencedAt));
        return Pass("host", a, g_h.run, nums);
    }
    if (Past(g_h.run, kFailBound))
        Fail("host", a, g_h.run, "the client's own copy never gave up the seat", "no line", ClientLine("yielded"));
}

// ---- the client ----

enum class CP : int { Wait, Walk, Act, Undo, Yield, Back };

// The raw requests' numbers. Honest requests cycle NextSeq 1..255, so 250 and 251 are free only on the drill's
// fresh world, where this client sends a handful of requests before the raw one: the drill's contract.
constexpr uint8_t kSeatSeq = 250, kTakeoverSeq = 251;

struct Client {
    Run                                             run;
    bool                                            idleSaid = false;
    bool                                            sawFirst = false, sawBack = false;  // the host's edges, latched
    ue_wrap::CachedObjRef                           atv;
    std::shared_ptr<coop::director::BackgroundWalk> walk;
    atv_seat::SitTry                                sit;
    std::unique_ptr<coop::director::AimedGrab>      grab;
    Clock::time_point                               walkAt{}, arrivedAt{}, nextTry{};
    Clock::time_point                               rawFrom{};   // the raw send's first try: its bound counts from here
};
Client g_c;

void ClientEnter(CP p, const char* name) { Enter(g_c.run, static_cast<int>(p), name); }

// A verdict ends the run: a walk still walking is cancelled, the grab let go of and the stream's mute lifted.
void ClientEnd() {
    if (g_c.walk && g_c.walk->state.load() == 0) coop::director::CancelBackgroundWalk(*g_c.walk);
    g_c.walk.reset();
    g_c.grab.reset();
    coop::atv_sync::SetDrillMute(false);
}

void ClientFail(Arm a, const std::string& what, const std::string& got, const std::string& want) {
    Fail("client", a, g_c.run, what, got, want);
    ClientEnd();
}

// The client acts once its own streams run, as a player in a settled world does.
bool StreamsRun() {
    return coop::net_pump::HasAnnouncedWorldReady() &&
           coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle &&
           coop::net_pump::IsWorldSettled() && coop::join_membership_sweep::HasLoadTailQuiesced();
}

// Once the streams run and the table holds the ATV: the director's walk to its root, and the host told.
void ClientWait(Arm a) {
    TableView t;
    ue_wrap::FVector at{};
    ue_wrap::FRotator rot{};
    if (StreamsRun() && coop::atv_sync::ReadTable(kKey, t) && A::GetRootTransform(t.actor, at, rot)) {
        g_c.atv.Set(t.actor);
        const float reach = a == Arm::Grab ? kGrabReachCm : kSitReachCm;
        g_c.walk = coop::director::StartBackgroundWalk(at, reach, kWalkDeadlineS);
        g_c.walkAt = Clock::now();
        DC::Say(ReadyLine(a));
        UE_LOGI("[ATV-HEALTH-DRILL] client arm=%s the ATV is at (%.0f,%.0f,%.0f), walking to it", SpecOf(a).name,
                at.X, at.Y, at.Z);
        return ClientEnter(CP::Walk, "walk");
    }
    if (Past(g_c.run, kFailBound))
        ClientFail(a, "the client's streams or the ATV's table never held", "not ready", "streams running, ATV indexed");
}

void ClientWalk(Arm a, void* player) {
    void* atv = g_c.atv.Get();
    if (!atv) return ClientFail(a, "the ATV is gone before the walk ended", "no ATV", "the world's ATV");
    const int state = g_c.walk->state.load();
    if (state == 2) return ClientFail(a, "the director's walk to the ATV did not arrive", "walk failed", "arrived");
    if (state != 1) return;
    g_c.arrivedAt = Clock::now();
    if (a == Arm::Grab) g_c.grab = std::make_unique<coop::director::AimedGrab>(player, atv);
    ClientEnter(CP::Act, SpecOf(a).act);
}

// The seat through the game's own verb, then the host's grant: the seat is the host's table, so one check follows.
void ClientSit(Arm a, void* player, void* atv, uint8_t self) {
    if (!atv_seat::TrySit(player, atv, g_c.sit)) {
        if (Past(g_c.run, kFailBound)) ClientFail(a, "the sit never finished", "no grant", "seated");
        return;
    }
    if (coop::atv_sync::ActorSeatedBy(self) != atv)
        return ClientFail(a, "the sit did not take (see the [ATVP] sit lines)", "not seated", "seated");
    ClientEnter(CP::Undo, SpecOf(a).undo);
}

// The grav hand's grab until the player's own grabbing actor is the ATV.
void ClientGrab() {
    switch (g_c.grab->Tick()) {
    case coop::director::GrabState::Working:
        if (Past(g_c.run, kFailBound)) ClientFail(Arm::Grab, "the grab never finished", "working", "grabbed");
        return;
    case coop::director::GrabState::Failed:
        return ClientFail(Arm::Grab, GrabWhy(*g_c.grab), "failed", "grabbed");
    case coop::director::GrabState::Grabbed:
        g_c.grab.reset();
        return ClientEnter(CP::Undo, SpecOf(Arm::Grab).undo);
    }
}

// On the host's edge line, the act undone: the dismount, or the grav hand's release, tried at most once a second until
// the seat or the grab is gone.
void ClientUndo(Arm a, void* player, void* atv, uint8_t self) {
    if (!g_c.sawFirst) {
        if (Past(g_c.run, kFailBound))
            ClientFail(a, "the host never said its edge", "no line", PhaseLine(SpecOf(a).first));
        return;
    }
    const bool sit = a == Arm::Sit;
    if (sit ? coop::atv_sync::ActorSeatedBy(self) != atv : !E::IsMainPlayerGrabbing(player, atv))
        return ClientEnter(CP::Back, "back");
    if (Past(g_c.run, kFailBound))
        return ClientFail(a, sit ? "the dismount never freed the seat" : "the release never let go",
                          sit ? "seated" : "grabbing", sit ? "not seated" : "not grabbing");
    const Clock::time_point now = Clock::now();
    if (now < g_c.nextTry) return;
    g_c.nextTry = now + std::chrono::seconds(1);
    if (sit) { if (A::IsDriven(atv)) A::Dismount(atv); }
    else E::ReleaseMainPlayerGrabIfHolding(player, atv);
}

// The silent arm: on the host's edge line the stream goes quiet, and the client's own copy must then give up the seat
// by the host's stamp (the table's occupant alone does not say so; the body does).
void ClientMute(Arm a) {
    if (!g_c.sawFirst) {
        if (Past(g_c.run, kFailBound))
            ClientFail(a, "the host never said its edge", "no line", PhaseLine(SpecOf(a).first));
        return;
    }
    coop::atv_sync::SetDrillMute(true);
    DC::Say(ClientLine("muted"));
    UE_LOGI("[ATV-HEALTH-DRILL] client arm=%s the stream is muted", SpecOf(a).name);
    ClientEnter(CP::Yield, "yield");
}

void ClientYield(Arm a) {
    TableView t;
    const bool read = coop::atv_sync::ReadTable(kKey, t);
    if (read && !t.localSeated) {
        DC::Say(ClientLine("yielded"));
        return ClientEnter(CP::Back, "back");
    }
    if (Past(g_c.run, kFailBound))
        ClientFail(a, "the client's own copy never gave up the seat", read ? "seated" : "no table", "not seated");
}

// The arms where the host acts: one raw request once the host says its edge, retried once a second until the send took.
// The client's local verb may refuse first, so the host's judgement is what is under test: an AtvSeat for the seat the
// host holds, or an AtvTakeover of the ATV the host carries.
void ClientRaw(coop::net::Session* s, Arm a) {
    if (!g_c.sawFirst) {
        if (Past(g_c.run, kHostFirstBound))
            ClientFail(a, "the host never said its edge", "no line", PhaseLine(SpecOf(a).first));
        return;
    }
    const Clock::time_point now = Clock::now();
    if (now < g_c.nextTry) return;
    g_c.nextTry = now + std::chrono::seconds(1);
    if (g_c.rawFrom == Clock::time_point{}) g_c.rawFrom = now;
    if (now - g_c.rawFrom > kFailBound) return ClientFail(a, "the raw request never went out", "no send", "sent");
    bool sent = false;
    if (a == Arm::Occupied) {
        coop::net::AtvSeatPayload p{};
        coop::net::WireKeyFromString(kKey, p.key);
        p.seat = 0;
        p.action = 1;
        p.seq = kSeatSeq;
        sent = s->SendReliable(coop::net::ReliableKind::AtvSeat, &p, sizeof(p));
    } else {
        coop::net::AtvTakeoverPayload p{};
        coop::net::WireKeyFromString(kKey, p.key);
        p.seq = kTakeoverSeq;
        sent = s->SendReliable(coop::net::ReliableKind::AtvTakeover, &p, sizeof(p));
    }
    if (!sent) return;
    DC::Say(ClientLine("sent"));
    UE_LOGI("[ATV-HEALTH-DRILL] client arm=%s sent its raw request", SpecOf(a).name);
    ClientEnter(CP::Back, "back");
}

void ClientBack(Arm a) {
    if (g_c.sawBack) {
        char nums[64];
        std::snprintf(nums, sizeof(nums), "walk_ms=%lld", Ms(g_c.walkAt, g_c.arrivedAt));
        Pass("client", a, g_c.run, nums);
        return ClientEnd();
    }
    if (Past(g_c.run, kFailBound))
        ClientFail(a, "the host never said it had the ATV back", "no line", PhaseLine("back"));
}

}  // namespace

void HostTick(coop::net::Session* s, Arm a) {
    if (g_h.run.done) return;
    if (g_h.run.at == Clock::time_point{}) {   // the bounds run from the edge of slot 1 being in the world
        if (!s->IsSlotWorldReady(kSlot)) return;
        HostEnter(HP::Wait, "wait");
    }
    if (static_cast<HP>(g_h.run.phase) == HP::Wait) return HostWait(a);
    TableView t;
    const bool read = coop::atv_sync::ReadTable(kKey, t);
    const TableView* view = read ? &t : nullptr;
    switch (static_cast<HP>(g_h.run.phase)) {
    case HP::First: return HostFirst(a, view);
    case HP::Left:  return HostLeft(a, view);
    case HP::Back:  return HostBack(a, view);
    case HP::Silenced: return HostSilenced(a, view);
    case HP::Yielded:  return HostYielded(a);
    case HP::Walk:
    case HP::Act:
    case HP::Judge: return ActHostTick(a, view);
    case HP::Wait:  return;
    }
}

void ClientTick(coop::net::Session* s, Arm a) {
    if (g_c.run.done) return;
    const uint8_t self = coop::players::Registry::Get().LocalPeerId();
    void* player = coop::players::Registry::Get().Local();
    if (!coop::net_pump::HasAnnouncedWorldReady() || !player || self == 0 || self >= coop::players::kMaxPeers) return;
    if (self != kSlot) {
        if (!g_c.idleSaid) {
            g_c.idleSaid = true;
            UE_LOGI("[ATV-HEALTH-DRILL] client idles: this client is slot %u, the drill acts on slot %u",
                    static_cast<unsigned>(self), static_cast<unsigned>(kSlot));
        }
        return;
    }
    if (g_c.run.at == Clock::time_point{}) ClientEnter(CP::Wait, "wait");   // the bounds run from the world-ready edge
    // The host's edge lines, latched on sight: a live chat row retires after seconds.
    if (DC::PollDue()) {
        if (!g_c.sawFirst) g_c.sawFirst = DC::ChatHasLive(PhaseLine(SpecOf(a).first));
        if (!g_c.sawBack) g_c.sawBack = DC::ChatHasLive(PhaseLine("back"));
    }
    void* atv = g_c.atv.Get();
    switch (static_cast<CP>(g_c.run.phase)) {
    case CP::Wait: return ClientWait(a);
    case CP::Walk: return ClientWalk(a, player);
    case CP::Act:
        if (!atv) return ClientFail(a, "the ATV is gone before the act", "no ATV", "the world's ATV");
        if (a == Arm::Grab) return ClientGrab();
        if (a == Arm::Occupied || a == Arm::Synced) return ClientRaw(s, a);
        return ClientSit(a, player, atv, self);
    case CP::Undo:
        if (a == Arm::Silent) return ClientMute(a);
        if (!atv) return ClientFail(a, "the ATV is gone before the undo", "no ATV", "the world's ATV");
        return ClientUndo(a, player, atv, self);
    case CP::Yield: return ClientYield(a);
    case CP::Back: return ClientBack(a);
    }
}

void HostReset() {
    HostEnd();
    g_h = Host{};
}

void ClientReset() {
    ClientEnd();
    g_c = Client{};
}

}  // namespace coop::dev::atv_health_drill::detail
