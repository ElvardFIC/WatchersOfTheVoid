// coop/dev/sleepingbag_drill.cpp -- see coop/dev/sleepingbag_drill.h.

#include "coop/dev/sleepingbag_drill.h"

#include "coop/comms/chat_feed.h"
#include "coop/comms/chat_log.h"
#include "coop/comms/chat_sync.h"
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/director/director.h"
#include "coop/dev/director/routes.h"
#include "coop/dev/drill_chat_pace.h"
#include "coop/dev/drill_props.h"
#include "coop/element/element.h"
#include "coop/element/registry.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/player/puppet_drive.h"
#include "coop/player/remote_player.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/actors/prop_use.h"
#include "ue_wrap/actors/sleepingbag.h"
#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_attach.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coop::dev::sleepingbag_drill {
namespace {

namespace E  = ue_wrap::engine;
namespace EL = coop::element;
namespace OI = ue_wrap::object_index;
namespace R  = ue_wrap::reflection;
namespace SB = ue_wrap::sleepingbag;
namespace PU = ue_wrap::prop_use;
namespace DP = coop::dev::drill_props;
namespace D  = coop::director;
using ue_wrap::FVector;

constexpr const wchar_t* kBagPath = L"/Game/objects/prop_sleepingbag.prop_sleepingbag_C";
constexpr uint8_t kActUnroll = 4;  // actionOptionIndex: the packed bag's only action, and the deployed bag's first
constexpr uint8_t kActFold   = 7;  // actionOptionIndex: the deployed bag's second, which calls fold()
constexpr uint8_t kActTake   = 7;  // actionOptionIndex: the bunk mattress's second, which takes it off
constexpr int     kClientSlot = 1;
constexpr const wchar_t* kBunkPath = L"/Game/objects/bed_bunkMattress.bed_bunkMattress_C";

constexpr char kReadyHead[]      = "[BAG-DRILL] host ready eid=";
constexpr char kBunkReadyHead[]  = "[BAG-DRILL] host bunk ready eid=";
constexpr char kLineFoldSeen[]   = "[BAG-DRILL] c1 fold seen";
constexpr char kLineUnrollSeen[] = "[BAG-DRILL] c1 host-unroll seen";

// A column's distance from the client's puppet: the near ones first, the far ones for a leg whose nearer floor is
// already taken by the family census (an earlier leg's bag lies within kFamilyCm of it).
constexpr float kAheadCm[]   = {200.f, 150.f, 250.f, 500.f, 700.f, 900.f};
constexpr float kFanDeg[]    = {0.f, 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f};
constexpr float kDropCm      = 40.f;   // above the floor, so the bag settles
constexpr float kFloorDownCm = 400.f;
constexpr float kMinColumnCm = 20.f;
constexpr float kReachCm     = 150.f;  // a route must end this near the bag; the director's stop; the host's reach is 450
constexpr float kFamilyCm    = 300.f;  // every packed, deployed or bunk form within this of the drill's source is counted
constexpr float kBaselineMarginCm = 150.f;  // a column is judged empty over this much more than the census reads
constexpr float kStandWorldS = 5.f;    // a deployed bag must still stand this long after its birth on a copy
constexpr int   kWalkDeadlineS = 300;
// Failure bounds only: each leg ends on the state it waits for, and these say it never came.
constexpr uint64_t kCheckEveryMs    = 250;
constexpr uint64_t kSpawnBoundMs    = 60000;   // HOST: a floor with a route round the puppet
constexpr uint64_t kEidBoundMs      = 30000;   // HOST: the dropped bag takes its element id
constexpr uint64_t kSeenBoundMs     = 120000;  // CLIENT: the host's ready line and the bag's mirror
constexpr uint64_t kClientLegBoundMs = 480000; // HOST: the client's walk (300 s) and its two presses
constexpr uint64_t kLegBoundMs      = 60000;   // one change crossing, or the other peer's word
constexpr uint64_t kClockBoundMs    = 30000;   // CLIENT: the world clock must move the stand's span within this

bool Enabled() {
    static const bool on =
        coop::config::ResolveString(::coop::config_registry::rows::sleepingbag_drill) == "run";
    return on;
}

uint64_t NowMs() { return ::GetTickCount64(); }
bool Due(uint64_t& next, uint64_t every) {
    const uint64_t now = NowMs();
    if (now < next) return false;
    next = now + every;
    return true;
}

// ---- what a peer sees round the bag -----------------------------------------------------------

struct Member {
    void* obj = nullptr;
    EL::ElementId eid = EL::kInvalidId;
};
struct Census {
    std::vector<Member> packed, wraps, bunks;
    bool Empty() const { return packed.empty() && wraps.empty() && bunks.empty(); }
};

enum class Form : uint8_t { Packed, Wrap, Bunk };
struct ClassHit {
    void* cls;
    Form  form;
};
struct Gather {
    FVector center;
    float   radius;
    std::vector<Member>* into;
};

void OnClass(void* ctx, void* cls, void* any) {
    auto* v = static_cast<std::vector<ClassHit>*>(ctx);
    if (SB::IsPacked(any)) v->push_back({cls, Form::Packed});
    else if (SB::IsDeployed(any)) v->push_back({cls, Form::Wrap});
    else if (SB::IsBunkMattress(any)) v->push_back({cls, Form::Bunk});
}

void OnInstance(void* ctx, void* obj, int32_t index) {
    auto* g = static_cast<Gather*>(ctx);
    if (!obj || (R::SlotFlags(index) & (R::slot_flags::Dying | R::slot_flags::NotYetReadable))) return;
    FVector at{};
    if (!E::TryGetActorLocation(obj, at)) return;
    const float dx = at.X - g->center.X, dy = at.Y - g->center.Y, dz = at.Z - g->center.Z;
    if (std::sqrt(dx * dx + dy * dy + dz * dz) > g->radius) return;
    g->into->push_back({obj, EL::Registry::Get().EidForActor(obj)});
}

// Every live packed, deployed and bunk-mattress form of the family within `radius` of `center`, whatever its class.
Census Take(const FVector& center, float radius = kFamilyCm) {
    std::vector<ClassHit> classes;
    OI::ForEachClass(&OnClass, &classes);
    Census c;
    for (const ClassHit& h : classes) {
        Gather g{center, radius, h.form == Form::Wrap ? &c.wraps : h.form == Form::Bunk ? &c.bunks : &c.packed};
        OI::ForEachInstance(h.cls, &OnInstance, &g);
    }
    return c;
}

// One change, judged on a copy: the old form gone and exactly one new form, with an element id.
bool OneWrap(const Census& c) {
    return c.packed.empty() && c.bunks.empty() && c.wraps.size() == 1 && c.wraps[0].eid != EL::kInvalidId;
}
bool OnePacked(const Census& c) {
    return c.wraps.empty() && c.bunks.empty() && c.packed.size() == 1 && c.packed[0].eid != EL::kInvalidId;
}

std::string Why(const Census& c) {
    std::string s = "packed=" + std::to_string(c.packed.size()) + " deployed=" + std::to_string(c.wraps.size()) +
                    " bunk=" + std::to_string(c.bunks.size());
    for (const Member& m : c.packed) s += " [packed eid=" + std::to_string(m.eid) + "]";
    for (const Member& m : c.wraps) s += " [deployed eid=" + std::to_string(m.eid) + "]";
    for (const Member& m : c.bunks) s += " [bunk eid=" + std::to_string(m.eid) + "]";
    return s;
}

// The press as a player's runs it: the prop's actionOptionIndex with this peer's own player. Null on success.
const char* Press(void* actor, void* player, uint8_t action) {
    void* const root = E::RootComponentOf(actor);
    FVector at{};
    if (!root || !E::TryGetActorLocation(actor, at)) return "the bag's root or place did not read";
    return PU::CallActionOptionIndex(actor, player, root, at, action, root) ? nullptr
                                                                           : "actionOptionIndex did not dispatch";
}

void Pass(const char* role, const char* leg, const Census& c) {
    UE_LOGI("[BAG-DRILL] %s %s PASS -- %s", role, leg, Why(c).c_str());
}

// ---- the chat lines the peers say to each other ------------------------------------------------

void Say(const std::string& line) { coop::chat_sync::QueueSend(line); }

bool ClientSaid(int slot, const char* line) {
    bool found = false;
    const std::string want = line;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) {
        if (!found && r.slot == slot && r.text.compare(0, want.size(), want) == 0) found = true;
    });
    return found;
}

bool HostSaid(const std::string& head, std::string* rest) {
    bool found = false;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        const size_t at = found ? std::string_view::npos : r.line.find(head);
        if (at == std::string_view::npos) return;
        found = true;
        if (rest) *rest = std::string(r.line.substr(at + head.size()));
    });
    return found;
}

// ---- HOST ---------------------------------------------------------------------------------------

enum class HStep : uint8_t { Arm, Announce, ClientUnroll, ClientFold, AwaitFoldSeen, HostUnroll, AwaitUnrollSeen,
                             HostFold, BunkArm, BunkTake, Done };
HStep    g_h = HStep::Arm;
uint64_t g_hStepMs = 0, g_hNextMs = 0, g_armedMs = 0;
int      g_spawnTries = 0;
FVector  g_center{};
ue_wrap::CachedObjRef g_hostSrc;      // the dropped source: the packed bag, then the bunk mattress
const char* g_announceHead = kReadyHead;
const char* g_announceWhat = "packed bag";
HStep    g_announceNext = HStep::ClientUnroll;

void HEnter(HStep s) {
    g_h = s;
    g_hStepMs = NowMs();
    g_hNextMs = 0;
}
bool HExpired(uint64_t bound) { return NowMs() - g_hStepMs > bound; }

void HostFail(const char* leg, const std::string& why) {
    UE_LOGW("[BAG-DRILL] FAIL host %s: %s", leg, why.c_str());
    g_h = HStep::Done;
}

// The first floor round the client's puppet, its facing first and then a fan, that a route from the puppet reaches
// and that holds no form of the family within kFamilyCm: a bag or mattress already lying there (the base's own, or an
// earlier leg's) would be counted into this leg's census. Drops `path`'s class there. Null on success, else why not.
const char* SpawnSource(const wchar_t* path, const char* what, void* player, void** out) {
    coop::RemotePlayer& rp = coop::puppet_drive::Puppet(kClientSlot);
    void* const puppet = rp.valid() ? rp.GetActor() : nullptr;
    FVector at{};
    if (!puppet || !E::TryGetActorLocation(puppet, at)) return "slot 1's puppet place could not be read";
    const FVector fwd = E::GetActorForwardVector(puppet);
    const float len = std::sqrt(fwd.X * fwd.X + fwd.Y * fwd.Y);
    if (len < 0.1f) return "slot 1's puppet facing could not be read";
    void* const cls = ue_wrap::asset_load::LoadObjectByPath(path);
    if (!cls) return "the source's class did not load";
    for (const float deg : kFanDeg) {
        const float rad = deg * (3.14159265f / 180.f);
        const float dx = (fwd.X * std::cos(rad) - fwd.Y * std::sin(rad)) / len;
        const float dy = (fwd.X * std::sin(rad) + fwd.Y * std::cos(rad)) / len;
        for (const float ahead : kAheadCm) {
            const float x = at.X + dx * ahead, y = at.Y + dy * ahead;
            float floorZ = 0.f;
            if (!DP::FloorAt(player, x, y, at.Z, kFloorDownCm, kMinColumnCm, &floorZ)) continue;
            if (!D::RouteInLegs(player, at, FVector{x, y, floorZ}, kReachCm)) continue;
            const Census there = Take(FVector{x, y, floorZ}, kFamilyCm + kBaselineMarginCm);
            if (!there.Empty()) {
                UE_LOGI("[BAG-DRILL] host skips the column at (%.0f, %.0f, %.0f): the family already stands there -- %s",
                        x, y, floorZ, Why(there).c_str());
                continue;
            }
            void* src = nullptr;
            const DP::Drop d = DP::DropOnFloor(player, cls, nullptr, x, y, at.Z, kFloorDownCm, kMinColumnCm, kDropCm,
                                               &src, &floorZ);
            if (d == DP::Drop::NoFloor) continue;
            if (d == DP::Drop::SpawnFailed) return "the source did not spawn";
            UE_LOGI("[BAG-DRILL] host dropped a %s at (%.0f, %.0f, %.0f), %.0f uu away at %.0f deg from slot 1's "
                    "puppet's facing", what, x, y, floorZ + kDropCm, ahead, deg);
            *out = src;
            return nullptr;
        }
    }
    return "no empty floor a route reaches was found round slot 1's puppet";
}

// Drops the source and moves on to its announcement; stays put, trying again, while no column took it.
void HostDrop(const wchar_t* path, const char* what, const char* head, HStep next, void* player) {
    if (!Due(g_hNextMs, 1000)) return;
    if (!g_armedMs) g_armedMs = NowMs();
    void* src = nullptr;
    if (const char* why = SpawnSource(path, what, player, &src)) {
        if (g_spawnTries++ % 10 == 0) UE_LOGI("[BAG-DRILL] host waits to drop the %s: %s", what, why);
        if (NowMs() - g_armedMs > kSpawnBoundMs) {
            UE_LOGW("[BAG-DRILL] ABANDONED host: %s", why);
            g_h = HStep::Done;
        }
        return;
    }
    g_hostSrc.Set(src);
    g_announceHead = head;
    g_announceWhat = what;
    g_announceNext = next;
    HEnter(HStep::Announce);
}

// The host's change legs: a state wait ended by what its own copy shows. True once the leg passed.
bool HostWait(HStep next, const char* leg, bool wantWrap, uint64_t bound) {
    if (g_hNextMs > NowMs()) return false;
    g_hNextMs = NowMs() + kCheckEveryMs;
    const Census c = Take(g_center);
    const bool one = wantWrap ? OneWrap(c) : OnePacked(c);
    void* const form = !one ? nullptr : wantWrap ? c.wraps[0].obj : c.packed[0].obj;
    if (form && DP::HasKey(form)) {
        Pass("host", leg, c);
        HEnter(next);
        return true;
    }
    if (HExpired(bound)) HostFail(leg, Why(c) + (one ? " -- the one form carries no prop key" : ""));
    return false;
}

// The host presses on its own copy, the way a local player would.
void HostPress(HStep next, const char* leg, bool onWrap, uint8_t action, void* player) {
    const Census c = Take(g_center);
    const std::vector<Member>& from = onWrap ? c.wraps : c.packed;
    if (from.size() != 1) { HostFail(leg, "the bag to press is not one: " + Why(c)); return; }
    if (const char* why = Press(from[0].obj, player, action)) { HostFail(leg, why); return; }
    UE_LOGI("[BAG-DRILL] host pressed %s on its own copy", leg);
    HEnter(next);
}

void HostTick() {
    if (g_h == HStep::Done) return;
    void* const me = coop::players::Registry::Get().Local();
    if (!me) return;
    switch (g_h) {
    case HStep::Arm: {
        coop::RemotePlayer& rp = coop::puppet_drive::Puppet(kClientSlot);
        if (!rp.valid() || !rp.HasPose()) return;
        HostDrop(kBagPath, "packed bag", kReadyHead, HStep::ClientUnroll, me);
        return;
    }
    case HStep::BunkArm:
        HostDrop(kBunkPath, "bunk mattress", kBunkReadyHead, HStep::BunkTake, me);
        return;
    case HStep::Announce: {
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        void* src = g_hostSrc.Get();
        if (!src) { HostFail("announce", std::string("the dropped ") + g_announceWhat + " died before it was announced"); return; }
        const EL::ElementId eid = EL::Registry::Get().EidForActor(src);
        if (eid == EL::kInvalidId || !E::TryGetActorLocation(src, g_center)) {
            if (HExpired(kEidBoundMs)) HostFail("announce", std::string("the dropped ") + g_announceWhat + " took no element id");
            return;
        }
        Say(std::string(g_announceHead) + std::to_string(eid));
        UE_LOGI("[BAG-DRILL] host READY %s eid=%u at (%.0f, %.0f, %.0f)", g_announceWhat, eid, g_center.X, g_center.Y,
                g_center.Z);
        HEnter(g_announceNext);
        return;
    }
    case HStep::ClientUnroll: HostWait(HStep::ClientFold, "client-unroll", true, kClientLegBoundMs); return;
    case HStep::ClientFold:   HostWait(HStep::AwaitFoldSeen, "client-fold", false, kLegBoundMs); return;
    case HStep::AwaitFoldSeen:
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        if (ClientSaid(kClientSlot, kLineFoldSeen)) HostPress(HStep::HostUnroll, "host-unroll", false, kActUnroll, me);
        else if (HExpired(kLegBoundMs)) HostFail("client-fold", "the client never said it saw both changes");
        return;
    case HStep::HostUnroll:   HostWait(HStep::AwaitUnrollSeen, "host-unroll", true, kLegBoundMs); return;
    case HStep::AwaitUnrollSeen:
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        if (ClientSaid(kClientSlot, kLineUnrollSeen)) HostPress(HStep::HostFold, "host-fold", true, kActFold, me);
        else if (HExpired(kLegBoundMs)) HostFail("host-unroll", "the client never said it saw the host's unroll");
        return;
    case HStep::HostFold:
        if (HostWait(HStep::BunkArm, "host-fold", false, kLegBoundMs)) {
            g_armedMs = 0;
            g_spawnTries = 0;
        }
        return;
    case HStep::BunkTake:
        if (HostWait(HStep::Done, "client-bunk-take", false, kClientLegBoundMs)) UE_LOGI("[BAG-DRILL] host DONE PASS");
        return;
    default: return;
    }
}

// ---- CLIENT -------------------------------------------------------------------------------------

enum class CStep : uint8_t { Arm, AwaitReady, Walk, Unroll, UnrollWait, Stand, Fold, FoldWait, SayFoldSeen,
                             HostUnrollWait, SayUnrollSeen, HostFoldWait, BunkTake, BunkWait, Done };
CStep    g_c = CStep::Arm;
CStep    g_afterStand = CStep::Fold;
CStep    g_afterWalk = CStep::Unroll;
const char* g_awaitHead = kReadyHead;  // the host's ready line this wait reads, and the form it names
bool     g_awaitBunk = false;
const char* g_standLeg = "";
uint64_t g_cStepMs = 0, g_cNextMs = 0, g_nextSayMs = 0;
float    g_birthS = 0.f;
ue_wrap::CachedObjRef g_bag, g_wrap;
std::shared_ptr<D::BackgroundWalk> g_walk;

void CEnter(CStep s) {
    g_c = s;
    g_cStepMs = NowMs();
    g_cNextMs = 0;
}
bool CExpired(uint64_t bound) { return NowMs() - g_cStepMs > bound; }

void ClientFail(const char* leg, const std::string& why) {
    UE_LOGW("[BAG-DRILL] FAIL client %s: %s", leg, why.c_str());
    UE_LOGW("[BAG-DRILL] client DONE FAIL");
    g_c = CStep::Done;
}
void ClientAbandon(const char* why) {
    UE_LOGW("[BAG-DRILL] ABANDONED client: %s", why);
    g_c = CStep::Done;
}

// The deployed bag has just appeared on this copy: it must stand kStandWorldS of world time from now.
void BeginStand(const Census& c, const char* leg, CStep after) {
    float now = 0.f;
    if (!E::TryGetWorldTimeSeconds(now)) { ClientFail(leg, "the world clock did not read"); return; }
    g_wrap.Set(c.wraps[0].obj);
    g_birthS = now;
    g_standLeg = leg;
    g_afterStand = after;
    CEnter(CStep::Stand);
}

// A change that crossed to this copy: the same judgement as the host's, without the key. A deployed bag is then
// stood for kStandWorldS before `next`.
void ClientWait(const char* leg, bool wantWrap, CStep next) {
    if (g_cNextMs > NowMs()) return;
    g_cNextMs = NowMs() + kCheckEveryMs;
    const Census c = Take(g_center);
    if (wantWrap ? OneWrap(c) : OnePacked(c)) {
        Pass("client", leg, c);
        if (wantWrap) BeginStand(c, leg, next);
        else CEnter(next);
        return;
    }
    if (CExpired(kLegBoundMs)) ClientFail(leg, Why(c));
}

void ClientTick() {
    if (g_c == CStep::Done) return;
    void* const player = coop::players::Registry::Get().Local();
    if (!player) return;
    switch (g_c) {
    case CStep::Arm: {
        if (!coop::net_pump::HasAnnouncedWorldReady() ||
            coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle)
            return;
        if (coop::players::Registry::Get().LocalPeerId() != kClientSlot) {
            UE_LOGI("[BAG-DRILL] client idle -- this drill runs on slot %d only", kClientSlot);
            g_c = CStep::Done;
            return;
        }
        CEnter(CStep::AwaitReady);
        return;
    }
    case CStep::AwaitReady: {
        if (!Due(g_cNextMs, kCheckEveryMs)) return;
        std::string rest;
        void* bag = nullptr;
        if (HostSaid(g_awaitHead, &rest)) {
            const EL::ElementId eid = static_cast<EL::ElementId>(std::strtoul(rest.c_str(), nullptr, 10));
            EL::Element* el = EL::Registry::Get().Get(eid);
            void* a = el ? el->GetActor() : nullptr;
            if (a && R::IsLiveByIndex(a, el->GetInternalIdx()) && (g_awaitBunk ? SB::IsBunkMattress(a) : SB::IsPacked(a)))
                bag = a;
        }
        if (!bag) {
            if (CExpired(kSeenBoundMs)) ClientFail("announce", "the host's source never stood in this world");
            return;
        }
        FVector me{};
        if (!E::TryGetActorLocation(bag, g_center) || !E::TryGetActorLocation(player, me)) {
            ClientAbandon("the source's or the player's place did not read");
            return;
        }
        if (!D::RouteInLegs(player, me, g_center, kReachCm)) { ClientAbandon("no NavMesh route reaches the source"); return; }
        g_bag.Set(bag);
        UE_LOGI("[BAG-DRILL] client sees the %s at (%.0f, %.0f, %.0f); walking to it",
                g_awaitBunk ? "bunk mattress" : "packed bag", g_center.X, g_center.Y, g_center.Z);
        g_walk = D::StartBackgroundWalk(g_center, kReachCm, kWalkDeadlineS);
        CEnter(CStep::Walk);
        return;
    }
    case CStep::Walk: {
        const int st = g_walk ? g_walk->state.load() : 2;
        if (st == 0) return;
        if (st == 2) { ClientAbandon("the walk to the source did not arrive"); return; }
        CEnter(g_afterWalk);
        return;
    }
    case CStep::Unroll: {
        void* bag = g_bag.Get();
        if (!bag) { ClientFail("client-unroll", "the bag died before the press"); return; }
        if (const char* why = Press(bag, player, kActUnroll)) { ClientFail("client-unroll", why); return; }
        // The lane cancels this copy's own body: a bag gone at the press's return was unrolled here.
        if (!g_bag.Alive()) {
            ClientFail("client-unroll", "the press unrolled the bag on this copy: the lane did not cancel the local body");
            return;
        }
        UE_LOGI("[BAG-DRILL] client pressed the unroll; this copy still holds the bag");
        CEnter(CStep::UnrollWait);
        return;
    }
    case CStep::UnrollWait: ClientWait("client-unroll", true, CStep::Fold); return;
    case CStep::Stand: {
        if (!g_wrap.Alive()) {
            ClientFail(g_standLeg, "the deployed bag was destroyed on this copy before it had stood long enough");
            return;
        }
        float now = 0.f;
        if (!E::TryGetWorldTimeSeconds(now)) { ClientFail(g_standLeg, "the world clock did not read"); return; }
        if (now - g_birthS < kStandWorldS) {
            if (CExpired(kClockBoundMs + static_cast<uint64_t>(kStandWorldS * 1000.f)))
                ClientFail(g_standLeg, "the world clock did not advance through the stand");
            return;
        }
        const Census c = Take(g_center);
        if (!OneWrap(c) || c.wraps[0].obj != g_wrap.Get()) {
            ClientFail(g_standLeg, "the deployed bag did not stand: " + Why(c));
            return;
        }
        UE_LOGI("[BAG-DRILL] client %s-stand PASS -- the deployed bag still stands %.1f s after its birth here",
                g_standLeg, now - g_birthS);
        CEnter(g_afterStand);
        return;
    }
    case CStep::Fold: {
        void* wrap = g_wrap.Get();
        if (!wrap) { ClientFail("client-fold", "the deployed bag died before the press"); return; }
        if (const char* why = Press(wrap, player, kActFold)) { ClientFail("client-fold", why); return; }
        if (!g_wrap.Alive()) {
            ClientFail("client-fold", "the press folded the bag on this copy: the lane did not cancel the local body");
            return;
        }
        UE_LOGI("[BAG-DRILL] client pressed the fold; this copy still holds the deployed bag");
        CEnter(CStep::FoldWait);
        return;
    }
    case CStep::FoldWait: ClientWait("client-fold", false, CStep::SayFoldSeen); return;
    case CStep::SayFoldSeen:
        // A client's chat passes the host's rate limit only at the drills' spacing.
        if (NowMs() < g_nextSayMs) return;
        Say(kLineFoldSeen);
        g_nextSayMs = NowMs() + coop::dev::kTalkEveryMs;
        CEnter(CStep::HostUnrollWait);
        return;
    case CStep::HostUnrollWait: ClientWait("host-unroll", true, CStep::SayUnrollSeen); return;
    case CStep::SayUnrollSeen:
        if (NowMs() < g_nextSayMs) return;
        Say(kLineUnrollSeen);
        g_nextSayMs = NowMs() + coop::dev::kTalkEveryMs;
        CEnter(CStep::HostFoldWait);
        return;
    case CStep::HostFoldWait: {
        if (g_cNextMs > NowMs()) return;
        g_cNextMs = NowMs() + kCheckEveryMs;
        const Census c = Take(g_center);
        if (OnePacked(c)) {
            Pass("client", "host-fold", c);
            g_awaitHead = kBunkReadyHead;
            g_awaitBunk = true;
            g_afterWalk = CStep::BunkTake;
            CEnter(CStep::AwaitReady);
            return;
        }
        if (CExpired(kLegBoundMs)) ClientFail("host-fold", Why(c));
        return;
    }
    case CStep::BunkTake: {
        void* mattress = g_bag.Get();
        if (!mattress) { ClientFail("client-bunk-take", "the bunk mattress died before the press"); return; }
        if (const char* why = Press(mattress, player, kActTake)) { ClientFail("client-bunk-take", why); return; }
        if (!g_bag.Alive()) {
            ClientFail("client-bunk-take",
                       "the press took the mattress off on this copy: the lane did not cancel the local body");
            return;
        }
        UE_LOGI("[BAG-DRILL] client pressed the take-off; this copy still holds the bunk mattress");
        CEnter(CStep::BunkWait);
        return;
    }
    case CStep::BunkWait: {
        if (g_cNextMs > NowMs()) return;
        g_cNextMs = NowMs() + kCheckEveryMs;
        const Census c = Take(g_center);
        if (OnePacked(c)) {
            Pass("client", "client-bunk-take", c);
            UE_LOGI("[BAG-DRILL] client DONE PASS");
            g_c = CStep::Done;
        } else if (CExpired(kLegBoundMs)) {
            ClientFail("client-bunk-take", Why(c));
        }
        return;
    }
    default: return;
    }
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (!Enabled() || !s) return;
    if (s->role() == coop::net::Role::Host) {
        if (s->running()) HostTick();
    } else if (s->connected()) {
        ClientTick();
    }
}

void OnDisconnect() {
    if (!Enabled()) return;
    if (g_walk && g_walk->state.load() == 0) D::CancelBackgroundWalk(*g_walk);
    g_walk.reset();
    g_c = CStep::Arm;
    g_h = HStep::Arm;
    g_cStepMs = g_cNextMs = g_nextSayMs = g_hStepMs = g_hNextMs = g_armedMs = 0;
    g_spawnTries = 0;
    g_awaitHead = kReadyHead;
    g_awaitBunk = false;
    g_afterWalk = CStep::Unroll;
    g_announceHead = kReadyHead;
    g_announceWhat = "packed bag";
    g_announceNext = HStep::ClientUnroll;
    g_bag.Reset();
    g_wrap.Reset();
    g_hostSrc.Reset();
}

}  // namespace coop::dev::sleepingbag_drill
