// coop/player/hand_history.h -- what each slot held in its hand, and when.
//
// A short per-slot ring of (class, from, to) entries, so the host can ask whether a slot held a class
// at any moment in the last N ms. The hand lane stamps it where it APPLIES a slot's state, never at
// receive: that lane paces its applies, so the history follows the hand the host shows. Engine-free
// and wire-free; game thread only.

#pragma once

#include <cstdint>
#include <string>

namespace coop::hand_history {

// Entries kept per slot; the oldest is overwritten.
inline constexpr int kRing = 4;

// Records that `slot` now holds `cls` (has) or holds nothing (!has), at `nowMs`. The class already
// held changes nothing; a different class or an empty hand closes the open entry first.
void Note(uint8_t slot, bool has, const std::wstring& cls, uint64_t nowMs);

// True when `slot` held `cls` (a case-insensitive class name) at any moment in [nowMs - windowMs, nowMs].
bool HeldWithin(uint8_t slot, const wchar_t* cls, uint64_t windowMs, uint64_t nowMs);

// Forgets `slot`'s entries (its leave, a session reset).
void Clear(uint8_t slot);

// Once per process; logs ALL PASS / n/m checks passed, and later calls return the first verdict silently.
bool RunSelftest();

}  // namespace coop::hand_history
