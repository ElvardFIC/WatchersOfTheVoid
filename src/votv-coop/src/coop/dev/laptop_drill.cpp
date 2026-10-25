// coop/dev/laptop_drill.cpp -- see coop/dev/laptop_drill.h.

#include "coop/dev/laptop_drill.h"

#include "coop/config/config.h"
#include "coop/interactables/floppy_slot_sync.h"
#include "coop/interactables/laptop_buffer_sync.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/save/save_transfer.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/actors/floppy_disc.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/devices/floppy_slot.h"
#include "ue_wrap/devices/laptop.h"
#include "ue_wrap/engine/engine.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace coop::dev::laptop_drill {
namespace {

namespace FS = ue_wrap::floppy_slot;
namespace FD = ue_wrap::floppy_disc;
namespace L  = ue_wrap::laptop;
namespace R  = ue_wrap::reflection;
namespace E  = ue_wrap::engine;
namespace SS = coop::floppy_slot_sync;
namespace QS = coop::laptop_buffer_sync;

constexpr uint64_t kFirstStepMs  = 60000;  // a client's world up to its first insert reaching the host
constexpr uint64_t kStepBoundMs  = 30000;  // one leg's answer
constexpr uint64_t kCheckEveryMs = 250;
constexpr int32_t  kDiscWrites   = 32;
constexpr int32_t  kBufferUid    = 771101;
const wchar_t* const kRowA = L"LAPTOP-DRILL\nA";    // the client's first disc
const wchar_t* const kRowB = L"LAPTOP-DRILL\r\nB";  // its edit after its claim, before the answer
const wchar_t* const kRowE = L"LAPTOP-DRILL\nE";    // the client's second disc
const wchar_t* const kRowH = L"LAPTOP-DRILL\nH";    // the host's word that its eject is armed
const wchar_t* const kRowC = L"LAPTOP-DRILL\nC";    // the client's edit the host's eject races
const wchar_t* const kRowD = L"LAPTOP-DRILL\nD";    // the client's batch on the ejected disc's generation
const wchar_t* const kRowJ = L"LAPTOP-DRILL\nJ";    // the host's disc in a joiner's window
const wchar_t* const kRowK = L"LAPTOP-DRILL\r\nK";  // the host's buffer row in that window
const wchar_t* const kDiscClasses[] = {L"prop_floppyDisc_R_C", L"prop_floppyDisc_G_C", L"prop_floppyDisc_Y_C"};

enum class HStep : uint8_t { Wait, AwaitInsert, AwaitEdit, AwaitClientEject, AwaitSecond, Watch, JoinWindow, JoinEdit,
                             Done };
enum class CStep : uint8_t { Ready, Settle, ClearFirst, Insert, AwaitAnswer, Eject, AwaitEjectAnswer, InsertSecond,
                             AwaitSecondAnswer, AwaitSignal, AwaitHostEject, AwaitStale, JoinCheck, Done };
HStep    g_host = HStep::Wait;
CStep    g_client = CStep::Ready;
uint64_t g_stepMs = 0;
uint64_t g_nextCheckMs = 0;
int      g_session = 1;
void*    g_disc = nullptr;          // CLIENT: the second disc, until the laptop takes it
uint32_t g_genIn = 0;               // CLIENT: the generation of the disc the host's eject races
uint64_t g_markTaken = 0;           // CLIENT: canonicals taken before its edit's answer was due
uint64_t g_markDropped = 0;         // CLIENT: canonicals dropped before the stale one was due
uint32_t g_staleGen = 0;            // HOST: the second disc's generation and its quad, sent again once it is gone
L::BufferQuad g_stale;
bool     g_sentStale = false;

const std::string& Mode() {
    static const std::string s = coop::config::ResolveString(::coop::config_registry::rows::laptop_drill);
    return s;
}
bool Join() { return Mode() == "join"; }
bool Enabled() {
    static const bool on = Mode() == "run" || Join();
    return on;
}
bool Expired(uint64_t bound) { return ::GetTickCount64() - g_stepMs > bound; }

void Fail(const char* what) {
    UE_LOGW("[LAPTOP-DRILL] FAIL in session %d: %s", g_session, what);
    g_host = HStep::Done;
    g_client = CStep::Done;
}
void Abandon(const char* why) {
    UE_LOGW("[LAPTOP-DRILL] ABANDONED in session %d: %s", g_session, why);
    g_host = HStep::Done;
    g_client = CStep::Done;
}
void HGo(HStep s) { g_host = s; g_stepMs = ::GetTickCount64(); }
void CGo(CStep s) { g_client = s; g_stepMs = ::GetTickCount64(); }

void* Laptop() {
    return L::EnsureResolved() && FS::EnsureResolved(FS::DeviceKind::Laptop) ? L::Instance() : nullptr;
}

// The laptop's slot: whether it holds a disc, and its file rows.
bool ReadLaptop(bool& occupied, std::vector<std::wstring>& rows) {
    void* l = Laptop();
    FS::Scalars st;
    L::BufferQuad q;
    if (!l || !FS::ReadScalars(FS::DeviceKind::Laptop, l, st) || !L::ReadQuad(q)) return false;
    occupied = st.floppyType >= 0;
    rows = std::move(q.data);
    return true;
}

bool Has(const std::vector<std::wstring>& rows, const wchar_t* row) {
    return std::find(rows.begin(), rows.end(), std::wstring(row)) != rows.end();
}

// A file row appended as the laptop's own verbs append one: the raw write and the widget's rebuild, no prime, so the
// quad lane ships it as an edit.
bool AppendRow(const wchar_t* row) {
    L::BufferQuad q;
    if (!L::ReadQuad(q)) return false;
    q.data.push_back(row);
    return L::WriteQuadAndRebuild(q);
}

// A row in the laptop's buffer, which no slot carries: only the quad does.
bool AppendBufferRow(const wchar_t* row) {
    L::BufferQuad q;
    if (!L::ReadQuad(q)) return false;
    q.buffer.push_back(row);
    q.bufferUids.push_back(kBufferUid);
    return L::WriteQuadAndRebuild(q);
}

// A disc holding one row, spawned above `where`.
void* SpawnDisc(const ue_wrap::FVector& where, const wchar_t* row) {
    if (!FD::EnsureResolved()) return nullptr;
    void* cls = nullptr;
    for (const wchar_t* name : kDiscClasses)
        if ((cls = R::FindClass(name)) != nullptr) break;
    if (!cls) return nullptr;
    void* disc = E::SpawnActor(cls, {where.X, where.Y, where.Z + 80.f});
    if (!disc) return nullptr;
    FD::DiscContent dc;
    dc.readWrites = kDiscWrites;
    dc.data = {row};
    return FD::WriteDiscContent(disc, dc) ? disc : nullptr;
}

// The laptop's own insert of `disc`; true once the laptop holds it. The laptop refuses one while its last insert or
// eject still animates, and a refused disc stays where it was, so the caller retries on its next check.
bool InsertDisc(void* disc) {
    bool occupied = false;
    std::vector<std::wstring> rows;
    if (disc && R::IsLive(disc)) L::CallInsertDisc(disc);
    return ReadLaptop(occupied, rows) && occupied;
}

// The laptop's own eject; true once its slot is empty. Refused the same way while it animates.
bool EjectDisc() {
    bool occupied = true;
    std::vector<std::wstring> rows;
    if (ReadLaptop(occupied, rows) && occupied) L::CallEjectDisc();
    return ReadLaptop(occupied, rows) && !occupied;
}

// The final quad, which both peers must agree on.
void SayQuad(const char* who) {
    L::BufferQuad q;
    if (!L::ReadQuad(q)) return;
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](const std::wstring& w) {
        for (wchar_t c : w) { h ^= static_cast<uint16_t>(c); h *= 1099511628211ull; }
        h ^= 0x1F; h *= 1099511628211ull;
    };
    for (const auto& s : q.data) mix(s);
    mix(L"|");
    for (const auto& s : q.buffer) mix(s);
    UE_LOGI("[LAPTOP-DRILL] quad %s: rows=%zu buffer=%zu rw=%d hash=%016llx", who, q.data.size(), q.buffer.size(),
            q.readWrites, static_cast<unsigned long long>(h));
}

void HostTick(coop::net::Session* s) {
    bool occupied = false;
    std::vector<std::wstring> rows;
    switch (g_host) {
    case HStep::Wait:
        if (!s->running() || !Laptop()) return;
        if (!Join()) {
            if (s->AnyWorldReadyPeer()) HGo(HStep::AwaitInsert);
            return;
        }
        for (int i = 1; i < coop::net::kMaxPeers; ++i)
            if (coop::save_transfer::WorldTakenFor(i) && !s->IsSlotWorldReady(i)) HGo(HStep::JoinWindow);
        if (g_host == HStep::Wait && s->AnyWorldReadyPeer())
            Abandon("a joiner's world was ready before the host saw its window");
        return;
    case HStep::AwaitInsert:
        if (!ReadLaptop(occupied, rows) || !occupied || !Has(rows, kRowA)) {
            if (Expired(kFirstStepMs)) Fail("the client's disc never reached the host's laptop");
            return;
        }
        UE_LOGI("[LAPTOP-DRILL] host: the client's disc is in its laptop");
        HGo(HStep::AwaitEdit);
        return;
    case HStep::AwaitEdit:
        if (!ReadLaptop(occupied, rows) || !Has(rows, kRowB)) {
            if (Expired(kStepBoundMs)) Fail("the client's edit made after its claim never reached the host's laptop");
            return;
        }
        UE_LOGI("[LAPTOP-DRILL] host: the client's edit made while its claim was unanswered is in its laptop");
        HGo(HStep::AwaitClientEject);
        return;
    case HStep::AwaitClientEject:
        if (!ReadLaptop(occupied, rows) || occupied) {
            if (Expired(kStepBoundMs)) Fail("the client's eject never emptied the host's laptop");
            return;
        }
        UE_LOGI("[LAPTOP-DRILL] host: the client's eject emptied its laptop");
        HGo(HStep::AwaitSecond);
        return;
    case HStep::AwaitSecond:
        if (!ReadLaptop(occupied, rows) || !occupied || !Has(rows, kRowE)) {
            if (Expired(kStepBoundMs)) Fail("the client's second disc never reached the host's laptop");
            return;
        }
        // The eject is armed before the word goes out, so the client's next batch, sent once the word reaches it, is
        // the one it meets.
        QS::DevEjectAtNextBatch();
        if (!AppendRow(kRowH) || !L::ReadQuad(g_stale)) {
            Abandon("the host could not append its word to its laptop's files");
            return;
        }
        g_staleGen = SS::LaptopGeneration();
        UE_LOGI("[LAPTOP-DRILL] host: the second disc is in its laptop (generation %u); its eject is armed ahead of "
                "the client's next batch", g_staleGen);
        HGo(HStep::Watch);
        return;
    case HStep::Watch:
        // Until the session ends: an edit made on the ejected disc must never land on this laptop.
        if (!ReadLaptop(occupied, rows)) return;
        if (Has(rows, kRowC) || Has(rows, kRowD)) {
            Fail("an edit made on the ejected disc landed on the host's laptop");
            return;
        }
        if (g_sentStale || QS::ReadCounts().batchesRefused < 2 || !SS::LaptopOccupancySettled()) return;
        if (occupied) {
            Abandon("the host's laptop still holds a disc after refusing both edits");
            return;
        }
        // The ejected disc's canonical on its own generation, arriving after the eject's: what a canonical minted
        // before a change of the disc would be.
        if (!QS::DevSendCanonicalOn(g_staleGen, g_stale)) {
            Abandon("the host could not send the ejected disc's canonical");
            return;
        }
        g_sentStale = true;
        UE_LOGI("[LAPTOP-DRILL] host: both edits made on the ejected disc refused; its canonical sent on generation %u, "
                "the laptop now on %u", g_staleGen, SS::LaptopGeneration());
        SayQuad("host");
        return;
    case HStep::JoinWindow: {
        ue_wrap::FVector at{};
        void* disc = E::TryGetActorLocation(Laptop(), at) ? SpawnDisc(at, kRowJ) : nullptr;
        if (!disc || !InsertDisc(disc)) {
            Abandon("the host could not put a disc in its laptop in the joiner's window");
            return;
        }
        HGo(HStep::JoinEdit);
        return;
    }
    case HStep::JoinEdit:
        if (!AppendBufferRow(kRowK)) {
            Abandon("the host could not append to its laptop's buffer");
            return;
        }
        UE_LOGI("[LAPTOP-DRILL] host (join): a disc and a buffer row in its laptop while the joiner loads");
        SayQuad("host");
        g_host = HStep::Done;
        return;
    default:
        return;
    }
}

void ClientTick(void* player) {
    bool occupied = false;
    std::vector<std::wstring> rows;
    switch (g_client) {
    case CStep::Ready:
        if (!coop::net_pump::HasAnnouncedWorldReady() || !Laptop() ||
            coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle)
            return;
        CGo(Join() ? CStep::JoinCheck : CStep::Settle);
        return;
    case CStep::Settle:
        // The host's slot has landed here: a claim before it is a claim of this client's own save.
        if (!SS::LaptopOccupancySettled() || !ReadLaptop(occupied, rows)) {
            if (Expired(kStepBoundMs)) Fail("this client's laptop never settled on the host's slot");
            return;
        }
        CGo(occupied ? CStep::ClearFirst : CStep::Insert);
        return;
    case CStep::ClearFirst:
        // A world whose laptop holds a disc: out through its own eject first, and the host's answer.
        if (!EjectDisc() || !SS::LaptopOccupancySettled()) {
            if (Expired(kStepBoundMs)) Abandon("the laptop's disc from the save would not come out");
            return;
        }
        CGo(CStep::Insert);
        return;
    case CStep::Insert: {
        ue_wrap::FVector at{};
        void* disc = E::TryGetActorLocation(player, at) ? SpawnDisc(at, kRowA) : nullptr;
        if (!disc || !InsertDisc(disc)) {
            Abandon("the client could not put a disc in its laptop");
            return;
        }
        // The claim now, and an edit after it in the same frame: the host cannot have answered it yet.
        g_markTaken = QS::ReadCounts().canonicalsTaken;
        SS::DevClaimLaptopNow();
        if (!AppendRow(kRowB)) {
            Abandon("the client could not append to its laptop's files");
            return;
        }
        CGo(CStep::AwaitAnswer);
        return;
    }
    case CStep::AwaitAnswer:
        // The host's answer to the claim, and then its canonical answering the edit, which goes once the claim's is in:
        // an eject before that would take the edit out with the disc.
        if (!SS::LaptopOccupancySettled() || QS::ReadCounts().canonicalsTaken <= g_markTaken) {
            if (Expired(kStepBoundMs)) Fail("the host never answered this client's insert and its edit");
            return;
        }
        CGo(CStep::Eject);
        return;
    case CStep::Eject:
        if (!EjectDisc()) {
            if (Expired(kStepBoundMs)) Abandon("the laptop would not eject this client's disc");
            return;
        }
        CGo(CStep::AwaitEjectAnswer);
        return;
    case CStep::AwaitEjectAnswer:
        if (!SS::LaptopOccupancySettled()) {
            if (Expired(kStepBoundMs)) Fail("the host never answered this client's eject");
            return;
        }
        CGo(CStep::InsertSecond);
        return;
    case CStep::InsertSecond: {
        if (!g_disc) {
            ue_wrap::FVector at{};
            g_disc = E::TryGetActorLocation(player, at) ? SpawnDisc(at, kRowE) : nullptr;
            if (!g_disc) {
                Abandon("the client could not spawn its second disc");
                return;
            }
        }
        if (!InsertDisc(g_disc)) {
            if (Expired(kStepBoundMs)) Abandon("the laptop would not take this client's second disc");
            return;
        }
        g_disc = nullptr;  // destroyed by the insert
        CGo(CStep::AwaitSecondAnswer);
        return;
    }
    case CStep::AwaitSecondAnswer:
        if (!SS::LaptopOccupancySettled()) {
            if (Expired(kStepBoundMs)) Fail("the host never answered this client's second insert");
            return;
        }
        CGo(CStep::AwaitSignal);
        return;
    case CStep::AwaitSignal:
        if (!ReadLaptop(occupied, rows) || !Has(rows, kRowH)) {
            if (Expired(kStepBoundMs)) Fail("the host's word on the second disc never reached this client");
            return;
        }
        // The edit the host's armed eject meets: made on the disc this laptop holds, which the host takes away first.
        g_genIn = SS::LaptopGeneration();
        if (!AppendRow(kRowC)) {
            Abandon("the client could not append the edit the eject races");
            return;
        }
        CGo(CStep::AwaitHostEject);
        return;
    case CStep::AwaitHostEject:
        if (!ReadLaptop(occupied, rows) || occupied || !rows.empty() || !SS::LaptopOccupancySettled() ||
            SS::LaptopGeneration() == g_genIn) {
            if (Expired(kStepBoundMs)) Fail("the host's eject never emptied this client's laptop");
            return;
        }
        // A batch made on the disc the host ejected, arriving after the eject was published.
        g_markDropped = QS::ReadCounts().canonicalsDropped;
        if (!QS::DevSendAppendOn(g_genIn, kRowD)) {
            Abandon("the client could not send its batch on the ejected disc's generation");
            return;
        }
        UE_LOGI("[LAPTOP-DRILL] client: the host's eject emptied its laptop (generation %u -> %u); a batch on the old "
                "one sent", g_genIn, SS::LaptopGeneration());
        CGo(CStep::AwaitStale);
        return;
    case CStep::AwaitStale:
        if (!ReadLaptop(occupied, rows)) return;
        if (!rows.empty() || occupied) {
            Fail("a canonical of the ejected disc was applied to this client's empty laptop");
            return;
        }
        if (QS::ReadCounts().canonicalsDropped <= g_markDropped) {
            if (Expired(kStepBoundMs)) Fail("the host's canonical of the ejected disc never arrived here to be dropped");
            return;
        }
        SayQuad("client");
        UE_LOGI("[LAPTOP-DRILL] client DONE in session %d (run): its insert, its edit after the claim and its eject "
                "reached the host; the host's eject beat its next edit, and a batch and a canonical of the ejected disc "
                "landed nowhere -- PASS", g_session);
        g_client = CStep::Done;
        return;
    case CStep::JoinCheck: {
        L::BufferQuad q;
        const bool read = ReadLaptop(occupied, rows) && L::ReadQuad(q);
        if (!read || !occupied || !Has(rows, kRowJ) || !Has(q.buffer, kRowK)) {
            if (Expired(kStepBoundMs)) Fail("the joiner's laptop does not hold the host's disc and its buffer row");
            return;
        }
        SayQuad("client");
        UE_LOGI("[LAPTOP-DRILL] client DONE in session %d (join): the host's disc came with the slot's connect set and "
                "its buffer row with the quad's -- PASS", g_session);
        g_client = CStep::Done;
        return;
    }
    default:
        return;
    }
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (!Enabled() || !s) return;
    const uint64_t now = ::GetTickCount64();
    if (now < g_nextCheckMs) return;
    g_nextCheckMs = now + kCheckEveryMs;
    if (s->role() == coop::net::Role::Host) {
        if (g_host != HStep::Done) HostTick(s);
        return;
    }
    if (!s->connected() || g_client == CStep::Done) return;
    if (void* player = coop::players::Registry::Get().Local()) ClientTick(player);
}

void OnDisconnect() {
    if (!Enabled()) return;
    ++g_session;
    g_host = HStep::Wait;
    g_client = CStep::Ready;
    g_stepMs = g_nextCheckMs = 0;
    g_disc = nullptr;
    g_genIn = g_staleGen = 0;
    g_markTaken = g_markDropped = 0;
    g_stale = L::BufferQuad{};
    g_sentStale = false;
}

}  // namespace coop::dev::laptop_drill
