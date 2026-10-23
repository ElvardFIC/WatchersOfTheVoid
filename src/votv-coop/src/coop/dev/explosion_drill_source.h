// coop/dev/explosion_drill_source.h -- what the source arms' three files share: the leg table's types, the run's one
// state, and the helpers the legs call. explosion_drill_source.cpp walks the legs and owns the salt ones;
// explosion_drill_source_verbs.cpp owns the held legs' verbs and the guard;
// explosion_drill_source_measure.cpp owns the measure legs (sticky_parent, selfpill) and the host's thrown-prop watch.
// The request arms' client half (explosion_drill_request.cpp) acts through the same verbs and the host's half
// (explosion_drill_request_host.cpp) holds the same guard; the drill's host half (explosion_drill_host.cpp) puts it
// back when it abandons.
// Private to coop/dev: those six files and the hostile drill's explosion family (its client and host halves hold the
// same guard and take the same items) include it. Dev only; game thread.

#pragma once

#include "coop/dev/explosion_drill_internal.h"
#include "coop/world/explosion_sync.h"

#include "ue_wrap/core/cached_obj_ref.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coop::dev::explosion_drill::detail {

constexpr auto  kStepBound = std::chrono::seconds(10);    // a step that waits on the game
constexpr auto  kBirthBound = std::chrono::seconds(10);   // from the verb that starts the fuse
constexpr auto  kAttachBound = std::chrono::seconds(5);   // a sticky bomb that never attaches is detonated anyway
constexpr auto  kSaltBound = std::chrono::seconds(20);    // from the ignite: the fuse is about ten seconds
constexpr auto  kPillBound = std::chrono::seconds(20);    // from the ignite: the burn's first tick is about a second
constexpr auto  kFateWindow = std::chrono::seconds(2);    // how long the bomb is watched after its parent is destroyed
constexpr float kLift = 50.f;
constexpr float kMoveCm = 500.f;                          // how far the crate is moved under the stuck bomb
// A refused spawn is the answer after a few tries at this gap, not a thing to hammer every frame.
constexpr int  kSpawnTries = 3;
constexpr auto kSpawnGap = std::chrono::milliseconds(500);

constexpr const wchar_t* kRocketCls = L"prop_rocket_C";
constexpr const wchar_t* kBombCls = L"prop_stickybomb_C";

enum class Kind : uint8_t { Grenade, Pipebomb, Rocket, Sticky, SaltAttached, SaltFree, StickyParent, SelfPill };
struct LegDef {
    const char*    name;
    Kind           kind;
    const wchar_t* row;
    const wchar_t* cls;
    const wchar_t* path;
};

inline bool IsSalt(Kind k) { return k == Kind::SaltAttached || k == Kind::SaltFree; }
inline bool IsSticky(Kind k) { return k == Kind::Sticky || k == Kind::StickyParent; }
// A leg that goes through an item in the player's hand.
inline bool IsHeld(Kind k) { return !IsSalt(k) && k != Kind::SelfPill; }

enum class SP : uint8_t { Fixture, Legs, Done };
enum class LStep : uint8_t { Begin, Held, Aim, Act, Act2, AwaitBomb, AwaitAttach, Detonate, Birth, SaltRest, SaltIgnite,
                             SaltWait, CrateRest, Follow, Followed, CrateGone, PillWait, End, Settle };

// What one leg read. -1 is unread.
struct Reading {
    float       timer = -1.f, reload = -1.f;
    int         ammo = -1, active = -1, attached = -1, rain0 = -1, rain1 = -1, parentSim = -1;
    bool        thrown = false, fired = false, detonated = false, ignited = false, birth = false;
    int         rocketMax = 0, bombMax = 0;
    long long   ms = -1, explodeMs = -1;
    const char* skip = nullptr;
    char        skipText[200] = "";  // a reason that names what the leg found (skip points at it)
    // The weather before the leg's first spawn (restored at its end), and the spawn tries of a leg that spawns.
    int               rainBefore = -1;
    int               spawnTries = 0;
    Clock::time_point spawnAt{};
    // The projectile the leg watches: its location on the last tick it was seen live.
    ue_wrap::CachedObjRef track;
    Vec                   last{};
    bool                  haveLast = false;
    // sticky_parent: the bomb's location before the crate moved, how far it followed, and its fate after the crate's
    // destruction (alive = -1 unread, the birth in that window, the ms from the destroy to the bomb's end).
    Vec                   bomb0{};
    float                 followed = -1.f;
    int                   bombAlive = -1, fateBirth = 0;
    long long             goneMs = -1;
    Clock::time_point     destroyAt{};
    // selfpill: the pill before and after, the attempt (0 = guard on, 1 = guard off), the burn the game showed, and
    // the client's self requests sent when the player was lit (its birth is the request that follows).
    float                 pillBefore = -1.f, pill0 = -1.f, pill1 = -1.f, burnMax = 0.f, scale = -1.f;
    int                   attempt = 0, burning = 0;
    uint64_t              selfSent = 0;
};

struct Src {
    SP                    phase = SP::Fixture;
    int                   leg = 0, measured = 0;
    LStep                 step = LStep::Begin;
    Clock::time_point     stepAt{}, verbAt{}, fixAt{};
    Vec                   B{}, aim{};  // the blast point, and where this leg's shot is aimed (B, or the crate)
    coop::explosion_sync::Counters base{};
    Reading               r;
    Rest                  rest;
    ue_wrap::CachedObjRef rocket, parent;  // the salt legs' own actors; parent is the crate of sticky_parent too
    bool                  said = false;
    int                   lastRocket = -1, lastBomb = -1, sample = 0;
    bool                  host = false;  // this process's role, set each Tick
    // The acting game's switches the arm holds, each with the value it puts back (-1 = not held): the player's
    // startInvinc and the gamemode's unlockFungunsInStory. unreadableSaid: one bit per member already warned about.
    int                   invincBefore = -1, unlockBefore = -1;
    int                   unreadableSaid = 0;
};
extern Src g_src;
extern std::vector<ue_wrap::CachedObjRef> g_pre;  // the projectiles the world held when the drill began
extern std::vector<void*>                 g_scan;

// ---- the helpers of explosion_drill_source.cpp ----

const char* RoleName(bool host);

// The held or selfpill leg of that kind in the walk's table (the request arms act through the same definitions).
const LegDef* LegOf(Kind k);
void Enter(LStep s);
bool Late(std::chrono::seconds bound);
long long Since(Clock::time_point t);

bool ReadFloat(void* obj, const wchar_t* name, float& out);
bool ReadBool(void* obj, const wchar_t* name, bool& out);

// The leg ends: with the reason (it points at storage that outlives the End step), or after a refused spawn is counted.
void Skip(const char* why);
void Refused(const char* why);

// The projectiles the world holds now become the ones the drill's start found (g_pre); `Count` with `newOnly` leaves
// them out.
void SnapshotPre();

// The live instances of `cls` (its default object left out); with `newOnly`, those the drill's start did not find.
int Count(const wchar_t* cls, bool newOnly, std::vector<void*>* out = nullptr);

// The drill's own destroy: the game's for the acting host, local for a client (no PropDestroy is sent).
void Remove(void* actor, bool broadcast);

// The explosion's birth on the acting peer: the host's capture at begin-play, the client's dissolve at the spawn.
bool BirthSeen(bool host);

// The leg's watched projectile: set once; its location is read on every tick it is live.
void Track(void* actor);
// The watched projectile's last location as "(x,y,z)", or "none" when it was never seen.
void LastText(char* out, size_t cap);

// ---- explosion_drill_source_verbs.cpp ----

// A reflected bool written in place.
bool WriteBool(void* obj, const wchar_t* name, bool value);

// The held leg's start: its class loaded, the item taken into the hand.
void BeginItem(const LegDef& d);

// Holds the acting player's startInvinc and the gamemode's unlockFungunsInStory (the host's role: the gamemode's
// alone); the name of the first member that cannot be read, else null. Unguard puts back what Guard held.
const wchar_t* Guard(bool hostRole);
void Unguard();

// The local hand's actor when it is live and of class `cls`; the class name of what the hand holds, or "none".
void*       HeldOf(void* player, const wchar_t* cls);
std::string HeldClassName(void* player);

// The held legs' steps, walked by LegTick: the aim, the first verb and the throw after it, the sticky bomb's waits,
// the launcher's detonate, and the wait on the explosion's birth.
void Aim(void* player);
void Act(const LegDef& d, void* player);
void Act2(const LegDef& d, void* player);
void AwaitBomb(const LegDef& d);
void AwaitAttach();
void Detonate(void* player);
void Birth(const LegDef& d, bool host);

// ---- explosion_drill_source_measure.cpp ----

// sticky_parent: the crate is spawned (Begin), rests (CrateRest) and becomes the shot's aim; the stuck bomb is then
// waited for (AwaitAttachCrate), the crate moved (Follow, Followed) and destroyed, and the bomb watched (CrateGone).
void CrateBegin();
void CrateRest(const LegDef& d);
void AwaitAttachCrate();
void Follow();
void Followed(bool host);
void CrateGone(bool host);

// selfpill: the pill written and the player lit (Begin), the first birth waited for (PillWait).
void PillIgnite(void* player);
void PillWait(bool host, void* player);

// The measure legs' line, their put-back, and the host's watch of the props a client threw at it.
void EmitMeasure(const LegDef& d, bool host);
void CleanupMeasure(const LegDef& d, bool host);
void PassiveThrown();
void MeasureReset();

}  // namespace coop::dev::explosion_drill::detail
