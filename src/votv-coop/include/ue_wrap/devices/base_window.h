// ue_wrap/devices/base_window.h -- engine access for the base's observation window (AbaseWindow_C),
// the "main huge window". Principle-7 engine-wrapper layer: the reflection, struct-offset and
// UFunction-thunk details of a baseWindow actor, with NO network logic and NO coop state --
// coop::window_sync owns those and reaches the engine through here.
//
// A baseWindow is an Aactor_save_C descendant. Its dirt is one WHOLE-SURFACE scalar, `clean`:
// higher is dirtier, cleanSponge wipes it DOWN toward 0 and nothing re-raises it, and setClean()
// pushes it into the mesh shader through SetCustomPrimitiveDataFloat(0). Its cross-peer-stable
// identity is the inherited Aactor_save_C::Key FName, assigned deterministically by
// intComs_gamemodeMakeKeys and save-persistent, the same Key family the doors use. The class does
// not declare Key itself; EnsureResolved asks baseWindow_C, and the property lookup climbs to the
// actor_save_C parent that declares it.

#pragma once

#include <string>
#include <functional>

namespace ue_wrap::base_window {

// Indexed class and reflected clean/Key fields. Missing fields retry; a class
// serial change invalidates the layout. Repainting also requires StaticMesh1
// and the actor class's current setClean function. Game thread.
bool EnsureResolved();

// True iff `obj`'s class is baseWindow_C or a subclass. Cheap (a bounded SuperStruct
// walk; no allocation). False if not yet resolved.
bool IsBaseWindow(void* obj);

// Read the window's Aactor_save_C::Key FName as a wide string. Empty on failure
// (null / not resolved). An unkeyed window returns L"None".
std::wstring GetKeyString(void* win);

// Read the window's `clean` scalar into `out`. Returns false if the read could not be
// made (null window / not resolved); leaves `out` untouched on failure.
bool ReadClean(void* win, float& out);

// Write the window's `clean` scalar + call AbaseWindow_C::setClean() to push it into the
// shader (SetCustomPrimitiveDataFloat(0, clean) -- a pure setter, no gameplay side effect,
// game-thread). Returns false while fields, mesh or native dispatch are unavailable,
// including a partial repaint. The current predicate fences the field write and
// native call; an unchanged scalar may retry its paint. No cleanSponge is invoked.
bool WriteCleanAndApply(void* win, float clean,const std::function<bool()>& current = {});

}  // namespace ue_wrap::base_window
