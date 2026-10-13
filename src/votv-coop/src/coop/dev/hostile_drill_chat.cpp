// coop/dev/hostile_drill_chat.cpp -- see coop/dev/hostile_drill_chat.h.

#include "coop/dev/hostile_drill_chat.h"

#include "coop/comms/chat_log.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/log.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>

namespace coop::dev::hostile_drill::chat {
namespace {

using Clock = std::chrono::steady_clock;
using Kind = coop::net::ReliableKind;

// The forged content, named once and shared by the sender and the host's judge so the two never drift.
constexpr const char* kChatForged   = "hostile-drill forged";    // + one lone 0x80 appended
constexpr const char* kChatSentinel = "hostile-drill sentinel";  // one well-formed line behind it
constexpr auto        kFailBound    = std::chrono::seconds(180);  // a failure bound, never a wait

// ---- client (sender) ----
bool g_sent = false;

bool OwnJoinOver() {
    return coop::net_pump::HasAnnouncedWorldReady() &&
           coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle;
}

void ClientFail(Arm a, const char* reasonToken) {
    UE_LOGW("[HOSTILE-DRILL] %s FAIL: %s", ArmName(a), reasonToken);
}

void SendChatIllFormed(coop::net::Session* s, Arm a) {
    coop::net::ChatMessagePayload forged{};
    const size_t fn = std::strlen(kChatForged);
    std::memcpy(forged.text, kChatForged, fn);
    forged.text[fn] = static_cast<char>(0x80);  // a lone continuation byte: ill-formed UTF-8 unconditionally
    forged.len = static_cast<uint8_t>(fn + 1);
    if (!s->SendReliable(Kind::ChatMessage, &forged, sizeof(forged))) {
        ClientFail(a, "the forged line was not sent");  // and no sentinel behind it, so the host cannot pass
        return;
    }
    coop::net::ChatMessagePayload sentinel{};
    const size_t sn = std::strlen(kChatSentinel);
    std::memcpy(sentinel.text, kChatSentinel, sn);
    sentinel.len = static_cast<uint8_t>(sn);
    if (!s->SendReliable(Kind::ChatMessage, &sentinel, sizeof(sentinel))) {
        ClientFail(a, "the sentinel was not sent");
        return;
    }
    UE_LOGI("[HOSTILE-DRILL] %s sent the forged line + sentinel", ArmName(a));
}

// ---- host (judge) ----
// The judge counts only what arrives after it arms: a sentinel the lobby record kept from an earlier run must never
// read as this run's.
enum class Step { WaitArm, Judge, Done };
Step              g_step = Step::WaitArm;
int               g_slot = -1;
uint32_t          g_lineSeqBase = 0;  // the last line committed before the arm
Clock::time_point g_armAt{};

void Verdict(Arm a, bool pass, const char* detail) {
    if (pass) UE_LOGI("[HOSTILE-DRILL] %s PASS -- %s", ArmName(a), detail);
    else      UE_LOGW("[HOSTILE-DRILL] %s FAIL: %s", ArmName(a), detail);
    g_step = Step::Done;
}

uint32_t LastLineSeq() {
    uint32_t last = 0;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) { if (r.lineSeq > last) last = r.lineSeq; });
    return last;
}

void ArmHost(coop::net::Session* s, Arm a) {
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers) && g_slot < 0; ++slot)
        if (s->IsSlotWorldReady(slot)) g_slot = slot;
    if (g_slot < 0) return;
    g_lineSeqBase = LastLineSeq();
    g_armAt = Clock::now();
    g_step = Step::Judge;
    UE_LOGI("[HOSTILE-DRILL] %s armed on slot %d (last line %u)", ArmName(a), g_slot, g_lineSeqBase);
}

void JudgeIllFormed(Arm a) {
    bool found0x80 = false, foundSentinel = false;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) {
        if (r.lineSeq <= g_lineSeqBase || r.slot != static_cast<uint8_t>(g_slot)) return;
        if (r.text == kChatSentinel) foundSentinel = true;
        if (r.text.find(static_cast<char>(0x80)) != std::string::npos) found0x80 = true;
    });
    // Decide only once the sentinel (sent AFTER the forged line on the same ordered lane) is committed: that is the
    // signal that the forged line has already been judged.
    if (!foundSentinel) return;
    if (found0x80) Verdict(a, false, "an ill-formed line was committed");
    else           Verdict(a, true, "the ill-formed line was refused; the well-formed one was kept");
}

}  // namespace

const char* ArmName(Arm a) { return a == Arm::ChatIllFormed ? "chat_ill_formed" : "off"; }

void TickClient(coop::net::Session* s, Arm a) {
    // Wait for THIS client's own join to be over before sending (its world-ready announce and the host's connect
    // replay are both behind this edge), so the host has armed. A readiness predicate, not a clock.
    if (g_sent || !OwnJoinOver()) return;
    g_sent = true;
    SendChatIllFormed(s, a);
}

void TickHostJudge(coop::net::Session* s, Arm a) {
    if (g_step == Step::WaitArm) { ArmHost(s, a); return; }
    if (g_step == Step::Done) return;
    if (!s->HasPeerConn(g_slot)) { Verdict(a, false, "the judged slot left before the verdict"); return; }
    JudgeIllFormed(a);
    if (g_step == Step::Judge && Clock::now() - g_armAt > kFailBound)
        Verdict(a, false, "timeout, the sentinel never arrived");
}

void OnDisconnect() {
    g_sent = false;
    g_step = Step::WaitArm;
    g_slot = -1;
    g_lineSeqBase = 0;
    g_armAt = {};
}

}  // namespace coop::dev::hostile_drill::chat
