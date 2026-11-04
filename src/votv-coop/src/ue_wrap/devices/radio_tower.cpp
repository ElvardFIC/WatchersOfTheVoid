// ue_wrap/devices/radio_tower.cpp -- see ue_wrap/devices/radio_tower.h. Offsets are the class's layout, the same
// in every world, and resolve once by name; verbs resolve on the tower in hand through the dispatch cache.
// The BP facts (bp_cpp radiotower_C, line numbers are its pseudo-C++): getActionOptions recomputes every
// look-at flag on every look (:1803-1827); actionOptionIndex acts on the fuse look, the button, the reset lever,
// then the confirm lever, and is ignored while `animation` (:510-568); playerUsedOn is gated the same (:655);
// setBroken is (isBroken, pullLever) (:1163); scramble, tryToFix and completeMinigame take no argument (:1496,
// :1599, :2185).

#include "ue_wrap/devices/radio_tower.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine_component.h"  // GetComponentLocation, PlayAudioComponent
#include "ue_wrap/engine/engine_timeline.h"
#include "ue_wrap/engine/hit_result.h"
#include "ue_wrap/world/world_singleton.h"

#include "tower_access.h"

#include <algorithm>

namespace ue_wrap::radio_tower {
namespace {

namespace R = reflection;
namespace TA = tower_access;
using field_io::TArrayView;

constexpr const wchar_t* kClassName = L"radiotower_C";

struct Bool { int32_t off = -1; uint8_t mask = 0; };

CachedObjRef g_cls;             // radiotower_C; a class the map loads, so a new world may load a new one
Bool     g_broken, g_anim, g_skipPuzzle;
int32_t  g_offFuses = -1;       // TArray<uint8>
int32_t  g_offCells = -1;       // TArray<bool> puzzleData
int32_t  g_offSize = -1;        // FIntPoint puzzleSize
// The look-at getActionOptions writes and the use reads.
Bool     g_lookButton, g_lookConfirm, g_lookReset, g_lookFuse;
int32_t  g_offButtonIndex = -1, g_offFuseIndex = -1;
// The parts a use strikes: TArray<UPrimitiveComponent*> puzzleButtons and fusesButtons, the two levers' boxes.
int32_t  g_offButtons = -1, g_offFuseSlots = -1, g_offConfirmBox = -1, g_offResetBox = -1;
int32_t  g_offLeverTl = -1, g_offResetTl = -1;  // UTimelineComponent* TL_lever, TL_reset
// Audio components by Sound. The game names its two fuse sounds the other way round: a pull plays
// audio_fuseInsert and an insert audio_fusePullout.
constexpr const wchar_t* kSoundFields[] = { L"audio_click", L"audio_success", L"audio_fail", L"audio_fuseInsert",
                                            L"audio_fusePullout", L"audio_lever", L"audio_scramble" };
constexpr int kSoundCount = sizeof(kSoundFields) / sizeof(kSoundFields[0]);
int32_t  g_offSound[kSoundCount] = {};
bool     g_resolving = false, g_interacting = false;
bool     g_membersResolved = false;
bool     g_latchedOff = false;  // the class loaded without a member: it will not appear later

uint8_t* Base(void* obj) { return reinterpret_cast<uint8_t*>(obj); }

bool ReadBit(void* obj, const Bool& b) { return (Base(obj)[b.off] & b.mask) != 0; }

void WriteBit(void* obj, const Bool& b, bool v) {
    uint8_t& byte = Base(obj)[b.off];
    byte = v ? static_cast<uint8_t>(byte | b.mask) : static_cast<uint8_t>(byte & ~b.mask);
}

int32_t& IntAt(void* obj, int32_t off) { return *reinterpret_cast<int32_t*>(Base(obj) + off); }

void* ObjectAt(void* obj, int32_t off) { return *reinterpret_cast<void* const*>(Base(obj) + off); }

TArrayView& ArrayAt(void* obj, int32_t off) { return *reinterpret_cast<TArrayView*>(Base(obj) + off); }

void* LiveObjectAt(void* obj, int32_t off) {
    void* o = ObjectAt(obj, off);
    return (o && R::IsLive(o)) ? o : nullptr;
}

void* ArrayObject(void* obj, int32_t off, int32_t index) {
    const TArrayView& a = ArrayAt(obj, off);
    const int limit = off == g_offFuseSlots ? kMaxFuses : kMaxCells;
    if (!TA::Array(a, limit) || index < 0 || index >= a.num) return nullptr;
    void* o = reinterpret_cast<void* const*>(a.data)[index];
    return (o && R::IsLive(o)) ? o : nullptr;
}

// Resolve every member from `cls`, the loaded radiotower_C; a member missing from it latches the wrapper off.
bool ResolveMembers(void* cls) {
    const int32_t size = R::StructSize(cls);
    const wchar_t* missing = nullptr;
    auto need = [&](int32_t off, const wchar_t* name, int32_t bytes) {
        if (!TA::Span(off, bytes, size)) { if (!missing) missing = name; return -1; }
        return off;
    };
    auto needBool = [&](Bool& b, const wchar_t* name) {
        if (!R::FindBoolProperty(cls, name, b.off, b.mask) || !TA::Span(b.off, 1, size) ||
            !b.mask || (b.mask != 0xff && (b.mask & (b.mask - 1)))) { b.off = -1; if (!missing) missing = name; }
    };
    needBool(g_broken, L"isBroken");
    needBool(g_anim, L"animation");
    needBool(g_skipPuzzle, L"skipPuzzle");
    g_offFuses = need(R::FindPropertyOffset(cls, L"fuses"), L"fuses", sizeof(TArrayView));
    g_offCells = need(R::FindPropertyOffset(cls, L"puzzleData"), L"puzzleData", sizeof(TArrayView));
    g_offSize = need(R::FindPropertyOffset(cls, L"puzzleSize"), L"puzzleSize", 2 * sizeof(int32_t));
    needBool(g_lookButton, L"isLookAtButton");
    needBool(g_lookConfirm, L"lookAtConfirm");
    needBool(g_lookReset, L"lookAtReset");
    needBool(g_lookFuse, L"lookAtFuse");
    g_offButtonIndex = need(R::FindPropertyOffset(cls, L"lookAtIndex"), L"lookAtIndex", sizeof(int32_t));
    g_offFuseIndex = need(R::FindPropertyOffset(cls, L"lookAtFuseIndex"), L"lookAtFuseIndex", sizeof(int32_t));
    g_offButtons = need(R::FindPropertyOffset(cls, L"puzzleButtons"), L"puzzleButtons", sizeof(TArrayView));
    g_offFuseSlots = need(R::FindPropertyOffset(cls, L"fusesButtons"), L"fusesButtons", sizeof(TArrayView));
    g_offConfirmBox = need(R::FindPropertyOffset(cls, L"button_confirm"), L"button_confirm", sizeof(void*));
    g_offResetBox = need(R::FindPropertyOffset(cls, L"button_reset"), L"button_reset", sizeof(void*));
    g_offLeverTl = need(R::FindPropertyOffset(cls, L"TL_lever"), L"TL_lever", sizeof(void*));
    g_offResetTl = need(R::FindPropertyOffset(cls, L"TL_reset"), L"TL_reset", sizeof(void*));
    for (int i = 0; i < kSoundCount; ++i) g_offSound[i] = need(R::FindPropertyOffset(cls, kSoundFields[i]),
                                                               kSoundFields[i], sizeof(void*));
    if (missing) {
        g_latchedOff = true;
        UE_LOGW("radio_tower: radiotower_C is loaded but %ls did not resolve -- the tower reads and writes stay "
                "off; game version mismatch?", missing);
        return false;
    }
    g_membersResolved = true;
    UE_LOGI("radio_tower: resolved (isBroken=0x%X animation=0x%X fuses=0x%X puzzleData=0x%X TL_lever=0x%X)",
            g_broken.off, g_anim.off, g_offFuses, g_offCells, g_offLeverTl);
    return true;
}

// Resolve the layout once per class incarnation; a replacement also retires a failed layout.
bool Adopt(void* cls) {
    if (g_resolving || !cls || !R::IsLive(cls)) return false;
    if (g_cls.Is(cls)) return g_membersResolved && !g_latchedOff;
    TA::Busy resolving(g_resolving);
    g_membersResolved = false;
    g_latchedOff = false;
    g_cls.Set(cls);
    const uint32_t world = world_identity::Generation();
    const bool ok = ResolveMembers(cls);
    if (!g_cls.Is(cls) || world != world_identity::Generation()) {
        g_cls.Reset(); g_membersResolved = false; g_latchedOff = false;
        return false;
    }
    return ok;
}

struct Scope : TA::Lifetime {
    bool ready = false;
    explicit Scope(void* tower) : TA::Lifetime(tower) {
        ready = TA::Lifetime::Current() && R::NameEquals(R::NameOf(cls.Raw()), kClassName) && Adopt(cls.Raw());
    }
    bool Current() const {
        return ready && TA::Lifetime::Current() && g_cls.Is(cls.Raw()) && g_membersResolved && !g_resolving;
    }
};

bool Usable(void* tower) { return Scope(tower).Current(); }

bool CallNamed(void* tower, const wchar_t* fn) {
    Scope scope(tower);
    return TA::Dispatch(tower, fn, [&] { return scope.Current(); }, [](ParamFrame&) { return true; });
}

// The component a look-at names, the part a use strikes.
void* PartComponent(void* tower, const LookAt& at) {
    switch (at.part) {
    case Part::Button:  return ArrayObject(tower, g_offButtons, at.index);
    case Part::Fuse:    return ArrayObject(tower, g_offFuseSlots, at.index);
    case Part::Confirm: return LiveObjectAt(tower, g_offConfirmBox);
    case Part::Reset:   return LiveObjectAt(tower, g_offResetBox);
    default:            return nullptr;
    }
}

struct SavedLook {
    bool button, confirm, reset, fuse;
    int32_t buttonIndex, fuseIndex;
};

SavedLook SaveLook(void* tower) {
    return {ReadBit(tower, g_lookButton), ReadBit(tower, g_lookConfirm), ReadBit(tower, g_lookReset),
            ReadBit(tower, g_lookFuse), IntAt(tower, g_offButtonIndex), IntAt(tower, g_offFuseIndex)};
}

void RestoreLook(void* tower, const SavedLook& s) {
    WriteBit(tower, g_lookButton, s.button);
    WriteBit(tower, g_lookConfirm, s.confirm);
    WriteBit(tower, g_lookReset, s.reset);
    WriteBit(tower, g_lookFuse, s.fuse);
    IntAt(tower, g_offButtonIndex) = s.buttonIndex;
    IntAt(tower, g_offFuseIndex) = s.fuseIndex;
}

// What getActionOptions writes for `at`: one flag on, the others off, and the index of a button or a fuse.
void WriteLook(void* tower, const LookAt& at) {
    WriteBit(tower, g_lookButton, at.part == Part::Button);
    WriteBit(tower, g_lookConfirm, at.part == Part::Confirm);
    WriteBit(tower, g_lookReset, at.part == Part::Reset);
    WriteBit(tower, g_lookFuse, at.part == Part::Fuse);
    if (at.part == Part::Button) IntAt(tower, g_offButtonIndex) = at.index;
    if (at.part == Part::Fuse) IntAt(tower, g_offFuseIndex) = at.index;
}

bool SameLook(const SavedLook& a, const SavedLook& b) {
    return a.button == b.button && a.confirm == b.confirm && a.reset == b.reset && a.fuse == b.fuse && a.buttonIndex == b.buttonIndex && a.fuseIndex == b.fuseIndex;
}
struct LookRestore {
    const Scope& scope;
    SavedLook saved, staged;
    LookRestore(const Scope& s, const LookAt& at) : scope(s), saved(SaveLook(s.actor.Raw())) {
        WriteLook(scope.actor.Raw(), at);
        staged = SaveLook(scope.actor.Raw());
    }
    ~LookRestore() {
        if (scope.Current() && SameLook(SaveLook(scope.actor.Raw()), staged)) RestoreLook(scope.actor.Raw(), saved);
    }
};


}  // namespace

void* Instance() {
    void* gm = world_singleton::Gamemode();
    TA::Lifetime owner(gm);
    if (!owner.Current()) return nullptr;
    const int32_t off = R::FindPropertyOffset(owner.cls.Raw(), L"radiotower");
    if (!owner.Current()) return nullptr;
    const int32_t size = R::StructSize(owner.cls.Raw());
    if (!owner.Current() || !TA::Span(off, sizeof(void*), size)) return nullptr;
    void* rt = ObjectAt(gm, off);
    Scope tower(rt);
    return tower.Current() && owner.Current() && ObjectAt(gm, off) == rt ? rt : nullptr;
}

bool IsTower(void* obj) { return Usable(obj); }

bool Read(void* tower, State& out) {
    Scope scope(tower);
    if (!scope.Current()) return false;
    State value{};
    value.isBroken = ReadBit(tower, g_broken);
    value.animation = ReadBit(tower, g_anim);
    value.skipPuzzle = ReadBit(tower, g_skipPuzzle);
    const int32_t width = IntAt(tower, g_offSize), height = IntAt(tower, g_offSize + 4);
    if (width < 0 || width > kMaxCells || height < 0 || height > kMaxCells || width * height > kMaxCells) return false;
    value.width = static_cast<uint8_t>(width);
    value.height = static_cast<uint8_t>(height);
    const int32_t timelineOffsets[] = {g_offLeverTl, g_offResetTl};
    void* timelines[] = {ObjectAt(tower, timelineOffsets[0]), ObjectAt(tower, timelineOffsets[1])};
    CachedObjRef refs[2];
    for (int i = 0; i < 2; ++i) {
        if (timelines[i] && R::IsLive(timelines[i])) refs[i].Set(timelines[i]);
    }
    auto timelinesCurrent = [&] {
        if (!scope.Current()) return false;
        for (int i = 0; i < 2; ++i)
            if (ObjectAt(tower, timelineOffsets[i]) != timelines[i] || (timelines[i] && !refs[i].Get())) return false;
        return true;
    };
    for (int i = 0; i < 2; ++i) {
        if (!timelinesCurrent()) return false;
        engine_timeline::State tl{};
        const bool read = refs[i].Get() && engine_timeline::Read(timelines[i], tl);
        if (!timelinesCurrent()) return false;
        if (read) {
            if (i == 0) { value.leverPlaying = tl.playing; value.leverForward = !tl.reverse; }
            else value.resetPlaying = tl.playing;
        }
    }
    const TArrayView f = ArrayAt(tower, g_offFuses);
    const TArrayView puzzle = ArrayAt(tower, g_offCells);
    if (!TA::Array(f, kMaxFuses) || !TA::Array(puzzle, kMaxCells)) return false;
    value.fuseCount = static_cast<uint8_t>(f.num);
    for (int i = 0; i < f.num; ++i) {
        if (f.data[i] > 2) return false;
        value.fuses[i] = f.data[i];
    }
    value.cellCount = static_cast<uint8_t>(puzzle.num);
    for (int i = 0; i < puzzle.num; ++i) value.cells[i] = puzzle.data[i] != 0;
    out = value;
    return true;
}

bool WriteBroken(void* tower, bool broken) {
    if (!Usable(tower)) return false;
    WriteBit(tower, g_broken, broken);
    return true;
}

bool WriteAnimation(void* tower, bool animation) {
    if (!Usable(tower)) return false;
    WriteBit(tower, g_anim, animation);
    return true;
}

bool WriteFuse(void* tower, int32_t slot, uint8_t value) {
    if (slot < 0 || slot >= kMaxFuses || value > 2 || !Usable(tower)) return false;
    const TArrayView f = ArrayAt(tower, g_offFuses);
    if (!TA::Array(f, kMaxFuses) || slot >= f.num) return false;
    f.data[slot] = value;
    return true;
}

bool WriteCells(void* tower, const bool* values, int32_t count) {
    if (!values || count < 0 || count > kMaxCells || !Usable(tower)) return false;
    const TArrayView p = ArrayAt(tower, g_offCells);
    if (!TA::Array(p, kMaxCells) || p.num != count) return false;
    for (int32_t i = 0; i < count; ++i) p.data[i] = values[i] ? 1 : 0;
    return true;
}

bool UpdPuzzle(void* tower) { return CallNamed(tower, L"updPuzzle"); }
bool UpdFuses(void* tower) { return CallNamed(tower, L"updFuses"); }
bool CompleteMinigame(void* tower) { return CallNamed(tower, L"completeMinigame"); }
bool Scramble(void* tower) { return CallNamed(tower, L"scramble"); }

bool SetBroken(void* tower, bool broken, bool pullLever) {
    Scope scope(tower);
    return TA::Dispatch(tower, L"setBroken", [&] { return scope.Current(); }, [&](ParamFrame& f) {
        return f.Set<bool>(L"isBroken", broken) && f.Set<bool>(L"pullLever", pullLever);
    });
}

bool PlayLever(void* tower, Lever lever, bool forward) {
    Scope scope(tower);
    if (!scope.Current() || (lever != Lever::Confirm && lever != Lever::Reset) ||
        (lever == Lever::Reset && !forward)) return false;
    const int32_t off = lever == Lever::Reset ? g_offResetTl : g_offLeverTl;
    void* tl = ObjectAt(tower, off);
    TA::Lifetime timeline(tl);
    if (!timeline.Current()) return false;
    return TA::Dispatch(tl, forward ? L"PlayFromStart" : L"ReverseFromEnd", [&] {
        return scope.Current() && timeline.Current() && ObjectAt(tower, off) == tl;
    }, [](ParamFrame&) { return true; });
}

bool Play(void* tower, Sound sound) {
    const int i = static_cast<int>(sound);
    Scope scope(tower);
    if (!scope.Current() || i < 0 || i >= kSoundCount) return false;
    const int32_t off = g_offSound[i];
    void* comp = ObjectAt(tower, off);
    TA::Lifetime component(comp);
    if (!component.Current()) return false;
    return TA::Dispatch(comp, L"Play", [&] {
        return scope.Current() && component.Current() && ObjectAt(tower, off) == comp;
    }, [](ParamFrame& f) { return f.Set<float>(L"StartTime", 0.f); });
}

bool Alert(void* tower, bool damagedHealth) {
    Scope scope(tower);
    if (!scope.Current()) return false;
    void* console = world_singleton::Find(L"ui_console_C");
    return TA::Dispatch(console, L"radioTowerBroken", [&] { return scope.Current(); }, [&](ParamFrame& f) {
        return f.Set<void*>(L"radiotower", tower) && f.Set<bool>(L"damagedHealth", damagedHealth);
    });
}

bool ReadLookAt(void* tower, LookAt& out) {
    if (!Usable(tower)) return false;
    LookAt value{};
    // getActionOptions tests the button, the confirm lever, the reset lever, then the fuse; the use tests the fuse
    // first, then the button, the reset lever and the confirm lever. Only one is ever set by a trace.
    if (ReadBit(tower, g_lookFuse)) value = {Part::Fuse, IntAt(tower, g_offFuseIndex)};
    else if (ReadBit(tower, g_lookButton)) value = {Part::Button, IntAt(tower, g_offButtonIndex)};
    else if (ReadBit(tower, g_lookReset)) value.part = Part::Reset;
    else if (ReadBit(tower, g_lookConfirm)) value.part = Part::Confirm;
    if (value.part == Part::Button || value.part == Part::Fuse) {
        const int limit = value.part == Part::Fuse ? kMaxFuses : kMaxCells;
        const int32_t off = value.part == Part::Fuse ? g_offFuses : g_offCells;
        const TArrayView a = ArrayAt(tower, off);
        if (!TA::Array(a, limit) || value.index < 0 || value.index >= a.num) return false;
    }
    out = value;
    return true;
}

bool ReadFuseLook(void* tower, int32_t& slot) {
    if (!Usable(tower)) return false;
    const int32_t value = IntAt(tower, g_offFuseIndex);
    const TArrayView f = ArrayAt(tower, g_offFuses);
    if (!TA::Array(f, kMaxFuses) || value < 0 || value >= f.num) return false;
    slot = value;
    return true;
}

bool Interact(void* tower, void* player, const LookAt& input, void* fuse, bool insert) {
    const LookAt at = input;
    if (g_interacting) return false;
    TA::Busy busy(g_interacting);
    Scope scope(tower);
    TA::Lifetime owner(player), held(fuse);
    if (!scope.Current() || !owner.Current() || (insert && !held.Current())) return false;
    void* part = PartComponent(tower, at);
    TA::Lifetime component(part);
    auto current = [&] {
        return scope.Current() && owner.Current() && component.Current() &&
               (!insert || held.Current()) && PartComponent(tower, at) == part;
    };
    if (!current()) return false;
    FVector where{};
    if (!TA::Location(part, current, where)) return false;
    if (!current() || !TA::Finite(where)) return false;
    // The restorer stays alive through dispatch, including C++ exception unwinding.
    LookRestore restore(scope, at);
    auto staged = [&] { return current() && SameLook(SaveLook(tower), restore.staged); };
    return TA::Dispatch(tower, insert ? L"playerUsedOn" : L"actionOptionIndex", staged,
        [&](ParamFrame& f) {
            return current() && f.Set<void*>(L"player", player) && current() &&
                   hit_result::WriteWhile(f, L"hit", tower, part, where, staged) && current() &&
                   f.Set<void*>(L"lookAtComponent", part) &&
                   (insert ? f.Set<void*>(L"holdObject", fuse) : f.Set<uint8_t>(L"action", 4));
        });
}

bool Use(void* tower, void* player, const LookAt& at) {
    return Interact(tower, player, at, nullptr, false);
}

bool PartLocation(void* tower, const LookAt& input, FVector& out) {
    const LookAt at = input;
    Scope scope(tower);
    if (!scope.Current()) return false;
    void* part = PartComponent(tower, at);
    TA::Lifetime component(part);
    if (!component.Current()) return false;
    auto current = [&] {
        return scope.Current() && component.Current() && PartComponent(tower, at) == part;
    };
    FVector where{};
    if (!TA::Location(part, current, where)) return false;
    if (!scope.Current() || !component.Current() || PartComponent(tower, at) != part || !TA::Finite(where)) return false;
    out = where;
    return true;
}

bool InsertFuse(void* tower, void* player, int32_t slot, void* fuse) {
    return Interact(tower, player, {Part::Fuse, slot}, fuse, true);
}

}  // namespace ue_wrap::radio_tower
