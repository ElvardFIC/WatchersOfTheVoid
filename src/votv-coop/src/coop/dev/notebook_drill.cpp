// coop/dev/notebook_drill.cpp -- see coop/dev/notebook_drill.h.

#include "coop/dev/notebook_drill.h"

#include "coop/comms/chat_feed.h"
#include "coop/comms/chat_sync.h"
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/dev/director/director.h"
#include "coop/dev/director/routes.h"
#include "coop/dev/drill_props.h"
#include "coop/element/element.h"
#include "coop/element/registry.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/interactables/notebook_sync.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/player/puppet_drive.h"
#include "coop/player/remote_player.h"
#include "coop/props/prop_save_data.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady

#include "ue_wrap/actors/notebook.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/prop_use.h"
#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/desk/device_screen.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_attach.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coop::dev::notebook_drill {
namespace {

namespace E  = ue_wrap::engine;
namespace EL = coop::element;
namespace OI = ue_wrap::object_index;
namespace R  = ue_wrap::reflection;
namespace N  = ue_wrap::notebook;
namespace PU = ue_wrap::prop_use;
namespace DP = coop::dev::drill_props;
namespace DS = ue_wrap::device_screen;
namespace D  = coop::director;
using ue_wrap::FVector;

constexpr const wchar_t* kBookPath = L"/Game/objects/prop_notebook.prop_notebook_C";
constexpr const wchar_t* kBookClass = L"prop_notebook_C";
// prop_notebook_C::actionOptionIndex runs the editor's open for this action, the only one getActionOptions lists.
constexpr uint8_t kActOpen = 4;
constexpr int     kClientSlot = 1;
constexpr uint8_t kOtherSlot = 6;   // a slot that holds nothing: the rule is asked about it directly
constexpr const wchar_t* kSeedPage   = L"notebook drill: the page the host wrote";
constexpr const wchar_t* kPeriodicPage = L"notebook drill: the page the client typed and left open";
constexpr const wchar_t* kEditedPage = L"notebook drill: the page the client wrote";

constexpr char kLineJudged[]  = "[NOTE-DRILL] host judged the claim";
constexpr char kLineHasText[] = "[NOTE-DRILL] host read the client's page";
constexpr char kLinePeriodic[] = "[NOTE-DRILL] host read the periodic page";

constexpr float kAheadCm[]   = {200.f, 250.f, 500.f, 700.f, 900.f};
constexpr float kFanDeg[]    = {0.f, 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f};
constexpr float kDropCm      = 40.f;   // above the floor, so the notebook settles
constexpr float kFloorDownCm = 400.f;
constexpr float kMinColumnCm = 20.f;
constexpr float kReachCm     = 150.f;  // a route must end this near the notebook; the director's stop
constexpr int   kWalkDeadlineS = 300;
// Failure bounds only: each leg ends on the state it waits for, and these say it never came.
constexpr uint64_t kCheckEveryMs     = 250;
constexpr uint64_t kSpawnBoundMs     = 60000;   // HOST: a floor with a route round the puppet
constexpr uint64_t kEidBoundMs       = 30000;   // HOST: the dropped notebook takes its element id and key
constexpr uint64_t kSeenBoundMs      = 120000;  // CLIENT: the host's notebook and its seed page stand in this world
constexpr uint64_t kClientLegBoundMs = 480000;  // HOST: the client's walk (300 s) and its press; OBSERVER: the same
constexpr uint64_t kLegBoundMs       = 60000;   // one change crossing, or the other peer's word

enum class Mode : uint8_t { Off, Run, Rejoin, Red };

Mode ModeOf() {
    static const Mode mode = [] {
        const std::string v = coop::config::ResolveString(::coop::config_registry::rows::notebook_drill);
        return v == "run" ? Mode::Run : v == "rejoin" ? Mode::Rejoin : v == "red" ? Mode::Red : Mode::Off;
    }();
    return mode;
}

uint64_t NowMs() { return ::GetTickCount64(); }
bool Due(uint64_t& next, uint64_t every) {
    const uint64_t now = NowMs();
    if (now < next) return false;
    next = now + every;
    return true;
}

void Say(const char* line) { coop::chat_sync::QueueSend(line); }

bool HeardFromHost(const char* line) {
    bool found = false;
    coop::chat_feed::ForEachRow([&](const coop::chat_feed::RowView& r) {
        if (!found && r.line.find(line) != std::string_view::npos) found = true;
    });
    return found;
}

// ---- what a peer sees of the notebooks --------------------------------------------------------

void OnBook(void* ctx, void* obj, int32_t) { static_cast<std::vector<void*>*>(ctx)->push_back(obj); }

struct Found {
    void* seeded = nullptr;   // the notebook whose first page is the host's seed
    void* written = nullptr;  // the one whose first page is the client's
};

Found FindBook() {
    Found f;
    void* const cls = OI::ClassByName(kBookClass);
    if (!cls) return f;
    std::vector<void*> books;
    OI::ForEachInstance(cls, &OnBook, &books);
    for (void* b : books) {
        std::vector<std::wstring> pages;
        if (!N::ReadPages(b, pages) || pages.empty()) continue;
        if (pages[0] == kSeedPage) f.seeded = b;
        else if (pages[0] == kEditedPage) f.written = b;
    }
    return f;
}

std::wstring FirstPage(void* book) {
    std::vector<std::wstring> pages;
    return N::ReadPages(book, pages) && !pages.empty() ? pages[0] : std::wstring(L"(no page read)");
}

// ---- HOST ---------------------------------------------------------------------------------------

enum class HStep : uint8_t { Arm, Settle, AwaitClaim, ClaimWait, AwaitPeriodic, AwaitText, AwaitFree, Done };
HStep    g_h = HStep::Arm;
uint64_t g_hStepMs = 0, g_hNextMs = 0, g_armedMs = 0;
int      g_spawnTries = 0;
ue_wrap::CachedObjRef g_book;
std::wstring g_claim;
coop::notebook_sync::Stats g_statsBefore;

void HEnter(HStep s) {
    g_h = s;
    g_hStepMs = NowMs();
    g_hNextMs = 0;
}
bool HExpired(uint64_t bound) { return NowMs() - g_hStepMs > bound; }

void HostFail(const char* leg, const std::string& why) {
    UE_LOGW("[NOTE-DRILL] FAIL host %s: %s", leg, why.c_str());
    g_h = HStep::Done;
}
void HostAbandon(const std::string& why) {
    UE_LOGW("[NOTE-DRILL] ABANDONED host: %s", why.c_str());
    g_h = HStep::Done;
}

// The first floor round the client's puppet, its facing first and then a fan, that a route from the puppet reaches:
// drops a notebook there. Null on success, else why not.
const char* SpawnBook(void* player, void** out) {
    coop::RemotePlayer& rp = coop::puppet_drive::Puppet(kClientSlot);
    void* const puppet = rp.valid() ? rp.GetActor() : nullptr;
    FVector at{};
    if (!puppet || !E::TryGetActorLocation(puppet, at)) return "slot 1's puppet place could not be read";
    const FVector fwd = E::GetActorForwardVector(puppet);
    const float len = std::sqrt(fwd.X * fwd.X + fwd.Y * fwd.Y);
    if (len < 0.1f) return "slot 1's puppet facing could not be read";
    void* const cls = ue_wrap::asset_load::LoadObjectByPath(kBookPath);
    if (!cls) return "the notebook's class did not load";
    for (const float deg : kFanDeg) {
        const float rad = deg * (3.14159265f / 180.f);
        const float dx = (fwd.X * std::cos(rad) - fwd.Y * std::sin(rad)) / len;
        const float dy = (fwd.X * std::sin(rad) + fwd.Y * std::cos(rad)) / len;
        for (const float ahead : kAheadCm) {
            const float x = at.X + dx * ahead, y = at.Y + dy * ahead;
            float floorZ = 0.f;
            if (!DP::FloorAt(player, x, y, at.Z, kFloorDownCm, kMinColumnCm, &floorZ)) continue;
            if (!D::RouteInLegs(player, at, FVector{x, y, floorZ}, kReachCm)) continue;
            void* book = nullptr;
            const DP::Drop d = DP::DropOnFloor(player, cls, nullptr, x, y, at.Z, kFloorDownCm, kMinColumnCm, kDropCm,
                                               &book, &floorZ);
            if (d == DP::Drop::NoFloor) continue;
            if (d == DP::Drop::SpawnFailed) return "the notebook did not spawn";
            UE_LOGI("[NOTE-DRILL] host dropped a notebook at (%.0f, %.0f, %.0f), %.0f uu away at %.0f deg from slot 1's "
                    "puppet's facing", x, y, floorZ + kDropCm, ahead, deg);
            *out = book;
            return nullptr;
        }
    }
    return "no floor a route reaches was found round slot 1's puppet";
}

void HostTick(coop::net::Session& s) {
    if (g_h == HStep::Done) return;
    void* const me = coop::players::Registry::Get().Local();
    if (!me) return;
    switch (g_h) {
    case HStep::Arm: {
        coop::RemotePlayer& rp = coop::puppet_drive::Puppet(kClientSlot);
        if (!rp.valid() || !rp.HasPose() || !Due(g_hNextMs, 1000)) return;
        if (!g_armedMs) g_armedMs = NowMs();
        void* book = nullptr;
        if (const char* why = SpawnBook(me, &book)) {
            if (g_spawnTries++ % 10 == 0) UE_LOGI("[NOTE-DRILL] host waits to drop the notebook: %s", why);
            if (NowMs() - g_armedMs > kSpawnBoundMs) HostAbandon(why);
            return;
        }
        g_book.Set(book);
        HEnter(HStep::Settle);
        return;
    }
    case HStep::Settle: {
        // The notebook takes its key and element id, then the seed page: the client finds the notebook by it, and a
        // joiner reads it from the snapshot's record.
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        void* book = g_book.Get();
        if (!book) { HostFail("settle", "the dropped notebook died before it was seeded"); return; }
        if (!DP::HasKey(book) || EL::Registry::Get().EidForActor(book) == EL::kInvalidId) {
            if (HExpired(kEidBoundMs)) HostFail("settle", "the dropped notebook took no key or element id");
            return;
        }
        g_claim = DS::ClassifyDeviceActorClaimKey(book);
        if (g_claim.empty() || !N::WritePages(book, std::vector<std::wstring>{std::wstring(kSeedPage)})) {
            HostFail("settle", "the notebook's claim key or its seed page did not take");
            return;
        }
        coop::prop_save_data::Publish(&s, book, ue_wrap::prop::GetKeyString(book));
        UE_LOGI("[NOTE-DRILL] host READY notebook claim='%ls' page='%ls'", g_claim.c_str(), kSeedPage);
        HEnter(HStep::AwaitClaim);
        return;
    }
    case HStep::AwaitClaim: {
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        void* book = g_book.Get();
        if (!book) { HostFail("claim", "the notebook died before the client took its claim"); return; }
        if (coop::device_occupancy::HolderOf(g_claim.c_str()) != kClientSlot) {
            if (HExpired(kClientLegBoundMs)) HostFail("claim", "the client never took the notebook's claim");
            return;
        }
        // The client holds the editor: the host's own press on its copy must open it and be sent out again.
        void* const root = E::RootComponentOf(book);
        FVector at{};
        if (!DS::EnsureResolved() || !root || !E::TryGetActorLocation(book, at) ||
            !PU::CallActionOptionIndex(book, me, root, at, kActOpen, root)) {
            HostAbandon("the host's press on the notebook did not dispatch");
            return;
        }
        if (N::NotebookOf(DS::ReadActiveInterface(me)) != book) {
            HostAbandon("the host's press did not open the editor on the notebook, so there is no claim to refuse");
            return;
        }
        UE_LOGI("[NOTE-DRILL] host pressed the open on its own copy; its editor stands open on the client's notebook");
        HEnter(HStep::ClaimWait);
        return;
    }
    case HStep::ClaimWait: {
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        if (DS::ReadActiveInterface(me)) {
            if (HExpired(kLegBoundMs)) HostFail("claim-refused", "the host's editor stayed open on a notebook the client holds");
            return;
        }
        if (coop::device_occupancy::HolderOf(g_claim.c_str()) != kClientSlot ||
            coop::device_occupancy::LocalHolds(g_claim.c_str())) {
            HostFail("claim-refused", "the host's press took the claim from the client");
            return;
        }
        UE_LOGI("[NOTE-DRILL] host claim-refused PASS -- the host's editor was sent out and slot %d still holds '%ls'",
                kClientSlot, g_claim.c_str());
        Say(kLineJudged);
        HEnter(HStep::AwaitPeriodic);
        return;
    }
    case HStep::AwaitPeriodic: {
        // The client has changed the page through the editor's commit and left the editor open: the lane's 2 s
        // publish must carry it, with the client still holding the claim.
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        void* book = g_book.Get();
        if (!book) { HostFail("periodic", "the notebook died before the client's page arrived"); return; }
        const std::wstring first = FirstPage(book);
        if (first != kPeriodicPage) {
            if (HExpired(kLegBoundMs)) {
                UE_LOGW("[NOTE-DRILL] FAIL host periodic: the host's copy still reads '%ls'", first.c_str());
                g_h = HStep::Done;
            }
            return;
        }
        if (coop::device_occupancy::HolderOf(g_claim.c_str()) != kClientSlot) {
            HostFail("periodic", "the page arrived after the client let go of the claim");
            return;
        }
        UE_LOGI("[NOTE-DRILL] host periodic PASS -- the host's copy reads '%ls' while slot %d holds the editor open",
                first.c_str(), kClientSlot);
        // The rule, asked directly: a slot that is not the holder is refused while the client holds.
        if (coop::notebook_sync::AcceptsClientRecord(book, kOtherSlot)) {
            HostFail("refusal", "the host's rule took a record from a slot that does not hold the claim");
            return;
        }
        UE_LOGI("[NOTE-DRILL] host refusal PASS -- slot %d's record is refused while slot %d holds the claim",
                kOtherSlot, kClientSlot);
        g_statsBefore = coop::notebook_sync::StatsNow();
        Say(kLinePeriodic);
        HEnter(HStep::AwaitText);
        return;
    }
    case HStep::AwaitText: {
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        void* book = g_book.Get();
        if (!book) { HostFail("text", "the notebook died before the client's page arrived"); return; }
        const std::wstring first = FirstPage(book);
        if (first == kEditedPage) {
            // The close's record was taken from the slot that still held the claim: the client keeps it until
            // the host has answered, so the release cannot have come first.
            const coop::notebook_sync::Stats st = coop::notebook_sync::StatsNow();
            if (st.acceptedHeld <= g_statsBefore.acceptedHeld || st.acceptedFree != g_statsBefore.acceptedFree ||
                st.refused != g_statsBefore.refused) {
                UE_LOGW("[NOTE-DRILL] FAIL host closing-order: held %u->%u free %u->%u refused %u->%u across the close",
                        g_statsBefore.acceptedHeld, st.acceptedHeld, g_statsBefore.acceptedFree, st.acceptedFree,
                        g_statsBefore.refused, st.refused);
                g_h = HStep::Done;
                return;
            }
            UE_LOGI("[NOTE-DRILL] host text PASS -- the host's copy reads '%ls', taken from the slot that held the claim",
                    first.c_str());
            Say(kLineHasText);
            HEnter(HStep::AwaitFree);
        } else if (HExpired(kLegBoundMs)) {
            UE_LOGW("[NOTE-DRILL] FAIL host text: the host's copy still reads '%ls'", first.c_str());
            g_h = HStep::Done;
        }
        return;
    }
    case HStep::AwaitFree: {
        // The client lets go once the host has answered; then the notebook is free, and any slot's record is taken.
        if (!Due(g_hNextMs, kCheckEveryMs)) return;
        void* book = g_book.Get();
        if (!book) { HostFail("free", "the notebook died before its claim was released"); return; }
        if (coop::device_occupancy::HolderOf(g_claim.c_str()) != 0xFF) {
            if (HExpired(kLegBoundMs)) HostFail("free", "the client never released the notebook's claim");
            return;
        }
        if (!coop::notebook_sync::AcceptsClientRecord(book, kOtherSlot)) {
            HostFail("free", "the host's rule refused a record for a notebook nobody holds");
            return;
        }
        UE_LOGI("[NOTE-DRILL] host free-accept PASS -- the claim is released and any slot's record is taken");
        UE_LOGI("[NOTE-DRILL] host DONE");
        g_h = HStep::Done;
        return;
    }
    default: return;
    }
}

// ---- CLIENT and OBSERVER ------------------------------------------------------------------------

enum class CStep : uint8_t { Arm, Find, Walk, Open, AwaitClaim, AwaitJudged, ArmLook, Type, AwaitPeriodic, Edit, AwaitText, Done };
CStep    g_c = CStep::Arm;
const char* g_role = "client";
uint64_t g_cStepMs = 0, g_cNextMs = 0;
ue_wrap::CachedObjRef g_cBook;
std::wstring g_cClaim;
std::shared_ptr<D::BackgroundWalk> g_walk;

void CEnter(CStep s) {
    g_c = s;
    g_cStepMs = NowMs();
    g_cNextMs = 0;
}
bool CExpired(uint64_t bound) { return NowMs() - g_cStepMs > bound; }

void ClientFail(const char* leg, const std::string& why) {
    UE_LOGW("[NOTE-DRILL] FAIL %s %s: %s", g_role, leg, why.c_str());
    UE_LOGW("[NOTE-DRILL] %s DONE FAIL", g_role);
    g_c = CStep::Done;
}
void ClientAbandon(const std::string& why) {
    UE_LOGW("[NOTE-DRILL] ABANDONED %s: %s", g_role, why.c_str());
    g_c = CStep::Done;
}

// A peer that finds the client's page already on the notebook only reads it.
void ClientRead(void* book) {
    UE_LOGI("[NOTE-DRILL] %s %s PASS -- this copy reads '%ls'", g_role,
            g_role[0] == 'o' ? "text" : "rejoin-read", FirstPage(book).c_str());
    UE_LOGI("[NOTE-DRILL] %s DONE", g_role);
    g_c = CStep::Done;
}

void ClientTick() {
    if (g_c == CStep::Done) return;
    void* const player = coop::players::Registry::Get().Local();
    if (!player) return;
    const uint8_t mySlot = coop::players::Registry::Get().LocalPeerId();
    switch (g_c) {
    case CStep::Arm:
        if (!coop::net_pump::HasAnnouncedWorldReady() ||
            coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle || mySlot == 0xFF)
            return;
        g_role = mySlot == kClientSlot ? "client" : "observer";
        CEnter(CStep::Find);
        return;
    case CStep::Find: {
        if (!Due(g_cNextMs, kCheckEveryMs)) return;
        const Found f = FindBook();
        if (f.written) { ClientRead(f.written); return; }
        if (f.seeded && mySlot == kClientSlot) {
            FVector me{}, at{};
            if (!E::TryGetActorLocation(f.seeded, at) || !E::TryGetActorLocation(player, me)) {
                ClientAbandon("the notebook's or the player's place did not read");
                return;
            }
            if (!D::RouteInLegs(player, me, at, kReachCm)) { ClientAbandon("no NavMesh route reaches the notebook"); return; }
            g_cBook.Set(f.seeded);
            UE_LOGI("[NOTE-DRILL] client sees the notebook at (%.0f, %.0f, %.0f); walking to it", at.X, at.Y, at.Z);
            g_walk = D::StartBackgroundWalk(at, kReachCm, kWalkDeadlineS);
            CEnter(CStep::Walk);
            return;
        }
        if (CExpired(mySlot == kClientSlot ? kSeenBoundMs : kClientLegBoundMs))
            ClientFail("find", "the host's notebook never stood in this world with the page this peer waits for");
        return;
    }
    case CStep::Walk: {
        const int st = g_walk ? g_walk->state.load() : 2;
        if (st == 0) return;
        if (st == 2) { ClientAbandon("the walk to the notebook did not arrive"); return; }
        CEnter(CStep::Open);
        return;
    }
    case CStep::Open: {
        // The press as a player's runs it: the notebook's actionOptionIndex with this peer's own player.
        void* book = g_cBook.Get();
        if (!book) { ClientFail("open", "the notebook died before the press"); return; }
        void* const root = E::RootComponentOf(book);
        FVector at{};
        if (!DS::EnsureResolved() || !root || !E::TryGetActorLocation(book, at) ||
            !PU::CallActionOptionIndex(book, player, root, at, kActOpen, root)) {
            ClientFail("open", "the notebook's actionOptionIndex did not dispatch");
            return;
        }
        if (N::NotebookOf(DS::ReadActiveInterface(player)) != book) {
            ClientFail("open", "the press did not open the editor on the notebook");
            return;
        }
        g_cClaim = DS::ClassifyDeviceActorClaimKey(book);
        UE_LOGI("[NOTE-DRILL] client pressed the open; its editor stands open, claim='%ls'", g_cClaim.c_str());
        CEnter(CStep::AwaitClaim);
        return;
    }
    case CStep::AwaitClaim:
        if (!Due(g_cNextMs, kCheckEveryMs)) return;
        if (!g_cClaim.empty() && coop::device_occupancy::LocalHolds(g_cClaim.c_str()) &&
            coop::device_occupancy::HolderOf(g_cClaim.c_str()) == mySlot) {
            UE_LOGI("[NOTE-DRILL] client holds the notebook's claim");
            CEnter(CStep::AwaitJudged);
        } else if (CExpired(kLegBoundMs)) {
            ClientFail("claim", "the host never granted the notebook's claim");
        }
        return;
    case CStep::AwaitJudged:
        if (!Due(g_cNextMs, kCheckEveryMs)) return;
        if (!coop::device_occupancy::LocalHolds(g_cClaim.c_str())) {
            ClientFail("hold", "the claim left this peer while the host judged it");
        } else if (HeardFromHost(kLineJudged)) {
            CEnter(CStep::ArmLook);
        } else if (CExpired(kLegBoundMs)) {
            ClientFail("judged", "the host never said it judged the claim");
        }
        return;
    case CStep::ArmLook: {
        // The lane measures an edit from the text it first saw on the open notebook, so the page is left alone
        // (the editor's own commit of the box's text, unchanged) until the lane says it has looked.
        if (!Due(g_cNextMs, kCheckEveryMs)) return;
        void* const widget = DS::ReadActiveInterface(player);
        std::wstring boxNow;
        if (N::NotebookOf(widget) != g_cBook.Get() || !N::ReadEditorText(widget, boxNow)) {
            ClientFail("look", "the editor is no longer open on the notebook");
        } else if (coop::notebook_sync::BaselineTaken(g_cBook.Get())) {
            CEnter(CStep::Type);
        } else if (CExpired(kLegBoundMs)) {
            ClientFail("look", "the notebook sync never looked at the open notebook");
        } else if (!N::CommitKeystroke(widget, boxNow)) {
            ClientFail("look", "the editor's commit handler did not dispatch");
        }
        return;
    }
    case CStep::Type: {
        // A page typed and left open: the box takes the text and the editor's commit handler writes it into the
        // notebook, as the box does when a typed page is committed. The commit repeats until the host has read
        // the page, since the lane publishes on the first graph event after its 2 s gate.
        void* const widget = DS::ReadActiveInterface(player);
        if (N::NotebookOf(widget) != g_cBook.Get() || !N::SetEditorText(widget, kPeriodicPage)) {
            ClientFail("type", "the editor's box did not take the page");
            return;
        }
        UE_LOGI("[NOTE-DRILL] client typed a page and left the editor open");
        CEnter(CStep::AwaitPeriodic);
        return;
    }
    case CStep::AwaitPeriodic: {
        if (!Due(g_cNextMs, kCheckEveryMs)) return;
        if (HeardFromHost(kLinePeriodic)) {
            UE_LOGI("[NOTE-DRILL] client periodic PASS -- the host read the page before the editor closed");
            CEnter(CStep::Edit);
            return;
        }
        if (CExpired(kLegBoundMs)) { ClientFail("periodic", "the host never said it read the typed page"); return; }
        void* const widget = DS::ReadActiveInterface(player);
        if (N::NotebookOf(widget) != g_cBook.Get() || !N::CommitKeystroke(widget, kPeriodicPage))
            ClientFail("periodic", "the editor's commit handler did not dispatch");
        return;
    }
    case CStep::Edit: {
        // What a player does, through the editor's own verbs: the box takes the page, quit closes the editor (the
        // red arm closes it with the discard), and the player leaves the interface as the tear and crumple flows do.
        void* const widget = DS::ReadActiveInterface(player);
        if (N::NotebookOf(widget) != g_cBook.Get()) { ClientFail("edit", "the editor is no longer open on the notebook"); return; }
        const bool discard = ModeOf() == Mode::Red;
        if (!N::SetEditorText(widget, kEditedPage) || !N::CallQuit(widget, discard)) {
            ClientFail("edit", "the editor's box or its quit did not dispatch");
            return;
        }
        DS::ForceExitInterface(player);
        UE_LOGI("[NOTE-DRILL] client wrote its page and closed the editor (quit skipEdit=%d)", discard ? 1 : 0);
        CEnter(CStep::AwaitText);
        return;
    }
    case CStep::AwaitText:
        if (!Due(g_cNextMs, kCheckEveryMs)) return;
        if (HeardFromHost(kLineHasText)) {
            UE_LOGI("[NOTE-DRILL] client text PASS -- the host read the page");
            if (ModeOf() == Mode::Rejoin) UE_LOGI("[NOTE-DRILL] client LIFE DONE");
            else UE_LOGI("[NOTE-DRILL] client DONE");
            g_c = CStep::Done;
        } else if (CExpired(kLegBoundMs)) {
            ClientFail("text", "the host never said it read the client's page");
        }
        return;
    default: return;
    }
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (ModeOf() == Mode::Off || !s) return;
    if (s->role() == coop::net::Role::Host) {
        if (s->running()) HostTick(*s);
    } else if (s->connected()) {
        ClientTick();
    }
}

void OnDisconnect() {
    if (ModeOf() == Mode::Off) return;
    if (g_walk && g_walk->state.load() == 0) D::CancelBackgroundWalk(*g_walk);
    g_walk.reset();
    g_c = CStep::Arm;
    g_h = HStep::Arm;
    g_role = "client";
    g_cStepMs = g_cNextMs = g_hStepMs = g_hNextMs = g_armedMs = 0;
    g_spawnTries = 0;
    g_cBook.Reset();
    g_book.Reset();
    g_claim.clear();
    g_statsBefore = coop::notebook_sync::Stats{};
    g_cClaim.clear();
}

}  // namespace coop::dev::notebook_drill
