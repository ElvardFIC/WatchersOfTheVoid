// coop/dev/desk_verb_drill_desk.cpp -- the desk-verb drill's census, its readings of a peer's desk and the host's
// fixtures; see coop/dev/desk_verb_drill_internal.h.

#include "coop/dev/desk_verb_drill_internal.h"

#include "coop/element/element.h"
#include "coop/element/registry.h"
#include "coop/interactables/signal_wire.h"
#include "coop/net/blob_chunks.h"  // Fnv64: a photo's digest

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/desk/comp_pane.h"
#include "ue_wrap/desk/console_desk.h"
#include "ue_wrap/desk/desk_press.h"
#include "ue_wrap/desk/drive_chain.h"
#include "ue_wrap/desk/meadow_store.h"
#include "ue_wrap/desk/saved_signals.h"
#include "ue_wrap/desk/space_renderer.h"
#include "ue_wrap/engine/engine.h"  // SpawnActor, TryGetActorLocation
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/world/profile.h"
#include "ue_wrap/world/upgrades.h"
#include "ue_wrap/world/world_singleton.h"

#include <cstdio>
#include <cstring>

namespace coop::dev::desk_verb_drill::detail {
namespace {

namespace CD = ue_wrap::console_desk;
namespace DC = ue_wrap::drive_chain;
namespace DP = ue_wrap::desk_press;
namespace E  = ue_wrap::engine;
namespace R  = ue_wrap::reflection;
namespace SD = ue_wrap::signal_dynamic;
namespace SS = ue_wrap::saved_signals;
namespace sg = ue_wrap::script_gate;

// The census's functions, by the class declaring each.
UE_FACT_FN(kRowSaveSignal, "dev.desk_verb_drill_desk", L"mainGamemode_C", L"saveSignal", Declared);
UE_FACT_FN(kRowGetSigObj, "dev.desk_verb_drill_desk", L"saveSlot_C", L"getSigObj", Declared);
UE_FACT_FN(kRowSetSignalId, "dev.desk_verb_drill_desk", L"lib_C", L"setSignalID", Declared);
UE_FACT_FN(kRowAddGloss, "dev.desk_verb_drill_desk", L"lib_C", L"addGloss", Declared);
UE_FACT_FN(kRowDeleteActiveSignal, "dev.desk_verb_drill_desk", L"mainGamemode_C", L"deleteActiveSignal", Declared);
UE_FACT_FN(kRowAddSignal, "dev.desk_verb_drill_desk", L"ui_laptop_C", L"addSignal", Declared);
UE_FACT_FN(kRowDeleteSignal, "dev.desk_verb_drill_desk", L"mainGamemode_C", L"deleteSignal", Declared);
UE_FACT_FN(kRowCompUploadData, "dev.desk_verb_drill_desk", L"analogDScreenTest_C", L"comp_uploadData", Declared);
UE_FACT_FN(kRowCompStart, "dev.desk_verb_drill_desk", L"analogDScreenTest_C", L"comp_start", Declared);
UE_FACT_FN(kRowCompStop, "dev.desk_verb_drill_desk", L"analogDScreenTest_C", L"comp_stop", Declared);
const ue_wrap::facts::Function* const kFnRow[kFns] = {
    &kRowSaveSignal,       &kRowGetSigObj,    &kRowSetSignalId,    &kRowAddGloss,  &kRowDeleteActiveSignal,
    &kRowAddSignal,        &kRowDeleteSignal, &kRowCompUploadData, &kRowCompStart, &kRowCompStop};
constexpr int kTagBase = 0x44564430;  // 'DVD0'

Census g_census;
bool g_watched[kFns] = {};
bool g_refused[kFns] = {};  // the gate refused the watch: a full table or no gate, which no retry mends

sg::Verdict OnCensusPre(const sg::Call& c) {
    const int i = c.tag - kTagBase;
    if (i >= 0 && i < kFns) ++(c.callerFunction ? g_census.game[i] : g_census.other[i]);
    return sg::Verdict::Run;
}

void OnCensusPost(const sg::Call& c) {
    const int i = c.tag - kTagBase;
    if (i >= 0 && i < kFns) ++g_census.ran[i];
}

bool InBand(const SD::Row& r) { return r.locY == kBandY && r.locX >= kBandX && r.locX < kBandX + kBandWidth; }

uint64_t HashOf(const SD::Row& r) { return coop::signal_wire::ContentHash(coop::signal_wire::Serialize(r)); }

bool DeskBool(R::InstanceOffset& member) {
    void* desk = CD::Instance();
    const int32_t off = desk ? member.Of(desk) : -1;
    return off >= 0 && *(static_cast<const uint8_t*>(desk) + off) != 0;
}

// The refiner is decoding: its upload is then denied (comp_uploadData's first branch).
bool CompDecoding() {
    static R::InstanceOffset sOff{L"comp_isDecodeActive"};
    return DeskBool(sOff);
}

// The process upgrade (upg_processLvl, the laptop's row 7) as the drill found it, while it holds it raised.
constexpr int kProcessLvlRow = 7;
bool    g_processHeld = false;
int32_t g_processWas = 0;

// The host's fixture state: the sky signal its caught signals are formed from (its object names the objects-table
// row the download is formed from, as a ping's catch names it), and a drive leg's spawned drive until it is in.
ue_wrap::space_renderer::SignalRow g_source;
void* g_fixDrive = nullptr;
bool  g_fixIn = false;

}  // namespace

// ---- the census ----------------------------------------------------------------------------------------------------

const Census& CensusNow() { return g_census; }

const wchar_t* CensusNotLive() {
    for (int i = 0; i < kFns; ++i)
        if (!g_watched[i] && !g_refused[i]) {
            g_watched[i] = sg::WatchName(*kFnRow[i], kTagBase + i, &OnCensusPre, &OnCensusPost);
            g_refused[i] = !g_watched[i];
        }
    sg::ResolvePendingNames();
    for (int i = 0; i < kFns; ++i)
        if (!g_watched[i] || !sg::NameWatchLive(*kFnRow[i], kTagBase + i)) return kFnRow[i]->Name();
    return nullptr;
}

const wchar_t* CensusDead() {
    for (int i = 0; i < kFns; ++i)
        if (g_refused[i] || (g_watched[i] && sg::NameWatchSettled(*kFnRow[i], kTagBase + i) &&
                             !sg::NameWatchLive(*kFnRow[i], kTagBase + i)))
            return kFnRow[i]->Name();
    return nullptr;
}

Census Since(const Census& before) {
    Census d;
    for (int i = 0; i < kFns; ++i) {
        d.game[i] = g_census.game[i] - before.game[i];
        d.other[i] = g_census.other[i] - before.other[i];
        d.ran[i] = g_census.ran[i] - before.ran[i];
    }
    return d;
}

std::string CensusLine(const Census& d) {
    std::string out;
    char buf[96];
    for (int i = 0; i < kFns; ++i) {
        std::snprintf(buf, sizeof(buf), "%s%ls game=%u other=%u ran=%u", i ? ", " : "", kFnRow[i]->Name(), d.game[i],
                      d.other[i], d.ran[i]);
        out += buf;
    }
    return out;
}

// ---- what this peer's desk holds -----------------------------------------------------------------------------------

std::string PowerLine() {
    CD::Scalars sc;
    if (!CD::ReadScalars(sc)) return "desk(play,downl,coords,comp)=(unread)";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "desk(play,downl,coords,comp)=%d%d%d%d", sc.activePlay ? 1 : 0,
                  sc.activeDownload ? 1 : 0, sc.activeCoords ? 1 : 0, sc.activeComp ? 1 : 0);
    return buf;
}

float CaughtX() {
    CD::CoordSignal sig;
    if (!CD::ReadCoordSignal(sig) || sig.objectName.empty() || sig.objectName == L"None") return -1.f;
    return sig.x;
}

float Needle() {
    CD::Scalars sc;
    return CD::ReadScalars(sc) ? sc.dlResDetecPercent : -1.f;
}

std::vector<int32_t> BandRows() {
    std::vector<int32_t> out;
    const int32_t n = SS::Count();
    for (int32_t i = 0; i < n; ++i) {
        SD::Row r;
        if (SS::ReadRow(i, r) && InBand(r)) out.push_back(i);
    }
    return out;
}

uint64_t BandHash() {
    const std::vector<int32_t> rows = BandRows();
    SD::Row r;
    return !rows.empty() && SS::ReadRow(rows.back(), r) ? HashOf(r) : 0;
}

uint64_t LaptopNewest() {
    const int32_t n = ue_wrap::meadow_store::Count();
    SD::Row r;
    return n > 0 && ue_wrap::meadow_store::ReadRow(n - 1, r) ? HashOf(r) : 0;
}

void* SlotDrive(int role) {
    void* slot = DC::SlotActor(role);
    return slot ? DC::SlotDrive(slot) : nullptr;
}

uint64_t DriveRow(void* drive) {
    SD::Row r;
    return drive && DC::ReadDriveRow(drive, r) && r.size > 0 ? HashOf(r) : 0;
}

uint64_t CompRow() {
    SD::Row r;
    void* base = ue_wrap::comp_pane::CompDataPtr();
    return base && SD::ReadStruct(base, r) && r.size > 0 ? HashOf(r) : 0;
}

bool CompLatched() { return CompDecoding(); }

std::wstring CompSignal() {
    SD::Row r;
    void* base = ue_wrap::comp_pane::CompDataPtr();
    return base && SD::ReadStruct(base, r) ? r.signal : std::wstring{};
}

int32_t CompLevel() {
    const uint8_t* base = static_cast<const uint8_t*>(ue_wrap::comp_pane::CompDataPtr());
    if (!base) return -1;
    int32_t level = 0;
    std::memcpy(&level, base + SD::kOff_level, sizeof(level));
    return level;
}

int32_t Processed() {
    int32_t v = 0;
    return ue_wrap::profile::ReadSignalsProcessed(v) ? v : -1;
}

void* Button(int leg) {
    void* desk = CD::Instance();
    return desk ? DP::Member(desk, kLegButton[leg]) : nullptr;
}

// The download unit's DELETE sits under the desk's own cap, its `cap` mesh. A press on the cap flips `capOpened`
// and turns DELETE's collision on (analogDScreenTest.cpp:3339-3354), so a player presses the cap first.
bool CapOpened(void*) {
    static R::InstanceOffset sOff{L"capOpened"};
    return DeskBool(sOff);
}

int GlossaryHas(const std::wstring& name) {
    void* gm = ue_wrap::world_singleton::Gamemode();
    const int32_t offSave = gm ? R::FindPropertyOffset(R::ClassOf(gm), L"save_main") : -1;
    void* save = offSave >= 0 ? *reinterpret_cast<void**>(static_cast<uint8_t*>(gm) + offSave) : nullptr;
    if (!save || !R::IsLive(save)) return -1;
    const int32_t offGloss = R::FindPropertyOffset(R::ClassOf(save), L"signalsGlossary");
    if (offGloss < 0) return -1;
    struct { R::FName* data; int32_t num; int32_t max; } names{};
    std::memcpy(&names, static_cast<uint8_t*>(save) + offGloss, sizeof(names));
    if (names.num < 0 || (names.num > 0 && !names.data)) return -1;
    for (int32_t i = 0; i < names.num; ++i)
        if (R::NameEquals(names.data[i], name.c_str())) return 1;
    return 0;
}

bool ReadRowSeen(int32_t index, RowSeen& out) {
    SD::Row r;
    if (!SS::ReadRow(index, r, /*withImage=*/true)) return false;
    out.hash = HashOf(r);
    out.photo = r.image.size();
    out.photoDigest = r.image.empty() ? 0 : coop::blob_chunks::Fnv64(r.image);
    out.signal = r.signal;
    return true;
}

// ---- the host's fixtures -------------------------------------------------------------------------------------------

bool FindSource() {
    std::vector<ue_wrap::space_renderer::SignalRow> sky;
    if (!ue_wrap::space_renderer::ReadSignals(sky)) return false;
    for (const auto& s : sky) {
        if (s.objectName.empty() || s.objectName == L"None") continue;
        g_source = s;
        return true;
    }
    return false;
}

const std::wstring& SourceObject() { return g_source.objectName; }

int PurgeBand() {
    int purged = 0;
    for (std::vector<int32_t> rows = BandRows(); !rows.empty(); rows = BandRows()) {
        if (!SS::DeleteSignal(rows.back())) break;
        ++purged;
    }
    return purged;
}

// The download is formed once the catch is on the wire: a receiver forms its download from the caught signal's
// object, so an arm that overtook the catch would form nothing there ("download_arm: ARM with no caught
// signal here").
bool WriteCaught(int leg) {
    CD::CoordSignal sig;
    sig.x = kBandX + static_cast<float>(leg);
    sig.y = kBandY;
    sig.z = 0.f;
    sig.type = g_source.type;
    sig.strength = 1.f;
    sig.frequency = 1.f + static_cast<float>(leg);
    sig.frequencySpread = 0.5f;
    sig.polarity = 0.f;
    sig.polaritySpread = 0.5f;
    sig.objectName = g_source.objectName;
    return CD::WriteCoordSignal(sig);
}

// The polarity is the native arm's on a dish stop, -1.
bool FormDownload() { return CD::ArmDownloadFromSignal(1.0e6f, -1); }

// SAVE's photo is DL_current, which the download's play cycle draws (download_playSignall,
// analogDScreenTest.cpp:9465), so the needle waits for it and a press cannot come first.
bool Rendered() {
    void* desk = CD::Instance();
    return desk && DP::Member(desk, L"DL_current") != nullptr;
}

bool Detect() {
    CD::Scalars sc;
    if (!CD::ReadScalars(sc)) return false;
    sc.dlResDetecPercent = 1.f;
    return CD::WriteScalars(sc, 0);
}

// One step a tick. putDriveIn refuses a slot that holds a drive (driveSlot.cpp, its Label_1358), so a slot's own
// drive is used and its row written; an empty slot gets a spawned one, put in once the prop lanes gave it an eid.
int ArmDrive(int role, bool withRow) {
    void* slot = DC::SlotActor(role);
    if (!slot) return -1;
    SD::Row row;
    if (withRow) {
        const std::vector<int32_t> rows = BandRows();
        if (rows.empty() || !SS::ReadRow(rows.back(), row)) return -1;
    }
    if (void* held = DC::SlotDrive(slot); held && held != g_fixDrive) {
        if (!DC::WriteDriveRow(held, row)) return -1;
        DC::CallDriveUpd(held);
        UE_LOGI("[DESK-VERB-DRILL] host: the slot's own drive %p now holds %s (the refiner %s)", held,
                withRow ? "the band row" : "no row", CompDecoding() ? "decoding" : "idle");
        return 1;
    }
    if (!g_fixDrive) {
        ue_wrap::FVector at{};
        if (!E::TryGetActorLocation(slot, at)) return -1;
        at.Z += 40.f;
        g_fixDrive = E::SpawnActor(DC::DriveClass(), at);
        g_fixIn = false;
        return g_fixDrive ? 0 : -1;
    }
    if (DC::SlotDrive(slot) == g_fixDrive) {
        g_fixDrive = nullptr;  // in: the next drive leg starts from its own slot
        return 1;
    }
    if (g_fixIn || coop::element::Registry::Get().EidForActor(g_fixDrive) == coop::element::kInvalidId) return 0;
    if (withRow && !DC::WriteDriveRow(g_fixDrive, row)) return -1;
    g_fixIn = DC::CallPutDriveIn(slot, g_fixDrive);
    return g_fixIn ? 0 : -1;
}

// The decode's step adds its increment to comp_progress and completes at 100 or more (analogDScreenTest.cpp
// :6219-6226), so a progress of 100 completes at the next step whatever the increment.
bool SetCompProgress(float progress) {
    ue_wrap::comp_pane::CompScalars cs;
    return ue_wrap::comp_pane::ReadCompScalars(cs) && ue_wrap::comp_pane::WriteCompScalars(progress, cs.downloading);
}

bool SetCompLevel(int32_t level) {
    uint8_t* base = static_cast<uint8_t*>(ue_wrap::comp_pane::CompDataPtr());
    if (!base) return false;
    std::memcpy(base + SD::kOff_level, &level, sizeof(level));
    return true;
}

// comp_start refuses a row at the process upgrade's level or above (analogDScreenTest.cpp :9777).
bool HoldProcessLevel(int32_t atLeast) {
    int32_t now = 0;
    if (!ue_wrap::upgrades::ReadLevel(kProcessLvlRow, &now)) return false;
    if (now >= atLeast) return true;
    if (!g_processHeld) {
        g_processHeld = true;
        g_processWas = now;
    }
    UE_LOGI("[DESK-VERB-DRILL] host holds the process upgrade at %d (it was %d)", atLeast, g_processWas);
    return ue_wrap::upgrades::WriteLevel(kProcessLvlRow, atLeast);
}

void RestoreProcessLevel() {
    if (!g_processHeld) return;
    g_processHeld = false;
    if (ue_wrap::upgrades::WriteLevel(kProcessLvlRow, g_processWas))
        UE_LOGI("[DESK-VERB-DRILL] host put the process upgrade back at %d", g_processWas);
}

// A row of the host's own list, else one made here (a New Game's list is empty), its size raised so the decode
// barely moves across a join, at level 0.
bool ArmCompRow() {
    void* base = ue_wrap::comp_pane::CompDataPtr();
    if (!base) return false;
    SD::Row row;
    if (SS::Count() <= 0 || !SS::ReadRow(0, row) || row.size <= 0.f) {
        row = SD::Row{};  // its object and signal empty: None, which a name write takes only as empty
        row.name = L"refiner drill";
        row.date = 1;     // non-epoch, as a row on the wire must be
    }
    row.size = 1.0e6f;
    row.decoded = row.size;
    row.level = 0;
    row.hasData = true;
    if (!SD::WriteStructLive(base, row)) return false;
    ue_wrap::comp_pane::UpdComp(true);
    UE_LOGI("[DESK-VERB-DRILL] host loaded the refiner with its row '%ls' (signal '%ls')", row.name.c_str(),
            row.signal.c_str());
    return true;
}

void ResetFixtures() {
    RestoreProcessLevel();
    g_fixDrive = nullptr;
    g_fixIn = false;
    g_source = ue_wrap::space_renderer::SignalRow{};
}

}  // namespace coop::dev::desk_verb_drill::detail
