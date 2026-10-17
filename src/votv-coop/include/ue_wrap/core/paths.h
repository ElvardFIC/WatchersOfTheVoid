// ue_wrap/core/paths.h -- the ONE owner of the install's directory anchor.
//
// Every per-install runtime artifact (multivoid.log, multivoid.ini + .example,
// multivoid-loaded.txt, multivoid-players.txt, multivoid-compat-report.txt, coop-screenshots/,
// coop_players/, multivoid_servers/, multivoid_reports/) anchors on the GAME EXE's directory
// (...\VotV\Binaries\Win64), NOT on the mod DLL's own directory.
// The module's location is loader-dependent: UE4SS maps us at Mods\Multivoid\dlls\, and
// unreal_shimloader additionally virtualizes Mods\ into the r2modman profile, where
// module-dir writes were measured landing. The exe dir is the one real, loader-independent
// home of the install, so every artifact above resolves its directory through this one
// helper rather than computing it per file. The identity key is the exception: it lives in the
// mod's own data folder (ModDataDir), so a copied game folder carries no identity.
//
// UE4SS's own log is the one artifact beside the loaded UE4SS module instead: Ue4ssModulePath and
// Ue4ssDir find that module.

#pragma once

#include <string>

namespace ue_wrap::paths {

// Directory containing the game executable (no trailing slash). Empty only on
// a GetModuleFileNameW failure (callers treat empty as "skip the write").
std::wstring ExeDir();

// The mod's own data folder, the one home every file the mod keeps for itself is read from and written
// to: the PACKAGE folder that holds `dlls\main.dll` and `enabled.txt` (`Mods\Multivoid`), or, when a
// manager's shimloader launched the process (it exports SHIMLOADER_MOD_DIR, the profile's mod folder),
// `<SHIMLOADER_MOD_DIR>\<pkg>` -- the real profile path, since the module's own path is the logical one
// and a rename there is remapped. `<pkg>` is the folder above `dlls\` in the module's path. No trailing
// slash. Empty on any failure (no module path, the module not under a `dlls` folder, the profile's
// package folder missing), with one log line saying which; every caller treats empty as "no data
// folder". Safe on the boot thread from the first ini read: it uses the process environment and the
// module's path only, reads no config row, and resolves once (latched). Any thread.
std::wstring ModDataDir();

// The loaded UE4SS module's full path (the official "UE4SS.dll" or shimloader's lowercase
// "ue4ss.dll": the module lookup is case-insensitive) and its folder (no trailing slash). Both
// empty when no such module is loaded or its path cannot be read. Any thread.
std::wstring Ue4ssModulePath();
std::wstring Ue4ssDir();

}  // namespace ue_wrap::paths
