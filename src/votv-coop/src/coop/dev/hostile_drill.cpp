// coop/dev/hostile_drill.cpp -- see coop/dev/hostile_drill.h.

#include "coop/dev/hostile_drill.h"

#include "coop/comms/chat_log.h"
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/interactables/signal_wire.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/desk/saved_signals.h"
#include "ue_wrap/desk/signal_dynamic.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace coop::dev::hostile_drill {
namespace {

namespace SD = ue_wrap::signal_dynamic;
namespace SS = ue_wrap::saved_signals;
using Clock = std::chrono::steady_clock;

enum class Arm { Off, SignalPark, ChatIllFormed, ChatIllFormedRed };

// Resolved once: with the environment variable unset every read would scan the ini on disk, and the
// drill is asked per tick (and ChatGuardOff per chat message).
Arm ArmOf() {
    static const Arm a = [] {
        const std::string v =
            coop::config::ResolveEnum(::coop::config_registry::rows::hostile_drill);
        return v == "signal_park"         ? Arm::SignalPark
             : v == "chat_ill_formed"     ? Arm::ChatIllFormed
             : v == "chat_ill_formed_red" ? Arm::ChatIllFormedRed
                                          : Arm::Off;
    }();
    return a;
}

const char* ArmName() {
    switch (ArmOf()) {
    case Arm::SignalPark:       return "signal_park";
    case Arm::ChatIllFormed:    return "chat_ill_formed";
    case Arm::ChatIllFormedRed: return "chat_ill_formed_red";
    default:                    return "off";
    }
}

// The forged content, named once and shared by the sender and the host's judge so the two never drift.
constexpr const char*    kChatForged         = "hostile-drill forged";    // + one lone 0x80 appended
constexpr const char*    kChatSentinel        = "hostile-drill sentinel"; // one well-formed line behind it
constexpr const wchar_t* kSignalSentinelName  = L"hostile-drill sentinel";
constexpr int            kForgedSignals = 300;        // > kParkCap (256): on the old bytes the park fills
constexpr uint32_t       kSeqBase = 0xB0570000u;      // outside the signal lane's own seq (starts at 1)
constexpr auto           kFailBound = std::chrono::seconds(180);  // a failure bound, never a wait

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_saidArm = false;

// Host (receiver / judge).
enum class Step { WaitArm, Judge, Done };
Step              g_step = Step::WaitArm;
int               g_slot = -1;
uint64_t          g_witnessBase = 0;
Clock::time_point g_armAt{};

// Client (sender).
bool         g_sent = false;
std::wstring g_sentinelSignal;  // signal_park: an ordinary template name, resolved once

bool IsHost(coop::net::Session* s) { return s->role() == coop::net::Role::Host; }

void Pass(const char* detail) {
    UE_LOGI("[HOSTILE-DRILL] %s PASS -- %s", ArmName(), detail);
    g_step = Step::Done;
}

// Every FAIL carries a reason token, so a red arm's EXPECTED fail is never confused with a timeout or a
// dropped link: the rig matches the exact string, not a bare "FAIL".
void Fail(const char* reasonToken) {
    UE_LOGW("[HOSTILE-DRILL] %s FAIL: %s", ArmName(), reasonToken);
    g_step = Step::Done;
}

// --- the sender (client) ---

void SendSignalPark(coop::net::Session* s) {
    int sent = 0;
    for (int i = 0; i < kForgedSignals; ++i) {
        SD::Row r;
        r.name   = std::wstring(L"hostile-drill forged ") + std::to_wstring(i);  // distinct -> distinct hash
        r.signal = L"hostile-drill";  // non-empty so the row reaches the native; size 0 then makes it refuse
        r.size = 0.f;                 // the game's own gate: succ is false unless decoded >= size > 0, so
        r.decoded = 0.f;              // every forged row is refused BEFORE the "Create Signal List" rebuild
        const std::vector<uint8_t> blob = coop::signal_wire::Serialize(r);
        if (coop::blob_chunks::SendBlob(s, coop::net::ReliableKind::SavedSignalAppend,
                                        kSeqBase + static_cast<uint32_t>(i), blob))
            ++sent;
    }
    // The sentinel: one honest row the native accepts (decoded >= size > 0), with a real ordinary
    // template name so there is no unique-signal `specials` side effect. Sent LAST on the ordered lane,
    // so its arrival means the whole forged burst has already been judged.
    SD::Row sentinel;
    sentinel.name    = kSignalSentinelName;
    sentinel.signal  = g_sentinelSignal;
    sentinel.size    = 1.f;
    sentinel.decoded = 1.f;
    sentinel.level   = 1;
    sentinel.downloadedAtQuality = 1.f;
    sentinel.date    = 0;  // a non-positive date: the native stamps Now
    const std::vector<uint8_t> sb = coop::signal_wire::Serialize(sentinel);
    const bool sentSentinel = coop::blob_chunks::SendBlob(
        s, coop::net::ReliableKind::SavedSignalAppend, kSeqBase + kForgedSignals, sb);
    UE_LOGI("[HOSTILE-DRILL] signal_park sent %d/%d forged row(s) + sentinel (%s); signal '%ls'",
            sent, kForgedSignals, sentSentinel ? "sent" : "SEND FAILED", g_sentinelSignal.c_str());
}

void SendChatIllFormed(coop::net::Session* s) {
    coop::net::ChatMessagePayload forged{};
    const size_t fn = std::strlen(kChatForged);
    std::memcpy(forged.text, kChatForged, fn);
    forged.text[fn] = static_cast<char>(0x80);  // a lone continuation byte: ill-formed UTF-8 unconditionally
    forged.len = static_cast<uint8_t>(fn + 1);
    const bool f = s->SendReliable(coop::net::ReliableKind::ChatMessage, &forged, sizeof(forged));

    coop::net::ChatMessagePayload sentinel{};
    const size_t sn = std::strlen(kChatSentinel);
    std::memcpy(sentinel.text, kChatSentinel, sn);
    sentinel.len = static_cast<uint8_t>(sn);
    const bool c = s->SendReliable(coop::net::ReliableKind::ChatMessage, &sentinel, sizeof(sentinel));
    UE_LOGI("[HOSTILE-DRILL] %s sent the forged line (%s) + sentinel (%s)", ArmName(),
            f ? "sent" : "SEND FAILED", c ? "sent" : "SEND FAILED");
}

void TickClient(coop::net::Session* s) {
    if (g_sent) return;
    // Wait for THIS client's own join to be over before sending (its world-ready announce and the
    // host's connect replay are both behind this edge), so the host has armed. A readiness predicate,
    // not a clock.
    if (!coop::net_pump::HasAnnouncedWorldReady() ||
        coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle)
        return;
    if (ArmOf() == Arm::SignalPark) {
        if (g_sentinelSignal.empty() && !SS::FirstOrdinarySignalName(g_sentinelSignal))
            return;  // the template names are not up yet; wait (the host's fail bound covers a stall)
        SendSignalPark(s);
    } else {
        SendChatIllFormed(s);
    }
    g_sent = true;
}

// --- the judge (host) ---

void ArmOnFirstReadySlot(coop::net::Session* s) {
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers) && g_slot < 0; ++slot)
        if (s->IsSlotWorldReady(slot)) g_slot = slot;
    if (g_slot < 0) return;
    // The witness: a monotonic count of a kind's apply park going from off to on. Baselined here, on
    // the game thread, in the same frame loop that drains the inbox the burst arrives on.
    g_witnessBase = s->ApplyPauseRaises();
    g_armAt = Clock::now();
    g_step = Step::Judge;
    UE_LOGI("[HOSTILE-DRILL] %s armed on slot %d (apply-pause raises baseline %llu)", ArmName(),
            g_slot, static_cast<unsigned long long>(g_witnessBase));
}

bool SignalSentinelApplied() {
    const int32_t n = SS::Count();
    for (int32_t i = 0; i < n; ++i) {
        SD::Row r;
        if (SS::ReadRow(i, r) && r.name == kSignalSentinelName) return true;
    }
    return false;
}

void JudgeSignalPark(coop::net::Session* s) {
    // The defect: refused rows filled the apply park and raised the receive pause. On the fixed bytes a
    // refused row is terminal and never parks, so this never rises; on the old bytes 300 > kParkCap park
    // and it does -- red, printed the tick it is seen.
    if (s->ApplyPauseRaises() > g_witnessBase) {
        Fail("the receive paused on SavedSignalAppend's apply park");
        return;
    }
    if (SignalSentinelApplied())
        Pass("the apply park never paused the receive loop; an honest saved signal still applied");
}

void JudgeChat() {
    bool found0x80 = false, foundSentinel = false;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) {
        if (r.slot != static_cast<uint8_t>(g_slot)) return;
        if (r.text == kChatSentinel) foundSentinel = true;
        if (r.text.find(static_cast<char>(0x80)) != std::string::npos) found0x80 = true;
    });
    // Decide only once the sentinel (sent AFTER the forged line on the same ordered lane) is committed:
    // that is the signal that the forged line has already been judged.
    if (!foundSentinel) return;
    if (found0x80)
        Fail("an ill-formed line was committed");  // red arm: EXPECTED (the rig's done-marker)
    else
        Pass("the ill-formed line was refused; the well-formed one was kept");
}

void Judge(coop::net::Session* s) {
    if (!s->HasPeerConn(g_slot)) { Fail("the judged slot left before the verdict"); return; }
    if (ArmOf() == Arm::SignalPark) JudgeSignalPark(s);
    else                            JudgeChat();
    if (g_step == Step::Judge && Clock::now() - g_armAt > kFailBound)
        Fail("timeout, the sentinel never arrived");
}

void TickHost(coop::net::Session* s) {
    if (g_step == Step::WaitArm)      ArmOnFirstReadySlot(s);
    else if (g_step == Step::Judge)   Judge(s);
}

}  // namespace

bool IsEnabled() { return ArmOf() != Arm::Off; }

bool ChatGuardOff() {
    static const bool v = (ArmOf() == Arm::ChatIllFormedRed);
    return v;
}

void Install(coop::net::Session* session) {
    if (!IsEnabled()) return;
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    if (!IsEnabled()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running()) return;
    const bool host = IsHost(s);
    if (!g_saidArm) {
        g_saidArm = true;
        UE_LOGI("[HOSTILE-DRILL] %s -- this peer is the %s", ArmName(),
                host ? "HOST (judge)" : "CLIENT (sender)");
    }
    if (host) TickHost(s);
    else      TickClient(s);
}

void OnDisconnect() {
    g_step = Step::WaitArm;
    g_saidArm = false;
    g_slot = -1;
    g_witnessBase = 0;
    g_armAt = {};
    g_sent = false;
    g_sentinelSignal.clear();
}

}  // namespace coop::dev::hostile_drill
