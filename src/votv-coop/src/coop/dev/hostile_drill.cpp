// coop/dev/hostile_drill.cpp -- see coop/dev/hostile_drill.h.

#include "coop/dev/hostile_drill.h"

#include "coop/comms/chat_feed.h"
#include "coop/comms/chat_log.h"
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/element/registry.h"
#include "coop/interactables/signal_sync.h"
#include "coop/interactables/signal_wire.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/end_reason.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/desk/saved_signals.h"
#include "ue_wrap/desk/signal_dynamic.h"
#include "ue_wrap/engine/engine.h"

#include <algorithm>
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

enum class Arm { Off, SignalPark, ChatIllFormed, PropConvert, ReliableFlood };

// Resolved once: with the environment variable unset every read would scan the ini on disk, and the
// drill is asked per tick.
Arm ArmOf() {
    static const Arm a = [] {
        const std::string v =
            coop::config::ResolveEnum(::coop::config_registry::rows::hostile_drill);
        return v == "signal_park"     ? Arm::SignalPark
             : v == "chat_ill_formed" ? Arm::ChatIllFormed
             : v == "prop_convert"    ? Arm::PropConvert
             : v == "reliable_flood"  ? Arm::ReliableFlood
                                      : Arm::Off;
    }();
    return a;
}

const char* ArmName() {
    switch (ArmOf()) {
    case Arm::SignalPark:    return "signal_park";
    case Arm::ChatIllFormed: return "chat_ill_formed";
    case Arm::PropConvert:   return "prop_convert";
    case Arm::ReliableFlood: return "reliable_flood";
    default:                 return "off";
    }
}

// The forged content, named once and shared by the sender and the host's judge so the two never drift.
constexpr const char*    kChatForged         = "hostile-drill forged";    // + one lone 0x80 appended
constexpr const char*    kChatSentinel       = "hostile-drill sentinel";  // one well-formed line behind it
constexpr const wchar_t* kSignalSentinelName = L"hostile-drill sentinel";
constexpr const wchar_t* kSignalId           = L"hostile-drill";  // every row the drill sends
constexpr int            kForgedSignals = 300;        // > kParkCap (256): enough to fill a park that keeps them
constexpr uint32_t       kSeqBase = 0xB0570000u;      // outside the signal lane's own seq (starts at 1)
constexpr auto           kFailBound = std::chrono::seconds(180);  // a failure bound, never a wait
// prop_convert: the trash class both messages name, and two element ids at the top of the peer range
// ([32768, 65536)), far from any id a run allocates; the judge refuses to run if either resolves at arm.
constexpr const char*    kTrashClass   = "prop_garbageClump_C";
constexpr uint32_t       kConvertEid   = 65530u;
constexpr uint32_t       kSentinelEid  = 65531u;
// reliable_flood: two bursts of a kind only the host sends, each followed by a chat line on the same ordered lane.
// The first is under the most reliable messages an honest client is allowed to send at once (65536, refilled at
// 8000 a second) and the second far over it, with time to refill counted in; so the host must keep the link after
// the first and end it after the second, with its code for a sender past that bound.
constexpr int            kFloodUnder = 49152;     // three quarters of 65536
constexpr int            kFloodOver  = 251072;    // twice 65536, plus 15 s of 8000 a second
constexpr const char*    kFloodEndId = "MV-H32";  // the code's id string, so the judge needs no symbol of the fix

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_saidArm = false;

// Host (receiver / judge). The judge counts only what arrives after it arms: a sentinel left in the
// save or the lobby record by an earlier run must never read as this run's.
enum class Step { WaitArm, Judge, Done };
Step              g_step = Step::WaitArm;
int               g_slot = -1;
uint64_t          g_witnessBase = 0;
int               g_sentinelBase = 0;   // signal_park: sentinel rows already saved at arm
int32_t           g_rowsSeen = -1;      // signal_park: the row count the last scan saw
SS::RowKey        g_tailSeen{};         // signal_park: and the last row then
uint32_t          g_lineSeqBase = 0;    // chat: the last line committed before arm
Clock::time_point g_armAt{};

// Client (sender).
bool         g_sent = false;
std::wstring g_sentinelSignal;  // signal_park: an ordinary template name, resolved once
// reliable_flood: the client judges, since the host's answer is this session ending. The sentinels carry a per-run
// stamp, so a line the host's lobby record kept from an earlier run, and seeds at join, never reads as this run's.
enum class Flood { Idle, UnderSent, OverSent, Done };
Flood             g_flood = Flood::Idle;
std::string       g_floodStamp;
Clock::time_point g_floodAt{};

bool IsHost(coop::net::Session* s) { return s->role() == coop::net::Role::Host; }

void Pass(const char* detail) {
    UE_LOGI("[HOSTILE-DRILL] %s PASS -- %s", ArmName(), detail);
    g_step = Step::Done;
}

// Every FAIL carries a reason token, so an expected fail is never confused with a timeout or a dropped
// link: the rig matches the exact string, not a bare "FAIL".
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
        r.id     = kSignalId;
        r.signal = L"hostile-drill";  // a name of no template; size 0 is what the game refuses
        r.size = 0.f;                 // the game's own gate: succ is false unless decoded >= size > 0, so
        r.decoded = 0.f;              // every forged row is refused BEFORE the "Create Signal List" rebuild
        const std::vector<uint8_t> blob = coop::signal_wire::Serialize(r);
        if (coop::blob_chunks::SendBlob(s, coop::net::ReliableKind::SavedSignalAppend,
                                        kSeqBase + static_cast<uint32_t>(i), blob))
            ++sent;
    }
    // The sentinel goes only behind the whole burst: on the ordered lane its arrival then means every
    // forged row arrived and was judged first. A burst that did not all go out is no attack; the
    // sender says so and sends no sentinel, so the host's judge cannot pass.
    if (sent != kForgedSignals) {
        UE_LOGW("[HOSTILE-DRILL] signal_park sent %d/%d forged row(s)", sent, kForgedSignals);
        Fail("the forged rows were not all sent");
        return;
    }
    // One honest row the native saves (decoded >= size > 0), under a real ordinary template name so
    // there is no unique-signal `specials` side effect.
    SD::Row sentinel;
    sentinel.name    = kSignalSentinelName;
    sentinel.id      = kSignalId;
    sentinel.signal  = g_sentinelSignal;
    sentinel.size    = 1.f;
    sentinel.decoded = 1.f;
    sentinel.level   = 1;
    sentinel.downloadedAtQuality = 1.f;
    sentinel.date    = 0;  // a non-positive date: the native stamps Now
    const std::vector<uint8_t> sb = coop::signal_wire::Serialize(sentinel);
    if (!coop::blob_chunks::SendBlob(s, coop::net::ReliableKind::SavedSignalAppend,
                                     kSeqBase + kForgedSignals, sb)) {
        Fail("the sentinel was not sent");
        return;
    }
    UE_LOGI("[HOSTILE-DRILL] signal_park sent %d/%d forged row(s) + sentinel; signal '%ls'", sent,
            kForgedSignals, g_sentinelSignal.c_str());
}

void SendChatIllFormed(coop::net::Session* s) {
    coop::net::ChatMessagePayload forged{};
    const size_t fn = std::strlen(kChatForged);
    std::memcpy(forged.text, kChatForged, fn);
    forged.text[fn] = static_cast<char>(0x80);  // a lone continuation byte: ill-formed UTF-8 unconditionally
    forged.len = static_cast<uint8_t>(fn + 1);
    if (!s->SendReliable(coop::net::ReliableKind::ChatMessage, &forged, sizeof(forged))) {
        Fail("the forged line was not sent");  // and no sentinel behind it, so the host cannot pass
        return;
    }
    coop::net::ChatMessagePayload sentinel{};
    const size_t sn = std::strlen(kChatSentinel);
    std::memcpy(sentinel.text, kChatSentinel, sn);
    sentinel.len = static_cast<uint8_t>(sn);
    if (!s->SendReliable(coop::net::ReliableKind::ChatMessage, &sentinel, sizeof(sentinel))) {
        Fail("the sentinel was not sent");
        return;
    }
    UE_LOGI("[HOSTILE-DRILL] %s sent the forged line + sentinel", ArmName());
}

template <typename Wire>
void PutClassName(Wire& w, const char* cls) {
    const size_t n = std::strlen(cls);
    std::memcpy(w.data, cls, n);
    w.len = static_cast<uint8_t>(n);
}

// A convert no honest client sends -- the trash channel's re-skin, the host's alone -- of a trash pile at
// this client's own feet, then one honest trash spawn behind it on the same ordered lane (both Bulk): the
// host dispatches the convert before the spawn, so the spawn's element appearing on the host means the
// convert has been judged.
void SendPropConvert(coop::net::Session* s, const ue_wrap::FVector& at) {
    coop::net::PropConvertPayload c{};
    c.oldEid = kConvertEid;
    c.newEid = kConvertEid;
    PutClassName(c.pileClass, kTrashClass);
    c.locX = at.X + 120.f;
    c.locY = at.Y;
    c.locZ = at.Z;
    c.scaleX = c.scaleY = c.scaleZ = 1.f;
    c.kind = coop::net::propconvert_kind::kToPile;
    if (!s->SendReliable(coop::net::ReliableKind::PropConvert, &c, sizeof(c))) {
        Fail("the forged convert was not sent");  // and no sentinel behind it, so the host cannot pass
        return;
    }
    coop::net::PropSpawnPayload p{};
    PutClassName(p.className, kTrashClass);
    p.locX = at.X - 120.f;
    p.locY = at.Y;
    p.locZ = at.Z;
    p.scaleX = p.scaleY = p.scaleZ = 1.f;
    p.elementId = kSentinelEid;
    if (!s->SendPropSpawn(p)) {
        Fail("the sentinel was not sent");
        return;
    }
    UE_LOGI("[HOSTILE-DRILL] %s sent the convert (eid %u) + sentinel spawn (eid %u)", ArmName(), kConvertEid,
            kSentinelEid);
}

std::string FloodSentinel(int which) {
    return "hostile-drill flood sentinel " + std::to_string(which) + " " + g_floodStamp;
}

// Is the host's copy of this client's sentinel line in this client's chat record? The host commits a client's line and
// sends it back to every client, the sender included.
bool FeedHas(const std::string& line) {
    bool found = false;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        if (r.line.find(line) != std::string_view::npos) found = true;
    });
    return found;
}

// `count` messages of a kind only the host authors, one byte each, then one chat line: all on the Normal lane,
// which GNS keeps in order, so the line arriving at the host means every message before it has arrived.
bool SendFloodBurst(coop::net::Session* s, int count, const std::string& sentinel) {
    const uint8_t filler = 0;
    for (int i = 0; i < count; ++i) {
        if (!s->SendReliable(coop::net::ReliableKind::ChatLine, &filler, sizeof(filler))) {
            UE_LOGW("[HOSTILE-DRILL] reliable_flood sent %d/%d before a refusal", i, count);
            return false;
        }
    }
    coop::net::ChatMessagePayload line{};
    const size_t n = std::min(sentinel.size(), sizeof(line.text));
    std::memcpy(line.text, sentinel.data(), n);
    line.len = static_cast<uint8_t>(n);
    return s->SendReliable(coop::net::ReliableKind::ChatMessage, &line, sizeof(line));
}

void TickClientFlood(coop::net::Session* s) {
    if (g_flood == Flood::Done) return;
    if (g_flood == Flood::Idle) {
        if (!coop::net_pump::HasAnnouncedWorldReady() ||
            coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle)
            return;
        // The bursts must arrive faster than the host refills, which an unpinned link opening at its measured rate need
        // not do: a FAIL then would be the link's, not the host's.
        if (coop::config::ResolveInt(::coop::config_registry::rows::net_sendrate_kbs) < 4096) {
            Fail("the link is not pinned (net.sendrate_kbs below 4096)");
            g_flood = Flood::Done;
            return;
        }
        g_floodStamp = std::to_string(static_cast<unsigned long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count()));
        if (!SendFloodBurst(s, kFloodUnder, FloodSentinel(1))) {
            Fail("the first burst was not sent");
            g_flood = Flood::Done;
            return;
        }
        UE_LOGI("[HOSTILE-DRILL] reliable_flood sent %d + sentinel 1", kFloodUnder);
        g_flood = Flood::UnderSent;
        g_floodAt = Clock::now();
        return;
    }
    if (g_flood == Flood::UnderSent && FeedHas(FloodSentinel(1))) {
        if (!SendFloodBurst(s, kFloodOver, FloodSentinel(2))) {
            Fail("the second burst was not sent");
            g_flood = Flood::Done;
            return;
        }
        UE_LOGI("[HOSTILE-DRILL] reliable_flood sentinel 1 came back; sent %d + sentinel 2", kFloodOver);
        g_flood = Flood::OverSent;
        g_floodAt = Clock::now();
        return;
    }
    if (g_flood == Flood::OverSent && FeedHas(FloodSentinel(2))) {
        Fail("flood-accepted");
        g_flood = Flood::Done;
        return;
    }
    if (Clock::now() - g_floodAt > kFailBound) {
        Fail("timeout");
        g_flood = Flood::Done;
    }
}

// The session ended while a burst was out: the host's code decides. Before sentinel 1 came back, a burst under the
// bound was cut; after it, only the host's flood code passes.
void JudgeFloodEnd() {
    if (g_flood != Flood::UnderSent && g_flood != Flood::OverSent) return;
    coop::join_progress::Notice n;
    const char* id = coop::join_progress::PeekNotice(n) ? coop::net::Describe(n.code).id : "";
    const bool floodCode = std::strcmp(id, kFloodEndId) == 0;
    UE_LOGI("[HOSTILE-DRILL] reliable_flood: the session ended with code '%s'", id);
    if (g_flood == Flood::UnderSent)
        Fail(floodCode ? "kicked-under-burst" : "closed-otherwise");
    else if (floodCode)
        Pass("the host kept the link after the first burst and ended it after the second");
    else
        Fail("closed-otherwise");
    g_flood = Flood::Done;
}

void TickClient(coop::net::Session* s) {
    if (ArmOf() == Arm::ReliableFlood) {
        TickClientFlood(s);
        return;
    }
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
    } else if (ArmOf() == Arm::PropConvert) {
        ue_wrap::FVector at{};
        void* pawn = coop::players::Registry::Get().Local();
        if (!pawn || !ue_wrap::engine::TryGetActorLocation(pawn, at)) return;  // no body yet; wait
        SendPropConvert(s, at);
    } else {
        SendChatIllFormed(s);
    }
    g_sent = true;
}

// --- the judge (host) ---

int CountSignalSentinels() {
    int found = 0;
    const int32_t n = SS::Count();
    for (int32_t i = 0; i < n; ++i) {
        SD::Row r;
        if (SS::ReadRow(i, r) && r.name == kSignalSentinelName) ++found;
    }
    return found;
}

uint32_t LastLineSeq() {
    uint32_t last = 0;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) { if (r.lineSeq > last) last = r.lineSeq; });
    return last;
}

void ArmOnFirstReadySlot(coop::net::Session* s) {
    // signal_park's verdict means something only on a settled lane: an unsettled one parks every
    // arrival by design, and the pause that would raise is the settle's, not the attack's.
    if (ArmOf() == Arm::SignalPark && !coop::signal_sync::Settled()) return;
    for (int slot = 1; slot < static_cast<int>(coop::net::kMaxPeers) && g_slot < 0; ++slot)
        if (s->IsSlotWorldReady(slot)) g_slot = slot;
    if (g_slot < 0) return;
    // The witness: a monotonic count of a kind's apply park going from off to on. Baselined here, on
    // the game thread, in the same frame loop that drains the inbox the burst arrives on.
    g_witnessBase = s->ApplyPauseRaises();
    if (ArmOf() == Arm::PropConvert) {
        // Either id already bound would make the verdict about an earlier run's element, not this one's.
        auto& reg = coop::element::Registry::Get();
        if (reg.Get(kConvertEid) || reg.Get(kSentinelEid)) {
            Fail("the drill's element ids were in use at arm");
            return;
        }
    } else if (ArmOf() == Arm::SignalPark) {
        g_sentinelBase = CountSignalSentinels();
        g_rowsSeen = SS::Count();
        if (g_rowsSeen > 0) SS::ReadRowKey(g_rowsSeen - 1, g_tailSeen);
    } else {
        g_lineSeqBase = LastLineSeq();
    }
    g_armAt = Clock::now();
    g_step = Step::Judge;
    UE_LOGI("[HOSTILE-DRILL] %s armed on slot %d (apply-pause raises baseline %llu)", ArmName(),
            g_slot, static_cast<unsigned long long>(g_witnessBase));
}

void JudgeSignalPark(coop::net::Session* s) {
    // The attack: rows the game refuses, more than the apply park holds. A refused row that is final
    // never parks, so the witness never rises; a park that keeps them fills past kParkCap and it does --
    // FAIL, printed the tick it is seen, naming the kind still holding the pause (the witness counts
    // raises, not kinds, so a pause already lowered is said as such).
    if (s->ApplyPauseRaises() > g_witnessBase) {
        Fail(s->ApplyPausedBy(coop::net::ReliableKind::SavedSignalAppend)
                 ? "the receive paused on SavedSignalAppend's apply park"
             : s->ApplyPaused() ? "the receive paused on another lane's apply park"
                                : "the receive paused on an apply park, lowered before the judge looked");
        return;
    }
    // The rows are read only when the array changed: its count, or its last row, where the sentinel's
    // append lands (an append and a removal in one tick leave the count alone, not the tail).
    const int32_t n = SS::Count();
    SS::RowKey tail{};
    if (n > 0) SS::ReadRowKey(n - 1, tail);
    if (n == g_rowsSeen && tail == g_tailSeen) return;
    g_rowsSeen = n;
    g_tailSeen = tail;
    if (CountSignalSentinels() > g_sentinelBase)
        Pass("the apply park never paused the receive loop; an honest saved signal still applied");
}

void JudgeChat() {
    bool found0x80 = false, foundSentinel = false;
    coop::chat_log::ForEach([&](const coop::chat_log::Row& r) {
        if (r.lineSeq <= g_lineSeqBase || r.slot != static_cast<uint8_t>(g_slot)) return;
        if (r.text == kChatSentinel) foundSentinel = true;
        if (r.text.find(static_cast<char>(0x80)) != std::string::npos) found0x80 = true;
    });
    // Decide only once the sentinel (sent AFTER the forged line on the same ordered lane) is committed:
    // that is the signal that the forged line has already been judged.
    if (!foundSentinel) return;
    if (found0x80)
        Fail("an ill-formed line was committed");
    else
        Pass("the ill-formed line was refused; the well-formed one was kept");
}

void JudgePropConvert() {
    // Decide only once the sentinel spawn (sent AFTER the convert on the same ordered lane) is bound: the
    // convert has been judged by then.
    auto& reg = coop::element::Registry::Get();
    if (!reg.Get(kSentinelEid)) return;
    if (reg.Get(kConvertEid))
        Fail("convert-applied");
    else
        Pass("a client's convert was not applied; the spawn sent behind it was");
}

void Judge(coop::net::Session* s) {
    if (!s->HasPeerConn(g_slot)) { Fail("the judged slot left before the verdict"); return; }
    if (ArmOf() == Arm::SignalPark)       JudgeSignalPark(s);
    else if (ArmOf() == Arm::PropConvert) JudgePropConvert();
    else                                  JudgeChat();
    if (g_step == Step::Judge && Clock::now() - g_armAt > kFailBound)
        Fail("timeout, the sentinel never arrived");
}

void TickHost(coop::net::Session* s) {
    if (g_step == Step::WaitArm)      ArmOnFirstReadySlot(s);
    else if (g_step == Step::Judge)   Judge(s);
}

}  // namespace

bool IsEnabled() { return ArmOf() != Arm::Off; }

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
                ArmOf() == Arm::ReliableFlood ? (host ? "HOST (under test)" : "CLIENT (sender and judge)")
                                              : (host ? "HOST (judge)" : "CLIENT (sender)"));
    }
    if (host) {
        if (ArmOf() != Arm::ReliableFlood) TickHost(s);  // reliable_flood: the client judges
    } else {
        TickClient(s);
    }
}

void OnDisconnect() {
    if (ArmOf() == Arm::ReliableFlood) JudgeFloodEnd();
    g_step = Step::WaitArm;
    g_saidArm = false;
    g_slot = -1;
    g_witnessBase = 0;
    g_sentinelBase = 0;
    g_rowsSeen = -1;
    g_tailSeen = {};
    g_lineSeqBase = 0;
    g_armAt = {};
    g_sent = false;
    g_sentinelSignal.clear();
    g_flood = Flood::Idle;
    g_floodStamp.clear();
    g_floodAt = {};
}

}  // namespace coop::dev::hostile_drill
