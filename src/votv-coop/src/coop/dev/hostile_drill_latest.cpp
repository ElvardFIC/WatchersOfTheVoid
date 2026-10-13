// coop/dev/hostile_drill_latest.cpp -- see coop/dev/hostile_drill_latest.h.

#include "coop/dev/hostile_drill_latest.h"

#include "coop/dispatch/event_feed.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/nameplate.h"
#include "coop/player/nick_color.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_lifecycle.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"
#include "coop/session/player_handshake.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace coop::dev::hostile_drill::latest {
namespace {

namespace R = ue_wrap::reflection;
using Clock = std::chrono::steady_clock;
using Kind = coop::net::ReliableKind;

// The flood: per kind, kAlternates messages alternating A and B, then one C, which differs from every value the
// burst can leave applied. Both peers know A, B and C but for the hand's two classes, which the client picks from
// its own world (the hand's C is an empty hand), and the plate's (below).
constexpr int         kAlternates = 120;
constexpr uint32_t    kPerKind    = kAlternates + 1;
constexpr const char* kSkinA      = "kerfur_omega";    // built-in skins: every peer loads them by asset path
constexpr const char* kSkinB      = "kerfur_omega_h";
constexpr const char* kSkinC      = "kerfur_omega_m";
// The plate has two values, so its are the client's own (A) and the other (B and C): the burst ends on A, and C is
// owed. The host expects the opposite of the plate it held for the slot at arm.
const uint32_t        kColorA     = coop::nick_color::Pack(255, 0, 0);
const uint32_t        kColorB     = coop::nick_color::Pack(0, 255, 0);
const uint32_t        kColorC     = coop::nick_color::Pack(0, 0, 255);

// The judge's numbers, literals from RECEIVE_BOUNDARY_ARC.md §3m 3b-3b (no symbol of the change itself): at the
// first read after the drain, a kind has applied at least two of its alternating values and at most its burst plus
// one; each kind's C must then be applied within its refill period plus 10 s.
constexpr uint32_t kMinApplies     = 2;
constexpr uint32_t kPrefMaxApplies = 5;   // burst 4, + 1
constexpr uint32_t kHandMaxApplies = 9;   // burst 8, + 1
constexpr auto     kSkinWindow  = std::chrono::milliseconds(13000);  // one per 3 s, + 10 s
constexpr auto     kPrefWindow  = std::chrono::milliseconds(11000);  // one per 1 s, + 10 s
constexpr auto     kHandWindow  = std::chrono::milliseconds(10250);  // four per 1 s, + 10 s
constexpr auto     kFailBound   = std::chrono::seconds(180);         // a failure bound, never a wait

// The fallback hold transform the owner ships before its first measure (hand_item.cpp).
constexpr float kFallbackRel[6] = {45.f, 10.f, -40.f, 0.f, 0.f, 0.f};

// ---- client (sender) ----
bool g_sent = false;

bool OwnJoinOver() {
    return coop::net_pump::HasAnnouncedWorldReady() &&
           coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle;
}

// Two prop classes with a live instance in this world, outside the two families the coop layer never puts in a
// hand, and not the prop base itself.
struct ClassPick { std::wstring names[2]; int n = 0; };
void PickClass(void* ctx, void* /*cls*/, void* anyInstance) {
    auto* pick = static_cast<ClassPick*>(ctx);
    if (pick->n >= 2 || !anyInstance || !ue_wrap::prop::IsDescendantOfProp(anyInstance)) return;
    const std::wstring name = R::ClassNameOf(anyInstance);
    // Within the lane's string cap (hand_item.cpp kMaxStr, 63), so the host keeps the state.
    if (name.empty() || name.size() > 63 || name == L"prop_C" || coop::prop_lifecycle::IsPerPlayerPropClass(name) ||
        coop::prop_lifecycle::IsWireSuppressedPropClass(name))
        return;
    if (pick->n == 1 && pick->names[0] == name) return;
    pick->names[pick->n++] = name;
}

// The wire forms (player_handshake_prefs.cpp, hand_item.cpp BuildPayload).
std::vector<uint8_t> SkinPayload(uint8_t slot, const char* name) {
    std::vector<uint8_t> out{slot, static_cast<uint8_t>(std::strlen(name))};
    out.insert(out.end(), name, name + std::strlen(name));
    return out;
}
std::vector<uint8_t> PlatePayload(uint8_t slot, bool visible) { return {slot, static_cast<uint8_t>(visible ? 1 : 0)}; }
std::vector<uint8_t> ColorPayload(uint8_t slot, uint32_t packed) {
    return {slot, static_cast<uint8_t>(coop::nick_color::IsCustom(packed) ? 1 : 0), coop::nick_color::R(packed),
            coop::nick_color::G(packed), coop::nick_color::B(packed)};
}
std::vector<uint8_t> HandPayload(uint8_t slot, const std::wstring* cls) {
    std::vector<uint8_t> out{slot, static_cast<uint8_t>(cls ? 1 : 0)};
    if (!cls) {
        out.insert(out.end(), {0, 0, 0});  // no class, name or item
        return out;
    }
    out.push_back(static_cast<uint8_t>(cls->size()));
    for (wchar_t c : *cls) out.push_back(static_cast<uint8_t>(c));
    out.push_back(0);  // name
    out.push_back(0);  // item
    const size_t base = out.size();
    out.resize(base + sizeof(kFallbackRel));
    std::memcpy(out.data() + base, kFallbackRel, sizeof(kFallbackRel));
    return out;
}

bool Send(coop::net::Session* s, Kind k, const std::vector<uint8_t>& p) {
    return s->SendReliableToSlot(0, k, p.data(), static_cast<int>(p.size()));
}

// ---- host (judge) ----
enum class Step { WaitArm, WaitDrained, WaitLatest, Done };
Step              g_step = Step::WaitArm;
int               g_slot = -1;
Clock::time_point g_armAt{};
Clock::time_point g_drainedAt{};
constexpr Kind    kKinds[4] = {Kind::SkinChange, Kind::NameplateChange, Kind::NickColorChange, Kind::HandItem};
uint32_t          g_drainedBase[4] = {};
uint32_t          g_appliedBase[4] = {};
bool              g_plateAtArm = true;

uint32_t Applied(int slot, int i) {
    return i == 3 ? coop::hand_item::StatesApplied(slot) : coop::player_handshake::PrefApplies(slot, kKinds[i]);
}

bool LatestLanded(int i) {
    switch (i) {
    case 0:  return coop::roster_ledger::Get(g_slot).skin == kSkinC;
    case 1:  return coop::nameplate::VisibleForSlot(g_slot) == !g_plateAtArm;
    case 2:  return coop::nick_color::PackedForSlot(g_slot) == kColorC;
    default: return !coop::hand_item::HoldsItem(static_cast<uint8_t>(g_slot));
    }
}

void Verdict(Arm a, const char* what, const std::string& detail) {
    UE_LOGI("[HOSTILE-DRILL] %s %s -- %s", ArmName(a), what, detail.c_str());
    g_step = Step::Done;
}

void ArmHost(coop::net::Session* s, Arm a) {
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers) && g_slot < 0; ++slot)
        if (s->IsSlotWorldReady(slot)) g_slot = slot;
    if (g_slot < 0) return;
    for (int i = 0; i < 4; ++i) {
        g_drainedBase[i] = coop::event_feed::DrainedFrom(g_slot, kKinds[i]);
        g_appliedBase[i] = Applied(g_slot, i);
    }
    g_plateAtArm = coop::nameplate::VisibleForSlot(g_slot);
    g_armAt = Clock::now();
    g_step = Step::WaitDrained;
    UE_LOGI("[HOSTILE-DRILL] %s armed on slot %d (drained %u/%u/%u/%u, applied %u/%u/%u/%u)", ArmName(a), g_slot,
            g_drainedBase[0], g_drainedBase[1], g_drainedBase[2], g_drainedBase[3], g_appliedBase[0],
            g_appliedBase[1], g_appliedBase[2], g_appliedBase[3]);
}

void WaitDrained(Arm a) {
    for (int i = 0; i < 4; ++i) {
        if (coop::event_feed::DrainedFrom(g_slot, kKinds[i]) - g_drainedBase[i] < kPerKind) {
            if (Clock::now() - g_armAt > kFailBound) {
                UE_LOGW("[HOSTILE-DRILL] %s FAIL: timeout (kind %u drained %u of %u)", ArmName(a),
                        static_cast<unsigned>(kKinds[i]),
                        coop::event_feed::DrainedFrom(g_slot, kKinds[i]) - g_drainedBase[i], kPerKind);
                g_step = Step::Done;
            }
            return;
        }
    }
    uint32_t applied[4];
    bool over = false, under = false;
    for (int i = 0; i < 4; ++i) {
        applied[i] = Applied(g_slot, i) - g_appliedBase[i];
        over = over || applied[i] > (i == 3 ? kHandMaxApplies : kPrefMaxApplies);
        under = under || applied[i] < kMinApplies;
    }
    UE_LOGI("[HOSTILE-DRILL] %s drained %u of each kind; applied skin %u, plate %u, colour %u, hand %u", ArmName(a),
            kPerKind, applied[0], applied[1], applied[2], applied[3]);
    if (under) {
        UE_LOGW("[HOSTILE-DRILL] %s FAIL: none-applied (skin %u, plate %u, colour %u, hand %u; at least %u each)",
                ArmName(a), applied[0], applied[1], applied[2], applied[3], kMinApplies);
        g_step = Step::Done;
        return;
    }
    if (over) {
        UE_LOGW("[HOSTILE-DRILL] %s FAIL: applied-every-change (skin %u, plate %u, colour %u, hand %u; at most %u, "
                "%u for the hand)", ArmName(a), applied[0], applied[1], applied[2], applied[3], kPrefMaxApplies,
                kHandMaxApplies);
        g_step = Step::Done;
        return;
    }
    g_drainedAt = Clock::now();
    g_step = Step::WaitLatest;
}

void WaitLatest(Arm a) {
    static const char* const kNames[4] = {"skin", "plate", "colour", "hand"};
    const std::chrono::milliseconds windows[4] = {kSkinWindow, kPrefWindow, kPrefWindow, kHandWindow};
    bool all = true;
    for (int i = 0; i < 4; ++i) {
        if (LatestLanded(i)) continue;
        all = false;
        if (Clock::now() - g_drainedAt > windows[i]) {
            UE_LOGW("[HOSTILE-DRILL] %s FAIL: latest-lost (the %s's last value was not applied within %lld ms)",
                    ArmName(a), kNames[i], static_cast<long long>(windows[i].count()));
            g_step = Step::Done;
            return;
        }
    }
    if (!all) return;
    const long long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_drainedAt).count();
    Verdict(a, "PASS", "each kind applied no more than its burst plus one, and its last value landed " +
                           std::to_string(ms) + " ms after the drain");
}

}  // namespace

const char* ArmName(Arm a) { return a == Arm::KeepLatest ? "keep_latest" : "off"; }

void TickClient(coop::net::Session* s, Arm a) {
    if (g_sent || !OwnJoinOver()) return;
    const uint8_t self = coop::players::Registry::Get().LocalPeerId();
    if (self == 0 || self >= coop::players::kMaxPeers) return;  // no slot yet; wait
    g_sent = true;
    ClassPick pick;
    ue_wrap::object_index::ForEachClass(&PickClass, &pick);
    if (pick.n < 2) {
        UE_LOGW("[HOSTILE-DRILL] %s INCONCLUSIVE: this world holds fewer than two prop classes to hold", ArmName(a));
        return;
    }
    const bool plateA = coop::nameplate::LocalVisible();
    bool ok = true;
    for (int i = 0; i < kAlternates && ok; ++i) {
        const bool even = (i % 2) == 0;
        ok = Send(s, Kind::SkinChange, SkinPayload(self, even ? kSkinA : kSkinB)) &&
             Send(s, Kind::NameplateChange, PlatePayload(self, even ? plateA : !plateA)) &&
             Send(s, Kind::NickColorChange, ColorPayload(self, even ? kColorA : kColorB)) &&
             Send(s, Kind::HandItem, HandPayload(self, &pick.names[even ? 0 : 1]));
    }
    ok = ok && Send(s, Kind::SkinChange, SkinPayload(self, kSkinC)) &&
         Send(s, Kind::NameplateChange, PlatePayload(self, !plateA)) &&
         Send(s, Kind::NickColorChange, ColorPayload(self, kColorC)) &&
         Send(s, Kind::HandItem, HandPayload(self, nullptr));
    if (!ok) {
        UE_LOGW("[HOSTILE-DRILL] %s INCONCLUSIVE: a message of the flood was not sent", ArmName(a));
        return;
    }
    UE_LOGI("[HOSTILE-DRILL] %s sent %u of each kind for slot %u (hand classes '%ls' and '%ls', then empty)",
            ArmName(a), kPerKind, static_cast<unsigned>(self), pick.names[0].c_str(), pick.names[1].c_str());
}

void TickHostJudge(coop::net::Session* s, Arm a) {
    if (g_step == Step::WaitArm) { ArmHost(s, a); return; }
    if (g_step == Step::Done) return;
    if (!s->HasPeerConn(g_slot)) {
        Verdict(a, "INCONCLUSIVE", "the judged slot left before the verdict");
        return;
    }
    if (g_step == Step::WaitDrained)     WaitDrained(a);
    else if (g_step == Step::WaitLatest) WaitLatest(a);
}

void OnDisconnect() {
    g_sent = false;
    g_step = Step::WaitArm;
    g_slot = -1;
    g_armAt = {};
    g_drainedAt = {};
    for (int i = 0; i < 4; ++i) g_drainedBase[i] = g_appliedBase[i] = 0;
    g_plateAtArm = true;
}

}  // namespace coop::dev::hostile_drill::latest
