#include "event_fire_policy.h"

namespace coop::event_fire_sync {

// The replay policy, the dupe matrix: every row's concrete output and which lane already
// carries it. The default is no replay. Replay only rows whose effect is a deterministic
// level, save or cosmetic flip that no existing lane delivers; replaying a lane-covered row
// would double-deliver. Keyed by name only: the few names living in both dispatchers have
// the same verdict either way, and the replay call still uses the received dispatch kind.
const char* const kReplayRows[] = {
    // Story and save flips (level-placed triggers; no lane):
    "treehouse_0", "treehouse_1", "treehouse_2", "treehouse_3", "treehouse_4", "treehouse_5",
    "break_RomeoSierra", "break_Victor", "break_Victor2",
    "obelisk",
    // Force-object appends (a save array the client's own dish scan reads; no lane):
    "looker_0-1", "looker_1-1", "looker_2-1", "looker_3-1", "looker_4-1",
    "arirSignal", "arirSpk", "picSignal", "peace",
    "arirSat_0", "arirSat_1", "arirSat_2", "piramid_sig",
    // Cosmetic or sound with no lane (the solar row's lights-dark converges with the light lane,
    // the same resulting state, echo-suppressed by its last-known prime):
    "solar", "call0",
    // Trigger-box scare arms, per-viewer scares by design; arming both sides is the correct coop
    // semantics (each player gets the scare on their own overlap):
    "toeStab", "falseEnter", "mann", "vent", "crys", "fakeGrays", "susArir",
    // Graffiti decal specials (a grime decal spawn; no lane):
    "arirGraff_0", "arirGraff_1", "arirGraff_2", "arirGraff_3",
    "arirGraff_4", "arirGraff_5", "arirGraff_6",
};

struct NoReplayRow { const char* name; const char* lane; };
const NoReplayRow kNoReplayRows[] = {
    // Outputs already ride a lane (a replay is a double delivery):
    { "starRain", "event_cue lane (cue 0)" },
    { "arirFollower", "npc lane" },
    // The swarm's wisps ride the npc lane (the source-gated catch in npc_world_enum); a replay
    // would arm the client's own swarm trigger and double-spawn client-local creatures on top of
    // the mirrors:
    { "wisps", "npc lane (EX-catch; event-swarm wisp_C mirrored)" },
    // The pyramid's path is host-random (wander and chase timers), so a replay armed the client's
    // own trigger box and a client walk-in spawned a divergent client-local pyramid with
    // unmirrored wisps. The arrival comes by mirror: the world-actor pose stream, npc-lane wisps
    // and the pyramid sync's brain suppression and gather relay.
    { "piramid", "piramid mirror lane (WA pose + piramid_sync brain/gather)" },
    // The ship: the trigger-box arm's overlap spawns the ship and the alarm lamp (and possibly
    // NPC leaves); replaying the arm would spawn them client-local. Host-only until the ship gets
    // a lane:
    { "arirShip", "actor spawn on armed overlap (no lane)" },
    { "earthTp", "sky lane (black-hole material/light; not a player teleport)" },
    { "vehtp", "atv lane" },
    { "bedEvent", "sleep lane" },
    { "picnic", "prop lane" }, { "destroyPicnic", "prop lane" },
    { "enasus", "prop lane" }, { "enacros", "prop lane" },
    { "cookier", "prop lane (armed prop)" }, { "paperGray", "prop lane (armed prop)" },
    { "arirEgg", "prop lane (armed prop)" },
    { "console", "device lanes" }, { "lightswitch", "device lanes" },
    { "keypadGuess", "device lanes" }, { "atvExplode", "atv lane (trap flag)" },
    // Host-local by design:
    { "agrav", "physics divergence (by-design host-local)" },
    { "treehouseSleep", "per-player teleport" },
    // Creature and save-actor spawns. The eventer and the event controllers are sources of the
    // native-seam spawn catch (coop/creatures/npc_world_enum), so the creatures and ships these rows
    // spawn reach the npc and world-actor lanes, and a replay would double them:
    { "ventCrawler", "npc lane (allowlisted)" }, { "ventKnocker", "world-actor lane (kocker_C)" },
    { "tentacleBalls", "world-actor + npc lanes (the follower and its balls)" },
    { "morningGay", "world-actor lane (morningUfo_C)" },
    { "borgRozital", "world-actor lane (rozitBorg_C)" }, { "graysforest", "npc lane (grayTest_C)" },
    { "graystank", "npc lane (grayTest_C)" }, { "arirBuster", "creature spawn (no lane yet)" },
    { "eggvasion", "world-actor + npc lanes (superEgger_C and eg_C)" },
    { "boarwar", "npc + world-actor lanes (the boars, the shooters, the fire tank)" },
    { "soltoClean", "world-actor lane (soltomiaCleaning_C)" },
    { "salt", "save-actor spawn (no lane)" }, { "rozitalHole", "save-actor spawn (no lane)" },
    { "dreambase", "dreamer_dreambase_C world-actor lane (host EX-catch + transform snapshot)" },
    { "fallbody_0", "world-actor lane (ufoDropper_body_C)" },
    { "fallbody_1", "world-actor lane (ufoDropper_body_C)" },
    { "fallcar_0", "world-actor lane (ufoDropper_car_C)" },
    // The prank layer (host-local RNG; thrown-prop outputs ride the prop lane):
    { "food", "prank special (prop lane)" }, { "drive", "prank special (prop lane)" },
    { "atvFuel", "prank special (prop lane)" }, { "atvFix", "prank special (prop lane)" },
    { "poisonFood", "prank special (prop lane)" }, { "expDrive", "prank special (prop lane)" },
    { "cookiebox", "prank special (prop lane)" }, { "trashPiles", "prank special (prop lane)" },
    { "vaccine", "prank special (prop lane)" }, { "oil", "prank special (prop lane)" },
    { "begos", "prank special (prop lane)" }, { "gascans", "prank special (prop lane)" },
    { "bombBox", "prank special (prop lane)" },
    { "rockThrow", "prank spawner (no lane)" }, { "hillRoller", "prank spawner (no lane)" },
    { "alienJump", "prank spawner (no lane)" }, { "trashBase", "prank spawner (no lane)" },
    { "alienSounds", "world_sound_sync (native noiser audio; movement/footsteps remain separate)" },
};

// The interaction rows: the row's entire effect is the prank special, host-local RNG.
bool IsPrankRow(const std::string& n) {
    return n.rfind("arirInteraction_", 0) == 0;
}

// The verdict: 1 replay, 0 known no-replay (the lane says why), -1 unknown (default no-replay).
int ReplayVerdict(const std::string& name, const char** laneOut) {
    for (const char* r : kReplayRows)
        if (name == r) return 1;
    for (const auto& nr : kNoReplayRows)
        if (name == nr.name) { *laneOut = nr.lane; return 0; }
    if (IsPrankRow(name)) { *laneOut = "prank special (host-local RNG)"; return 0; }
    return -1;
}

}  // namespace coop::event_fire_sync
