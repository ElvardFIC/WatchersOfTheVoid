// coop/dev/desk_verb_drill_internal.h -- what the desk-verb drill's files share: its legs, its census of the entries
// into the save family's functions, what a peer's desk holds and how the host sets a leg up
// (coop/dev/desk_verb_drill_desk.cpp), and the refiner's legs (coop/dev/desk_verb_drill_refiner.cpp). Dev only; game
// thread.

#pragma once

#include "ue_wrap/desk/signal_dynamic.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::dev::desk_verb_drill::detail {

// The drill's caught signals sit at coordinates no sky signal reaches, and a saved row keeps its signal's x and y
// (formDownload's initDownloadSignal), so the band names the drill's rows. Each leg arms its own x.
inline constexpr float kBandX = 913000.f;
inline constexpr float kBandY = 917000.f;
inline constexpr float kBandWidth = 16.f;

// The legs: SAVE, DELETE on a second caught signal, the deck's send on the saved row, the deck's drive button both
// ways (the row exported onto an empty drive, then imported back), the refiner's upload of a drive's row; then the
// refiner's start, its stop, a start the host completes, and a start at level 2, whose completion is the last level.
enum Leg : int { kSave = 0, kDelete, kSend, kExport, kImport, kUpload, kStart, kStop, kComplete, kProcessed, kLegs };
inline constexpr const wchar_t* kLegButton[kLegs] = {
    L"button_downl_saveSig1", L"button_downl_delSig", L"button_play_saveSig", L"button_play_left",
    L"button_play_left",      L"button_comp_upload",  L"button_comp_start",   L"button_comp_stop",
    L"button_comp_start",     L"button_comp_start"};
inline constexpr const char* kLegName[kLegs] = {"SAVE",   "DELETE", "send", "export",   "import",
                                                "upload", "start",  "stop", "complete", "processed"};

// The functions the census counts.
enum Fn : int { kSaveSignal, kGetSigObj, kSetSignalID, kAddGloss, kDeleteActive, kLaptopAdd, kDeleteSignal, kCompUpload,
                kCompStart, kCompStop, kFns };

// ---- the census: entries into the family's functions, the game's own apart --------------------------------------

struct Census {
    uint32_t game[kFns] = {};   // called from a Blueprint frame
    uint32_t other[kFns] = {};  // through ProcessEvent: our code, a lane's replay
    uint32_t ran[kFns] = {};    // bodies that ran: a refused call enters and does not run
};
const Census& CensusNow();
const wchar_t* CensusNotLive();  // every census watch live: null; else the first function that is not
const wchar_t* CensusDead();     // the first function whose watch the gate refused or settled dead, else null
Census Since(const Census& before);
std::string CensusLine(const Census& d);

// ---- what this peer's desk holds ---------------------------------------------------------------------------------

std::string PowerLine();  // the desk's four units' power: DELETE reads active_download, the drive button active_play
float CaughtX();          // the caught signal's x, or -1 with none caught
float Needle();
std::vector<int32_t> BandRows();
uint64_t BandHash();      // the band row's identity, 0 with none
uint64_t LaptopNewest();  // the laptop's newest row's identity, 0 with none: what a send just appended
void* SlotDrive(int role);
uint64_t DriveRow(void* drive);  // a drive's row identity, 0 for no drive or an empty one
uint64_t CompRow();              // the refiner's, the same way
std::wstring CompSignal();       // the refiner's row's signal (its template key), empty when it does not read
bool CompLatched();              // this machine's own refiner decodes: its comp_isDecodeActive
int32_t CompLevel();             // the refiner's row's level, -1 when it does not read
int32_t Processed();             // this profile's signals_processed, -1 when it does not read
void* Button(int leg);
bool CapOpened(void* desk);      // the desk's own cap over DELETE is open

// Whether this machine's profile glossary (gamemode.save_main.signalsGlossary, CXXHeaderDump/lib.cpp:3472-3477) holds `name`:
// 1, 0, or -1 when it does not read.
int GlossaryHas(const std::wstring& name);

// A saved row as the drill reports it: its identity, its photo's bytes and digest (the identity leaves the photo
// out), its template key (the SAVE's gloss).
struct RowSeen {
    uint64_t hash = 0;
    size_t photo = 0;
    uint64_t photoDigest = 0;
    std::wstring signal;
};
bool ReadRowSeen(int32_t index, RowSeen& out);

// ---- the host's fixtures -----------------------------------------------------------------------------------------

bool FindSource();                   // the sky signal the caught signals are formed from, kept; false with none
const std::wstring& SourceObject();  // its object
int PurgeBand();                     // the rows an earlier run left in the band go, by the game's own delete
bool WriteCaught(int leg);           // a caught signal at the leg's x
bool FormDownload();                 // its download, formed whole
bool Rendered();                     // the download's render exists: SAVE's photo is that render
bool Detect();                       // the needle at 100%
int ArmDrive(int role, bool withRow);  // 1 once the leg's drive sits in the slot, 0 while working, -1 failed
bool SetCompProgress(float progress);  // the refiner's progress: at 100 its next decode step completes the level
bool SetCompLevel(int32_t level);     // the refiner's row at `level`
bool HoldProcessLevel(int32_t atLeast);  // the process upgrade raised to `atLeast` while the drill runs
void RestoreProcessLevel();              // and put back
bool ArmCompRow();                   // join: the refiner loaded with a row, decoding slowly
void ResetFixtures();

// ---- the refiner's legs (coop/dev/desk_verb_drill_refiner.cpp) -----------------------------------------------------

inline constexpr bool IsRefinerLeg(int leg) { return leg >= kStart; }

// HOST. RefinerArm sets the leg up: the process upgrade held at 3, so every start passes its gate; at `processed`, a
// decode the continue restarted stopped by the host's own press and the row set at level 2. RefinerStep sets the
// completing legs' progress at 100 once the client's start latched this refiner. Each: null, or why it cannot.
const char* RefinerArm(int leg, void* player);
const char* RefinerStep(int leg);
bool RefinerHostDone(int leg);
bool RefinerHostJudge(int leg, const Census& d, uint64_t glossesSent, std::string& tail);

// CLIENT.
bool RefinerArmed(int leg);
void RefinerBeforePress();
bool RefinerClientDone(int leg);
bool RefinerClientJudge(int leg, const Census& d, uint64_t glossesMade, std::string& tail);

// join. The host's own decode before any client joins: 1 decoding, 0 not yet readable, -1 failed with `why`. The
// client's census is marked as it goes live ahead of the load, so the join judges its own restore; seeded once the
// host's state is mirrored here.
bool JoinSnapshotTaken();
int HostJoinDecode(const char*& why);
void MarkJoinCensus();
bool RefinerJoinSeeded();
bool RefinerJoinCheck(std::string& tail);

void RefinerReset();

}  // namespace coop::dev::desk_verb_drill::detail
