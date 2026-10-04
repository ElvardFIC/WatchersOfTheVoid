// coop/player/model_folder.h -- the player-model folder (the MultiModel addon, folded in).
//
// The skin catalog already lists every skin pak under any LogicMods subfolder, and the engine mounts
// them all; this module gives players one folder of their own to put models in, `multimodel`,
// created at boot with a note beside it, and copies a pak that is new to the import folder
// (%LOCALAPPDATA%\MultiModel\Models, which a mod manager's update never wipes) into it. The engine
// mounts paks only at its start, so a model copied in is wearable from the next launch, and a pak
// already in the folder is never overwritten: the running game holds it open.
//
// Only a skin's NAME crosses the wire, so two players with different files of one name each see
// their own: the panel shows each pak's size and a fingerprint two players can compare, and the
// title written in <name>.txt beside it.
//
// Under r2modman's shimloader the game's LogicMods is the profile's shimloader\pak, a mapping only
// the game process sees; the folder this module creates and opens is the real one on disk.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace coop::model_folder {

// Create the folder and its note, and import the new paks. Filesystem only; the boot thread, once.
void PrepareAtBoot();

struct Report {
    std::wstring folder;                // the models folder on disk
    std::wstring inbox;                 // the import folder
    std::vector<std::string> imported;  // models copied in at this boot, wearable after a restart
    std::vector<std::string> notes;     // what a player should fix: a raw model file, a bad name, a stale copy
};
// Any thread: what the boot's preparation found.
Report LastReport();

// Any thread: open `dir` in Explorer, creating it first. False when it cannot be created.
bool OpenInExplorer(const std::wstring& dir);

// A pak's details for the skins panel. `fingerprint` is 8 hex digits, FNV-1a over the pak's first
// and last 256 KB (the whole file when smaller) and its size, the MultiModel addon's label -- not a
// cryptographic hash, a way for two players to see they hold the same bytes.
struct PakInfo {
    uint64_t    size = 0;
    std::string fingerprint;
};
// Render thread (the panel), cached per path: computed on the first ask, which reads up to 512 KB.
const PakInfo& InfoFor(const std::wstring& pakPath);

// The first line of <dir>\<skin>.txt, UTF-8, trimmed; empty when absent or not a one-line title.
std::string TitleFor(const std::wstring& dir, const std::string& skin);

}  // namespace coop::model_folder
