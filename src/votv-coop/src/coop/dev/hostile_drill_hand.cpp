// coop/dev/hostile_drill_hand.cpp -- see coop/dev/hostile_drill_hand.h.

#include "coop/dev/hostile_drill_hand.h"

#include "coop/comms/chat_log.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace coop::dev::hostile_drill::hand {
namespace {

namespace R = ue_wrap::reflection;
using Clock = std::chrono::steady_clock;

// The class the forged hand names: the engine's base actor. Every peer has it loaded, it is not a prop,
// and an instance of it is inert (no root component, so a mirror's placement calls do nothing).
constexpr const char*    kForgedClass  = "Actor";
constexpr const wchar_t* kForgedClassW = L"Actor";
// The fallback hold transform the owner ships before its first measure (hand_item.cpp), so the message
// is shaped as an honest one is in every field but the class.
constexpr float kFallbackRel[6] = {45.f, 10.f, -40.f, 0.f, 0.f, 0.f};
// A FIXED sentinel: the host judges it, and a line its record kept from a prior run is excluded by the
// lineSeq baseline the host takes at arm.
constexpr const char* kSentinel = "hostile-drill bad_hand_class sentinel";
constexpr auto        kFailBound = std::chrono::seconds(180);  // a failure bound, never a wait

// ---- client (sender) ----
bool g_sent = false;

bool OwnJoinOver() {
    return coop::net_pump::HasAnnouncedWorldReady() &&
           coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle;
}

// HandItem's wire layout (hand_item.cpp BuildPayload): slot, has, then a length-prefixed class, name and
// item, then the six-float hold transform when held. The name and item are empty, so no build writes an
// item name into the actor it spawns.
std::vector<uint8_t> BuildForgedHand(uint8_t slot) {
    std::vector<uint8_t> out;
    const size_t cl = std::strlen(kForgedClass);
    out.push_back(slot);
    out.push_back(1);
    out.push_back(static_cast<uint8_t>(cl));
    for (size_t i = 0; i < cl; ++i) out.push_back(static_cast<uint8_t>(kForgedClass[i]));
    out.push_back(0);  // name
    out.push_back(0);  // item
    const size_t base = out.size();
    out.resize(base + sizeof(kFallbackRel));
    std::memcpy(out.data() + base, kFallbackRel, sizeof(kFallbackRel));
    return out;
}

bool SendChatLine(coop::net::Session* s, const char* text) {
    coop::net::ChatMessagePayload line{};
    const size_t n = std::strlen(text) < sizeof(line.text) ? std::strlen(text) : sizeof(line.text);
    std::memcpy(line.text, text, n);
    line.len = static_cast<uint8_t>(n);
    return s->SendReliable(coop::net::ReliableKind::ChatMessage, &line, sizeof(line));
}

// ---- host (judge) ----
// Judge waits for the sentinel; ReadMirror is the one call after it, when the stored state has had one
// mirror pass: the pump runs the drills before local_streams (which runs hand_item::TickMirrors) and
// drains the inbox after both, so the call that first sees the sentinel precedes that pass and the next
// one follows it.
enum class Step { WaitArm, Judge, ReadMirror, Done };
Step              g_step = Step::WaitArm;
int               g_slot = -1;
uint32_t          g_lineSeqBase = 0;
Clock::time_point g_armAt{};

void Verdict(const char* what, const char* detail) {
    UE_LOGI("[HOSTILE-DRILL] %s %s -- %s", ArmName(), what, detail);
    g_step = Step::Done;
}

uint32_t LastLineSeq() {
    uint32_t last = 0;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) { if (r.lineSeq > last) last = r.lineSeq; });
    return last;
}

bool SentinelLanded() {
    bool seen = false;
    const std::string want = kSentinel;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) {
        if (r.lineSeq > g_lineSeqBase && r.slot == static_cast<uint8_t>(g_slot) && r.text == want)
            seen = true;
    });
    return seen;
}

void ArmHost(coop::net::Session* s) {
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers) && g_slot < 0; ++slot)
        if (s->IsSlotWorldReady(slot)) g_slot = slot;
    if (g_slot < 0) return;
    g_lineSeqBase = LastLineSeq();
    g_armAt = Clock::now();
    g_step = Step::Judge;
    UE_LOGI("[HOSTILE-DRILL] %s armed on slot %d (chat lineSeq baseline %u)", ArmName(), g_slot,
            g_lineSeqBase);
}

void Judge() {
    if (!SentinelLanded()) {
        if (Clock::now() - g_armAt > kFailBound) Verdict("INCONCLUSIVE", "the sentinel never arrived");
        return;
    }
    if (!coop::hand_item::HeldClassIs(static_cast<uint8_t>(g_slot), kForgedClassW)) {
        Verdict("PASS", "the forged hand class was refused at receipt: not stored, not relayed; the line "
                        "sent behind it arrived");
        return;
    }
    UE_LOGI("[HOSTILE-DRILL] %s the host STORED the forged class for slot %d; reading its mirror after the "
            "next mirror pass", ArmName(), g_slot);
    g_step = Step::ReadMirror;
}

void ReadMirror() {
    void* m = coop::hand_item::MirrorActorForSlot(static_cast<uint8_t>(g_slot));
    if (m && !ue_wrap::prop::IsDescendantOfProp(m)) {
        const std::wstring cls = R::ClassNameOf(m);
        UE_LOGW("[HOSTILE-DRILL] %s the slot's mirror is a '%ls', not a prop", ArmName(), cls.c_str());
        UE_LOGW("[HOSTILE-DRILL] %s FAIL: spawned", ArmName());
    } else {
        UE_LOGW("[HOSTILE-DRILL] %s the slot's mirror: %s", ArmName(), m ? "a prop" : "none");
        UE_LOGW("[HOSTILE-DRILL] %s FAIL: stored", ArmName());
    }
    g_step = Step::Done;
}

}  // namespace

const char* ArmName() { return "bad_hand_class"; }

void TickClient(coop::net::Session* s) {
    if (g_sent || !OwnJoinOver()) return;
    const uint8_t self = coop::players::Registry::Get().LocalPeerId();
    if (self == 0 || self >= coop::players::kMaxPeers) return;  // no slot yet; wait
    g_sent = true;
    const std::vector<uint8_t> p = BuildForgedHand(self);
    if (!s->SendReliableToSlot(0, coop::net::ReliableKind::HandItem, p.data(), static_cast<int>(p.size()))) {
        UE_LOGW("[HOSTILE-DRILL] %s INCONCLUSIVE: the forged hand state was not sent", ArmName());
        return;  // no sentinel behind it, so the host cannot judge
    }
    if (!SendChatLine(s, kSentinel)) {
        UE_LOGW("[HOSTILE-DRILL] %s INCONCLUSIVE: the sentinel was not sent", ArmName());
        return;
    }
    UE_LOGI("[HOSTILE-DRILL] %s sent a hand state naming class '%s' for slot %u + sentinel", ArmName(),
            kForgedClass, static_cast<unsigned>(self));
}

void TickHostJudge(coop::net::Session* s) {
    if (g_step == Step::WaitArm) { ArmHost(s); return; }
    if (g_step == Step::Done) return;
    if (!s->HasPeerConn(g_slot)) { Verdict("INCONCLUSIVE", "the judged slot left before the verdict"); return; }
    if (g_step == Step::Judge)           Judge();
    else if (g_step == Step::ReadMirror) ReadMirror();
}

void OnDisconnect() {
    g_sent = false;
    g_step = Step::WaitArm;
    g_slot = -1;
    g_lineSeqBase = 0;
    g_armAt = {};
}

}  // namespace coop::dev::hostile_drill::hand
