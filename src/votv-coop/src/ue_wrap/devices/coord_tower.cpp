// ue_wrap/devices/coord_tower.cpp -- see ue_wrap/devices/coord_tower.h.
// Field layouts belong to a class incarnation; calls retain actor/function identity.

#include "ue_wrap/devices/coord_tower.h"

#include "ue_wrap/actors/prop.h"  // GetPropNameString
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"            // SpawnActor
#include "ue_wrap/engine/engine_component.h"  // GetComponentLocation
#include "ue_wrap/engine/engine_timeline.h"
#include "ue_wrap/engine/hit_result.h"

#include "tower_access.h"

#include <algorithm>
#include <chrono>

namespace ue_wrap::coord_tower {
namespace {

namespace R = reflection;
namespace TA = tower_access;
using field_io::TArrayView;

constexpr const wchar_t* kClassName = L"coordRadarDish_C";

struct Bool { int32_t off = -1; uint8_t mask = 0; };

CachedObjRef g_cls;             // coordRadarDish_C; a class the map loads, so a new world may load a new one
int32_t  g_offId = -1;
Bool     g_broken, g_opened, g_anim, g_leverMoving;
int32_t  g_offFuses = -1;       // TArray<uint8>
int32_t  g_offLights = -1;      // TArray<bool> puzzleLights
// The look-at getActionOptions writes and the use reads.
Bool     g_lookButton, g_lookLever, g_lookFuse, g_lookRetract;
int32_t  g_offButtonIndex = -1, g_offFuseIndex = -1;
// The parts a use strikes: TArray<UPrimitiveComponent*> puzzle_buttons and fusesButtons, the lever's box, the
// panel's first retract button.
int32_t  g_offButtons = -1, g_offFuseSlots = -1, g_offLeverBox = -1, g_offRetract = -1;
int32_t  g_offTimeline = -1;    // UTimelineComponent* leverTL, read through ue_wrap/engine/engine_timeline
// Audio components by Sound. The game names its two fuse sounds the other way round: a pull plays
// audio_fuseInsert and an insert audio_fusePullout.
constexpr const wchar_t* kSoundFields[] = { L"audio_click", L"audio_success", L"audio_fail", L"audio_fuseInsert",
                                            L"audio_fusePullout" };
constexpr int kSoundCount = sizeof(kSoundFields) / sizeof(kSoundFields[0]);
int32_t  g_offSound[kSoundCount] = {};
bool     g_resolving = false, g_interacting = false;
bool     g_membersResolved = false;
bool     g_latchedOff = false;  // the class loaded without a member: it will not appear later
uint64_t g_missMs = 0;          // a class lookup that misses walks the object array; hold off after one

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint8_t* Base(void* obj) { return reinterpret_cast<uint8_t*>(obj); }

bool ReadBit(void* obj, const Bool& b) { return (Base(obj)[b.off] & b.mask) != 0; }

void WriteBit(void* obj, const Bool& b, bool v) {
    uint8_t& byte = Base(obj)[b.off];
    byte = v ? static_cast<uint8_t>(byte | b.mask) : static_cast<uint8_t>(byte & ~b.mask);
}

int32_t& IntAt(void* obj, int32_t off) { return *reinterpret_cast<int32_t*>(Base(obj) + off); }

void* ObjectAt(void* obj, int32_t off) { return *reinterpret_cast<void* const*>(Base(obj) + off); }

TArrayView& ArrayAt(void* obj, int32_t off) { return *reinterpret_cast<TArrayView*>(Base(obj) + off); }

void* ArrayObject(void* obj, int32_t off, int32_t index) {
    const TArrayView& a = ArrayAt(obj, off);
    const int limit = off == g_offFuseSlots ? kMaxFuses : kMaxLights;
    if (!TA::Array(a, limit) || index < 0 || index >= a.num) return nullptr;
    void* o = reinterpret_cast<void* const*>(a.data)[index];
    return (o && R::IsLive(o)) ? o : nullptr;
}

// Resolve every member from `cls`, the loaded coordRadarDish_C; a member missing from it latches the wrapper off.
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
    g_offId = need(R::FindPropertyOffset(cls, L"id"), L"id", sizeof(int32_t));
    needBool(g_broken, L"isBroken");
    needBool(g_opened, L"opened");
    needBool(g_anim, L"isAnim");
    needBool(g_leverMoving, L"leverMoving");
    g_offFuses = need(R::FindPropertyOffset(cls, L"fuses"), L"fuses", sizeof(TArrayView));
    g_offLights = need(R::FindPropertyOffset(cls, L"puzzleLights"), L"puzzleLights", sizeof(TArrayView));
    needBool(g_lookButton, L"isLookAtButton");
    needBool(g_lookLever, L"isLookAtLever");
    needBool(g_lookFuse, L"isLookAtFuse");
    needBool(g_lookRetract, L"isLookAtRetract");
    g_offButtonIndex = need(R::FindPropertyOffset(cls, L"lookAtButtonIndex"), L"lookAtButtonIndex", sizeof(int32_t));
    g_offFuseIndex = need(R::FindPropertyOffset(cls, L"lookAtFuseIndex"), L"lookAtFuseIndex", sizeof(int32_t));
    g_offButtons = need(R::FindPropertyOffset(cls, L"puzzle_buttons"), L"puzzle_buttons", sizeof(TArrayView));
    g_offFuseSlots = need(R::FindPropertyOffset(cls, L"fusesButtons"), L"fusesButtons", sizeof(TArrayView));
    g_offLeverBox = need(R::FindPropertyOffset(cls, L"buttonLever"), L"buttonLever", sizeof(void*));
    g_offRetract = need(R::FindPropertyOffset(cls, L"button_retract1"), L"button_retract1", sizeof(void*));
    g_offTimeline = need(R::FindPropertyOffset(cls, L"leverTL"), L"leverTL", sizeof(void*));
    for (int i = 0; i < kSoundCount; ++i) g_offSound[i] = need(R::FindPropertyOffset(cls, kSoundFields[i]),
                                                               kSoundFields[i], sizeof(void*));
    if (missing) {
        g_latchedOff = true;
        UE_LOGW("coord_tower: coordRadarDish_C is loaded but %ls did not resolve -- the tower reads and writes stay "
                "off; game version mismatch?", missing);
        return false;
    }
    g_membersResolved = true;
    UE_LOGI("coord_tower: resolved (id=0x%X isBroken=0x%X opened=0x%X fuses=0x%X puzzleLights=0x%X leverTL=0x%X)",
            g_offId, g_broken.off, g_opened.off, g_offFuses, g_offLights, g_offTimeline);
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

struct Collect { void** out; int32_t cap; int32_t n; };

void CollectOne(void* ctx, void* obj, int32_t index) {
    auto* c = static_cast<Collect*>(ctx);
    if (c->n >= c->cap || !obj) return;
    // An index member may still be loading, under construction or dying; the slot's flags say so.
    if (R::SlotFlags(index) & (R::slot_flags::Dying | R::slot_flags::NotYetReadable)) return;
    if (!R::IsLive(obj) || R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
    c->out[c->n++] = obj;
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
    case Part::Lever:   { void* c = ObjectAt(tower, g_offLeverBox); return (c && R::IsLive(c)) ? c : nullptr; }
    case Part::Retract: { void* c = ObjectAt(tower, g_offRetract); return (c && R::IsLive(c)) ? c : nullptr; }
    default:            return nullptr;
    }
}

struct SavedLook {
    bool button, lever, fuse, retract;
    int32_t buttonIndex, fuseIndex;
};

SavedLook SaveLook(void* tower) {
    return {ReadBit(tower, g_lookButton), ReadBit(tower, g_lookLever), ReadBit(tower, g_lookFuse),
            ReadBit(tower, g_lookRetract), IntAt(tower, g_offButtonIndex), IntAt(tower, g_offFuseIndex)};
}

void RestoreLook(void* tower, const SavedLook& s) {
    WriteBit(tower, g_lookButton, s.button);
    WriteBit(tower, g_lookLever, s.lever);
    WriteBit(tower, g_lookFuse, s.fuse);
    WriteBit(tower, g_lookRetract, s.retract);
    IntAt(tower, g_offButtonIndex) = s.buttonIndex;
    IntAt(tower, g_offFuseIndex) = s.fuseIndex;
}

// What getActionOptions writes for `at`: one flag on, the others off, and the index of a button or a fuse.
void WriteLook(void* tower, const LookAt& at) {
    WriteBit(tower, g_lookButton, at.part == Part::Button);
    WriteBit(tower, g_lookLever, at.part == Part::Lever);
    WriteBit(tower, g_lookFuse, at.part == Part::Fuse);
    WriteBit(tower, g_lookRetract, at.part == Part::Retract);
    if (at.part == Part::Button) IntAt(tower, g_offButtonIndex) = at.index;
    if (at.part == Part::Fuse) IntAt(tower, g_offFuseIndex) = at.index;
}

bool SameLook(const SavedLook& a, const SavedLook& b) {
    return a.button == b.button && a.lever == b.lever && a.fuse == b.fuse && a.retract == b.retract && a.buttonIndex == b.buttonIndex && a.fuseIndex == b.fuseIndex;
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

bool EnsureResolved() {
    if (g_membersResolved && g_cls.Alive()) return true;
    if (g_latchedOff && g_cls.Get()) return false;
    const uint64_t now = NowMs();
    if (g_missMs != 0 && now - g_missMs < 2000) return false;
    void* cls = object_index::ClassByName(kClassName);
    if (!cls) { g_missMs = now; return false; }
    g_missMs = 0;
    return Adopt(cls);
}

int32_t ReadAll(void** out, int32_t cap) {
    if (!out || cap <= 0 || !EnsureResolved()) return -1;
    Collect c{out, cap, 0};
    object_index::ForEachInstance(g_cls.Raw(), &CollectOne, &c);
    std::sort(out, out + c.n, [](void* a, void* b) { return IntAt(a, g_offId) < IntAt(b, g_offId); });
    return c.n;
}

int32_t IdOf(void* tower) { return Usable(tower) ? IntAt(tower, g_offId) : -1; }

bool Read(void* tower, State& out) {
    Scope scope(tower);
    if (!scope.Current()) return false;
    State value{};
    value.id = IntAt(tower, g_offId);
    value.isBroken = ReadBit(tower, g_broken);
    value.opened = ReadBit(tower, g_opened);
    value.isAnim = ReadBit(tower, g_anim);
    value.leverMoving = ReadBit(tower, g_leverMoving);
    const int32_t timelineOffset = g_offTimeline;
    void* timeline = ObjectAt(tower, timelineOffset);
    CachedObjRef ref;
    if (timeline && R::IsLive(timeline)) ref.Set(timeline);
    engine_timeline::State tl{};
    const bool read = ref.Get() && engine_timeline::Read(timeline, tl);
    if (!scope.Current() || ObjectAt(tower, timelineOffset) != timeline || (timeline && !ref.Get())) return false;
    if (read) value.leverUp = !tl.reverse && (tl.playing || tl.position > 0.0f);
    const TArrayView f = ArrayAt(tower, g_offFuses);
    const TArrayView puzzle = ArrayAt(tower, g_offLights);
    if (!TA::Array(f, kMaxFuses) || !TA::Array(puzzle, kMaxLights)) return false;
    value.fuseCount = static_cast<uint8_t>(f.num);
    for (int i = 0; i < f.num; ++i) {
        if (f.data[i] > 2) return false;
        value.fuses[i] = f.data[i];
    }
    value.lightCount = static_cast<uint8_t>(puzzle.num);
    for (int i = 0; i < puzzle.num; ++i) value.lights[i] = puzzle.data[i] != 0;
    out = value;
    return true;
}

bool WriteBroken(void* tower, bool broken) {
    if (!Usable(tower)) return false;
    WriteBit(tower, g_broken, broken);
    return true;
}

bool WriteLeverMoving(void* tower, bool moving) {
    if (!Usable(tower)) return false;
    WriteBit(tower, g_leverMoving, moving);
    return true;
}

bool WriteFuse(void* tower, int32_t slot, uint8_t value) {
    if (slot < 0 || slot >= kMaxFuses || value > 2 || !Usable(tower)) return false;
    const TArrayView f = ArrayAt(tower, g_offFuses);
    if (!TA::Array(f, kMaxFuses) || slot >= f.num) return false;
    f.data[slot] = value;
    return true;
}

bool WriteLights(void* tower, const bool* values, int32_t count) {
    if (!values || count < 0 || count > kMaxLights || !Usable(tower)) return false;
    const TArrayView p = ArrayAt(tower, g_offLights);
    if (!TA::Array(p, kMaxLights) || p.num != count) return false;
    for (int32_t i = 0; i < count; ++i) p.data[i] = values[i] ? 1 : 0;
    return true;
}

bool UpdBroken(void* tower) { return CallNamed(tower, L"updBroken"); }
bool UpdPuzzle(void* tower) { return CallNamed(tower, L"updPuzzle"); }
bool UpdFuses(void* tower) { return CallNamed(tower, L"updFuses"); }
bool SolvePuzzle(void* tower) { return CallNamed(tower, L"solvePuzzle"); }
bool Scramble(void* tower) { return CallNamed(tower, L"Scramble Radar Dish"); }

bool MoveLever(void* tower, bool up) {
    Scope scope(tower);
    return TA::Dispatch(tower, L"moveLever", [&] { return scope.Current(); }, [&](ParamFrame& f) {
        return f.Set<bool>(L"Condition", up);
    });
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

bool ReadLookAt(void* tower, LookAt& out) {
    if (!Usable(tower)) return false;
    LookAt value{};
    // getActionOptions tests the button first, then the lever, the fuse and the retract; the use tests the
    // retract first, then the fuse, the button and the lever. Only one is ever set by a trace.
    if (ReadBit(tower, g_lookRetract)) value.part = Part::Retract;
    else if (ReadBit(tower, g_lookFuse)) value = {Part::Fuse, IntAt(tower, g_offFuseIndex)};
    else if (ReadBit(tower, g_lookButton)) value = {Part::Button, IntAt(tower, g_offButtonIndex)};
    else if (ReadBit(tower, g_lookLever)) value.part = Part::Lever;
    if (value.part == Part::Button || value.part == Part::Fuse) {
        const int limit = value.part == Part::Fuse ? kMaxFuses : kMaxLights;
        const int32_t off = value.part == Part::Fuse ? g_offFuses : g_offLights;
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

bool IsFuse(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    void* cls = object_index::ClassByName(L"prop_fuse_C");
    return cls && R::ClassOf(actor) == cls;
}

void* SpawnFuse(const FVector& at) {
    void* cls = object_index::ClassByName(L"prop_fuse_C");
    return cls ? engine::SpawnActor(cls, at) : nullptr;
}
void* SpawnFuseTracked(const FVector& at,bool& attempted,const std::function<bool()>& current) {
    attempted=false;
    void* cls=object_index::ClassByName(L"prop_fuse_C");
    return cls && R::IsLive(cls) ? engine::SpawnActorTracked(cls,at,attempted,current) : nullptr;
}

bool IsPulledFuse(void* actor) {
    return actor && R::IsLive(actor) && prop::GetPropNameString(actor) == L"fuse_0";
}

bool InsertFuse(void* tower, void* player, int32_t slot, void* fuse) {
    return Interact(tower, player, {Part::Fuse, slot}, fuse, true);
}

}  // namespace ue_wrap::coord_tower
